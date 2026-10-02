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

inline long long width_of(long long low, long long high) {
    unsigned long long const span = static_cast<unsigned long long>(high) - static_cast<unsigned long long>(low);
    return span > 9223372036854775807ull ? 9223372036854775807LL : static_cast<long long>(span);
}

inline int decimal_length(long long value) {
    int length = 1;
    for (; value >= 10; value /= 10) length++;
    return length;
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

[[nodiscard]] inline std::vector<long long> log_uniform(rng& draw, long long count, long long low, long long high) {
    detail::room_for_values(count, low, high, "a log-uniform sequence");
    if (low < 0) eo::detail::library_error(fmt("log_uniform draws from 0 up; {}..{} reaches below it", low, high));
    std::vector<long long> powers{1};
    while (powers.size() < 19) powers.push_back(powers.back() * 10);
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++) {
        std::size_t const length = static_cast<std::size_t>(
            draw.uniform(detail::decimal_length(low), detail::decimal_length(high)));
        long long const shortest = length == 1 ? 0 : powers[length - 1];
        long long const longest = length == 19 ? high : powers[length] - 1;
        values.push_back(draw.uniform((std::max)(low, shortest), (std::min)(high, longest)));
    }
    return values;
}

[[nodiscard]] inline std::vector<long long> near_bounds(rng& draw, long long count, long long low, long long high,
                                                        long long spread) {
    detail::room_for_values(count, low, high, "a sequence near its bounds");
    if (spread < 0)
        eo::detail::library_error(
            fmt("near_bounds keeps within a spread of at least 0 of a bound, not {}", spread));
    long long const reach = (std::min)(spread, detail::width_of(low, high));
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++)
        values.push_back(draw.chance(0.5) ? draw.uniform(low, low + reach) : draw.uniform(high - reach, high));
    return values;
}

[[nodiscard]] inline std::vector<long long> spikes(rng& draw, long long count, long long tall, long long low,
                                                   long long high) {
    detail::room_for_values(count, low, high, "a sequence of spikes");
    if (tall < 0 || tall > count)
        eo::detail::library_error(fmt("{} elements hold 0..{} spikes, not {}", count, count, tall));
    long long const band = detail::width_of(low, high) / 16;
    std::vector<bool> raised(static_cast<std::size_t>(count), false);
    for (long long const at : draw.distinct(tall, 0, count - 1)) raised[static_cast<std::size_t>(at)] = true;
    std::vector<long long> values;
    values.reserve(static_cast<std::size_t>(count));
    for (std::size_t at = 0; at < raised.size(); at++)
        values.push_back(raised[at] ? draw.uniform(high - band, high) : draw.uniform(low, low + band));
    return values;
}

[[nodiscard]] inline std::vector<long long> split_sum(rng& draw, long long total, long long parts, long long least,
                                                      long long most) {
    if (parts < 1) eo::detail::library_error(fmt("split_sum splits into at least one part, not {}", parts));
    if (least > most) eo::detail::library_error(fmt("split_sum's parts lie in {}..{}, which is empty", least, most));
    if (least < 0) eo::detail::library_error(fmt("split_sum's parts are at least 0, not {}", least));
    long long lowest = 0;
    long long highest = 0;
    if (eo::detail::product_overflows(least, parts, &lowest))
        eo::detail::library_error(fmt("{} parts of {}..{} add up to more than a long long holds, not {}", parts,
                                      least, most, total));
    bool const unbounded = eo::detail::product_overflows(most, parts, &highest);
    if (total < lowest || (!unbounded && total > highest))
        eo::detail::library_error(
            fmt("{} parts of {}..{} add up to {}..{}, not {}", parts, least, most, lowest, highest, total));
    long long const room = most - least;
    std::vector<long long> shares = draw.partition(parts, total - lowest, 0);
    long long spill = 0;
    for (long long& share : shares) {
        if (share <= room) continue;
        spill += share - room;
        share = room;
    }
    if (spill > 0) {
        std::vector<std::size_t> order(shares.size());
        for (std::size_t at = 0; at < order.size(); at++) order[at] = at;
        draw.shuffle(order);
        for (std::size_t const at : order) {
            long long const given = (std::min)(room - shares[at], spill);
            shares[at] += given;
            spill -= given;
        }
    }
    for (long long& share : shares) share += least;
    return shares;
}

}  // namespace shapes
}  // namespace eo
