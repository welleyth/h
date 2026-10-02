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

    void erase(int u, int v) {
        std::uint64_t const key = pack(u, v);
        std::size_t hole = home(key);
        while (slots_[hole] != key) hole = (hole + 1) & (slots_.size() - 1);
        for (std::size_t probe = (hole + 1) & (slots_.size() - 1); slots_[probe] != empty;
             probe = (probe + 1) & (slots_.size() - 1)) {
            std::size_t const wanted = home(slots_[probe]);
            bool const stays = hole < probe ? hole < wanted && wanted <= probe : hole < wanted || wanted <= probe;
            if (stays) continue;
            slots_[hole] = slots_[probe];
            hole = probe;
        }
        slots_[hole] = empty;
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

inline int block_at(std::vector<edge>& edges, int shared, int made, int size) {
    int previous = shared;
    for (int vertex = made + 1; vertex < made + size; vertex++) {
        edges.push_back(edge{previous, vertex});
        previous = vertex;
    }
    if (size > 2) edges.push_back(edge{previous, shared});
    return made + size - 1;
}

inline graph walked(rng& draw, int n, long long m, bool closed, char const* what) {
    at_least(n, closed ? 3 : 2, what);
    long long const least = closed ? n : n - 1;
    long long const most = (std::max)(least, pairs_of(n) / 2);
    if (m < least || m > most)
        eo::detail::library_error(fmt("{} through all {} vertices of a simple graph has {}..{} edges, not {}{}", what,
                                      n, least, most, m,
                                      m > most ? fmt("; above half of the {} pairs, a random walk stalls", pairs_of(n))
                                               : ""));
    for (;;) {
        std::vector<int> const order = draw.perm(n, 1);
        pair_set seen(static_cast<std::size_t>(m));
        std::vector<int> degree(static_cast<std::size_t>(n) + 1, 0);
        std::vector<edge> edges;
        auto const join = [&](int u, int v) {
            seen.insert((std::min)(u, v), (std::max)(u, v));
            degree[static_cast<std::size_t>(u)]++;
            degree[static_cast<std::size_t>(v)]++;
            edges.push_back(edge{u, v});
        };
        for (std::size_t at = 1; at < order.size(); at++) join(order[at - 1], order[at]);
        int here = order.back();
        bool fine = true;
        while (fine && static_cast<long long>(edges.size()) < (closed ? m - 1 : m)) {
            int next = 0;
            if (2 * degree[static_cast<std::size_t>(here)] < n - 1) {
                do next = static_cast<int>(draw.uniform(1, n));
                while (next == here || seen.contains((std::min)(here, next), (std::max)(here, next)));
            } else {
                std::vector<int> open;
                for (int other = 1; other <= n; other++)
                    if (other != here && !seen.contains((std::min)(here, other), (std::max)(here, other)))
                        open.push_back(other);
                next = open.empty() ? 0 : draw.pick(open);
            }
            fine = next != 0;
            if (fine) {
                join(here, next);
                here = next;
            }
        }
        fine = fine && here != order.front();
        if (closed) fine = fine && !seen.contains((std::min)(here, order.front()), (std::max)(here, order.front()));
        if (!fine) continue;
        if (closed) join(here, order.front());
        return undirected(n, std::move(edges));
    }
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

[[nodiscard]] inline graph perfect_matching(rng& draw, int side, long long m) {
    if (side < 1)
        eo::detail::library_error(fmt("a perfect matching needs at least 1 vertices on a side, not {}", side));
    long long const most = static_cast<long long>(side) * side;
    if (m < side || m > most)
        eo::detail::library_error(fmt(
            "a {} by {} bipartite graph with a perfect matching has {}..{} edges, not {}", side, side, side, most, m));
    std::vector<int> const partner = draw.perm(side, 1);
    detail::pair_set seen(static_cast<std::size_t>(m));
    std::vector<edge> edges;
    for (int u = 1; u <= side; u++) {
        seen.insert(u, partner[static_cast<std::size_t>(u) - 1]);
        edges.push_back(edge{u, side + partner[static_cast<std::size_t>(u) - 1]});
    }
    if (m * 4 > most) {
        std::vector<edge> spare;
        for (int u = 1; u <= side; u++)
            for (int v = 1; v <= side; v++)
                if (!seen.contains(u, v)) spare.push_back(edge{u, side + v});
        draw.shuffle(spare);
        spare.resize(static_cast<std::size_t>(m - side));
        edges.insert(edges.end(), spare.begin(), spare.end());
    } else {
        while (static_cast<long long>(edges.size()) < m) {
            int const u = static_cast<int>(draw.uniform(1, side));
            int const v = static_cast<int>(draw.uniform(1, side));
            if (seen.insert(u, v)) edges.push_back(edge{u, side + v});
        }
    }
    return detail::undirected(2 * side, std::move(edges));
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

[[nodiscard]] inline graph regular_graph(rng& draw, int n, int k) {
    detail::at_least(n, 1, "a regular graph");
    if (k < 0 || k > n - 1)
        eo::detail::library_error(fmt("a regular graph on {} vertices has degree 0..{}, not {}", n, n - 1, k));
    if (static_cast<long long>(n) * k % 2 != 0)
        eo::detail::library_error(
            fmt("a {}-regular graph on {} vertices has {}/2 edges; n times k must be even", k, n,
                static_cast<long long>(n) * k));
    bool const flipped = 2 * k > n - 1;
    int const degree = flipped ? n - 1 - k : k;
    std::vector<edge> edges;
    for (int u = 0; u < n; u++)
        for (int step = 1; step <= degree / 2; step++) edges.push_back(edge{u + 1, (u + step) % n + 1});
    if (degree % 2 == 1)
        for (int u = 0; u < n / 2; u++) edges.push_back(edge{u + 1, u + n / 2 + 1});
    detail::pair_set seen(edges.size());
    for (edge const& one : edges) seen.insert((std::min)(one.u, one.v), (std::max)(one.u, one.v));
    long long const swaps = 10 * static_cast<long long>(edges.size());
    for (long long attempt = 0; attempt < swaps; attempt++) {
        std::size_t const i = static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(edges.size()) - 1));
        std::size_t const j = static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(edges.size()) - 1));
        edge const first = edges[i];
        edge second = edges[j];
        if (draw.chance(0.5)) std::swap(second.u, second.v);
        if (first.u == second.u || first.u == second.v || first.v == second.u || first.v == second.v) continue;
        if (seen.contains((std::min)(first.u, second.v), (std::max)(first.u, second.v)) ||
            seen.contains((std::min)(second.u, first.v), (std::max)(second.u, first.v)))
            continue;
        seen.erase((std::min)(first.u, first.v), (std::max)(first.u, first.v));
        seen.erase((std::min)(second.u, second.v), (std::max)(second.u, second.v));
        edges[i] = edge{first.u, second.v};
        edges[j] = edge{second.u, first.v};
        seen.insert((std::min)(first.u, second.v), (std::max)(first.u, second.v));
        seen.insert((std::min)(second.u, first.v), (std::max)(second.u, first.v));
    }
    if (!flipped) return detail::undirected(n, std::move(edges));
    std::vector<edge> missing;
    for (int u = 1; u <= n; u++)
        for (int v = u + 1; v <= n; v++)
            if (!seen.contains(u, v)) missing.push_back(edge{u, v});
    return detail::undirected(n, std::move(missing));
}

[[nodiscard]] inline graph cactus(rng& draw, int n, int longest) {
    detail::at_least(n, 1, "a cactus");
    if (longest < 2)
        eo::detail::library_error(
            fmt("cactus needs a longest cycle of at least 2, where 2 builds bridges alone, not {}", longest));
    std::vector<edge> edges;
    int made = 1;
    while (made < n) {
        int const at = static_cast<int>(draw.uniform(1, made));
        int const length = static_cast<int>(draw.uniform(2, (std::min)(longest, n - made + 1)));
        int previous = at;
        for (int vertex = made + 1; vertex < made + length; vertex++) {
            edges.push_back(edge{previous, vertex});
            previous = vertex;
        }
        if (length > 2) edges.push_back(edge{previous, at});
        made += length - 1;
    }
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph with_bridges(rng& draw, int n, int bridges) {
    detail::at_least(n, 1, "a graph with bridges");
    if (bridges < 0 || bridges > n - 1)
        eo::detail::library_error(
            fmt("a connected graph on {} vertices has 0..{} bridges, not {}", n, n - 1, bridges));
    int const rest = n - 1 - bridges;
    if (rest == 1)
        eo::detail::library_error(fmt("a connected graph on {} vertices never has exactly {} bridges; ask for {}{}", n,
                                      bridges, n - 1, n >= 3 ? fmt(", or for at most {}", n - 3) : ""));
    std::vector<int> sizes(static_cast<std::size_t>(bridges), 2);
    if (rest > 0)
        for (long long const extra : draw.partition(draw.uniform(1, rest / 2), rest, 2))
            sizes.push_back(static_cast<int>(extra) + 1);
    draw.shuffle(sizes);
    std::vector<edge> edges;
    int made = 1;
    for (int const size : sizes) made = detail::block_at(edges, static_cast<int>(draw.uniform(1, made)), made, size);
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph with_cut_vertices(rng& draw, int n, int cuts) {
    detail::at_least(n, 1, "a graph with cut vertices");
    int const most = (std::max)(0, n - 2);
    if (cuts < 0 || cuts > most)
        eo::detail::library_error(
            fmt("a connected graph on {} vertices has 0..{} cut vertices, not {}", n, most, cuts));
    if (n == 1) return detail::undirected(1, {});
    int const blocks = cuts == 0 ? 1 : static_cast<int>(draw.uniform(cuts + 1, n - 1));
    std::vector<long long> const sizes = draw.partition(blocks, n - 1);
    std::vector<char> fresh(static_cast<std::size_t>(blocks), 0);
    if (cuts > 0) {
        fresh[1] = 1;
        for (long long const at : draw.distinct(cuts - 1, 2, blocks - 1)) fresh[static_cast<std::size_t>(at)] = 1;
    }
    std::vector<edge> edges;
    std::vector<int> plain;
    std::vector<int> shared_ones;
    int made = 0;
    for (std::size_t at = 0; at < sizes.size(); at++) {
        int shared = 1;
        if (at == 0) {
            plain.push_back(1);
            made = 1;
        } else if (fresh[at] != 0) {
            std::size_t const which =
                static_cast<std::size_t>(draw.uniform(0, static_cast<long long>(plain.size()) - 1));
            shared = plain[which];
            plain[which] = plain.back();
            plain.pop_back();
            shared_ones.push_back(shared);
        } else {
            shared = draw.pick(shared_ones);
        }
        int const before = made;
        made = detail::block_at(edges, shared, made, static_cast<int>(sizes[at]) + 1);
        for (int vertex = before + 1; vertex <= made; vertex++) plain.push_back(vertex);
    }
    return detail::undirected(n, std::move(edges));
}

[[nodiscard]] inline graph euler_circuit(rng& draw, int n, long long m) {
    return detail::walked(draw, n, m, true, "an Euler circuit");
}

[[nodiscard]] inline graph euler_path(rng& draw, int n, long long m) {
    return detail::walked(draw, n, m, false, "an Euler path");
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
