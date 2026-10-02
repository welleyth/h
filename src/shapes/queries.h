#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "intervals.h"

namespace eo {
namespace shapes {

[[nodiscard]] inline std::vector<interval> ranges(rng& draw, long long count, long long n, std::string const& shape) {
    detail::room_for_values(count, 1, 1, "a list of ranges");
    if (n < 1) eo::detail::library_error(fmt("ranges lie inside 1..n, and n is {}", n));
    std::vector<std::string> const names{"random", "short", "long", "prefix", "suffix", "point", "full", "same"};
    if (std::find(names.begin(), names.end(), shape) == names.end())
        eo::detail::library_error(
            fmt("\"{}\" is not a range shape; the names are random, short, long, prefix, suffix, point, full and "
                "same",
                shape));
    auto const placed = [&](long long length) {
        long long const left = draw.uniform(1, n - length + 1);
        return interval{left, left + length - 1};
    };
    std::vector<interval> made;
    made.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++) {
        if (shape == "random") {
            long long const one = draw.uniform(1, n);
            long long const other = draw.uniform(1, n);
            made.push_back(interval{(std::min)(one, other), (std::max)(one, other)});
        } else if (shape == "short") {
            made.push_back(placed(draw.uniform(1, (std::min)(n, 16LL))));
        } else if (shape == "long") {
            made.push_back(placed(draw.uniform((std::max)(1LL, n - n / 16), n)));
        } else if (shape == "prefix") {
            made.push_back(interval{1, draw.uniform(1, n)});
        } else if (shape == "suffix") {
            made.push_back(interval{draw.uniform(1, n), n});
        } else if (shape == "point") {
            made.push_back(placed(1));
        } else if (shape == "full") {
            made.push_back(interval{1, n});
        } else if (made.empty()) {
            long long const one = draw.uniform(1, n);
            long long const other = draw.uniform(1, n);
            made.push_back(interval{(std::min)(one, other), (std::max)(one, other)});
        } else {
            made.push_back(made.front());
        }
    }
    return made;
}

[[nodiscard]] inline std::vector<int> query_order(rng& draw, std::vector<long long> const& counts,
                                                  std::string const& shape) {
    long long total = 0;
    for (std::size_t kind = 0; kind < counts.size(); kind++) {
        if (counts[kind] < 0)
            eo::detail::library_error(fmt(
                "query_order needs a count of at least 0 for every kind; kind {} has {}", kind, counts[kind]));
        total += counts[kind];
    }
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(total));
    if (shape == "random" || shape == "grouped") {
        for (std::size_t kind = 0; kind < counts.size(); kind++)
            order.insert(order.end(), static_cast<std::size_t>(counts[kind]), static_cast<int>(kind));
        if (shape == "random") draw.shuffle(order);
    } else if (shape == "alternating") {
        std::vector<long long> left = counts;
        std::vector<int> active;
        for (std::size_t kind = 0; kind < counts.size(); kind++)
            if (counts[kind] > 0) active.push_back(static_cast<int>(kind));
        while (!active.empty()) {
            std::vector<int> still;
            for (int const kind : active) {
                order.push_back(kind);
                if (--left[static_cast<std::size_t>(kind)] > 0) still.push_back(kind);
            }
            active = std::move(still);
        }
    } else {
        eo::detail::library_error(
            fmt("\"{}\" is not a query order; the names are random, grouped and alternating", shape));
    }
    return order;
}

}  // namespace shapes
}  // namespace eo
