namespace {

struct open_cells {
    long long cells = 0;
    long long joins = 0;
    long long reached = 0;
    int most_neighbours = 0;
    long long farthest = 0;
};

open_cells survey(std::vector<std::string> const& grid) {
    open_cells found;
    int const rows = static_cast<int>(grid.size());
    int const columns = rows == 0 ? 0 : static_cast<int>(grid[0].size());
    auto const open = [&](int r, int c) {
        return r >= 0 && c >= 0 && r < rows && c < columns &&
               grid[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] == '.';
    };
    auto const cell = [&](int r, int c) {
        return static_cast<std::size_t>(r) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(c);
    };
    int const steps[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < columns; c++) {
            if (!open(r, c)) continue;
            found.cells++;
            int neighbours = 0;
            for (auto const& step : steps) neighbours += open(r + step[0], c + step[1]) ? 1 : 0;
            found.most_neighbours = std::max(found.most_neighbours, neighbours);
            found.joins += neighbours;
        }
    found.joins /= 2;
    if (!open(0, 0)) return found;
    std::vector<long long> distance(static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns), -1);
    std::vector<std::pair<int, int>> waiting{{0, 0}};
    distance[0] = 0;
    for (std::size_t at = 0; at < waiting.size(); at++) {
        auto const [r, c] = waiting[at];
        long long const here = distance[cell(r, c)];
        found.farthest = std::max(found.farthest, here);
        for (auto const& step : steps) {
            int const nr = r + step[0];
            int const nc = c + step[1];
            if (!open(nr, nc)) continue;
            long long& there = distance[cell(nr, nc)];
            if (there >= 0) continue;
            there = here + 1;
            waiting.push_back({nr, nc});
        }
    }
    found.reached = static_cast<long long>(waiting.size());
    return found;
}

bool shaped_as(std::vector<std::string> const& grid, int rows, int columns) {
    if (static_cast<int>(grid.size()) != rows) return false;
    for (std::string const& row : grid)
        if (static_cast<int>(row.size()) != columns || row.find_first_not_of(".#") != std::string::npos) return false;
    return true;
}

}  // namespace

EO_TEST(a_maze_is_a_spanning_tree_of_its_open_cells) {
    eo::rng draw(701);
    for (int rows = 1; rows <= 12; rows++)
        for (int columns = 1; columns <= 12; columns++) {
            std::vector<std::string> const maze = eo::shapes::maze(draw, rows, columns);
            EO_CHECK(shaped_as(maze, rows, columns));
            open_cells const found = survey(maze);
            EO_CHECK_EQ(found.joins, found.cells - 1);
            EO_CHECK_EQ(found.reached, found.cells);
            EO_CHECK_EQ(found.cells >= ((rows + 1) / 2) * ((columns + 1) / 2), true);
        }
    std::vector<std::string> const big = eo::shapes::maze(draw, 999, 999);
    open_cells const found = survey(big);
    EO_CHECK_EQ(found.joins, found.cells - 1);
    EO_CHECK_EQ(found.reached, found.cells);
    EO_CHECK(found.farthest > 20000);
    EO_CHECK_EQ(big[998][998], '.');
    std::string drawn;
    for (std::string const& row : eo::shapes::maze(draw, 7, 9)) drawn += row + "/";
    pin_bytes(drawn, 70, 0x703631a7fd255e00ull, __FILE__, __LINE__);
    EO_CHECK_IN(refusal([&] { (void)eo::shapes::maze(draw, 0, 5); }), "a maze is at least 1 by 1, not 0 by 5");
}
