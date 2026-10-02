#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core.h"
#include "fmt.h"

namespace eo {

struct edge {
    int u;
    int v;
};

struct weighted_edge {
    int u;
    int v;
    long long w;
};

struct weight_bounds {
    long long low;
    long long high;
};

inline weight_bounds weighted(long long low, long long high) { return weight_bounds{low, high}; }

enum graph_shape {
    any_graph = 0,
    simple = 1,
    connected = 2,
};

inline graph_shape operator|(graph_shape left, graph_shape right) {
    return static_cast<graph_shape>(static_cast<int>(left) | static_cast<int>(right));
}

class [[nodiscard]] check_result {
public:
    check_result() = default;
    check_result(std::string complaint) : complaint_(std::move(complaint)) {}

    explicit operator bool() const { return complaint_.empty(); }
    std::string const& message() const { return complaint_; }

private:
    std::string complaint_;
};

namespace detail {

struct first_repeat {
    std::size_t first;
    std::size_t second;
};

template <class T>
inline first_repeat earliest_repeat(std::vector<std::pair<T, std::size_t>> placed) {
    std::less<T> const before;
    std::sort(placed.begin(), placed.end(), [&](auto const& left, auto const& right) {
        if (before(left.first, right.first)) return true;
        if (before(right.first, left.first)) return false;
        return left.second < right.second;
    });
    first_repeat found{placed.size(), placed.size()};
    std::size_t group = 0;
    for (std::size_t at = 1; at < placed.size(); at++) {
        if (before(placed[at - 1].first, placed[at].first)) {
            group = at;
            continue;
        }
        if (at == group + 1 && placed[at].second < found.second)
            found = {placed[group].second, placed[at].second};
    }
    return found;
}

}  // namespace detail

template <class T>
[[nodiscard]] inline check_result all_distinct(std::vector<T> const& values) {
    std::vector<std::pair<T, std::size_t>> placed;
    placed.reserve(values.size());
    for (std::size_t at = 0; at < values.size(); at++) placed.emplace_back(values[at], at);
    detail::first_repeat const found = detail::earliest_repeat(std::move(placed));
    if (found.second == values.size()) return {};
    return check_result(
        fmt("elements {} and {} are both {}", found.first + 1, found.second + 1, values[found.second]));
}

template <class T>
[[nodiscard]] inline check_result is_sorted(std::vector<T> const& values) {
    for (std::size_t at = 1; at < values.size(); at++)
        if (values[at] < values[at - 1])
            return check_result(
                fmt("element {} is {} and element {} is {}", at, values[at - 1], at + 1, values[at]));
    return {};
}

[[nodiscard]] inline check_result is_permutation(std::vector<int> const& values) {
    int const size = static_cast<int>(values.size());
    std::vector<int> place(static_cast<std::size_t>(size) + 1, 0);
    for (std::size_t at = 0; at < values.size(); at++) {
        int const value = values[at];
        if (value < 1 || value > size)
            return check_result(fmt("element {} is {}, outside 1..{}", at + 1, value, size));
        if (place[static_cast<std::size_t>(value)] != 0)
            return check_result(fmt("elements {} and {} are both {}", place[static_cast<std::size_t>(value)],
                                    at + 1, value));
        place[static_cast<std::size_t>(value)] = static_cast<int>(at) + 1;
    }
    return {};
}

namespace detail {

inline check_result vertices_are_inside(int n, std::vector<edge> const& edges) {
    for (std::size_t at = 0; at < edges.size(); at++) {
        edge const& one = edges[at];
        if (one.u < 1 || one.u > n || one.v < 1 || one.v > n)
            return check_result(fmt("edge {} is ({}, {}), outside 1..{}", at + 1, one.u, one.v, n));
    }
    return {};
}

inline int root_of(std::vector<int>& parent, int vertex) {
    while (parent[static_cast<std::size_t>(vertex)] != vertex) {
        parent[static_cast<std::size_t>(vertex)] = parent[static_cast<std::size_t>(
            parent[static_cast<std::size_t>(vertex)])];
        vertex = parent[static_cast<std::size_t>(vertex)];
    }
    return vertex;
}

}  // namespace detail

[[nodiscard]] inline check_result is_simple_graph(int n, std::vector<edge> const& edges) {
    if (check_result inside = detail::vertices_are_inside(n, edges); !inside) return inside;
    std::size_t loop = 0;
    while (loop < edges.size() && edges[loop].u != edges[loop].v) loop++;
    std::vector<std::pair<std::pair<int, int>, std::size_t>> placed;
    placed.reserve(loop);
    for (std::size_t at = 0; at < loop; at++)
        placed.push_back({{(std::min)(edges[at].u, edges[at].v), (std::max)(edges[at].u, edges[at].v)}, at});
    detail::first_repeat const found = detail::earliest_repeat(std::move(placed));
    if (found.second < loop) {
        edge const& one = edges[found.second];
        return check_result(fmt("edges {} and {} are both ({}, {})", found.first + 1, found.second + 1,
                                (std::min)(one.u, one.v), (std::max)(one.u, one.v)));
    }
    if (loop < edges.size()) return check_result(fmt("edge {} is a loop at vertex {}", loop + 1, edges[loop].u));
    return {};
}

[[nodiscard]] inline check_result is_connected(int n, std::vector<edge> const& edges) {
    if (check_result inside = detail::vertices_are_inside(n, edges); !inside) return inside;
    std::vector<int> parent(static_cast<std::size_t>(n) + 1);
    for (int vertex = 1; vertex <= n; vertex++) parent[static_cast<std::size_t>(vertex)] = vertex;
    for (edge const& one : edges) {
        int const left = detail::root_of(parent, one.u);
        int const right = detail::root_of(parent, one.v);
        parent[static_cast<std::size_t>(left)] = right;
    }
    for (int vertex = 2; vertex <= n; vertex++)
        if (detail::root_of(parent, vertex) != detail::root_of(parent, 1))
            return check_result(fmt("vertex {} cannot be reached from vertex 1", vertex));
    return {};
}

namespace detail {

inline bool joins_without_a_cycle(int n, std::vector<edge> const& edges) {
    std::vector<int> parent(static_cast<std::size_t>(n) + 1);
    for (int vertex = 1; vertex <= n; vertex++) parent[static_cast<std::size_t>(vertex)] = vertex;
    for (edge const& one : edges) {
        int const left = root_of(parent, one.u);
        int const right = root_of(parent, one.v);
        if (left == right) return false;
        parent[static_cast<std::size_t>(left)] = right;
    }
    return true;
}

}  // namespace detail

[[nodiscard]] inline check_result is_tree(int n, std::vector<edge> const& edges) {
    if (edges.size() + 1 != static_cast<std::size_t>(n))
        return check_result(fmt("a tree on {} vertices has {} edges, not {}", n, n - 1, edges.size()));
    if (detail::vertices_are_inside(n, edges) && detail::joins_without_a_cycle(n, edges)) return {};
    if (check_result simple_enough = is_simple_graph(n, edges); !simple_enough) return simple_enough;
    return is_connected(n, edges);
}

namespace detail {

inline std::vector<edge> endpoints(std::vector<weighted_edge> const& edges) {
    std::vector<edge> ends;
    ends.reserve(edges.size());
    for (weighted_edge const& one : edges) ends.push_back(edge{one.u, one.v});
    return ends;
}

inline check_result shaped(int n, std::vector<edge> const& edges, graph_shape shape) {
    if (check_result inside = vertices_are_inside(n, edges); !inside) return inside;
    if ((shape & simple) != 0)
        if (check_result plain = is_simple_graph(n, edges); !plain) return plain;
    if ((shape & connected) != 0) return is_connected(n, edges);
    return {};
}

struct tree_shape {
    long long max_degree = 0;
    long long leaves = 0;
    long long depth = 0;
    long long diameter = 0;
};

class adjacency {
public:
    adjacency(int n, std::vector<edge> const& edges)
        : start_(static_cast<std::size_t>(n) + 2, 0), ends_(2 * edges.size()) {
        for (edge const& one : edges) {
            start_[static_cast<std::size_t>(one.u) + 1]++;
            start_[static_cast<std::size_t>(one.v) + 1]++;
        }
        for (std::size_t at = 1; at < start_.size(); at++) start_[at] += start_[at - 1];
        std::vector<std::size_t> filled(start_.begin(), start_.end() - 1);
        for (edge const& one : edges) {
            ends_[filled[static_cast<std::size_t>(one.u)]++] = one.v;
            ends_[filled[static_cast<std::size_t>(one.v)]++] = one.u;
        }
    }

    long long degree(int vertex) const {
        return static_cast<long long>(start_[static_cast<std::size_t>(vertex) + 1] -
                                      start_[static_cast<std::size_t>(vertex)]);
    }

    std::pair<int, long long> farthest_from(int from) const {
        std::vector<long long> distance(start_.size() - 1, -1);
        std::vector<int> queue{from};
        distance[static_cast<std::size_t>(from)] = 0;
        for (std::size_t at = 0; at < queue.size(); at++) {
            std::size_t const here = static_cast<std::size_t>(queue[at]);
            for (std::size_t next = start_[here]; next < start_[here + 1]; next++)
                if (distance[static_cast<std::size_t>(ends_[next])] < 0) {
                    distance[static_cast<std::size_t>(ends_[next])] = distance[here] + 1;
                    queue.push_back(ends_[next]);
                }
        }
        int const last = queue.back();
        return {last, distance[static_cast<std::size_t>(last)]};
    }

private:
    std::vector<std::size_t> start_;
    std::vector<int> ends_;
};

inline tree_shape shape_of_tree(int n, std::vector<edge> const& edges) {
    adjacency const around(n, edges);
    tree_shape shape;
    for (int vertex = 1; vertex <= n; vertex++) {
        shape.max_degree = (std::max)(shape.max_degree, around.degree(vertex));
        if (around.degree(vertex) == 1) shape.leaves++;
    }
    std::pair<int, long long> const deepest = around.farthest_from(1);
    shape.depth = deepest.second;
    shape.diameter = around.farthest_from(deepest.first).second;
    return shape;
}

inline std::pair<long long, long long> degree_and_components(int n, std::vector<edge> const& edges) {
    std::vector<long long> degree(static_cast<std::size_t>(n) + 1, 0);
    std::vector<int> parent(static_cast<std::size_t>(n) + 1);
    for (int vertex = 1; vertex <= n; vertex++) parent[static_cast<std::size_t>(vertex)] = vertex;
    long long components = n;
    for (edge const& one : edges) {
        degree[static_cast<std::size_t>(one.u)]++;
        degree[static_cast<std::size_t>(one.v)]++;
        int const left = root_of(parent, one.u);
        int const right = root_of(parent, one.v);
        if (left != right) components--;
        parent[static_cast<std::size_t>(left)] = right;
    }
    return {*std::max_element(degree.begin(), degree.end()), components};
}

}  // namespace detail

}  // namespace eo
