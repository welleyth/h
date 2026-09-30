#pragma once

#include <algorithm>
#include <array>
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
#include "role.h"
#include "stream.h"
#include "summary.h"

namespace eo {

namespace detail {

template <class Role>
class dialogue : public scorer, public limits_keeper {
public:
    dialogue(dialogue const&) = delete;
    dialogue& operator=(dialogue const&) = delete;

    stream input;
    stream jury;

    bool has_jury() const { return !paths_[2].empty(); }

    void value(std::string name, double what) { held_.record(std::move(name), what); }

    eo::rng& rng() {
        if (!seeded_) {
            dice_ = eo::rng(kept_test_.empty() ? seed_of_file(paths_[0].c_str()) : seed_of(kept_test_));
            seeded_ = true;
        }
        return dice_;
    }

    long long round_trips() const { return round_trips_; }

    void declare_budget() final { budgets_++; }
    void spent_a_budget() final { budget_spent_ = true; }

    [[noreturn]] void pass(double fraction, std::string const& message) final {
        if (std::isnan(fraction)) refuse_a_score(fmt("a score of {}", fraction));
        role().closing_checks(fraction);
        held_.set_fraction(std::min(fraction, 1.0));
        held_.set_message(message);
        write_file(paths_[1], held_.written(), "summary");
        deliver(0, message.empty() ? "ok" : "ok " + message);
    }

    [[noreturn]] void fail_run(std::string const& message) final {
        if (message.empty()) warn("EO204", "this wrong answer carries no message", silence_fix_, site::here());
        deliver(1, message.empty() ? "wrong answer" : "wrong answer " + message);
    }

    [[noreturn]] void fail_jury(std::string const& message) final {
        deliver(3, message.empty() ? "jury error" : "jury error " + message);
    }

    void report_traffic() {
        if (reported_) return;
        reported_ = true;
        log_line(role().traffic());
        if (round_trips_ > 100000)
            diagnostics::shared().raise("EO401", round_trips_ > 500000 ? severity::warning : severity::note,
                                        fmt("this run made {} round trips", round_trips_),
                                        "a pipe manages about 150,000 a second", site::here());
    }

protected:
    dialogue(int argc, char** argv, char const* named, char const* silence_fix, site where)
        : silence_fix_(silence_fix) {
        if (live() != nullptr) library_error(fmt("{}: this program already has {}", where_of(where), named));
        std::array<char const*, 3> const given = test_paths(argc, argv);
        for (int at = 0; at < 3; at++)
            if (given[static_cast<std::size_t>(at)] != nullptr) paths_[at] = given[static_cast<std::size_t>(at)];
        if (paths_[0].empty() || paths_[1].empty())
            library_error(fmt("{}: {} needs the test and a file for its summary", where_of(where), named));
        ::signal(SIGPIPE, SIG_IGN);
        log_file() = stderr;
        emitter() = &dialogue::say;
        input = stream(source::over_file(paths_[0].c_str(), true), fault::jury_error, "input.txt");
    }

    ~dialogue() = default;

    void begin() {
        live() = &role();
        live_scorer() = this;
    }

    void let_go() {
        live() = nullptr;
        live_scorer() = nullptr;
        current_case() = 0;
    }

    void budget_checks() {
        if (round_trips_ > 10000 && budgets_ == 0)
            warn("EO402", fmt("{} round trips were answered with no eo::budget declared", round_trips_),
                 "declare the statement's limit with eo::budget", site::here());
        if (budgets_ > 0 && !budget_spent_)
            warn("EO403", "a budget was declared and never spent", "spend it before every reply, or drop it",
                 site::here());
    }

    [[noreturn]] void deliver(int code, std::string text) {
        delivered_ = true;
        role().last_words();
        finish(code, text);
    }

    std::string paths_[3];
    std::string kept_test_;
    summary held_;
    long long round_trips_ = 0;
    long long sent_bytes_ = 0;

private:
    static Role*& live() {
        static Role* only = nullptr;
        return only;
    }

    static void say(std::string const& text) {
        std::fwrite(text.data(), 1, text.size(), stderr);
        std::fputc('\n', stderr);
        if (live() != nullptr) live()->report_traffic();
        std::fflush(stderr);
    }

    Role& role() { return static_cast<Role&>(*this); }

    char const* silence_fix_;
    eo::rng dice_{0};
    long long budgets_ = 0;
    bool seeded_ = false;
    bool reported_ = false;
    bool budget_spent_ = false;
};

}  // namespace detail

class interactor final : public detail::dialogue<interactor> {
public:
    interactor(int argc, char** argv, detail::site where = detail::site::here())
        : dialogue(argc, argv, "an interactor", "say what the solution did", where) {
        if (!paths_[2].empty() && detail::file_is_there(paths_[2].c_str()))
            jury = stream(detail::source::over_file(paths_[2].c_str(), true), detail::fault::jury_error,
                          "answer.txt");
        contestant = stream(detail::source::over_descriptor(0, false, false), detail::fault::wrong_answer,
                            "the solution");
        contestant.inside().before_blocking(&interactor::flush_from, this);
        contestant.inside().on_end("the solution ended the dialogue early");
        begin();
    }

    ~interactor() noexcept(false) {
        detail::restore_channels afterwards;
        let_go();
        fail_closed("interactor");
    }

    stream contestant;

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

private:
    friend class phases;
    friend class detail::dialogue<interactor>;

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

    static void flush_from(void* owner) { static_cast<interactor*>(owner)->waiting_and_flush(); }

    void write_while_listening() {
        detail::write_while_absorbing(
            1, pending_, contestant.inside(), deaf_, [] { return std::string("the solution"); }, "interactor",
            "read the solution's answers between sends instead of sending everything first");
    }

    void waiting_and_flush() {
        waiting_ = true;
        flush();
    }

    void closing_checks(double fraction) {
        if (fraction > 0 && !passing_ && !contestant.inside().read_anything())
            detail::warn("EO405", "the interactor accepted without reading anything from the solution",
                         "read what it sent, or use a plain problem", detail::site::here());
        if (has_jury() && !jury.inside().read_anything())
            detail::note("EO406", "the test has an answer file that the interactor never read",
                         "drop it, or read it", detail::site::here());
        budget_checks();
        if (contestant.inside().content_waiting())
            detail::note("EO404", "the solution was still sending when the interactor finished",
                         "the protocol has a step the statement does not describe", detail::site::here());
    }

    std::string traffic() const { return fmt("{} round trips, {} bytes sent", round_trips_, sent_bytes_); }

    void last_words() {
        if (!deaf_) detail::write_while_read(1, pending_, detail::last_words_patience_ms,
                                                detail::last_words_deadline_ms);
        pending_.clear();
    }

    std::string pending_;
    bool deaf_ = false;
    bool waiting_ = false;
    bool passing_ = false;
};

}  // namespace eo
