#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"

namespace eo {
namespace shapes {
namespace detail {

inline void room_for_grid(int rows, int columns, char const* what) {
    if (rows < 1 || columns < 1)
        eo::detail::library_error(fmt("{} is at least 1 by 1, not {} by {}", what, rows, columns));
}

}  // namespace detail

[[nodiscard]] inline std::vector<std::string> maze(rng& draw, int rows, int columns) {
    detail::room_for_grid(rows, columns, "a maze");
    std::vector<std::string> grid(static_cast<std::size_t>(rows), std::string(static_cast<std::size_t>(columns), '#'));
    int const tall = (rows + 1) / 2;
    int const wide = (columns + 1) / 2;
    std::vector<char> seen(static_cast<std::size_t>(tall) * static_cast<std::size_t>(wide), 0);
    auto const room = [wide](int row, int column) {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(wide) + static_cast<std::size_t>(column);
    };
    auto const open = [&](int row, int column) {
        grid[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = '.';
    };
    std::vector<std::pair<int, int>> path{{0, 0}};
    seen[0] = 1;
    open(0, 0);
    int const steps[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    while (!path.empty()) {
        std::pair<int, int> const here = path.back();
        std::vector<std::pair<int, int>> fresh;
        for (auto const& step : steps) {
            int const row = here.first + step[0];
            int const column = here.second + step[1];
            if (row < 0 || column < 0 || row >= tall || column >= wide) continue;
            if (seen[room(row, column)] == 0) fresh.push_back({row, column});
        }
        if (fresh.empty()) {
            path.pop_back();
            continue;
        }
        std::pair<int, int> const next = draw.pick(fresh);
        seen[room(next.first, next.second)] = 1;
        open(here.first + next.first, here.second + next.second);
        open(2 * next.first, 2 * next.second);
        path.push_back(next);
    }
    return grid;
}

[[nodiscard]] inline std::vector<std::string> scattered_walls(rng& draw, int rows, int columns, double density) {
    detail::room_for_grid(rows, columns, "a grid with walls");
    if (!(density >= 0 && density <= 1))
        eo::detail::library_error(fmt("scattered_walls takes a density in 0..1, not {}", density));
    std::vector<char> downward(static_cast<std::size_t>(rows) + static_cast<std::size_t>(columns) - 2, 0);
    for (std::size_t at = 0; at + 1 < static_cast<std::size_t>(rows); at++) downward[at] = 1;
    draw.shuffle(downward);
    std::vector<std::string> grid(static_cast<std::size_t>(rows), std::string(static_cast<std::size_t>(columns), '#'));
    std::vector<char> kept(static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns), 0);
    std::size_t row = 0;
    std::size_t column = 0;
    kept[0] = 1;
    for (char const down : downward) {
        if (down != 0) {
            row++;
        } else {
            column++;
        }
        kept[row * static_cast<std::size_t>(columns) + column] = 1;
    }
    for (std::size_t at = 0; at < kept.size(); at++)
        if (kept[at] != 0 || !draw.chance(density))
            grid[at / static_cast<std::size_t>(columns)][at % static_cast<std::size_t>(columns)] = '.';
    return grid;
}

}  // namespace shapes
}  // namespace eo
