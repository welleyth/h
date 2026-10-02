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

namespace {

int cycles_if_cactus(eo::graph const& made) {
    std::vector<std::vector<std::pair<int, std::size_t>>> near(static_cast<std::size_t>(made.n) + 1);
    for (std::size_t at = 0; at < made.edges.size(); at++) {
        near[static_cast<std::size_t>(made.edges[at].u)].push_back({made.edges[at].v, at});
        near[static_cast<std::size_t>(made.edges[at].v)].push_back({made.edges[at].u, at});
    }
    std::vector<int> depth(static_cast<std::size_t>(made.n) + 1, -1);
    std::vector<int> parent(static_cast<std::size_t>(made.n) + 1, 0);
    std::vector<bool> tree_edge(made.edges.size(), false);
    std::vector<int> order{1};
    depth[1] = 0;
    for (std::size_t at = 0; at < order.size(); at++)
        for (auto const& [other, id] : near[static_cast<std::size_t>(order[at])])
            if (depth[static_cast<std::size_t>(other)] < 0) {
                depth[static_cast<std::size_t>(other)] = depth[static_cast<std::size_t>(order[at])] + 1;
                parent[static_cast<std::size_t>(other)] = order[at];
                tree_edge[id] = true;
                order.push_back(other);
            }
    if (static_cast<int>(order.size()) != made.n) return -1;
    std::vector<bool> used(static_cast<std::size_t>(made.n) + 1, false);
    int cycles = 0;
    for (std::size_t id = 0; id < made.edges.size(); id++) {
        if (tree_edge[id]) continue;
        int u = made.edges[id].u;
        int v = made.edges[id].v;
        cycles++;
        while (u != v) {
            if (depth[static_cast<std::size_t>(u)] < depth[static_cast<std::size_t>(v)]) std::swap(u, v);
            if (used[static_cast<std::size_t>(u)]) return -1;
            used[static_cast<std::size_t>(u)] = true;
            u = parent[static_cast<std::size_t>(u)];
        }
    }
    return cycles;
}

}  // namespace

EO_TEST(a_cactus_puts_every_edge_on_at_most_one_cycle) {
    eo::rng draw(503);
    for (int const longest : {2, 3, 4, 10, 1000}) {
        eo::graph const made = eo::shapes::cactus(draw, 2000, longest);
        EO_CHECK(static_cast<bool>(eo::is_simple_graph(made.n, made.edges)));
        int const cycles = cycles_if_cactus(made);
        EO_CHECK_EQ(cycles, static_cast<int>(made.edges.size()) - made.n + 1);
        if (longest == 2) EO_CHECK_EQ(cycles, 0);
        if (longest > 2) EO_CHECK(cycles > 0);
    }
    EO_CHECK_EQ(cycles_if_cactus(eo::shapes::cycle_with_chords(draw, 10, 2)), -1);
    eo::graph const triangles = eo::shapes::cactus(draw, 3001, 3);
    EO_CHECK(highest_degree(triangles) >= 6);
    EO_CHECK(eo::shapes::cactus(draw, 1, 5).edges.empty());
    std::string const drawn = edges_of(eo::shapes::cactus(draw, 30, 5).edges);
    pin_bytes(drawn, 198, 0x4cc38c47a32307d1ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cactus(draw, 10, 1); }),
                "cactus needs a longest cycle of at least 2, where 2 builds bridges alone, not 1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cactus(draw, 0, 3); }), "a cactus needs at least 1 vertices, not 0");
}

namespace {

struct separators {
    int bridges = 0;
    int cuts = 0;
};

separators separators_of(eo::graph const& made) {
    std::vector<std::vector<std::pair<int, std::size_t>>> near(static_cast<std::size_t>(made.n) + 1);
    for (std::size_t at = 0; at < made.edges.size(); at++) {
        near[static_cast<std::size_t>(made.edges[at].u)].push_back({made.edges[at].v, at});
        near[static_cast<std::size_t>(made.edges[at].v)].push_back({made.edges[at].u, at});
    }
    std::vector<int> entered(static_cast<std::size_t>(made.n) + 1, 0);
    std::vector<int> low(static_cast<std::size_t>(made.n) + 1, 0);
    std::vector<bool> cut(static_cast<std::size_t>(made.n) + 1, false);
    separators found;
    int clock = 0;
    for (int root = 1; root <= made.n; root++) {
        if (entered[static_cast<std::size_t>(root)] != 0) continue;
        struct frame {
            int vertex;
            std::size_t via;
            std::size_t next;
        };
        std::vector<frame> stack{{root, made.edges.size(), 0}};
        entered[static_cast<std::size_t>(root)] = low[static_cast<std::size_t>(root)] = ++clock;
        int root_children = 0;
        while (!stack.empty()) {
            frame& top = stack.back();
            std::size_t const here = static_cast<std::size_t>(top.vertex);
            if (top.next < near[here].size()) {
                auto const [other, id] = near[here][top.next++];
                if (id == top.via) continue;
                std::size_t const there = static_cast<std::size_t>(other);
                if (entered[there] != 0) {
                    low[here] = std::min(low[here], entered[there]);
                } else {
                    entered[there] = low[there] = ++clock;
                    if (top.vertex == root) root_children++;
                    stack.push_back({other, id, 0});
                }
                continue;
            }
            stack.pop_back();
            if (stack.empty()) break;
            std::size_t const up = static_cast<std::size_t>(stack.back().vertex);
            low[up] = std::min(low[up], low[here]);
            if (low[here] > entered[up]) found.bridges++;
            if (stack.back().vertex != root && low[here] >= entered[up]) cut[up] = true;
        }
        if (root_children >= 2) cut[static_cast<std::size_t>(root)] = true;
    }
    found.cuts = static_cast<int>(std::count(cut.begin(), cut.end(), true));
    return found;
}

}  // namespace

EO_TEST(a_graph_has_exactly_the_bridges_asked_for) {
    eo::rng draw(504);
    for (int n = 1; n <= 12; n++)
        for (int bridges = 0; bridges <= n - 1; bridges++) {
            if (n >= 2 && bridges == n - 2) continue;
            eo::graph const made = eo::shapes::with_bridges(draw, n, bridges);
            EO_CHECK(static_cast<bool>(eo::is_simple_graph(made.n, made.edges)));
            EO_CHECK(static_cast<bool>(eo::is_connected(made.n, made.edges)));
            EO_CHECK_EQ(separators_of(made).bridges, bridges);
        }
    for (int const bridges : {0, 1, 500, 50000, 99997, 99999}) {
        eo::graph const made = eo::shapes::with_bridges(draw, 100000, bridges);
        EO_CHECK(static_cast<bool>(eo::is_connected(made.n, made.edges)));
        EO_CHECK_EQ(separators_of(made).bridges, bridges);
    }
    std::string const drawn = edges_of(eo::shapes::with_bridges(draw, 30, 7).edges);
    pin_bytes(drawn, 192, 0x429d1d2117b951aeull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_bridges(draw, 10, 8); }),
                "a connected graph on 10 vertices never has exactly 8 bridges; ask for 9, or for at most 7");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_bridges(draw, 2, 0); }),
                "a connected graph on 2 vertices never has exactly 0 bridges; ask for 1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_bridges(draw, 10, 10); }),
                "a connected graph on 10 vertices has 0..9 bridges, not 10");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_bridges(draw, 0, 0); }),
                "a graph with bridges needs at least 1 vertices, not 0");
}

EO_TEST(a_graph_has_exactly_the_cut_vertices_asked_for) {
    eo::rng draw(505);
    for (int n = 1; n <= 12; n++)
        for (int cuts = 0; cuts <= std::max(0, n - 2); cuts++)
            for (int round = 0; round < 3; round++) {
                eo::graph const made = eo::shapes::with_cut_vertices(draw, n, cuts);
                EO_CHECK(static_cast<bool>(eo::is_simple_graph(made.n, made.edges)));
                EO_CHECK(static_cast<bool>(eo::is_connected(made.n, made.edges)));
                EO_CHECK_EQ(separators_of(made).cuts, cuts);
            }
    for (int const cuts : {0, 1, 300, 30000, 99998}) {
        eo::graph const made = eo::shapes::with_cut_vertices(draw, 100000, cuts);
        EO_CHECK(static_cast<bool>(eo::is_connected(made.n, made.edges)));
        EO_CHECK_EQ(separators_of(made).cuts, cuts);
    }
    std::string const drawn = edges_of(eo::shapes::with_cut_vertices(draw, 30, 6).edges);
    pin_bytes(drawn, 179, 0xd2372dfc74bc8031ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_cut_vertices(draw, 10, 9); }),
                "a connected graph on 10 vertices has 0..8 cut vertices, not 9");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_cut_vertices(draw, 1, 1); }),
                "a connected graph on 1 vertices has 0..0 cut vertices, not 1");
}
