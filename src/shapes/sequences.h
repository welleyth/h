#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"

namespace eo {
namespace shapes {
namespace detail {

inline void room_for_values(long long count, long long low, long long high, char const* what) {
    if (count < 0) eo::detail::library_error(fmt("{} has at least no elements, not {}", what, count));
    if (low > high) eo::detail::library_error(fmt("{} draws from {}..{}, which is empty", what, low, high));
}

}  // namespace detail

[[nodiscard]] inline std::vector<long long> equal_values(long long count, long long value) {
    detail::room_for_values(count, value, value, "a plateau");
    return std::vector<long long>(static_cast<std::size_t>(count), value);
}

[[nodiscard]] inline std::vector<long long> few_distinct(rng& draw, long long count, long long kinds,
                                                         long long low, long long high) {
    detail::room_for_values(count, low, high, "a sequence of few distinct values");
    if (kinds < 1) eo::detail::library_error(fmt("few_distinct needs at least one kind, not {}", kinds));
    std::vector<long long> const chosen = draw.distinct((std::min)(kinds, high - low + 1), low, high);
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++) values.push_back(draw.pick(chosen));
    return values;
}

[[nodiscard]] inline std::vector<long long> plateaus(rng& draw, long long count, long long runs,
                                                     long long low, long long high) {
    detail::room_for_values(count, low, high, "a sequence of plateaus");
    if (runs < 1 || runs > count)
        eo::detail::library_error(fmt("{} elements fall into 1..{} runs, not {}", count, count, runs));
    std::vector<long long> const lengths = draw.partition(runs, count);
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (long long const length : lengths) {
        long long const value = draw.uniform(low, high);
        for (long long at = 0; at < length; at++) values.push_back(value);
    }
    return values;
}

[[nodiscard]] inline std::vector<long long> nearly_sorted(rng& draw, long long count, long long low,
                                                          long long high, long long swaps) {
    detail::room_for_values(count, low, high, "a nearly sorted sequence");
    if (swaps < 0) eo::detail::library_error(fmt("nearly_sorted makes at least no swaps, not {}", swaps));
    std::vector<long long> values = draw.ints(count, low, high);
    std::sort(values.begin(), values.end());
    for (long long at = 0; at < swaps && count >= 2; at++) {
        std::size_t const one = static_cast<std::size_t>(draw.uniform(0, count - 1));
        std::size_t const other = static_cast<std::size_t>(draw.uniform(0, count - 1));
        std::swap(values[one], values[other]);
    }
    return values;
}

[[nodiscard]] inline std::vector<long long> alternating(rng& draw, long long count, long long low,
                                                        long long high) {
    detail::room_for_values(count, low, high, "an alternating sequence");
    long long const middle = low + (high - low) / 2;
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++)
        values.push_back(at % 2 == 0 ? draw.uniform(low, middle) : draw.uniform(middle, high));
    return values;
}

[[nodiscard]] inline std::vector<long long> hash_collisions(long long count, long long buckets = 107897) {
    if (count < 0) eo::detail::library_error(fmt("hash_collisions makes at least no values, not {}", count));
    if (buckets < 1) eo::detail::library_error(fmt("hash_collisions needs a bucket count, not {}", buckets));
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (long long at = 1; at <= count; at++) values.push_back(at * buckets);
    return values;
}

}  // namespace shapes
}  // namespace eo
