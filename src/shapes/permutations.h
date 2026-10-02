#pragma once

#include <string>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"

namespace eo {
namespace shapes {
namespace detail {

inline void room_for_elements(long long n, long long least, char const* what) {
    if (n < least) {
        if (least == 0) eo::detail::library_error(fmt("{} has at least no elements, not {}", what, n));
        eo::detail::library_error(fmt("{} needs at least {} elements, not {}", what, least, n));
    }
}

inline std::vector<int> joined_in_cycles(std::vector<int> const& order, std::vector<long long> const& lengths) {
    std::vector<int> images(order.size(), 0);
    std::size_t first = 0;
    for (long long const length : lengths) {
        std::size_t const last = first + static_cast<std::size_t>(length) - 1;
        for (std::size_t at = first; at < last; at++)
            images[static_cast<std::size_t>(order[at]) - 1] = order[at + 1];
        images[static_cast<std::size_t>(order[last]) - 1] = order[first];
        first = last + 1;
    }
    return images;
}

}  // namespace detail

[[nodiscard]] inline std::vector<int> permutation_cycles(rng& draw, int n, int k) {
    detail::room_for_elements(n, 1, "a permutation with cycles");
    if (k < 1 || k > n)
        eo::detail::library_error(fmt("a permutation of {} elements has 1..{} cycles, not {}", n, n, k));
    std::vector<long long> const lengths = draw.partition(k, n);
    return detail::joined_in_cycles(draw.perm(n, 1), lengths);
}

[[nodiscard]] inline std::vector<int> derangement(rng& draw, int n) {
    detail::room_for_elements(n, 0, "a derangement");
    if (n == 1) eo::detail::library_error("no derangement of 1 element exists; ask for 0 or at least 2");
    for (;;) {
        std::vector<int> p = draw.perm(n, 1);
        bool moved = true;
        for (int at = 0; at < n && moved; at++) moved = p[static_cast<std::size_t>(at)] != at + 1;
        if (moved) return p;
    }
}

[[nodiscard]] inline std::vector<int> involution(rng& draw, int n, int fixed) {
    detail::room_for_elements(n, 0, "an involution");
    if (fixed < 0 || fixed > n)
        eo::detail::library_error(
            fmt("an involution of {} elements has 0..{} fixed points, not {}", n, n, fixed));
    if ((n - fixed) % 2 != 0)
        eo::detail::library_error(
            fmt("an involution of {} elements pairs the other {}, which is odd; ask for an {} number of fixed points",
                n, n - fixed, n % 2 == 0 ? "even" : "odd"));
    std::vector<int> const order = draw.perm(n, 1);
    std::vector<int> images(static_cast<std::size_t>(n), 0);
    for (std::size_t at = 0; at < static_cast<std::size_t>(fixed); at++)
        images[static_cast<std::size_t>(order[at]) - 1] = order[at];
    for (std::size_t at = static_cast<std::size_t>(fixed); at < order.size(); at += 2) {
        images[static_cast<std::size_t>(order[at]) - 1] = order[at + 1];
        images[static_cast<std::size_t>(order[at + 1]) - 1] = order[at];
    }
    return images;
}

[[nodiscard]] inline std::vector<int> permutation(rng& draw, int n, std::string const& shape) {
    detail::room_for_elements(n, 1, "a permutation");
    if (shape == "random") return draw.perm(n, 1);
    if (shape == "cycle") return permutation_cycles(draw, n, 1);
    if (shape == "derangement") return derangement(draw, n);
    if (shape == "involution") return involution(draw, n, n % 2);
    std::vector<int> ordered(static_cast<std::size_t>(n), 0);
    for (int at = 0; at < n; at++) ordered[static_cast<std::size_t>(at)] = at + 1;
    if (shape == "identity") return ordered;
    if (shape == "reversed") return std::vector<int>(ordered.rbegin(), ordered.rend());
    eo::detail::library_error(
        fmt("\"{}\" is not a permutation shape; the names are random, identity, reversed, cycle, derangement and "
            "involution",
            shape));
}

}  // namespace shapes
}  // namespace eo
