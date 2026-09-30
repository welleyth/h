# Writing a generator with eolymp.h

A **generator** writes one test input to stdout, from its command-line arguments. Instead of
uploading a 50 MB file, a problem stores "run `gen` with `-n=200000 -shape=path`", and Eolymp
runs the generator when it needs the test.

A generator must be **deterministic**: the same arguments give the same bytes, on every
machine and with every compiler. Eolymp caches a generated test by its arguments, a rejudge
years later must see the same test, and anyone rebuilding the problem must get the tests the
contestants got. Every random number therefore comes from a seed derived from the arguments,
never from the clock.

The test's **answer** is not the generator's business: Eolymp produces it by running the
reference solution on the generated input.

This page describes the library as it is built, and §Not here yet lists what is still
missing.

## A first generator

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int n = g.option<int>("n", 1, 200000);
    int top = g.option<int>("max", 1, 1000000000, 1000000000);
    std::vector<long long> a = g.rng("values").ints(n, 1, top);
    g.out.line(n);
    g.out.line(a);
}
```

Run as `gen -n=10 -max=100`, it prints two lines: `10`, then ten numbers between 1 and 100
separated by single spaces.

- `eo::generator g(argc, argv)` makes the program a generator and derives the seed from the
  arguments.
- `g.option<int>("n", 1, 200000)` declares `-n`: an integer in that range, required.
- `g.option<int>("max", 1, 1000000000, 1000000000)` declares `-max` with a default.
- `g.rng("values")` is a named random stream.
- `g.out.line(...)` writes one line: the values separated by single spaces. A container is
  written as its elements.

Run with an option it never declared, it stops before writing anything:

```
unknown option -m; the options are -n, -max
```

## How the judge runs it

| | Value |
| --- | --- |
| arguments | the arguments stored with the test, exactly as written |
| stdin | empty |
| stdout | the test |
| stderr | kept by stress runs |
| environment | `EOLYMP=1` |
| time | 60 s |
| result | exit 0: the output becomes the test. Anything else: the generation fails |

A generated test is cached under a key built from the runtime, the source, the name, the
arguments and the role. To get a different test, change the arguments.

**Stress runs** run the generator over and over looking for a failing input. Every `[a..b]`
inside a stored argument becomes a random integer in that range, and a 16-hex-digit seed is
appended as the last argument. The library recognises that seed and folds it into its own;
you declare nothing. A stress run that makes no random draw gets warning EO501, because every
iteration would otherwise get the same test.

**Errors and warnings go to stderr,** never into the test.

## Options

Every argument a generator accepts is **declared**, with its type and its range. The
declarations are the generator's documentation, and the library refuses anything they do not
allow.

| Declaration | Declares |
| --- | --- |
| `g.option<int>("n", low, high)` | a required integer in `[low, high]` |
| `g.option<int>("n", low, high, def)` | the same, with a default |
| `g.option<long long>(…)`, `g.option<double>(…)` | the same for other number types |
| `g.option<std::optional<int>>("n", low, high)` | an integer in `[low, high]` that may be left out: `std::nullopt` when it is; `long long` and `double` too |
| `g.option<std::string>("shape", {"random", "path"})` | a required choice from a list |
| `g.option<std::string>("shape", {"random", "path"}, "random")` | a choice with a default |
| `g.option<bool>("distinct", false)` | a flag, written `-distinct=true` or `-distinct=false` (or `1`, `0`) |

**The syntax is `-name=value`, always.** `-n 10`, `--n=10` and a bare `-distinct` are errors.
Because the value is always attached to its name, a flag can never swallow the argument after
it, which is what makes a stress run's appended seed safe to pass.

Everything is checked before the first byte of output:

| Problem | Message |
| --- | --- |
| an argument no option declares | `unknown option -m; the options are -n, -max, -shape` |
| a required option missing | `-n is required` |
| a value out of range | `-n=0 is below 1` |
| a value that is not one of the choices | `-shape=cycle is not one of random, sorted` |
| a value of the wrong type | `-n=ten is not an integer` |
| something that is not an option at all | `n=5 is not an option; write -name=value` |

Nothing is written while an argument on the command line is still undeclared: `g.out` holds
the test in memory until every one has been declared, so an unknown option writes no byte at
all, and an option declared late, after a megabyte of output, costs that much memory. Declare
every option at the top of `main`.

**`-seed=…` is built in.** Every argument feeds the random seed, so `gen -n=10 -seed=1` and
`gen -n=10 -seed=2` give two different tests of the same size. `seed` needs no declaration
and means nothing else.

Options that only make sense together go through `g.require`:

```cpp
g.require(total >= t, "-total={} is smaller than -t={}", total, t);
```

A refusal by `g.require` exits 4, where every other refusal exits 3. The judge fails the
generation either way; `eo-judge check` reads the 4 as "these options do not go together", so
EO813 does not blame an extreme that only some combinations allow.

An optional option is for a value with no sensible default, one the generator works out when
it is not given:

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int n = g.option<int>("n", 1, 200000);
    std::optional<int> m = g.option<std::optional<int>>("m", 0, 200000);
    g.out.line(n, m.value_or(n - 1));
}
```

`gen --eo-describe` prints the declarations instead of generating:

```
eo-describe option n an integer 1..200000
eo-describe option max an integer 1..1000000000 default=1000000000
eo-describe option m an integer 0..200000 optional
eo-describe option shape choice random, sorted default=random
```

Declare every option at the top of `main` so that they are all listed; one declared inside a
branch is listed only when that branch runs.

## Randomness

`g.rng()` is the default stream. `g.rng("label")` is a **named stream**, derived from the
seed and the label, and independent of every other stream:

```cpp
eo::rng& shape = g.rng("shape");
eo::rng& weights = g.rng("weights");
```

Named streams keep a generator stable while you edit it. If the shape code draws one more
number, only the shape changes; the weights, drawn from their own stream, stay the same. With
one shared stream an extra draw anywhere shifts every draw after it. Using the default stream
and named ones in the same generator gets note EO507 for that reason.

| Call | Returns |
| --- | --- |
| `r.uniform(low, high)` | an integer in `[low, high]` |
| `r.real(low, high)` | a real number in `[low, high)` |
| `r.chance(p)` | true with probability `p` |
| `r.pick(container)` | a random element |
| `r.weighted(low, high, lean)` | an integer skewed towards `high` when `lean > 0` and towards `low` when it is negative |
| `r.pair(low, high)` | two integers drawn in a fixed order |
| `r.ints(count, low, high)` | independent integers |
| `r.distinct(count, low, high)` | different integers |
| `r.perm(n)`, `r.perm(n, first)` | a random permutation |
| `r.partition(count, sum)`, `r.partition(count, sum, least)` | positive integers adding up to `sum` |
| `r.letters(length, eo::charset("a-z"))` | a string over those characters |
| `r.pattern("[A-Z][a-z]{0,9}")`, `r.pattern(p)` | a string that matches the pattern, in the syntax of [validator.md](validator.md#patterns) |
| `r.shuffle(values)` | shuffles in place |

**Every draw but `shuffle` is `[[nodiscard]]`**, because a draw whose value is thrown away
silently shifts every later draw. A composite draw checks its own arguments:
`r.distinct(10, 1, 5)` stops with `cannot draw 10 different values from 1..5` rather than
looping for ever.

### Drawing from a pattern

`r.pattern("[a-z]{1,5}")` draws one construct at a time, each uniformly on its own:

- a class, each of its characters with the same chance;
- alternatives, each side with the same chance;
- a repeat, its count between its least and its most, each count with the same chance, and
  each copy drawn afresh; `*`, `+` and `{n,}` draw at most 20 more than their least.

So the string is not uniform over everything the pattern matches: `a|bc*` draws `a` half the
time. A class written with `^` draws from the visible characters `!` to `~` that it does not
exclude, never a space, a control character or a byte above 127. A draw is refused, with the
line that asked for it and whatever the seed, when the pattern has a class that excludes them
all, even under a `?`, or when it could draw more than 100,000,000 characters, as `[a-z]{200000000}`
or `((((((a*)*)*)*)*)*)*` could. The same seed draws the same string everywhere, but not the
string testlib's `rnd.next` draws from the same pattern.

### Why not `rand`, `std::shuffle` or the clock

Each of these makes the same generator produce different tests on different machines:

| Avoid | Why |
| --- | --- |
| `time(0)`, `std::random_device` | a different test on every run |
| `rand()`, `srand` | the algorithm differs between C libraries, and the judge's is not your laptop's |
| `std::shuffle`, `std::uniform_int_distribution` | their results are unspecified: libstdc++ and libc++ differ for the same seed |
| `f(r.uniform(1, n), r.uniform(1, n))` | C++ does not fix the order arguments are evaluated in. Use `r.pair`, or draw into variables first |
| iterating an `unordered_set` to build the test | the order depends on the standard library |
| plain `char` for small numbers | signed on x86, unsigned on ARM |

The library's streams use one documented algorithm and one method per operation whatever the
C++ types of the arguments, so the tests are the same everywhere.

## Writing the test

| Call | Writes |
| --- | --- |
| `g.out.line(values…)` | the values separated by single spaces, then a line break |
| `g.out.line()` | an empty line |
| `g.out.lines(container)` | each element on its own line |
| `eo::fixed(x, digits)` | a real number with exactly that many digits after the point |

```cpp
g.out.line(n, m);
g.out.line(a);
g.out.lines(rows);
g.out.line(eo::fixed(p, 6));
```

The writer is buffered and never flushes per line. It never adds a trailing space of its own:
a value at the end of a line that prints nothing, such as an empty string, takes its separator
with it, so `g.out.line("a", "")` writes `a`. A value that itself ends with a space is written
as it is. Every line ends with a single line break, and so does the file.

It prints numbers, text, `eo::fixed` and containers of them. Anything else, a struct or a
`std::pair`, does not compile, and the error says so in one line, `eolymp.h cannot print this
type`, instead of a page of template instantiations; pass the fields one by one. The same holds
for `it.send` and every message.

Calling `std::exit` or `std::quick_exit` ends the generator as returning from `main` does: the
arguments are checked, `g.out` writes what it holds, and the exit code is the one given. So
`exit(0)` part-way through writes what was written so far: the library cannot tell a
deliberate early end from a half-written test, and the validator is what refuses the second.
A generator that is never destroyed ends the same way. `std::_Exit` and `std::abort` run
nothing and lose what `g.out` holds, and `std::quick_exit` does the same where the C library
offers no `at_quick_exit`: glibc and musl have one, so the judge writes the test, and macOS has
none, so there a `quick_exit` writes an empty test.

Anything that reaches stdout another way — `std::cout`, `printf` — still lands in the test,
but not where it was written: `g.out` holds its lines until it has a megabyte of them or the
generator ends, so bytes written another way land ahead of every line `g.out` still holds.
`g.out.line("first"); printf("stray\n"); g.out.line("last");` writes `stray`, `first`,
`last`. That gets warning EO503, because mixing the two makes the layout hard to predict. The check compares what the writer wrote with how far stdout actually moved,
so it is silent when stdout is a pipe rather than a file.

## Warnings

The rules are those of every eolymp.h program: a warning never changes the output except in
strict mode (`EOLYMP_STRICT=1`); each carries a stable code and a fix, and the line of your
source when it is about a call; EO501, EO502, EO503 and EO507 are about the whole run and
name only the generator; `eo::allow quiet("EO507", "why")` silences one code in a scope. They go to stderr, which
Eolymp keeps for stress runs.

| Code | Fires when |
| --- | --- |
| EO501 | a stress run, recognised by its seed argument, made no random draw |
| EO502 | the test exceeded 64 MB |
| EO503 | bytes reached stdout without going through `g.out` |
| EO506 | a random draw's result was ignored (at compile time, through `[[nodiscard]]`) |
| EO507 | the default stream is used alongside named ones (a note) |

## Testing a generator locally

```bash
g++ -std=c++17 -O2 -o gen gen.cpp
```

```bash
./gen -n=10 -shape=sorted -seed=1 | ./validator --group 1
```

Running it twice with the same arguments must give the same bytes, and passing the result
through the problem's validator is the cheapest check there is. This repository's own gate
does both on every build.

**The shapes are a second header.** Trees, graphs, sequences, strings and points, with the
relabelling a generator must not forget, live in `eolymp-shapes.h`: see
[shapes.md](shapes.md).

**A generator rewritten against this library draws different numbers,** so it produces
different tests — equally valid, but not the same ones. That is fine for a new problem, and
it means a problem whose existing tests must be reproduced byte for byte keeps its original
generator.

## Not here yet

Nothing a generator draws is missing. EO812 (the same bytes under two compilers) and EO813
(the extremes of every option produce a valid test) are run by `eo-judge check`; see
[judge.md](judge.md).

## Reference card

| Member | Does |
| --- | --- |
| `eo::generator g(argc, argv)` | makes the program a generator; derives the seed |
| `g.option<T>(name, low, high)`, `(name, low, high, def)` | a number option |
| `g.option<std::optional<T>>(name, low, high)` | a number option that may be left out |
| `g.option<std::string>(name, {choices})`, `(name, {choices}, def)` | a choice |
| `g.option<bool>(name, def)` | a flag |
| `-seed=…` | built in: varies the test without meaning anything else |
| `g.require(cond, "…", args)` | stops when options do not fit together |
| `g.rng()`, `g.rng("label")` | the default stream, a named stream |
| `g.out.line(…)`, `g.out.line()`, `g.out.lines(container)` | the test |

| Stream call | Returns |
| --- | --- |
| `uniform`, `real`, `chance`, `pick`, `weighted`, `pair` | single values |
| `ints`, `distinct`, `perm`, `partition`, `letters`, `pattern` | composite values |
| `shuffle` | shuffles in place |

**Helpers**: `eo::fixed(x, digits)`, `eo::charset("a-z")`, `eo::fmt(…)`, `eo::log(…)`,
`eo::allow name("EO507", "why")`, `eo::version()`.
