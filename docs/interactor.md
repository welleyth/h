# Writing an interactor with eolymp.h

In an **interactive problem** the solution does not read its data from a file. It talks to a
jury program, the **interactor**, while it runs: it asks questions and gets answers, makes
moves and sees the replies. The interactor holds the hidden data, answers each request,
enforces the rules, and decides how well the solution did.

```
             its stdout ──────────────►  its stdin
  solution                                           interactor ◄── the test input
             its stdin  ◄──────────────  its stdout             ──► a summary for the checker
```

On Eolymp an interactor cannot hand out points through its exit code. When it finishes, the
library writes a short **summary** of the outcome, and a checker turns that into the verdict.
The checker is one line and the same for almost every interactive problem.

This page describes the library as it is built. Everything here works; nothing here is
planned, and §Not here yet lists what is still missing.

## A first interactor

Guess a hidden number between 1 and `n` in at most 20 questions. The solution prints `? x`
and learns whether the number is smaller (`<`), larger (`>`) or equal (`=`); it answers with
`! x`. The test holds `n` and the hidden number.

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::interactor it(argc, argv);
    int n = it.input.read_int(1, 1000000, "n");
    int secret = it.input.read_int(1, n, "secret");
    eo::budget queries(it, 20, "queries");

    it.send(n);
    for (;;) {
        std::string command = it.contestant.read_choice({"?", "!"}, "command");
        int x = it.contestant.read_int(1, n, "x");
        if (command == "!") {
            if (x != secret) eo::wrong("answered {}, the number was {}", x, secret);
            eo::accept("{} queries", queries.used());
        }
        queries.spend();
        it.send(x < secret ? "<" : x > secret ? ">" : "=");
    }
}
```

And the checker, the same one line for every problem whose interactor uses eolymp.h:

```cpp
#include <eolymp.h>

int main(int argc, char** argv) { eo::checker(argc, argv).from_interactor(); }
```

- `eo::interactor it(argc, argv)` makes the program an interactor. It opens the test, the
  pipes and the file for the summary, and ignores `SIGPIPE`.
- `it.input` is the test; `it.contestant` is what the solution prints.
- `it.send(n)` writes one line to the solution. Output is buffered and flushed automatically
  whenever the interactor is about to wait for the solution, so the forgotten flush cannot
  deadlock the pair.
- `read_choice` and `read_int(1, n, "x")` read the solution's request. Garbage, a value out
  of range, and the solution ending early are all wrong answers.
- `queries.spend()` counts a question and fails the solution on the 21st, **before** it is
  answered.

## Setting up

Include the header before anything else, in **both** programs — the interactor and the
checker. Neither attaches it: the judge's C++ runtime carries it, so both compile against the
one release that image holds, which is what they need — they share the summary's format.

**Never print with `std::cout` or `printf` in an interactor.** stdout is the pipe to the
solution. Use `it.send` for the solution and `eo::log` for the log, which is stderr.

## How the judge runs it

| | Value |
| --- | --- |
| arguments | `input.txt output.txt`, and `answer.txt` when the test has one |
| stdin | the pipe from the solution |
| stdout | the pipe to the solution |
| stderr | `interactor.log`, kept for every run |
| environment | `INPUT_FILE`, `OUTPUT_FILE`, `ANSWER_FILE`, `TEST_COST`, `TEST_GROUP`, `TEST_INDEX`, `TEST_ID` |
| time | the solution's wall limit plus 1 s, or the problem's interactor time limit |
| memory | 2 GiB |

The judge looks at the solution first: if it exceeded a limit or crashed, that is the
verdict whatever the interactor did. Otherwise the interactor's exit code decides:

| Exit code | Result |
| --- | --- |
| 0 | the checker runs and grades the summary |
| 1 or 2 | WRONG_ANSWER; the checker does not run |
| anything else | INTERACTION_FAILURE: the judge failed, and the submission lands in FAILURE |

**Set a wall `timeLimit`, and set it generously.** The interactor's own time is the
solution's wall limit plus one second, so without one it is killed after about a second.
Make `cpuLimit` the real limit for the solution: every round trip costs wall time, and the
pipe's system calls are billed to the solution's CPU time. A pipe manages roughly 150,000
round trips a second.

## The streams

| Stream | Holds | A failed read is | A number with no bounds |
| --- | --- | --- | --- |
| `it.input` | the test | a jury error | fine: the validator has checked it |
| `it.jury` | the answer file, when there is one | a jury error | warning EO103 |
| `it.contestant` | what the solution prints | a wrong answer | warning EO103 |

`it.has_jury()` says whether the test has an answer file; one passed and never read gets
note EO406.

`it.contestant` is a pipe, so a read **waits** until the solution has written enough. If the
solution ends instead, the read fails with a wrong answer:

```
wrong answer: the solution, line 1, x: the solution ended the dialogue early
```

A solution that goes silent without ending is stopped by its own time limit, and that
verdict wins.

**Bound everything the solution sends.** An interactor that reads a count and loops that
many times hangs on 2³¹−1; one that uses a value as an index crashes on a negative. Both
are INTERACTION_FAILURE — a system failure where a wrong answer belonged. A bounded read
turns them into a wrong answer on the spot:

```
wrong answer: the solution, line 1, x: 5000 is above 100
```

The read functions are the checker's, patterns included: `read_int`, `read_long`,
`read_real`, `read_token`, `read_line`, `read_choice`, `read_ints`, `read_longs`, `read_reals`, `read_tokens`,
`read_grid`, `read_edges`, `read_tree`, `read_graph`, `at_eof`, `at_eoln`, with bounds first and the name last, and `eo::any` where a value really
may be anything. They are described in [checker.md](checker.md).

## Sending

`it.send(...)` writes one line: the arguments separated by single spaces, then a line break.
A container is written as its elements.

```cpp
it.send(n);
it.send(n, m);
it.send("YES");
it.send(values);
```

- **Output is buffered and flushed automatically**, right before the interactor waits for
  the solution, and at exit. Several replies between two reads cost one system call, and a
  reply can never sit in a buffer while both programs wait for each other. `it.flush()`
  exists but is rarely needed.
- **A large batch cannot deadlock either.** When the interactor sends more than the pipe
  holds, about 64 KB, to a solution that answers each line as it reads it, the solution's
  answers fill the other pipe while the interactor is still writing. The library keeps
  taking them in while it waits for room, up to 16 MB, and they are read from there as
  usual; past that it stops, says so with warning EO409, and the two can wait for each
  other until the time limit. EO409 goes to stderr the moment it is raised, since a run
  killed at the limit never writes its report.
- **The solution may have stopped reading.** A correct solution often exits right after its
  last answer, before the interactor's last line reaches it. A write to a closed pipe is
  therefore not a verdict: the library drops what could not be written, stops writing to the
  solution, and lets the next read decide — a read past the end of what the solution said is
  "the solution ended the dialogue early". `SIGPIPE` is ignored, so the interactor is never
  killed by it.

## Limiting queries

```cpp
eo::budget queries(it, 20, "queries");
queries.spend();
```

| Member | Does |
| --- | --- |
| `eo::budget b(it, limit, name)` | declares a limit |
| `b.spend()`, `b.spend(k)` | counts one, or k; beyond the limit the run ends with `wrong answer more than 20 queries` |
| `b.used()`, `b.left()`, `b.limit()` | how many have gone, how many remain, and what the limit is |
| `b.restart()`, `b.restart(limit)` | clears what was spent, keeping the limit or replacing it |

Spend the budget **before** replying, so the question over the limit never gets a truthful
answer. Several budgets can coexist.

**A limit that is per round rather than per run** is what `restart` is for: a task allowing
1000 queries *per hunt* declares one budget and calls `queries.restart()` at the start of each
hunt. `restart(limit)` also replaces the limit, for a round whose allowance moves. A run with more than 10,000 round trips and no budget
declared gets warning EO402; a budget declared and never spent gets EO403.

## Ending the interaction

| Call | Exit | The summary says | Result |
| --- | --- | --- | --- |
| `eo::accept(…)` | 0 | full marks | ACCEPTED, the test's full cost |
| `eo::score(f, …)` | 0 | the fraction `f` | the full cost at 1, `f × cost` between, PARTIALLY_CORRECT at 0 |
| `eo::points(p, …)` | 0 | `p / cost` | as `score` |
| `eo::wrong(…)` | 1 | nothing: the checker does not run | WRONG_ANSWER, 0 |
| `eo::jury_error(…)` | 3 | nothing | INTERACTION_FAILURE |

`eo::ratio(a, b)` gives an exact fraction and `eo::round_to(d)` rounds the points, exactly as
in a checker. `it.value("quality", q)` records a named number in the summary for a checker
that wants to do its own mapping; the name is one word and the number is finite. Returning from `main` without a verdict is a jury error, and so is calling `std::exit` or
`std::quick_exit` before one, with any code, or leaving the interactor undestroyed: `jury error
the interactor ended without a verdict: exit() was called, or the interactor was never
destroyed`. `std::_Exit` and `std::abort` run nothing on the way out and cannot be caught, and
`std::quick_exit` is caught only where the C library offers `at_quick_exit`: glibc and musl,
so the judge, and not macOS.

The summary is a small text file the library writes and the stock checker reads. You never
write it or read it yourself.

## The checker for an interactive problem

```cpp
#include <eolymp.h>

int main(int argc, char** argv) { eo::checker(argc, argv).from_interactor(); }
```

When the score depends on something only the checker knows, such as the subtask, pass a
function. It receives the summary and returns the fraction to award:

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    c.from_interactor([&](eo::summary const& said) {
        return c.group() == 7 ? said.value("quality") : said.fraction();
    });
}
```

| Summary member | Holds |
| --- | --- |
| `said.fraction()` | the score the interactor gave: 1 for `accept`, `f` for `score(f)` |
| `said.value("name")` | a number recorded with `it.value` |
| `said.has("name")` | whether it recorded one |
| `said.message()` | the interactor's message |

The summary is jury data: a malformed one is a jury error.

## Randomness

An adaptive interactor, or one that shuffles its answers, needs random numbers. `it.rng()`
gives a generator seeded from the test, so the same solution gets the same dialogue and the
same verdict on every rejudge. There is no access to the clock.

| Call | Returns |
| --- | --- |
| `r.uniform(low, high)` | an integer in `[low, high]` |
| `r.real(low, high)` | a real number in `[low, high)` |
| `r.chance(p)` | true with probability `p` |
| `r.pick(container)` | a random element |
| `r.shuffle(values)` | shuffles in place |
| `r.perm(n)`, `r.perm(n, first)` | a random permutation |
| `r.ints(count, low, high)` | independent integers |
| `r.distinct(count, low, high)` | different integers |
| `r.weighted(low, high, lean)` | an integer skewed towards `high` when `lean > 0`, towards `low` when negative |
| `r.pair(low, high)` | two integers drawn in a fixed order |
| `r.partition(count, sum)` | positive integers adding up to `sum` |
| `r.letters(length, eo::charset("a-z"))` | a string over those characters |

It is the same `eo::rng` a generator draws from, so every draw listed in
[generator.md](generator.md) is available here too. The results do not depend on the compiler
or the standard library, unlike `std::shuffle` or `std::uniform_int_distribution`.

## The log

The interactor's stderr is `interactor.log`, kept for every run. `eo::log("…", args)` writes
a line to it. The library adds the verdict first and then the traffic:

```
ok 9 queries
10 round trips, 23 bytes sent
```

That round-trip count is the number that decides whether a problem fits the pipe. Over
100,000 it becomes note EO401, and over 500,000 a warning.

## Problems that run in phases

Some tasks run the solution **several times per test** and forbid it to remember anything
between the runs: an encoder and a decoder, Alice and Bob. On Eolymp this is `run_count`.
With `run_count` set to k the solution runs k times, each time a fresh process in a clean
working directory, and **run i+1's input is run i's interactor output**. That file is the
only channel between the runs, and only the jury writes to it.

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::interactor it(argc, argv);
    eo::phases ph(it, 2);
    long long x = it.input.read_long(0, 1000000000, "x");
    if (ph.number() == 1) {
        it.send("alice", x);
        std::string code = it.contestant.read_token(1, 60, eo::charset("01"), "code");
        ph.handoff([&](eo::writer& w) { w.line(code); });
    }
    std::string code = ph.previous().read_token(1, 60, eo::charset("01"), "code");
    it.send("bob", code);
    long long said = it.contestant.read_long(eo::any, "x");
    if (said != x) eo::wrong("Bob answered {}, the number was {}", said, x);
    if (code.size() <= 30) eo::accept("{} characters", code.size());
    eo::score(eo::ratio(30, static_cast<long long>(code.size())), "{} characters", code.size());
}
```

| Member | Does |
| --- | --- |
| `eo::phases ph(it, k)` | declares that the interaction has `k` phases |
| `ph.number()` | the current phase, from 1 to `k` |
| `ph.handoff(write)` | writes the file the next phase gets, and ends this run successfully |
| `ph.previous()` | a stream over what the previous phase wrote (jury data) |
| `ph.finish(f, …)` | ends the whole test with the fraction `f` from an early phase |
| `eo::writer`: `w.line(values…)`, `w.line()` | the lines of a handoff |

**`it.input` reads the original test in every phase.** The library copies it into each
handoff, so a later phase does not depend on what the previous one chose to pass on.
`ph.handoff` ends the run, so the rest of `main` only ever runs in the last phase.

**Set `run_count` to k and `interactive_followup` to true**, and send the problem `type` in
the same update, because the update rewrites it.

Both ways of getting `run_count` wrong now say so instead of scoring noise:

| Configuration | What happens |
| --- | --- |
| `run_count` smaller than the phases | the checker finds a handoff where a summary belongs: `jury error: output.txt, line 1: the interactor stopped at phase 1 of 2; set run_count to 2` |
| `run_count` larger than the phases | the extra run finds a finished summary as its input: `jury error run_count is more than the 2 phases the interactor declares` |
| a handoff over 64 MB | warning EO407 |

**Ending early.** A wrong answer in any phase is `eo::wrong`, exit 1: the judge stops the
chain and the test scores 0. An early phase that wants to end with a *partial* score calls
`ph.finish(f)`; the fraction rides in the handoff and the later phases pass it through
without contacting the solution. Those runs still start the solution, so say in the statement
that a run may receive no input.

## Warnings

The rules are those of every eolymp.h program: a warning never changes a verdict except in
strict mode (`EOLYMP_STRICT=1`); each carries a stable code, a line and a fix, the line
of your source for a read, a bound or a clamped score and the header's own line for how the
run ended (EO204, EO402–EO406); each is counted once per call site; `eo::allow quiet("EO402", "why")` silences one code
in a scope, with a reason the report prints. They appear on stderr, which on the judge is
`interactor.log`.

| Code | Fires when |
| --- | --- |
| EO101 | a value is read without a name |
| EO103 | a number from the solution or the answer file is read without bounds |
| EO104, EO105, EO106, EO107 | bounds that look wrong, as in a checker |
| EO111 | a token over 1 MB from the solution was held in memory (a note) |
| EO204 | `eo::wrong` carries no message |
| EO205 | a fraction outside [0, 1], or negative points, was clamped |
| EO213 | on the judge, `TEST_COST` is missing or not a number, so the cost of `eo::points` is a guess |
| EO401 | more than 100,000 round trips (a note), or more than 500,000 (a warning) |
| EO402 | more than 10,000 round trips with no `eo::budget` declared |
| EO403 | a budget was declared and never spent |
| EO404 | the solution was still sending when the interactor finished (a note) |
| EO405 | the interactor accepted without reading anything from the solution |
| EO406 | an answer file was passed and never read (a note) |
| EO407 | a phase handoff exceeded 64 MB |
| EO409 | the solution sent more than 16 MB while the interactor was still writing to it |

## Testing an interactor locally

Build it like any C++ program. In a terminal **you are the solution**: run it, read what it
prints, and type the solution's lines.

```bash
./interactor input.txt output.txt
```

When it ends, `output.txt` holds the summary and the exit code is the one the judge would
see. To run a real solution against it, connect the two with pipes the way the judge does;
`tests/e2e/play.cpp` in this repository is a short program that does exactly that, and the
repository's own gate uses it to run a correct solution, a two-phase chain, and seven badly
behaved solutions on every build.

## Not here yet

Still missing:

- The member forms `it.accept`, `it.score`, `it.points`, `it.wrong` and `it.jury_error`. The
  free functions do the same and work in a checker too.
EO814 (hostile clients) and EO815 (the same solution twice) are run by `eo-judge check`; see
[judge.md](judge.md). The repository's own end-to-end gate does a small version of EO814 on
every build.

## Reference card

| Member | Does |
| --- | --- |
| `eo::interactor it(argc, argv)` | makes the program an interactor |
| `it.input`, `it.jury`, `it.contestant` | the streams |
| `it.has_jury()` | whether the test has an answer file |
| `it.send(values…)` | one line to the solution, flushed before the next read |
| `it.flush()` | send now; rarely needed |
| `it.value("name", x)` | record a number for a custom checker |
| `it.rng()` | the deterministic random stream |
| `it.cost()`, `it.round_trips()` | the test's points, and the traffic so far |

| Function | Does |
| --- | --- |
| `eo::accept`, `eo::wrong`, `eo::score`, `eo::points`, `eo::jury_error` | end the run |
| `eo::budget b(it, limit, name)` | a query limit; `spend`, `used`, `left`, `limit`, `restart` |
| `eo::phases ph(it, k)` | a `run_count` interaction; `number`, `handoff`, `previous`, `finish` |
| `eo::writer` | the lines of a handoff |
| `eo::summary` | what a `from_interactor` mapping receives |
| `eo::ratio`, `eo::round_to`, `eo::fmt`, `eo::log`, `eo::any`, `eo::charset`, `eo::allow` | the shared helpers |
