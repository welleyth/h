#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#define EOLYMP_H_VERSION "2.1.0"
#define EOLYMP_H_VERSION_MAJOR 2
#define EOLYMP_H_VERSION_MINOR 1
#define EOLYMP_H_VERSION_PATCH 0

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

struct stop {
    int code;
    std::string text;
};

inline std::FILE*& log_file() {
    static std::FILE* where = stdout;
    return where;
}

inline void log_line(std::string const& text) {
    std::fwrite(text.data(), 1, text.size(), log_file());
    std::fputc('\n', log_file());
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
