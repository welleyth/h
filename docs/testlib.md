# Coming from testlib

Most jury programs written today use [testlib](https://github.com/MikeMirzayanov/testlib).
This page puts each testlib call next to what eolymp.h does instead, role by role, and then
lists the differences that change a program's behaviour rather than its spelling. Every name
in the right-hand columns is in the header; the programs below are compiled by `make check`.

## Validator

| testlib | eolymp.h |
| --- | --- |
| `registerValidation(argc, argv);` | `eo::validator v(argc, argv);` |
| `inf.readInt(1, n, "n")` | `v.read_int(1, n, "n")` |
| `inf.readLong(lo, hi, "x")` | `v.read_long(lo, hi, "x")` |
| `inf.readStrictDouble(lo, hi, 1, 6, "x")` | `v.read_real(lo, hi, 1, 6, "x")` |
| `inf.readToken("[a-z]{1,10}", "s")` | `v.read_token(eo::pattern("[a-z]{1,10}"), "s")`, or `v.read_token(1, 10, eo::charset("a-z"), "s")` |
| `inf.readLine("[a-z\\ ]{1,100}", "s")` | `v.read_line(eo::pattern("[a-z ]{1,100}"), "s")`, or `v.read_line(1, 100, eo::charset("a-z "), "s")` |
| `inf.readTokens(n, "[a-z]{1,10}", "s")` | `v.read_tokens(n, eo::pattern("[a-z]{1,10}"), "s")` |
| `inf.readInts(n, 1, 1000000000, "a")` | `v.read_ints(n, 1, 1000000000, "a")` |
| a loop of `inf.readLine("[.#]{m}", "row")` | `v.read_grid(n, m, eo::charset(".#"), "row")` |
| a loop of `inf.readInt(1, n, "u")`, `readSpace`, `inf.readInt(1, n, "v")`, `readEoln` | `v.read_edges(m, n, "edge")`, a `std::vector<eo::edge>` |
| the same loop with `inf.readInt(1, c, "w")` after `v` | `v.read_edges(m, n, eo::weighted(1, c), "edge")`, a `std::vector<eo::weighted_edge>` |
| `inf.readSpace()`, `inf.readEoln()`, `inf.readEof()` | `v.read_space()`, `v.read_eoln()`, `v.read_eof()` |
| `inf.readChar(':')` | `v.read_char(':')` |
| `inf.eoln()`, `inf.eof()` | `v.at_eoln()`, `v.at_eof()` |
| `ensuref(cond, "n is %d", n)` | `v.require(cond, "n is {}", n)` |
| `format("a[%d]", i)` as a name | `eo::element("a", i)` |
| `validator.group()` | `v.group()`, a `std::optional<int>` |
| `pattern p("[a-z]{1,10}"); p.matches(s)` | `eo::pattern p("[a-z]{1,10}"); p.matches(s)`; see [Patterns](validator.md#patterns) for where the two dialects differ |

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int n = v.read_int(1, 200000, "n");
    v.read_eoln();
    std::vector<int> a = v.read_ints(n, 1, 1000000000, "a");
    v.read_eoln();
    v.require(eo::all_distinct(a), "a");
}
```

A pattern keeps its text, and six details of the dialect differ, listed under
[Patterns](validator.md#patterns): testlib drops an unquoted space, so its `[a-z ]` must be
written `[a-z\ ]`, where here either spelling is a space. A charset and a length say the
same as `[a-z]{1,10}` and say it faster. `v.read_eof()` is optional, because the library checks the end of the
input itself. See [validator.md](validator.md).

## Checker

| testlib | eolymp.h |
| --- | --- |
| `registerTestlibCmd(argc, argv);` | `eo::checker c(argc, argv);` |
| `inf`, `ouf`, `ans` | `c.input`, `c.output`, `c.jury` |
| `ouf.readInt(1, n, "k")` | `c.output.read_int(1, n, "k")` |
| `ans.readLong()` | `c.jury.read_long(eo::any, "sum")`: bounds, or `eo::any` to say there are none |
| `ouf.readToken("[a-z]{1,5}", "w")`, `ouf.readLine(...)`, `ouf.readTokens(k, ...)` with a pattern | `c.output.read_token(eo::pattern("[a-z]{1,5}"), "w")`, `read_line` and `read_tokens` with an `eo::pattern`, on any of the three streams |
| `quitf(_ok, "...")` | `eo::accept("...")` |
| `quitf(_wa, "got %d", x)`, `quitf(_pe, ...)` | `eo::wrong("got {}", x)`; Eolymp has no presentation error |
| `quitf(_fail, ...)` | `eo::jury_error(...)` |
| a loop of `ouf.readInt(1, n, "u")`, `ouf.readInt(1, n, "v")` | `c.output.read_edges(k, n, "edge")` |
| that loop over `n - 1` edges, then a union-find to see they form a tree | `c.output.read_tree(n, "edge")`; `read_graph(n, m, eo::connected, "edge")` for a graph |
| `ouf.quitf(_wa, ...)` | `c.output.wrong(...)`, blamed on that stream |
| `quitp(p)` | `eo::score(f)` for a fraction of the test, or `eo::points(p)` for points |
| a `readAns(ouf)` / `readAns(ans)` pair, then `quitf(_fail)` if the contestant beats the jury | `c.read_both(reader)` and `c.optimum(by_the_jury, found, eo::minimize)` |
| `doubleCompare(expected, result, 1e-9)` | `eo::close_enough(expected, found, 1e-9)` |
| `doubleCompare` on an optimum, then `quitf(_fail)` if the contestant's is better | `c.optimum(by_the_jury, found, eo::minimize, eo::within(1e-9))` |
| `wcmp` | `c.tokens()` |
| `rcmp6`, `rcmp9` | `c.reals(1e-6)`, `c.reals(1e-9)` |
| `lcmp` | `c.lines(eo::exact)`, which keeps blank lines as `lcmp` does, and compares a line character by character where `lcmp` compares its words |

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    int n = c.input.read_int(1, 100000, "n");
    auto chosen = [n](eo::answer_stream& a) {
        int k = a.read_int(0, n, "k");
        std::vector<int> items = a.read_ints(k, 1, n, "item");
        if (!eo::all_distinct(items)) a.wrong("an item is chosen twice");
        return k;
    };
    c.answers(eo::many);
    auto [by_the_jury, found] = c.read_both(chosen);
    c.optimum(by_the_jury, found, eo::maximize);
}
```

`c.tokens()` compares tokens as text, as `wcmp` does; a checker that compares numbers, as
`ncmp` does, reads them with `read_long`. Nothing needs `ouf.seekEof()`: text after the
answer is a wrong answer unless the stream says `trailing(eo::ignore)`. See
[checker.md](checker.md).

## Interactor

| testlib | eolymp.h |
| --- | --- |
| `registerInteraction(argc, argv);` | `eo::interactor it(argc, argv);` |
| `inf`, `ans`, `ouf` | `it.input`, `it.jury`, `it.contestant` |
| `cout << x << endl;` | `it.send(x);`, flushed before the next read |
| `tout << ...` for the checker, which reads it back | `eo::accept`, `eo::score` or `eo::points` write the summary, and the checker is `eo::checker(argc, argv).from_interactor();` |
| `quitf(_wa, ...)` | `eo::wrong(...)` |

See [interactor.md](interactor.md).

## Generator

| testlib | eolymp.h |
| --- | --- |
| `registerGen(argc, argv, 1);` | `eo::generator g(argc, argv);` |
| `opt<int>("n")` | `g.option<int>("n", 1, 200000)`, with its range |
| `opt<int>("n", 10)` | `g.option<int>("n", 1, 200000, 10)` |
| `has_opt("m") ? opt<int>("m") : n - 1` | `g.option<std::optional<int>>("m", 0, 200000).value_or(n - 1)` |
| `atoi(argv[1])`, `opt<int>(1)` | no positional arguments: `-n=10` |
| `rnd.next(a, b)` | `r.uniform(a, b)` |
| `rnd.next(n)` | `r.uniform(0, n - 1)` |
| `rnd.next(0.0, 1.0)` | `r.real(0.0, 1.0)` |
| `rnd.wnext(a, b, t)` | `r.weighted(a, b, t)` |
| `rnd.any(v)` | `r.pick(v)` |
| `shuffle(v.begin(), v.end())` | `r.shuffle(v)` |
| `rnd.perm(n, 1)` | `r.perm(n, 1)` |
| `rnd.distinct(k, a, b)` | `r.distinct(k, a, b)` |
| `rnd.partition(k, sum, least)` | `r.partition(k, sum, least)` |
| `rnd.next("[a-z]{5}")` | `r.letters(5, eo::charset("a-z"))` |
| `println(a)`, `cout << a` | `g.out.line(a)` |
| lattice points on a circle, found by a loop over `x` | `eo::shapes::cocircular(r, count, limit)`, from `eolymp-shapes.h` |

`r` is `g.rng()`, the default stream, or `g.rng("label")`, a named one.

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int n = g.option<int>("n", 1, 200000);
    int top = g.option<int>("max", 1, 1000000000, 1000000000);
    eo::rng& r = g.rng("values");
    std::vector<long long> a = r.ints(n, 1, top);
    r.shuffle(a);
    g.out.line(n);
    g.out.line(a);
}
```

See [generator.md](generator.md).

## What behaves differently

- **A generator's arguments are all `-name=value`.** testlib accepts `-n 10`, `--n=10` and
  positional arguments read with `argv[1]` or `opt<int>(1)`; eolymp.h refuses each of them
  and every option it was not told about, before writing anything. A problem moved from
  testlib therefore needs its stored generator arguments rewritten, `gen 10 3` becoming
  `gen -n=10 -k=3`. The one positional argument allowed is the seed a stress run appends.
- **The same seed does not give the same numbers.** eolymp.h draws with its own documented
  algorithm, so a test regenerated after the move is a different test of the same shape.
  `r.weighted(a, b, t)` has the distribution of `rnd.wnext(a, b, t)`, the largest of
  `t + 1` draws, but not its values.
- **Qualify the library's names.** testlib's are global, so a ported program is often written
  without a prefix. Writing `using namespace eo;` to get that back, next to
  `using namespace std;`, makes `unique`, `ignore`, `any` and `ratio` ambiguous, and leaves
  `log`, `is_sorted` and `is_permutation` to overload resolution, and a later release may add
  names that clash too, since `using namespace eo;` is outside the compatibility promise; keep
  `eo::` in front of the library's names instead.
- **A read has bounds and a name, or says it has none.** `readInt()` with neither is legal in
  testlib; in eolymp.h it compiles, with warnings EO101 and EO102 (EO103 in a checker), and
  `eo::any` or `eo::unnamed` says that is deliberate.
- **A score is a fraction of the test.** `eo::score(0.5)` on a test worth 10 pays 5 points;
  `eo::points(5)` says the same in points. A score of 2 or more is clamped to full marks and
  called a likely percentage (EO205).
- **A jury answer is read strictly by the checker too.** A malformed answer file is a jury
  error, not a wrong answer, and `c.read_both` holds the jury's answer to the same checks as
  the contestant's.
