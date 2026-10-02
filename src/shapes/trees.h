#pragma once

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

[[nodiscard]] inline graph tree(rng& draw, int n, std::string const& shape) {
    if (shape == "random") return random_tree(draw, n);
    if (shape == "uniform") return uniform_tree(draw, n);
    if (shape == "path") return path(n);
    if (shape == "star") return star(n);
    if (shape == "caterpillar") return caterpillar(draw, n);
    if (shape == "broom") return broom(n);
    if (shape == "binary") return binary_tree(n);
    if (shape == "dumbbell") return dumbbell(n);
    eo::detail::library_error(
        fmt("\"{}\" is not a tree shape; the names are random, uniform, path, star, caterpillar, broom, "
            "binary and dumbbell",
            shape));
}

}  // namespace shapes
}  // namespace eo
