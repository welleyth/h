#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "../core.h"
#include "../fmt.h"
#include "../random.h"
#include "../structure.h"
#include "present.h"
#include "trees.h"

namespace eo {
namespace shapes {
namespace detail {

inline long long pairs_of(int n) {
    return static_cast<long long>(n) * static_cast<long long>(n - 1) / 2;
}

inline void room_for(int n, long long m, long long least, char const* what) {
    at_least(n, 1, what);
    long long const most = pairs_of(n);
    if (m < least || m > most)
        eo::detail::library_error(
            fmt("{} on {} vertices has {}..{} edges, not {}", what, n, least, most, m));
}

class pair_set {
public:
    explicit pair_set(std::size_t most) {
        while ((std::size_t{1} << bits_) < 2 * most + 2) bits_++;
        slots_.assign(std::size_t{1} << bits_, empty);
    }

    bool insert(int u, int v) {
        std::uint64_t const key = pack(u, v);
        std::size_t at = home(key);
        while (slots_[at] != empty) {
            if (slots_[at] == key) return false;
            at = (at + 1) & (slots_.size() - 1);
        }
        slots_[at] = key;
        return true;
    }

    bool contains(int u, int v) const {
        std::uint64_t const key = pack(u, v);
        for (std::size_t at = home(key); slots_[at] != empty; at = (at + 1) & (slots_.size() - 1))
            if (slots_[at] == key) return true;
        return false;
    }

private:
    static std::uint64_t constexpr empty = ~std::uint64_t{0};

    static std::uint64_t pack(int u, int v) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(u)) << 32) | static_cast<std::uint32_t>(v);
    }

    std::size_t home(std::uint64_t key) const {
        return static_cast<std::size_t>((key * 0x9e3779b97f4a7c15ull) >> (64 - bits_));
    }

    int bits_ = 4;
    std::vector<std::uint64_t> slots_;
};

inline std::vector<edge> filled_sparsely(rng& draw, int n, long long m, std::vector<edge> have) {
    pair_set seen(static_cast<std::size_t>(m));
    for (edge const& one : have) seen.insert((std::min)(one.u, one.v), (std::max)(one.u, one.v));
    long long const given = static_cast<long long>(have.size());
    long long tries = 0;
    long long const most = 32 * m + 1000;
    while (static_cast<long long>(have.size()) < m) {
        still_trying(tries, most, "a graph with that many edges");
        int const u = static_cast<int>(draw.uniform(1, n));
        int const v = static_cast<int>(draw.uniform(1, n));
        if (u == v) continue;
        if (!seen.insert((std::min)(u, v), (std::max)(u, v))) continue;
        have.push_back(edge{u, v});
    }
    kept_of(m - given, tries, "a graph with that many edges", eo::detail::site::here());
    return have;
}

inline std::vector<edge> filled_densely(rng& draw, int n, long long m, std::vector<edge> have) {
    pair_set seen(have.size());
    for (edge const& one : have) seen.insert((std::min)(one.u, one.v), (std::max)(one.u, one.v));
    std::vector<edge> spare;
    for (int u = 1; u <= n; u++)
        for (int v = u + 1; v <= n; v++)
            if (!seen.contains(u, v)) spare.push_back(edge{u, v});
    draw.shuffle(spare);
    for (edge const& one : spare) {
        if (static_cast<long long>(have.size()) >= m) break;
        have.push_back(one);
    }
    return have;
}

inline std::vector<edge> filled(rng& draw, int n, long long m, std::vector<edge> have) {
    if (m * 4 > pairs_of(n)) return filled_densely(draw, n, m, std::move(have));
    return filled_sparsely(draw, n, m, std::move(have));
}

}  // namespace detail

[[nodiscard]] inline graph connected_graph(rng& draw, int n, long long m) {
    detail::room_for(n, m, static_cast<long long>(n) - 1, "a connected graph");
    graph made = random_tree(draw, n);
    made.edges = detail::filled(draw, n, m, std::move(made.edges));
    return made;
}

[[nodiscard]] inline graph sparse_graph(rng& draw, int n, long long m) {
    detail::room_for(n, m, 0, "a graph");
    return detail::undirected(n, detail::filled(draw, n, m, {}));
}

[[nodiscard]] inline graph complete_graph(int n) {
    detail::at_least(n, 1, "a complete graph");
    std::vector<edge> edges;
    for (int u = 1; u <= n; u++)
        for (int v = u + 1; v <= n; v++) edges.push_back(edge{u, v});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph cycle(int n) {
    detail::at_least(n, 3, "a cycle");
    std::vector<edge> edges;
    for (int vertex = 2; vertex <= n; vertex++) edges.push_back(edge{vertex - 1, vertex});
    edges.push_back(edge{n, 1});
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph cycle_with_chords(rng& draw, int n, long long chords) {
    detail::at_least(n, 3, "a cycle");
    long long const m = static_cast<long long>(n) + chords;
    detail::room_for(n, m, n, "a cycle with chords");
    graph made = cycle(n);
    made.edges = detail::filled(draw, n, m, std::move(made.edges));
    return made;
}

[[nodiscard]] inline graph grid(int rows, int columns) {
    if (rows < 1 || columns < 1)
        eo::detail::library_error(fmt("a grid is at least 1 by 1, not {} by {}", rows, columns));
    auto const at = [columns](int row, int column) { return (row - 1) * columns + column; };
    std::vector<edge> edges;
    for (int row = 1; row <= rows; row++)
        for (int column = 1; column <= columns; column++) {
            if (column < columns) edges.push_back(edge{at(row, column), at(row, column + 1)});
            if (row < rows) edges.push_back(edge{at(row, column), at(row + 1, column)});
        }
    return detail::undirected(rows * columns, std::move(edges));
}

[[nodiscard]] inline graph bipartite_graph(rng& draw, int left, int right, long long m) {
    if (left < 1 || right < 1)
        eo::detail::library_error(fmt("a bipartite graph has sides of at least 1, not {} and {}", left, right));
    long long const most = static_cast<long long>(left) * static_cast<long long>(right);
    if (m < 0 || m > most)
        eo::detail::library_error(fmt("{} by {} holds 0..{} edges, not {}", left, right, most, m));
    std::vector<edge> edges;
    if (m * 4 > most) {
        std::vector<edge> spare;
        for (int u = 1; u <= left; u++)
            for (int v = 1; v <= right; v++) spare.push_back(edge{u, left + v});
        draw.shuffle(spare);
        spare.resize(static_cast<std::size_t>(m));
        edges = std::move(spare);
    } else {
        detail::pair_set seen(static_cast<std::size_t>(m));
        long long tries = 0;
        long long const ceiling = 32 * m + 1000;
        while (static_cast<long long>(edges.size()) < m) {
            detail::still_trying(tries, ceiling, "a bipartite graph with that many edges");
            int const u = static_cast<int>(draw.uniform(1, left));
            int const v = static_cast<int>(draw.uniform(1, right));
            if (!seen.insert(u, v)) continue;
            edges.push_back(edge{u, left + v});
        }
        detail::kept_of(m, tries, "a bipartite graph with that many edges", eo::detail::site::here());
    }
    return detail::undirected(left + right, std::move(edges));
}

[[nodiscard]] inline graph complete_bipartite(int left, int right) {
    if (left < 1 || right < 1)
        eo::detail::library_error(fmt("a bipartite graph has sides of at least 1, not {} and {}", left, right));
    std::vector<edge> edges;
    for (int u = 1; u <= left; u++)
        for (int v = 1; v <= right; v++) edges.push_back(edge{u, left + v});
    return detail::undirected(left + right, std::move(edges));
}

[[nodiscard]] inline graph many_components(rng& draw, int n, int pieces) {
    detail::at_least(n, 1, "a graph");
    if (pieces < 1 || pieces > n)
        eo::detail::library_error(fmt("{} vertices fall into 1..{} pieces, not {}", n, n, pieces));
    std::vector<long long> const sizes = draw.partition(pieces, n);
    std::vector<edge> edges;
    int first = 1;
    for (long long const size : sizes) {
        graph const piece = random_tree(draw, static_cast<int>(size));
        for (edge const& one : piece.edges) edges.push_back(edge{one.u + first - 1, one.v + first - 1});
        first += static_cast<int>(size);
    }
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph dag(rng& draw, int n, long long m) {
    detail::room_for(n, m, 0, "a dag");
    std::vector<int> const order = draw.perm(n, 1);
    std::vector<edge> edges;
    if (m * 4 > detail::pairs_of(n)) {
        std::vector<edge> spare;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                spare.push_back(edge{order[static_cast<std::size_t>(i)], order[static_cast<std::size_t>(j)]});
        draw.shuffle(spare);
        spare.resize(static_cast<std::size_t>(m));
        edges = std::move(spare);
    } else {
        detail::pair_set seen(static_cast<std::size_t>(m));
        long long tries = 0;
        long long const most = 32 * m + 1000;
        while (static_cast<long long>(edges.size()) < m) {
            detail::still_trying(tries, most, "a dag with that many edges");
            int const i = static_cast<int>(draw.uniform(0, n - 2));
            int const j = static_cast<int>(draw.uniform(i + 1, n - 1));
            if (!seen.insert(i, j)) continue;
            edges.push_back(edge{order[static_cast<std::size_t>(i)], order[static_cast<std::size_t>(j)]});
        }
        detail::kept_of(m, tries, "a dag with that many edges", eo::detail::site::here());
    }
    return graph{n, std::move(edges), true};
}

[[nodiscard]] inline std::vector<int> functional(rng& draw, int n, std::string const& shape) {
    detail::at_least(n, 1, "a functional graph");
    std::vector<int> next(static_cast<std::size_t>(n) + 1, 0);
    std::vector<int> const order = draw.perm(n, 1);
    auto const along = [&](int from, int to) {
        for (int at = from; at < to; at++)
            next[static_cast<std::size_t>(order[static_cast<std::size_t>(at)])] =
                order[static_cast<std::size_t>(at) + 1];
    };
    if (shape == "cycle") {
        along(0, n - 1);
        next[static_cast<std::size_t>(order[static_cast<std::size_t>(n) - 1])] =
            order[0];
    } else if (shape == "rho") {
        int const tail = n > 1 ? static_cast<int>(draw.uniform(1, n - 1)) : 0;
        along(0, n - 1);
        next[static_cast<std::size_t>(order[static_cast<std::size_t>(n) - 1])] =
            order[static_cast<std::size_t>(tail)];
    } else if (shape == "self") {
        for (int vertex = 1; vertex <= n; vertex++) next[static_cast<std::size_t>(vertex)] = vertex;
    } else if (shape == "random") {
        for (int vertex = 1; vertex <= n; vertex++)
            next[static_cast<std::size_t>(vertex)] = static_cast<int>(draw.uniform(1, n));
    } else {
        eo::detail::library_error(
            fmt("\"{}\" is not a functional shape; the names are cycle, rho, self and random", shape));
    }
    next.erase(next.begin());
    return next;
}

}  // namespace shapes
}  // namespace eo
