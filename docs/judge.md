# eo-judge

`eo-judge` runs a problem the way the Eolymp judge does, on your machine, and then runs the
checks that no single program can make from inside one run. It lives in [judge/](../judge)
and is written in Go with no dependencies beyond the standard library.

**Installing it.** Each release of eo-judge, tagged `judge/v<version>`, carries static binaries
for Linux and macOS on amd64 and arm64, with a `SHA256SUMS` file to check them against:

```bash
curl -LO https://github.com/eolymp/h/releases/download/judge/v2.3.0/eo-judge-linux-amd64
curl -LO https://github.com/eolymp/h/releases/download/judge/v2.3.0/SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
install -m 755 eo-judge-linux-amd64 ~/.local/bin/eo-judge
```

With Go 1.23 or later, `go install github.com/eolymp/h/judge/v2@v2.3.0` builds the same
program from the tag; the `/v2` is Go's rule for a module at major version 2, and Go names the
binary `judge` after its directory. In a checkout, `make build/eo-judge`
writes `build/eo-judge`, and `make judge` runs gofmt, go vet and the eo-judge tests.

```bash
eo-judge run   <problem>   # build, generate, validate, judge every solution, score it
eo-judge check <problem>   # EO801-EO821 and EO901-EO912
eo-judge lint  <problem>   # what is only visible in the source
eo-judge stress <problem> --args '-n=[1..8]'
                           # generated inputs until a solution breaks its type; see below
eo-judge init  <dir>       # write a new problem that run and check pass
eo-judge version           # the version of eo-judge
```

| Flag | Does |
| --- | --- |
| `--solution name` | judge one solution instead of all of them; a name the problem does not have is a usage error that lists the names it has |
| `--strict` | exit non-zero if anything raised a warning |
| `--deep` | use the full 100 MB hostile output rather than 2 MB |
| `--work dir` | keep the workspace instead of a temporary directory; eo-judge clears the directories it makes there, so a directory that is the problem's, holds it or lies inside it is a usage error; one eo-judge uses it at a time, and a second that asks for it while the first runs exits 3 |
| `-v` | with `check`, a table of the features each test has, before the report; see [Reading a check](#reading-a-check). With `run`, after each testset, list every run: `1:2 WRONG_ANSWER 12ms` and the first line of what the checker or interactor said; for a solution that crashed or ran out of time, its exit code and then the interactor's line |
| `--transcript` | with `run` on an `INTERACTIVE` problem, `-v` and, under each run, the dialogue: every line the interactor and the solution sent each other, in the order they arrived, with `phase 1`, `phase 2`, … above each phase's when `runCount` is above 1; at most 1,000 lines a phase and 200 characters a line. It is meant for writing a statement's examples, and on any other command or type it is a usage error |
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

Each carries one [test for its validator and one for its
checker](#tests-for-the-validator-and-the-checker), an input the validator refuses and an
output with its verdict, to copy for more. Every generator it writes carries a salt of its
own, 32 hex digits from the operating system's secure random source, written in place of the
template's, so no two problems made by `init` share a test; see [What is secret and what is
not](generator.md#what-is-secret-and-what-is-not). Each passes `run --expect --strict` and
`check --strict` as written, with only note EO821 left, so everything the report says after
an edit is about the edit. The programs include
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

An entry is named by the compiler (the path `CXX` names or `PATH` finds for it, and the file
that path resolves to, with its size and modification time, so `/usr/lib/ccache/g++` and
`/usr/lib/ccache/clang++`, both links to ccache, are two compilers), the flags, `CPATH`,
`CPLUS_INCLUDE_PATH`, `C_INCLUDE_PATH`, `GCC_EXEC_PREFIX` and `COMPILER_PATH`, and the bytes of
the source and of every attached file. The headers the compiler finds on its own are
checked rather than named: the entry lists every one the build read, with its size and
modification time, and every place in the compiler's search path where an attached header's
name, or `eolymp.h` or `eolymp-shapes.h`, was absent, so an updated system header, or an
installed `eolymp.h` that would now win over an attached or a carried one, rebuilds the
program. Two `eo-judge` processes that need the same
program take turns building it.

Nothing is ever removed from the cache on its own; `rm -rf ~/.cache/eo-judge` empties it, and
the next run builds everything again. A program's warnings name the problem's own files, as
they do without a cache.

The compiler is known by the driver that `CXX` names and nothing it runs, so replacing only
what the driver runs, such as `cc1plus` or the linker, with the driver untouched is not
noticed; empty the cache after such an upgrade, as ccache asks too. With the cache on, `--work` keeps each program's copied
sources and its runs, but the compiled program lives in the cache entry.

## It reproduces the judge, deliberately

Every scoring rule is taken from the platform's own source rather than from a specification,
because the two have disagreed before:

| Rule | Comes from |
| --- | --- |
| exit 0 accepted, 1 and 2 wrong answer, **7** a fraction, anything else a system failure | agent `internal/judge/checker/program.go` |
| the environment a checker is given: `EOLYMP`, `INPUT_FILE`, `OUTPUT_FILE`, `ANSWER_FILE`, `TEST_ID`, `TEST_COST`, `TEST_INDEX`, `TEST_GROUP` | the same file |
| an interactor's arguments and environment: the input, the output, and the answer only when the test has one; the run's metadata merged in; a limit of the solution's plus a second | agent `internal/judge/runner/script.go`, `interact()` |
| a validator's input: the test's path, `--group <testset index>`, and the test again on stdin, so a validator that reads stdin, as testlib's do, sees it | the judge's validator run, as [validator.md](validator.md#how-the-judge-runs-it) describes it |
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
| `type` | `PROGRAM` | `PROGRAM`, `INTERACTIVE`, [`OUTPUT`](#output-only-problems) or [`FUNCTION`](#function-problems); `COMMUNICATION` is refused by `run` and `check` with "eo-judge does not run COMMUNICATION problems yet", and `lint` reads it; and `SQL`, `ML`, `QUIZ` and `WIDGET`, platform types too, with "eo-judge does not run SQL problems" |
| `runCount` | 1 | how many times a solution runs, chaining the interactor's output into the next run |
| `timeLimit`, `cpuLimit` | — | milliseconds; a testset may override `timeLimit`; eo-judge enforces `timeLimit` as a wall-clock limit and reads `cpuLimit` without enforcing it |
| `interactorTimeLimit` | — | read and not used: an interactor gets the solution's limit plus a second, as the agent gives it |
| `memoryLimit` | — | bytes |
| `uniqueAnswer` | false | the answer is the only correct one, which is what turns EO804 on |
| `exactFormat` | false | whitespace is part of the format, which turns EO818 off |
| `checker`, `validator`, `interactor` | — | one program each |
| `scripts` | — | named generators, whose names become directory names like a solution's; `answerGenerator` names one of them |
| `solutions` | — | what `run` judges and `check` compares subtasks against; each has a `name`, which becomes a directory name and so cannot hold `/`, be `..`, be longer than 240 bytes or be another solution's, a `source` — on an `OUTPUT` problem `outputs` instead, [its answer files](#output-only-problems) — an optional `runtime`, a C++ one, whose standard it is compiled with and which on a `FUNCTION` problem picks [its template](#function-problems), an optional `type`, and an optional expected score in `scores`; `CORRECT` is a reference expected to score full marks unless `scores` says otherwise, `DONT_RUN` is left out of `run` and `check` unless `--solution` names it, and `INCORRECT`, `WRONG_ANSWER`, `TIMEOUT`, `OVERFLOW`, `TIMEOUT_OR_ACCEPTED`, `OVERFLOW_OR_ACCEPTED` and `FAILURE` are judged with no expectation checked unless `run --expect` [checks them](#expected-types) |
| `templates` | — | the code templates, one per runtime, each `{"runtime", "header", "source", "footer"}` with files for the last three; a `FUNCTION` problem needs them and [wraps its solutions in them](#function-problems), any other type takes them with a `source` only, the code a contestant starts from, and an `OUTPUT` problem has none |
| `testsets` | — | the groups |
| `validatorTests` | — | inputs the validator must accept or refuse, which `check` runs; [see below](#tests-for-the-validator-and-the-checker) |
| `checkerTests` | — | outputs and the verdict the checker must give them, which `check` runs; [see below](#tests-for-the-validator-and-the-checker) |

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

### Tests for the validator and the checker

`validatorTests` lists inputs and what the validator must say about them, and `checkerTests`
outputs and what the checker must give them, as Polygon's validator and checker tests do.
`eo-judge check` runs each one and reports every test the program answers otherwise, as
warning EO911 for the validator and EO912 for the checker, naming it by its place in its
list: `validator test 2`, `checker test 1`. `run` does not read them.

```json
"validatorTests": [
  {"input": "3\n1 2 3\n", "expect": "VALID"},
  {"input": "0\n\n", "expect": "INVALID"},
  {"file": "tests/four.txt", "expect": "INVALID", "group": 2}
]
```

| Field | Means |
| --- | --- |
| `input` | the input itself; `""` is an empty input |
| `file` | a file holding it, relative to the problem directory; give `input` or `file`, not both |
| `expect` | `VALID` or `INVALID` |
| `group` | a testset's index, passed as `--group`; without it the validator is given no group, as in a stress run |

The validator is given the input with CRLF line endings folded to LF, as the judge folds a
test's. A validator that breaks, printing a first line that starts with `eolymp.h: `, being
killed by a signal or not finishing in its 30 s, breaks either expectation.

```json
"checkerTests": [
  {"input": "2\n1 2\n", "output": "3\n", "answer": "3\n", "expect": "ACCEPTED"},
  {"input": "2\n1 2\n", "output": "5\n", "answer": "3\n", "expect": "WRONG_ANSWER"},
  {"input": "2\n1 2\n", "output": "4\n", "answer": "3\n", "expect": {"points": 20}, "cost": 40}
]
```

| Field | Default | Means |
| --- | --- | --- |
| `input`, `output`, `answer` | `""` | the three files the checker is given, written out as they are |
| `expect` | — | `ACCEPTED`, `WRONG_ANSWER`, `PARTIAL`, `FAILURE`, or `{"points": x}` with x from 0 to the test's `cost` |
| `cost` | 100 | what the test is worth, at least 0, given as `TEST_COST` |
| `group` | 0 | a testset's index, given as `TEST_GROUP` |

The checker's exit code and log are read as a solution's run is read: exit 0 is `ACCEPTED`,
1 and 2 `WRONG_ANSWER`, 7 the points its log names, `PARTIALLY_CORRECT` below the cost and
`ACCEPTED` at it, and anything else, a timeout included, `FAILURE`. `PARTIAL` expects
`PARTIALLY_CORRECT`; `{"points": x}` expects a run that pays x points and is not a failure, so
`{"points": 0}` holds for a wrong answer and for `eo::score(0)`. On a test worth 0 every
points exit is `ACCEPTED`, as on the judge. The checker is also given `TEST_INDEX`, the test's
place in the list, and an empty `TEST_ID`.

### Output-only problems

On an `OUTPUT` problem the contestant uploads one file per test, and the judge runs the
checker on the test's input, that file and the jury's answer. A solution is the files it
would upload, named per test, in place of a `source`:

```json
"type": "OUTPUT",
"solutions": [
  {"name": "full", "type": "CORRECT", "outputs": {"1": "full-1.txt", "2": "full-2.txt", "3": "full-3.txt"}},
  {"name": "two",  "type": "INCORRECT", "outputs": {"1:1": "full-1.txt", "1:2": "full-2.txt"}}
]
```

A key is a test as eo-judge prints it, `"group:index"`, or its index alone when no other
testset has a test with that index; load refuses a key that is no test, two keys for one test
and a file that cannot be read. The file is relative to the problem directory and is given
to the checker as it is, as a program's output is, with no CRLF folding. A test the solution
gives no file for is judged as an empty file, and its message starts "no file was given for
this test"; the judge's own handling of a missing upload has not been verified. Nothing is
built or timed for such a solution, `stress` refuses the problem, and every other part of `run`
and `check` reads it as a `PROGRAM` problem's. A checker that reads neither the input nor the
answer on purpose says so with [`c.output_only("why")`](checker.md#output-only-problems).

`check` asks two more things of an `OUTPUT` problem's checker, since a contestant chooses
which file goes to which test: that it refuses an empty file on every test, not only the
first, as warning EO802, and that it refuses the jury's answer of the next test, when that
test's input differs, as this test's output, as warning EO822 — a checker that never looks at
the input would pass one good file uploaded for every test.

### Function problems

On a `FUNCTION` problem the contestant writes a function rather than a program, and the judge
compiles the template's header, the submission and the template's footer, in that order, as
one file: the header declares what the function needs, and the footer reads the input, calls
it and prints the result. eo-judge builds a solution the same way, from the template whose
`runtime` is the solution's, concatenating the three files as they are, with nothing added
between them:

```json
"type": "FUNCTION",
"templates": [
  {"runtime": "cpp:20-gnu14", "header": "templates/cpp-header.cpp",
   "source": "templates/cpp-source.cpp", "footer": "templates/cpp-footer.cpp"},
  {"runtime": "python:3.14-python", "header": "templates/python-header.py",
   "source": "templates/python-source.py", "footer": "templates/python-footer.py"}
],
"solutions": [
  {"name": "main", "source": "main.cpp", "runtime": "cpp:20-gnu14", "type": "CORRECT"},
  {"name": "first-two", "source": "first-two.cpp", "type": "WRONG_ANSWER"}
]
```

A solution without a `runtime` takes the problem's one C++ template; when there are several,
it names one. Load refuses a `FUNCTION` problem with no templates, a template with no runtime,
two templates for one runtime, a solution whose runtime has no template, and a solution in a
runtime other than C++, which eo-judge cannot build. Templates for other runtimes are loaded
and left to the judge; eo-judge builds the C++ ones. A template's `source` is the code a
contestant finds in the editor, the stub, and is not part of a solution's build.

A solution that does not compile inside its template stops `run` and `check` with exit 3, as
any solution that does not compile does, and the message names the template: a solution that
brings its own `main()` meets the footer's, which is what a contestant who submits a whole
program gets on the judge, a compilation error, here in GCC's words:

```
eo-judge: solution.with-main does not compile inside the template for cpp:20-gnu14, which is header, source and footer in one file; a FUNCTION problem's solution is the function alone, and the template gives the rest, main() included:
grader.cpp:1:5: error: redefinition of 'int main()'
solution.cpp:8:5: note: 'int main()' previously defined here
```

The line numbers are the solution's own because the header ends with `#line 1
"solution.cpp"` and the footer starts with `#line 1 "grader.cpp"`; [templates.md](templates.md)
has that pattern, and the Python and Java ones.

`check` reads every C++ template: a header whose last line is not a `#line` directive, or which
does not end with a line break, and a footer that does not start with one, are warning EO913; a stub that does not compile
inside its template, or that is judged as anything but a wrong answer, is EO823; and a whole
program, `int main() { return 0; }`, that compiles inside the template, so that a contestant
who submits one would not get the compilation error the judge gives, is EO824. `stress` builds
the reference and the solutions it compares inside their templates too.

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

With `--transcript` every run of an interactive problem is followed by its dialogue, which is
what a statement's example interaction is copied from:

```
binary: ACCEPTED, 100
  testset 1  ACCEPTED                 100 of 100      5 ACCEPTED
    1:2 ACCEPTED 2ms ok 9 queries
      interactor: 1000
      solution:   ? 500
      interactor: >
      solution:   ? 250
```

The two programs then talk through eo-judge, which copies each pipe as it reads it. The copy of
each direction is closed as soon as the program reading it ends, so a side that writes to a
peer that has left still gets the broken pipe it gets without the flag. What does change is
time: every message takes one more hop, so a run of many round trips can take up to about
twice as long, and one near its limit can exceed it under the flag; the copies also hold more
bytes than a bare pipe, so a pair that deadlocks on a full pipe without the flag may get further
with it. Judge with `-v` for verdicts, and with `--transcript` for the dialogue.

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

**With `-v`, `check` first shows which test has which feature**, a table per testset of the
features the validator declares with `v.feature` or `v.features`, `x` where `v.saw` marked one for that
test:

```
features of testset 1  caterpillar  path  star
  1:1                  .            x     .
  1:2                  .            .     x
```

A feature no test has is warning EO808 whether or not `-v` is given; the table is for seeing
where the others are. A validator that declares no feature prints no table, and `check -v`
then prints what `check` does.

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

## Stress

`eo-judge stress` does what the platform's stress run does, on your machine: it runs a
generator with random arguments, over and over, and compares the solutions with a reference on
every input it makes, until one of them does something its type does not allow.

```
$ eo-judge stress problems/sum --args '-n=[1..8] -max=[1..100]' --work /tmp/sum
stress: gen -n=[1..8] -max=[1..100] against brute, comparing twin, pairs, first; at most 100 iterations in 300 s

iteration 3: COUNTEREXAMPLE
  twin: ACCEPTED 1ms: ok the sum is 115
  pairs: WRONG_ANSWER 0ms, which breaks its type CORRECT: wrong answer the sum is 115, not 46
  first: WRONG_ANSWER 0ms: wrong answer the sum is 115, not 32
  "generator": {"script": "gen", "arguments": ["-n=3", "-max=73", "c83134a3824b3fe6"]}
  kept in /tmp/sum/stress/3: input.txt, answer.txt, twin/output.txt, pairs/output.txt, first/output.txt

eo-judge: 0 warning(s), 0 note(s)
eo-judge: iteration 3 of 100 is a counterexample
```

The `"generator"` line pastes into a test in `problem.json` as it is, and makes the same input
again: the arguments are resolved, and the seed is part of them.

| Flag | Default | Means |
| --- | --- | --- |
| `--gen name` | the one script the tests generate with | the generator, a name from `scripts` |
| `--args '…'` | none | its arguments, split at spaces; every `[a..b]` inside one becomes a random integer from `a` to `b`, drawn again on every iteration, and a random seed of 16 hexadecimal digits is appended, which is what [eolymp.h's generator](generator.md) recognises as a stress run |
| `--arg '…'` | | one argument, spaces and all, with its ranges drawn the same way; give it again for the next, and give either `--arg` or `--args`; eo-judge prints one with a space in it quoted |
| `--reference name` | the first `CORRECT` solution | the solution whose output is the answer; it must be `CORRECT`, as on the platform |
| `--solution name` | every solution but the reference and the `DONT_RUN` ones | a solution to compare with the reference; give it again for more |
| `--iterations n` | 100 | at most this many inputs |
| `--timeout s` | 300 | at most this many seconds for the whole stress |
| `--work dir` | a temporary directory | keep the workspace, and in it the iteration the stress stopped at |
| `--continue` | | go on past an `INVALID` or a `BROKEN` iteration, keeping each, and stop only at a `COUNTEREXAMPLE` |
| `-v` | | one line for every iteration, with its verdict and the generator's call |
| `--json` | | the result as one object; see [below](#json) |

The defaults are the platform's. The platform stops at 500 iterations and 600 seconds;
eo-judge takes more.

Every iteration makes an input with the generator and validates it, with no `--group`, as a
stress run does on the judge. It runs the reference on the input under the problem's
`timeLimit`, or 10 s when it has none, and takes its output as the answer, then runs each
solution under the same limit and checks its output against that answer. The checker is given
`TEST_COST=0`, `TEST_GROUP=0`, the iteration's number, from 1, as `TEST_INDEX` and an empty
`TEST_ID`, which is what [checker.md](checker.md#what-the-checker-knows-about-the-test) says a
stress run gives it; a test worth 0 accepts any points, so a partial score reads as `ACCEPTED`,
and eolymp.h's checker says so with warning EO208. The iteration's verdict is one of the
platform's:

| Verdict | Means |
| --- | --- |
| `PASSED` | every solution kept its type; the iteration's files are removed and the next one starts |
| `COUNTEREXAMPLE` | a solution broke its type |
| `INVALID` | the validator refused the input, so the generator is at fault: its options allow an input the statement does not |
| `BROKEN` | the generator or the reference did not finish, the validator could not run, its first line starting with `eolymp.h: ` or killed by a signal, or ran out of its 30 s, or the checker failed on a solution's output |

The stress stops at the first iteration that did not pass, prints it, and keeps its files under
`--work`: at a `COUNTEREXAMPLE`, and at an `INVALID` or a `BROKEN` one too, which the
platform's run records and goes past; its `continueOnFailure` is about counterexamples, and
eo-judge has no such switch. With `--continue` eo-judge goes past `INVALID` and `BROKEN` as the
platform does, printing and keeping each. What breaks a type is what the platform reads: a `CORRECT` solution that is not
`ACCEPTED`, and a solution declared `WRONG_ANSWER`, `TIMEOUT` or `TIMEOUT_OR_ACCEPTED` that
gets a verdict its type does not allow, as in the table of [expected
types](#expected-types): a `WRONG_ANSWER` solution that crashes, not one that answers wrong.
A solution with no type, `INCORRECT`, `FAILURE`, `OVERFLOW` or `OVERFLOW_OR_ACCEPTED` never
breaks it, `OVERFLOW` because eo-judge does not measure memory; a stress that compares only
such solutions is refused, since it could find nothing. To look for an input a solution fails
on, declare it `CORRECT` and compare it with a brute force as the reference.

It exits 0 when every iteration passed, or when `--timeout` ended the stress after one had, and
1 when it stopped at an iteration or went past one with `--continue`; an iteration the timeout
cut short is dropped rather than blamed on the solution it stopped. A timeout before any
iteration passed has found nothing, so it exits 3 and names the program it cut short: `the 1 s
timeout ended the stress in iteration 1 while the solution slow ran`. It exits 2 on a usage
error, and 3 too when a program does not build or the problem is `INTERACTIVE`,
`COMMUNICATION` or `OUTPUT`, which `stress` does not run. The programs are built once, from [the
cache](#cache), and the warnings the generator and the checker raise, such as EO501 for a
generator that never draws and EO208 for a partial score on a test worth nothing, are reported
once each, as `run` reports them.

## JSON

With `--json`, `run`, `check`, `lint` and `stress` print nothing on stdout but one object, and
the exit code is the same as without it; `version --json` prints `{"version": "2.3.0"}`, and `init`
refuses the flag:

```json
{
  "version": "2.3.0",
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
| `attempts` | what `run` judged, in the order of `solutions`; empty for `check`, `lint` and `stress` |
| `coverage` | from `check`, the features the validator declares, as `features`, and for each test its `group`, `test` and the `features` it has, in the order of `testsets`; left out when the validator declares none |
| `stress` | what `stress` did, and left out for the other commands: `generator`, `arguments` as given, `reference`, the compared `solutions`, `iterations` asked for, how many `passed`, whether the `deadline` ended it, the iterations `--continue` went past as `failed`, and the iteration it `stopped` at, left out when it stopped at none, with its `index`, `verdict`, resolved `arguments`, `why` for `INVALID` and `BROKEN`, the directory it was `kept` in under `--work`, and the `results` of the solutions, each with its `solution`, `type`, `verdict`, `ms`, the checker's `message` and whether it was `unexpected`, the platform's word for breaking its type |
| `breaks` | under `--expect`, why the solution breaks its type; left out when it holds |
| `transcript` | under `--transcript`, a run's dialogue as the text shows it, one string a line; left out without the flag and for a run in which nothing was said |
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
      - uses: eolymp/h@v2.3.0
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

## On Windows

eo-judge does not run natively on Windows: a Windows build stops at once with a message that
points here. Run the Linux eo-judge under [WSL2](https://learn.microsoft.com/windows/wsl/install),
which is the Linux judge's own toolchain and gives the same results:

```bash
wsl --install                     # once, from an administrator's PowerShell
git clone <your problems> ~/problems && cd ~/problems
eo-judge run <problem>
```

Keep the checkout in WSL's own file system, under `~`, rather than on `/mnt/c`, which is many
times slower to build and read from. A test written on the Windows side may come with CRLF line
breaks, from an editor or from Git's `core.autocrlf`; the library reads them as the judge does
and says so with an EO110 note, and a `.gitattributes` line such as `*.txt text eol=lf` keeps
them out of the repository. The jury programs themselves build and judge natively on Windows
with MSVC, clang-cl and mingw-w64; see [Windows](README.md#windows).

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
