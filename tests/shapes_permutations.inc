namespace {

int cycles_of(std::vector<int> const& p) {
    std::vector<bool> seen(p.size() + 1, false);
    int found = 0;
    for (int start = 1; start <= static_cast<int>(p.size()); start++) {
        if (seen[static_cast<std::size_t>(start)]) continue;
        found++;
        for (int at = start; !seen[static_cast<std::size_t>(at)]; at = p[static_cast<std::size_t>(at) - 1])
            seen[static_cast<std::size_t>(at)] = true;
    }
    return found;
}

int fixed_points_of(std::vector<int> const& p) {
    int found = 0;
    for (std::size_t at = 0; at < p.size(); at++)
        if (p[at] == static_cast<int>(at) + 1) found++;
    return found;
}

std::string ints_of(std::vector<int> const& values) {
    std::string out;
    for (int const one : values) out += eo::fmt("{};", one);
    return out;
}

}  // namespace

EO_TEST(a_permutation_has_exactly_the_cycles_asked_for) {
    eo::rng draw(101);
    for (int n = 1; n <= 30; n++)
        for (int k = 1; k <= n; k += 3) {
            std::vector<int> const p = eo::shapes::permutation_cycles(draw, n, k);
            EO_CHECK(static_cast<bool>(eo::is_permutation(p)));
            EO_CHECK_EQ(static_cast<int>(p.size()), n);
            EO_CHECK_EQ(cycles_of(p), k);
        }
    EO_CHECK_EQ(cycles_of(eo::shapes::permutation_cycles(draw, 100000, 1)), 1);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::permutation_cycles(draw, 5, 6); }),
                "a permutation of 5 elements has 1..5 cycles, not 6");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::permutation_cycles(draw, 5, 0); }), "has 1..5 cycles, not 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::permutation_cycles(draw, 0, 1); }),
                "a permutation with cycles needs at least 1 elements, not 0");
}

EO_TEST(a_derangement_moves_every_element_and_reaches_every_derangement) {
    eo::rng draw(102);
    for (int n = 2; n <= 40; n++) {
        std::vector<int> const p = eo::shapes::derangement(draw, n);
        EO_CHECK(static_cast<bool>(eo::is_permutation(p)));
        EO_CHECK_EQ(fixed_points_of(p), 0);
    }
    std::vector<std::string> seen;
    for (int round = 0; round < 400; round++) seen.push_back(ints_of(eo::shapes::derangement(draw, 4)));
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    EO_CHECK_EQ(seen.size(), static_cast<std::size_t>(9));
    EO_CHECK(eo::shapes::derangement(draw, 0).empty());
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::derangement(draw, 1); }), "no derangement of 1 element exists");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::derangement(draw, -1); }),
                "a derangement has at least no elements, not -1");
}

EO_TEST(an_involution_is_its_own_inverse_with_the_fixed_points_asked_for) {
    eo::rng draw(103);
    for (int n = 0; n <= 20; n++)
        for (int fixed = n % 2; fixed <= n; fixed += 2) {
            std::vector<int> const p = eo::shapes::involution(draw, n, fixed);
            EO_CHECK(static_cast<bool>(eo::is_permutation(p)));
            bool own_inverse = true;
            for (int at = 1; at <= n; at++)
                if (p[static_cast<std::size_t>(p[static_cast<std::size_t>(at) - 1]) - 1] != at) own_inverse = false;
            EO_CHECK(own_inverse);
            EO_CHECK_EQ(fixed_points_of(p), fixed);
        }
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::involution(draw, 6, 1); }),
                "an involution of 6 elements pairs the other 5, which is odd; ask for an even number of fixed points");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::involution(draw, 6, 7); }),
                "an involution of 6 elements has 0..6 fixed points, not 7");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::involution(draw, 5, 2); }), "ask for an odd number of fixed points");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::involution(draw, -2, 0); }),
                "an involution has at least no elements, not -2");
}

EO_TEST(the_permutation_dispatcher_knows_every_name_it_offers) {
    eo::rng draw(104);
    for (char const* name : {"random", "identity", "reversed", "cycle", "derangement", "involution"}) {
        std::vector<int> const p = eo::shapes::permutation(draw, 9, name);
        EO_CHECK(static_cast<bool>(eo::is_permutation(p)));
        EO_CHECK_EQ(p.size(), static_cast<std::size_t>(9));
    }
    EO_CHECK_EQ(ints_of(eo::shapes::permutation(draw, 4, "identity")), std::string("1;2;3;4;"));
    EO_CHECK_EQ(ints_of(eo::shapes::permutation(draw, 4, "reversed")), std::string("4;3;2;1;"));
    EO_CHECK_EQ(cycles_of(eo::shapes::permutation(draw, 50, "cycle")), 1);
    EO_CHECK_EQ(fixed_points_of(eo::shapes::permutation(draw, 50, "derangement")), 0);
    EO_CHECK_EQ(fixed_points_of(eo::shapes::permutation(draw, 51, "involution")), 1);
    EO_CHECK_EQ(cycles_of(eo::shapes::permutation(draw, 50, "involution")), 25);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::permutation(draw, 5, "sorted"); }),
                "\"sorted\" is not a permutation shape; the names are random, identity, reversed, cycle, derangement "
                "and involution");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::permutation(draw, 0, "random"); }),
                "a permutation needs at least 1 elements, not 0");
}

EO_TEST(permutations_by_cycles_are_the_same_bytes_for_the_same_seed) {
    eo::rng draw(105);
    std::string drawn;
    drawn += ints_of(eo::shapes::permutation_cycles(draw, 30, 4));
    drawn += ints_of(eo::shapes::derangement(draw, 30));
    drawn += ints_of(eo::shapes::involution(draw, 30, 6));
    for (char const* name : {"random", "cycle", "derangement", "involution"})
        drawn += ints_of(eo::shapes::permutation(draw, 20, name));
    pin_bytes(drawn, 447, 0xcd87eb060fabc061ull, __FILE__, __LINE__);
}

namespace {

long long inversions_by_merging(std::vector<int> values) {
    long long found = 0;
    std::vector<int> spare(values.size());
    for (std::size_t width = 1; width < values.size(); width *= 2)
        for (std::size_t from = 0; from + width < values.size(); from += 2 * width) {
            std::size_t const middle = from + width;
            std::size_t const to = std::min(values.size(), from + 2 * width);
            std::size_t left = from;
            std::size_t right = middle;
            std::size_t put = from;
            while (left < middle || right < to) {
                if (right == to || (left < middle && values[left] <= values[right])) {
                    spare[put++] = values[left++];
                } else {
                    found += static_cast<long long>(middle - left);
                    spare[put++] = values[right++];
                }
            }
            std::copy(spare.begin() + static_cast<long>(from), spare.begin() + static_cast<long>(to),
                      values.begin() + static_cast<long>(from));
        }
    return found;
}

}  // namespace

EO_TEST(a_permutation_has_exactly_the_inversions_asked_for) {
    eo::rng draw(106);
    for (int n = 1; n <= 7; n++)
        for (long long k = 0; k <= n * (n - 1) / 2; k++) {
            std::vector<int> const p = eo::shapes::with_inversions(draw, n, k);
            EO_CHECK(static_cast<bool>(eo::is_permutation(p)));
            long long counted = 0;
            for (std::size_t i = 0; i < p.size(); i++)
                for (std::size_t j = i + 1; j < p.size(); j++)
                    if (p[i] > p[j]) counted++;
            EO_CHECK_EQ(counted, k);
        }
    for (long long const k : {0LL, 1LL, 1000000LL, 1999000LL, 1998999LL}) {
        std::vector<int> const p = eo::shapes::with_inversions(draw, 2000, k);
        EO_CHECK_EQ(inversions_by_merging(p), k);
    }
    std::vector<std::string> seen;
    for (int round = 0; round < 300; round++) seen.push_back(ints_of(eo::shapes::with_inversions(draw, 5, 4)));
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    EO_CHECK_EQ(seen.size(), static_cast<std::size_t>(20));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_inversions(draw, 5, 11); }),
                "a permutation of 5 elements has 0..10 inversions, not 11");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_inversions(draw, 5, -1); }), "has 0..10 inversions, not -1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::with_inversions(draw, 0, 0); }),
                "a permutation with inversions needs at least 1 elements, not 0");
}

EO_TEST(inversions_reach_beyond_an_int_and_are_the_same_bytes_for_the_same_seed) {
    eo::rng draw(107);
    std::vector<int> const most = eo::shapes::with_inversions(draw, 100000, 4999950000LL);
    EO_CHECK_EQ(most.front(), 100000);
    EO_CHECK_EQ(most.back(), 1);
    std::vector<int> const beyond = eo::shapes::with_inversions(draw, 100000, 3000000000LL);
    EO_CHECK_EQ(inversions_by_merging(beyond), 3000000000LL);
    std::string drawn = ints_of(eo::shapes::with_inversions(draw, 40, 300));
    drawn += ints_of(eo::shapes::with_inversions(draw, 40, 7));
    pin_bytes(drawn, 222, 0x4c109276fec3ad51ull, __FILE__, __LINE__);
}
