#pragma once

#include <string>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../read.h"
#include "permutations.h"
#include "sequences.h"
#include "strings.h"
#include "trees.h"

namespace eo {
namespace shapes {
namespace detail {

inline long long counted(unsigned long long kinds, long long length, std::string const& what) {
    if (length == 0) return 1;
    if (kinds == 0) return 0;
    unsigned long long total = 1;
    for (long long at = 0; at < length; at++) {
        if (kinds > 9223372036854775807ull || total > 9223372036854775807ull / kinds)
            eo::detail::library_error(fmt("there are more {} than a long long counts", what));
        total *= kinds;
    }
    return static_cast<long long>(total);
}

inline void numbered(long long index, long long count, std::string const& what) {
    if (index < 0 || index >= count)
        eo::detail::library_error(fmt("the {} are numbered 0..{}, not {}", what, count - 1, index));
}

inline std::vector<long long> digits_of(long long index, long long base, long long length) {
    std::vector<long long> digits(static_cast<std::size_t>(length), 0);
    for (std::size_t at = digits.size(); at-- > 0;) {
        digits[at] = index % base;
        index /= base;
    }
    return digits;
}

inline std::string arrays_named(long long length, long long low, long long high) {
    if (length < 0) eo::detail::library_error(fmt("arrays have at least no values, not {}", length));
    if (low > high) eo::detail::library_error(fmt("arrays draw from {}..{}, which is empty", low, high));
    return fmt("arrays of {} values in {}..{}", length, low, high);
}

inline unsigned long long kinds_between(long long low, long long high) {
    return static_cast<unsigned long long>(high) - static_cast<unsigned long long>(low) + 1;
}

}  // namespace detail

[[nodiscard]] inline long long count_arrays(long long length, long long low, long long high) {
    std::string const what = detail::arrays_named(length, low, high);
    unsigned long long const kinds = detail::kinds_between(low, high);
    return detail::counted(kinds == 0 ? ~0ull : kinds, length, what);
}

[[nodiscard]] inline std::vector<long long> array_at(long long length, long long low, long long high, long long index) {
    std::string const what = detail::arrays_named(length, low, high);
    detail::numbered(index, count_arrays(length, low, high), what);
    std::vector<long long> values =
        detail::digits_of(index, static_cast<long long>(detail::kinds_between(low, high)), length);
    for (long long& one : values) one += low;
    return values;
}

[[nodiscard]] inline long long count_strings(long long length, charset const& allowed) {
    detail::room_for_letters(length, "a string to count");
    return detail::counted(detail::letters_of(allowed).size(), length,
                           fmt("strings of {} letters over \"{}\"", length, allowed.text()));
}

[[nodiscard]] inline std::string string_at(long long length, charset const& allowed, long long index) {
    long long const count = count_strings(length, allowed);
    detail::numbered(index, count, fmt("strings of {} letters over \"{}\"", length, allowed.text()));
    std::vector<char> const letters = detail::letters_of(allowed);
    std::string out;
    for (long long const digit : detail::digits_of(index, static_cast<long long>(letters.size()), length))
        out.push_back(letters[static_cast<std::size_t>(digit)]);
    return out;
}

[[nodiscard]] inline long long count_permutations(int n) {
    detail::room_for_elements(n, 0, "a permutation to count");
    long long total = 1;
    for (int factor = 2; factor <= n; factor++) {
        if (total > 9223372036854775807LL / factor)
            eo::detail::library_error(fmt("there are more permutations of {} elements than a long long counts", n));
        total *= factor;
    }
    return total;
}

[[nodiscard]] inline std::vector<int> permutation_at(int n, long long index) {
    detail::numbered(index, count_permutations(n), fmt("permutations of {} elements", n));
    std::vector<long long> code(static_cast<std::size_t>(n), 0);
    for (int place = n; place >= 1; place--) {
        code[static_cast<std::size_t>(place) - 1] = index % (n - place + 1);
        index /= n - place + 1;
    }
    return detail::from_lehmer(code);
}

[[nodiscard]] inline long long count_trees(int n) {
    detail::at_least(n, 1, "a tree to count");
    return detail::counted(static_cast<unsigned long long>(n), n <= 2 ? 0 : n - 2,
                           fmt("labelled trees on {} vertices", n));
}

[[nodiscard]] inline graph tree_at(int n, long long index) {
    detail::numbered(index, count_trees(n), fmt("labelled trees on {} vertices", n));
    if (n == 1) return detail::undirected(1, {});
    std::vector<int> code;
    for (long long const digit : detail::digits_of(index, n, n - 2)) code.push_back(static_cast<int>(digit) + 1);
    return tree_from_pruefer(code);
}

}  // namespace shapes
}  // namespace eo
