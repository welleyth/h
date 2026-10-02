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
| `tree(draw, n, name)` | any of `random`, `uniform`, `path`, `star`, `caterpillar`, `broom`, `binary`, `dumbbell` by name | — |

**`random_tree` is not "a random tree".** It is a random recursive tree, and its depth is
Θ(log n) — 13 at n = 1,000, 25 at n = 100,000. It will never stress a recursive DFS and never
produce a long path. It is a good default case and one shape, not the shape.

**`deep_tree` cannot give you a bamboo either.** Even `lean = 50` reaches depth about 429 out
of 100,000. If you need Θ(n) depth, that is `path`.

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
| `many_components(draw, n, pieces)` | disjoint pieces | "assume connected" |
| `dag(draw, n, m)` | a hidden topological order, `directed` set | a solution that reads the vertices in input order as a topological order |
| `functional(draw, n, name)` | `f(i)` for each `i`: `cycle`, `rho`, `self` or `random` | a cycle finder that assumes one cycle, or no tails |

**The edge count is exact or it is a jury error.** `connected_graph(draw, 10, 8)` says
*a connected graph on 10 vertices has 9..45 edges, not 8* rather than looping. Filling is
rejection sampling while the graph is sparse and switches to enumerate-and-shuffle once `m`
passes a quarter of `n(n−1)/2`, so a near-complete graph does not stall.

**A DAG never comes out in topological order.** `dag` draws a random order first and emits
every arrow along it, so a solution that ignores the actual sort cannot pass by accident.

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

A uniform random string over a large alphabet is the easy case; `draw.letters(n,
eo::charset("ab"))` over an alphabet of two maximises repeats, borders and periods.

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
| trees | `tree`, `random_tree`, `uniform_tree`, `deep_tree`, `path`, `star`, `caterpillar`, `broom`, `binary_tree`, `kary_tree`, `dumbbell`, `spider` |
| graphs | `connected_graph`, `sparse_graph`, `complete_graph`, `cycle`, `cycle_with_chords`, `grid`, `bipartite_graph`, `complete_bipartite`, `many_components`, `dag`, `functional` |
| sequences | `equal_values`, `few_distinct`, `plateaus`, `nearly_sorted`, `alternating`, `hash_collisions`, `log_uniform`, `near_bounds`, `spikes`, `split_sum` |
| permutations | `permutation`, `permutation_cycles`, `derangement`, `involution`, `with_inversions`, `with_lis` |
| strings | `repeated`, `periodic`, `near_periodic`, `fibonacci_word`, `thue_morse`, `palindrome` |
| geometry | `scattered`, `collinear`, `convex_position`, `cocircular`, `extreme_points` |
| types | `eo::graph` (`n`, `edges`, `directed`), `eo::edge`, `eo::point` |

Everything lives in `eo::shapes::`, except `eo::graph`, `eo::edge` and `eo::point`.
