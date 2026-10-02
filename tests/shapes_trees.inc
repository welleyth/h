namespace {

std::vector<int> degrees_of(eo::graph const& made) {
    std::vector<int> count(static_cast<std::size_t>(made.n) + 1, 0);
    for (eo::edge const& one : made.edges) {
        count[static_cast<std::size_t>(one.u)]++;
        count[static_cast<std::size_t>(one.v)]++;
    }
    return count;
}

std::vector<int> distances_from(eo::graph const& made, int from) {
    std::vector<std::vector<int>> near(static_cast<std::size_t>(made.n) + 1);
    for (eo::edge const& one : made.edges) {
        near[static_cast<std::size_t>(one.u)].push_back(one.v);
        if (!made.directed) near[static_cast<std::size_t>(one.v)].push_back(one.u);
    }
    std::vector<int> far(static_cast<std::size_t>(made.n) + 1, -1);
    std::vector<int> waiting{from};
    far[static_cast<std::size_t>(from)] = 0;
    for (std::size_t at = 0; at < waiting.size(); at++) {
        int const here = waiting[at];
        for (int const other : near[static_cast<std::size_t>(here)])
            if (far[static_cast<std::size_t>(other)] < 0) {
                far[static_cast<std::size_t>(other)] = far[static_cast<std::size_t>(here)] + 1;
                waiting.push_back(other);
            }
    }
    return far;
}

int eccentricity(eo::graph const& made, int from) {
    std::vector<int> const far = distances_from(made, from);
    return *std::max_element(far.begin() + 1, far.end());
}

int tree_diameter(eo::graph const& made) {
    std::vector<int> const far = distances_from(made, 1);
    int const end = static_cast<int>(std::max_element(far.begin() + 1, far.end()) - far.begin());
    return eccentricity(made, end);
}

int leaves_of(eo::graph const& made) {
    std::vector<int> const degree = degrees_of(made);
    return static_cast<int>(std::count(degree.begin() + 1, degree.end(), 1));
}

}  // namespace

EO_TEST(a_tree_from_a_pruefer_code_is_the_tree_the_code_names) {
    eo::graph const made = eo::shapes::tree_from_pruefer({4, 4, 4, 5});
    EO_CHECK_EQ(made.n, 6);
    EO_CHECK_EQ(edges_of(made.edges), std::string("1 4;2 4;3 4;4 5;5 6;"));
    EO_CHECK_EQ(eo::shapes::tree_from_pruefer({}).n, 2);
    EO_CHECK_EQ(edges_of(eo::shapes::tree_from_pruefer({}).edges), std::string("1 2;"));
    eo::rng draw(401);
    for (int round = 0; round < 50; round++) {
        std::vector<int> code;
        for (int at = 0; at < 30; at++) code.push_back(static_cast<int>(draw.uniform(1, 32)));
        eo::graph const tree = eo::shapes::tree_from_pruefer(code);
        EO_CHECK(holds_a_tree(tree));
        std::vector<int> const degree = degrees_of(tree);
        bool matches = true;
        for (int vertex = 1; vertex <= 32; vertex++)
            matches = matches && degree[static_cast<std::size_t>(vertex)] ==
                                     1 + static_cast<int>(std::count(code.begin(), code.end(), vertex));
        EO_CHECK(matches);
    }
    EO_CHECK_IN(refusal([] { (void)eo::shapes::tree_from_pruefer({1, 5}); }),
                "a Pruefer code of length 2 holds vertices 1..4; element 2 is 5");
}

EO_TEST(a_tree_from_degrees_has_exactly_those_degrees) {
    eo::rng draw(402);
    std::vector<int> const wanted{1, 3, 1, 2, 1, 4, 1, 1, 1, 3};
    for (int round = 0; round < 30; round++) {
        eo::graph const made = eo::shapes::tree_from_degrees(draw, wanted);
        EO_CHECK(holds_a_tree(made));
        std::vector<int> const degree = degrees_of(made);
        EO_CHECK(std::equal(wanted.begin(), wanted.end(), degree.begin() + 1));
    }
    EO_CHECK_EQ(eo::shapes::tree_from_degrees(draw, {0}).n, 1);
    EO_CHECK_EQ(eo::shapes::tree_from_degrees(draw, {1, 1}).edges.size(), static_cast<std::size_t>(1));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_from_degrees(draw, {1, 2, 2}); }),
                "the degrees of a tree on 3 vertices add up to 4, and these add up to 5");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_from_degrees(draw, {0, 2, 2}); }),
                "every vertex of a tree on 3 vertices has a degree of at least 1; vertex 1 has 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_from_degrees(draw, {}); }),
                "a tree from degrees needs at least 1 vertices, not 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_from_degrees(draw, {1}); }),
                "a tree on 1 vertex has degree 0, not 1");
}

EO_TEST(a_tree_has_exactly_the_leaves_asked_for) {
    eo::rng draw(403);
    for (int n = 3; n <= 40; n++)
        for (int leaves = 2; leaves < n; leaves += 3) {
            eo::graph const made = eo::shapes::tree_with_leaves(draw, n, leaves);
            EO_CHECK(holds_a_tree(made));
            EO_CHECK_EQ(leaves_of(made), leaves);
        }
    EO_CHECK_EQ(leaves_of(eo::shapes::tree_with_leaves(draw, 2, 2)), 2);
    EO_CHECK_EQ(leaves_of(eo::shapes::tree_with_leaves(draw, 100000, 99999)), 99999);
    EO_CHECK_EQ(tree_diameter(eo::shapes::tree_with_leaves(draw, 1000, 2)), 999);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_with_leaves(draw, 10, 10); }),
                "a tree on 10 vertices has 2..9 leaves, not 10");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_with_leaves(draw, 2, 1); }),
                "a tree on 2 vertices has 2..2 leaves, not 1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::tree_with_leaves(draw, 1, 0); }),
                "a tree with leaves needs at least 2 vertices, not 1");
}

EO_TEST(trees_through_pruefer_codes_are_the_same_bytes_for_the_same_seed) {
    eo::rng draw(404);
    std::string drawn = edges_of(eo::shapes::tree_from_degrees(draw, {1, 3, 1, 2, 1, 4, 1, 1, 1, 3}).edges);
    drawn += edges_of(eo::shapes::tree_with_leaves(draw, 30, 9).edges);
    pin_bytes(drawn, 196, 0x8ac484b964453a80ull, __FILE__, __LINE__);
}
