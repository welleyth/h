#pragma once

#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "present.h"

namespace eo {

struct point {
    long long x;
    long long y;
};

namespace shapes {
namespace detail {

inline void room_for_points(long long count, long long limit, char const* what) {
    if (count < 0) eo::detail::library_error(fmt("{} has at least no points, not {}", what, count));
    if (limit < 1) eo::detail::library_error(fmt("{} needs a coordinate bound of at least 1, not {}", what, limit));
}

inline long long whole_root(long long square) {
    long long root = static_cast<long long>(std::sqrt(static_cast<double>(square)));
    while (root > 0 && root * root > square) root--;
    while ((root + 1) * (root + 1) <= square) root++;
    return root;
}

inline long long shared_divisor(long long left, long long right) {
    while (right != 0) {
        long long const next = left % right;
        left = right;
        right = next;
    }
    return left < 0 ? -left : left;
}

inline std::vector<long long> const& circle_radii() {
    static std::vector<long long> const ladder{5, 65, 1105, 32045, 1185665};
    return ladder;
}

inline std::vector<point> lattice_on(long long radius) {
    std::vector<point> found;
    for (long long x = -radius; x <= radius; x++) {
        long long const rest = radius * radius - x * x;
        long long const y = whole_root(rest);
        if (y * y != rest) continue;
        found.push_back(point{x, y});
        if (y != 0) found.push_back(point{x, -y});
    }
    return found;
}

inline int half_of(point const& step) {
    return step.y < 0 || (step.y == 0 && step.x < 0) ? 1 : 0;
}

inline std::vector<long long> chained(rng& draw, long long count, long long limit) {
    std::vector<long long> values = draw.distinct(count, 0, limit);
    std::sort(values.begin(), values.end());
    long long const least = values.front();
    long long const most = values.back();
    long long top = least;
    long long bottom = least;
    std::vector<long long> steps;
    for (std::size_t at = 1; at + 1 < values.size(); at++) {
        long long const here = values[at];
        if (draw.chance(0.5)) {
            steps.push_back(here - top);
            top = here;
        } else {
            steps.push_back(bottom - here);
            bottom = here;
        }
    }
    steps.push_back(most - top);
    steps.push_back(bottom - most);
    return steps;
}

}  // namespace detail

[[nodiscard]] inline std::vector<point> scattered(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "a cloud of points");
    std::vector<point> made;
    made.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++)
        made.push_back(point{draw.uniform(-limit, limit), draw.uniform(-limit, limit)});
    return made;
}

[[nodiscard]] inline std::vector<point> collinear(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "a line of points");
    if (count <= 1) return scattered(draw, count, limit);
    long long const steps = count - 1;
    long long const reach = 2 * limit / steps;
    if (reach < 1)
        eo::detail::library_error(
            fmt("{} different collinear points do not fit inside +-{}", count, limit));
    long long dx = 0;
    long long dy = 0;
    while (dx == 0 && dy == 0) {
        dx = draw.uniform(0, reach);
        dy = draw.uniform(-reach, reach);
    }
    long long const shared = detail::shared_divisor(dx, dy);
    dx /= shared;
    dy /= shared;
    long long const from_x = draw.uniform(-limit, limit - steps * dx);
    long long const from_y = dy >= 0 ? draw.uniform(-limit, limit - steps * dy)
                                     : draw.uniform(-limit - steps * dy, limit);
    std::vector<point> made;
    made.reserve(static_cast<std::size_t>(count));
    for (long long at = 0; at < count; at++) made.push_back(point{from_x + at * dx, from_y + at * dy});
    draw.shuffle(made);
    return made;
}

[[nodiscard]] inline std::vector<point> convex_position(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "a convex polygon");
    if (count < 3) eo::detail::library_error(fmt("a convex polygon has at least 3 points, not {}", count));
    if (count > 2 * limit + 1)
        eo::detail::library_error(fmt("{} points in convex position do not fit inside +-{}", count, limit));
    if (limit > 1000000000)
        eo::detail::library_error(
            fmt("a convex polygon inside +-{} would overflow its own cross products; 1000000000 is the most",
                limit));
    std::vector<long long> across = detail::chained(draw, count, 2 * limit);
    std::vector<long long> upward = detail::chained(draw, count, 2 * limit);
    draw.shuffle(upward);
    std::vector<point> steps;
    steps.reserve(static_cast<std::size_t>(count));
    for (std::size_t at = 0; at < across.size(); at++) steps.push_back(point{across[at], upward[at]});
    std::sort(steps.begin(), steps.end(), [](point const& left, point const& right) {
        int const left_half = detail::half_of(left);
        int const right_half = detail::half_of(right);
        if (left_half != right_half) return left_half < right_half;
        long long const turn = left.x * right.y - left.y * right.x;
        if (turn != 0) return turn > 0;
        return std::tie(left.x, left.y) < std::tie(right.x, right.y);
    });
    std::vector<point> made;
    made.reserve(static_cast<std::size_t>(count));
    long long x = 0;
    long long y = 0;
    long long least_x = 0;
    long long least_y = 0;
    for (point const& one : steps) {
        made.push_back(point{x, y});
        x += one.x;
        y += one.y;
        least_x = (std::min)(least_x, x);
        least_y = (std::min)(least_y, y);
    }
    for (point& one : made) {
        one.x += -limit - least_x;
        one.y += -limit - least_y;
    }
    return made;
}

[[nodiscard]] inline std::vector<point> cocircular(rng& draw, long long count) {
    if (count < 0) eo::detail::library_error(fmt("a circle holds at least no points, not {}", count));
    for (long long const radius : detail::circle_radii()) {
        std::vector<point> found = detail::lattice_on(radius);
        if (static_cast<long long>(found.size()) < count) continue;
        draw.shuffle(found);
        found.resize(static_cast<std::size_t>(count));
        return found;
    }
    eo::detail::library_error(
        fmt("no circle of radius under 1200000 carries {} lattice points; convex_position scales instead",
            count));
}

[[nodiscard]] inline std::vector<point> cocircular(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "a circle");
    for (long long const radius : detail::circle_radii()) {
        if (radius > limit) break;
        std::vector<point> found = detail::lattice_on(radius);
        if (static_cast<long long>(found.size()) < count) continue;
        draw.shuffle(found);
        found.resize(static_cast<std::size_t>(count));
        return found;
    }
    if (count <= 4) {
        std::vector<point> found = detail::lattice_on(1);
        draw.shuffle(found);
        found.resize(static_cast<std::size_t>(count));
        return found;
    }
    eo::detail::library_error(
        fmt("the circles cocircular draws from, of radius 1, 5, 65, 1105, 32045 and 1185665, have none of radius "
            "at most {} with {} lattice points; raise the limit, or ask for fewer",
            limit, count));
}

[[nodiscard]] inline std::vector<point> extreme_points(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "points on the bound");
    if (count > 8 * limit)
        eo::detail::library_error(fmt("the edge of +-{} carries {} different points, not {}", limit,
                                      8 * limit, count));
    std::set<std::pair<long long, long long>> seen;
    std::vector<point> made;
    long long tries = 0;
    long long const most = 64 * count + 1000;
    while (static_cast<long long>(made.size()) < count) {
        detail::still_trying(tries, most, "that many different points on the bound");
        long long const along = draw.uniform(-limit, limit);
        long long const side = draw.uniform(0, 3);
        point const one = side == 0   ? point{along, -limit}
                          : side == 1 ? point{along, limit}
                          : side == 2 ? point{-limit, along}
                                      : point{limit, along};
        if (!seen.emplace(one.x, one.y).second) continue;
        made.push_back(one);
    }
    detail::kept_of(count, tries, "that many different points on the bound", eo::detail::site::here());
    return made;
}

}  // namespace shapes
}  // namespace eo
