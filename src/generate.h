#pragma once

#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "core.h"
#include "diag.h"
#include "fmt.h"
#include "parse.h"
#include "random.h"
#include "read.h"

namespace eo {

class generator;

namespace detail {

inline generator*& live_generator() {
    static generator* only = nullptr;
    return only;
}

struct declared_option {
    std::string name;
    std::string kind;
    std::string range;
    std::string fallback;
};

inline bool looks_like_a_seed(std::string const& word) {
    if (word.size() != 16) return false;
    for (char const one : word)
        if (!((one >= '0' && one <= '9') || (one >= 'a' && one <= 'f'))) return false;
    return true;
}

}  // namespace detail

class generator {
public:
    generator(int argc, char** argv, detail::site where = detail::site::here()) {
        if (detail::live_generator() != nullptr)
            detail::library_error(fmt("{}: this program already has a generator", detail::where_of(where)));
        detail::diagnostics::shared().start_the_clock("EO504", "generator", 60000, where);
        std::string all;
        for (int at = 1; at < argc; at++) {
            std::string const word = argv[at];
            all += word;
            all.push_back('\0');
            if (at == argc - 1 && detail::looks_like_a_seed(word)) {
                stress_ = true;
                continue;
            }
            if (word == "--eo-describe") {
                describing_ = true;
                continue;
            }
            std::size_t const split = word.find('=');
            if (word.size() < 2 || word[0] != '-' || split == std::string::npos || split < 2)
                detail::library_error(fmt("{} is not an option; write -name=value", word));
            given_[word.substr(1, split - 1)] = word.substr(split + 1);
        }
        base_ = detail::seed_of(all);
        dice_.emplace("", eo::rng(base_));
        std::fflush(stdout);
        struct stat towards {};
        if (::fstat(1, &towards) == 0 && S_ISREG(towards.st_mode)) started_ = ::lseek(1, 0, SEEK_CUR);
        detail::log_file() = stderr;
        detail::emitter() = &generator::say;
        out.owner_ = this;
        detail::live_generator() = this;
    }

    generator(generator const&) = delete;
    generator& operator=(generator const&) = delete;

    ~generator() noexcept(false) {
        detail::restore_channels afterwards;
        detail::live_generator() = nullptr;
        if (std::uncaught_exceptions() != 0) return;
        out.flush();
        if (describing_) describe();
        every_option_was_asked_for();
        closing_warnings();
        detail::diagnostics::shared().emit();
    }

    template <class T>
    [[nodiscard]] T option(std::string name, T low, T high,
                           detail::site where = detail::site::here()) {
        declare(name, kind_of<T>(), fmt("{}..{}", low, high), "");
        std::string const* const found = look(name);
        if (found == nullptr && describing_) return low;
        if (found == nullptr) refuse(fmt("-{} is required", name));
        return bounded<T>(name, *found, low, high, where);
    }

    template <class T>
    [[nodiscard]] T option(std::string name, T low, T high, T fallback,
                           detail::site where = detail::site::here()) {
        declare(name, kind_of<T>(), fmt("{}..{}", low, high), fmt("{}", fallback));
        std::string const* const found = look(name);
        if (found == nullptr) return fallback;
        return bounded<T>(name, *found, low, high, where);
    }

    template <class T>
    [[nodiscard]] T option(std::string name, std::initializer_list<char const*> choices) {
        declare(name, "choice", listed(choices), "");
        std::string const* const found = look(name);
        if (found == nullptr && describing_) return T(*choices.begin());
        if (found == nullptr) refuse(fmt("-{} is required", name));
        return chosen<T>(name, *found, choices);
    }

    template <class T>
    [[nodiscard]] T option(std::string name, std::initializer_list<char const*> choices,
                           char const* fallback) {
        declare(name, "choice", listed(choices), fallback);
        std::string const* const found = look(name);
        if (found == nullptr) return T(fallback);
        return chosen<T>(name, *found, choices);
    }

    template <class T, class = std::enable_if_t<std::is_same_v<T, bool>>>
    [[nodiscard]] T option(std::string name, bool fallback) {
        declare(name, "flag", "true or false", fallback ? "true" : "false");
        std::string const* const found = look(name);
        if (found == nullptr) return fallback;
        if (*found == "true" || *found == "1") return true;
        if (*found == "false" || *found == "0") return false;
        refuse(fmt("-{}={} is not true or false", name, *found));
    }

    template <class... Args>
    void require(bool condition, detail::pattern pattern, Args const&... args) {
        if (!condition) refuse(fmt(pattern, args...));
    }

    eo::rng& rng(std::string label = "") {
        drew_ = true;
        if (label.empty()) used_the_default_ = true;
        else used_a_label_ = true;
        auto const found = dice_.find(label);
        if (found != dice_.end()) return found->second;
        std::uint64_t const from = detail::seed_of(label) * 0x9e3779b97f4a7c15ull ^ base_;
        return dice_.emplace(label, eo::rng(from)).first->second;
    }

    class sheet {
    public:
        template <class... Args>
        void line(Args const&... values) {
            bool first = true;
            (add(values, first), ...);
            held_.push_back('\n');
            if (held_.size() >= detail::mebibyte) flush();
        }

        void line() { put("\n"); }

        template <class Container>
        void lines(Container const& rows) {
            for (auto const& one : rows) line(one);
        }

        void flush() {
            if (held_.empty()) return;
            if (!owner_->describing_) std::fwrite(held_.data(), 1, held_.size(), stdout);
            owner_->written_ += static_cast<long long>(held_.size());
            held_.clear();
        }

    private:
        friend class generator;

        template <class T>
        void add(T const& value, bool& first) {
            if constexpr (detail::is_a_list<T>::value && !std::is_convertible_v<T const&, std::string_view>) {
                for (auto const& one : value) {
                    add(one, first);
                    if (held_.size() >= detail::mebibyte) flush();
                }
            } else {
                detail::add_to_line(held_, value, first);
            }
        }

        void put(std::string const& bytes) {
            held_ += bytes;
            if (held_.size() >= detail::mebibyte) flush();
        }

        generator* owner_ = nullptr;
        std::string held_;
    };

    sheet out;

private:
    template <class T>
    static char const* kind_of() {
        if constexpr (std::is_same_v<T, double>) return "a real number";
        else if constexpr (std::is_same_v<T, bool>) return "a flag";
        else return "an integer";
    }

    static std::string listed(std::initializer_list<char const*> choices) {
        std::string out_;
        for (char const* one : choices) out_ += (out_.empty() ? "" : ", ") + std::string(one);
        return out_;
    }

    static void say(std::string const& text) {
        std::fwrite(text.data(), 1, text.size(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }

    [[noreturn]] void refuse(std::string const& message) { detail::finish(3, message); }

    void declare(std::string const& name, char const* kind, std::string range, std::string fallback) {
        for (detail::declared_option const& one : shape_)
            if (one.name == name) return;
        shape_.push_back({name, kind, std::move(range), std::move(fallback)});
    }

    std::string const* look(std::string const& name) {
        asked_.insert(name);
        auto const found = given_.find(name);
        return found == given_.end() ? nullptr : &found->second;
    }

    template <class T>
    T bounded(std::string const& name, std::string const& text, T low, T high, detail::site where) {
        (void)where;
        if constexpr (std::is_same_v<T, double>) {
            detail::real_read const parsed = detail::parse_real(text, true);
            if (parsed.problem != detail::number_problem::none)
                refuse(fmt("-{}={} is not a real number", name, text));
            if (parsed.value < low) refuse(fmt("-{}={} is below {}", name, text, low));
            if (parsed.value > high) refuse(fmt("-{}={} is above {}", name, text, high));
            return static_cast<T>(parsed.value);
        } else {
            detail::integer_read const parsed = detail::parse_integer(text);
            if (parsed.problem != detail::number_problem::none)
                refuse(fmt("-{}={} is not an integer", name, text));
            if (parsed.value < static_cast<long long>(low))
                refuse(fmt("-{}={} is below {}", name, text, low));
            if (parsed.value > static_cast<long long>(high))
                refuse(fmt("-{}={} is above {}", name, text, high));
            return static_cast<T>(parsed.value);
        }
    }

    template <class T>
    T chosen(std::string const& name, std::string const& text, std::initializer_list<char const*> choices) {
        for (char const* one : choices)
            if (text == one) return T(one);
        refuse(fmt("-{}={} is not one of {}", name, text, listed(choices)));
    }

    void every_option_was_asked_for() {
        if (checked_) return;
        checked_ = true;
        if (describing_) return;
        for (auto const& one : given_)
            if (one.first != "seed" && asked_.count(one.first) == 0) {
                std::string known;
                for (detail::declared_option const& said : shape_)
                    known += (known.empty() ? "" : ", ") + ("-" + said.name);
                refuse(fmt("unknown option -{}; the options are {}", one.first,
                           known.empty() ? "none" : known));
            }
    }

    void closing_warnings() {
        every_option_was_asked_for();
        if (stress_ && !drew_)
            detail::warn("EO501", "this stress run made no random draw",
                         "every iteration would get the same test", where_of_run_);
        if (written_ > static_cast<long long>(detail::large_file))
            detail::warn("EO502", fmt("this test is {} bytes", written_),
                         "storage and judging time", where_of_run_);
        int const flushed = std::fflush(stdout);
        int const reason = errno;
        if (flushed != 0) detail::finish(3, fmt("the test could not be written: {}", std::strerror(reason)));
        if (std::ferror(stdout))
            detail::finish(3, "the test could not be written: an earlier write to stdout failed");
        long long const ended = ::lseek(1, 0, SEEK_CUR);
        if (!describing_ && started_ >= 0 && ended >= 0 && ended - started_ != written_)
            detail::warn("EO503", fmt("{} bytes reached stdout without going through g.out",
                                      ended - started_ - written_),
                         "write the test with g.out.line", where_of_run_);
        if (used_the_default_ && used_a_label_)
            detail::note("EO507", "the default random stream is used alongside named ones",
                         "an added draw shifts every later draw of the default stream", where_of_run_);
    }

    void describe() {
        std::string said;
        for (detail::declared_option const& one : shape_)
            said += fmt("eo-describe option {} {} {}{}\n", one.name, one.kind, one.range,
                        one.fallback.empty() ? "" : " default=" + one.fallback);
        std::fwrite(said.data(), 1, said.size(), stdout);
        std::fflush(stdout);
    }

    friend class sheet;

    std::map<std::string, std::string> given_;
    std::set<std::string> asked_;
    std::vector<detail::declared_option> shape_;
    std::map<std::string, eo::rng> dice_;
    std::uint64_t base_ = 0;
    long long started_ = -1;
    long long written_ = 0;
    bool stress_ = false;
    bool drew_ = false;
    bool describing_ = false;
    bool checked_ = false;
    bool used_the_default_ = false;
    bool used_a_label_ = false;
    detail::site where_of_run_{"generator", 0};
};

}  // namespace eo
