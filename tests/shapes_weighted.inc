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

namespace {

struct relaxing {
    std::vector<std::vector<std::pair<int, long long>>> out;
    long long scans = 0;

    explicit relaxing(int n, std::vector<eo::weighted_edge> const& edges, bool directed)
        : out(static_cast<std::size_t>(n) + 1) {
        for (eo::weighted_edge const& one : edges) {
            out[static_cast<std::size_t>(one.u)].push_back({one.v, one.w});
            if (!directed) out[static_cast<std::size_t>(one.v)].push_back({one.u, one.w});
        }
    }

    std::vector<long long> queued(int source) {
        std::vector<long long> best(out.size(), LLONG_MAX);
        std::vector<bool> waiting(out.size(), false);
        std::vector<int> line{source};
        best[static_cast<std::size_t>(source)] = 0;
        waiting[static_cast<std::size_t>(source)] = true;
        for (std::size_t at = 0; at < line.size(); at++) {
            std::size_t const here = static_cast<std::size_t>(line[at]);
            waiting[here] = false;
            for (auto const& [other, w] : out[here]) {
                scans++;
                std::size_t const there = static_cast<std::size_t>(other);
                if (best[here] + w >= best[there]) continue;
                best[there] = best[here] + w;
                if (!waiting[there]) {
                    waiting[there] = true;
                    line.push_back(other);
                }
            }
        }
        return best;
    }

    std::vector<long long> heaped(int source, bool skip_stale) {
        std::vector<long long> best(out.size(), LLONG_MAX);
        std::priority_queue<std::pair<long long, int>, std::vector<std::pair<long long, int>>, std::greater<>> heap;
        best[static_cast<std::size_t>(source)] = 0;
        heap.push({0, source});
        while (!heap.empty()) {
            auto const [distance, vertex] = heap.top();
            heap.pop();
            std::size_t const here = static_cast<std::size_t>(vertex);
            if (skip_stale && distance > best[here]) continue;
            for (auto const& [other, w] : out[here]) {
                scans++;
                std::size_t const there = static_cast<std::size_t>(other);
                if (best[here] + w >= best[there]) continue;
                best[there] = best[here] + w;
                heap.push({best[there], other});
            }
        }
        return best;
    }
};

}  // namespace

EO_TEST(anti_spfa_makes_a_queue_of_relaxations_crawl_where_dijkstra_does_not) {
    eo::rng draw(603);
    eo::weighted_graph const trap = eo::shapes::anti_spfa(draw, 3000, 1000000000);
    EO_CHECK_EQ(trap.n, 3000);
    EO_CHECK(!trap.directed);
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(3000, eo::detail::endpoints(trap.edges))));
    EO_CHECK(static_cast<bool>(eo::is_connected(3000, eo::detail::endpoints(trap.edges))));
    EO_CHECK(trap.edges.size() <= 2 * 3000);
    std::vector<eo::weighted_edge> const shown = eo::shapes::presented(draw, trap, {1});
    relaxing slow(3000, shown, false);
    relaxing fast(3000, shown, false);
    EO_CHECK(slow.queued(1) == fast.heaped(1, true));
    EO_CHECK(slow.scans > 200 * static_cast<long long>(shown.size()));
    EO_CHECK_EQ(fast.scans, 2 * static_cast<long long>(shown.size()));
    eo::weighted_graph const plain = eo::shapes::with_weights(draw, eo::shapes::connected_graph(draw, 3000, 5690), 1,
                                                              1000000000);
    relaxing typical(3000, eo::shapes::presented(draw, plain), false);
    (void)typical.queued(1);
    EO_CHECK(slow.scans > 20 * typical.scans);
    for (int const n : {1, 2, 9, 10, 11, 25}) {
        eo::weighted_graph const small = eo::shapes::anti_spfa(draw, n, 5);
        EO_CHECK(static_cast<bool>(eo::is_connected(n, eo::detail::endpoints(small.edges))));
        bool inside = true;
        for (eo::weighted_edge const& one : small.edges) inside = inside && one.w >= 1 && one.w <= 5;
        EO_CHECK(inside);
    }
    std::string const drawn = weighted_of(eo::shapes::anti_spfa(draw, 25, 100).edges);
    pin_bytes(drawn, 287, 0xb32a90a9fd0bbfc9ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::anti_spfa(draw, 10, 0); }),
                "anti_spfa draws weights from 1..high, and high is 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::anti_spfa(draw, 0, 10); }),
                "anti_spfa needs at least 1 vertices, not 0");
}

EO_TEST(anti_dijkstra_makes_a_heap_without_the_stale_check_rescan_the_hub) {
    eo::weighted_graph const trap = eo::shapes::anti_dijkstra(4000);
    EO_CHECK_EQ(trap.n, 4000);
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(4000, eo::detail::endpoints(trap.edges))));
    EO_CHECK(static_cast<bool>(eo::is_connected(4000, eo::detail::endpoints(trap.edges))));
    long long heaviest = 0;
    for (eo::weighted_edge const& one : trap.edges) heaviest = std::max(heaviest, one.w);
    EO_CHECK(heaviest <= 4000);
    EO_CHECK(trap.edges.size() <= 6000);
    eo::rng draw(604);
    std::vector<eo::weighted_edge> const shown = eo::shapes::presented(draw, trap, {1});
    relaxing careful(4000, shown, false);
    relaxing careless(4000, shown, false);
    EO_CHECK(careful.heaped(1, true) == careless.heaped(1, false));
    EO_CHECK_EQ(careful.scans, 2 * static_cast<long long>(shown.size()));
    EO_CHECK(careless.scans > 4000LL * 4000 / 8);
    relaxing both_ways(4000, trap.edges, false);
    relaxing away(4000, trap.edges, true);
    EO_CHECK(both_ways.heaped(1, true) == away.heaped(1, true));
    for (int const n : {1, 2, 3, 4, 5}) {
        eo::weighted_graph const small = eo::shapes::anti_dijkstra(n);
        EO_CHECK_EQ(small.n, n);
        EO_CHECK(static_cast<bool>(eo::is_connected(n, eo::detail::endpoints(small.edges))));
    }
    EO_CHECK_EQ(weighted_of(eo::shapes::anti_dijkstra(8).edges),
                std::string("1 2 1;2 5 5;1 3 2;3 5 3;1 4 3;4 5 1;5 6 1;5 7 1;5 8 1;"));
    EO_CHECK_IN(refusal([] { (void)eo::shapes::anti_dijkstra(0); }), "anti_dijkstra needs at least 1 vertices, not 0");
}

namespace {

struct blocking_flow {
    struct arc {
        int to;
        long long room;
    };
    std::vector<arc> arcs;
    std::vector<std::vector<std::size_t>> out;
    std::vector<int> level;
    std::vector<std::size_t> next;
    bool remembers;
    long long visits = 0;
    long long budget = 50000000;

    blocking_flow(int n, std::vector<eo::weighted_edge> const& edges, bool remember)
        : out(static_cast<std::size_t>(n) + 1), level(static_cast<std::size_t>(n) + 1),
          next(static_cast<std::size_t>(n) + 1), remembers(remember) {
        for (eo::weighted_edge const& one : edges) {
            out[static_cast<std::size_t>(one.u)].push_back(arcs.size());
            arcs.push_back({one.v, one.w});
            out[static_cast<std::size_t>(one.v)].push_back(arcs.size());
            arcs.push_back({one.u, 0});
        }
    }

    bool layered(int source, int sink) {
        std::fill(level.begin(), level.end(), -1);
        std::vector<int> waiting{source};
        level[static_cast<std::size_t>(source)] = 0;
        for (std::size_t at = 0; at < waiting.size(); at++)
            for (std::size_t const id : out[static_cast<std::size_t>(waiting[at])])
                if (arcs[id].room > 0 && level[static_cast<std::size_t>(arcs[id].to)] < 0) {
                    level[static_cast<std::size_t>(arcs[id].to)] = level[static_cast<std::size_t>(waiting[at])] + 1;
                    waiting.push_back(arcs[id].to);
                }
        return level[static_cast<std::size_t>(sink)] >= 0;
    }

    long long push(int here, int sink, long long most) {
        if (here == sink) return most;
        std::size_t const at = static_cast<std::size_t>(here);
        long long sent = 0;
        std::size_t scan = remembers ? next[at] : 0;
        for (; scan < out[at].size() && sent < most && visits < budget; scan++) {
            visits++;
            arc& one = arcs[out[at][scan]];
            if (one.room <= 0 || level[static_cast<std::size_t>(one.to)] != level[at] + 1) continue;
            long long const got = push(one.to, sink, std::min(most - sent, one.room));
            one.room -= got;
            arcs[out[at][scan] ^ 1].room += got;
            sent += got;
            if (sent == most) break;
        }
        if (remembers) next[at] = scan;
        return sent;
    }

    long long run(int source, int sink) {
        long long total = 0;
        while (visits < budget && layered(source, sink)) {
            std::fill(next.begin(), next.end(), 0);
            total += push(source, sink, LLONG_MAX);
        }
        return total;
    }
};

}  // namespace

EO_TEST(a_layered_network_sends_a_dinic_without_its_arc_pointers_round_and_round) {
    eo::rng draw(605);
    eo::weighted_graph const net = eo::shapes::layered_network(draw, 5, 10, 1000);
    EO_CHECK(net.directed);
    EO_CHECK_EQ(net.n, 52);
    EO_CHECK_EQ(net.edges.size(), static_cast<std::size_t>(10 + 4 * 100 + 10));
    bool positive = true;
    for (eo::weighted_edge const& one : net.edges) positive = positive && one.w >= 1 && one.w <= 1000;
    EO_CHECK(positive);
    std::vector<eo::weighted_edge> const shown = eo::shapes::presented(draw, net, {1, 52});
    blocking_flow careful(52, shown, true);
    blocking_flow forgetful(52, shown, false);
    long long const flow = careful.run(1, 52);
    EO_CHECK_EQ(forgetful.run(1, 52), flow);
    EO_CHECK(careful.visits < 10 * static_cast<long long>(shown.size()));
    EO_CHECK(forgetful.visits > 500 * careful.visits);
    eo::weighted_graph const single = eo::shapes::layered_network(draw, 1, 1, 5);
    EO_CHECK_EQ(single.n, 3);
    EO_CHECK_EQ(single.edges.size(), static_cast<std::size_t>(2));
    std::string const drawn = weighted_of(eo::shapes::layered_network(draw, 3, 3, 20).edges);
    pin_bytes(drawn, 163, 0x34d8657c25398189ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::layered_network(draw, 0, 3, 20); }),
                "a layered network has at least 1 layer of at least 1 vertex, not 0 of 3");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::layered_network(draw, 3, 3, 0); }),
                "layered_network draws capacities from 1..high, and high is 0");
}
