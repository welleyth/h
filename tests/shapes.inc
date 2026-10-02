namespace {

template <class Body>
std::string refusal(Body&& body) {
    eot::outcome const result = eot::run_until_stop(std::forward<Body>(body));
    return result.stopped ? result.text : std::string("(nothing was refused)");
}

bool holds_a_tree(eo::graph const& made) {
    return static_cast<bool>(eo::is_tree(made.n, made.edges));
}

int longest_walk(eo::graph const& made) {
    std::vector<std::vector<int>> near(static_cast<std::size_t>(made.n) + 1);
    for (eo::edge const& one : made.edges) {
        near[static_cast<std::size_t>(one.u)].push_back(one.v);
        near[static_cast<std::size_t>(one.v)].push_back(one.u);
    }
    std::vector<int> depth(static_cast<std::size_t>(made.n) + 1, 0);
    std::vector<int> waiting{1};
    std::vector<bool> seen(static_cast<std::size_t>(made.n) + 1, false);
    seen[1] = true;
    int deepest = 1;
    while (!waiting.empty()) {
        int const here = waiting.back();
        waiting.pop_back();
        deepest = std::max(deepest, depth[static_cast<std::size_t>(here)] + 1);
        for (int const other : near[static_cast<std::size_t>(here)])
            if (!seen[static_cast<std::size_t>(other)]) {
                seen[static_cast<std::size_t>(other)] = true;
                depth[static_cast<std::size_t>(other)] = depth[static_cast<std::size_t>(here)] + 1;
                waiting.push_back(other);
            }
    }
    return deepest;
}

int highest_degree(eo::graph const& made) {
    std::vector<int> count(static_cast<std::size_t>(made.n) + 1, 0);
    for (eo::edge const& one : made.edges) {
        count[static_cast<std::size_t>(one.u)]++;
        count[static_cast<std::size_t>(one.v)]++;
    }
    return *std::max_element(count.begin() + 1, count.end());
}

bool stays_acyclic(int n, std::vector<eo::edge> const& arrows) {
    std::vector<std::vector<int>> out(static_cast<std::size_t>(n) + 1);
    std::vector<int> waiting_on(static_cast<std::size_t>(n) + 1, 0);
    for (eo::edge const& one : arrows) {
        out[static_cast<std::size_t>(one.u)].push_back(one.v);
        waiting_on[static_cast<std::size_t>(one.v)]++;
    }
    std::vector<int> ready;
    for (int vertex = 1; vertex <= n; vertex++)
        if (waiting_on[static_cast<std::size_t>(vertex)] == 0) ready.push_back(vertex);
    int placed = 0;
    while (!ready.empty()) {
        int const here = ready.back();
        ready.pop_back();
        placed++;
        for (int const other : out[static_cast<std::size_t>(here)])
            if (--waiting_on[static_cast<std::size_t>(other)] == 0) ready.push_back(other);
    }
    return placed == n;
}

long long turn_of(eo::point const& a, eo::point const& b, eo::point const& c) {
    long long const cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    return cross > 0 ? 1 : (cross < 0 ? -1 : 0);
}

}  // namespace

EO_TEST(a_random_tree_is_a_tree_and_stays_bushy) {
    eo::rng draw(1);
    eo::graph const made = eo::shapes::random_tree(draw, 500);
    EO_CHECK(holds_a_tree(made));
    EO_CHECK(longest_walk(made) < 60);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::random_tree(draw, 0); }),
                "a tree needs at least 1 vertices, not 0");
}

EO_TEST(a_deep_tree_leans_further_than_a_random_one) {
    eo::rng draw(2);
    eo::graph const bushy = eo::shapes::deep_tree(draw, 2000, 0);
    eo::graph const leaning = eo::shapes::deep_tree(draw, 2000, 50);
    EO_CHECK(holds_a_tree(bushy));
    EO_CHECK(holds_a_tree(leaning));
    EO_CHECK(longest_walk(leaning) > longest_walk(bushy) * 3);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::deep_tree(draw, 10, -1); }), "is below 0");
}

EO_TEST(a_uniform_tree_is_a_tree_at_every_size) {
    eo::rng draw(3);
    for (int n = 1; n <= 60; n++) {
        eo::graph const made = eo::shapes::uniform_tree(draw, n);
        EO_CHECK(holds_a_tree(made));
    }
    eo::graph const alone = eo::shapes::uniform_tree(draw, 1);
    EO_CHECK_EQ(alone.edges.size(), static_cast<std::size_t>(0));
}

EO_TEST(a_path_and_a_star_are_the_two_extremes) {
    eo::graph const line = eo::shapes::path(400);
    eo::graph const hub = eo::shapes::star(400);
    EO_CHECK(holds_a_tree(line));
    EO_CHECK(holds_a_tree(hub));
    EO_CHECK_EQ(longest_walk(line), 400);
    EO_CHECK_EQ(highest_degree(hub), 399);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::path(0); }), "a path needs at least 1 vertices");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::star(0); }), "a star needs at least 1 vertices");
}

EO_TEST(a_caterpillar_and_a_broom_have_a_spine_and_a_head) {
    eo::rng draw(4);
    eo::graph const crawler = eo::shapes::caterpillar(draw, 100);
    EO_CHECK(holds_a_tree(crawler));
    EO_CHECK(longest_walk(crawler) >= 50);
    eo::graph const narrow = eo::shapes::caterpillar(draw, 100, 90);
    EO_CHECK(holds_a_tree(narrow));
    EO_CHECK(longest_walk(narrow) >= 90);
    eo::graph const sweeper = eo::shapes::broom(100);
    EO_CHECK(holds_a_tree(sweeper));
    EO_CHECK_EQ(highest_degree(sweeper), 51);
    eo::graph const handled = eo::shapes::broom(100, 10);
    EO_CHECK_EQ(highest_degree(handled), 91);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::caterpillar(draw, 10, 11); }), "a spine of 11 does not fit");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::broom(10, 11); }), "a handle of 11 does not fit");
}

EO_TEST(a_kary_tree_branches_as_asked) {
    eo::graph const two = eo::shapes::binary_tree(63);
    EO_CHECK(holds_a_tree(two));
    EO_CHECK_EQ(longest_walk(two), 6);
    eo::graph const three = eo::shapes::kary_tree(40, 3);
    EO_CHECK(holds_a_tree(three));
    EO_CHECK_EQ(highest_degree(three), 4);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::kary_tree(10, 0); }), "k is 0");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::kary_tree(0, 2); }), "a k-ary tree needs at least 1 vertices");
}

EO_TEST(a_dumbbell_has_two_hubs_and_a_spider_has_legs) {
    eo::graph const weights = eo::shapes::dumbbell(100);
    EO_CHECK(holds_a_tree(weights));
    EO_CHECK_EQ(highest_degree(weights), 50);
    eo::graph const alone = eo::shapes::dumbbell(1);
    EO_CHECK_EQ(alone.edges.size(), static_cast<std::size_t>(0));
    eo::graph const legs = eo::shapes::spider(31, 3);
    EO_CHECK(holds_a_tree(legs));
    EO_CHECK_EQ(highest_degree(legs), 3);
    EO_CHECK_EQ(longest_walk(legs), 11);
    eo::graph const stubby = eo::shapes::spider(5, 10);
    EO_CHECK(holds_a_tree(stubby));
    EO_CHECK_IN(refusal([] { (void)eo::shapes::spider(10, 0); }), "at least one leg, not 0");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::spider(0, 2); }), "a spider needs at least 1 vertices");
}

EO_TEST(the_tree_dispatcher_knows_every_name_it_offers) {
    eo::rng draw(5);
    for (char const* name :
         {"random", "uniform", "path", "star", "caterpillar", "broom", "binary", "dumbbell"}) {
        eo::graph const made = eo::shapes::tree(draw, 30, name);
        EO_CHECK(holds_a_tree(made));
    }
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree(draw, 30, "bamboo"); }),
                "\"bamboo\" is not a tree shape");
}

EO_TEST(presenting_relabels_shuffles_and_turns_the_edges) {
    eo::rng draw(6);
    eo::graph const line = eo::shapes::path(200);
    std::vector<eo::edge> const shown = eo::shapes::presented(draw, line);
    EO_CHECK(static_cast<bool>(eo::is_tree(200, shown)));
    bool in_order = true;
    bool ascending = true;
    for (std::size_t at = 0; at < shown.size(); at++) {
        if (shown[at].u > shown[at].v) ascending = false;
        if (at > 0 && shown[at - 1].v != shown[at].u) in_order = false;
    }
    EO_CHECK(!in_order);
    EO_CHECK(!ascending);
    eo::graph const arrows = eo::shapes::dag(draw, 40, 100);
    std::vector<eo::edge> const kept = eo::shapes::presented(draw, arrows);
    EO_CHECK_EQ(kept.size(), static_cast<std::size_t>(100));
    EO_CHECK(stays_acyclic(40, kept));
    std::vector<eo::edge> turned = kept;
    for (std::size_t at = 0; at < turned.size(); at += 2) std::swap(turned[at].u, turned[at].v);
    EO_CHECK(!stays_acyclic(40, turned));
}

EO_TEST(a_parent_array_hides_the_order_it_was_built_in) {
    eo::rng draw(7);
    eo::graph const made = eo::shapes::random_tree(draw, 200);
    std::vector<int> const parents = eo::shapes::parent_array(draw, made);
    EO_CHECK_EQ(parents.size(), static_cast<std::size_t>(199));
    bool every_parent_is_earlier = true;
    for (std::size_t at = 0; at < parents.size(); at++)
        if (parents[at] >= static_cast<int>(at) + 2) every_parent_is_earlier = false;
    EO_CHECK(every_parent_is_earlier);
    std::vector<eo::edge> back;
    for (std::size_t at = 0; at < parents.size(); at++)
        back.push_back(eo::edge{parents[at], static_cast<int>(at) + 2});
    EO_CHECK(static_cast<bool>(eo::is_tree(200, back)));
    EO_CHECK_EQ(eo::shapes::parent_array(draw, eo::shapes::path(1)).size(), static_cast<std::size_t>(0));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::parent_array(draw, made, 0); }), "the root is 0, outside");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::parent_array(draw, eo::shapes::many_components(draw, 6, 3)); }),
                "a parent array needs a tree");
}

EO_TEST(a_connected_graph_has_exactly_the_edges_asked_for) {
    eo::rng draw(8);
    eo::graph const sparse = eo::shapes::connected_graph(draw, 300, 800);
    EO_CHECK_EQ(sparse.edges.size(), static_cast<std::size_t>(800));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(300, sparse.edges)));
    EO_CHECK(static_cast<bool>(eo::is_connected(300, sparse.edges)));
    eo::graph const dense = eo::shapes::connected_graph(draw, 30, 400);
    EO_CHECK_EQ(dense.edges.size(), static_cast<std::size_t>(400));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(30, dense.edges)));
    EO_CHECK(static_cast<bool>(eo::is_connected(30, dense.edges)));
    eo::graph const full = eo::shapes::connected_graph(draw, 12, 66);
    EO_CHECK_EQ(full.edges.size(), static_cast<std::size_t>(66));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::connected_graph(draw, 10, 8); }),
                "a connected graph on 10 vertices has 9..45 edges, not 8");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::connected_graph(draw, 10, 46); }), "not 46");
}

EO_TEST(a_sparse_graph_may_be_disconnected) {
    eo::rng draw(9);
    eo::graph const made = eo::shapes::sparse_graph(draw, 200, 100);
    EO_CHECK_EQ(made.edges.size(), static_cast<std::size_t>(100));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(200, made.edges)));
    EO_CHECK(!static_cast<bool>(eo::is_connected(200, made.edges)));
    eo::graph const empty = eo::shapes::sparse_graph(draw, 5, 0);
    EO_CHECK_EQ(empty.edges.size(), static_cast<std::size_t>(0));
}

EO_TEST(a_complete_graph_and_a_cycle_are_the_density_extremes) {
    eo::graph const full = eo::shapes::complete_graph(20);
    EO_CHECK_EQ(full.edges.size(), static_cast<std::size_t>(190));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(20, full.edges)));
    eo::graph const ring = eo::shapes::cycle(20);
    EO_CHECK_EQ(ring.edges.size(), static_cast<std::size_t>(20));
    EO_CHECK_EQ(highest_degree(ring), 2);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::complete_graph(0); }), "a complete graph needs at least 1");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::cycle(2); }), "a cycle needs at least 3 vertices, not 2");
}

EO_TEST(a_cycle_with_chords_keeps_the_ring_and_adds_to_it) {
    eo::rng draw(10);
    eo::graph const made = eo::shapes::cycle_with_chords(draw, 50, 30);
    EO_CHECK_EQ(made.edges.size(), static_cast<std::size_t>(80));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(50, made.edges)));
    EO_CHECK(static_cast<bool>(eo::is_connected(50, made.edges)));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cycle_with_chords(draw, 5, 20); }), "a cycle with chords");
}

EO_TEST(a_grid_is_planar_and_the_right_size) {
    eo::graph const made = eo::shapes::grid(4, 5);
    EO_CHECK_EQ(made.n, 20);
    EO_CHECK_EQ(made.edges.size(), static_cast<std::size_t>(4 * 4 + 5 * 3));
    EO_CHECK(static_cast<bool>(eo::is_connected(20, made.edges)));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(20, made.edges)));
    EO_CHECK_IN(refusal([] { (void)eo::shapes::grid(0, 5); }), "a grid is at least 1 by 1, not 0 by 5");
}

EO_TEST(a_bipartite_graph_only_crosses_between_its_sides) {
    eo::rng draw(11);
    eo::graph const sparse = eo::shapes::bipartite_graph(draw, 50, 60, 200);
    EO_CHECK_EQ(sparse.edges.size(), static_cast<std::size_t>(200));
    eo::graph const dense = eo::shapes::bipartite_graph(draw, 10, 10, 90);
    EO_CHECK_EQ(dense.edges.size(), static_cast<std::size_t>(90));
    bool crosses = true;
    for (eo::edge const& one : dense.edges)
        if (one.u > 10 || one.v <= 10) crosses = false;
    EO_CHECK(crosses);
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(20, dense.edges)));
    eo::graph const full = eo::shapes::complete_bipartite(6, 7);
    EO_CHECK_EQ(full.edges.size(), static_cast<std::size_t>(42));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::bipartite_graph(draw, 0, 5, 1); }), "sides of at least 1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::bipartite_graph(draw, 5, 5, 26); }), "holds 0..25 edges");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::complete_bipartite(5, 0); }), "sides of at least 1");
}

EO_TEST(many_components_breaks_a_graph_into_pieces) {
    eo::rng draw(12);
    eo::graph const made = eo::shapes::many_components(draw, 100, 7);
    EO_CHECK_EQ(made.edges.size(), static_cast<std::size_t>(100 - 7));
    EO_CHECK(static_cast<bool>(eo::is_simple_graph(100, made.edges)));
    EO_CHECK(!static_cast<bool>(eo::is_connected(100, made.edges)));
    eo::graph const whole = eo::shapes::many_components(draw, 10, 1);
    EO_CHECK(static_cast<bool>(eo::is_connected(10, whole.edges)));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::many_components(draw, 10, 11) ; }), "fall into 1..10 pieces");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::many_components(draw, 0, 1); }), "a graph needs at least 1");
}

EO_TEST(a_dag_hides_its_topological_order) {
    eo::rng draw(13);
    eo::graph const sparse = eo::shapes::dag(draw, 200, 500);
    EO_CHECK_EQ(sparse.edges.size(), static_cast<std::size_t>(500));
    EO_CHECK(sparse.directed);
    bool always_forward = true;
    for (eo::edge const& one : sparse.edges)
        if (one.u > one.v) always_forward = false;
    EO_CHECK(!always_forward);
    eo::graph const dense = eo::shapes::dag(draw, 20, 150);
    EO_CHECK_EQ(dense.edges.size(), static_cast<std::size_t>(150));
    std::vector<std::pair<int, int>> seen;
    for (eo::edge const& one : dense.edges) seen.push_back({std::min(one.u, one.v), std::max(one.u, one.v)});
    std::sort(seen.begin(), seen.end());
    EO_CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
}

EO_TEST(a_functional_graph_has_one_arrow_out_of_every_vertex) {
    eo::rng draw(14);
    std::vector<int> const ring = eo::shapes::functional(draw, 50, "cycle");
    EO_CHECK_EQ(ring.size(), static_cast<std::size_t>(50));
    EO_CHECK(static_cast<bool>(eo::is_permutation(ring)));
    std::vector<int> const rho = eo::shapes::functional(draw, 50, "rho");
    EO_CHECK_EQ(rho.size(), static_cast<std::size_t>(50));
    EO_CHECK(!static_cast<bool>(eo::is_permutation(rho)));
    std::vector<int> const alone = eo::shapes::functional(draw, 1, "rho");
    EO_CHECK_EQ(alone[0], 1);
    std::vector<int> const loops = eo::shapes::functional(draw, 10, "self");
    for (int at = 0; at < 10; at++) EO_CHECK_EQ(loops[static_cast<std::size_t>(at)], at + 1);
    std::vector<int> const anywhere = eo::shapes::functional(draw, 50, "random");
    EO_CHECK_EQ(anywhere.size(), static_cast<std::size_t>(50));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::functional(draw, 5, "tree"); }),
                "\"tree\" is not a functional shape");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::functional(draw, 0, "cycle"); }),
                "a functional graph needs at least 1");
}

EO_TEST(sequences_come_in_the_shapes_that_break_solutions) {
    eo::rng draw(15);
    std::vector<long long> const flat = eo::shapes::equal_values(10, 7);
    EO_CHECK_EQ(flat.size(), static_cast<std::size_t>(10));
    EO_CHECK_EQ(flat[9], 7LL);
    std::vector<long long> const few = eo::shapes::few_distinct(draw, 200, 3, 1, 1000000000);
    std::vector<long long> kinds = few;
    std::sort(kinds.begin(), kinds.end());
    kinds.erase(std::unique(kinds.begin(), kinds.end()), kinds.end());
    EO_CHECK_EQ(kinds.size(), static_cast<std::size_t>(3));
    std::vector<long long> const narrow = eo::shapes::few_distinct(draw, 20, 100, 1, 2);
    std::vector<long long> narrow_kinds = narrow;
    std::sort(narrow_kinds.begin(), narrow_kinds.end());
    narrow_kinds.erase(std::unique(narrow_kinds.begin(), narrow_kinds.end()), narrow_kinds.end());
    EO_CHECK(narrow_kinds.size() <= 2);
    std::vector<long long> const runs = eo::shapes::plateaus(draw, 100, 4, 1, 1000);
    EO_CHECK_EQ(runs.size(), static_cast<std::size_t>(100));
    std::size_t breaks = 0;
    for (std::size_t at = 1; at < runs.size(); at++)
        if (runs[at] != runs[at - 1]) breaks++;
    EO_CHECK(breaks <= 3);
    std::vector<long long> const almost = eo::shapes::nearly_sorted(draw, 100, 1, 1000000, 2);
    EO_CHECK(!static_cast<bool>(eo::is_sorted(almost)));
    std::vector<long long> const still = eo::shapes::nearly_sorted(draw, 100, 1, 1000000, 0);
    EO_CHECK(static_cast<bool>(eo::is_sorted(still)));
    std::vector<long long> const zigzag = eo::shapes::alternating(draw, 100, 1, 1000);
    EO_CHECK(zigzag[0] <= 500 && zigzag[1] >= 500);
    std::vector<long long> const killers = eo::shapes::hash_collisions(5);
    EO_CHECK_EQ(killers[0], 107897LL);
    EO_CHECK_EQ(killers[4], 5LL * 107897LL);
}

EO_TEST(a_sequence_that_cannot_be_built_is_a_library_error) {
    eo::rng draw(16);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::few_distinct(draw, -1, 2, 1, 9); }), "at least no elements");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::few_distinct(draw, 5, 2, 9, 1); }), "which is empty");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::few_distinct(draw, 5, 0, 1, 9); }), "at least one kind, not 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::plateaus(draw, 5, 6, 1, 9); }), "fall into 1..5 runs, not 6");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::nearly_sorted(draw, 5, 1, 9, -1); }), "at least no swaps");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::hash_collisions(-1); }), "at least no values");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::hash_collisions(5, 0); }), "needs a bucket count");
}

EO_TEST(strings_come_in_the_shapes_that_break_string_algorithms) {
    eo::rng draw(17);
    EO_CHECK_EQ(eo::shapes::repeated('a', 5), std::string("aaaaa"));
    EO_CHECK_EQ(eo::shapes::periodic("abc", 8), std::string("abcabcab"));
    EO_CHECK_EQ(eo::shapes::fibonacci_word(13), std::string("abaababaabaab"));
    EO_CHECK_EQ(eo::shapes::thue_morse(8), std::string("abbabaab"));
    std::string const mirror = eo::shapes::palindrome(draw, 21, eo::charset("ab"));
    EO_CHECK_EQ(mirror.size(), static_cast<std::size_t>(21));
    EO_CHECK_EQ(mirror, std::string(mirror.rbegin(), mirror.rend()));
    std::string const nearly = eo::shapes::near_periodic(draw, "ab", 20, eo::charset("ab"));
    EO_CHECK_EQ(nearly.size(), static_cast<std::size_t>(20));
    EO_CHECK(nearly != eo::shapes::periodic("ab", 20));
    std::size_t different = 0;
    std::string const exact = eo::shapes::periodic("ab", 20);
    for (std::size_t at = 0; at < 20; at++)
        if (nearly[at] != exact[at]) different++;
    EO_CHECK_EQ(different, static_cast<std::size_t>(1));
    EO_CHECK_EQ(eo::shapes::near_periodic(draw, "a", 0, eo::charset("ab")), std::string(""));
}

EO_TEST(a_string_that_cannot_be_built_is_a_library_error) {
    eo::rng draw(18);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::repeated('a', -1); }), "is at least empty");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::periodic("", 5); }), "needs a unit to repeat");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::fibonacci_word(-1); }), "a Fibonacci word is at least empty");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::thue_morse(-1); }), "a Thue-Morse word is at least empty");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::palindrome(draw, -1, eo::charset("ab")); }),
                "a palindrome is at least empty");
}

EO_TEST(points_come_in_the_shapes_that_break_geometry) {
    eo::rng draw(19);
    std::vector<eo::point> const cloud = eo::shapes::scattered(draw, 100, 1000000000);
    EO_CHECK_EQ(cloud.size(), static_cast<std::size_t>(100));
    bool all_on_one_line = true;
    bool inside = true;
    bool ever_fell = false;
    bool ever_rose = false;
    for (int round = 0; round < 20; round++) {
        std::vector<eo::point> const line = eo::shapes::collinear(draw, 60, 1000000000);
        EO_CHECK_EQ(line.size(), static_cast<std::size_t>(60));
        for (std::size_t at = 2; at < line.size(); at++)
            if (turn_of(line[0], line[1], line[at]) != 0) all_on_one_line = false;
        for (eo::point const& one : line)
            if (one.x < -1000000000 || one.x > 1000000000 || one.y < -1000000000 || one.y > 1000000000)
                inside = false;
        std::vector<eo::point> sorted = line;
        std::sort(sorted.begin(), sorted.end(),
                  [](eo::point const& a, eo::point const& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
        if (sorted.front().y > sorted.back().y) ever_fell = true;
        if (sorted.front().y < sorted.back().y) ever_rose = true;
    }
    EO_CHECK(all_on_one_line);
    EO_CHECK(inside);
    EO_CHECK(ever_fell);
    EO_CHECK(ever_rose);
    EO_CHECK_EQ(eo::shapes::collinear(draw, 1, 100).size(), static_cast<std::size_t>(1));
}

EO_TEST(points_in_convex_position_never_turn_back) {
    eo::rng draw(20);
    for (int round = 0; round < 20; round++) {
        std::vector<eo::point> const hull = eo::shapes::convex_position(draw, 30, 100000);
        EO_CHECK_EQ(hull.size(), static_cast<std::size_t>(30));
        long long way = 0;
        bool consistent = true;
        bool fits = true;
        for (std::size_t at = 0; at < hull.size(); at++) {
            long long const turn = turn_of(hull[at], hull[(at + 1) % hull.size()],
                                           hull[(at + 2) % hull.size()]);
            if (turn != 0 && way == 0) way = turn;
            if (turn != 0 && turn != way) consistent = false;
            if (hull[at].x < -100000 || hull[at].x > 100000 || hull[at].y < -100000 ||
                hull[at].y > 100000)
                fits = false;
        }
        EO_CHECK(consistent);
        EO_CHECK(fits);
    }
}

EO_TEST(points_in_convex_position_never_turn_back_at_scale) {
    eo::rng draw(24);
    std::vector<eo::point> const hull = eo::shapes::convex_position(draw, 2000, 1000000);
    EO_CHECK_EQ(hull.size(), static_cast<std::size_t>(2000));
    long long way = 0;
    std::size_t reversed = 0;
    for (std::size_t at = 0; at < hull.size(); at++) {
        long long const t = turn_of(hull[at], hull[(at + 1) % hull.size()], hull[(at + 2) % hull.size()]);
        if (t == 0) continue;
        if (way == 0) way = t;
        if (t != way) reversed++;
    }
    EO_CHECK_EQ(reversed, static_cast<std::size_t>(0));
}

EO_TEST(points_on_a_circle_are_the_same_points_for_the_same_seed) {
    eo::rng draw(23);
    std::string first;
    for (eo::point const& one : eo::shapes::cocircular(draw, 8)) first += eo::fmt("{} {};", one.x, one.y);
    EO_CHECK_EQ(first, std::string("-3 4;3 4;0 -5;-4 3;0 5;-4 -3;-3 -4;5 0;"));
    std::string rest;
    for (long long const count : {20LL, 100LL, 300LL})
        for (eo::point const& one : eo::shapes::cocircular(draw, count)) rest += eo::fmt("{} {};", one.x, one.y);
    EO_CHECK_EQ(rest.size(), static_cast<std::size_t>(4836));
    EO_CHECK_EQ(eo::detail::seed_of(rest), 0x95d36165e8318cb1ull);
}

EO_TEST(points_on_a_circle_are_exactly_on_it) {
    eo::rng draw(21);
    for (long long const count : {8LL, 20LL, 100LL, 300LL}) {
        std::vector<eo::point> const ring = eo::shapes::cocircular(draw, count);
        EO_CHECK_EQ(ring.size(), static_cast<std::size_t>(count));
        long long const radius = ring[0].x * ring[0].x + ring[0].y * ring[0].y;
        bool all_on_it = true;
        for (eo::point const& one : ring)
            if (one.x * one.x + one.y * one.y != radius) all_on_it = false;
        EO_CHECK(all_on_it);
    }
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cocircular(draw, 5000); }), "convex_position scales instead");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cocircular(draw, -1); }), "at least no points, not -1");
}

EO_TEST(points_on_a_circle_stay_inside_a_limit_and_are_otherwise_the_same) {
    for (long long const count : {8LL, 20LL, 100LL}) {
        eo::rng unbounded(23);
        eo::rng bounded(23);
        std::vector<eo::point> const loose = eo::shapes::cocircular(unbounded, count);
        std::vector<eo::point> const kept = eo::shapes::cocircular(bounded, count, 1000000000);
        bool same = loose.size() == kept.size();
        for (std::size_t at = 0; same && at < loose.size(); at++)
            same = loose[at].x == kept[at].x && loose[at].y == kept[at].y;
        EO_CHECK(same);
    }
    eo::rng draw(24);
    std::vector<eo::point> const small = eo::shapes::cocircular(draw, 12, 5);
    bool inside = true;
    for (eo::point const& one : small) inside = inside && one.x * one.x + one.y * one.y == 25;
    EO_CHECK(inside);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cocircular(draw, 13, 64); }),
                "have none of radius at most 64 with 13 lattice points");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cocircular(draw, 5000, 2000000); }), "with 5000 lattice points");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cocircular(draw, 5, 4); }), "none of radius at most 4 with 5");
    std::vector<eo::point> const unit = eo::shapes::cocircular(draw, 4, 3);
    bool round = unit.size() == 4;
    for (eo::point const& one : unit) round = round && one.x * one.x + one.y * one.y == 1;
    EO_CHECK(round);
    EO_CHECK(eo::shapes::cocircular(draw, 0, 1).empty());
    EO_CHECK_EQ(eo::shapes::cocircular(draw, 3, 4).size(), static_cast<std::size_t>(3));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::cocircular(draw, 3, 0); }), "a coordinate bound of at least 1");
}

EO_TEST(extreme_points_sit_on_the_bound_itself) {
    eo::rng draw(22);
    std::vector<eo::point> const edges = eo::shapes::extreme_points(draw, 200, 1000000000);
    EO_CHECK_EQ(edges.size(), static_cast<std::size_t>(200));
    bool on_the_edge = true;
    for (eo::point const& one : edges) {
        bool const x_pinned = one.x == -1000000000 || one.x == 1000000000;
        bool const y_pinned = one.y == -1000000000 || one.y == 1000000000;
        if (!x_pinned && !y_pinned) on_the_edge = false;
    }
    EO_CHECK(on_the_edge);
    std::vector<eo::point> const crowded = eo::shapes::extreme_points(draw, 20, 3);
    EO_CHECK_EQ(crowded.size(), static_cast<std::size_t>(20));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::extreme_points(draw, 30, 3); }),
                "the edge of +-3 carries 24 different points, not 30");
}

EO_TEST(a_shape_that_cannot_be_drawn_says_so_instead_of_spinning) {
    eo::rng draw(25);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::near_periodic(draw, "a", 5, eo::charset("a")); }),
                "near_periodic needs a character other than 'a'");
    EO_CHECK_EQ(eo::shapes::near_periodic(draw, "a", 5, eo::charset("ab")).size(),
                static_cast<std::size_t>(5));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::extreme_points(draw, 24, 3); }), "");
    std::string const nearly = eo::shapes::near_periodic(draw, "ab", 9, eo::charset("ab"));
    EO_CHECK_EQ(nearly.size(), static_cast<std::size_t>(9));
}

EO_TEST(a_shape_outside_its_bounds_is_a_library_error) {
    eo::rng draw(23);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::scattered(draw, 5, 0); }), "coordinate bound of at least 1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::scattered(draw, -1, 5); }), "at least no points");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::collinear(draw, 100, 3); }),
                "100 different collinear points do not fit inside +-3");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::convex_position(draw, 2, 100); }), "at least 3 points, not 2");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::convex_position(draw, 30, 5); }),
                "30 points in convex position do not fit inside +-5");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::convex_position(draw, 10, 2000000000); }),
                "would overflow its own cross products");
}

EO_TEST(a_shape_that_throws_most_draws_away_is_reported) {
    eo::detail::diagnostics::shared().forget_everything();
    eo::rng draw(7);
    std::vector<eo::point> const whole = eo::shapes::extreme_points(draw, 800, 100);
    EO_CHECK_EQ(static_cast<long long>(whole.size()), 800LL);
    EO_CHECK_IN(eo::detail::diagnostics::shared().local_block(),
                "warning EO505: that many different points on the bound kept 800 of ");

    eo::detail::diagnostics::shared().forget_everything();
    std::vector<eo::point> const few = eo::shapes::extreme_points(draw, 10, 100);
    EO_CHECK_EQ(static_cast<long long>(few.size()), 10LL);
    EO_CHECK(!eot::contains(eo::detail::diagnostics::shared().local_block(), "EO505"));
    eo::detail::diagnostics::shared().forget_everything();
}

namespace {

std::string edges_of(std::vector<eo::edge> const& edges) {
    std::string out;
    for (eo::edge const& one : edges) out += eo::fmt("{} {};", one.u, one.v);
    return out;
}

std::string values_of(std::vector<long long> const& values) {
    std::string out;
    for (long long const one : values) out += eo::fmt("{};", one);
    return out;
}

std::string points_of(std::vector<eo::point> const& points) {
    std::string out;
    for (eo::point const& one : points) out += eo::fmt("{} {};", one.x, one.y);
    return out;
}

std::string every_existing_shape() {
    eo::rng draw(20261002);
    std::string out;
    for (char const* name : {"random", "uniform", "path", "star", "caterpillar", "broom", "binary", "dumbbell"})
        out += edges_of(eo::shapes::presented(draw, eo::shapes::tree(draw, 50, name))) + "|";
    out += edges_of(eo::shapes::deep_tree(draw, 60, 3).edges) + "|";
    out += edges_of(eo::shapes::caterpillar(draw, 40, 7).edges) + "|";
    out += edges_of(eo::shapes::broom(30, 4).edges) + edges_of(eo::shapes::kary_tree(30, 3).edges) + "|";
    out += edges_of(eo::shapes::spider(30, 4).edges) + "|";
    out += edges_of(eo::shapes::connected_graph(draw, 40, 70).edges) + "|";
    out += edges_of(eo::shapes::connected_graph(draw, 12, 50).edges) + "|";
    out += edges_of(eo::shapes::sparse_graph(draw, 40, 30).edges) + "|";
    out += edges_of(eo::shapes::complete_graph(6).edges) + edges_of(eo::shapes::cycle(7).edges) + "|";
    out += edges_of(eo::shapes::cycle_with_chords(draw, 20, 9).edges) + "|";
    out += edges_of(eo::shapes::grid(3, 4).edges) + "|";
    out += edges_of(eo::shapes::bipartite_graph(draw, 10, 12, 30).edges) + "|";
    out += edges_of(eo::shapes::bipartite_graph(draw, 5, 6, 25).edges) + "|";
    out += edges_of(eo::shapes::complete_bipartite(3, 4).edges) + "|";
    out += edges_of(eo::shapes::many_components(draw, 30, 4).edges) + "|";
    out += edges_of(eo::shapes::dag(draw, 30, 40).edges);
    out += edges_of(eo::shapes::dag(draw, 10, 30).edges) + "|";
    for (char const* name : {"cycle", "rho", "self", "random"})
        for (int const one : eo::shapes::functional(draw, 20, name)) out += eo::fmt("{};", one);
    for (int const one : eo::shapes::parent_array(draw, eo::shapes::uniform_tree(draw, 40))) out += eo::fmt("{};", one);
    out += "|" + values_of(eo::shapes::equal_values(5, 9));
    out += values_of(eo::shapes::few_distinct(draw, 30, 4, 1, 1000));
    out += values_of(eo::shapes::plateaus(draw, 30, 5, 1, 1000));
    out += values_of(eo::shapes::nearly_sorted(draw, 30, 1, 1000, 4));
    out += values_of(eo::shapes::alternating(draw, 30, 1, 1000));
    out += values_of(eo::shapes::hash_collisions(5, 11)) + "|";
    out += eo::shapes::repeated('x', 5) + eo::shapes::periodic("abc", 10);
    out += eo::shapes::near_periodic(draw, "ab", 15, eo::charset("abc"));
    out += eo::shapes::fibonacci_word(20) + eo::shapes::thue_morse(20);
    out += eo::shapes::palindrome(draw, 15, eo::charset("a-z")) + "|";
    out += points_of(eo::shapes::scattered(draw, 20, 1000));
    out += points_of(eo::shapes::collinear(draw, 20, 1000));
    out += points_of(eo::shapes::convex_position(draw, 20, 1000));
    out += points_of(eo::shapes::cocircular(draw, 12));
    out += points_of(eo::shapes::cocircular(draw, 12, 100));
    out += points_of(eo::shapes::extreme_points(draw, 20, 1000));
    return out;
}

}  // namespace

EO_TEST(every_shape_of_2_3_0_draws_the_bytes_it_drew_then) {
    std::string const drawn = every_existing_shape();
    EO_CHECK_EQ(drawn.size(), static_cast<std::size_t>(6770));
    EO_CHECK_EQ(eo::detail::seed_of(drawn), 0xa28c85369db98243ull);
}
