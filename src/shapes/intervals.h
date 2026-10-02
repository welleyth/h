#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "sequences.h"

namespace eo {

struct interval {
    long long l;
    long long r;
};

namespace shapes {
namespace detail {

inline std::vector<long long> endpoints(rng& draw, long long count, long long low, long long high, char const* what) {
    long long const span = width_of(low, high);
    if (span < count - 1)
        eo::detail::library_error(fmt("{} need {} different endpoints, and {}..{} holds {}", what, count, low, high,
                                      span + 1));
    std::vector<long long> points = draw.distinct(count, low, high);
    std::sort(points.begin(), points.end());
    return points;
}

inline std::vector<char> balanced_brackets(rng& draw, long long pairs) {
    std::vector<char> opens(static_cast<std::size_t>(2 * pairs + 1), 0);
    for (std::size_t at = 0; at < static_cast<std::size_t>(pairs); at++) opens[at] = 1;
    draw.shuffle(opens);
    long long depth = 0;
    long long lowest = 0;
    std::size_t start = 0;
    for (std::size_t at = 0; at < opens.size(); at++) {
        depth += opens[at] != 0 ? 1 : -1;
        if (depth < lowest) {
            lowest = depth;
            start = at + 1;
        }
    }
    std::vector<char> word;
    word.reserve(opens.size() - 1);
    for (std::size_t step = 0; step + 1 < opens.size(); step++) word.push_back(opens[(start + step) % opens.size()]);
    return word;
}

}  // namespace detail

[[nodiscard]] inline std::vector<interval> intervals(rng& draw, long long count, long long low, long long high,
                                                     std::string const& shape) {
    detail::room_for_values(count, low, high, "a family of intervals");
    std::vector<interval> made;
    made.reserve(static_cast<std::size_t>(count));
    auto const named = [&](char const* kind, long long apart) {
        return detail::endpoints(draw, apart, low, high, fmt("{} {} intervals", count, kind).c_str());
    };
    if (shape == "random") {
        for (long long at = 0; at < count; at++) {
            long long const one = draw.uniform(low, high);
            long long const other = draw.uniform(low, high);
            made.push_back(interval{(std::min)(one, other), (std::max)(one, other)});
        }
    } else if (shape == "disjoint") {
        std::vector<long long> const ends = named("disjoint", 2 * count);
        for (std::size_t at = 0; at + 1 < ends.size(); at += 2) made.push_back(interval{ends[at], ends[at + 1]});
    } else if (shape == "touching") {
        std::vector<long long> const ends = named("touching", count + 1);
        for (std::size_t at = 0; at + 1 < ends.size(); at++) made.push_back(interval{ends[at], ends[at + 1]});
    } else if (shape == "nested") {
        std::vector<long long> const ends = named("nested", 2 * count);
        for (std::size_t at = 0; at < static_cast<std::size_t>(count); at++)
            made.push_back(interval{ends[at], ends[ends.size() - 1 - at]});
    } else if (shape == "laminar") {
        std::vector<long long> const ends = named("laminar", 2 * count);
        std::vector<char> const word = detail::balanced_brackets(draw, count);
        std::vector<std::size_t> open;
        for (std::size_t at = 0; at < word.size(); at++) {
            if (word[at] != 0) {
                open.push_back(at);
                continue;
            }
            made.push_back(interval{ends[open.back()], ends[at]});
            open.pop_back();
        }
    } else if (shape == "chain") {
        std::vector<long long> const ends = named("chain", count == 0 ? 0 : 2 * count + 2);
        for (std::size_t at = 0; at < static_cast<std::size_t>(count); at++)
            made.push_back(interval{ends[2 * at], ends[2 * at + 3]});
    } else if (shape == "through") {
        long long const shared = draw.uniform(low, high);
        for (long long at = 0; at < count; at++) {
            long long const left = draw.uniform(low, shared);
            made.push_back(interval{left, draw.uniform(shared, high)});
        }
    } else if (shape == "same") {
        long long const one = draw.uniform(low, high);
        long long const other = draw.uniform(low, high);
        made.assign(static_cast<std::size_t>(count), interval{(std::min)(one, other), (std::max)(one, other)});
    } else if (shape == "points") {
        for (long long at = 0; at < count; at++) {
            long long const one = draw.uniform(low, high);
            made.push_back(interval{one, one});
        }
    } else {
        eo::detail::library_error(
            fmt("\"{}\" is not an interval shape; the names are random, disjoint, touching, nested, laminar, chain, "
                "through, same and points",
                shape));
    }
    draw.shuffle(made);
    return made;
}

}  // namespace shapes
}  // namespace eo
