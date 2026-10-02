#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "../structure.h"
#include "present.h"

namespace eo {
namespace shapes {
namespace detail {

inline graph decoded(int n, std::vector<int> const& code) {
    std::vector<int> degree(static_cast<std::size_t>(n) + 1, 1);
    for (int const chosen : code) degree[static_cast<std::size_t>(chosen)]++;
    std::vector<edge> edges;
    edges.reserve(static_cast<std::size_t>(n) - 1);
    int lowest = 1;
    while (degree[static_cast<std::size_t>(lowest)] != 1) lowest++;
    int leaf = lowest;
    for (int const chosen : code) {
        edges.push_back(edge{leaf, chosen});
        if (--degree[static_cast<std::size_t>(chosen)] == 1 && chosen < lowest) {
            leaf = chosen;
        } else {
            lowest++;
            while (degree[static_cast<std::size_t>(lowest)] != 1) lowest++;
            leaf = lowest;
        }
    }
    edges.push_back(edge{leaf, n});
    return undirected(n, std::move(edges));
}

inline graph hung_from_path(rng& draw, int n, std::vector<int> const& allowance) {
    int const along = static_cast<int>(allowance.size());
    std::vector<edge> edges;
    edges.reserve(static_cast<std::size_t>(n) - 1);
    for (int vertex = 2; vertex <= along; vertex++) edges.push_back(edge{vertex - 1, vertex});
    std::vector<int> room(static_cast<std::size_t>(n) + 1, 0);
    std::vector<int> open;
    for (int vertex = 1; vertex <= along; vertex++) {
        room[static_cast<std::size_t>(vertex)] = allowance[static_cast<std::size_t>(vertex) - 1];
        if (room[static_cast<std::size_t>(vertex)] > 0) open.push_back(vertex);
    }
    for (int vertex = along + 1; vertex <= n; vertex++) {
        int const parent = draw.pick(open);
        edges.push_back(edge{parent, vertex});
        room[static_cast<std::size_t>(vertex)] = room[static_cast<std::size_t>(parent)] - 1;
        if (room[static_cast<std::size_t>(vertex)] > 0) open.push_back(vertex);
    }
    return undirected(n, std::move(edges));
}

}  // namespace detail

[[nodiscard]] inline graph random_tree(rng& draw, int n) {
    detail::at_least(n, 1, "a tree");
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++)
        edges.push_back(edge{static_cast<int>(draw.uniform(1, vertex - 1)), vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph deep_tree(rng& draw, int n, int lean) {
    detail::at_least(n, 1, "a tree");
    if (lean < 0) eo::detail::library_error(fmt("deep_tree leans toward the last vertex; {} is below 0", lean));
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++)
        edges.push_back(edge{static_cast<int>(draw.weighted(1, vertex - 1, lean)), vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph uniform_tree(rng& draw, int n) {
    detail::at_least(n, 1, "a tree");
    if (n <= 2) {
        std::vector<edge> edges;
        if (n == 2) edges.push_back(edge{1, 2});
        return detail::undirected(n, std::move(edges));
    }
    std::vector<int> code;
    for (int at = 0; at < n - 2; at++) code.push_back(static_cast<int>(draw.uniform(1, n)));
    return detail::decoded(n, code);
}

[[nodiscard]] inline graph path(int n) {
    detail::at_least(n, 1, "a path");
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++) edges.push_back(edge{vertex - 1, vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph star(int n) {
    detail::at_least(n, 1, "a star");
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++) edges.push_back(edge{1, vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph caterpillar(rng& draw, int n, int spine = 0) {
    detail::at_least(n, 1, "a caterpillar");
    int const along = spine > 0 ? spine : (n + 1) / 2;
    if (along > n) eo::detail::library_error(fmt("a spine of {} does not fit {} vertices", along, n));
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= along; vertex++) edges.push_back(edge{vertex - 1, vertex});
    for (int vertex = along + 1; vertex <= n; vertex++)
        edges.push_back(edge{static_cast<int>(draw.uniform(1, along)), vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph broom(int n, int handle = 0) {
    detail::at_least(n, 1, "a broom");
    int const along = handle > 0 ? handle : (n + 1) / 2;
    if (along > n) eo::detail::library_error(fmt("a handle of {} does not fit {} vertices", along, n));
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= along; vertex++) edges.push_back(edge{vertex - 1, vertex});
    for (int vertex = along + 1; vertex <= n; vertex++) edges.push_back(edge{along, vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph kary_tree(int n, int k) {
    detail::at_least(n, 1, "a k-ary tree");
    if (k < 1) eo::detail::library_error(fmt("a k-ary tree branches at least once; k is {}", k));
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++) edges.push_back(edge{(vertex - 2) / k + 1, vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph binary_tree(int n) { return kary_tree(n, 2); }

[[nodiscard]] inline graph dumbbell(int n) {
    detail::at_least(n, 1, "a dumbbell");
    std::vector<edge> edges;
    if (n >= 2) edges.push_back(edge{1, 2});
    int const near_one = n / 2;
    for (int vertex = 3; vertex <= n; vertex++) edges.push_back(edge{vertex <= near_one + 1 ? 1 : 2, vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph spider(int n, int legs) {
    detail::at_least(n, 1, "a spider");
    if (legs < 1) eo::detail::library_error(fmt("a spider has at least one leg, not {}", legs));
    std::vector<edge> edges;
    int next = 2;
    for (int leg = 0; leg < legs && next <= n; leg++) {
        int const length = (n - 1) / legs + (leg < (n - 1) % legs ? 1 : 0);
        int attach = 1;
        for (int step = 0; step < length; step++) {
            edges.push_back(edge{attach, next});
            attach = next++;
        }
    }
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph tree_from_pruefer(std::vector<int> const& code) {
    int const n = static_cast<int>(code.size()) + 2;
    for (std::size_t at = 0; at < code.size(); at++)
        if (code[at] < 1 || code[at] > n)
            eo::detail::library_error(fmt("a Pruefer code of length {} holds vertices 1..{}; element {} is {}",
                                          code.size(), n, at + 1, code[at]));
    return detail::decoded(n, code);
}

[[nodiscard]] inline graph tree_from_degrees(rng& draw, std::vector<int> const& degrees) {
    int const n = static_cast<int>(degrees.size());
    detail::at_least(n, 1, "a tree from degrees");
    if (n == 1) {
        if (degrees[0] != 0) eo::detail::library_error(fmt("a tree on 1 vertex has degree 0, not {}", degrees[0]));
        return detail::undirected(1, {});
    }
    long long total = 0;
    for (std::size_t at = 0; at < degrees.size(); at++) {
        if (degrees[at] < 1)
            eo::detail::library_error(fmt(
                "every vertex of a tree on {} vertices has a degree of at least 1; vertex {} has {}", n, at + 1,
                degrees[at]));
        total += degrees[at];
    }
    if (total != 2LL * (n - 1))
        eo::detail::library_error(fmt("the degrees of a tree on {} vertices add up to {}, and these add up to {}", n,
                                      2LL * (n - 1), total));
    std::vector<int> code;
    code.reserve(static_cast<std::size_t>(n) - 2);
    for (std::size_t at = 0; at < degrees.size(); at++)
        code.insert(code.end(), static_cast<std::size_t>(degrees[at]) - 1, static_cast<int>(at) + 1);
    draw.shuffle(code);
    return detail::decoded(n, code);
}

[[nodiscard]] inline graph tree_with_leaves(rng& draw, int n, int leaves) {
    detail::at_least(n, 2, "a tree with leaves");
    int const most = n == 2 ? 2 : n - 1;
    if (leaves < 2 || leaves > most)
        eo::detail::library_error(fmt("a tree on {} vertices has 2..{} leaves, not {}", n, most, leaves));
    if (n == 2) return path(2);
    std::vector<int> const inner = draw.perm(n, 1);
    std::size_t const kinds = static_cast<std::size_t>(n - leaves);
    std::vector<int> code(static_cast<std::size_t>(n) - 2, 0);
    std::vector<long long> const places = draw.distinct(static_cast<long long>(kinds), 0, n - 3);
    for (std::size_t at = 0; at < kinds; at++) code[static_cast<std::size_t>(places[at])] = inner[at];
    for (int& chosen : code)
        if (chosen == 0) chosen = inner[static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(kinds) - 1))];
    return detail::decoded(n, code);
}

[[nodiscard]] inline graph tree_with_diameter(rng& draw, int n, int d) {
    detail::at_least(n, 1, "a tree with a diameter");
    int const least = n == 1 ? 0 : (n == 2 ? 1 : 2);
    int const most = n - 1;
    if (d < least || d > most)
        eo::detail::library_error(
            fmt("a tree on {} vertices has a diameter of {}..{}, not {}", n, least, most, d));
    std::vector<int> allowance;
    for (int at = 0; at <= d; at++) allowance.push_back((std::min)(at, d - at));
    return detail::hung_from_path(draw, n, allowance);
}

[[nodiscard]] inline graph tree_with_height(rng& draw, int n, int h) {
    detail::at_least(n, 1, "a tree with a height");
    int const least = n == 1 ? 0 : 1;
    if (h < least || h > n - 1)
        eo::detail::library_error(fmt("a tree on {} vertices has a height of {}..{}, not {}", n, least, n - 1, h));
    std::vector<int> allowance;
    for (int at = 0; at <= h; at++) allowance.push_back(h - at);
    return detail::hung_from_path(draw, n, allowance);
}

[[nodiscard]] inline graph bounded_degree_tree(rng& draw, int n, int most) {
    detail::at_least(n, 1, "a tree with bounded degree");
    int const needed = n >= 3 ? 2 : n - 1;
    if (most < needed)
        eo::detail::library_error(fmt(
            "a tree on {} vertices has a vertex of degree at least {}; a bound of {} leaves none", n, needed, most));
    std::vector<int> room(static_cast<std::size_t>(n) + 1, most);
    std::vector<int> open{1};
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++) {
        std::size_t const at = static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(open.size()) - 1));
        int const parent = open[at];
        edges.push_back(edge{parent, vertex});
        if (--room[static_cast<std::size_t>(parent)] == 0) {
            open[at] = open.back();
            open.pop_back();
        }
        if (--room[static_cast<std::size_t>(vertex)] > 0) open.push_back(vertex);
    }
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph comb(int n) {
    detail::at_least(n, 1, "a comb");
    int const spine = (n + 1) / 2;
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= spine; vertex++) edges.push_back(edge{vertex - 1, vertex});
    for (int vertex = spine + 1; vertex <= n; vertex++) edges.push_back(edge{vertex - spine, vertex});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph staircase(int n) {
    detail::at_least(n, 1, "a staircase");
    int steps = 0;
    while (static_cast<long long>(steps + 1) * (steps + 3) <= n) steps++;
    if (steps == 0) return path(n);
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= steps; vertex++) edges.push_back(edge{vertex - 1, vertex});
    int next = steps + 1;
    for (int step = 1; step <= steps; step++) {
        int const length = 2 * (steps - step + 1) + (step == 1 ? n - steps * (steps + 2) : 0);
        int attach = step;
        for (int at = 0; at < length; at++) {
            edges.push_back(edge{attach, next});
            attach = next++;
        }
    }
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph tree(rng& draw, int n, std::string const& shape) {
    if (shape == "random") return random_tree(draw, n);
    if (shape == "uniform") return uniform_tree(draw, n);
    if (shape == "path") return path(n);
    if (shape == "star") return star(n);
    if (shape == "caterpillar") return caterpillar(draw, n);
    if (shape == "broom") return broom(n);
    if (shape == "binary") return binary_tree(n);
    if (shape == "dumbbell") return dumbbell(n);
    if (shape == "comb") return comb(n);
    if (shape == "staircase") return staircase(n);
    eo::detail::library_error(
        fmt("\"{}\" is not a tree shape; the names are random, uniform, path, star, caterpillar, broom, "
            "binary, dumbbell, comb and staircase",
            shape));
}

}  // namespace shapes
}  // namespace eo
