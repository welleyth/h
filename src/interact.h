#pragma once

#include <algorithm>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include <poll.h>
#include <unistd.h>

#include "check.h"
#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "io.h"
#include "random.h"
#include "stream.h"
#include "summary.h"

namespace eo {

class interactor;

namespace detail {

inline interactor*& live_interactor() {
    static interactor* only = nullptr;
    return only;
}


}  // namespace detail

class interactor final : public detail::scorer, public detail::limits_keeper {
public:
    interactor(int argc, char** argv, detail::site where = detail::site::here()) {
        if (detail::live_interactor() != nullptr)
            detail::library_error(fmt("{}: this program already has an interactor", detail::where_of(where)));
        std::vector<std::string> named;
        for (int at = 1; at < argc; at++) named.emplace_back(argv[at]);
        char const* const from_env[3] = {detail::environment("INPUT_FILE"), detail::environment("OUTPUT_FILE"),
                                         detail::environment("ANSWER_FILE")};
        for (int at = 0; at < 3; at++) {
            if (from_env[at] != nullptr) paths_[at] = from_env[at];
            else if (named.size() > static_cast<std::size_t>(at))
                paths_[at] = named[static_cast<std::size_t>(at)];
        }
        if (paths_[0].empty() || paths_[1].empty())
            detail::library_error(fmt("{}: an interactor needs the test and a file for its summary",
                                      detail::where_of(where)));
        ::signal(SIGPIPE, SIG_IGN);
        detail::log_file() = stderr;
        detail::emitter() = &interactor::say;
        input = stream(detail::source::over_file(paths_[0].c_str(), true), detail::fault::jury_error,
                       "input.txt");
        if (!paths_[2].empty() && detail::file_is_there(paths_[2].c_str()))
            jury = stream(detail::source::over_file(paths_[2].c_str(), true), detail::fault::jury_error,
                          "answer.txt");
        contestant = stream(detail::source::over_descriptor(0, false, false), detail::fault::wrong_answer,
                            "the solution");
        contestant.inside().before_blocking(&interactor::flush_from, this);
        contestant.inside().on_end("the solution ended the dialogue early");
        detail::live_interactor() = this;
        detail::live_scorer() = this;
    }

    interactor(interactor const&) = delete;
    interactor& operator=(interactor const&) = delete;

    ~interactor() noexcept(false) {
        detail::restore_channels afterwards;
        detail::live_interactor() = nullptr;
        detail::live_scorer() = nullptr;
        detail::current_case() = 0;
        if (delivered_) return;
        if (std::uncaught_exceptions() == 0) fail_jury("the interactor ended without a verdict");
#ifndef EOLYMP_TESTING
        fail_jury("an exception left the interactor before its verdict; catch it inside the interactor's scope "
                  "and give a verdict there, or let it end the program");
#endif
    }

    stream input;
    stream jury;
    stream contestant;

    bool has_jury() const { return !paths_[2].empty(); }

    template <class... Args>
    void send(Args const&... values) {
        std::string line;
        bool first = true;
        (detail::add_to_line(line, values, first), ...);
        line.push_back('\n');
        pending_ += line;
        sent_bytes_ += static_cast<long long>(line.size());
        if (pending_.size() >= 1u << 16) flush();
    }

    void flush() {
        if (pending_.empty()) return;
        if (waiting_) round_trips_++;
        waiting_ = false;
        if (!deaf_) write_while_listening();
        pending_.clear();
    }

    double cost() const final { return detail::test_cost(); }

    void value(std::string name, double what) { held_.record(std::move(name), what); }

    eo::rng& rng() {
        if (!seeded_) {
            dice_ = eo::rng(kept_test_.empty() ? detail::seed_of_file(paths_[0].c_str())
                                               : detail::seed_of(kept_test_));
            seeded_ = true;
        }
        return dice_;
    }

    long long round_trips() const { return round_trips_; }


    [[noreturn]] void pass(double fraction, std::string const& message) final {
        if (std::isnan(fraction)) detail::refuse_a_score(fmt("a score of {}", fraction));
        closing_checks(fraction);
        held_.set_fraction(std::min(fraction, 1.0));
        held_.set_message(message);
        put_the_summary_down();
        deliver(0, message.empty() ? "ok" : "ok " + message);
    }

    [[noreturn]] void fail_run(std::string const& message) final {
        if (message.empty())
            detail::warn("EO204", "this wrong answer carries no message", "say what the solution did",
                         detail::site::here());
        deliver(1, message.empty() ? "wrong answer" : "wrong answer " + message);
    }

    [[noreturn]] void fail_jury(std::string const& message) final {
        deliver(3, message.empty() ? "jury error" : "jury error " + message);
    }

private:
    friend class phases;

    std::string whole_input() const {
        detail::source reading = detail::source::over_file(paths_[0].c_str(), true);
        std::string bytes;
        for (int one = reading.take(); one >= 0; one = reading.take())
            bytes.push_back(static_cast<char>(one));
        return bytes;
    }

    void read_the_test_again(std::string const& bytes) {
        kept_test_ = bytes;
        input = stream(detail::source::over_text(kept_test_, true), detail::fault::jury_error, "input.txt");
    }

    [[noreturn]] void hand_the_file_on(std::string const& bytes) {
        detail::write_file(paths_[1], bytes, "handoff");
        deliver(0, "ok handed on to the next phase");
    }

    void passing_through() { passing_ = true; }
public:
    void declare_budget() final { budgets_++; }
    void spent_a_budget() final { budget_spent_ = true; }

private:

    static void flush_from(void* owner) { static_cast<interactor*>(owner)->waiting_and_flush(); }

    static void say(std::string const& text) {
        std::fwrite(text.data(), 1, text.size(), stderr);
        std::fputc('\n', stderr);
        if (detail::live_interactor() != nullptr) detail::live_interactor()->report_traffic();
        std::fflush(stderr);
    }

    void write_while_listening() {
        detail::write_while_absorbing(
            1, pending_, contestant.inside(), deaf_, [] { return std::string("the solution"); }, "interactor",
            "read the solution's answers between sends instead of sending everything first");
    }

    void waiting_and_flush() {
        waiting_ = true;
        flush();
    }

    bool solution_still_talking() { return contestant.inside().content_waiting(); }

    void closing_checks(double fraction) {
        if (fraction > 0 && !passing_ && !contestant.inside().read_anything())
            detail::warn("EO405", "the interactor accepted without reading anything from the solution",
                         "read what it sent, or use a plain problem", detail::site::here());
        if (has_jury() && !jury.inside().read_anything())
            detail::note("EO406", "the test has an answer file that the interactor never read",
                         "drop it, or read it", detail::site::here());
        if (round_trips_ > 10000 && budgets_ == 0)
            detail::warn("EO402", fmt("{} round trips were answered with no eo::budget declared",
                                      round_trips_),
                         "declare the statement's limit with eo::budget", detail::site::here());
        if (budgets_ > 0 && !budget_spent_)
            detail::warn("EO403", "a budget was declared and never spent",
                         "spend it before every reply, or drop it", detail::site::here());
        if (solution_still_talking())
            detail::note("EO404", "the solution was still sending when the interactor finished",
                         "the protocol has a step the statement does not describe", detail::site::here());
    }

public:
    void report_traffic() {
        if (reported_) return;
        reported_ = true;
        detail::log_line(fmt("{} round trips, {} bytes sent", round_trips_, sent_bytes_));
        if (round_trips_ > 100000)
            detail::diagnostics::shared().raise(
                "EO401", round_trips_ > 500000 ? detail::severity::warning : detail::severity::note,
                fmt("this run made {} round trips", round_trips_),
                "a pipe manages about 150,000 a second", detail::site::here());
    }

private:

    void put_the_summary_down() {
        detail::write_file(paths_[1], held_.written(), "summary");
    }

    [[noreturn]] void deliver(int code, std::string text) {
        delivered_ = true;
        if (!deaf_) detail::write_while_read(1, pending_, detail::last_words_patience_ms,
                                                detail::last_words_deadline_ms);
        pending_.clear();
        detail::finish(code, text);
    }

    std::string paths_[3];
    std::string kept_test_;
    std::string pending_;
    summary held_;
    eo::rng dice_{0};
    bool seeded_ = false;
    bool delivered_ = false;
    bool deaf_ = false;
    bool reported_ = false;
    bool waiting_ = false;
    bool budget_spent_ = false;
    bool passing_ = false;
    long long round_trips_ = 0;
    long long sent_bytes_ = 0;
    long long budgets_ = 0;
};

}  // namespace eo
