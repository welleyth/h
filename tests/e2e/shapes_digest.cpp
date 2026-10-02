#include "../../eolymp-shapes.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

void edges(char const* tag, eo::graph const& made) {
    for (eo::edge const& one : made.edges) std::printf("%s %d %d\n", tag, one.u, one.v);
}

void values(char const* tag, std::vector<long long> const& made) {
    for (long long const one : made) std::printf("%s %lld\n", tag, one);
}

void ints(char const* tag, std::vector<int> const& made) {
    for (int const one : made) std::printf("%s %d\n", tag, one);
}

void points(char const* tag, std::vector<eo::point> const& made) {
    for (eo::point const& one : made) std::printf("%s %lld %lld\n", tag, one.x, one.y);
}

void rows(char const* tag, std::vector<std::string> const& made) {
    for (std::string const& one : made) std::printf("%s %s\n", tag, one.c_str());
}

void catalogue(eo::rng& draw) {
    ints("pc", eo::shapes::permutation_cycles(draw, 50, 4));
    ints("pd", eo::shapes::derangement(draw, 50));
    ints("pi", eo::shapes::involution(draw, 50, 6));
    ints("pv", eo::shapes::with_inversions(draw, 50, 400));
    ints("pl", eo::shapes::with_lis(draw, 50, 7));
    values("sl", eo::shapes::log_uniform(draw, 50, 1, 1000000000000000000LL));
    values("sn", eo::shapes::near_bounds(draw, 50, -1000000000, 1000000000, 5));
    values("ss", eo::shapes::spikes(draw, 50, 5, 1, 1000000000));
    values("su", eo::shapes::split_sum(draw, 1000, 30, 1, 60));
    values("sg", eo::shapes::distinct_gapped(draw, 50, 1, 1000000, 1000));
    values("sm", eo::shapes::mountain(draw, 50, 1, 1000));
    for (char const* name :
         {"random", "disjoint", "touching", "nested", "laminar", "chain", "through", "same", "points"})
        for (eo::interval const& one : eo::shapes::intervals(draw, 20, 1, 1000, name))
            std::printf("i %lld %lld\n", one.l, one.r);
    for (eo::interval const& one : eo::shapes::ranges(draw, 30, 1000, "short"))
        std::printf("q %lld %lld\n", one.l, one.r);
    ints("qo", eo::shapes::query_order(draw, {10, 7, 3}, "random"));
    edges("td", eo::shapes::tree_from_degrees(draw, {1, 3, 1, 2, 1, 4, 1, 1, 1, 3}));
    edges("tl", eo::shapes::tree_with_leaves(draw, 60, 20));
    edges("tm", eo::shapes::tree_with_diameter(draw, 60, 12));
    edges("th", eo::shapes::tree_with_height(draw, 60, 9));
    edges("tb", eo::shapes::bounded_degree_tree(draw, 60, 3));
    edges("gr", eo::shapes::regular_graph(draw, 40, 5));
    edges("gc", eo::shapes::cactus(draw, 60, 6));
    edges("gb", eo::shapes::with_bridges(draw, 60, 12));
    edges("gv", eo::shapes::with_cut_vertices(draw, 60, 12));
    edges("ge", eo::shapes::euler_circuit(draw, 30, 90));
    edges("gp", eo::shapes::euler_path(draw, 30, 90));
    edges("gm", eo::shapes::perfect_matching(draw, 20, 60));
    edges("gt", eo::shapes::tournament(draw, 12));
    edges("gs", eo::shapes::with_sccs(draw, 40, 5, 120));
    edges("gd", eo::shapes::graph_with_diameter(draw, 40, 100, 7));
    for (eo::weighted_edge const& one : eo::shapes::presented(draw, eo::shapes::anti_spfa(draw, 60, 1000000000), {1}))
        std::printf("w %d %d %lld\n", one.u, one.v, one.w);
    for (eo::weighted_edge const& one : eo::shapes::layered_network(draw, 3, 4, 1000).edges)
        std::printf("f %d %d %lld\n", one.u, one.v, one.w);
    rows("m", eo::shapes::maze(draw, 15, 21));
    rows("x", eo::shapes::scattered_walls(draw, 10, 12, 0.4));
    std::printf("l %s\n", eo::shapes::lyndon(draw, 60, eo::charset("abc")).c_str());
    std::pair<std::string, std::string> const twins =
        eo::shapes::anti_hash(draw, {{131, 1000000007}, {137, 998244353}, {10007, 1000000009}}, eo::charset("a-z"));
    std::printf("h %zu %s\n", twins.first.size(), twins.second.substr(0, 200).c_str());
    std::printf("n %lld\n", eo::shapes::random_prime(draw, 1, 1000000000000000000LL));
    std::printf("n %lld\n", eo::shapes::semiprime(draw, 1000000000000000000LL));
    std::printf("n %lld\n", eo::shapes::carmichael(draw, 1000000000000LL));
    std::printf("n %lld\n", eo::shapes::prime_power(draw, 1000000000000000000LL, 3));
    points("pg", eo::shapes::general_position(draw, 50, 1000000000));
    points("ps", eo::shapes::simple_polygon(draw, 50, 1000000));
    points("pk", eo::shapes::strictly_convex(draw, 50, 1000000));
    for (auto const& one : eo::shapes::crossing_segments(draw, 10, 1000))
        std::printf("c %lld %lld %lld %lld\n", one.first.x, one.first.y, one.second.x, one.second.y);
    ints("e", eo::shapes::permutation_at(12, 123456789));
    edges("et", eo::shapes::tree_at(9, 1234567));
    edges("eg", eo::shapes::graph_at(9, 12345678901LL));
}

}  // namespace

int main() {
    eo::rng draw(20260922);

    for (eo::point const& one : eo::shapes::convex_position(draw, 500, 100000))
        std::printf("c %lld %lld\n", one.x, one.y);
    for (eo::point const& one : eo::shapes::cocircular(draw, 100)) std::printf("o %lld %lld\n", one.x, one.y);
    for (eo::point const& one : eo::shapes::collinear(draw, 100, 1000000))
        std::printf("l %lld %lld\n", one.x, one.y);
    for (eo::point const& one : eo::shapes::extreme_points(draw, 100, 1000000))
        std::printf("e %lld %lld\n", one.x, one.y);

    for (long long const one : draw.distinct(100, 1, 1000000000)) std::printf("d %lld\n", one);
    for (long long const one : draw.partition(20, 1000)) std::printf("p %lld\n", one);

    eo::graph const made = eo::shapes::connected_graph(draw, 200, 600);
    for (eo::edge const& one : eo::shapes::presented(draw, made)) std::printf("g %d %d\n", one.u, one.v);
    for (int const one : eo::shapes::parent_array(draw, eo::shapes::uniform_tree(draw, 200)))
        std::printf("t %d\n", one);

    catalogue(draw);
}
