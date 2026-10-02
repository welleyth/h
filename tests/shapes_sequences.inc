namespace {

int digits_of(long long value) {
    int digits = 1;
    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}

}  // namespace

EO_TEST(log_uniform_values_take_every_length_equally_often) {
    eo::rng draw(201);
    std::vector<long long> const values = eo::shapes::log_uniform(draw, 18000, 1, 999999999999999999LL);
    std::vector<int> lengths(19, 0);
    bool inside = true;
    for (long long const one : values) {
        if (one < 1 || one > 999999999999999999LL) inside = false;
        lengths[static_cast<std::size_t>(digits_of(one))]++;
    }
    EO_CHECK(inside);
    for (int length = 1; length <= 18; length++)
        EO_CHECK(lengths[static_cast<std::size_t>(length)] > 800 && lengths[static_cast<std::size_t>(length)] < 1200);
    std::vector<long long> const narrow = eo::shapes::log_uniform(draw, 3000, 95, 1005);
    std::vector<int> narrow_lengths(5, 0);
    for (long long const one : narrow) {
        EO_CHECK(one >= 95 && one <= 1005);
        narrow_lengths[static_cast<std::size_t>(digits_of(one))]++;
    }
    EO_CHECK(narrow_lengths[2] > 800 && narrow_lengths[3] > 800 && narrow_lengths[4] > 800);
    std::vector<long long> const with_zero = eo::shapes::log_uniform(draw, 100, 0, 9);
    EO_CHECK(std::count(with_zero.begin(), with_zero.end(), 0LL) > 0);
    std::vector<long long> const top = eo::shapes::log_uniform(draw, 100, 1, 9223372036854775807LL);
    EO_CHECK(*std::max_element(top.begin(), top.end()) > 999999999999999999LL);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::log_uniform(draw, 5, -1, 9); }),
                "log_uniform draws from 0 up; -1..9 reaches below it");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::log_uniform(draw, 5, 9, 1); }), "draws from 9..1, which is empty");
}

EO_TEST(values_near_the_bounds_stay_within_the_spread_of_one) {
    eo::rng draw(202);
    std::vector<long long> const values = eo::shapes::near_bounds(draw, 2000, -1000000000, 1000000000, 3);
    std::size_t low_side = 0;
    bool near = true;
    for (long long const one : values) {
        if (one <= -999999997) {
            low_side++;
        } else if (one < 999999997) {
            near = false;
        }
    }
    EO_CHECK(near);
    EO_CHECK(low_side > 800 && low_side < 1200);
    EO_CHECK(std::count(values.begin(), values.end(), 1000000000LL) > 0);
    EO_CHECK(std::count(values.begin(), values.end(), -1000000000LL) > 0);
    std::vector<long long> const wide = eo::shapes::near_bounds(draw, 200, 1, 10, 100);
    bool inside = true;
    for (long long const one : wide) inside = inside && one >= 1 && one <= 10;
    EO_CHECK(inside);
    std::vector<long long> const ends = eo::shapes::near_bounds(draw, 50, 5, 9, 0);
    bool only_ends = true;
    for (long long const one : ends) only_ends = only_ends && (one == 5 || one == 9);
    EO_CHECK(only_ends);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::near_bounds(draw, 5, 1, 9, -1); }),
                "near_bounds keeps within a spread of at least 0 of a bound, not -1");
}

EO_TEST(spikes_stand_out_of_a_low_background) {
    eo::rng draw(203);
    std::vector<long long> const values = eo::shapes::spikes(draw, 1000, 7, 1, 1600);
    std::size_t tall = 0;
    bool split = true;
    for (long long const one : values) {
        if (one >= 1501) {
            tall++;
        } else if (one > 100) {
            split = false;
        }
    }
    EO_CHECK(split);
    EO_CHECK_EQ(tall, static_cast<std::size_t>(7));
    std::vector<long long> const flat = eo::shapes::spikes(draw, 10, 0, 5, 5);
    EO_CHECK_EQ(std::count(flat.begin(), flat.end(), 5LL), 10);
    std::vector<long long> const all = eo::shapes::spikes(draw, 10, 10, 0, 1000000000);
    EO_CHECK(*std::min_element(all.begin(), all.end()) >= 937500000);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::spikes(draw, 5, 6, 1, 9); }),
                "5 elements hold 0..5 spikes, not 6");
}

EO_TEST(distributions_are_the_same_bytes_for_the_same_seed) {
    eo::rng draw(204);
    std::string drawn = values_of(eo::shapes::log_uniform(draw, 30, 1, 1000000000000LL));
    drawn += values_of(eo::shapes::near_bounds(draw, 30, -100, 100, 5));
    drawn += values_of(eo::shapes::spikes(draw, 30, 4, 0, 1000000));
    pin_bytes(drawn, 508, 0xe2ed5d834119dc26ull, __FILE__, __LINE__);
}

EO_TEST(a_split_sum_adds_up_with_every_part_inside_its_bounds) {
    eo::rng draw(205);
    struct asked {
        long long total;
        long long parts;
        long long least;
        long long most;
    };
    for (asked const one : {asked{200000, 1000, 1, 1000}, asked{200000, 1000, 1, 200}, asked{200000, 1000, 200, 200},
                            asked{10, 10, 0, 1}, asked{0, 5, 0, 0}, asked{7, 1, 7, 7}, asked{200000, 3, 1, 100000},
                            asked{1000000000000000000LL, 4, 0, 1000000000000000000LL},
                            asked{100, 10, 0, 4000000000000000000LL}}) {
        std::vector<long long> const parts = eo::shapes::split_sum(draw, one.total, one.parts, one.least, one.most);
        EO_CHECK_EQ(static_cast<long long>(parts.size()), one.parts);
        long long sum = 0;
        bool inside = true;
        for (long long const part : parts) {
            sum += part;
            inside = inside && part >= one.least && part <= one.most;
        }
        EO_CHECK_EQ(sum, one.total);
        EO_CHECK(inside);
    }
    std::vector<long long> const capped = eo::shapes::split_sum(draw, 200000, 1000, 1, 201);
    EO_CHECK(std::count(capped.begin(), capped.end(), 201LL) > 0);
    EO_CHECK(*std::min_element(capped.begin(), capped.end()) < 190);
    std::string drawn = values_of(eo::shapes::split_sum(draw, 100, 7, 2, 30));
    pin_bytes(drawn, 18, 0x9adbd55037ee6fc1ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::split_sum(draw, 10, 0, 1, 5); }),
                "split_sum splits into at least one part, not 0");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::split_sum(draw, 10, 2, 6, 5); }),
                "split_sum's parts lie in 6..5, which is empty");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::split_sum(draw, 10, 2, -1, 5); }),
                "split_sum's parts are at least 0, not -1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::split_sum(draw, 11, 2, 1, 5); }),
                "2 parts of 1..5 add up to 2..10, not 11");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::split_sum(draw, 1, 2, 1, 5); }), "add up to 2..10, not 1");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::split_sum(draw, 5, 3, 4000000000000000000LL,
                                                          4000000000000000000LL); }),
                "3 parts of 4000000000000000000..4000000000000000000 add up to more than a long long holds, not 5");
}
