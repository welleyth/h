EO_TEST(a_regular_graph_gives_every_vertex_the_same_degree) {
    eo::rng draw(501);
    struct asked {
        int n;
        int k;
    };
    for (asked const one : {asked{10, 3}, asked{11, 4}, asked{1000, 3}, asked{1000, 10}, asked{30, 28}, asked{31, 30},
                            asked{20, 0}, asked{100000, 4}, asked{2, 1}, asked{1, 0}}) {
        eo::graph const made = eo::shapes::regular_graph(draw, one.n, one.k);
        EO_CHECK_EQ(made.n, one.n);
        EO_CHECK_EQ(static_cast<long long>(made.edges.size()), static_cast<long long>(one.n) * one.k / 2);
        EO_CHECK(static_cast<bool>(eo::is_simple_graph(made.n, made.edges)));
        std::vector<int> const degree = degrees_of(made);
        EO_CHECK(std::all_of(degree.begin() + 1, degree.end(), [&](int d) { return d == one.k; }));
    }
    eo::graph const mixed = eo::shapes::regular_graph(draw, 1000, 4);
    long long neighbours_in_order = 0;
    for (eo::edge const& one : mixed.edges)
        if (std::abs(one.u - one.v) <= 2 || std::abs(one.u - one.v) >= 998) neighbours_in_order++;
    EO_CHECK(neighbours_in_order < 50);
    EO_CHECK(static_cast<bool>(eo::is_connected(1000, mixed.edges)));
    std::string drawn = edges_of(eo::shapes::regular_graph(draw, 12, 3).edges);
    drawn += edges_of(eo::shapes::regular_graph(draw, 9, 6).edges);
    pin_bytes(drawn, 189, 0x045a4db9a0ead3abull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::regular_graph(draw, 7, 3); }),
                "a 3-regular graph on 7 vertices has 21/2 edges; n times k must be even");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::regular_graph(draw, 7, 7); }),
                "a regular graph on 7 vertices has degree 0..6, not 7");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::regular_graph(draw, 0, 0); }),
                "a regular graph needs at least 1 vertices, not 0");
}

EO_TEST(the_pair_set_forgets_what_it_erases_and_nothing_else) {
    eo::rng draw(502);
    eo::shapes::detail::pair_set kept(3000);
    std::vector<std::pair<int, int>> pairs;
    for (int at = 0; at < 3000; at++) {
        int const u = static_cast<int>(draw.uniform(1, 200));
        int const v = static_cast<int>(draw.uniform(1, 200));
        if (kept.insert(u, v)) pairs.push_back({u, v});
    }
    for (int round = 0; round < 20; round++) {
        for (std::size_t at = 0; at < pairs.size(); at += 2) kept.erase(pairs[at].first, pairs[at].second);
        bool right = true;
        for (std::size_t at = 0; at < pairs.size(); at++)
            right = right && kept.contains(pairs[at].first, pairs[at].second) == (at % 2 == 1);
        EO_CHECK(right);
        for (std::size_t at = 0; at < pairs.size(); at += 2)
            right = right && kept.insert(pairs[at].first, pairs[at].second);
        EO_CHECK(right);
    }
}
