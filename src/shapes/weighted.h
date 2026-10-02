#pragma once

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

}  // namespace shapes
}  // namespace eo
