#include "fuzz.h"
#include "../../eolymp-shapes.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <vector>

namespace {

bool simple(eo::graph const& made) {
    if (made.directed) {
        std::vector<std::pair<int, int>> arcs;
        for (eo::edge const& one : made.edges) {
            if (one.u == one.v || one.u < 1 || one.v < 1 || one.u > made.n || one.v > made.n) return false;
            arcs.push_back({one.u, one.v});
        }
        std::sort(arcs.begin(), arcs.end());
        return std::adjacent_find(arcs.begin(), arcs.end()) == arcs.end();
    }
    return static_cast<bool>(eo::is_simple_graph(made.n, made.edges));
}

bool tree(eo::graph const& made) { return static_cast<bool>(eo::is_tree(made.n, made.edges)); }

void check(bool holds) {
    if (!holds) std::abort();
}

void shape(FuzzedDataProvider& fdp, eo::rng& draw) {
    int const which = fdp.ConsumeIntegralInRange(0, 24);
    int const n = fdp.ConsumeIntegralInRange(-2, 40);
    int const k = fdp.ConsumeIntegralInRange(-2, 40);
    long long const m = fdp.ConsumeIntegralInRange<long long>(-2, 200);
    long long const low = fdp.ConsumeIntegralInRange<long long>(-50, 50);
    long long const high = fdp.ConsumeIntegralInRange<long long>(-50, 50);
    switch (which) {
    case 0: check(static_cast<bool>(eo::is_permutation(eo::shapes::permutation_cycles(draw, n, k)))); break;
    case 1: check(static_cast<bool>(eo::is_permutation(eo::shapes::with_inversions(draw, n, m)))); break;
    case 2: check(static_cast<bool>(eo::is_permutation(eo::shapes::with_lis(draw, n, k)))); break;
    case 3: check(static_cast<bool>(eo::is_permutation(eo::shapes::involution(draw, n, k)))); break;
    case 4: {
        std::vector<long long> const parts = eo::shapes::split_sum(draw, m, n, low, high);
        long long sum = 0;
        for (long long const part : parts) sum += part;
        check(sum == m && static_cast<int>(parts.size()) == n);
        break;
    }
    case 5: check(static_cast<long long>(eo::shapes::distinct_gapped(draw, n, low, high, k).size()) == n); break;
    case 6: check(static_cast<long long>(eo::shapes::mountain(draw, n, low, high).size()) == n); break;
    case 7: check(static_cast<long long>(eo::shapes::intervals(draw, n, low, high, "laminar").size()) == n); break;
    case 8: check(tree(eo::shapes::tree_with_leaves(draw, n, k))); break;
    case 9: check(tree(eo::shapes::tree_with_diameter(draw, n, k))); break;
    case 10: check(tree(eo::shapes::tree_with_height(draw, n, k))); break;
    case 11: check(tree(eo::shapes::bounded_degree_tree(draw, n, k))); break;
    case 12: check(simple(eo::shapes::regular_graph(draw, n, k))); break;
    case 13: check(simple(eo::shapes::cactus(draw, n, k))); break;
    case 14: check(simple(eo::shapes::with_bridges(draw, n, k))); break;
    case 15: check(simple(eo::shapes::with_cut_vertices(draw, n, k))); break;
    case 16: check(simple(eo::shapes::euler_circuit(draw, n, m))); break;
    case 17: check(simple(eo::shapes::euler_path(draw, n, m))); break;
    case 18: check(simple(eo::shapes::perfect_matching(draw, n, m))); break;
    case 19: check(simple(eo::shapes::with_sccs(draw, n, k, m))); break;
    case 20: check(simple(eo::shapes::graph_with_diameter(draw, n, m, k))); break;
    case 21: check(static_cast<int>(eo::shapes::maze(draw, n, k).size()) == n); break;
    case 22: check(static_cast<long long>(eo::shapes::strictly_convex(draw, m, high).size()) == m); break;
    case 23: check(static_cast<long long>(eo::shapes::simple_polygon(draw, m, high).size()) == m); break;
    default: {
        std::pair<std::string, std::string> const twins =
            eo::shapes::anti_hash(draw, {{low, high}, {k, m}}, eo::charset("ab"));
        check(twins.first.size() == twins.second.size() && twins.first != twins.second);
    }
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size) {
    FuzzedDataProvider fdp(data, size);
    eo::rng draw(fdp.ConsumeIntegral<std::uint64_t>());
    eof::verdict const result = eof::run([&] { shape(fdp, draw); });
    if (result.stopped && result.code != 3) std::abort();
    return 0;
}
