#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <set>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include <sys/syscall.h>
#include <unistd.h>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "io.h"
#include "read.h"
#include "stream.h"
#include "role.h"
#include "structure.h"
#include "summary.h"

namespace eo {

struct ignore_t {};
struct lenient_t {};
struct plain_t {};
struct any_case_t {};

inline constexpr ignore_t ignore{};
inline constexpr lenient_t lenient{};
inline constexpr plain_t plain{};
inline constexpr any_case_t any_case{};

enum class answers_are { unique, many };

inline constexpr answers_are unique = answers_are::unique;
inline constexpr answers_are many = answers_are::many;

enum class towards { smaller, larger };

inline constexpr towards minimize = towards::smaller;
inline constexpr towards maximize = towards::larger;

class checker;

namespace detail {

inline checker*& live_checker() {
    static checker* only = nullptr;
    return only;
}

inline std::FILE* opened_scratch(int descriptor) {
    if (descriptor < 0) return nullptr;
    std::FILE* const file = ::fdopen(descriptor, "w+b");
    if (file == nullptr) ::close(descriptor);
    return file;
}

inline std::FILE* scratch_in_memory() {
#if defined(__linux__) && defined(SYS_memfd_create)
    return opened_scratch(static_cast<int>(::syscall(SYS_memfd_create, "eolymp-checker-output", 0)));
#else
    return nullptr;
#endif
}

inline std::FILE* scratch_in_the_temporary_directory() { return std::tmpfile(); }

inline std::FILE* scratch_in_the_workspace() {
    char name[] = "eolymp-checker-output-XXXXXX";
    int const descriptor = ::mkstemp(name);
    if (descriptor >= 0) ::unlink(name);
    return opened_scratch(descriptor);
}

using scratch_maker = std::FILE* (*)();

inline std::array<scratch_maker, 3> scratch_makers() {
    return {&scratch_in_the_temporary_directory, &scratch_in_memory, &scratch_in_the_workspace};
}

template <std::size_t Count>
inline std::FILE* first_scratch(std::array<scratch_maker, Count> const& makers) {
    for (scratch_maker const make : makers)
        if (std::FILE* const made = make()) return made;
    return nullptr;
}

inline std::FILE* scratch_file() { return first_scratch(scratch_makers()); }

class reader;

}  // namespace detail

class stream {
public:
    stream() = default;

    stream(detail::source from, detail::fault whose, std::string label)
        : reader_(std::move(from), whose, std::move(label), true, "EO103") {
        reader_.exponents(true);
    }

    int read_int(long long low, long long high, detail::value_name name,
                 detail::site where = detail::site::here()) {
        return reader_.whole_int(low, high, detail::stated::yes, name, where);
    }

    int read_int(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.whole_int(0, 0, detail::stated::deliberate, name, where);
    }

    [[deprecated("eolymp EO103: bound this value, or say read_int(eo::any, name)")]] int
    read_int(detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.whole_int(0, 0, detail::stated::absent, name, where);
    }

    [[deprecated("eolymp EO101: name this value, or say read_int(low, high, eo::unnamed)")]] int
    read_int(long long low, long long high, detail::site where = detail::site::here()) {
        return reader_.whole_int(low, high, detail::stated::yes, detail::value_name::nothing(), where);
    }

    long long read_long(long long low, long long high, detail::value_name name,
                        detail::site where = detail::site::here()) {
        return reader_.whole_long(low, high, detail::stated::yes, name, where);
    }

    long long read_long(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.whole_long(0, 0, detail::stated::deliberate, name, where);
    }

    double read_real(double low, double high, detail::value_name name,
                     detail::site where = detail::site::here()) {
        return reader_.fractional(low, high, detail::stated::yes, 0, 0, false, name, where);
    }

    [[deprecated("eolymp EO103: bound this value, or say read_real(eo::any, name)")]] double
    read_real(detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.fractional(0, 0, detail::stated::absent, 0, 0, false, name, where);
    }

    double read_real(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.fractional(0, 0, detail::stated::deliberate, 0, 0, false, name, where);
    }

    std::string read_token(long long least, long long most, charset allowed, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return reader_.word(least, most, &allowed, detail::stated::yes, name, where);
    }

    std::string read_token(long long least, long long most, detail::value_name name,
                           detail::site where = detail::site::here()) {
        return reader_.word(least, most, nullptr, detail::stated::yes, name, where);
    }

    std::string read_token(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.word(0, 0, nullptr, detail::stated::deliberate, name, where);
    }

    std::string read_line(long long least, long long most, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return reader_.rest_of_line(least, most, nullptr, detail::stated::yes, name, where);
    }

    std::string read_line(long long least, long long most, charset allowed, detail::value_name name,
                          detail::site where = detail::site::here()) {
        return reader_.rest_of_line(least, most, &allowed, detail::stated::yes, name, where);
    }

    std::string read_line(any_t, detail::value_name name, detail::site where = detail::site::here()) {
        return reader_.rest_of_line(0, 0, nullptr, detail::stated::deliberate, name, where);
    }

    std::string read_choice(std::initializer_list<char const*> choices, detail::value_name name,
                            detail::site where = detail::site::here()) {
        return choose(choices, false, name, where);
    }

    std::string read_choice(std::initializer_list<char const*> choices, any_case_t, detail::value_name name,
                            detail::site where = detail::site::here()) {
        return choose(choices, true, name, where);
    }

    std::vector<int> read_ints(long long count, long long low, long long high, detail::value_name name,
                               detail::site where = detail::site::here()) {
        std::vector<int> values;
        values.reserve(reader_.room_for(count, name));
        for (long long at = 1; at <= count; at++) {
            values.push_back(reader_.whole_int(low, high, detail::stated::yes, name.lent_at(at), where));
        }
        return values;
    }

    std::vector<long long> read_longs(long long count, long long low, long long high, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        std::vector<long long> values;
        values.reserve(reader_.room_for(count, name));
        for (long long at = 1; at <= count; at++)
            values.push_back(reader_.whole_long(low, high, detail::stated::yes, name.lent_at(at), where));
        return values;
    }

    std::vector<long long> read_longs(long long count, any_t, detail::value_name name,
                                      detail::site where = detail::site::here()) {
        std::vector<long long> values;
        values.reserve(reader_.room_for(count, name));
        for (long long at = 1; at <= count; at++)
            values.push_back(reader_.whole_long(0, 0, detail::stated::deliberate, name.lent_at(at), where));
        return values;
    }

    std::vector<double> read_reals(long long count, double low, double high, detail::value_name name,
                                   detail::site where = detail::site::here()) {
        std::vector<double> values;
        values.reserve(reader_.room_for(count, name));
        for (long long at = 1; at <= count; at++)
            values.push_back(
                reader_.fractional(low, high, detail::stated::yes, 0, 0, false, name.lent_at(at), where));
        return values;
    }

    std::vector<std::string> read_tokens(long long count, long long least, long long most, charset allowed,
                                         detail::value_name name,
                                         detail::site where = detail::site::here()) {
        std::vector<std::string> values;
        values.reserve(reader_.room_for(count, name));
        for (long long at = 1; at <= count; at++)
            values.push_back(reader_.word(least, most, &allowed, detail::stated::yes, name.lent_at(at), where));
        return values;
    }

    bool at_eof() { return reader_.at_end(); }
    bool at_eoln() { return reader_.at_line_end(); }

    [[noreturn]] void wrong(std::string const& message) const {
        reader_.refuse(detail::value_name(unnamed), message);
    }

    template <class... Args>
    [[noreturn]] void wrong(detail::pattern pattern, Args const&... args) const {
        reader_.refuse(detail::value_name(unnamed), fmt(pattern, args...));
    }

    void numbers(lenient_t) { reader_.relaxed(true); }
    void reals(plain_t) { reader_.exponents(false); }
    void trailing(ignore_t) { trailing_matters_ = false; }

    void skip_rest(std::string reason) {
        if (reason.empty()) detail::library_error("skip_rest needs a reason");
        skipped_ = true;
    }

private:
    friend class checker;
    friend class interactor;
    friend class controller;
    friend class channel;

    detail::reader& inside() { return reader_; }
    bool trailing_matters() const { return trailing_matters_; }
    bool skipped() const { return skipped_; }


    std::string choose(std::initializer_list<char const*> choices, bool fold, detail::value_name const& name,
                       detail::site where) {
        std::string found = reader_.take_word(name, where, "a token", reader_.longest_of(choices));
        std::string listed;
        for (char const* one : choices) {
            listed += (listed.empty() ? "" : ", ") + std::string(one);
            if (found == one) return found;
            if (fold && detail::same_folded(found, one)) return std::string(one);
        }
        reader_.refuse(name, fmt("\"{}\" is not one of {}", detail::shorten(found), listed));
    }

    detail::reader reader_;
    bool trailing_matters_ = true;
    bool skipped_ = false;
};

using input_stream = stream;
using answer_stream = stream;

}  // namespace eo

namespace eo {

class checker final : public detail::scorer {
public:
    checker(int argc, char** argv, detail::site where = detail::site::here()) {
        if (detail::live_checker() != nullptr)
            detail::library_error(fmt("{}: this program already has a checker", detail::where_of(where)));
        detail::diagnostics::shared().start_the_clock("EO209", "checker", 10000, where);
        std::array<char const*, 3> const given = detail::test_paths(argc, argv);
        char const* const kinds[3] = {"input", "output", "answer"};
        std::string paths[3];
        for (int at = 0; at < 3; at++) {
            if (given[static_cast<std::size_t>(at)] == nullptr)
                detail::library_error(
                    fmt("{}: the checker was given no {} file", detail::where_of(where), kinds[at]));
            paths[at] = given[static_cast<std::size_t>(at)];
        }
        if (argc >= 4 && detail::environment("OUTPUT_FILE") != nullptr && paths[1] != argv[2])
            detail::note("EO211", "this checker runs as the legacy type, which swaps its last two arguments",
                         "the ordinary PROGRAM type is the norm", where);
        input = stream(detail::source::over_file(paths[0].c_str(), true), detail::fault::jury_error,
                       "input.txt");
        output = stream(detail::source::over_file(paths[1].c_str(), false), detail::fault::wrong_answer,
                        "output.txt");
        jury = stream(detail::source::over_file(paths[2].c_str(), true), detail::fault::jury_error,
                      "answer.txt");
        if (detail::on_judge()) {
            saved_out_ = ::dup(1);
            saved_err_ = ::dup(2);
            held_ = detail::scratch_file();
            if (held_ == nullptr)
                detail::library_error("the checker cannot open a scratch file for its own output: not in memory, "
                                      "not in the temporary directory and not in the workspace");
            ::dup2(::fileno(held_), 1);
            ::dup2(::fileno(held_), 2);
            detail::emitter() = &checker::write_log;
        }
        detail::live_checker() = this;
        detail::live_scorer() = this;
    }

    checker(checker const&) = delete;
    checker& operator=(checker const&) = delete;

    ~checker() noexcept(false) {
        detail::live_checker() = nullptr;
        detail::live_scorer() = nullptr;
        detail::blaming() = nullptr;
        detail::current_case() = 0;
        detail::emitter() = nullptr;
        if (held_ != nullptr) {
            put_the_output_back();
            let_go_of_what_was_held();
        }
        fail_closed("checker");
    }

    stream input;
    stream output;
    stream jury;

    int group() const { return whole_of("TEST_GROUP"); }
    int index() const { return whole_of("TEST_INDEX"); }

    std::string test_id() const {
        char const* const set = detail::environment("TEST_ID");
        return set == nullptr ? std::string() : std::string(set);
    }

    void answers(answers_are how) { declared_ = how; }

    template <class Body>
    void cases(long long count, Body&& body) {
        for (long long at = 1; at <= count; at++) {
            detail::current_case() = at;
            body();
        }
        detail::current_case() = 0;
    }

    template <class Read>
    auto read_both(Read read_one) {
        auto expected = read_with(jury, read_one);
        auto found = read_with(output, read_one);
        return std::make_pair(std::move(expected), std::move(found));
    }

    template <class T>
    [[noreturn]] void optimum(T const& by_the_jury, T const& found, towards direction) {
        if (found == by_the_jury) pass(1, fmt("{}", found));
        bool const better = direction == towards::smaller ? found < by_the_jury : found > by_the_jury;
        if (better)
            fail_jury(fmt("the contestant's {} beats the jury's {}", found, by_the_jury));
        fail_run(fmt("the answer is {}; the optimum is {}", found, by_the_jury));
    }

    [[noreturn]] void tokens() {
        compared_only_ = true;
        long long seen = 0;
        std::string want;
        std::string got;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} tokens", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} tokens, the output has more", seen - 1));
            jury_token(want, detail::site::here());
            if (output_done) fail_run(fmt("the output ended after {} tokens, the answer has more", seen - 1));
            contestant_token(got, want.size());
            if (got.size() > want.size())
                fail_run(fmt("token {} is longer than the expected \"{}\"; it starts \"{}\"", seen,
                             detail::shorten(want), detail::shorten(got)));
            if (want != got)
                fail_run(fmt("token {} is \"{}\", expected \"{}\"", seen, detail::shorten(got),
                             detail::shorten(want)));
        }
    }

    [[noreturn]] void reals(double epsilon) {
        compared_only_ = true;
        long long seen = 0;
        std::string want;
        std::string got;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} values", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} tokens, the output has more", seen - 1));
            jury_token(want, detail::site::here());
            if (output_done) fail_run(fmt("the output ended after {} tokens, the answer has more", seen - 1));
            std::size_t const longest = std::max<std::size_t>(want.size(), detail::reader::longest_number);
            contestant_token(got, longest);
            if (got.size() > longest)
                fail_run(fmt("token {} is longer than {} characters: \"{}\"", seen, longest, detail::shorten(got)));
            detail::real_read const wanted = detail::parse_real(want, true, true);
            detail::real_read const found = detail::parse_real(got, true, true);
            if (wanted.problem == detail::number_problem::none &&
                found.problem == detail::number_problem::none) {
                if (!close_enough(wanted.value, found.value, epsilon))
                    fail_run(fmt("value {} is {}, expected {}", seen, found.value, wanted.value));
                continue;
            }
            if (want != got)
                fail_run(fmt("token {} is \"{}\", expected \"{}\"", seen, detail::shorten(got),
                             detail::shorten(want)));
        }
    }

    [[noreturn]] void lines() {
        compared_only_ = true;
        long long seen = 0;
        while (true) {
            bool const jury_done = jury.at_eof();
            bool const output_done = output.at_eof();
            if (jury_done && output_done) pass(1, fmt("{} lines", seen));
            seen++;
            if (jury_done) fail_run(fmt("the answer has {} lines, the output has more", seen - 1));
            std::string want = jury.read_line(any, unnamed);
            if (output_done) fail_run(fmt("the output ended after {} lines, the answer has more", seen - 1));
            bool longer = false;
            std::string got = output.inside().line_up_to(want.size() + 1, longer, unnamed);
            while (!want.empty() && trailing_blank(want.back())) want.pop_back();
            if (longer)
                fail_run(fmt("line {} is longer than the expected \"{}\"; it starts \"{}\"", seen,
                             detail::shorten(want), detail::shorten(got)));
            while (!got.empty() && trailing_blank(got.back())) got.pop_back();
            if (want != got) fail_run(fmt("line {} is \"{}\", expected \"{}\"", seen, detail::shorten(got),
                                          detail::shorten(want)));
        }
    }

    [[noreturn]] void from_interactor(detail::site where = detail::site::here()) {
        from_interactor([](summary const& what) { return what.fraction(); }, where);
    }

    template <class Mapping>
    [[noreturn]] void from_interactor(Mapping mapping, detail::site where = detail::site::here()) {
        stock_ = true;
        output.inside().blame(detail::fault::jury_error);
        if (jury.inside().read_anything() == false) jury.skip_rest("an interactive problem is graded by the interactor");
        summary const said = read_summary(output);
        pass(detail::clamped(mapping(said), where), said.message());
    }

    template <class Certificate>
    [[noreturn]] void yes_no(Certificate certificate, char const* yes = "YES", char const* no = "NO") {
        std::string const by_the_jury = jury.read_choice({yes, no}, any_case, "verdict");
        std::string const found = output.read_choice({yes, no}, any_case, "verdict");
        if (by_the_jury == no && found == no) pass(1, "both say no");
        if (by_the_jury == yes && found == no) fail_run("a solution exists");
        if (by_the_jury == no && found == yes) {
            {
                detail::blame_guard blamed(&output.inside());
                certificate(output);
            }
            fail_jury("the contestant found a solution the jury says does not exist");
        }
        {
            detail::blame_guard blamed(&jury.inside());
            certificate(jury);
        }
        {
            detail::blame_guard blamed(&output.inside());
            certificate(output);
        }
        pass(1, "a valid certificate");
    }

    [[noreturn]] void pass(double fraction, std::string const& message) final {
        if (std::isnan(fraction)) detail::refuse_a_score(fmt("a score of {}", fraction));
        closing_checks(fraction);
        if (fraction >= 1) deliver(0, "ok", message);
        double const paid = fraction * cost();
        std::string const printed = detail::format_points(paid);
        if (cost() > 0 && std::strtof(printed.c_str(), nullptr) >= static_cast<float>(cost()))
            detail::warn("EO206", fmt("'points {}' is below the test's {}, but the judge reads points as a "
                                      "float, which rounds it to the full cost: the run counts as accepted",
                                      printed, cost()),
                         "use eo::ratio(a, b), which is exact, or eo::accept for full marks", detail::site::here());
        if (cost() <= 0)
            detail::warn("EO208", fmt("this test is worth {} points, so the judge counts this score of {} as "
                                      "accepted: points reach a cost of 0", cost(), fraction),
                         "end an answer that earns nothing with eo::wrong, which a sample shows as a wrong answer",
                         detail::site::here());
        deliver(7, "points " + printed, message);
    }

    [[noreturn]] void fail_run(std::string const& message) final {
        if (message.empty())
            detail::warn("EO204", "this wrong answer carries no message", "say what was wrong with it",
                         detail::site::here());
        deliver(1, "wrong answer", message);
    }

    [[noreturn]] void fail_jury(std::string const& message) final {
        if (message.empty())
            detail::warn("EO204", "this jury error carries no message", "say what the jury got wrong",
                         detail::site::here());
        deliver(3, "jury error", message);
    }

private:
    static bool trailing_blank(char one) { return one == ' ' || one == '\t' || one == '\r'; }

    void jury_token(std::string& want, detail::site where) {
        jury.inside().word_into(want, 0, 0, nullptr, detail::stated::deliberate, unnamed, where);
    }

    void contestant_token(std::string& got, std::size_t longest) {
        output.inside().take_word_into(got, unnamed, detail::site::here(), "a token",
                                       static_cast<long long>(longest) + 1);
    }

    static void write_log(std::string const& verdict) {
        checker* const one = detail::live_checker();
        if (one == nullptr) return;
        one->unwrap(verdict);
    }

    template <class Read>
    auto read_with(stream& which, Read& read_one) {
        detail::blame_guard blamed(&which.inside());
        return read_one(which);
    }

    summary read_summary(stream& said) {
        summary out;
        std::string const marker = said.read_token(any, "marker");
        if (marker == "eolymp-phase") {
            long long const at = said.read_long(1, 1000, "phase");
            long long const of = said.read_long(1, 1000, "phases");
            said.wrong("the interactor stopped at phase {} of {}; set run_count to {}", at - 1, of, of);
        }
        if (marker != summary::marker())
            said.wrong("this is not an interactor's summary: it starts with \"{}\"",
                       detail::shorten(marker));
        said.read_long(1, 1, "version");
        std::set<std::string> seen;
        while (!said.at_eof()) {
            std::string const field = said.read_token(any, "field");
            if (field != "value" && !seen.insert(field).second)
                said.wrong("the summary has a second {} field", detail::shorten(field));
            if (field == "fraction") {
                out.set_fraction(said.read_real(0.0, 1.0, "fraction"));
            } else if (field == "value") {
                std::string const name = said.read_token(any, "name");
                if (out.has(name)) said.wrong("the summary has a second value called \"{}\"", detail::shorten(name));
                out.record(name, said.read_real(any, "value"));
            } else if (field == "message") {
                std::string text = said.read_line(any, "message");
                if (!text.empty() && text.front() == ' ') text.erase(0, 1);
                out.set_message(std::move(text));
            } else {
                said.wrong("the summary has a field called \"{}\"", detail::shorten(field));
            }
        }
        return out;
    }

    int whole_of(char const* name) const {
        char const* const set = detail::environment(name);
        if (set == nullptr) return 0;
        detail::integer_read const parsed = detail::parse_integer(set);
        return parsed.problem == detail::number_problem::none ? static_cast<int>(parsed.value) : 0;
    }

    void closing_checks(double fraction) {
        if (fraction <= 0) return;
        if (!output.inside().read_anything())
            detail::warn("EO201", "the checker passed the run without reading any of the output",
                         "read the output, or use a built-in checker", detail::site::here());
        if (!stock_ && !input.inside().read_anything() && !jury.inside().read_anything())
            detail::warn("EO202", "the checker read neither the input nor the answer",
                         "a verdict that cannot depend on the test", detail::site::here());
        if (declared_ == answers_are::many && compared_only_)
            detail::warn("EO212", "the problem declares many answers, but this checker only compares "
                                  "with the jury's",
                         "verify the contestant's answer instead", detail::site::here());
        if (!jury.skipped() && !jury.at_eof())
            detail::warn("EO203", fmt("the answer file still holds \"{}\" when the checker finished",
                                      detail::shorten(jury.inside().rest_of_the_input())),
                         "read it, or say why not: c.jury.skip_rest(\"...\")", detail::site::here());
        if (output.trailing_matters() && !output.at_eof())
            output.wrong(fmt("extra output after the answer: \"{}\"",
                             detail::shorten(output.inside().rest_of_the_input())));
    }

    [[noreturn]] void deliver(int code, std::string head, std::string const& message) {
        delivered_ = true;
        detail::finish(code, message.empty() ? head : head + " " + message);
    }

    void put_the_output_back() {
        std::fflush(stdout);
        std::fflush(stderr);
        ::dup2(saved_out_, 1);
        ::dup2(saved_err_, 2);
        ::close(saved_out_);
        ::close(saved_err_);
    }

    long long copy_what_was_held() {
        std::rewind(held_);
        char buffer[1 << 16];
        long long copied = 0;
        std::size_t got = 0;
        while ((got = std::fread(buffer, 1, sizeof(buffer), held_)) > 0) {
            std::fwrite(buffer, 1, got, stdout);
            copied += static_cast<long long>(got);
        }
        return copied;
    }

    void let_go_of_what_was_held() {
        std::fclose(held_);
        held_ = nullptr;
    }

    void unwrap(std::string const& verdict) {
        detail::emitter() = nullptr;
        bool const holding = held_ != nullptr;
        if (holding) put_the_output_back();
        detail::report(verdict);
        long long const held = holding ? copy_what_was_held() : 0;
        if (holding) let_go_of_what_was_held();
        std::fwrite("eolymp.h ", 1, 9, stdout);
        std::fwrite(EOLYMP_H_VERSION, 1, std::strlen(EOLYMP_H_VERSION), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        if (held > 64 * 1024)
            detail::note("EO210", fmt("the checker printed {} bytes before its verdict", held),
                         "stored logs are truncated", detail::site::here());
    }

    answers_are declared_ = answers_are::unique;
    bool stock_ = false;
    bool compared_only_ = false;
    int saved_out_ = -1;
    int saved_err_ = -1;
    std::FILE* held_ = nullptr;
};

}  // namespace eo
