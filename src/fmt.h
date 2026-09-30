#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "core.h"

namespace eo {
namespace detail {

inline void append_integer(std::string& out, long long value) {
    char buffer[24];
    std::to_chars_result const written = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, static_cast<std::size_t>(written.ptr - buffer));
}

inline void append_unsigned(std::string& out, unsigned long long value) {
    char buffer[24];
    std::to_chars_result const written = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, static_cast<std::size_t>(written.ptr - buffer));
}

inline bool append_non_finite(std::string& out, double value) {
    if (std::isfinite(value)) return false;
    if (std::isnan(value)) out.append(std::signbit(value) ? "-nan" : "nan");
    else out.append(value < 0 ? "-inf" : "inf");
    return true;
}

inline void append_real(std::string& out, double value) {
    if (append_non_finite(out, value)) return;
    char buffer[48];
#if defined(__cpp_lib_to_chars)
    char* end = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 15).ptr;
    double back = 0;
    std::from_chars(buffer, end, back);
    if (back != value)
        end = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 17).ptr;
    out.append(buffer, static_cast<std::size_t>(end - buffer));
#else
    int written = std::snprintf(buffer, sizeof(buffer), "%.15g", value);
    if (std::strtod(buffer, nullptr) != value)
        written = std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    out.append(buffer, static_cast<std::size_t>(written));
#endif
}

template <class T>
struct fixed_number {
    T value;
    int digits;
};

template <class T>
struct is_fixed : std::false_type {};

template <class T>
struct is_fixed<fixed_number<T>> : std::true_type {};

inline void append_fixed(std::string& out, double value, int digits) {
    if (append_non_finite(out, value)) return;
    char buffer[64];
    std::size_t const written =
        static_cast<std::size_t>(std::snprintf(buffer, sizeof(buffer), "%.*f", digits, value));
    if (written < sizeof(buffer)) {
        out.append(buffer, written);
        return;
    }
    std::size_t const at = out.size();
    out.resize(at + written + 1);
    std::snprintf(&out[at], written + 1, "%.*f", digits, value);
    out.resize(at + written);
}

template <class T>
inline void append_value(std::string& out, T const& value) {
    using plain = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (is_fixed<plain>::value) {
        append_fixed(out, static_cast<double>(value.value), value.digits);
    } else if constexpr (std::is_same_v<plain, bool>) {
        out.append(value ? "true" : "false");
    } else if constexpr (std::is_same_v<plain, char>) {
        out.push_back(value);
    } else if constexpr (std::is_same_v<plain, char const*> || std::is_same_v<plain, char*>) {
        if (value == nullptr) out.append("(null)");
        else out.append(std::string_view(value));
    } else if constexpr (std::is_floating_point_v<plain>) {
        append_real(out, static_cast<double>(value));
    } else if constexpr (std::is_integral_v<plain> && std::is_signed_v<plain>) {
        append_integer(out, static_cast<long long>(value));
    } else if constexpr (std::is_integral_v<plain>) {
        append_unsigned(out, static_cast<unsigned long long>(value));
    } else {
        out.append(std::string_view(value));
    }
}

template <class T, class = void>
struct is_a_list : std::false_type {};

template <class T>
struct is_a_list<T, std::void_t<decltype(std::declval<T const&>().begin()),
                                decltype(std::declval<T const&>().end())>> : std::true_type {};

template <class T>
inline void add_to_line(std::string& line, T const& value, bool& first) {
    if constexpr (is_a_list<T>::value && !std::is_convertible_v<T const&, std::string_view>) {
        for (auto const& one : value) add_to_line(line, one, first);
    } else {
        if (!first) line.push_back(' ');
        first = false;
        append_value(line, value);
    }
}

using appender = void (*)(std::string&, void const*);

class pattern {
public:
    template <class T, class = std::enable_if_t<std::is_convertible_v<T const&, std::string_view>>>
    pattern(T const& text, char const* file = __builtin_FILE(), int line = __builtin_LINE())
        : text_(text), file_(file), line_(line) {}

    std::string_view text() const { return text_; }
    char const* file() const { return file_; }
    int line() const { return line_; }

private:
    std::string_view text_;
    char const* file_;
    int line_;
};

inline void (*&bad_pattern_hook())(std::string const&, char const*, int) {
    static void (*hook)(std::string const&, char const*, int) = nullptr;
    return hook;
}

template <class T>
inline void append_erased(std::string& out, void const* value) {
    append_value(out, *static_cast<T const*>(value));
}

inline std::string assemble(pattern const& told, void const* const* values, appender const* appenders,
                            std::size_t count) {
    std::string_view const pattern = told.text();
    std::string out;
    std::size_t const expected = pattern.size() + 6 * count;
    if (expected > 15) out.reserve(expected);
    std::size_t used = 0;
    std::size_t slots = 0;
    char lone = '\0';
    for (std::size_t at = 0; at < pattern.size(); at++) {
        char const here = pattern[at];
        char const next = at + 1 < pattern.size() ? pattern[at + 1] : '\0';
        if ((here == '{' || here == '}') && next == here) {
            out.push_back(here);
            at++;
        } else if (here == '{' && next == '}') {
            slots++;
            if (used < count) {
                appenders[used](out, values[used]);
                used++;
            } else {
                out += "{}";
            }
            at++;
        } else {
            if ((here == '{' || here == '}') && lone == '\0') lone = here;
            out.push_back(here);
        }
    }
    for (; used < count; used++) {
        out.push_back(' ');
        appenders[used](out, values[used]);
    }
    if ((lone != '\0' || slots != count) && bad_pattern_hook() != nullptr) {
        std::string problem = "the message \"" + std::string(pattern) + "\" has " + std::to_string(slots) +
                              " {} for " + std::to_string(count) + " values";
        if (lone != '\0') problem += std::string(" and a lone '") + lone + "'";
        bad_pattern_hook()(problem, told.file(), told.line());
    }
    return out;
}

}  // namespace detail

template <class T>
inline detail::fixed_number<T> fixed(T value, int digits) {
    return detail::fixed_number<T>{value, digits};
}

template <class... Args>
inline std::string fmt(detail::pattern pattern, Args const&... args) {
    if constexpr (sizeof...(Args) == 0) {
        return detail::assemble(pattern, nullptr, nullptr, 0);
    } else {
        void const* const values[] = {static_cast<void const*>(&args)...};
        detail::appender const appenders[] = {&detail::append_erased<Args>...};
        return detail::assemble(pattern, values, appenders, sizeof...(Args));
    }
}

}  // namespace eo
