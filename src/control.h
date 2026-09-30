#pragma once

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "interact.h"
#include "io.h"
#include "os.h"
#include "random.h"
#include "role.h"
#include "stream.h"
#include "summary.h"

namespace eo {

class controller;

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

class controller final : public detail::dialogue<controller> {
public:
    controller(int argc, char** argv, detail::site where = detail::site::here())
        : dialogue(argc, argv, "a controller", "say what an instance did", where) {
        if (!paths_[2].empty())
            jury = stream(detail::source::over_file(paths_[2].c_str(), true), detail::fault::jury_error,
                          "answer.txt");
        begin();
    }

    ~controller() noexcept(false) {
        detail::restore_channels afterwards;
        let_go();
        team_.clear();
        if (requests_ != nullptr) std::fclose(requests_);
        if (replies_ != nullptr) std::fclose(replies_);
        requests_ = nullptr;
        replies_ = nullptr;
        fail_closed("controller");
    }

    char const* called() const final { return "controller"; }

    long long instance_limit() const { return detail::environment_integer("INSTANCE_LIMIT"); }

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
        made->writes_ = detail::open_to_write(to_them.c_str());
        if (made->writes_ < 0) fail_jury(fmt("cannot write to instance {}", made->index_));
        std::string const named = fmt("instance {}", made->index_);
        detail::source listening = detail::source::over_channel(from_them.c_str());
        static_cast<stream&>(*made) = stream(std::move(listening), detail::fault::wrong_answer, named);
        made->inside().before_blocking(&controller::flush_from, this);
        made->inside().on_end(fmt("instance {} ended the dialogue early", made->index_));
        team_.push_back(std::move(made));
        return *team_.back();
    }

private:
    friend class channel;
    friend class detail::dialogue<controller>;

    static void flush_from(void* owner) { static_cast<controller*>(owner)->flush_everything(); }

    static void exited_early() {
        controller* const one = current();
        if (one != nullptr && !one->delivered_) one->fail_jury(
            "the controller ended without a verdict: exit() was called, or the controller was never destroyed");
    }

    void open_the_control() {
        if (requests_ != nullptr) return;
        char const* const out = detail::environment("CONTROL_OUTPUT_FILE");
        char const* const in = detail::environment("CONTROL_INPUT_FILE");
        if (out == nullptr || in == nullptr)
            fail_jury("this problem is not set up for instances: there is no control channel");
        requests_ = std::fopen(out, "wb");
        if (requests_ == nullptr) fail_jury("cannot reach the judge's control channel");
        replies_ = std::fopen(in, "rb");
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
        budget_checks();
    }

    std::string traffic() const {
        return fmt("{} instances, {} round trips, {} bytes sent", team_.size(), round_trips_, sent_bytes_);
    }

    void last_words() {
        for (std::unique_ptr<channel> const& one : team_)
            if (one) one->hand_over();
    }

    std::vector<std::unique_ptr<channel>> team_;
    std::FILE* requests_ = nullptr;
    std::FILE* replies_ = nullptr;
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
    if (writes_ >= 0) detail::close_descriptor(writes_);
    writes_ = -1;
}

}  // namespace eo
