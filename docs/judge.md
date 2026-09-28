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
eo-judge version           # the version of eo-judge
```

| Flag | Does |
| --- | --- |
| `--solution name` | judge one solution instead of all of them; a name the problem does not have is a usage error that lists the names it has |
| `--strict` | exit non-zero if anything raised a warning |
| `--deep` | use the full 100 MB hostile output rather than 2 MB |
| `--work dir` | keep the workspace instead of a temporary directory |
| `-v` | after each testset, list every run: `1:2 WRONG_ANSWER 12ms` and the first line of what the checker or interactor said |

Flags may come before or after the problem directory, and `-h` or `--help` prints the usage.
It exits 0 when it finished, 1 under `--strict` with warnings, 2 on a usage error and 3 when
the problem itself could not be run — a program that does not compile, a generator that
fails, a missing file.

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
  "checker":   {"source": "checker.cpp",   "runtime": "cpp:20-gnu14", "files": ["../../eolymp.h"]},
  "validator": {"source": "validator.cpp", "runtime": "cpp:20-gnu14", "files": ["../../eolymp.h"]},
  "scripts": {
    "gen":      {"source": "generator.cpp", "files": ["../../eolymp.h", "../../eolymp-shapes.h"]},
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
| `type` | `PROGRAM` | `PROGRAM` or `INTERACTIVE`; `COMMUNICATION` is refused with "eo-judge does not run COMMUNICATION problems yet", and `FUNCTION`, `OUTPUT`, `SQL`, `ML`, `QUIZ` and `WIDGET`, platform types too, with "eo-judge does not run FUNCTION problems" |
| `runCount` | 1 | how many times a solution runs, chaining the interactor's output into the next run |
| `timeLimit`, `cpuLimit` | — | milliseconds; a testset may override `timeLimit`; eo-judge enforces `timeLimit` as a wall-clock limit and reads `cpuLimit` without enforcing it |
| `interactorTimeLimit` | — | read and not used: an interactor gets the solution's limit plus a second, as the agent gives it |
| `memoryLimit` | — | bytes |
| `uniqueAnswer` | false | the answer is the only correct one, which is what turns EO804 on |
| `exactFormat` | false | whitespace is part of the format, which turns EO818 off |
| `checker`, `validator`, `interactor` | — | one program each |
| `scripts` | — | named generators, whose names become directory names like a solution's; `answerGenerator` names one of them |
| `solutions` | — | what `run` judges and `check` compares subtasks against; each has a `name`, which becomes a directory name and so cannot hold `/`, be `..`, be longer than 240 bytes or be another solution's, a `source`, an optional `type`, and an optional expected score in `scores`; `CORRECT` is a reference expected to score full marks unless `scores` says otherwise, `DONT_RUN` is left out of `run` and `check` unless `--solution` names it, and `INCORRECT`, `WRONG_ANSWER`, `TIMEOUT`, `OVERFLOW`, `TIMEOUT_OR_ACCEPTED`, `OVERFLOW_OR_ACCEPTED` and `FAILURE` are judged with no expectation checked |
| `testsets` | — | the groups |

### Program

| Field | Means |
| --- | --- |
| `source` | the file, relative to the problem directory |
| `runtime` | an Eolymp runtime name; only the C++ standard is read from it |
| `files` | headers copied next to the source before compiling, exactly as the judge's `files[]` does; that directory is searched after the system's headers, so `#include <eolymp.h>` finds an installed copy first, as the judge does, and an attached one when there is none |

`eo-judge` compiles on your machine, not in the judge's runtime image, so a problem that uses
`eolymp.h` still names it in `files` here even though it attaches nothing on the judge.

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
before anything is uploaded.

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
- **It does not run `COMMUNICATION` problems.** It refuses one when it loads the problem,
  exit 3, rather than drive a controller as if it were an interactor; several instances
  behind the SPAWN handshake are not emulated yet.
- **It does not fetch a problem from the platform.** The directory is written by hand, or by
  a tool that exports one.
- **It does not read the statement**, so it cannot tell that a bound disagrees with the text.
  EO106 is the nearest thing, and it only notices a bound one away from a round number.
