# Changelog

## 2.1.1

No verdict and no score changes: a program built against 2.1.1 judges every run as it did
under 2.1.0, with the same messages, exit codes, warnings and warning counts, and a generator
writes the same bytes. The header is faster. It is not shorter: `eolymp.h` has 4,572 lines
against 2.1.0's 4,567, and 177,540 bytes against 179,499.

### For problem authors

- **Reading is faster.** Instructions to read, from `make bench`, 2.1.0 → 2.1.1: a validator's
  `read_ints` 2.65 → 1.05 G for 2 million values, `read_longs` 1.92 → 0.66 G, `read_reals`
  1.16 → 0.71 G, `read_line` 0.42 → 0.18 G, `read_tree` 1.08 → 0.39 G; a checker's
  `tokens()` 2.17 → 0.69 G, `reals()` 2.31 → 0.95 G, `lines()` 0.78 → 0.19 G, `read_longs`
  on both files 3.09 → 1.03 G. `tokens()` on 105 MB of 11.6 million words takes 8.1 G
  instructions, where testlib's `wcmp` takes 19.2 G and 2.1.0 took 25.8 G. An interactor runs
  about 850 user-space instructions of its own per round trip instead of about 1,400
  (`perf stat -e instructions:u` over 200,000 round trips with an echoing solution).
- **Writing `eo::fixed` is faster**: a generator writing `eo::fixed(x, 6)` runs 2.09 → 0.50 G
  instructions for 500,000 lines.
- Integers are parsed where they lie in the buffer, a real is read with `std::from_chars` and
  `eo::fixed` written with `std::to_chars` where the library has them, and only while the
  rounding mode is to-nearest and the decimal point is `.`, since `strtod` and `printf` follow
  both; elsewhere, and on macOS's libc++, the old paths run. Lines are taken a run at a time,
  names are not copied for every element, and a tree is accepted after one union-find pass.
- Because the programs finish sooner, the warnings about time, EO209 for a checker, EO303 for a
  validator and EO504 for a generator, fire less often on the same test.
- A controller's `eo::channel` is an `eo::stream`, so it has every read a checker's streams
  have, `read_line`, `read_ints`, `read_reals`, `read_tokens` and `at_eoln` among them, and
  `numbers(eo::lenient)`, `reals(eo::plain)` and `wrong(…)`. `skip_rest` and `trailing`, which
  only a checker's closing checks look at, are deleted on a channel.
- The header builds without a warning under `-Wpedantic -Wconversion -Wsign-conversion
  -Wold-style-cast -Wuseless-cast`.

### For maintainers

- `make bench` prints the instructions of the main read, check and generate paths under
  `perf`, and `make bench BASE=<revision>` the change against another revision.
- `tests/pinned.inc` pins what the reader, the comparisons, the formatting and the roles say
  today, byte for byte, through buffers of every size and under other rounding modes and
  locales.
- The interactor and the controller share one base for their dialogue, the three roles one
  lookup of their files and one fail-closed ending, the readers one loop for arrays and one for
  choices, and the sizes the header repeats have names.
- A warning code is known by its text: two spellings of one code at one site are counted
  together. The library raises its codes as literals, which compilers merge, so no program
  sees the difference.
- The hostile gate builds a program that uses every role under the strict warnings above, and,
  with libstdc++, a program that includes only `eolymp.h` and uses `std::function`,
  `std::unordered_map`, `std::hash`, `std::bind`, `std::not_fn` and `std::invoke`, which it
  has always got through the header. CI runs `make judge` on macOS as well as Linux.
- Left as they were:
  - An interactor still polls before each write. The poll is the only call that tells both
    whether the write can go ahead and whether the solution's output must be taken in first.
    Pipes refuse `RWF_NOWAIT` on Linux 6.1, a non-blocking standard output would break jury
    code that writes to it, and writing or taking in at another moment changes the round trips
    and EO409s the log reports.
  - EO204 still warns about an empty `jury_error` message in a checker and not in an
    interactor or a controller: making the roles agree adds or drops a warning on some runs.
  - The simplification removed about the lines it was planned to remove, and the faster code
    added about as many: `src/` is 540 lines in and 525 out against 2.1.0, where the plan
    counted −214 for the simplification alone.

## 2.1.0

This release changes no verdict. The header is the same library as 2.0.1 under a new
version; everything else is eo-judge, whose emulated verdicts and scores are the same as
2.0.1's, and 2.0.0's, on every problem it ran then.

### eo-judge

- **A build cache.** Programs are compiled once into `~/.cache/eo-judge` and reused while the
  compiler, the flags, the include variables, the sources and every header they read are
  unchanged. A warm `run` of `tests/live/degrees` takes 0.16 s instead of 3.2 s, and a warm
  `check` 0.40 s instead of 6.2 s. `EO_JUDGE_CACHE` moves the cache or, set to `off`, turns
  it off; a cache directory that cannot be written is the same as `off`.
- **Both headers are carried.** eo-judge embeds `eolymp.h` and `eolymp-shapes.h` of its own
  release and searches them after the system's headers and the program's attached files, so
  a problem attaches neither, as on the judge. `files` still works.
- **`eo-judge init <dir> --type program|interactive|phases`** writes a new problem that passes
  `run --expect --strict` and `check --strict`.
- **`--json`** prints `run`, `check` and `lint` as one object: the attempts, the findings, the
  exit code and the error.
- **`run --expect`** exits 1 when a solution breaks its declared `type`, which makes `run` a
  gate for CI; `docs/judge.md` lists what each type asserts.
- **A GitHub Action**, `uses: eolymp/h@v2.1.0`, downloads and verifies eo-judge, keeps its
  cache, runs `check` and optionally `run --expect`, and turns the findings into annotations.
- `check` prints the same report on every run: findings are ordered by severity, code,
  place in natural order, and message, instead of the order maps gave.
- `run` and `check` refuse a `COMMUNICATION` problem, exit 3, instead of judging it as an
  interactive one whose solutions all got RUNTIME_ERROR; `lint` reads it.
- Every test is generated before a failure stops the run, and each failed test is named with
  the generator call that failed; nothing is judged, exit 3, as before. A generator that runs
  out of its 60 s is not run again for its other tests.
- A testset's "of N" is what its mode can pay: the smallest test under `WORST`, the largest
  under `BEST`, 0 under `NO_SCORE`; EO907 adds up the same figures.
- `-v` and `--json` show the interactor's line next to a solution that crashed or ran out of
  time.
- EO814 also fires for an interactor that a hostile client kills with a signal, which the judge
  takes as an interaction failure too.
- EO812's second compiler is the other family, clang beside GCC or GCC beside clang; it used
  to pick `g++` beside `c++`, the same compiler under another name, on Debian and Ubuntu, so
  it never compared with clang there. `check` can now raise EO812 where it could not.
- EO903 no longer asks a program that includes `"eolymp.h"` or `"eolymp-shapes.h"` to attach
  it, since the judge's runtime carries both, and EO910 counts the carried copy against an
  attached one.
- An unknown command is reported before the problem is loaded, exit 2, and `version --json`
  prints the version as JSON.
- A generated test or answer that cannot be closed, as on a full disk, stops the run instead
  of being judged truncated; a compiler that hangs when asked what it is no longer hangs
  `check`; a compiler that cannot start says why.

All of these change what `check` and `lint` report, or how eo-judge runs; none changes a
verdict or a score.

## 2.0.1

No verdict and no score changes: a program built against 2.0.1 judges every run as it did
under 2.0.0. The messages are the same, byte for byte.

### For problem authors

- [docs/controller.md](docs/controller.md) described 1.x: it said a controller does not take
  in an instance's answers while it writes, and that sending an instance more than a pipe holds
  before reading leaves both waiting. 2.0.0 already takes them in, up to 16 MB for each
  instance, and raises EO409 beyond that; the page now says so, and says that only the instance
  being written to is taken in from, so a wait across instances is the protocol's to avoid. The
  EO409 row in [docs/warnings.md](docs/warnings.md) names the controller and the instance.
- A controller builds an instance's name for EO409 only when EO409 is raised, instead of on
  every flush: 200,000 round trips run about a third fewer instructions.
- The 2.0.0 section below now also lists the controller's large sends and the spelling of a
  real that is not a number, both of which shipped in 2.0.0.

### For maintainers

- The release job, and the nightly fuzzing, run only in eolymp/h, so a fork that syncs `main`
  publishes nothing.
- The coverage gate builds the suite with `-fkeep-inline-functions` under GNU `gcov`, so an
  inline function nothing calls counts as unrun. Five internal functions nothing called are
  gone, and the public reads no test called are tested, among them a channel's.
- New tests: a controller sending 100,000 lines before it reads, end to end through
  `tests/e2e/serve.cpp`, which now kills a run after 10 s; and the EO409 a controller raises.
- A faster gate, measured on one shared 12-core machine: `make -j12 check` 77 s → 47 s,
  `make sanitize` 126 s → 42 s, `make mutants` 91 s → 60 s. The suite no longer sleeps; one
  suite is built at `-O2`, and the other standards, the sanitizer build and the mutants at
  `-O0`; each standard is its own make target, run once; the e2e programs build in one batch;
  and `make budget` times only what it gates, and stops once a build is under its ceiling. On
  CI, the check workflow takes about 2m15s instead of about 3m14s.
- `make fuzz-<harness>` runs one harness, and `make -j fuzz` all of them side by side. A pull
  request now fuzzes all harnesses together in one job for 90 s instead of in six 45 s jobs, at
  under a third of the runner time. That is less fuzzing per harness: about 40 % of the
  executions for the number parser, the fastest harness, and 71 % to 102 % for the others. The
  push job keeps its own corpus rather than starting from the nightly ones. The nightly run
  keeps its full depth, one 30-minute job per harness, and takes the harnesses from the files
  in `tests/fuzz/`.
- `tests/e2e/run.sh` names every failing check with the first line of its log, and builds with
  the Makefile's `WARNINGS`. The tools take `CXX`, `CXXSTD` and `WARNINGS` from
  `tools/common.py`, which splits `CXX` as a command line, so `CXX="ccache g++"` works in every
  tool and in `make e2e`. `tools/version.py --print` is the one reader of the version, and the
  release job uses it.
- The gate is described once, in the table in
  [docs/README.md](docs/README.md#building-and-testing), and `make codes` checks the count of
  built codes on [docs/warnings.md](docs/warnings.md).

## 2.0.0

This release changes verdicts, so it is a major version. Every verdict change moves a run
toward the result it should have had: a broken checker's accept becomes a jury error, and a
correct solution's wrong answer becomes an accept. The judge's C++ runtime carries one release
of the header, so rebuilding the runtime moves every problem at once. The list below is
complete, so a problem author can check whether any of their problems depends on the old
behaviour.

### Checkers and scoring

- An exception that leaves a checker, interactor or controller and is caught outside the role
  object: was an accept, or whatever code the handler chose, → a jury error. Affects a checker
  wrapped in a `try` whose `catch` is outside the role object.
- A NaN score or NaN points, including through `pass()`: was `points nan`, which the judge paid
  in full, → a jury error. Affects a checker with a 0/0 formula.
- ±inf in an interactor or controller (`eo::points(+inf)`, a score of ±inf): was a jury error
  at the checker → accept or `points 0`, as a checker already gave. Checkers are unchanged.
- Negative `eo::points`: was `points -5` → `points 0`, still exit 7.
- `eo::points` above the cost in an interactor or controller: was a jury error, a summary
  fraction above 1, → accept, as in a checker.
- A fraction above 1: unchanged, still clamped to 1; EO205 now calls 2 or more a likely
  percentage.

### Reading contestant output

- A lenient number longer than 4096 characters: was split into two values, or accepted with
  its tail unread, → a wrong answer.
- `reals()`: a numeric token longer than 4096 characters and than the answer's could be
  accepted → a wrong answer.
- A negative zero on a checker's, interactor's or controller's streams (`-0.000000`, `-0`,
  `-1e-400` in `read_real` and `reals()`): wrong answer → accept. Integers and validators are
  unchanged.
- `lines()`: a line that differs from the answer only by trailing carriage returns: wrong
  answer → accept.
- A huge contestant token or line in `tokens()`, `reals()`, `lines()` or `yes_no()`: a system
  failure from running out of memory → a wrong answer.

### Validators, generators, summaries and handoffs

- A `sum_limit` total that overflows a `long long`: the invalid test was accepted → refused.
- A generator whose write to stdout fails, on a full disk or a closed descriptor: exit 0 with
  a truncated test → exit 3.
- A summary or handoff that cannot be written whole: could pay a truncated `fraction 1` → a
  jury error.
- A summary that carries a field twice: the last value won → a jury error.
- A truncated phase handoff, or one with a negative size: SIGABRT → a jury error.
- `partition(1, s, s)` and similar in a generator: exit 3 → the test is written.

### Messages and interaction

- A message whose `{}` do not match its values, such as `eo::wrong("expected %lld", a)` or
  contestant text used as the pattern: a jury error → the intended verdict, with warning EO112
  (still fatal under `EOLYMP_STRICT`).
- A solution that exits before the interactor's or controller's last line: a wrong answer
  depending on a race → decided by what the solution said.
- The last output to a slow reader: what did not fit in the pipe was dropped, so a correct
  solution got a runtime error or a wrong answer, → delivered, with a 500 ms no-progress and a
  2 s overall cutoff.
- An interactor that sends more than 64 KB before reading, to a solution that answers as it
  reads: a deadlock, a time limit or an idleness verdict, → accept, taking in up to 16 MB of
  answers meanwhile; beyond that, warning EO409 and the old deadlock.
- A controller that sends an instance more than 64 KB before reading, to an instance that
  answers as it reads: a deadlock, a time limit or an idleness verdict, → the controller's own
  verdict, taking in up to 16 MB of that instance's answers meanwhile; beyond that, warning
  EO409 and the old deadlock. Only the instance being written to is taken in from.
- An inherited non-blocking pipe: a jury error, "Resource temporarily unavailable", → the real
  verdict.
- A controller with many instances: about 1 GB per 1000 instances, which could reach the
  memory limit, → about 68 MB.
- A checker when `/tmp` is read-only or full: a jury error on every test → judges, keeping its
  output in a memfd or the working directory instead.

### Not verdicts, but visible changes

- `eo::allow(...)`, `eo::sum_limit(...)` or `eo::budget(...)` written as a bare statement now
  gets a `[[nodiscard]]` warning under GCC 10 or later and clang, an error with `-Werror`.
- `rng::real` in builds that fuse multiply-add (clang with FMA, arm64 Macs, GCC with
  `-march=native` or `haswell`, GCC on arm64) now draws the same bits as g++ on x86, so tests
  those builds generated before come out different. GCC on x86 without FMA, the judge's
  build, is unchanged.
- A real that is not a number, written by `{}` or `eo::fixed` into a message, a log or a
  generated test, is spelt `nan`, `-nan`, `inf` or `-inf` under every C library. Built on
  macOS, a negative NaN used to come out as `nan`.
- New or changed warnings, which change no verdict except under `EOLYMP_STRICT` or
  `eo-judge --strict`: EO206 (points the judge's float rounds up to the cost), EO208 (now a
  warning, and right that the run is accepted), EO213, EO409 and EO112.
- eo-judge reads `problem.json` strictly: unknown or wrongly cased fields, trailing content,
  values outside the platform's enums, repeated keys and repeated names are refused, and
  `DONT_RUN` solutions are skipped. Solutions run outside the workspace, and a test or answer
  rewritten during a run ends it with exit 3. Emulated verdicts and scores for valid problems
  are unchanged.

### Also

- Performance: numbers and tokens are read a run of bytes at a time, element names and
  bounds are looked up only when needed, the warning report is ordered without a stable sort,
  and shapes and distinct draws use flat hash sets; every change produces identical output.
- eo-judge: flags anywhere on the command line, `version`, `-v` per-run lines, parallel builds
  that build each recipe once, attached headers found after the system's, whole process groups
  stopped on a timeout or an interrupt, and released binaries for Linux and macOS under
  `judge/v<version>`, with the same version as the header.
- CI: the suite at `-O2` in three standards on GCC, clang, musl and macOS; ASan and UBSan;
  mutants; libFuzzer harnesses on every change and nightly; a version check on every pull
  request; and releases made only from a commit every job passed.
- [docs/testlib.md](docs/testlib.md) maps testlib calls to eolymp.h.
