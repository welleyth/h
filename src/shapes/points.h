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
#include "numbers.h"
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

[[nodiscard]] inline std::vector<point> general_position(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "points in general position");
    long long const side = limit > 4611686018427387903LL ? 9223372036854775783LL : prev_prime(2 * limit + 1);
    if (count > side)
        eo::detail::library_error(
            fmt("general_position fits at most {} points inside +-{}, not {}", side, limit, count));
    std::uint64_t const modulus = static_cast<std::uint64_t>(side);
    std::uint64_t const square = static_cast<std::uint64_t>(draw.uniform(1, side - 1));
    std::uint64_t const linear = static_cast<std::uint64_t>(draw.uniform(0, side - 1));
    std::uint64_t const constant = static_cast<std::uint64_t>(draw.uniform(0, side - 1));
    bool const turned = draw.chance(0.5);
    bool const mirrored = draw.chance(0.5);
    std::vector<point> made;
    made.reserve(static_cast<std::size_t>(count));
    for (long long const x : draw.distinct(count, 0, side - 1)) {
        std::uint64_t const at = static_cast<std::uint64_t>(x);
        std::uint64_t const y =
            (detail::mul_mod(detail::mul_mod(at, at, modulus), square, modulus) + detail::mul_mod(at, linear, modulus) +
             constant) % modulus;
        point one{x - limit, static_cast<long long>(y) - limit};
        if (mirrored) one.x = -one.x;
        if (turned) std::swap(one.x, one.y);
        made.push_back(one);
    }
    return made;
}

[[nodiscard]] inline std::vector<point> simple_polygon(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "a simple polygon");
    if (count < 3) eo::detail::library_error(fmt("a simple polygon has at least 3 vertices, not {}", count));
    if (limit > 1000000000)
        eo::detail::library_error(
            fmt("a simple polygon inside +-{} would overflow its own cross products; 1000000000 is the most", limit));
    std::vector<point> points = general_position(draw, count, limit);
    std::sort(points.begin(), points.end(),
              [](point const& left, point const& right) {
                  return std::tie(left.x, left.y) < std::tie(right.x, right.y);
              });
    point const first = points.front();
    point const last = points.back();
    std::vector<point> lower;
    std::vector<point> upper;
    for (std::size_t at = 1; at + 1 < points.size(); at++) {
        point const& one = points[at];
        long long const turn = (last.x - first.x) * (one.y - first.y) - (last.y - first.y) * (one.x - first.x);
        (turn < 0 ? lower : upper).push_back(one);
    }
    std::vector<point> made{first};
    made.insert(made.end(), lower.begin(), lower.end());
    made.push_back(last);
    made.insert(made.end(), upper.rbegin(), upper.rend());
    std::rotate(made.begin(), made.begin() + static_cast<std::ptrdiff_t>(draw.uniform(0, count - 1)), made.end());
    return made;
}

[[nodiscard]] inline std::vector<point> strictly_convex(rng& draw, long long count, long long limit) {
    detail::room_for_points(count, limit, "a strictly convex polygon");
    if (count < 3) eo::detail::library_error(fmt("a strictly convex polygon has at least 3 vertices, not {}", count));
    if (limit > 1000000000)
        eo::detail::library_error(fmt(
            "a strictly convex polygon inside +-{} would overflow its own cross products; 1000000000 is the most",
            limit));
    long long const wanted = (count + 3) / 4;
    std::vector<point> quarter;
    long long spent = 0;
    for (long long sum = 1; static_cast<long long>(quarter.size()) < wanted; sum++)
        for (long long across = 1; across <= sum && static_cast<long long>(quarter.size()) < wanted; across++) {
            if (detail::shared_divisor(across, sum - across) != 1) continue;
            if (spent + sum > 2 * limit)
                eo::detail::library_error(
                    fmt("at most {} vertices of a strictly convex polygon fit inside +-{}, not {}", 4 * quarter.size(),
                        limit, count));
            spent += sum;
            quarter.push_back(point{across, sum - across});
        }
    std::vector<point> steps;
    for (point const& one : quarter)
        for (point const& turned : {one, point{-one.y, one.x}, point{-one.x, -one.y}, point{one.y, -one.x}})
            steps.push_back(turned);
    std::sort(steps.begin(), steps.end(), [](point const& left, point const& right) {
        int const left_half = detail::half_of(left);
        int const right_half = detail::half_of(right);
        if (left_half != right_half) return left_half < right_half;
        return left.x * right.y - left.y * right.x > 0;
    });
    while (static_cast<long long>(steps.size()) > count) {
        std::size_t const at = static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(steps.size()) - 1));
        std::size_t const next = (at + 1) % steps.size();
        steps[at] = point{steps[at].x + steps[next].x, steps[at].y + steps[next].y};
        steps.erase(steps.begin() + static_cast<std::ptrdiff_t>(next));
    }
    std::vector<point> made;
    point here{0, 0};
    point least{0, 0};
    point most{0, 0};
    for (point const& step : steps) {
        made.push_back(here);
        here = point{here.x + step.x, here.y + step.y};
        least = point{(std::min)(least.x, here.x), (std::min)(least.y, here.y)};
        most = point{(std::max)(most.x, here.x), (std::max)(most.y, here.y)};
    }
    long long const shift_x = -limit - least.x + draw.uniform(0, 2 * limit - (most.x - least.x));
    long long const shift_y = -limit - least.y + draw.uniform(0, 2 * limit - (most.y - least.y));
    for (point& one : made) one = point{one.x + shift_x, one.y + shift_y};
    std::rotate(made.begin(), made.begin() + static_cast<std::ptrdiff_t>(draw.uniform(0, count - 1)), made.end());
    return made;
}

[[nodiscard]] inline std::vector<std::pair<point, point>> crossing_segments(rng& draw, long long count,
                                                                           long long limit) {
    if (count < 1) eo::detail::library_error(fmt("crossing_segments makes at least 1 segment, not {}", count));
    long long const corners = (std::max)(4LL, 2 * count);
    std::vector<point> const ring = strictly_convex(draw, corners, limit);
    std::vector<std::pair<point, point>> made;
    for (std::size_t at = 0; at < static_cast<std::size_t>(count); at++) {
        point const& one = ring[at];
        point const& other = ring[at + static_cast<std::size_t>(corners) / 2];
        made.push_back(draw.chance(0.5) ? std::make_pair(one, other) : std::make_pair(other, one));
    }
    draw.shuffle(made);
    return made;
}

}  // namespace shapes
}  // namespace eo
