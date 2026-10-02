namespace {

std::string intervals_of(std::vector<eo::interval> const& made) {
    std::string out;
    for (eo::interval const& one : made) out += eo::fmt("{} {};", one.l, one.r);
    return out;
}

bool crossing(eo::interval const& a, eo::interval const& b) { return a.l <= b.r && b.l <= a.r; }

bool inside(eo::interval const& small, eo::interval const& big) { return big.l <= small.l && small.r <= big.r; }

}  // namespace

EO_TEST(intervals_come_in_the_shapes_their_names_promise) {
    eo::rng draw(301);
    long long const low = -1000;
    long long const high = 1000;
    for (char const* name : {"random", "disjoint", "touching", "nested", "laminar", "chain", "through", "same",
                             "points"}) {
        std::vector<eo::interval> const made = eo::shapes::intervals(draw, 60, low, high, name);
        EO_CHECK_EQ(made.size(), static_cast<std::size_t>(60));
        bool ordered = true;
        for (eo::interval const& one : made) ordered = ordered && low <= one.l && one.l <= one.r && one.r <= high;
        EO_CHECK(ordered);
        std::string const shape = name;
        bool holds = true;
        std::vector<eo::interval> sorted = made;
        std::sort(sorted.begin(), sorted.end(),
                  [](eo::interval const& a, eo::interval const& b) { return a.l < b.l || (a.l == b.l && a.r > b.r); });
        for (std::size_t i = 0; i < made.size(); i++)
            for (std::size_t j = i + 1; j < made.size(); j++) {
                eo::interval const& a = made[i];
                eo::interval const& b = made[j];
                if (shape == "disjoint") holds = holds && !crossing(a, b);
                if (shape == "touching") holds = holds && (!crossing(a, b) || a.r == b.l || b.r == a.l);
                if (shape == "nested" || shape == "laminar")
                    holds = holds && (!crossing(a, b) || inside(a, b) || inside(b, a));
                if (shape == "nested") holds = holds && (inside(a, b) || inside(b, a)) && a.l != b.l && a.r != b.r;
                if (shape == "through" || shape == "same") holds = holds && crossing(a, b);
                if (shape == "same") holds = holds && a.l == b.l && a.r == b.r;
                if (shape == "points") holds = holds && a.l == a.r;
            }
        if (shape == "touching")
            for (std::size_t at = 1; at < sorted.size(); at++) holds = holds && sorted[at - 1].r == sorted[at].l;
        if (shape == "chain")
            for (std::size_t i = 0; i < sorted.size(); i++)
                for (std::size_t j = i + 1; j < sorted.size(); j++)
                    holds = holds && crossing(sorted[i], sorted[j]) == (j == i + 1) &&
                            !inside(sorted[i], sorted[j]) && !inside(sorted[j], sorted[i]);
        if (shape == "laminar") {
            bool some_nested = false;
            bool some_apart = false;
            for (std::size_t i = 0; i < made.size(); i++)
                for (std::size_t j = i + 1; j < made.size(); j++) {
                    if (inside(made[i], made[j]) || inside(made[j], made[i])) some_nested = true;
                    if (!crossing(made[i], made[j])) some_apart = true;
                }
            holds = holds && some_nested && some_apart;
        }
        if (!holds) std::fprintf(stderr, "intervals: %s does not hold\n", name);
        EO_CHECK(holds);
    }
    EO_CHECK(eo::shapes::intervals(draw, 0, 1, 1, "disjoint").empty());
    EO_CHECK(eo::shapes::intervals(draw, 0, 1, 1, "chain").empty());
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::intervals(draw, 5, 1, 9, "disjoint"); }),
                "5 disjoint intervals need 10 different endpoints, and 1..9 holds 9");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::intervals(draw, 5, 1, 5, "touching"); }),
                "5 touching intervals need 6 different endpoints, and 1..5 holds 5");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::intervals(draw, 4, 1, 9, "chain"); }),
                "4 chain intervals need 10 different endpoints, and 1..9 holds 9");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::intervals(draw, 5, 1, 9, "crossing"); }),
                "\"crossing\" is not an interval shape; the names are random, disjoint, touching, nested, laminar, "
                "chain, through, same and points");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::intervals(draw, -1, 1, 9, "random"); }),
                "a family of intervals has at least no elements, not -1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::intervals(draw, 1, 9, 1, "random"); }),
                "a family of intervals draws from 9..1, which is empty");
}

EO_TEST(a_laminar_family_draws_every_nesting_of_three_and_the_same_bytes_for_the_same_seed) {
    eo::rng draw(302);
    std::vector<std::string> seen;
    for (int round = 0; round < 300; round++) {
        std::vector<eo::interval> made = eo::shapes::intervals(draw, 3, 1, 6, "laminar");
        std::sort(made.begin(), made.end(), [](eo::interval const& a, eo::interval const& b) { return a.l < b.l; });
        seen.push_back(intervals_of(made));
    }
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    EO_CHECK_EQ(seen.size(), static_cast<std::size_t>(5));
    std::string drawn;
    for (char const* name : {"random", "disjoint", "touching", "nested", "laminar", "chain", "through", "same",
                             "points"})
        drawn += intervals_of(eo::shapes::intervals(draw, 6, 1, 100, name));
    pin_bytes(drawn, 308, 0x5330ef545630cb36ull, __FILE__, __LINE__);
}
