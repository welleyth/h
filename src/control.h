#pragma once

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

#include "check.h"
#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "io.h"
#include "random.h"
#include "role.h"
#include "stream.h"
#include "summary.h"

namespace eo {

class controller;

namespace detail {

inline controller*& live_controller() {
    static controller* only = nullptr;
    return only;
}

}  // namespace detail

class channel : public stream {
public:
    channel() = default;

    channel(channel const&) = delete;
    channel& operator=(channel const&) = delete;
    channel(channel&&) = default;
    channel& operator=(channel&&) = default;

    ~channel() { close(); }

    stream& from() { return *this; }

    void skip_rest(std::string reason) = delete;
    void trailing(ignore_t) = delete;

    template <class... Args>
    void send(Args const&... values) {
        std::string line;
        bool first = true;
        (detail::add_to_line(line, values, first), ...);
        line.push_back('\n');
        pending_ += line;
    }

    void close();

    long long index() const { return index_; }

private:
    friend class controller;

    void flush();
    void hand_over();

    controller* owner_ = nullptr;
    int writes_ = -1;
    long long index_ = 0;
    std::string pending_;
    bool spoken_to_ = false;
    bool shut_ = false;
    bool deaf_ = false;
};

class controller final : public detail::scorer, public detail::limits_keeper {
public:
    controller(int argc, char** argv, detail::site where = detail::site::here()) {
        if (detail::live_controller() != nullptr)
            detail::library_error(fmt("{}: this program already has a controller", detail::where_of(where)));
        std::array<char const*, 3> const given = detail::test_paths(argc, argv);
        for (int at = 0; at < 3; at++)
            if (given[static_cast<std::size_t>(at)] != nullptr) paths_[at] = given[static_cast<std::size_t>(at)];
        if (paths_[0].empty() || paths_[1].empty())
            detail::library_error(fmt("{}: a controller needs the test and a file for its summary",
                                      detail::where_of(where)));
        ::signal(SIGPIPE, SIG_IGN);
        detail::log_file() = stderr;
        detail::emitter() = &controller::say;
        input = stream(detail::source::over_file(paths_[0].c_str(), true), detail::fault::jury_error,
                       "input.txt");
        if (!paths_[2].empty())
            jury = stream(detail::source::over_file(paths_[2].c_str(), true), detail::fault::jury_error,
                          "answer.txt");
        detail::live_controller() = this;
        detail::live_scorer() = this;
    }

    controller(controller const&) = delete;
    controller& operator=(controller const&) = delete;

    ~controller() noexcept(false) {
        detail::restore_channels afterwards;
        detail::live_controller() = nullptr;
        detail::live_scorer() = nullptr;
        detail::current_case() = 0;
        team_.clear();
        if (requests_ != nullptr) std::fclose(requests_);
        if (replies_ != nullptr) std::fclose(replies_);
        requests_ = nullptr;
        replies_ = nullptr;
        fail_closed("controller");
    }

    stream input;
    stream jury;

    bool has_jury() const { return !paths_[2].empty(); }

    long long instance_limit() const {
        char const* const set = detail::environment("INSTANCE_LIMIT");
        if (set == nullptr) return 0;
        detail::integer_read const parsed = detail::parse_integer(set);
        return parsed.problem == detail::number_problem::none ? parsed.value : 0;
    }

    channel& spawn(detail::site where = detail::site::here()) {
        open_the_control();
        std::fputs("SPAWN\n", requests_);
        std::fflush(requests_);
        std::string const reply = next_reply();
        if (reply == "LIMIT")
            fail_jury(fmt("{}: the judge refused instance {}; instance_limit is {}",
                          detail::where_of(where), team_.size() + 1, instance_limit()));
        std::size_t const gap = reply.find(' ');
        if (gap == std::string::npos)
            fail_jury(fmt("the judge answered SPAWN with \"{}\"", detail::shorten(reply)));
        std::string const to_them = reply.substr(0, gap);
        std::string const from_them = reply.substr(gap + 1);
        auto made = std::make_unique<channel>();
        made->owner_ = this;
        made->index_ = static_cast<long long>(team_.size()) + 1;
        made->writes_ = ::open(to_them.c_str(), O_WRONLY);
        if (made->writes_ < 0) fail_jury(fmt("cannot write to instance {}", made->index_));
        std::string const named = fmt("instance {}", made->index_);
        detail::source listening = detail::source::over_channel(from_them.c_str());
        static_cast<stream&>(*made) = stream(std::move(listening), detail::fault::wrong_answer, named);
        made->inside().before_blocking(&controller::flush_from, this);
        made->inside().on_end(fmt("instance {} ended the dialogue early", made->index_));
        team_.push_back(std::move(made));
        return *team_.back();
    }

    void value(std::string name, double what) { held_.record(std::move(name), what); }

    eo::rng& rng() {
        if (!seeded_) {
            dice_ = eo::rng(detail::seed_of_file(paths_[0].c_str()));
            seeded_ = true;
        }
        return dice_;
    }

    long long round_trips() const { return round_trips_; }

    void declare_budget() final { budgets_++; }
    void spent_a_budget() final { budget_spent_ = true; }

    [[noreturn]] void pass(double fraction, std::string const& message) final {
        if (std::isnan(fraction)) detail::refuse_a_score(fmt("a score of {}", fraction));
        closing_checks(fraction);
        held_.set_fraction(std::min(fraction, 1.0));
        held_.set_message(message);
        detail::write_file(paths_[1], held_.written(), "summary");
        deliver(0, message.empty() ? "ok" : "ok " + message);
    }

    [[noreturn]] void fail_run(std::string const& message) final {
        if (message.empty())
            detail::warn("EO204", "this wrong answer carries no message", "say what an instance did",
                         detail::site::here());
        deliver(1, message.empty() ? "wrong answer" : "wrong answer " + message);
    }

    [[noreturn]] void fail_jury(std::string const& message) final {
        deliver(3, message.empty() ? "jury error" : "jury error " + message);
    }

    void report_traffic() {
        if (reported_) return;
        reported_ = true;
        detail::log_line(fmt("{} instances, {} round trips, {} bytes sent", team_.size(), round_trips_,
                             sent_bytes_));
        if (round_trips_ > 100000)
            detail::diagnostics::shared().raise(
                "EO401", round_trips_ > 500000 ? detail::severity::warning : detail::severity::note,
                fmt("this run made {} round trips", round_trips_),
                "a pipe manages about 150,000 a second", detail::site::here());
    }

private:
    friend class channel;

    static void flush_from(void* owner) { static_cast<controller*>(owner)->flush_everything(); }

    static void say(std::string const& text) {
        std::fwrite(text.data(), 1, text.size(), stderr);
        std::fputc('\n', stderr);
        if (detail::live_controller() != nullptr) detail::live_controller()->report_traffic();
        std::fflush(stderr);
    }

    void open_the_control() {
        if (requests_ != nullptr) return;
        char const* const out = detail::environment("CONTROL_OUTPUT_FILE");
        char const* const in = detail::environment("CONTROL_INPUT_FILE");
        if (out == nullptr || in == nullptr)
            fail_jury("this problem is not set up for instances: there is no control channel");
        requests_ = std::fopen(out, "w");
        if (requests_ == nullptr) fail_jury("cannot reach the judge's control channel");
        replies_ = std::fopen(in, "r");
        if (replies_ == nullptr) fail_jury("cannot hear the judge's control channel");
    }

    std::string next_reply() {
        std::string line;
        for (int one = std::fgetc(replies_); one != EOF && one != '\n'; one = std::fgetc(replies_))
            line.push_back(static_cast<char>(one));
        if (line.empty()) fail_jury("the judge closed the control channel");
        return line;
    }

    void flush_everything() {
        bool sent = false;
        for (std::unique_ptr<channel> const& one : team_)
            if (!one->pending_.empty()) {
                sent = true;
                one->flush();
            }
        if (sent) round_trips_++;
    }

    void closing_checks(double fraction) {
        bool heard = false;
        for (std::unique_ptr<channel> const& one : team_) {
            if (one->inside().read_anything()) heard = true;
            if (!one->spoken_to_)
                detail::warn("EO408", fmt("instance {} was started and never talked to", one->index_),
                             "spawn it where it is needed, or drop it", detail::site::here());
        }
        if (fraction > 0 && !heard && !team_.empty())
            detail::warn("EO405", "the controller accepted without reading anything from any instance",
                         "read what they sent", detail::site::here());
        if (round_trips_ > 10000 && budgets_ == 0)
            detail::warn("EO402", fmt("{} round trips were answered with no eo::budget declared",
                                      round_trips_),
                         "declare the statement's limit with eo::budget", detail::site::here());
        if (budgets_ > 0 && !budget_spent_)
            detail::warn("EO403", "a budget was declared and never spent",
                         "spend it before every reply, or drop it", detail::site::here());
    }


    [[noreturn]] void deliver(int code, std::string text) {
        delivered_ = true;
        for (std::unique_ptr<channel> const& one : team_)
            if (one) one->hand_over();
        detail::finish(code, text);
    }

    std::string paths_[3];
    std::vector<std::unique_ptr<channel>> team_;
    summary held_;
    eo::rng dice_{0};
    std::FILE* requests_ = nullptr;
    std::FILE* replies_ = nullptr;
    long long round_trips_ = 0;
    long long sent_bytes_ = 0;
    long long budgets_ = 0;
    bool seeded_ = false;
    bool reported_ = false;
    bool budget_spent_ = false;
};

inline void channel::flush() {
    if (pending_.empty() || shut_) return;
    spoken_to_ = true;
    if (!deaf_)
        detail::write_while_absorbing(
            writes_, pending_, inside(), deaf_, [this] { return fmt("instance {}", index_); }, "controller",
            "read the instances' answers between sends instead of sending everything first");
    owner_->sent_bytes_ += static_cast<long long>(pending_.size());
    pending_.clear();
}

inline void channel::hand_over() {
    if (pending_.empty() || shut_ || deaf_) return;
    detail::write_while_read(writes_, pending_, detail::last_words_patience_ms, detail::last_words_deadline_ms);
    pending_.clear();
}

inline void channel::close() {
    if (shut_) return;
    flush();
    shut_ = true;
    if (writes_ >= 0) ::close(writes_);
    writes_ = -1;
}

}  // namespace eo
