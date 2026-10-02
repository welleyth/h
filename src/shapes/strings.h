#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "../read.h"

namespace eo {
namespace shapes {
namespace detail {

inline void room_for_letters(long long length, char const* what) {
    if (length < 0) eo::detail::library_error(fmt("{} is at least empty, not {} long", what, length));
}

inline std::vector<char> letters_of(charset const& allowed) {
    std::vector<char> found;
    for (int one = 0; one < 256; one++)
        if (allowed.has(static_cast<char>(one))) found.push_back(static_cast<char>(one));
    return found;
}

}  // namespace detail

[[nodiscard]] inline std::string repeated(char one, long long length) {
    detail::room_for_letters(length, "a repeated string");
    return std::string(static_cast<std::size_t>(length), one);
}

[[nodiscard]] inline std::string periodic(std::string const& unit, long long length) {
    detail::room_for_letters(length, "a periodic string");
    if (unit.empty()) eo::detail::library_error("a periodic string needs a unit to repeat");
    std::string out;
    out.reserve(static_cast<std::size_t>(length));
    while (static_cast<long long>(out.size()) < length)
        out.append(unit, 0, static_cast<std::size_t>(length) - out.size());
    return out;
}

[[nodiscard]] inline std::string near_periodic(rng& draw, std::string const& unit, long long length,
                                               charset const& allowed) {
    std::string out = periodic(unit, length);
    if (out.empty()) return out;
    std::size_t const spot = static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(out.size()) - 1));
    std::vector<char> others;
    for (int one = 0; one < 256; one++) {
        char const letter = static_cast<char>(one);
        if (allowed.has(letter) && letter != out[spot]) others.push_back(letter);
    }
    if (others.empty())
        eo::detail::library_error(
            fmt("near_periodic needs a character other than '{}' in \"{}\"", out[spot], allowed.text()));
    out[spot] = draw.pick(others);
    return out;
}

[[nodiscard]] inline std::string fibonacci_word(long long length) {
    detail::room_for_letters(length, "a Fibonacci word");
    std::string older = "b";
    std::string newer = "a";
    while (static_cast<long long>(newer.size()) < length) {
        std::string next = newer + older;
        older = std::move(newer);
        newer = std::move(next);
    }
    newer.resize(static_cast<std::size_t>(length));
    return newer;
}

[[nodiscard]] inline std::string thue_morse(long long length) {
    detail::room_for_letters(length, "a Thue-Morse word");
    std::string out;
    out.reserve(static_cast<std::size_t>(length));
    for (long long at = 0; at < length; at++) {
        int ones = 0;
        for (unsigned long long bits = static_cast<unsigned long long>(at); bits != 0; bits >>= 1)
            ones += static_cast<int>(bits & 1u);
        out.push_back(ones % 2 == 0 ? 'a' : 'b');
    }
    return out;
}

[[nodiscard]] inline std::string abacaba(long long length) {
    detail::room_for_letters(length, "an abacaba word");
    std::string out;
    out.reserve(static_cast<std::size_t>(length));
    for (long long at = 1; at <= length; at++) {
        int zeros = 0;
        for (long long rest = at; rest % 2 == 0 && zeros < 25; rest /= 2) zeros++;
        out.push_back(static_cast<char>('a' + zeros));
    }
    return out;
}

[[nodiscard]] inline std::pair<std::string, std::string> thue_morse_twins(long long length) {
    if (length < 1 || length % 1024 != 0)
        eo::detail::library_error(fmt(
            "Thue-Morse twins collide modulo 2^64 at lengths that are multiples of 1024, not {}", length));
    std::string first = thue_morse(length);
    std::string second = first;
    for (char& one : second) one = one == 'a' ? 'b' : 'a';
    return {std::move(first), std::move(second)};
}

[[nodiscard]] inline std::string palindrome(rng& draw, long long length, charset const& allowed) {
    detail::room_for_letters(length, "a palindrome");
    std::string out = draw.letters(length, allowed);
    for (long long at = 0; at * 2 < length; at++)
        out[static_cast<std::size_t>(length - 1 - at)] = out[static_cast<std::size_t>(at)];
    return out;
}

[[nodiscard]] inline std::string de_bruijn(charset const& allowed, int order) {
    if (order < 1) eo::detail::library_error(fmt("a de Bruijn sequence has an order of at least 1, not {}", order));
    std::vector<char> const letters = detail::letters_of(allowed);
    if (letters.empty()) eo::detail::library_error(fmt("charset(\"{}\") holds no characters", allowed.text()));
    long long const kinds = static_cast<long long>(letters.size());
    long long words = 1;
    for (int at = 0; at < order && words <= 100000000; at++) words *= kinds;
    if (words > 100000000)
        eo::detail::library_error(fmt(
            "a de Bruijn sequence of order {} over {} letters is {} long; 100000000 is the most", order, kinds,
            words <= 100000000 * kinds ? fmt("{}", words) : std::string("more than 10^8")));
    std::string out;
    out.reserve(static_cast<std::size_t>(words) + static_cast<std::size_t>(order) - 1);
    std::vector<long long> word{-1};
    while (!word.empty()) {
        word.back()++;
        std::size_t const length = word.size();
        if (static_cast<std::size_t>(order) % length == 0)
            for (long long const one : word) out.push_back(letters[static_cast<std::size_t>(one)]);
        while (word.size() < static_cast<std::size_t>(order)) word.push_back(word[word.size() - length]);
        while (!word.empty() && word.back() == kinds - 1) word.pop_back();
    }
    std::size_t const cycle = out.size();
    for (std::size_t at = 0; at + 1 < static_cast<std::size_t>(order); at++) out.push_back(out[at % cycle]);
    return out;
}

[[nodiscard]] inline std::string lyndon(rng& draw, long long length, charset const& allowed) {
    if (length < 1) eo::detail::library_error(fmt("a Lyndon word is at least 1 long, not {}", length));
    if (length >= 2 && detail::letters_of(allowed).size() < 2)
        eo::detail::library_error(
            fmt("a Lyndon word of {} letters needs two different letters to choose from; charset(\"{}\") has fewer",
                length, allowed.text()));
    std::size_t const size = static_cast<std::size_t>(length);
    for (;;) {
        std::string const word = draw.letters(length, allowed);
        std::size_t first = 0;
        std::size_t second = 1;
        std::size_t matched = 0;
        while (first < size && second < size && matched < size) {
            char const one = word[(first + matched) % size];
            char const other = word[(second + matched) % size];
            if (one == other) {
                matched++;
                continue;
            }
            if (one > other) {
                first += matched + 1;
            } else {
                second += matched + 1;
            }
            if (first == second) second++;
            matched = 0;
        }
        if (matched == size && size > 1) continue;
        std::size_t const start = (std::min)(first, second);
        return word.substr(start) + word.substr(0, start);
    }
}

}  // namespace shapes
}  // namespace eo
