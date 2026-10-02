namespace {

long long cross_of(eo::point const& a, eo::point const& b, eo::point const& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool no_three_collinear(std::vector<eo::point> const& points) {
    for (std::size_t i = 0; i < points.size(); i++)
        for (std::size_t j = i + 1; j < points.size(); j++)
            for (std::size_t k = j + 1; k < points.size(); k++)
                if (cross_of(points[i], points[j], points[k]) == 0) return false;
    return true;
}

bool inside_box(std::vector<eo::point> const& points, long long limit) {
    for (eo::point const& one : points)
        if (one.x < -limit || one.x > limit || one.y < -limit || one.y > limit) return false;
    return true;
}

}  // namespace

EO_TEST(points_in_general_position_have_no_three_on_a_line) {
    eo::rng draw(1001);
    for (long long const limit : {1LL, 2LL, 5LL, 50LL, 1000000000LL}) {
        long long const count = std::min(120LL, limit == 1 ? 3LL : 2 * limit - 1);
        std::vector<eo::point> const points = eo::shapes::general_position(draw, count, limit);
        EO_CHECK_EQ(static_cast<long long>(points.size()), count);
        EO_CHECK(inside_box(points, limit));
        EO_CHECK(no_three_collinear(points));
    }
    std::vector<eo::point> const many = eo::shapes::general_position(draw, 200000, 1000000000);
    EO_CHECK(inside_box(many, 1000000000));
    std::vector<std::pair<long long, long long>> pairs;
    for (eo::point const& one : many) pairs.push_back({one.x, one.y});
    std::sort(pairs.begin(), pairs.end());
    EO_CHECK(std::adjacent_find(pairs.begin(), pairs.end()) == pairs.end());
    EO_CHECK(inside_box(eo::shapes::general_position(draw, 50, 9223372036854775807LL), 9223372036854775807LL));
    std::string const drawn = points_of(eo::shapes::general_position(draw, 12, 1000));
    pin_bytes(drawn, 105, 0x0b8f3aab6b410d6cull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::general_position(draw, 12, 5); }),
                "general_position fits at most 11 points inside +-5, not 12");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::general_position(draw, 3, 0); }),
                "points in general position needs a coordinate bound of at least 1, not 0");
}
