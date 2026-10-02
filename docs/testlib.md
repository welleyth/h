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
| `inf.readWord()`, `inf.readString()` | `v.read_token(…)`, `v.read_line(…)`, with a length and a charset or a pattern |
| `addFeature("path")`, `feature("path")` | `v.feature("path")`, `v.saw("path")` |
| `format("a[%d]", i)` as a name | `eo::element("a", i)` |
| `validator.group()` | `v.group()`, a `std::optional<int>` |
| `setTestCase(i)` in a loop, `unsetTestCase()` | `v.cases(t, body)` |
| `--testCase k --testCaseFileName case.txt` | `--eo-case=k > case.txt`; see [Pulling one case out](validator.md#pulling-one-case-out) |
| `--testMarkupFileName markup.txt` | `--eo-describe`, whose `eo-describe case k start end` lines give each case's bytes |
| `--testOverviewLogFileName overview.txt` | `--eo-describe`, whose `eo-describe value` lines give each bound reached |
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
| `wcmp` with `upperCase` on both tokens | `c.tokens(eo::any_case)` |
| one of the 21 checkers in testlib's `checkers/` | the call in the table below |

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

Nothing needs `ouf.seekEof()`: text after the answer is a wrong answer unless the stream says
`trailing(eo::ignore)`. See [checker.md](checker.md).

### testlib's stock checkers

Each of the 21 is one call, which gives the verdict and ends the checker. They all compare
the whole output, token after token, where several of testlib's read one value. A word or a
number that testlib reads as a presentation error is a wrong answer: Eolymp has none.

| testlib | eolymp.h | Where they differ |
| --- | --- | --- |
| `wcmp` | `c.tokens()` | |
| `ncmp` | `c.integers()` | |
| `icmp` | `c.integers()` | a `long long` where `icmp` reads an `int` |
| `hcmp` | `c.integers(eo::big)` | |
| `uncmp` | `c.tokens(eo::any_order)` | the tokens are compared as text, which on integers is `uncmp`'s verdict, since neither takes `+`, a leading zero or `-0`, and any other token is compared too, where `uncmp` refuses it |
| `rcmp4`, `rcmp6`, `rcmp9`, `dcmp` | `c.reals(1e-4)`, `c.reals(1e-6)`, `c.reals(1e-9)`, `c.reals(1e-6)` | a real is read in the stream's syntax, so `+1.5`, `.5`, `5.` and `05` are text and not numbers, and differ from the answer's `1.5`, `0.5`, `5` and `5`, where testlib reads them all as numbers; a token that is not a number must be equal; the tolerance allows `1e-15` more, as testlib's does |
| `rcmp`, `acmp` | `c.reals(1.5e-6, eo::absolute)` | as above; the error allowed is `eps + 1e-15`, as testlib's is |
| `rncmp` | `c.reals(1.5e-5, eo::absolute)` | as above |
| `yesno`, `nyesno` | `c.yes_no()` | every word of the answer is read, so an answer file with anything but YES and NO in it, a comment after the words say, is a jury error, where testlib's `nyesno`, once the output has ended, counts the answer's remaining words without reading them and calls the run a wrong answer |
| `lcmp` | `c.lines(eo::exact)` | a line is compared character by character, where `lcmp` compares its words; the blanks that end a line are ignored |
| `fcmp` | `c.lines(eo::exact)` | the blanks that end a line, and the blank lines that end the file, are ignored |
| `caseicmp`, `casencmp`, `casewcmp` | `c.tokens()` | `Case`, `1:` and every value are compared as text, which on integers is the same verdict; a missing or misnumbered `Case k:` is a wrong answer at that token rather than a message about the case |
| `pointscmp`, `pointsinfo` | `eo::points(p)` | these two are examples of a scoring checker rather than comparisons; `eo::score(f)` pays a fraction of the test instead, and Eolymp has no `points_info` |

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
| `registerGen(argc, argv, 1);` | `eo::generator g(argc, argv, eo::salt("…"))`, with a salt of the problem's own, so that the arguments alone reproduce no test; or `eo::generator g(argc, argv)`, whose seed is the arguments alone, as testlib's is. See [What is secret and what is not](generator.md#what-is-secret-and-what-is-not) |
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
| `rnd.next("[a-z]{5}")`, `rnd.next("YES\|NO")` | `r.pattern("[a-z]{5}")`, `r.pattern("YES\|NO")`; see [Drawing from a pattern](generator.md#drawing-from-a-pattern) |
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

- **The same verdict on Windows.** testlib reads line ends by the platform it was built on:
  built for Windows, its strict `readEoln()` wants CR LF unless `FOR_LINUX` is defined, so a
  validator that passes a test on Linux refuses it on Windows. eolymp.h has one rule on every
  platform, the judge's: CRLF in the jury's files is folded, with an EO110 note, and the
  contestant's output is read as it is; see [Windows](README.md#windows).
- **A generator's arguments are all `-name=value`.** testlib accepts `-n 10`, `--n=10` and
  positional arguments read with `argv[1]` or `opt<int>(1)`; eolymp.h refuses each of them
  and every option it was not told about, before writing anything. A problem moved from
  testlib therefore needs its stored generator arguments rewritten, `gen 10 3` becoming
  `gen -n=10 -k=3`. The one positional argument allowed is the seed a stress run appends.
- **The same seed does not give the same numbers.** eolymp.h draws with its own documented
  algorithm, so a test regenerated after the move is a different test of the same shape.
  `r.weighted(a, b, t)` has the distribution of `rnd.wnext(a, b, t)`, the largest of
  `t + 1` draws, but not its values, and `r.pattern` draws as `rnd.next` does, one construct
  at a time, but other strings; it also draws `*`, `+` and `{n,}`, which testlib refuses.
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
- **A pattern is testlib's syntax read as a regular expression.** Six details differ, all
  where testlib's matcher is a trap: see [Patterns](validator.md#patterns).

## What eolymp.h leaves out

Everything a testlib validator, checker, interactor or generator calls has a counterpart
above. What is left out is left out on purpose:

- **Other platforms.** eolymp.h is POSIX-only and knows Eolymp's exit codes, not Polygon's,
  ejudge's, Contester's or TESTSYS's; a problem judged on Windows keeps testlib.
- **testlib's random sequence.** A generator drawing through `rnd` cannot be made to write the
  same bytes here; a problem whose tests must stay byte for byte keeps its testlib generator
  or its stored tests.
- **Positional and `-n 10` generator arguments.** Declared `-name=value` options are what let
  `--eo-describe` and `eo-judge check` reach every option's extremes.
- **`_pe`, `_pc(k)`, `quitp`'s points and `quitpi`'s `points_info`.** Eolymp has no
  presentation error and no points information; a score is `eo::score` or `eo::points`, and a
  malformed output a wrong answer.
- **`validator.testset()`.** On Eolymp a testset is the group, which is `v.group()`.
- **`ouf.seekEof()`, `readEof()` and `readEoln()` on a checker's streams.** Whitespace between
  tokens is skipped, and text after the answer is refused by the library's own closing check.
- **The string helpers, `format`, `vtos`, `upperCase`, `compress`, `englishEnding`.** `eo::fmt`
  builds a message, and a message shortens and escapes the values it quotes by itself.
