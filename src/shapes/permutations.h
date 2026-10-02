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

class ranked_values {
public:
    explicit ranked_values(int n) : counts_(static_cast<std::size_t>(n) + 1, 0) {
        while ((std::size_t{1} << (high_bit_ + 1)) <= static_cast<std::size_t>(n)) high_bit_++;
        for (std::size_t at = 1; at < counts_.size(); at++) {
            counts_[at]++;
            std::size_t const up = at + (at & (~at + 1));
            if (up < counts_.size()) counts_[up] += counts_[at];
        }
    }

    int take(long long rank) {
        std::size_t at = 0;
        for (int bit = high_bit_; bit >= 0; bit--) {
            std::size_t const next = at + (std::size_t{1} << bit);
            if (next < counts_.size() && counts_[next] <= rank) {
                at = next;
                rank -= counts_[next];
            }
        }
        for (std::size_t down = at + 1; down < counts_.size(); down += down & (~down + 1)) counts_[down]--;
        return static_cast<int>(at) + 1;
    }

private:
    std::vector<long long> counts_;
    int high_bit_ = 0;
};

inline std::vector<int> from_lehmer(std::vector<long long> const& code) {
    ranked_values left(static_cast<int>(code.size()));
    std::vector<int> values;
    values.reserve(code.size());
    for (long long const smaller_after : code) values.push_back(left.take(smaller_after));
    return values;
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

[[nodiscard]] inline std::vector<int> with_inversions(rng& draw, int n, long long k) {
    detail::room_for_elements(n, 1, "a permutation with inversions");
    long long room = static_cast<long long>(n) * (n - 1) / 2;
    if (k < 0 || k > room)
        eo::detail::library_error(fmt("a permutation of {} elements has 0..{} inversions, not {}", n, room, k));
    std::vector<long long> code(static_cast<std::size_t>(n), 0);
    for (int const at : draw.perm(n)) {
        long long const most = n - 1 - at;
        room -= most;
        long long const given = draw.uniform((std::max)(0LL, k - room), (std::min)(most, k));
        code[static_cast<std::size_t>(at)] = given;
        k -= given;
    }
    return detail::from_lehmer(code);
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
