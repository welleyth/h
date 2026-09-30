#pragma once

#include <string>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "interact.h"
#include "summary.h"

namespace eo {

class writer {
public:
    template <class... Args>
    void line(Args const&... values) {
        bool first = true;
        (detail::add_to_line(text_, values, first), ...);
        text_.push_back('\n');
    }

    void line() { text_.push_back('\n'); }

    std::string const& text() const { return text_; }

private:
    std::string text_;
};

class phases {
public:
    phases(interactor& owner, long long count, detail::site where = detail::site::here())
        : owner_(&owner), count_(count), where_(where) {
        if (count < 1) detail::library_error(fmt("{}: a run has at least one phase", detail::where_of(where)));
        std::string const carried = owner.whole_input();
        if (carried.rfind(summary::marker(), 0) == 0)
            owner.fail_jury(fmt("run_count is more than the {} phases the interactor declares", count_));
        if (carried.rfind(marker(), 0) != 0) {
            number_ = 1;
            test_ = carried;
            return;
        }
        unpack(carried);
        owner.read_the_test_again(test_);
        earlier_ = stream(detail::source::over_text(payload_, true), detail::fault::jury_error,
                          "the previous phase");
        if (finished_) {
            owner.passing_through();
            hand_on(std::string());
        }
    }

    phases(phases const&) = delete;
    phases& operator=(phases const&) = delete;

    long long number() const { return number_; }
    stream& previous() { return earlier_; }

    template <class Write>
    [[noreturn]] void handoff(Write write) {
        if (number_ >= count_)
            owner_->fail_jury(fmt("{}: phase {} of {} cannot hand off; it must end the test",
                                  detail::where_of(where_), number_, count_));
        writer out;
        write(out);
        hand_on(out.text());
    }

    template <class... Args>
    [[noreturn]] void finish(detail::scored fraction, detail::pattern pattern = "", Args const&... args) {
        if (number_ >= count_) owner_->pass(detail::clamped(fraction.value, fraction.where), fmt(pattern, args...));
        finished_ = true;
        share_ = detail::clamped(fraction.value, fraction.where);
        note_ = fmt(pattern, args...);
        hand_on(std::string());
    }

    static char const* marker() { return "eolymp-phase"; }

private:
    [[noreturn]] void hand_on(std::string const& payload) {
        if (finished_ && number_ >= count_) owner_->pass(share_, note_);
        std::string built = fmt("{} {} {}", marker(), number_ + 1, count_);
        if (finished_) built += fmt(" finished {} {}", share_, summary::one_line(note_));
        built += fmt("\ninput {}\n", test_.size());
        built += test_;
        built += "\n";
        built += payload;
        if (built.size() > detail::large_file)
            detail::warn("EO407", fmt("this handoff is {} bytes", built.size()),
                         "the judge copies it between runs", where_);
        owner_->hand_the_file_on(built);
    }

    void unpack(std::string const& carried) {
        std::size_t at = carried.find('\n');
        std::string const head = carried.substr(0, at);
        std::size_t const second = carried.find('\n', at + 1);
        std::string const sizes = carried.substr(at + 1, second - at - 1);
        long long given = 0;
        long long total = 0;
        double share = 0;
        std::string note;
        if (!read_head(head, given, total, share, note))
            owner_->fail_jury(fmt("the previous phase left a header this release cannot read: \"{}\"",
                                  detail::shorten(head)));
        if (total != count_)
            owner_->fail_jury(fmt("the previous run declared {} phases and this one declares {}", total,
                                  count_));
        number_ = given;
        share_ = share;
        note_ = note;
        long long bytes = 0;
        if (sizes.rfind("input ", 0) != 0)
            owner_->fail_jury("the previous phase left no test in its handoff");
        detail::integer_read const parsed = detail::parse_integer(sizes.substr(6));
        if (parsed.problem != detail::number_problem::none)
            owner_->fail_jury("the previous phase left a handoff with no size on its test");
        bytes = parsed.value;
        if (bytes < 0)
            owner_->fail_jury(fmt("the previous phase left a handoff whose test has a size of {} bytes", bytes));
        if (second == std::string::npos || static_cast<unsigned long long>(bytes) >= carried.size() - second - 1)
            owner_->fail_jury(fmt("the previous phase left a handoff cut short of its {}-byte test and the "
                                  "line break after it", bytes));
        test_ = carried.substr(second + 1, static_cast<std::size_t>(bytes));
        payload_ = carried.substr(second + 1 + static_cast<std::size_t>(bytes) + 1);
    }

    bool read_head(std::string const& head, long long& given, long long& total, double& share,
                   std::string& note) {
        std::vector<std::string> words;
        std::string one;
        for (char const letter : head) {
            if (letter == ' ') {
                words.push_back(one);
                one.clear();
                if (words.size() == 4) break;
            } else {
                one.push_back(letter);
            }
        }
        if (!one.empty()) words.push_back(one);
        if (words.size() < 3) return false;
        detail::integer_read const which = detail::parse_integer(words[1]);
        detail::integer_read const how_many = detail::parse_integer(words[2]);
        if (which.problem != detail::number_problem::none ||
            how_many.problem != detail::number_problem::none)
            return false;
        given = which.value;
        total = how_many.value;
        if (words.size() < 4 || words[3] != "finished") return true;
        finished_ = true;
        std::size_t const after = head.find("finished ") + 9;
        std::size_t const gap = head.find(' ', after);
        detail::real_read const paid = detail::parse_real(head.substr(after, gap - after), true);
        if (paid.problem != detail::number_problem::none) return false;
        share = paid.value;
        note = gap == std::string::npos ? std::string() : head.substr(gap + 1);
        return true;
    }

    interactor* owner_;
    long long count_;
    detail::site where_;
    long long number_ = 1;
    bool finished_ = false;
    double share_ = 0;
    std::string note_;
    std::string test_;
    std::string payload_;
    stream earlier_;
};

}  // namespace eo
