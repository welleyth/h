#pragma once

#include <algorithm>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "../structure.h"
#include "present.h"

namespace eo {

struct weighted_graph {
    int n;
    std::vector<weighted_edge> edges;
    bool directed;
};

namespace shapes {

[[nodiscard]] inline weighted_graph with_weights(rng& draw, graph const& made, long long low, long long high) {
    if (low > high) eo::detail::library_error(fmt("with_weights draws from {}..{}, which is empty", low, high));
    weighted_graph heavy{made.n, {}, made.directed};
    heavy.edges.reserve(made.edges.size());
    for (edge const& one : made.edges) heavy.edges.push_back(weighted_edge{one.u, one.v, draw.uniform(low, high)});
    return heavy;
}

[[nodiscard]] inline std::vector<weighted_edge> presented(rng& draw, weighted_graph const& made) {
    std::vector<int> const labels = draw.perm(made.n, 1);
    return detail::relabelled(draw, labels, made.edges, made.directed);
}

[[nodiscard]] inline std::vector<weighted_edge> presented(rng& draw, weighted_graph const& made,
                                                          std::vector<int> const& kept) {
    std::vector<int> const labels = detail::labels_keeping(draw, made.n, kept);
    return detail::relabelled(draw, labels, made.edges, made.directed);
}

[[nodiscard]] inline weighted_graph anti_spfa(rng& draw, int n, long long high) {
    detail::at_least(n, 1, "anti_spfa");
    if (high < 1) eo::detail::library_error(fmt("anti_spfa draws weights from 1..high, and high is {}", high));
    int const rows = (std::min)(n, 10);
    long long const rung = (std::min)(high, 10LL);
    weighted_graph made{n, {}, false};
    for (int vertex = 1; vertex <= n; vertex++) {
        if ((vertex - 1) % rows + 1 < rows && vertex < n)
            made.edges.push_back(weighted_edge{vertex, vertex + 1, draw.uniform(1, rung)});
        if (vertex + rows <= n) made.edges.push_back(weighted_edge{vertex, vertex + rows, draw.uniform(1, high)});
    }
    return made;
}

[[nodiscard]] inline weighted_graph anti_dijkstra(int n) {
    detail::at_least(n, 1, "anti_dijkstra");
    weighted_graph made{n, {}, false};
    if (n == 2) made.edges.push_back(weighted_edge{1, 2, 1});
    if (n <= 2) return made;
    int const ways = (std::max)(1, (n - 2) / 2);
    int const hub = ways + 2;
    for (int way = 1; way <= ways; way++) {
        made.edges.push_back(weighted_edge{1, way + 1, way});
        made.edges.push_back(weighted_edge{way + 1, hub, 2 * (ways - way) + 1});
    }
    for (int leaf = hub + 1; leaf <= n; leaf++) made.edges.push_back(weighted_edge{hub, leaf, 1});
    return made;
}

}  // namespace shapes
}  // namespace eo
