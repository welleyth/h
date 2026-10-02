# Test shapes

`eolymp-shapes.h` is the second header. It holds the shapes a generator draws from — trees,
graphs, sequences, strings and points — and the relabelling that a hand-written generator
forgets. It is opt-in: a program that does not include it pays nothing for it.

```cpp
#include <eolymp.h>
#include <eolymp-shapes.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int n = g.option<int>("n", 1, 200000);
    std::string kind = g.option<std::string>("shape", {"random", "path", "star", "caterpillar"}, "random");

    eo::graph tree = eo::shapes::tree(g.rng("tree"), n, kind);
    std::vector<long long> w = g.rng("weights").ints(n - 1, 1, 1000000000);
    std::vector<eo::edge> edges = eo::shapes::presented(g.rng("labels"), tree);

    g.out.line(n);
    for (int at = 0; at + 1 < n; at++)
        g.out.line(edges[at].u, edges[at].v, w[at]);
}
```

The generator attaches neither file. Both are in the judge's C++ runtime, beside each other
in `/usr/include/`, so including them is all it takes:

```cpp
#include <eolymp.h>
#include <eolymp-shapes.h>
```

## The rule that applies to every graph

**Relabel the vertices, turn the endpoints and shuffle the edge list before printing.** Skip
it and every tree you emit is rooted at 1 with `parent < child` in construction order, so a
wrong solution that happens to process vertices in input order passes, and an `O(n²)`
solution may never reach its worst case. The tree *is* random; its presentation is not, and
that is invisible in review.

`presented` is that step, and it is not optional advice here — it is where a shape becomes an
edge list:

```cpp
std::vector<eo::edge> edges = eo::shapes::presented(draw, made);
```

It relabels through a random permutation, swaps the endpoints of about half the edges, and
shuffles the list. A graph built by `dag` carries `directed`, and `presented` then relabels
and shuffles but leaves each arrow pointing the way it was built.

A source, a sink or a root that the statement fixes keeps its label through
`presented(draw, made, {1, made.n})`: the vertices listed stay where they are, and every other
vertex is relabelled among the rest. A weighted graph, `eo::weighted_graph`, is presented the
same way, each weight travelling with its edge.

If the format is a parent array rather than an edge list, the edges cannot be shuffled, but
the construction order must still be hidden. `parent_array` relabels within the constraint
that a parent comes before its child:

```cpp
std::vector<int> parents = eo::shapes::parent_array(draw, made);
for (int at = 0; at + 1 < n; at++) g.out.line(parents[at]);
```

## What is secret

The shapes are public by design, as testlib and jngen are: an author reads exactly what
`caterpillar` builds, and a contestant can read it too. What makes a test unknowable is the
seed it is drawn from, never the algorithm, so a generator that anyone could re-run with the
same arguments gives away its tests whatever shapes it calls. [generator.md](generator.md)
says what the seed is made of and how to keep it secret.

## How the topics read

Each topic opens with what its tests must cover, then one row per call: what it gives, and
the wrong solution it is there to kill. A row's call is the whole signature; everything is in
`eo::shapes::`, and every call that takes `draw` takes it first.

## Trees

A tree problem needs the two depth extremes, a path for recursion depth and a star for
degree, then the shapes between them that break a solution tuned to one of the two:
caterpillars and brooms for "long and wide at once", balanced trees for logarithmic depth,
and a uniform tree for everything typical. Every tree goes through `presented` or
`parent_array` before it is printed.

| Call | Gives | Kills |
| --- | --- | --- |
| `random_tree(draw, n)` | the three-line random *recursive* tree: depth Θ(log n), bushy | nothing in particular; the typical case |
| `uniform_tree(draw, n)` | uniform over all n^(n-2) labelled trees, through Prüfer: diameter Θ(√n) | a solution only ever run on recursive trees |
| `deep_tree(draw, n, lean)` | the parent drawn toward the last vertex; larger `lean` is deeper | depth assumptions between log n and n |
| `path(n)` | the bamboo: depth n | a recursive DFS on the default stack; O(n · depth) |
| `star(n)` | one vertex of degree n−1 | O(deg²) per vertex; re-scanning a neighbour list per child |
| `caterpillar(draw, n, spine = 0)` | a spine with legs: a long path and a high degree at once | a solution fast on the path and on the star but not on both |
| `broom(n, handle = 0)` | a path, then a star at its end | the same, with the degree at the bottom of the path |
| `binary_tree(n)`, `kary_tree(n, k)` | perfectly balanced, depth log n | an off-by-one in level arithmetic; binary lifting tables one level short |
| `dumbbell(n)` | two hubs joined by an edge; at even n it has exactly two centroids | a centroid search that assumes one centroid |
| `spider(n, legs)` | legs of equal length from one centre | diameter through the centre; ties between equally deep leaves |
| `tree_from_pruefer(code)` | the tree whose Prüfer code is `code`, on `code.size() + 2` vertices | — ; the decoder the next two use, and [`tree_at`](#small-exhaustive-tests) |
| `tree_from_degrees(draw, degrees)` | vertex `i` has degree `degrees[i − 1]` exactly; uniform over such trees | a solution that only meets the degree sequences random trees have |
| `tree_with_leaves(draw, n, leaves)` | exactly `leaves` leaves, from 2 (a path) to n − 1 (a star) | leaf-counting off by one; "a leaf is a vertex of degree 1" forgotten at the root |
| `tree_with_diameter(draw, n, d)` | a diameter of exactly `d`: a path of `d` edges, everything else hung within reach of its middle | an answer of "n − 1" or "about 2 log n"; a diameter by two BFS that starts its second sweep from the wrong end |
| `tree_with_height(draw, n, h)` | rooted at vertex 1, its deepest vertex exactly `h` below it | depth arrays one short; binary lifting with too few levels at the height that needs one more |
| `bounded_degree_tree(draw, n, most)` | a random recursive tree in which no vertex has more than `most` neighbours, and some has `most` | a solution correct only for binary trees given a ternary one, or that sizes its per-vertex arrays by a smaller degree |
| `comb(n)` | a spine of ⌈n/2⌉ vertices, each with one tooth | a heavy-light decomposition that takes any child as heavy: a coin flip at every spine vertex, and Θ(n) chains on the path down it |
| `staircase(n)` | a spine down from vertex 1 whose every vertex carries a branch two longer than the one below, about √n steps | ranking children by height instead of size, the long-path decomposition and small-to-large by depth: Θ(√n) light edges from the root, where by size there are at most log n |
| `tree(draw, n, name)` | any of `random`, `uniform`, `path`, `star`, `caterpillar`, `broom`, `binary`, `dumbbell`, `comb`, `staircase` by name | — |

**`random_tree` is not "a random tree".** It is a random recursive tree, and its depth is
Θ(log n) — 13 at n = 1,000, 25 at n = 100,000. It will never stress a recursive DFS and never
produce a long path. It is a good default case and one shape, not the shape.

**`deep_tree` cannot give you a bamboo either.** Even `lean = 50` reaches depth about 429 out
of 100,000. If you need Θ(n) depth, that is `path`.

A tree with a root keeps it through the relabelling with `parent_array(draw, made, 1)`,
which prints vertex 1 as the root; `tree_with_height` measures its height from there.

`tree(draw, n, name)` exists so that one generator covers several shapes and the generation
script documents the plan by itself. A name it does not know is a jury error, not a silent
default.

## Graphs

A graph problem needs the density extremes, a tree-like sparse graph and a complete one, a
disconnected case unless the statement forbids it, and the structures its algorithm
reasons about: cycles for anything that works on trees, bipartite graphs for parity, a DAG
for ordering. Every graph goes through `presented` before it is printed.

| Call | Gives | Kills |
| --- | --- | --- |
| `connected_graph(draw, n, m)` | a connected simple graph with exactly `m` edges | nothing in particular; the typical case |
| `sparse_graph(draw, n, m)` | a simple graph with exactly `m` edges, connected or not | "the graph is connected" |
| `complete_graph(n)` | the density ceiling | O(n · m) and adjacency matrices built per query |
| `cycle(n)`, `cycle_with_chords(draw, n, chords)` | every degree 2, then just enough cycles to break tree algorithms | "m = n − 1, so it is a tree" |
| `grid(rows, columns)` | planar, diameter √n | an exponential search that only small-width graphs let through |
| `bipartite_graph(draw, left, right, m)`, `complete_bipartite(left, right)` | only cross edges | odd-cycle assumptions |
| `perfect_matching(draw, side, m)` | `side` by `side`, `m` edges: `side` of them a hidden perfect matching, the rest random cross edges | greedy matching without augmenting paths, which on 1,000 by 1,000 with 3,000 edges falls short |
| `many_components(draw, n, pieces)` | disjoint pieces | "assume connected" |
| `dag(draw, n, m)` | a hidden topological order, `directed` set | a solution that reads the vertices in input order as a topological order |
| `regular_graph(draw, n, k)` | every vertex of degree exactly `k`: a circulant mixed by 10·m random edge swaps, or the complement of one when `k` is above half | a greedy that chooses by degree, which here has nothing to choose by; pruned search that relies on small cuts, since random regular graphs have none |
| `cactus(draw, n, longest)` | connected, every edge on at most one cycle: blocks of 2..`longest` vertices, each a bridge or a cycle, glued at random vertices | a cactus solution that handles a cycle hanging from a vertex but not two cycles through the same vertex; tree DP pushed onto cycles |
| `with_bridges(draw, n, bridges)` | connected, exactly `bridges` bridges: that many single edges and some cycles, glued at random vertices | `low[v] >= tin[u]` written where `>` belongs, and the reverse; a bridge search that skips the parent vertex instead of the parent edge |
| `with_cut_vertices(draw, n, cuts)` | connected, exactly `cuts` articulation points: blocks glued so that each new block either creates one or reuses one | the root rule of the articulation-point DFS, which needs two children; `low[v] >= tin[u]` against `>` |
| `euler_circuit(draw, n, m)`, `euler_path(draw, n, m)` | connected and simple, `m` edges, every degree even; or exactly two odd, the path's ends. A random walk that first visits every vertex, so `m` is at most half the pairs | Hierholzer written recursively, `m` deep; an Euler path started at vertex 1 instead of at an odd vertex; a connectivity check forgotten in the other tests |
| `tournament(draw, n)` | every pair joined by one arc, its direction a coin flip, `directed` set | ranking by the number of wins as if it were transitive; a Hamiltonian path search that assumes no cycle. `dag(draw, n, n(n−1)/2)` is the transitive one |
| `with_sccs(draw, n, k, m)` | a digraph with exactly `k` strongly connected components and `m` arcs: each component a random cycle plus arcs inside it, every other arc forward in a hidden order of the components | Tarjan's recursion at its deepest, inside one large component; Kosaraju's second pass run in the wrong order; DP over the condensation that forgets an arc between components |
| `graph_with_diameter(draw, n, m, d)` | connected, `m` edges, diameter exactly `d`: `tree_with_diameter` plus edges only between vertices whose distance from one end of its path differs by at most 1 | an answer read off one BFS, or off a double sweep, which only bounds a graph's diameter from below; distance arrays sized by a smaller diameter |
| `functional(draw, n, name)` | `f(i)` for each `i`: `cycle`, `rho`, `self` or `random` | a cycle finder that assumes one cycle, or no tails |

**The edge count is exact or it is a jury error.** `connected_graph(draw, 10, 8)` says
*a connected graph on 10 vertices has 9..45 edges, not 8* rather than looping. Filling is
rejection sampling while the graph is sparse and switches to enumerate-and-shuffle once `m`
passes a quarter of `n(n−1)/2`, so a near-complete graph does not stall.

**Exact counts of bridges and of cut vertices are what those shapes are for.** A connected
graph on `n` vertices never has exactly `n − 2` bridges, so `with_bridges` refuses it; every
other count from 0 to `n − 1` is built. Their blocks are plain cycles and single edges.

**`graph_with_diameter` keeps both bounds by construction.** The tree it starts from has
diameter `d` and an added edge can only shorten distances, so the diameter is at most `d`;
every added edge joins two vertices whose distances from one end of the tree's longest path
differ by at most one, so those distances do not change, and the other end stays `d` away.
That caps the edges below a complete graph's, and the refusal names the most it builds; `d =
1` is the complete graph itself.

**A DAG never comes out in topological order.** `dag` draws a random order first and emits
every arrow along it, so a solution that ignores the actual sort cannot pass by accident.

## Shortest paths and flows

`eo::weighted_graph` holds `n`, `edges` as `eo::weighted_edge`s `{u, v, w}`, and `directed`.
A shortest-path problem needs weights at the bound, for overflow in the distances; zero
weights where the statement allows them; and the graphs that make a wrong algorithm slow
rather than wrong, which random graphs never are. A flow problem needs a network whose
augmenting paths are many and tangled.

| Call | Gives | Kills |
| --- | --- | --- |
| `with_weights(draw, made, low, high)` | `made` with every edge weighted uniformly in `low..high`, `directed` kept | nothing in particular; the typical case |
| `anti_spfa(draw, n, high)` | a grid ten rows deep and n/10 long, cheap rungs of 1..10 across and heavy edges of 1..`high` along; the source is vertex 1, at a corner | SPFA, Bellman–Ford with a queue: at n = 100,000 it scans 3.1·10^9 edges, 16,000 per edge, where on a random graph of the same size it scans 4 per edge |
| `anti_dijkstra(n)` | vertex 1 reaches a hub through n/2 middle vertices, each path shorter than the one found before it, and the hub carries the other n/2 vertices as leaves; weights below n | Dijkstra without `if (d > dist[v]) continue`: the hub enters the heap n/2 times and each copy rescans its n edges, 5·10^9 scans at n = 100,000. Every edge points away from vertex 1, so `directed = true` keeps it a trap |
| `layered_network(draw, layers, width, high)` | a directed network: vertex 1 feeds `width` vertices, consecutive layers are joined completely with capacities 1..`high`, the last layer drains into vertex `layers · width + 2`; `w` is the capacity | Dinic without the current-arc pointer, which re-explores the dead ends of a layered graph on every push: 5 layers of 30 make it scan 343 million arcs where with the pointer it scans 7,177 |

The traps name their source vertex 1, and `layered_network` its sink `n`; keep them through
the relabelling with `presented(draw, made, {1})` or `presented(draw, made, {1, made.n})`.
Each trap is aimed at one algorithm and is an ordinary input to the others, so a test set
carries it beside random weighted graphs, not instead of them. The traps for Dinic and
SPFA are as hard as these measurements say for the implementations measured; one that
detects them is beyond what a fixed shape can promise.

## Grids

A grid is a `std::vector<std::string>` of `rows` strings of `columns` characters, `.` open and
`#` a wall; `std::replace` turns them into whatever the statement uses. A grid problem needs
the open grid, the longest path a BFS can be made to walk, a maze in which the walls
decide everything, and walls at a density with a path kept through them.

| Call | Gives | Kills |
| --- | --- | --- |
| `maze(draw, rows, columns)` | a perfect maze: rooms at even coordinates joined by a random depth-first search, so the open cells form a tree; `(0, 0)` is open, and so is the far corner when both sizes are odd | flood fill written recursively, as deep as the maze's longest corridor; a BFS that stops at the first dead end |
| `scattered_walls(draw, rows, columns, density)` | each cell a wall with probability `density`, except a random monotone path from `(0, 0)` to the far corner, which stays open | "unreachable" answered by default; a BFS that never meets a wall in the other tests; at `density` 1 only the path is left |
| `serpentine(rows, columns)` | every other row open, joined at alternate ends: one corridor from `(0, 0)` through half the cells | distance arrays or a BFS queue sized by `rows + columns`; recursion along the corridor; an "answer at most `rows · columns / 4`" bound |
| `spiral(rows, columns)` | one corridor from `(0, 0)` spiralling inward, a wall between its laps, ending near the centre | the same, with the far end in the middle rather than at a corner |
| `checkerboard(rows, columns)` | `(r + c)` even open, odd a wall; `(0, 0)` open | 4- against 8-connectivity: ⌈rows·columns/2⌉ components under one and a single component under the other; DSU without union by size at its most components |

## Sequences

An array problem needs all values equal, all values at the bound for overflow, few distinct
values for ties, sorted and reverse-sorted input for anything that pivots, and a random case
of each size.

| Call | Gives | Kills |
| --- | --- | --- |
| `equal_values(count, value)` | one value everywhere | solutions that assume distinct; at `value` = the bound, the overflow bait `n × V` |
| `few_distinct(draw, count, kinds, low, high)` | ties everywhere | a strict comparison where a non-strict one belongs |
| `plateaus(draw, count, runs, low, high)` | long runs of one value | run-length bugs at the boundaries of a run |
| `nearly_sorted(draw, count, low, high, swaps)` | sorted with `swaps` random swaps | an "if sorted, done" shortcut; a naive quicksort |
| `alternating(draw, count, low, high)` | low, high, low, high | "merge adjacent" heuristics |
| `hash_collisions(count, buckets = 107897)` | multiples of one bucket count | an `unordered_map` with the default hash |
| `log_uniform(draw, count, low, high)` | every decimal length from `low`'s to `high`'s equally likely, so short numbers are as common as long ones; `low` is at least 0 | a solution only ever run on values near the bound, as uniform values are: digit DP at short lengths, `log` and `sqrt` estimates |
| `near_bounds(draw, count, low, high, spread)` | each value within `spread` of `low` or of `high`, half and half | off-by-one at either bound; sums and products at the extremes |
| `spikes(draw, count, tall, low, high)` | `tall` values, at random places, from the top sixteenth of the range; the rest from the bottom sixteenth | cost that grows with the maximum: counting sort, DP over the value or the sum, sqrt decomposition by value |
| `split_sum(draw, total, parts, least, most)` | `parts` values in `least..most` adding up to exactly `total` | a multi-test solution that clears its arrays per case in O(limit), not O(n); per-case work that only the sum bounds |
| `distinct_gapped(draw, count, low, high, gap)` | different values, no two closer than `gap`, in a random order; uniform over such sets | a "closest pair" or "minimum distance" answer that is never exactly `gap` in the other tests; values packed densely enough to index an array by |
| `mountain(draw, count, low, high)`, `valley(draw, count, low, high)` | strictly up to one peak, then strictly down, the peak anywhere; and the mirror image | a monotonic stack at its deepest, which grows to the peak and then pops everything; "the maximum is at an end" |

`draw.partition(t, n)` splits a total across test cases, which is how you build the two tests
that catch different bugs: `t = 10^5` cases of `n = 1`, and one case of `n = 10^5`. When each
case also has a ceiling, as in "the sum of `n` is at most 2·10^5 and each `n` at most 10^5",
`split_sum(draw, total, parts, least, most)` is the same split with every part inside
`least..most`: it draws a uniform composition and moves what overflows a part's ceiling to
parts with room, in a random order, so a few parts sit exactly at `most`.

## Permutations

A permutation is returned as `p[0..n−1]` holding `p(1)..p(n)`, the values 1..n. A
permutation problem needs the identity and the reversal, one long cycle and many short ones,
no fixed point and every fixed point, and the structure its answer counts: the cycles for
swaps, the inversions for sorting, the longest increasing subsequence for patience.

| Call | Gives | Kills |
| --- | --- | --- |
| `permutation(draw, n, name)` | any of `random`, `identity`, `reversed`, `cycle`, `derangement`, `involution` by name | — |
| `permutation_cycles(draw, n, k)` | exactly `k` cycles, of random lengths adding up to `n` | an off-by-one in "n − cycles swaps" at `k = 1` and `k = n`; following a cycle recursively, at `k = 1` |
| `derangement(draw, n)` | no fixed point, uniform over all derangements | "some `p(i) = i` exists" |
| `involution(draw, n, fixed)` | `p(p(i)) = i`, with exactly `fixed` fixed points; uniform over those | a cycle walk that mishandles cycles of length 2. Keep a random case too: here `p` is its own inverse, so a solution that confuses `p` with `p⁻¹` passes |
| `with_inversions(draw, n, k)` | exactly `k` inversions, from 0 to n(n−1)/2 | insertion sort and bubble counts, O(n + k), at large `k`; an inversion count in `int`, which overflows past n ≈ 65,536 |
| `with_lis(draw, n, k)` | a longest increasing subsequence of exactly `k` | patience sorting that scans its piles, O(n · k), at large `k`; greedy "extend the last element" answers |

`permutation_cycles` draws the cycle lengths as a random composition of `n`, so they are of
comparable size; a uniformly random permutation instead has one cycle of about `n/2` and
about `ln n` cycles in all. `with_inversions` spreads `k` over the Lehmer code in a random
order, which reaches every permutation with `k` inversions, though not uniformly.
`with_lis` interleaves `k` decreasing runs at random, so no increasing subsequence is longer
than `k`, and plants one of length `k` on their last elements; it reaches most permutations
with that subsequence length, not all of them. Under its
name, `involution` has as few fixed points as `n` allows, 0 or 1.

## Intervals

`eo::interval` holds `l` and `r`, two `long long`s with `l <= r`, read as the closed interval
`[l, r]`. An interval problem needs the three ways two intervals relate, apart, overlapping
and one inside the other, each on its own and mixed; the touching case, where closed and
half-open readings disagree; and the degenerate ones, intervals that are points and
intervals that are all the same.

| Call | Gives | Kills |
| --- | --- | --- |
| `intervals(draw, count, low, high, "random")` | both ends uniform, then ordered | nothing in particular; the typical case |
| `intervals(draw, count, low, high, "disjoint")` | no two share a point | a sweep that assumes something is always open |
| `intervals(draw, count, low, high, "touching")` | sorted, each ends where the next begins | a half-open reading of closed intervals, or the reverse |
| `intervals(draw, count, low, high, "nested")` | each strictly inside the one before it, `count` deep | recursion or a stack as deep as the nesting; "sort by left end, keep the last" |
| `intervals(draw, count, low, high, "laminar")` | any two apart or nested, never crossing, uniform over the nestings | a solution that builds the containment tree and assumes a chain or a forest of single intervals |
| `intervals(draw, count, low, high, "chain")` | each overlaps exactly its two neighbours and contains neither | "overlap is transitive" |
| `intervals(draw, count, low, high, "through")` | every interval contains one common point | output-sensitive work: all count(count − 1)/2 pairs intersect |
| `intervals(draw, count, low, high, "same")` | one interval `count` times | ties in a sort by both ends; deduplication |
| `intervals(draw, count, low, high, "points")` | every interval a single point | `l < r` assumed |

Every family comes out shuffled. The shapes built from different endpoints, `disjoint`,
`touching`, `nested`, `laminar` and `chain`, refuse a range with too few values for them,
and say how many they need.

## Queries

A query problem needs ranges that are long, for a solution that is linear per query; short,
for block-boundary bugs in sqrt and sparse-table structures; prefixes and suffixes for the
ends; single points; and the order of updates and queries mixed as tightly as it can be.

| Call | Gives | Kills |
| --- | --- | --- |
| `ranges(draw, count, n, "random")` | both ends uniform in 1..n, then ordered: a mean length of n/3 | nothing in particular; the typical case |
| `ranges(draw, count, n, "short")` | lengths 1..16 at random places | sqrt decomposition and sparse tables wrong when a range sits inside one block |
| `ranges(draw, count, n, "long")` | lengths of at least n − n/16 | O(length) per query |
| `ranges(draw, count, n, "prefix")`, `ranges(draw, count, n, "suffix")` | every range starts at 1, or ends at n | an off-by-one at either end of a prefix-sum array |
| `ranges(draw, count, n, "point")` | `l = r` | `l < r` assumed; a segment tree that never reaches a leaf |
| `ranges(draw, count, n, "full")` | always `[1, n]` | a root-only shortcut answered wrongly |
| `ranges(draw, count, n, "same")` | one random range `count` times | caching keyed on the wrong thing |
| `query_order(draw, counts, "random")` | kind `i` exactly `counts[i]` times, shuffled | nothing in particular; the typical case |
| `query_order(draw, counts, "grouped")` | all of kind 0, then all of kind 1, and so on | a solution only correct when kinds interleave |
| `query_order(draw, counts, "alternating")` | one of each kind still left, in turn | a structure rebuilt lazily on the first query after an update: every query pays the rebuild |

`ranges` returns `eo::interval`s. `query_order` returns the kinds as numbers from 0, for the
generator to turn into its own lines, so the ratio of updates to queries is exactly what the
counts say.

**Queries that must be answered online are not encoded here.** Hiding a query behind the
previous answer, as `l = (l' xor last) mod n + 1`, needs that answer, which only the reference
solution has; a generator that encodes them embeds the solution, and the encoding is two lines
of it.

## Strings

A string problem needs one letter repeated, which maximises borders and periods, a binary
alphabet for repeats, the words with the most distinct factors for suffix structures, and a
random string over the full alphabet as the easy case.

| Call | Gives | Kills |
| --- | --- | --- |
| `repeated('a', length)` | maximal borders | naive matching, O(n · m) |
| `periodic(unit, length)` | long borders | period arithmetic off by one |
| `near_periodic(draw, unit, length, allowed)` | exactly one mismatch from periodic | a period check that stops at the first match |
| `fibonacci_word(length)`, `thue_morse(length)` | many distinct factors | worst cases for suffix structures |
| `palindrome(draw, length, allowed)` | a palindrome | Manacher and palindromic trees at their deepest |
| `de_bruijn(allowed, order)` | every word of length `order` over `allowed` exactly once, as a linear sequence of kᵒʳᵈᵉʳ + order − 1 letters; the least such sequence | a dictionary or trie that assumes repeats; counting distinct substrings of one length, at its most |
| `lyndon(draw, length, allowed)` | a Lyndon word, strictly smaller than each of its rotations: the least rotation of a random primitive word | a minimal-rotation or Duval implementation that is off by one when the answer is the whole word |
| `abacaba(length)` | `abacabadabacaba…`: letter `i`, counting from 1, is the number of times 2 divides `i`, capped at `z`; the Zimin words | quadratic work on nested borders and squares, where every prefix of length 2ᵏ − 1 is a palindrome made of two copies around one new letter |
| `thue_morse_twins(length)` | the Thue–Morse word and its complement, `a` and `b` swapped, at a length that is a multiple of 1024 | a polynomial hash modulo 2⁶⁴, unsigned overflow, with any odd base, in either direction and with any letter values: the two strings hash the same |

A uniform random string over a large alphabet is the easy case; `draw.letters(n,
eo::charset("ab"))` over an alphabet of two maximises repeats, borders and periods.

## Numbers

A number-theory problem needs the largest prime under the bound, for a solution that trial
divides up to `n`; a semiprime of two large primes, for one that trial divides up to `√n`;
the number with the most divisors, for one that enumerates them; and the composites that
pass weak primality tests. Every call here is exact for every `long long`: primality is
Miller–Rabin with the first twelve primes as bases, which is deterministic below 3·10²³.

| Call | Gives | Kills |
| --- | --- | --- |
| `is_prime(n)` | whether `n` is prime, for any `long long` | — ; the test the others use, for a validator or a checker too |
| `next_prime(n)`, `prev_prime(n)` | the least prime at least `n`, the largest at most `n` | trial division up to `n` at `prev_prime(bound)`, the prime it cannot shortcut |
| `random_prime(draw, low, high)` | a prime in `low..high`, uniform over the primes there | a solution tuned to one prime, such as a hard-coded modulus |

## Geometry

`eo::point` holds two `long long`s. A geometry problem needs collinear points for every sign
test, points on the bounding box for the largest cross products, and points in convex
position for anything that builds a hull.

| Call | Gives | Kills |
| --- | --- | --- |
| `scattered(draw, count, limit)` | random points in a box | nothing in particular; the typical case |
| `collinear(draw, count, limit)` | exactly collinear | cross-product sign assumptions; a hull that drops or keeps collinear points wrongly |
| `convex_position(draw, count, limit)` | points in convex position: the turn never reverses, so none is strictly inside the hull of the others | a hull that is only correct when most points are inside it |
| `cocircular(draw, count)` | exact lattice points on one circle | circle and Delaunay predicates computed in doubles |
| `cocircular(draw, count, limit)` | the same, on a circle of radius at most `limit`, so inside the box; up to 4 points fit any box, on the circle of radius 1 | the same, inside the statement's box |
| `extreme_points(draw, count, limit)` | every point on the edge of the box, so cross products reach 10^18 | cross products in `int`, or in `double` |

**`convex_position` is convex, not strictly convex.** The steps are integer vectors inside a
bounded box, so many of them come out parallel and the points they build are collinear in
threes: measured at 1,000 points in ±1,000 there are 760 collinear triples, and at 100,000 in
±10^6 there are 74,726 — and never a reversed turn. Every point is on the hull's *boundary*,
but the hull has far fewer *vertices* than you asked for. If the statement promises that no
three points are collinear, this is not the generator for it.

**`cocircular` is bounded by arithmetic, not by the library.** Lattice points on a circle are
scarce: the radii this library ships top out at 972 of them, and finding more costs a scan
proportional to the radius. Asking for thousands is a jury error. With a `limit`, the radius
is the same one `cocircular(draw, count)` picks, and so are the points, as long as it fits;
when it does not, that is a jury error too, rather than points outside the statement's box. Larger co-circular sets do
exist under 10^9 — r = 48,612,265 carries 2,916 — but they are not worth the scan.

## What the shapes do not do

- **They do not check the statement.** A shape that the problem forbids is still generated;
  the validator is what rejects it, and running the validator over every generated test is
  the check that matters.
- **They do not choose the plan.** Which shapes a problem admits, and which anti-test belongs
  to which wrong approach, is a question about the problem, not about this header.
- **`scattered` may repeat a point.** That is what random means. `draw.distinct` will not
  help — it draws scalars, not points. For points that must differ, draw distinct scalars in
  `0 .. (2·limit+1)² − 1` and split each into a coordinate pair, or use `convex_position`,
  `collinear` or `extreme_points`, all of which are distinct by construction.

## Reference card

| Group | Names |
| --- | --- |
| presentation | `presented`, `parent_array` |
| shortest paths and flows | `with_weights`, `anti_spfa`, `anti_dijkstra`, `layered_network` |
| trees | `tree`, `random_tree`, `uniform_tree`, `deep_tree`, `path`, `star`, `caterpillar`, `broom`, `binary_tree`, `kary_tree`, `dumbbell`, `spider`, `tree_from_pruefer`, `tree_from_degrees`, `tree_with_leaves`, `tree_with_diameter`, `tree_with_height`, `bounded_degree_tree`, `comb`, `staircase` |
| graphs | `connected_graph`, `sparse_graph`, `complete_graph`, `cycle`, `cycle_with_chords`, `grid`, `bipartite_graph`, `complete_bipartite`, `perfect_matching`, `many_components`, `dag`, `functional`, `regular_graph`, `cactus`, `with_bridges`, `with_cut_vertices`, `euler_circuit`, `euler_path`, `tournament`, `with_sccs`, `graph_with_diameter` |
| grids | `maze`, `scattered_walls`, `serpentine`, `spiral`, `checkerboard` |
| sequences | `equal_values`, `few_distinct`, `plateaus`, `nearly_sorted`, `alternating`, `hash_collisions`, `log_uniform`, `near_bounds`, `spikes`, `split_sum`, `distinct_gapped`, `mountain`, `valley` |
| permutations | `permutation`, `permutation_cycles`, `derangement`, `involution`, `with_inversions`, `with_lis` |
| intervals | `intervals` |
| queries | `ranges`, `query_order` |
| strings | `repeated`, `periodic`, `near_periodic`, `fibonacci_word`, `thue_morse`, `palindrome`, `de_bruijn`, `lyndon`, `abacaba`, `thue_morse_twins` |
| numbers | `is_prime`, `next_prime`, `prev_prime`, `random_prime` |
| geometry | `scattered`, `collinear`, `convex_position`, `cocircular`, `extreme_points` |
| types | `eo::graph` (`n`, `edges`, `directed`), `eo::edge`, `eo::weighted_graph` (`n`, `edges`, `directed`), `eo::weighted_edge`, `eo::point`, `eo::interval` (`l`, `r`) |

Everything lives in `eo::shapes::`, except the types: `eo::graph`, `eo::weighted_graph`, `eo::edge`,
`eo::weighted_edge`, `eo::point` and `eo::interval`.
