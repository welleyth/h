# Changelog

## 2.1.0

This release changes no verdict. The header is the same library as 2.0.0 under a new
version; everything else is eo-judge, whose emulated verdicts and scores are the same as
2.0.0's on every problem it ran then.

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
