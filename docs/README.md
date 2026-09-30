# eolymp.h

One header that jury programs for the [Eolymp](https://www.eolymp.com) judge are written
against. Everything lives in `namespace eo`: no global names, no macros beyond the include
guard and the version, nothing that redefines part of the C library.

**Today it supports every kind of jury program Eolymp runs**: validators, checkers,
interactors, controllers and generators, including problems that run in phases and problems
whose instances run side by side. A second, opt-in header, `eolymp-shapes.h`, holds the test
shapes a generator draws from — see [shapes.md](shapes.md).

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

[validator.md](validator.md), [checker.md](checker.md), [interactor.md](interactor.md),
[generator.md](generator.md) and [controller.md](controller.md) are the guides, one per kind
of jury program. [shapes.md](shapes.md) is the test shapes, [judge.md](judge.md) is the
emulator, and [warnings.md](warnings.md) is every warning code, one self-contained row each —
that is the page to look a code up in. [testlib.md](testlib.md) puts each testlib call beside
its eolymp.h counterpart, for a problem moving over.

## What it gives you

- **A score is a fraction of the test, and a percentage is caught while you prepare.** Points
  on Eolymp are absolute and clamped to the test's cost, so a checker that reports a percentage
  overpays a cheap test to full marks and underpays an expensive one. `eo::score` takes the
  fraction and the library multiplies; a score of 2 or more is clamped to full marks with
  warning EO205, which calls it a likely percentage, and `EOLYMP_STRICT=1` or
  `eo-judge --strict` makes that fatal. A full score leaves through exit 0, not the exit 7
  that is not an accept.
- **The verdict line is written before anything the checker printed.** The judge's parser
  gives up if any line before `points` ends in whitespace, so one debug `printf` with a
  trailing space turns a partial score into a VERIFICATION_FAILURE. The checker takes over its
  own stdout and stderr; an end-to-end test proves the point by running a checker that prints
  a line ending in a space and then a blank line, and feeding the log through a copy of the
  judge's own parser, which still reads the score.
- **A failed read blames the side it came from.** The same call is a wrong answer on the
  contestant's output and a jury error on the answer file, including inside a reader function
  shared by both — so a broken answer file is caught as the jury's fault instead of silently
  failing every contestant.
- **It says what is wrong with the validator, not with the test.** Leave out a separator and
  the message names the value, the line of your source and the fix:
  `validator.cpp:18: line 2, k: a space follows n; read it with read_space()`.
- **It reads in constant memory.** The reader holds one fixed buffer, 1 MB, whatever the
  input's size, so a 49 MB test costs the same as a small one. A token or a line read with a
  stated maximum stops one character past it rather than holding the rest.
- **It compiles in under three seconds.** The validator above builds with `-O2` in about 2.6 s
  and leaves an object of about 194 KB, and the first checker in [checker.md](checker.md) is
  about the same (g++ 12 on Linux; the standard headers eolymp.h includes take 0.45 s on
  their own). `make budget` measures the compiler's CPU time for both on every run of the
  gate, against those standard headers built in the same run, and fails when either takes
  more than 8.5 times as much or leaves an object over 220 KB. The judge compiles the
  validator again for every run that needs it.
- **It keeps out of your code's way, as long as you write `eo::`.** Everything is inside
  `namespace eo`, with no global names and no macros beyond the include guard and the version.
  `make check` builds the header after `<bits/stdc++.h>` with `using namespace std`, and beside
  globals named `OK`, `checker`, `split`, `format`, `join` and `trim`.

  **`using namespace eo;` is outside that, and outside the compatibility promise.** A minor
  release adds names to `eo` the way a C++ standard adds names to `std`, and a program that
  opens `eo` can meet an ambiguity it did not have before; 2.3.0 adds `pattern`, `any_order`,
  `big` and `absolute`. Some names are in both namespaces
  already. `unique`, `ignore`, `any` and `ratio` are a value in one and a
  type or a function in the other, so once both are open an unqualified use, such as
  `c.answers(unique)`, is ambiguous and does not compile; `log`, `is_sorted` and
  `is_permutation` are functions in both, and an unqualified call picks one by its arguments,
  which is easy to misread. Write `eo::` in front of the library's names, as every page does;
  `using namespace std;` on its own is fine, and `make check` builds a program that uses each
  of those names from both namespaces that way.
- **A message with the wrong number of `{}` can fail to compile, if you ask.** Under C++20,
  building with `-DEOLYMP_CHECK_PATTERNS` checks every literal message at compile time, in
  `eo::wrong`, `eo::accept`, `eo::score`, `eo::points`, `eo::jury_error`, `eo::fmt`,
  `eo::log`, `require` and a stream's `wrong`: `eo::wrong("got %d", x)`, which otherwise runs
  with warning EO112, stops the build at
  `a_message_needs_one_placeholder_for_each_value`, and a lone brace at
  `a_message_needs_two_braces_to_print_one`; a literal `eo::pattern` that does not parse
  stops it at `a_pattern_that_does_not_parse`. It is opt-in because a program that built with
  the last release has to build with this one. C++17, and a compiler without `consteval`,
  ignore the macro, and a message held in a `std::string` or a `char` array is still checked
  when it runs.
- **It checks the problem, not only the test.** A value read without bounds, a bound one away
  from a round number, a subtask table nobody declared, a `cases` loop with no sum limit —
  each becomes a warning with a stable code, the line of your source and a fix. A warning
  never makes a test invalid; `EOLYMP_STRICT=1` turns them fatal while you prepare.

**CRLF is converted, and that is deliberate.** The agent converts those line endings before a
validator ever runs, so a local run has to do the same or it disagrees with the judge about
its own tests. The library converts them and says so with note EO110.

## Using it on a problem

The judge's C++ runtime carries the header, at `/usr/include/eolymp.h`, so a program
includes it and attaches nothing:

```cpp
#include <eolymp.h>
```

`eolymp-shapes.h` is beside it, for the generators that draw shapes.

**The runtime image is the version.** One release of the header lives in the image, and the
runtime's tag names the image; rebuilding that runtime moves every program on the judge to
the release it then carries, so the library is upgraded for a whole judge at once rather than
one problem at a time.

Locally, download both headers from the latest release, put them next to the source and
point the compiler at them:

```bash
curl -LO https://github.com/eolymp/h/releases/latest/download/eolymp.h
curl -LO https://github.com/eolymp/h/releases/latest/download/eolymp-shapes.h
g++ -std=c++17 -O2 -I. -o validator validator.cpp
```

C++17 is the floor, and the header builds unchanged as C++20 and C++23. It is tested against
GCC and clang, on glibc, on musl — the judge's own libc — and on macOS with libc++.

## How a warning reaches you

A warning is raised where it is noticed and kept once per code and line with a count, so a
read inside a loop is reported once. A warning about a read, a bound or a clamped score
carries the line of *your* source, found without a macro. One about how the run ended — the
answer file left unread, a verdict with no message, points the judge rounds up, a test worth
nothing (EO201–EO204, EO206's rounding, EO208, EO210, EO212, the EO40x end-of-run checks) —
carries the header's own line, and a validator's or a generator's run-level warning names
only the program. What
happens to it when the program ends depends on one thing: whether `EOLYMP` is set in the
environment. That is how the header tells a local build from the judge.

**Locally** it is a block on stderr, warnings before notes, capped at thirty, each with its
fix underneath:

```
validator.cpp:4: note EO106: the bounds 1..200001 are one away from a round number
  compare them with the statement
```

**On the judge** the same warning is one line, then a machine-readable line for tooling that
would rather not parse prose:

```
note EO106 validator.cpp:4 the bounds 1..200001 are one away from a round number
eo-report {"version":1,"warnings":[{"code":"EO106","at":"validator.cpp:4","count":1}]}
```

Which channel that block goes to depends on what the role can afford to write to:

| Role | On the judge the report goes to |
| --- | --- |
| validator | its stdout, which the validation result keeps for valid tests as well as invalid ones |
| checker | `checker.log`, after the verdict |
| interactor, controller | stderr, because stdout is the solution's input |
| generator | stderr, because stdout is the test |

**The verdict is always written first.** A checker holds its own stdout and stderr on a
temporary file for the whole run and replays them only after the verdict line, so nothing can
get between the judge's parser and the score it is looking for:

```
points 25 matched 10 of 40
eolymp.h 2.3.0
warning EO203 ./eolymp.h:NNNN the answer file still holds "40" when the checker finished
note EO106 checker.cpp:4 the bounds 1..200001 are one away from a round number
eo-report {"version":1,"warnings":[{"code":"EO106","at":"checker.cpp:4","count":1},{"code":"EO203","at":"./eolymp.h:NNNN","count":1}]}
```

`NNNN` is a line of the header itself, which moves from one release to the next.

**A warning never changes a verdict.** It cannot make a test invalid or an answer wrong. Two
things change that deliberately: `EOLYMP_STRICT=1` makes the first warning fatal, which is
for a preparation loop and not for the judge, and `eo::allow` silences one code in a scope and
needs a reason, which the report lists so a silence is visible rather than lost.

Some codes never reach a run at all. `[[deprecated]]` on the forms that omit a name or bounds
and `[[nodiscard]]` on the calls whose result must be used arrive as **compiler** warnings,
before the program has run once.

**The whole-problem codes are never sent by a program.** EO8xx and EO9xx describe a problem
across runs — a bound no test reaches, a subtask no solution fails, a testset in its own
dependencies — so no single program can see them, and none of them will ever appear in a
judge log. They come from `eo-judge check`, which reads what each run recorded and prints
them on your machine while you prepare; see [judge.md](judge.md). Every code, from either
side, is in [warnings.md](warnings.md).

## What is in the repository

| Path | Holds |
| --- | --- |
| `eolymp.h` | the library, generated from `src/` and committed, because this is the file the judge's C++ runtime ships |
| `src/` | the sources it is built from, one file per layer |
| `eolymp-shapes.h` | the opt-in test shapes, generated from `src/shapes/` and committed beside it |
| `docs/` | this page, the five guides — one per kind of jury program — the shapes, `eo-judge`, and every warning code |
| `tests/` | the suite, the end-to-end programs, the live problems and the hostile builds |
| `judge/` | `eo-judge`, the emulator: a Go command with no dependencies |
| `tools/` | the amalgamator and the gates |

`src/` is layered so that the parts every future role needs are already separate from the
validator:

| File | Holds |
| --- | --- |
| `core.h` | the environment the judge sets, and the single exit every verdict leaves through |
| `fmt.h` | `eo::fmt`, the `{}` messages the API takes everywhere |
| `parse.h` | the strict number syntax |
| `io.h` | the reader: one buffer of a fixed size, grown only to hold what an interactor or a controller takes in while a large send waits, no copy of what it has read, `read()` refills so a pipe cannot deadlock it, and the line and column a message needs |
| `diag.h` | the warnings: codes, call sites, counts, the report, strict mode, `eo::allow` |
| `read.h` | `eo::charset`, names, and the vocabulary the readers share |
| `structure.h` | `all_distinct`, `is_sorted`, `is_permutation`, `is_tree`, `is_connected`, `is_simple_graph` |
| `stream.h` | the reader both roles share: strict for a validator, whitespace-lenient for a checker |
| `validate.h` | `eo::validator` |
| `role.h` | the verdicts both scoring roles share |
| `check.h` | `eo::checker`, its three streams and the ready-made comparisons |
| `random.h`, `summary.h` | a deterministic random stream, and the format an interactor hands the checker |
| `interact.h`, `phases.h` | `eo::interactor`, the dialogue it shares with `eo::controller`, `eo::budget`, and the `run_count` chain |
| `generate.h` | `eo::generator`: declared options, named streams and the writer |
| `shapes/` | the second header: trees, graphs, sequences, strings, points, and `presented` |
| `control.h` | `eo::controller` and `eo::channel`: the SPAWN handshake and one pipe pair per instance |

`eolymp.h` is generated, so edit `src/` and run `make`. The gate fails if the committed header
does not match.

## Building and testing

```bash
make check
```

That is the whole C++ gate, and CI runs it on g++, clang++, musl and macOS, and its `test`,
`hostile` and `examples` parts on GCC 9, whose warnings differ from today's compilers' and
fail the build under `-Werror` in every program that includes the header. CI also runs
`make judge`, `make mutants`, `make sanitize` and `make fuzz`, and `make version` on a pull
request. This table is the one description of the gate: the rows down to `budget` are what
`make check` runs, the rest run on their own, and each answers a question:

| Target | Proves |
| --- | --- |
| `amalgamation-check` | the committed `eolymp.h` and `eolymp-shapes.h` are what `src/` generates |
| `test` | the suite passes, built in `CXXSTD` (C++17 unless set) at `-O2` under `-Wall -Wextra -Wshadow -Werror`, which is where GCC's flow warnings such as `-Wstringop-overflow` appear; the suite calls every role and every shape |
| `coverage` | every line of both headers runs at least once, including inline functions nothing calls, and fails the build if one does not |
| `standards` | it also compiles and passes in the other two of C++17, C++20 and C++23, at `-O0` under the same warnings, which proves the language and library differences in a third of the build time |
| `e2e` | a real compiled validator gives the judge's exit codes and messages, through the exit path the tests cannot reach |
| `hostile` | both headers build after `<bits/stdc++.h>` with `using namespace std`, beside organiser-style globals, and without a warning under `-Wpedantic -Wconversion -Wsign-conversion -Wold-style-cast` and, where the compiler has it, `-Wuseless-cast`, in a program that uses every role; a program that prints a value the library cannot print fails to build with the library's own message, one built below C++17 stops at a single `#error` that names the standard, `using namespace eo` beside `using namespace std` is ambiguous, and `eo::` with `using namespace std` builds cleanly; and with libstdc++, the judge's library, a program that includes only `eolymp.h` still gets `std::function`, `std::unordered_map`, `std::hash`, `std::bind`, `std::not_fn` and `std::invoke` from it, as with 2.2.0 |
| `examples` | every example in `docs/` compiles |
| `codes` | every warning code the sources raise has a row in `docs/warnings.md`, and the page's count of built codes is right |
| `budget` | how long the validator above and the first checker in checker.md take to build, and how large they are, and fails when either takes more than 8.5 times the compiler CPU time of the standard headers `eolymp.h` includes, a fixed list in `tools/budget.py` that the gate holds to the header's own `#include` lines, built alone in the same run, each program the fastest of three builds, or leaves an object over 220 KB; here the validator is 5.8 times and 194 KB, and CI's largest object is 197 KB, on musl |
| `bench` | how many instructions the main paths take, from reading integers to comparing tokens and writing reals, counted by `perf`; `BASE=<revision>` builds the same programs against that revision's headers and shows the change; run with `make bench` |
| `mutants` | a changed operator or bound in either header makes the suite fail; run with `make mutants` |
| `sanitize` | the suite and the end-to-end programs pass under ASan and UBSan; run with `make sanitize` |
| `fuzz` | every libFuzzer harness in `tests/fuzz/` finds no crash, sanitizer report or broken property in 45 s each (in CI, 90 s for all of them side by side on a push or pull request, and 30 minutes each nightly); needs clang++; `make fuzz-<harness>` or `FUZZER` runs one harness, `FUZZ_SECONDS` sets the time, and `make -j fuzz` runs them side by side; run with `make fuzz` |
| `judge` | `gofmt` and `go vet` are clean and the `eo-judge` tests pass, on Linux and on macOS in CI; run with `make judge` |
| `version` | a change to the headers or to eo-judge raises `EOLYMP_H_VERSION`, and eo-judge's version is the same number; CI runs `make version` on every pull request |

Set `CXX` and `CXXSTD` to choose a toolchain, and `GCOV` to the matching coverage tool:

```bash
CXX=g++-16 GCOV=gcov-16 make check
```

**One caveat about coverage.** The line gate needs GNU `gcov`. LLVM's `gcov` emulation loses
a basic block whose only exit is a throw, so it reports lines as unrun that the tests
provably run; under it the tool prints what it found and says the gate is elsewhere rather
than failing or passing quietly. CI runs the real gate on the GCC and musl legs. Under GNU
`gcov` the suite is built with `-fkeep-inline-functions`, because without it an inline function
that nothing calls is never emitted, so `gcov` cannot see it and it passes as covered; a
template that nothing instantiates is still invisible.

Tests are one translation unit — `tests/all.cpp` including `tests/*.inc` — so coverage is
measured on the shipped header rather than on the sources it came from. Set `EOT_TRACE=1` to
print each test as it runs. A watchdog stops the suite, naming the test, when one test runs
longer than `EOT_TEST_SECONDS` (120 by default; `0` turns it off).

## Versions

Semantic versioning, with one promise: nothing that changes a verdict changes within a major
version. Warnings can be added in a minor version, because they never change a verdict on
their own. `eo::version()` and `EOLYMP_H_VERSION` say which release you have, as a string
such as `"2.0.0"`; `EOLYMP_H_VERSION_MAJOR`, `EOLYMP_H_VERSION_MINOR` and
`EOLYMP_H_VERSION_PATCH` are the same numbers for the preprocessor, as in
`#if EOLYMP_H_VERSION_MAJOR >= 2`. [CHANGELOG.md](../CHANGELOG.md) lists every verdict a
release changes, and every name a release adds to `eo`. A new name can make an unqualified use
ambiguous in a program that says `using namespace eo;`, so that program is outside the
promise; write `eo::` in front of the library's names.

## Licence

MIT, in [LICENSE](../LICENSE).
