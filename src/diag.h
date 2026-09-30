#pragma once

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "core.h"
#include "fmt.h"

namespace eo {
namespace detail {

enum class severity { note, warning };

struct site {
    char const* file;
    int line;

    static site here(char const* file = __builtin_FILE(), int line = __builtin_LINE()) {
        return site{file, line};
    }
};

inline std::string where_of(site place) { return fmt("{}:{}", place.file, place.line); }

inline std::string json_string(std::string const& text) {
    char const* const digits = "0123456789abcdef";
    std::string out = "\"";
    for (char const one : text) {
        unsigned char const byte = static_cast<unsigned char>(one);
        if (one == '"' || one == '\\') {
            out += '\\';
            out += one;
        } else if (byte < 0x20) {
            out += "\\u00";
            out += digits[byte >> 4];
            out += digits[byte & 15];
        } else {
            out += one;
        }
    }
    return out + "\"";
}

struct raised {
    char const* code;
    severity level;
    site where;
    std::string message;
    std::string fix;
    long long count;
};

struct allowance {
    std::string code;
    std::string reason;
    site where;
    long long count;
};

struct time_budget {
    char const* code = nullptr;
    char const* role = nullptr;
    long long limit_ms = 0;
    site where{nullptr, 0};
    std::chrono::steady_clock::time_point started{};
};

inline std::size_t constexpr report_limit = 30;

class diagnostics {
public:
    static diagnostics& shared() {
        static diagnostics only;
        epilogue() = &diagnostics::emit_from_hook;
        return only;
    }

    bool raise(char const* code, severity level, std::string message, std::string fix, site where) {
        bool const fresh = record(code, level, message, std::move(fix), where);
        if (fresh && level == severity::warning && strict_mode())
            finish(3, fmt("{}: {} {}: {}", where_of(where), "strict mode stops at", code, message));
        return fresh;
    }

    bool again(char const* code, site where) {
        for (allowance& permitted : allowed_)
            if (permitted.code == code) {
                permitted.count++;
                return true;
            }
        for (raised& already : entries_)
            if ((already.code == code || std::strcmp(already.code, code) == 0) && already.where.line == where.line &&
                same_text(already.where.file, where.file)) {
                already.count++;
                return true;
            }
        return false;
    }

    void start_the_clock(char const* code, char const* role, long long limit_ms, site where) {
        clock_ = time_budget{code, role, limit_ms, where, std::chrono::steady_clock::now()};
    }

    time_budget& clock() { return clock_; }

    void allow_code(std::string code, std::string reason, site where) {
        if (reason.empty()) library_error(fmt("{}: eo::allow(\"{}\") needs a reason", where_of(where), code));
        allowed_.push_back({std::move(code), std::move(reason), where, 0});
    }

    void forget_code() {
        silenced_.push_back(allowed_.back());
        allowed_.pop_back();
    }

    bool anything() const { return !entries_.empty() || !silenced_.empty(); }

    std::string local_block() {
        std::string out;
        for (raised const& one : ordered())
            out += fmt("{}: {} {}: {}{}\n  {}\n", where_of(one.where), word(one.level), one.code, one.message,
                       times(one.count), one.fix);
        out += overflow();
        for (allowance const& one : silenced_)
            if (one.count > 0)
                out += fmt("{}: silenced {}{}: {}\n", where_of(one.where), one.code, times(one.count), one.reason);
        return out;
    }

    std::string judge_lines() {
        std::string out;
        for (raised const& one : ordered())
            out += fmt("{} {} {} {}{}\n", word(one.level), one.code, where_of(one.where), one.message,
                       times(one.count));
        out += overflow();
        return out;
    }

    std::string report_line() {
        std::string out = "eo-report {\"version\":1,\"warnings\":[";
        bool first = true;
        for (raised const& one : entries_) {
            out += fmt("{}{{\"code\":\"{}\",\"at\":{},\"count\":{}}}", first ? "" : ",", one.code,
                       json_string(where_of(one.where)), one.count);
            first = false;
        }
        out += "]}\n";
        return out;
    }

    void emit() {
        if (emitted_) return;
        look_at_the_clock();
        if (!anything()) return;
        emitted_ = true;
        std::string const text = on_judge() ? judge_lines() + report_line() : local_block();
        std::FILE* const target = on_judge() ? log_file() : stderr;
        std::fwrite(text.data(), 1, text.size(), target);
        std::fflush(target);
    }

    std::vector<raised> const& all() const { return entries_; }

    bool raised_already(char const* code) const {
        for (raised const& one : entries_)
            if (std::string(one.code) == code) return true;
        return false;
    }

    void forget_everything() {
        entries_.clear();
        allowed_.clear();
        silenced_.clear();
        emitted_ = false;
        clock_ = time_budget{};
    }

private:
    static void emit_from_hook() { shared().emit(); }

    bool record(char const* code, severity level, std::string const& message, std::string fix, site where) {
        if (again(code, where)) return false;
        entries_.push_back({code, level, where, message, std::move(fix), 1});
        return true;
    }

    void look_at_the_clock() {
        if (clock_.code == nullptr) return;
        long long const spent = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                           std::chrono::steady_clock::now() - clock_.started)
                                                           .count());
        if (spent * 2 > clock_.limit_ms)
            record(clock_.code, severity::warning,
                   fmt("the {} ran for {} ms of its {} ms limit", clock_.role, spent, clock_.limit_ms),
                   "a slower machine or a busy judge would not finish it in time", clock_.where);
        clock_.code = nullptr;
    }

    static char const* word(severity level) { return level == severity::warning ? "warning" : "note"; }

    static std::string times(long long count) { return count > 1 ? fmt(" ({} times)", count) : std::string(); }

    std::vector<raised> ordered() const {
        std::vector<raised> sorted;
        for (severity const level : {severity::warning, severity::note})
            for (raised const& one : entries_)
                if (one.level == level && sorted.size() < report_limit) sorted.push_back(one);
        return sorted;
    }

    std::string overflow() const {
        if (entries_.size() <= report_limit) return {};
        return fmt("... and {} more\n", entries_.size() - report_limit);
    }

    std::vector<raised> entries_;
    std::vector<allowance> allowed_;
    std::vector<allowance> silenced_;
    bool emitted_ = false;
    time_budget clock_;
};

inline void (*&unfinished())() {
    static void (*hook)() = nullptr;
    return hook;
}

inline void finish_what_exit_left() {
    if (unfinished() != nullptr) unfinished()();
}

inline void close_on_quick_exit() {
#if !defined(__APPLE__) && !(defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_AT_QUICK_EXIT))
    std::at_quick_exit(&finish_what_exit_left);
#endif
}

inline void close_on_exit(void (*closer)()) {
    diagnostics::shared();
    static bool const registered = (std::atexit(&finish_what_exit_left), close_on_quick_exit(), true);
    (void)registered;
    unfinished() = closer;
}

inline void warn(char const* code, std::string message, std::string fix, site where) {
    diagnostics::shared().raise(code, severity::warning, std::move(message), std::move(fix), where);
}

inline void warn_at_once(char const* code, std::string message, std::string fix, site where) {
    std::string const line = fmt("warning {} {} {}\n", code, where_of(where), message);
    if (diagnostics::shared().raise(code, severity::warning, std::move(message), std::move(fix), where)) {
        std::fwrite(line.data(), 1, line.size(), stderr);
        std::fflush(stderr);
    }
}

inline void note(char const* code, std::string message, std::string fix, site where) {
    diagnostics::shared().raise(code, severity::note, std::move(message), std::move(fix), where);
}

inline void report_a_bad_pattern(std::string const& problem, char const* file, int line) {
    warn("EO112", problem, "write one {} for each value and {{ or }} for a brace; the verdict stands",
         site{file, line});
}

inline bool const bad_patterns_are_reported = (bad_pattern_hook() = &report_a_bad_pattern, true);

}  // namespace detail

class allow {
public:
#if defined(__clang__) || __GNUC__ >= 10
    [[nodiscard]]
#endif
    allow(std::string code, std::string reason, char const* file = __builtin_FILE(),
          int line = __builtin_LINE()) {
        detail::diagnostics::shared().allow_code(std::move(code), std::move(reason), detail::site{file, line});
    }

    allow(allow const&) = delete;
    allow& operator=(allow const&) = delete;

    ~allow() { detail::diagnostics::shared().forget_code(); }
};

}  // namespace eo
