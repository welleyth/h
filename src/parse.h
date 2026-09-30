#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>

#include "core.h"

namespace eo {
namespace detail {

enum class number_problem {
    none,
    empty,
    missing_digits,
    bad_character,
    leading_zero,
    redundant_minus,
    out_of_range,
};

inline char const* describe(number_problem problem) {
    static char const* const words[] = {
        "",
        "it is empty",
        "it has no digits where digits are required",
        "it has a character that cannot be part of the number",
        "it has a leading zero",
        "it is zero written with a minus",
        "it does not fit the type it is read into",
    };
    return words[static_cast<int>(problem)];
}

struct integer_read {
    long long value = 0;
    number_problem problem = number_problem::none;
};

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

inline integer_read parse_integer(std::string_view text, bool relaxed = false) {
    if (text.empty()) return {0, number_problem::empty};
    std::size_t const start = text[0] == '-' || (relaxed && text[0] == '+') ? 1u : 0u;
    bool const negative = text[0] == '-';
    if (start == text.size()) return {0, number_problem::missing_digits};
    for (std::size_t at = start; at < text.size(); at++)
        if (!is_digit(text[at])) return {0, number_problem::bad_character};
    if (!relaxed && text[start] == '0' && text.size() - start > 1)
        return {0, number_problem::leading_zero};
    unsigned long long const limit = negative ? 9223372036854775808ull : 9223372036854775807ull;
    unsigned long long magnitude = 0;
    for (std::size_t at = start; at < text.size(); at++) {
        unsigned long long const digit = static_cast<unsigned long long>(text[at] - '0');
        if (magnitude > (limit - digit) / 10) return {0, number_problem::out_of_range};
        magnitude = magnitude * 10 + digit;
    }
    if (!relaxed && negative && magnitude == 0) return {0, number_problem::redundant_minus};
    return {negative ? static_cast<long long>(0ull - magnitude) : static_cast<long long>(magnitude),
            number_problem::none};
}

inline long long environment_integer(char const* name) {
    char const* const set = environment(name);
    if (set == nullptr) return 0;
    integer_read const parsed = parse_integer(set);
    return parsed.problem == number_problem::none ? parsed.value : 0;
}

struct real_read {
    double value = 0;
    int decimals = 0;
    number_problem problem = number_problem::none;
};

inline double decimal_value(std::string_view text) {
#if defined(__cpp_lib_to_chars)
    if (numbers_as_in_c()) {
        double quick = 0;
        std::from_chars_result const read = std::from_chars(text.data(), text.data() + text.size(), quick);
        if (read.ec == std::errc() && read.ptr == text.data() + text.size()) return quick;
    }
#endif
    char small[64];
    if (text.size() < sizeof(small)) {
        std::memcpy(small, text.data(), text.size());
        small[text.size()] = '\0';
        return std::strtod(small, nullptr);
    }
    std::string const whole(text);
    return std::strtod(whole.c_str(), nullptr);
}

inline real_read parse_real(std::string_view text, bool allow_exponent, bool negative_zero = false) {
    if (text.empty()) return {0, 0, number_problem::empty};
    std::size_t at = text[0] == '-' ? 1u : 0u;
    bool const negative = at == 1;
    std::size_t const whole = at;
    while (at < text.size() && is_digit(text[at])) at++;
    if (at == whole) return {0, 0, number_problem::missing_digits};
    if (text[whole] == '0' && at - whole > 1) return {0, 0, number_problem::leading_zero};
    int decimals = 0;
    if (at < text.size() && text[at] == '.') {
        at++;
        std::size_t const fraction = at;
        while (at < text.size() && is_digit(text[at])) at++;
        if (at == fraction) return {0, 0, number_problem::missing_digits};
        decimals = static_cast<int>(at - fraction);
    }
    if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
        if (!allow_exponent) return {0, 0, number_problem::bad_character};
        at++;
        if (at < text.size() && (text[at] == '-' || text[at] == '+')) at++;
        std::size_t const exponent = at;
        while (at < text.size() && is_digit(text[at])) at++;
        if (at == exponent) return {0, 0, number_problem::missing_digits};
    }
    if (at != text.size()) return {0, 0, number_problem::bad_character};
    double const value = decimal_value(text);
    if (!std::isfinite(value)) return {0, 0, number_problem::out_of_range};
    if (negative && value == 0 && !negative_zero) return {0, 0, number_problem::redundant_minus};
    if (value == 0) return {0, decimals, number_problem::none};
    return {value, decimals, number_problem::none};
}

}  // namespace detail
}  // namespace eo
