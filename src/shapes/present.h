#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../core.h"
#include "../diag.h"
#include "../fmt.h"
#include "../random.h"
#include "../structure.h"

namespace eo {

struct graph {
    int n;
    std::vector<edge> edges;
    bool directed;
};

namespace shapes {
namespace detail {

inline void at_least(int n, int least, char const* what) {
    if (n < least) eo::detail::library_error(fmt("{} needs at least {} vertices, not {}", what, least, n));
}

inline graph undirected(int n, std::vector<edge> edges) { return graph{n, std::move(edges), false}; }

inline void still_trying(long long& tries, long long most, char const* what) {
    if (++tries <= most) return;
    eo::detail::library_error(
        fmt("{} did not come out in {} attempts; ask for fewer, or build it another way", what, most));
}

inline void kept_of(long long kept, long long tries, char const* what, eo::detail::site where) {
    if (tries <= 1000 || tries <= 4 * kept) return;
    eo::detail::warn("EO505", fmt("{} kept {} of {} draws", what, kept, tries),
                     "most draws were thrown away; ask for fewer, or build it another way", where);
}

inline std::vector<int> labels_keeping(rng& draw, int n, std::vector<int> const& kept) {
    std::vector<int> labels(static_cast<std::size_t>(n), 0);
    for (int const vertex : kept) {
        if (vertex < 1 || vertex > n)
            eo::detail::library_error(fmt("a kept vertex is {}, outside 1..{}", vertex, n));
        if (labels[static_cast<std::size_t>(vertex) - 1] != 0)
            eo::detail::library_error(fmt("vertex {} is kept twice; keep each vertex once", vertex));
        labels[static_cast<std::size_t>(vertex) - 1] = vertex;
    }
    std::vector<int> moving;
    for (int vertex = 1; vertex <= n; vertex++)
        if (labels[static_cast<std::size_t>(vertex) - 1] == 0) moving.push_back(vertex);
    std::vector<int> shuffled = moving;
    draw.shuffle(shuffled);
    for (std::size_t at = 0; at < moving.size(); at++) labels[static_cast<std::size_t>(moving[at]) - 1] = shuffled[at];
    return labels;
}

template <class Edge>
std::vector<Edge> relabelled(rng& draw, std::vector<int> const& labels, std::vector<Edge> const& edges,
                             bool directed) {
    std::vector<Edge> shown;
    shown.reserve(edges.size());
    for (Edge one : edges) {
        one.u = labels[static_cast<std::size_t>(one.u) - 1];
        one.v = labels[static_cast<std::size_t>(one.v) - 1];
        if (!directed && draw.chance(0.5)) std::swap(one.u, one.v);
        shown.push_back(one);
    }
    draw.shuffle(shown);
    return shown;
}

}  // namespace detail

[[nodiscard]] inline std::vector<edge> presented(rng& draw, graph const& made) {
    std::vector<int> const labels = draw.perm(made.n, 1);
    std::vector<edge> shown;
    shown.reserve(made.edges.size());
    for (edge const& one : made.edges) {
        int const u = labels[static_cast<std::size_t>(one.u) - 1];
        int const v = labels[static_cast<std::size_t>(one.v) - 1];
        bool const turn = !made.directed && draw.chance(0.5);
        shown.push_back(turn ? edge{v, u} : edge{u, v});
    }
    draw.shuffle(shown);
    return shown;
}

[[nodiscard]] inline std::vector<edge> presented(rng& draw, graph const& made, std::vector<int> const& kept) {
    std::vector<int> const labels = detail::labels_keeping(draw, made.n, kept);
    return detail::relabelled(draw, labels, made.edges, made.directed);
}

[[nodiscard]] inline std::vector<int> parent_array(rng& draw, graph const& made, int root = 1) {
    detail::at_least(made.n, 1, "a parent array");
    if (root < 1 || root > made.n)
        eo::detail::library_error(fmt("the root is {}, outside 1..{}", root, made.n));
    std::vector<std::vector<int>> neighbours(static_cast<std::size_t>(made.n) + 1);
    for (edge const& one : made.edges) {
        neighbours[static_cast<std::size_t>(one.u)].push_back(one.v);
        neighbours[static_cast<std::size_t>(one.v)].push_back(one.u);
    }
    std::vector<int> label(static_cast<std::size_t>(made.n) + 1, 0);
    std::vector<int> came_from(static_cast<std::size_t>(made.n) + 1, 0);
    std::vector<int> waiting{root};
    label[static_cast<std::size_t>(root)] = 1;
    int given = 1;
    while (!waiting.empty()) {
        int const here = waiting.back();
        waiting.pop_back();
        std::vector<int> children;
        for (int const other : neighbours[static_cast<std::size_t>(here)])
            if (other != came_from[static_cast<std::size_t>(here)]) children.push_back(other);
        draw.shuffle(children);
        for (int const other : children) {
            came_from[static_cast<std::size_t>(other)] = here;
            label[static_cast<std::size_t>(other)] = ++given;
            waiting.push_back(other);
        }
    }
    if (given != made.n)
        eo::detail::library_error(fmt("a parent array needs a tree; {} of {} vertices were reached from {}",
                                      given, made.n, root));
    std::vector<int> parents(static_cast<std::size_t>(made.n) - 1, 0);
    for (int vertex = 1; vertex <= made.n; vertex++)
        if (vertex != root)
            parents[static_cast<std::size_t>(label[static_cast<std::size_t>(vertex)]) - 2] =
                label[static_cast<std::size_t>(came_from[static_cast<std::size_t>(vertex)])];
    return parents;
}

}  // namespace shapes
}  // namespace eo
