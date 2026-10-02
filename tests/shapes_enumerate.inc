EO_TEST(small_arrays_and_strings_are_numbered_in_lexicographic_order) {
    for (long long length = 0; length <= 4; length++)
        for (long long const high : {0LL, 1LL, 2LL}) {
            long long const count = eo::shapes::count_arrays(length, -1, high);
            long long expected = 1;
            for (long long at = 0; at < length; at++) expected *= high + 2;
            EO_CHECK_EQ(count, expected);
            std::vector<std::string> seen;
            for (long long index = 0; index < count; index++) {
                std::vector<long long> const values = eo::shapes::array_at(length, -1, high, index);
                EO_CHECK_EQ(static_cast<long long>(values.size()), length);
                std::string key;
                for (long long const one : values) key += static_cast<char>('a' + one + 1);
                seen.push_back(key);
            }
            EO_CHECK(std::is_sorted(seen.begin(), seen.end()));
            EO_CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
        }
    EO_CHECK_EQ(eo::shapes::count_strings(4, eo::charset("abc")), 81LL);
    std::vector<std::string> words;
    for (long long index = 0; index < 81; index++) words.push_back(eo::shapes::string_at(4, eo::charset("abc"), index));
    EO_CHECK(std::is_sorted(words.begin(), words.end()));
    EO_CHECK(std::adjacent_find(words.begin(), words.end()) == words.end());
    EO_CHECK_EQ(words.front(), std::string("aaaa"));
    EO_CHECK_EQ(words.back(), std::string("cccc"));
    EO_CHECK_EQ(eo::shapes::count_strings(0, eo::charset("ab")), 1LL);
    EO_CHECK_EQ(eo::shapes::count_strings(2, eo::charset("")), 0LL);
    EO_CHECK_EQ(eo::shapes::count_arrays(62, 0, 1), 4611686018427387904LL);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_arrays(63, 0, 1); }),
                "there are more arrays of 63 values in 0..1 than a long long counts");
    EO_CHECK_IN(
        refusal([] { (void)eo::shapes::count_arrays(1, -9223372036854775807LL - 1, 9223372036854775807LL); }),
                "there are more arrays of 1 values in -9223372036854775808..9223372036854775807 than a long long counts");
    EO_CHECK_EQ(eo::shapes::count_arrays(0, -9223372036854775807LL - 1, 9223372036854775807LL), 1LL);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_arrays(-1, 0, 1); }), "arrays have at least no values, not -1");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::array_at(3, 0, 1, 8); }),
                "the arrays of 3 values in 0..1 are numbered 0..7, not 8");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::array_at(3, 2, 1, 0); }), "arrays draw from 2..1, which is empty");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::string_at(2, eo::charset("ab"), -1); }),
                "the strings of 2 letters over \"ab\" are numbered 0..3, not -1");
}

EO_TEST(small_permutations_are_numbered_in_lexicographic_order) {
    for (int n = 0; n <= 7; n++) {
        long long const count = eo::shapes::count_permutations(n);
        std::vector<int> expected(static_cast<std::size_t>(n));
        for (int at = 0; at < n; at++) expected[static_cast<std::size_t>(at)] = at + 1;
        bool same = true;
        long long index = 0;
        do {
            same = same && eo::shapes::permutation_at(n, index) == expected;
            index++;
        } while (std::next_permutation(expected.begin(), expected.end()));
        EO_CHECK(same);
        EO_CHECK_EQ(index, count);
    }
    EO_CHECK_EQ(eo::shapes::count_permutations(20), 2432902008176640000LL);
    std::vector<int> const last = eo::shapes::permutation_at(20, 2432902008176639999LL);
    EO_CHECK_EQ(last.front(), 20);
    EO_CHECK_EQ(last.back(), 1);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_permutations(21); }),
                "there are more permutations of 21 elements than a long long counts");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::permutation_at(3, 6); }),
                "the permutations of 3 elements are numbered 0..5, not 6");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_permutations(-1); }),
                "a permutation to count has at least no elements, not -1");
}

namespace {

std::string edge_set_of(eo::graph const& made) {
    std::vector<std::pair<int, int>> pairs;
    for (eo::edge const& one : made.edges) pairs.push_back({std::min(one.u, one.v), std::max(one.u, one.v)});
    std::sort(pairs.begin(), pairs.end());
    std::string out;
    for (auto const& one : pairs) out += eo::fmt("{}-{};", one.first, one.second);
    return out;
}

std::vector<std::string> every_spanning_tree(int n) {
    std::vector<eo::edge> pairs;
    for (int u = 1; u <= n; u++)
        for (int v = u + 1; v <= n; v++) pairs.push_back(eo::edge{u, v});
    std::vector<std::string> trees;
    for (unsigned mask = 0; mask < (1u << pairs.size()); mask++) {
        std::vector<eo::edge> chosen;
        for (std::size_t at = 0; at < pairs.size(); at++)
            if ((mask >> at) & 1u) chosen.push_back(pairs[at]);
        if (static_cast<int>(chosen.size()) == n - 1 && static_cast<bool>(eo::is_tree(n, chosen)))
            trees.push_back(edge_set_of(eo::graph{n, chosen, false}));
    }
    std::sort(trees.begin(), trees.end());
    return trees;
}

}  // namespace

EO_TEST(small_labelled_trees_are_numbered_by_their_pruefer_codes) {
    for (int n = 1; n <= 6; n++) {
        long long const count = eo::shapes::count_trees(n);
        std::vector<std::string> numbered;
        for (long long index = 0; index < count; index++) {
            eo::graph const made = eo::shapes::tree_at(n, index);
            EO_CHECK(holds_a_tree(made));
            numbered.push_back(edge_set_of(made));
        }
        std::sort(numbered.begin(), numbered.end());
        EO_CHECK(numbered == every_spanning_tree(n));
    }
    EO_CHECK_EQ(eo::shapes::count_trees(7), 16807LL);
    EO_CHECK_EQ(eo::shapes::count_trees(16), 72057594037927936LL);
    EO_CHECK_EQ(eo::shapes::count_trees(17), 2862423051509815793LL);
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_trees(18); }),
                "there are more labelled trees on 18 vertices than a long long counts");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::tree_at(4, 16); }),
                "the labelled trees on 4 vertices are numbered 0..15, not 16");
    EO_CHECK_IN(refusal([] { (void)eo::shapes::count_trees(0); }), "a tree to count needs at least 1 vertices, not 0");
}
