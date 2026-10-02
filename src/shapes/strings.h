#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "../read.h"
#include "numbers.h"

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

struct wide {
    std::uint64_t high;
    std::uint64_t low;
};

inline bool below(wide left, wide right) {
    return left.high != right.high ? left.high < right.high : left.low < right.low;
}

inline wide minus(wide left, wide right) {
    return wide{left.high - right.high - (left.low < right.low ? 1 : 0), left.low - right.low};
}

inline wide add_mod(wide left, wide right, wide modulus) {
    std::uint64_t const low = left.low + right.low;
    wide const sum{left.high + right.high + (low < left.low ? 1 : 0), low};
    return below(sum, modulus) ? sum : minus(sum, modulus);
}

inline wide mul_mod(wide left, wide right, wide modulus) {
    if (modulus.high == 0) return wide{0, mul_mod(left.low, right.low, modulus.low)};
    wide product{0, 0};
    for (int bit = 127; bit >= 0; bit--) {
        product = add_mod(product, product, modulus);
        std::uint64_t const word = bit >= 64 ? right.high : right.low;
        if (((word >> (bit % 64)) & 1) != 0) product = add_mod(product, left, modulus);
    }
    return product;
}

inline wide multiplied(std::uint64_t left, std::uint64_t right) {
    std::uint64_t const mask = 0xffffffffull;
    std::uint64_t const low_low = (left & mask) * (right & mask);
    std::uint64_t const low_high = (left & mask) * (right >> 32);
    std::uint64_t const high_low = (left >> 32) * (right & mask);
    std::uint64_t const middle = (low_low >> 32) + (low_high & mask) + (high_low & mask);
    return wide{(left >> 32) * (right >> 32) + (low_high >> 32) + (high_low >> 32) + (middle >> 32),
                (low_low & mask) | (middle << 32)};
}

inline std::uint64_t remainder_of(wide value, std::uint64_t modulus) {
    std::uint64_t const carry = (~std::uint64_t{0} % modulus + 1) % modulus;
    return (mul_mod(value.high % modulus, carry, modulus) + value.low % modulus) % modulus;
}

inline int bits_of(wide value) {
    int bits = 0;
    for (std::uint64_t word = value.high != 0 ? value.high : value.low; word != 0; word >>= 1) bits++;
    return value.high != 0 ? bits + 64 : bits;
}

struct polynomial_hash {
    wide base;
    wide modulus;
};

inline wide hashed(std::string const& text, polynomial_hash const& under) {
    wide hash{0, 0};
    for (char const one : text) {
        std::uint64_t const letter = static_cast<unsigned char>(one);
        hash = add_mod(mul_mod(hash, under.base, under.modulus),
                       wide{0, under.modulus.high != 0 ? letter : letter % under.modulus.low}, under.modulus);
    }
    return hash;
}

inline wide pow_mod(wide base, std::uint64_t exponent, wide modulus) {
    wide result = modulus.high == 0 && modulus.low == 1 ? wide{0, 0} : wide{0, 1};
    for (; exponent != 0; exponent >>= 1) {
        if ((exponent & 1) != 0) result = mul_mod(result, base, modulus);
        base = mul_mod(base, base, modulus);
    }
    return result;
}

inline std::uint64_t inverse_of(std::uint64_t value, std::uint64_t modulus) {
    long long older = static_cast<long long>(modulus);
    long long newer = static_cast<long long>(value % modulus);
    long long older_factor = 0;
    long long newer_factor = 1;
    while (newer != 0) {
        long long const times = older / newer;
        long long const rest = older - times * newer;
        older = newer;
        newer = rest;
        long long const factor = older_factor - times * newer_factor;
        older_factor = newer_factor;
        newer_factor = factor;
    }
    return static_cast<std::uint64_t>(older_factor < 0 ? older_factor + static_cast<long long>(modulus) : older_factor);
}

inline std::vector<polynomial_hash> stages_of(std::vector<std::pair<long long, long long>> const& hashes) {
    if (hashes.empty()) eo::detail::library_error("anti_hash needs at least one (base, modulus) pair to break");
    std::vector<polynomial_hash> stages;
    for (std::size_t at = 0; at < hashes.size(); at++) {
        long long const given = hashes[at].second;
        if (given < 2)
            eo::detail::library_error(fmt("hash {} has modulus {}; a modulus is at least 2", at + 1, given));
        std::uint64_t const modulus = static_cast<std::uint64_t>(given);
        std::uint64_t const base = static_cast<std::uint64_t>((hashes[at].first % given + given) % given);
        if (stages.empty() || std::gcd(remainder_of(stages.back().modulus, modulus), modulus) != 1 ||
            bits_of(stages.back().modulus) + bits_of(wide{0, modulus}) > 126) {
            stages.push_back(polynomial_hash{wide{0, base}, wide{0, modulus}});
            continue;
        }
        polynomial_hash& last = stages.back();
        std::uint64_t const gap = (base + modulus - remainder_of(last.base, modulus)) % modulus;
        std::uint64_t const steps = mul_mod(gap, inverse_of(remainder_of(last.modulus, modulus), modulus), modulus);
        wide const offset = multiplied(last.modulus.low, steps);
        wide const reach{offset.high + last.modulus.high * steps, offset.low};
        std::uint64_t const low = last.base.low + reach.low;
        last.base = wide{last.base.high + reach.high + (low < reach.low ? 1 : 0), low};
        wide const product = multiplied(last.modulus.low, modulus);
        last.modulus = wide{product.high + last.modulus.high * modulus, product.low};
    }
    return stages;
}

inline std::vector<int> cancelling_signs(std::vector<wide> const& values, wide modulus) {
    struct node {
        wide value;
        std::size_t larger;
        std::size_t smaller;
    };
    std::size_t const leaf = ~std::size_t{0};
    std::vector<node> nodes;
    std::vector<int> turned(values.size(), 1);
    std::vector<std::size_t> alive;
    std::size_t found = leaf;
    for (std::size_t at = 0; at < values.size(); at++) {
        wide const flipped = values[at].high == 0 && values[at].low == 0 ? values[at] : minus(modulus, values[at]);
        if (below(flipped, values[at])) turned[at] = -1;
        nodes.push_back(node{turned[at] < 0 ? flipped : values[at], leaf, leaf});
        alive.push_back(at);
        if (nodes.back().value.high == 0 && nodes.back().value.low == 0 && found == leaf) found = at;
    }
    while (found == leaf && alive.size() > 1) {
        std::sort(alive.begin(), alive.end(), [&](std::size_t left, std::size_t right) {
            if (below(nodes[left].value, nodes[right].value)) return true;
            if (below(nodes[right].value, nodes[left].value)) return false;
            return left < right;
        });
        std::vector<std::size_t> paired;
        for (std::size_t at = 0; at + 1 < alive.size(); at += 2) {
            nodes.push_back(
                node{minus(nodes[alive[at + 1]].value, nodes[alive[at]].value), alive[at + 1], alive[at]});
            paired.push_back(nodes.size() - 1);
            if (nodes.back().value.high == 0 && nodes.back().value.low == 0 && found == leaf)
                found = nodes.size() - 1;
        }
        alive = std::move(paired);
    }
    std::vector<int> signs;
    if (found == leaf) return signs;
    signs.assign(values.size(), 0);
    std::vector<std::pair<std::size_t, int>> waiting{{found, 1}};
    while (!waiting.empty()) {
        std::pair<std::size_t, int> const here = waiting.back();
        waiting.pop_back();
        node const& one = nodes[here.first];
        if (one.larger == leaf) {
            signs[here.first] = here.second * turned[here.first];
            continue;
        }
        waiting.push_back({one.larger, here.second});
        waiting.push_back({one.smaller, -here.second});
    }
    return signs;
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
    std::size_t const period = out.size();
    for (std::size_t at = 0; at + 1 < static_cast<std::size_t>(order); at++) out.push_back(out[at % period]);
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

[[nodiscard]] inline std::pair<std::string, std::string> anti_hash(
    rng& draw, std::vector<std::pair<long long, long long>> const& hashes, charset const& allowed) {
    std::vector<detail::polynomial_hash> const stages = detail::stages_of(hashes);
    std::vector<char> const letters = detail::letters_of(allowed);
    if (letters.size() < 2)
        eo::detail::library_error(
            fmt("anti_hash needs two different letters; charset(\"{}\") has fewer", allowed.text()));
    std::size_t const longest = 10000000;
    std::size_t const first = static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(letters.size()) - 1));
    std::size_t const second =
        (first + static_cast<std::size_t>(draw.uniform(1, static_cast<long long>(letters.size()) - 1))) %
        letters.size();
    std::string one(1, letters[first]);
    std::string other(1, letters[second]);
    for (detail::polynomial_hash const& stage : stages) {
        detail::wide const one_hash = detail::hashed(one, stage);
        detail::wide const other_hash = detail::hashed(other, stage);
        if (one_hash.high == other_hash.high && one_hash.low == other_hash.low) continue;
        detail::wide const gap =
            detail::add_mod(one_hash, detail::minus(stage.modulus, other_hash), stage.modulus);
        detail::wide const shift = detail::pow_mod(stage.base, one.size(), stage.modulus);
        std::vector<int> signs;
        for (std::size_t blocks = 2; signs.empty(); blocks *= 2) {
            if (one.size() * blocks > longest)
                eo::detail::library_error(fmt("anti_hash needs more than {} letters to break these {} hashes at "
                                              "once; break fewer",
                                              longest, hashes.size()));
            std::vector<detail::wide> values(blocks, gap);
            for (std::size_t at = blocks - 1; at-- > 0;)
                values[at] = detail::mul_mod(values[at + 1], shift, stage.modulus);
            signs = detail::cancelling_signs(values, stage.modulus);
        }
        std::string left;
        std::string right;
        for (int const sign : signs) {
            if (sign == 0) {
                std::string const same = draw.letters(static_cast<long long>(one.size()), allowed);
                left += same;
                right += same;
            } else {
                left += sign > 0 ? one : other;
                right += sign > 0 ? other : one;
            }
        }
        one = std::move(left);
        other = std::move(right);
    }
    return {std::move(one), std::move(other)};
}

}  // namespace shapes
}  // namespace eo
