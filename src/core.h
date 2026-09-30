#pragma once

#include <cfenv>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#define EOLYMP_H_VERSION "2.2.1"
#define EOLYMP_H_VERSION_MAJOR 2
#define EOLYMP_H_VERSION_MINOR 2
#define EOLYMP_H_VERSION_PATCH 1

namespace eo {

inline char const* version() { return EOLYMP_H_VERSION; }

namespace detail {

constexpr long version_part(char const* text, int which) {
    for (; which > 0; which--) {
        while (*text != '.') text++;
        text++;
    }
    long value = 0;
    while (*text >= '0' && *text <= '9') value = value * 10 + (*text++ - '0');
    return value;
}

static_assert(version_part(EOLYMP_H_VERSION, 0) == EOLYMP_H_VERSION_MAJOR &&
                  version_part(EOLYMP_H_VERSION, 1) == EOLYMP_H_VERSION_MINOR &&
                  version_part(EOLYMP_H_VERSION, 2) == EOLYMP_H_VERSION_PATCH,
              "EOLYMP_H_VERSION and EOLYMP_H_VERSION_MAJOR, _MINOR and _PATCH disagree");

}

struct any_t {};
struct unnamed_t {};

inline constexpr any_t any{};
inline constexpr unnamed_t unnamed{};

namespace detail {

inline char const* environment(char const* name) { return std::getenv(name); }

inline bool environment_is(char const* name, char const* value) {
    char const* found = environment(name);
    return found != nullptr && std::strcmp(found, value) == 0;
}

inline bool on_judge() { return environment("EOLYMP") != nullptr; }

inline bool strict_mode() { return environment_is("EOLYMP_STRICT", "1"); }

inline std::size_t constexpr mebibyte = std::size_t{1} << 20;
inline std::size_t constexpr pipe_size = std::size_t{1} << 16;
inline std::size_t constexpr stored_log = std::size_t{1} << 16;
inline std::size_t constexpr large_file = 64 * mebibyte;

inline bool rounding_to_nearest() { return std::fegetround() == FE_TONEAREST; }

inline char const* decimal_point() { return std::localeconv()->decimal_point; }

inline std::string with_the_local_point(std::string text, char const* point = decimal_point()) {
    std::size_t const at = text.find('.');
    if (at != std::string::npos) text.replace(at, 1, point);
    return text;
}

inline void with_a_dot(std::string& text, std::size_t from, char const* point = decimal_point()) {
    std::size_t const at = std::strcmp(point, ".") == 0 ? std::string::npos : text.find(point, from);
    if (at != std::string::npos) text.replace(at, std::strlen(point), ".");
}

struct stop {
    int code;
    std::string text;
};

inline std::FILE*& log_file() {
    static std::FILE* where = stdout;
    return where;
}

inline void (*&after_a_log_line())(std::size_t) {
    static void (*hook)(std::size_t) = nullptr;
    return hook;
}

inline void log_line(std::string const& text) {
    std::fwrite(text.data(), 1, text.size(), log_file());
    std::fputc('\n', log_file());
    if (after_a_log_line() != nullptr) after_a_log_line()(text.size() + 1);
}

inline void (*&emitter())(std::string const&) {
    static void (*hook)(std::string const&) = nullptr;
    return hook;
}

inline void report(std::string const& text) {
    if (emitter() != nullptr) {
        emitter()(text);
        return;
    }
    if (!text.empty()) {
        std::fwrite(text.data(), 1, text.size(), stdout);
        if (text.back() != '\n') std::fputc('\n', stdout);
    }
    std::fflush(stdout);
}

inline void (*&epilogue())() {
    static void (*hook)() = nullptr;
    return hook;
}

class restore_channels {
public:
    restore_channels() = default;
    restore_channels(restore_channels const&) = delete;

    ~restore_channels() {
        emitter() = nullptr;
        log_file() = stdout;
    }
};

[[noreturn]] inline void finish(int code, std::string text) {
    report(text);
    if (epilogue() != nullptr) epilogue()();
#ifdef EOLYMP_TESTING
    throw stop{code, std::move(text)};
#else
    std::_Exit(code);
#endif
}

[[noreturn]] inline void library_error(std::string text) { finish(3, "eolymp.h: " + text); }

inline bool same_text(char const* left, char const* right) {
    if (left == right) return true;
    if (left == nullptr || right == nullptr) return false;
    return std::string(left) == std::string(right);
}

}  // namespace detail
}  // namespace eo
