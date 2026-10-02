namespace {

std::string weighted_of(std::vector<eo::weighted_edge> const& edges) {
    std::string out;
    for (eo::weighted_edge const& one : edges) out += eo::fmt("{} {} {};", one.u, one.v, one.w);
    return out;
}

std::vector<std::string> sorted_pairs(std::vector<eo::edge> const& edges, bool directed) {
    std::vector<std::string> out;
    for (eo::edge const& one : edges)
        out.push_back(directed ? eo::fmt("{}>{}", one.u, one.v)
                               : eo::fmt("{}-{}", std::min(one.u, one.v), std::max(one.u, one.v)));
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

EO_TEST(weights_ride_along_with_their_edges_through_the_presentation) {
    eo::rng draw(601);
    eo::graph const made = eo::shapes::connected_graph(draw, 50, 120);
    eo::weighted_graph const heavy = eo::shapes::with_weights(draw, made, 1, 1000000000);
    EO_CHECK_EQ(heavy.n, 50);
    EO_CHECK(!heavy.directed);
    EO_CHECK_EQ(heavy.edges.size(), made.edges.size());
    bool same = true;
    bool inside = true;
    for (std::size_t at = 0; at < made.edges.size(); at++) {
        same = same && heavy.edges[at].u == made.edges[at].u && heavy.edges[at].v == made.edges[at].v;
        inside = inside && heavy.edges[at].w >= 1 && heavy.edges[at].w <= 1000000000;
    }
    EO_CHECK(same && inside);
    std::vector<eo::weighted_edge> const shown = eo::shapes::presented(draw, heavy);
    EO_CHECK_EQ(shown.size(), heavy.edges.size());
    std::vector<long long> before;
    std::vector<long long> after;
    for (eo::weighted_edge const& one : heavy.edges) before.push_back(one.w);
    for (eo::weighted_edge const& one : shown) after.push_back(one.w);
    EO_CHECK(before != after);
    std::sort(before.begin(), before.end());
    std::sort(after.begin(), after.end());
    EO_CHECK(before == after);
    EO_CHECK(static_cast<bool>(eo::is_connected(50, eo::detail::endpoints(shown))));
    eo::weighted_graph const arrows = eo::shapes::with_weights(draw, eo::shapes::dag(draw, 30, 60), -5, 5);
    EO_CHECK(arrows.directed);
    std::vector<eo::weighted_edge> const kept = eo::shapes::presented(draw, arrows);
    EO_CHECK(stays_acyclic(30, eo::detail::endpoints(kept)));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_weights(draw, made, 5, 4); }),
                "with_weights draws from 5..4, which is empty");
}

EO_TEST(presenting_with_kept_vertices_leaves_their_labels_alone) {
    eo::rng draw(602);
    eo::graph const line = eo::shapes::path(100);
    std::vector<eo::edge> const shown = eo::shapes::presented(draw, line, {1, 100});
    EO_CHECK(static_cast<bool>(eo::is_tree(100, shown)));
    int ends_at_one = 0;
    int ends_at_hundred = 0;
    std::vector<int> degree(101, 0);
    for (eo::edge const& one : shown) {
        degree[static_cast<std::size_t>(one.u)]++;
        degree[static_cast<std::size_t>(one.v)]++;
    }
    ends_at_one = degree[1];
    ends_at_hundred = degree[100];
    EO_CHECK_EQ(ends_at_one, 1);
    EO_CHECK_EQ(ends_at_hundred, 1);
    EO_CHECK(sorted_pairs(shown, false) != sorted_pairs(line.edges, false));
    eo::weighted_graph const heavy = eo::shapes::with_weights(draw, eo::shapes::star(40), 7, 7);
    std::vector<eo::weighted_edge> const centred = eo::shapes::presented(draw, heavy, {1});
    bool hub_kept = true;
    for (eo::weighted_edge const& one : centred) hub_kept = hub_kept && (one.u == 1 || one.v == 1) && one.w == 7;
    EO_CHECK(hub_kept);
    std::vector<eo::edge> const plain = eo::shapes::presented(draw, eo::shapes::star(40), {});
    EO_CHECK_EQ(plain.size(), static_cast<std::size_t>(39));
    std::string drawn = edges_of(eo::shapes::presented(draw, eo::shapes::path(12), {1}));
    drawn += weighted_of(eo::shapes::presented(draw, eo::shapes::with_weights(draw, eo::shapes::path(12), 1, 9)));
    pin_bytes(drawn, 120, 0x9b21aa1fe928942aull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::presented(draw, line, {0}); }),
                "a kept vertex is 0, outside 1..100");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::presented(draw, line, {3, 3}); }), "vertex 3 is kept twice");
}
