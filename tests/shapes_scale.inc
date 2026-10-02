namespace {

double seconds_for(std::function<void(int)> const& shape, int n) {
    double best = 1e9;
    for (int round = 0; round < 2; round++) {
        auto const started = std::chrono::steady_clock::now();
        shape(n);
        best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    }
    return best;
}

}  // namespace

EO_TEST(every_new_shape_grows_about_linearly_with_its_size) {
    if (eot::sanitized()) return;
    eo::rng draw(1101);
    std::vector<std::pair<char const*, std::function<void(int)>>> const shapes{
        {"permutation_cycles", [&](int n) { (void)eo::shapes::permutation_cycles(draw, n, n / 3); }},
        {"derangement", [&](int n) { (void)eo::shapes::derangement(draw, n); }},
        {"with_inversions", [&](int n) { (void)eo::shapes::with_inversions(draw, n, 1LL * n * (n - 1) / 3); }},
        {"with_lis", [&](int n) { (void)eo::shapes::with_lis(draw, n, n / 2); }},
        {"log_uniform", [&](int n) { (void)eo::shapes::log_uniform(draw, n, 1, 1000000000000000000LL); }},
        {"split_sum", [&](int n) { (void)eo::shapes::split_sum(draw, 2LL * n, n / 2, 1, 5); }},
        {"distinct_gapped", [&](int n) { (void)eo::shapes::distinct_gapped(draw, n, 0, 1000000000, 1000); }},
        {"mountain", [&](int n) { (void)eo::shapes::mountain(draw, n, 1, 1000000000); }},
        {"intervals laminar", [&](int n) { (void)eo::shapes::intervals(draw, n, 1, 1000000000, "laminar"); }},
        {"ranges", [&](int n) { (void)eo::shapes::ranges(draw, n, n, "long"); }},
        {"tree_with_leaves", [&](int n) { (void)eo::shapes::tree_with_leaves(draw, n, n / 2); }},
        {"tree_with_diameter", [&](int n) { (void)eo::shapes::tree_with_diameter(draw, n, n / 3); }},
        {"bounded_degree_tree", [&](int n) { (void)eo::shapes::bounded_degree_tree(draw, n, 3); }},
        {"staircase", [&](int n) { (void)eo::shapes::staircase(n); }},
        {"regular_graph", [&](int n) { (void)eo::shapes::regular_graph(draw, n, 4); }},
        {"cactus", [&](int n) { (void)eo::shapes::cactus(draw, n, 10); }},
        {"with_bridges", [&](int n) { (void)eo::shapes::with_bridges(draw, n, n / 3); }},
        {"with_cut_vertices", [&](int n) { (void)eo::shapes::with_cut_vertices(draw, n, n / 3); }},
        {"euler_circuit", [&](int n) { (void)eo::shapes::euler_circuit(draw, n, 3LL * n); }},
        {"perfect_matching", [&](int n) { (void)eo::shapes::perfect_matching(draw, n, 3LL * n); }},
        {"with_sccs", [&](int n) { (void)eo::shapes::with_sccs(draw, n, n / 100, 3LL * n); }},
        {"graph_with_diameter", [&](int n) { (void)eo::shapes::graph_with_diameter(draw, n, 3LL * n, 20); }},
        {"anti_spfa", [&](int n) { (void)eo::shapes::anti_spfa(draw, n, 1000000000); }},
        {"anti_dijkstra", [&](int n) { (void)eo::shapes::anti_dijkstra(n); }},
        {"maze", [&](int n) { (void)eo::shapes::maze(draw, n / 100, 100); }},
        {"spiral", [&](int n) { (void)eo::shapes::spiral(n / 100, 100); }},
        {"lyndon", [&](int n) { (void)eo::shapes::lyndon(draw, n, eo::charset("ab")); }},
        {"general_position", [&](int n) { (void)eo::shapes::general_position(draw, n, 1000000000); }},
        {"simple_polygon", [&](int n) { (void)eo::shapes::simple_polygon(draw, n, 1000000000); }},
        {"strictly_convex", [&](int n) { (void)eo::shapes::strictly_convex(draw, n, 1000000000); }},
    };
    for (auto const& [name, shape] : shapes) {
        double const small = seconds_for(shape, 10000);
        double const large = seconds_for(shape, 40000);
        bool const linear = large < 0.05 || large < 10 * small;
        if (!linear) std::fprintf(stderr, "%s took %.4f s at 10000 and %.4f s at 40000\n", name, small, large);
        EO_CHECK(linear);
    }
}
