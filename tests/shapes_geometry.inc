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

namespace {

int sign_of(long long value) { return value > 0 ? 1 : (value < 0 ? -1 : 0); }

bool segments_meet(eo::point const& a, eo::point const& b, eo::point const& c, eo::point const& d) {
    int const d1 = sign_of(cross_of(c, d, a));
    int const d2 = sign_of(cross_of(c, d, b));
    int const d3 = sign_of(cross_of(a, b, c));
    int const d4 = sign_of(cross_of(a, b, d));
    if (d1 * d2 < 0 && d3 * d4 < 0) return true;
    auto const on = [](eo::point const& p, eo::point const& q, eo::point const& r) {
        return cross_of(p, q, r) == 0 && std::min(p.x, q.x) <= r.x && r.x <= std::max(p.x, q.x) &&
               std::min(p.y, q.y) <= r.y && r.y <= std::max(p.y, q.y);
    };
    return on(c, d, a) || on(c, d, b) || on(a, b, c) || on(a, b, d);
}

bool simple_and_counterclockwise(std::vector<eo::point> const& polygon) {
    std::size_t const n = polygon.size();
    long long twice_area = 0;
    for (std::size_t at = 0; at < n; at++) {
        eo::point const& one = polygon[at];
        eo::point const& next = polygon[(at + 1) % n];
        twice_area += one.x * next.y - one.y * next.x;
    }
    if (twice_area <= 0) return false;
    for (std::size_t i = 0; i < n; i++)
        for (std::size_t j = i + 1; j < n; j++) {
            if (j == i + 1 || (i == 0 && j == n - 1)) continue;
            if (segments_meet(polygon[i], polygon[(i + 1) % n], polygon[j], polygon[(j + 1) % n])) return false;
        }
    return true;
}

}  // namespace

EO_TEST(a_simple_polygon_never_crosses_itself_and_turns_counterclockwise) {
    eo::rng draw(1002);
    for (long long const count : {3LL, 4LL, 5LL, 10LL, 60LL, 300LL})
        for (int round = 0; round < 5; round++) {
            std::vector<eo::point> const polygon = eo::shapes::simple_polygon(draw, count, 1000);
            EO_CHECK_EQ(static_cast<long long>(polygon.size()), count);
            EO_CHECK(inside_box(polygon, 1000));
            EO_CHECK(simple_and_counterclockwise(polygon));
            if (count <= 60) EO_CHECK(no_three_collinear(polygon));
        }
    bool some_concave = false;
    std::vector<eo::point> const wide = eo::shapes::simple_polygon(draw, 2000, 1000000000);
    for (std::size_t at = 0; at < wide.size(); at++)
        if (cross_of(wide[at], wide[(at + 1) % wide.size()], wide[(at + 2) % wide.size()]) < 0) some_concave = true;
    EO_CHECK(some_concave);
    std::string const drawn = points_of(eo::shapes::simple_polygon(draw, 8, 100));
    pin_bytes(drawn, 55, 0x85dd942354fc3b0aull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::simple_polygon(draw, 2, 100); }),
                "a simple polygon has at least 3 vertices, not 2");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::simple_polygon(draw, 10, 2000000000); }),
                "a simple polygon inside +-2000000000 would overflow its own cross products; 1000000000 is the most");
}

EO_TEST(a_strictly_convex_polygon_turns_left_at_every_vertex) {
    eo::rng draw(1003);
    for (long long const limit : {1LL, 3LL, 10LL, 1000LL, 1000000000LL})
        for (long long const count : {3LL, 4LL, 5LL, 7LL, 8LL, 24LL, 1000LL, 100000LL}) {
            std::vector<eo::point> polygon;
            eot::outcome const made = eot::run_until_stop([&] { polygon = eo::shapes::strictly_convex(draw, count, limit); });
            if (made.stopped) {
                EO_CHECK_IN(made.text, "vertices of a strictly convex polygon fit inside");
                continue;
            }
            EO_CHECK_EQ(static_cast<long long>(polygon.size()), count);
            EO_CHECK(inside_box(polygon, limit));
            bool turns = true;
            for (std::size_t at = 0; at < polygon.size(); at++)
                turns = turns && cross_of(polygon[at], polygon[(at + 1) % polygon.size()],
                                          polygon[(at + 2) % polygon.size()]) > 0;
            EO_CHECK(turns);
        }
    EO_CHECK_EQ(eo::shapes::strictly_convex(draw, 24, 10).size(), static_cast<std::size_t>(24));
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::strictly_convex(draw, 25, 10); }),
                "at most 24 vertices of a strictly convex polygon fit inside +-10, not 25");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::strictly_convex(draw, 5, 1); }),
                "at most 4 vertices of a strictly convex polygon fit inside +-1, not 5");
    EO_CHECK_EQ(eo::shapes::strictly_convex(draw, 1000000, 1000000000).size(), static_cast<std::size_t>(1000000));
    std::string const drawn = points_of(eo::shapes::strictly_convex(draw, 10, 100));
    pin_bytes(drawn, 70, 0x759acaffe75d7ce9ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::strictly_convex(draw, 2, 100); }),
                "a strictly convex polygon has at least 3 vertices, not 2");
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::strictly_convex(draw, 10, 2000000000); }),
                "a strictly convex polygon inside +-2000000000 would overflow its own cross products; 1000000000 is the "
                "most");
}
