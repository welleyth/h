# eo-judge

`eo-judge` runs a problem the way the Eolymp judge does, on your machine, and then runs the
checks that no single program can make from inside one run. It lives in [judge/](../judge)
and is written in Go with no dependencies beyond the standard library.

**Installing it.** Each release of eo-judge, tagged `judge/v<version>`, carries static binaries
for Linux and macOS on amd64 and arm64, with a `SHA256SUMS` file to check them against:

```bash
curl -LO https://github.com/eolymp/h/releases/download/judge/v2.0.0/eo-judge-linux-amd64
curl -LO https://github.com/eolymp/h/releases/download/judge/v2.0.0/SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
install -m 755 eo-judge-linux-amd64 ~/.local/bin/eo-judge
```

With Go 1.23 or later, `go install github.com/eolymp/h/judge/v2@v2.0.0` builds the same
program from the tag; the `/v2` is Go's rule for a module at major version 2, and Go names the
binary `judge` after its directory. In a checkout, `make build/eo-judge`
writes `build/eo-judge`, and `make judge` runs gofmt, go vet and the eo-judge tests.

```bash
eo-judge run   <problem>   # build, generate, validate, judge every solution, score it
eo-judge check <problem>   # EO801-EO821 and EO901-EO910
eo-judge lint  <problem>   # what is only visible in the source
eo-judge init  <dir>       # write a new problem that run and check pass
eo-judge version           # the version of eo-judge
```

| Flag | Does |
| --- | --- |
| `--solution name` | judge one solution instead of all of them; a name the problem does not have is a usage error that lists the names it has |
| `--strict` | exit non-zero if anything raised a warning |
| `--deep` | use the full 100 MB hostile output rather than 2 MB |
| `--work dir` | keep the workspace instead of a temporary directory |
| `-v` | after each testset, list every run: `1:2 WRONG_ANSWER 12ms` and the first line of what the checker or interactor said; for a solution that crashed or ran out of time, its exit code and then the interactor's line |
| `--json` | print one JSON object on stdout instead of the text; see [below](#json) |
| `--expect` | with `run`, exit 1 when a solution breaks its declared type; see [below](#expected-types) |

Flags may come before or after the problem directory, and `-h` or `--help` prints the usage.
It exits 0 when it finished, 1 under `--strict` with warnings or `--expect` with a broken
type, 2 on a usage error and 3 when the problem itself could not be run — a program that does
not compile, a generator that fails, a missing file.

## A new problem

`eo-judge init <dir>` writes a complete problem into a new or empty directory, and
`--type` picks which:

| `--type` | Writes |
| --- | --- |
| `program`, the default | the sum of n numbers: a validator, a generator with options, a checker, a reference solution and a 32-bit one declared `WRONG_ANSWER`, and five generated tests |
| `interactive` | guessing a number in 20 queries: an interactor with a budget, a checker that takes the interactor's verdict, and a solution that always answers 1 |
| `phases` | Alice and Bob over `runCount` 2: an interactor that hands a code from the first run to the second, and a solution whose longer code scores part of a test |

Each passes `run --expect --strict` and `check --strict` as written, with only note EO821
left, so everything the report says after an edit is about the edit. The programs include
`eolymp.h` and attach nothing, as on the judge.

## Cache

Compiling is nearly all of a run's time, so `eo-judge` keeps every program it builds in
`~/.cache/eo-judge` (`$XDG_CACHE_HOME/eo-judge` when that is set, `~/Library/Caches/eo-judge`
on macOS) and builds it again only when something that decides its bytes changed. A second
`run` of an unchanged problem takes a fraction of a second.

| `EO_JUDGE_CACHE` | Means |
| --- | --- |
| unset | the directory above |
| a directory | that directory instead |
| `off` | no cache: every program is built in the workspace, every time |

A directory that cannot be created or written, on a read-only home for one, is the same as
`off`.

An entry is named by the compiler (its resolved path, size and modification time), the flags,
`CPATH`, `CPLUS_INCLUDE_PATH`, `C_INCLUDE_PATH`, `GCC_EXEC_PREFIX` and `COMPILER_PATH`, and the
bytes of the source and of every attached file. The headers the compiler finds on its own are
checked rather than named: the entry lists every one the build read, with its size and
modification time, and every place in the compiler's search path where an attached header's
name, or `eolymp.h` or `eolymp-shapes.h`, was absent, so an updated system header, or an
installed `eolymp.h` that would now win over an attached or a carried one, rebuilds the
program. Two `eo-judge` processes that need the same
program take turns building it.

Nothing is ever removed from the cache on its own; `rm -rf ~/.cache/eo-judge` empties it, and
the next run builds everything again. A program's warnings name the problem's own files, as
they do without a cache.

The compiler is known by the driver that `CXX` names, so replacing only what it runs, such as
`cc1plus` or the linker, with the driver untouched is not noticed; empty the cache after such
an upgrade, as ccache asks too. With the cache on, `--work` keeps each program's copied
sources and its runs, but the compiled program lives in the cache entry.

## It reproduces the judge, deliberately

Every scoring rule is taken from the platform's own source rather than from a specification,
because the two have disagreed before:

| Rule | Comes from |
| --- | --- |
| exit 0 accepted, 1 and 2 wrong answer, **7** a fraction, anything else a system failure | agent `internal/judge/checker/program.go` |
| the environment a checker is given: `EOLYMP`, `INPUT_FILE`, `OUTPUT_FILE`, `ANSWER_FILE`, `TEST_ID`, `TEST_COST`, `TEST_INDEX`, `TEST_GROUP` | the same file |
| an interactor's arguments and environment: the input, the output, and the answer only when the test has one; the run's metadata merged in; a limit of the solution's plus a second | agent `internal/judge/runner/script.go`, `interact()` |
| `readPoints`, which scans the log for the word `points` and then a float | copied verbatim from the same file, and a test diffs the copy against `origin/main` |
| an interactor's exit code: 0 runs the checker, 1 and 2 are a wrong answer, anything else an interaction failure — but only once the solution's own run completed, so a crash is never excused | agent `internal/judge/runner/script.go`, `run()`; the rule that forgives a broken pipe lives in `communicate()` and applies to COMMUNICATION only |
| the ICPC stop, which trips on a wrong answer worth **zero** and not on a partial score | agent `internal/judge/admissioner/showstopper.go` |
| `FULLY_ACCEPTED` and `FIRST_POINT` dependencies | agent `internal/judge/admissioner/dependency.go` |
| accepted pays the cost, a wrong answer and a partially correct run pay `min(cost, score)`, anything else pays nothing | atlas `internal/services/submissions/submission_reporter.go` |
| a checker's exit 7 is `PARTIALLY_CORRECT`, and `ACCEPTED` when its points reach the test's cost — which a test worth 0 always does | agent `internal/judge/evaluation.go` |
| `ALL` pays only if every run passed, `EACH` sums, `WORST` takes the smallest, `BEST` the largest | the same file |

The two problems in [../tests/live](../tests/live) are what holds this emulation to the
judge's behaviour: a batch problem with a partial-scoring checker and an interactive one with a
query budget. The `eo-judge` tests run both end to end and assert the verdict and the score of
all seven submissions, and those expected values are what the real judge awarded — so a change
that drifts from the judge fails the suite.

## The problem directory

A problem is a directory with a `problem.json` beside its sources. The field names follow the
Eolymp API, so a problem exported from the platform maps onto it one to one.

```json
{
  "title": "tree degrees",
  "type": "PROGRAM",
  "timeLimit": 2000,
  "memoryLimit": 268435456,
  "uniqueAnswer": true,
  "checker":   {"source": "checker.cpp",   "runtime": "cpp:20-gnu14"},
  "validator": {"source": "validator.cpp", "runtime": "cpp:20-gnu14"},
  "scripts": {
    "gen":      {"source": "generator.cpp"},
    "solution": {"source": "solution_full.cpp"}
  },
  "solutions": [
    {"name": "full",   "source": "solution_full.cpp", "type": "CORRECT"},
    {"name": "leaves", "source": "solution_leaves.cpp"}
  ],
  "testsets": [
    {"index": 1, "scoringMode": "EACH", "feedbackPolicy": "COMPLETE",
     "tests": [
       {"index": 1, "score": 3,
        "generator": {"script": "gen", "arguments": ["-n=6", "-shape=path"]},
        "answerGenerator": "solution"}
     ]}
  ]
}
```

`eo-judge` refuses a `problem.json` with a field it does not know or a value outside the lists
below, naming it, rather than ignoring a misspelt `"scoringMode": "WORSE"` and judging under the
default. Field names match exactly, case included, a key appears once in an object, and
nothing may follow the problem's object. Every value the platform itself exports is accepted, so a problem exported from Eolymp
loads as it is; an explicit `UNKNOWN_TYPE`, `UNKNOWN_FEEDBACK_POLICY`,
`UNKNOWN_DEPENDENCY_MODE` or `UNSET` is the same as leaving the field out.

### Problem

| Field | Default | Means |
| --- | --- | --- |
| `type` | `PROGRAM` | `PROGRAM` or `INTERACTIVE`; `COMMUNICATION` is refused by `run` and `check` with "eo-judge does not run COMMUNICATION problems yet", and `lint` reads it; and `FUNCTION`, `OUTPUT`, `SQL`, `ML`, `QUIZ` and `WIDGET`, platform types too, with "eo-judge does not run FUNCTION problems" |
| `runCount` | 1 | how many times a solution runs, chaining the interactor's output into the next run |
| `timeLimit`, `cpuLimit` | — | milliseconds; a testset may override `timeLimit`; eo-judge enforces `timeLimit` as a wall-clock limit and reads `cpuLimit` without enforcing it |
| `interactorTimeLimit` | — | read and not used: an interactor gets the solution's limit plus a second, as the agent gives it |
| `memoryLimit` | — | bytes |
| `uniqueAnswer` | false | the answer is the only correct one, which is what turns EO804 on |
| `exactFormat` | false | whitespace is part of the format, which turns EO818 off |
| `checker`, `validator`, `interactor` | — | one program each |
| `scripts` | — | named generators, whose names become directory names like a solution's; `answerGenerator` names one of them |
| `solutions` | — | what `run` judges and `check` compares subtasks against; each has a `name`, which becomes a directory name and so cannot hold `/`, be `..`, be longer than 240 bytes or be another solution's, a `source`, an optional `type`, and an optional expected score in `scores`; `CORRECT` is a reference expected to score full marks unless `scores` says otherwise, `DONT_RUN` is left out of `run` and `check` unless `--solution` names it, and `INCORRECT`, `WRONG_ANSWER`, `TIMEOUT`, `OVERFLOW`, `TIMEOUT_OR_ACCEPTED`, `OVERFLOW_OR_ACCEPTED` and `FAILURE` are judged with no expectation checked unless `run --expect` [checks them](#expected-types) |
| `testsets` | — | the groups |

### Program

| Field | Means |
| --- | --- |
| `source` | the file, relative to the problem directory |
| `runtime` | an Eolymp runtime name; only the C++ standard is read from it |
| `files` | headers copied next to the source before compiling, exactly as the judge's `files[]` does; that directory is searched after the system's headers, so `#include <eolymp.h>` finds an installed copy first, as the judge does, and an attached one when there is none |

A program that uses `eolymp.h` or `eolymp-shapes.h` names neither in `files`, here as on the
judge. The judge's runtime carries both in `/usr/include`; `eo-judge` carries the copies of its
own release and searches them last, after the system's headers and the program's attached
files. For `#include <eolymp.h>` an installed copy wins, then an attached one, then eo-judge's;
for `#include "eolymp.h"` the attached copy beside the source comes first, then an installed
one, then eo-judge's, which is the order the judge's compiler uses too. Attach a header only to
pin a different release than the one eo-judge carries.

The source is compiled as `source.cpp`, which is what the judge calls it, so a warning's line
matches what a judge log would say.

### Testset and test

| Field | Default | Means |
| --- | --- | --- |
| `index` | — | 0 is the examples testset |
| `scoringMode` | `EACH` | `EACH`, `ALL`, `WORST` or `BEST`, or `NO_SCORE`, which runs the group and pays nothing for it |
| `feedbackPolicy` | `COMPLETE` | `COMPLETE`, or `ICPC`, which stops the group after a test worth nothing; `ICPC_EXPANDED` stops the same way and only shows the contestant more |
| `dependencyMode` | `FULLY_ACCEPTED` | `FULLY_ACCEPTED` or `FIRST_POINT` |
| `dependencies` | — | testset indexes that must pass first |
| `timeLimit`, `memoryLimit` | the problem's | per testset |
| `tests[].score` | 0 | what the test is worth |
| `tests[].input`, `tests[].answer` | — | files, CRLF-normalised the way the judge does |
| `tests[].generator` | — | `{"script": name, "arguments": [...]}` |
| `tests[].answerGenerator` | — | a script name; it is given the input on stdin |
| `tests[].example` | false | a sample |

A test with no `answer` and no `answerGenerator` uses its input as the answer, which is what
an interactive problem wants.

## Reading a run

```
full: ACCEPTED, 100
  testset 1  ACCEPTED                   9 of 9        3 ACCEPTED
  testset 2  ACCEPTED                  91 of 91       7 ACCEPTED

leaves: PARTIALLY_CORRECT, 55.767494
  testset 1  PARTIALLY_CORRECT      5.425 of 9        3 PARTIALLY_CORRECT
  testset 2  PARTIALLY_CORRECT      50.34 of 91       7 PARTIALLY_CORRECT
```

That is the `check_solutions` oracle offline: the score a submission would get, per testset,
before anything is uploaded. The figure after "of" is the most the testset can pay under its
`scoringMode`: the sum of its tests for `EACH` and `ALL`, its smallest test for `WORST`, its
largest for `BEST`, and 0 for `NO_SCORE`.

Every test is generated before anything is judged. A test that cannot be made stops the run
with exit 3 once the rest have been tried, and each one that failed is named with the call
that failed, so one run lists them all:

```
eo-judge: 2 tests could not be made, so nothing was judged:
  test 1:1: the generator gen -n=0 exited 3: n is below 1
  test 1:3: open problems/sum/03.in: no such file or directory
```

A generator or an answer generator that runs out of its 60 s is not run again: the other tests
that need it are listed as not tried, with the test it hung on.

## Reading a check

Every finding carries a code, and every code is in [warnings.md](warnings.md), one
self-contained row each. A report looks like this:

```
testset 1: warning EO807: no test reaches n = 2
  a maximal test that is not maximal; generate one that reaches it
note EO821: the problem has 1 correct solution(s)
  a second correct solution is what a stress run compares the reference with
```

`check` also replays the warnings the programs themselves raised while generating and
validating, so one command covers both halves.

**Each solution runs in a directory of its own**, a fresh temporary directory outside the
workspace that is removed when the run ends, so no name or relative path from it reaches the
files the interactor and the checker use: the summary in `output.txt`, a phase's handoff, the
tests and their answers. The tests are also read-only once generated, and after every run
eo-judge checks that its input and answer are byte for byte what was generated; if either
changed, it stops with exit 3 and names the solution rather than score a run against an answer
the solution rewrote. None of this stops a solution that looks for the workspace on purpose —
it lives under `$TMPDIR`, and a process can find the directories of the processes around it —
so run a problem you do not trust in a container.

## Expected types

`run --expect` holds every judged solution to its `type` and exits 1 if one breaks it, which
is what makes `run` a gate for CI. The solution's block says why:

```
linear: RUNTIME_ERROR, 40
  testset 1  RUNTIME_ERROR             40 of 100      2 ACCEPTED, 3 RUNTIME_ERROR
  it is declared WRONG_ANSWER, but test 1:3 is RUNTIME_ERROR
...
eo-judge: 1 solution(s) break their declared type
```

A type means what the same tag means on Polygon, read onto the platform's names. A run the
judge skipped counts for nothing either way.

| `type` | Holds when |
| --- | --- |
| `CORRECT` | the solution ends `ACCEPTED` at 100 |
| `INCORRECT` | it does not end `ACCEPTED`: some run failed |
| `WRONG_ANSWER` | some run is `WRONG_ANSWER` or `PARTIALLY_CORRECT`, and every run is one of those or `ACCEPTED` |
| `TIMEOUT` | some run is `TIME_LIMIT_EXCEEDED`, and every run is that or `ACCEPTED` |
| `TIMEOUT_OR_ACCEPTED` | every run is `TIME_LIMIT_EXCEEDED` or `ACCEPTED` |
| `FAILURE` | some run is `FAILURE`, a jury error; a runtime error does not satisfy it, on the platform either |
| `OVERFLOW`, `OVERFLOW_OR_ACCEPTED` | not checked, and the block says so: eo-judge does not measure memory |
| `DONT_RUN`, none | nothing |

A solution with `scores` must also score exactly that; for `CORRECT` it takes the place of the
100, so a reference expected to score 60 is `"type": "CORRECT", "scores": "60"`. A time limit
on your machine is not the judge's, so a `TIMEOUT` solution that is only slightly slow can hold
here and break there, or the other way round; give such a solution `TIMEOUT_OR_ACCEPTED`.

## JSON

With `--json`, `run`, `check` and `lint` print nothing on stdout but one object, and the exit
code is the same as without it; `version --json` prints `{"version": "2.1.0"}`, and `init`
refuses the flag:

```json
{
  "version": "2.1.0",
  "problem": "tests/live/degrees",
  "invalid": [{"group": 1, "test": 2, "why": "line 1, n: 1 is below 2"}],
  "attempts": [
    {"name": "leaves", "type": "", "verdict": "PARTIALLY_CORRECT", "score": 55.767494,
     "groups": [
       {"index": 1, "verdict": "PARTIALLY_CORRECT", "score": 5.425, "cost": 9,
        "runs": [{"test": 1, "verdict": "PARTIALLY_CORRECT", "ms": 3, "message": "points 1 2 of 6 degrees"}]}
     ]}
  ],
  "findings": [
    {"code": "EO106", "level": "note", "where": "checker.cpp:10",
     "message": "the bounds 1..1999 are one away from a round number", "fix": "reported by checker"}
  ],
  "exit": 0
}
```

| Field | Holds |
| --- | --- |
| `attempts` | what `run` judged, in the order of `solutions`; empty for `check` and `lint` |
| `breaks` | under `--expect`, why the solution breaks its type; left out when it holds |
| `type` | the solution's `type` from `problem.json`, empty when it has none |
| `invalid` | the tests the validator refused; left out when there are none |
| `findings` | the report, in its order and without its repeats; `where` is empty for the whole problem |
| `exit` | the exit code |
| `error` | why eo-judge could not finish, when it could not; the same line also goes to stderr |

## In CI

The repository is also a GitHub Action, so a repository of problems can check each one on
every push:

```yaml
name: problems
on: [push, pull_request]
jobs:
  eo-judge:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v7
      - uses: eolymp/h@v2.1.0
        with:
          problem: problems/degrees
          expect: true
```

| Input | Default | Means |
| --- | --- | --- |
| `problem` | — | the directory that holds `problem.json`, relative to the repository |
| `version` | the action's own release | the eo-judge release to download |
| `strict` | `false` | fail when `check` raises any warning, as `--strict` does |
| `expect` | `false` | also `run --expect`, failing when a solution breaks its [declared type](#expected-types) |
| `binary` | — | an eo-judge you built, used instead of downloading one |

The action downloads eo-judge for the runner's system from the release, checks it against the
release's `SHA256SUMS`, and keeps [the build cache](#cache) between runs, keyed by the
problem's files, so an unchanged problem is judged without compiling anything. It runs
`check --json`, and `run --json --expect` when asked, prints each report in the log, and turns
it into annotations: every finding is a warning or a notice on the source line it names, or on
`problem.json` when it names a test or a testset, and a solution that breaks its type or a
problem that cannot be run is an error. A step fails when eo-judge exits non-zero. It runs on
Linux and macOS runners.

## What it does not do

- **It is not a sandbox.** Programs run as you, with your files and your network. Each one
  runs in a process group of its own, and when it ends, times out or eo-judge is interrupted
  the whole group is killed, so a child that stays in that group cannot outlive it; one that
  starts a group or a session of its own escapes, and nothing else is confined. An interrupt
  stops a build the same way, and a second one ends eo-judge at once. Run problems you do not
  trust in a container.
- **It does not enforce memory, or report it.** The time limit is enforced, as a wall-clock
  limit; `memoryLimit` is read and nothing measures a run against it. A memory-limit verdict
  is the judge's to give.
- **It does not run `COMMUNICATION` problems.** `run` and `check` refuse one, exit 3, rather
  than drive a controller as if it were an interactor; several instances behind the SPAWN
  handshake are not emulated yet. `lint` reads one as it reads any other.
- **It does not fetch a problem from the platform.** The directory is written by hand, or by
  a tool that exports one.
- **It does not read the statement**, so it cannot tell that a bound disagrees with the text.
  EO106 is the nearest thing, and it only notices a bound one away from a round number.
