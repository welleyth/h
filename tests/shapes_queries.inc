EO_TEST(query_ranges_stay_inside_one_to_n_and_take_their_shape) {
    eo::rng draw(311);
    long long const n = 1000;
    for (char const* name : {"random", "short", "long", "prefix", "suffix", "point", "full", "same"}) {
        std::vector<eo::interval> const made = eo::shapes::ranges(draw, 500, n, name);
        EO_CHECK_EQ(made.size(), static_cast<std::size_t>(500));
        std::string const shape = name;
        bool holds = true;
        for (eo::interval const& one : made) {
            long long const length = one.r - one.l + 1;
            holds = holds && 1 <= one.l && one.l <= one.r && one.r <= n;
            if (shape == "short") holds = holds && length <= 16;
            if (shape == "long") holds = holds && length >= n - n / 16;
            if (shape == "prefix") holds = holds && one.l == 1;
            if (shape == "suffix") holds = holds && one.r == n;
            if (shape == "point") holds = holds && length == 1;
            if (shape == "full") holds = holds && length == n;
            if (shape == "same") holds = holds && one.l == made[0].l && one.r == made[0].r;
        }
        if (!holds) std::fprintf(stderr, "ranges: %s does not hold\n", name);
        EO_CHECK(holds);
    }
    std::vector<eo::interval> const spread = eo::shapes::ranges(draw, 2000, n, "random");
    long long longest = 0;
    long long shortest = n;
    for (eo::interval const& one : spread) {
        longest = std::max(longest, one.r - one.l + 1);
        shortest = std::min(shortest, one.r - one.l + 1);
    }
    EO_CHECK(longest > 950 && shortest < 5);
    std::vector<eo::interval> const tiny = eo::shapes::ranges(draw, 50, 1, "long");
    EO_CHECK(tiny[7].l == 1 && tiny[7].r == 1);
    std::string drawn;
    for (char const* name : {"random", "short", "long", "prefix", "suffix", "point", "full", "same"})
        drawn += intervals_of(eo::shapes::ranges(draw, 5, 100, name));
    pin_bytes(drawn, 223, 0x36778fb66f91cb10ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::ranges(draw, 5, 0, "random"); }),
                "ranges lie inside 1..n, and n is 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::ranges(draw, -1, 5, "random"); }),
                "a list of ranges has at least no elements, not -1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::ranges(draw, 0, 5, "middle"); }),
                "\"middle\" is not a range shape; the names are random, short, long, prefix, suffix, point, full and "
                "same");
}

EO_TEST(queries_of_each_kind_come_in_the_order_asked_for) {
    eo::rng draw(312);
    std::vector<long long> const counts{5, 3, 0, 2};
    for (char const* name : {"random", "grouped", "alternating"}) {
        std::vector<int> const order = eo::shapes::query_order(draw, counts, name);
        EO_CHECK_EQ(order.size(), static_cast<std::size_t>(10));
        for (int kind = 0; kind < 4; kind++)
            EO_CHECK_EQ(std::count(order.begin(), order.end(), kind), counts[static_cast<std::size_t>(kind)]);
    }
    EO_CHECK_EQ(ints_of(eo::shapes::query_order(draw, counts, "grouped")), std::string("0;0;0;0;0;1;1;1;3;3;"));
    EO_CHECK_EQ(ints_of(eo::shapes::query_order(draw, counts, "alternating")),
                std::string("0;1;3;0;1;3;0;1;0;0;"));
    std::vector<int> const mixed = eo::shapes::query_order(draw, {500, 500}, "random");
    long long switches = 0;
    for (std::size_t at = 1; at < mixed.size(); at++) switches += mixed[at] != mixed[at - 1] ? 1 : 0;
    EO_CHECK(switches > 400 && switches < 600);
    std::string const drawn = ints_of(eo::shapes::query_order(draw, {4, 4, 4}, "random"));
    pin_bytes(drawn, 24, 0xc17874a625eaef89ull, __FILE__, __LINE__);
    EO_CHECK(eo::shapes::query_order(draw, {}, "random").empty());
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::query_order(draw, {3, -1}, "random"); }),
                "query_order needs a count of at least 0 for every kind; kind 1 has -1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::query_order(draw, {3, 1}, "batched"); }),
                "\"batched\" is not a query order; the names are random, grouped and alternating");
}
