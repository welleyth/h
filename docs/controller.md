# Writing a controller for a communication problem

In a **communication problem** several copies of the solution run **at the same time**, and
none of them can reach the others. Each copy is an **instance**: a separate process, in its
own working directory, with its own limits. A jury program, the **controller**, starts the
instances, talks to each through its own pair of pipes, passes on whatever the rules allow,
and decides the outcome.

```
               ┌──────────► instance 1
  controller ──┼──────────► instance 2        each arrow is a pair of pipes
               └──────────► instance 3
```

This is Eolymp's `COMMUNICATION` problem type. **If the copies run one after another** — an
encoder and then a decoder — the problem is an ordinary interactive problem with `run_count`
set, which is simpler and lives in [interactor.md](interactor.md).

As with an interactor, the library writes a **summary** and the problem's checker turns it
into the verdict. This page describes the library as built.

## A first controller

A relay: the test gives a number of instances and a secret. The first instance is told the
secret; each may pass one short message to the next; the last must say the secret. No
instance sees another's memory or files, so the messages are the only way the secret travels.

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::controller ctl(argc, argv);
    int k = ctl.input.read_int(2, 100, "instances");
    long long secret = ctl.input.read_long(eo::any, "secret");

    eo::channel& first = ctl.spawn();
    first.send("first", secret);
    std::string message = first.read_token(1, 20, eo::charset("a-z"), "message");
    first.close();

    for (int at = 2; at < k; at++) {
        eo::channel& middle = ctl.spawn();
        middle.send("middle", message);
        message = middle.read_token(1, 20, eo::charset("a-z"), "message");
        middle.close();
    }

    eo::channel& last = ctl.spawn();
    last.send("last", message);
    long long said = last.read_long(eo::any, "secret");
    if (said != secret) eo::wrong("the last instance said {}, the secret was {}", said, secret);
    eo::accept("the secret crossed {} instances", k);
}
```

The checker is the same one line as for any interactive problem:

```cpp
#include <eolymp.h>

int main(int argc, char** argv) { eo::checker(argc, argv).from_interactor(); }
```

## How the judge runs it

| | Value |
| --- | --- |
| arguments | `input.txt output.txt`, and `answer.txt` when the test has one |
| stdin | the test — **not** a pipe, unlike a plain interactor |
| stdout and stderr | both `interactor.log`, kept for every run |
| environment | the interactor's, plus `CONTROL_INPUT_FILE`, `CONTROL_OUTPUT_FILE` and `INSTANCE_LIMIT` |
| time | the problem's interactor time limit, for the whole test |
| instances | each gets the problem's own time and memory limit |

The exit codes are the interactor's: 0 runs the checker on the summary, 1 or 2 are
WRONG_ANSWER without it, anything else is INTERACTION_FAILURE. One difference: when the
controller exits 0, an instance that died because its pipe was closed counts as having
finished normally, so closing a channel early is safe.

**Configuring the problem.** The type is `COMMUNICATION`, with the controller as the
interactor and the stock checker. `instance_limit` is the most instances a test may start —
set it a little above the largest number any test needs. `timeLimit` and `memoryLimit` apply
to **each instance**, while `interactor_time_limit` bounds the controller for the whole test
and must cover every instance's work.

**How `spawn` works**, for the curious: the controller writes the line `SPAWN` to
`CONTROL_OUTPUT_FILE`, and the judge replies on `CONTROL_INPUT_FILE` with either the two FIFO
paths of a new instance or the word `LIMIT`. `ctl.spawn()` does all of it, opening the two
ends in the order that cannot deadlock with an instance.

## The controller

| Member | Does |
| --- | --- |
| `ctl.input`, `ctl.jury` | the test and its answer file; a failed read is a jury error |
| `ctl.has_jury()` | whether the test has an answer file |
| `ctl.instance_limit()` | the most instances this problem allows per test |
| `ctl.spawn()` | starts an instance and returns its `eo::channel&` |
| `ctl.value("name", x)` | records a number in the summary for a custom checker |
| `ctl.rng()` | a random stream seeded from the test |
| `ctl.cost()`, `ctl.round_trips()` | the test's points, and the traffic so far |

The verdicts are the shared ones — `eo::accept`, `eo::score`, `eo::points`, `eo::wrong` and
`eo::jury_error` — and they behave exactly as in an interactor. `eo::ratio(a, b)` gives an
exact fraction. Returning from `main` without a verdict is a jury error.

**A spawn beyond the limit is the problem's fault, not the contestant's**, so it is a jury
error that names the configured limit:

```
jury error relay.cpp:20: the judge refused instance 3; instance_limit is 2
```

## Channels

`ctl.spawn()` returns a reference to an `eo::channel`: the controller's end of one instance's
two pipes. The controller owns them, so a channel is never copied or moved by the author.

| Member | Does |
| --- | --- |
| `ch.read_int`, `read_long`, `read_real`, `read_token`, `read_choice`, `read_longs`, `at_eof` | read what the instance printed |
| `ch.send(values…)` | one line to the instance: the values separated by single spaces |
| `ch.close()` | closes the pipe; the instance sees the end of its input and can finish |
| `ch.index()` | the instance's number, counting spawns from 1 |

Reading from a channel works like reading the solution in an interactor: whitespace is
lenient, integers follow the strict syntax, bounds come first and the name last, and a number
read without bounds gets warning EO103. What changes is that every message names the
instance:

```
wrong answer: instance 1, line 1, x: expected an integer, found "maybe"
wrong answer: instance 1, line 1, message: instance 1 ended the dialogue early
```

**Flushing.** Whatever the controller sends to any channel is buffered. Before it waits on
*any* channel, the library flushes *every* channel — so an instance can never sit waiting for
a line that is still in the controller's buffer. A write to an instance that stopped reading
is not a verdict: what could not be written is dropped, nothing more is written to that
instance, and the next read from it decides, so sending a last line and closing a channel
is safe. `SIGPIPE` is ignored. Unlike an interactor, a controller does not take in an
instance's answers while it waits to write: sending one instance more than a pipe holds,
about 64 KB, before reading its answers, while that instance answers each line as it reads
it, leaves both waiting. Read the answers in between, or send less at a time.

**Read the channels in an order your code fixes**, such as instance 1, then 2, then 3. Never
let the dialogue depend on which instance happens to answer first: that depends on the
machine's timing, and the same submission would get different verdicts on a rejudge.

```cpp
for (eo::channel& ch : team) total += ch.read_long(0, 1000000000, "message");
for (eo::channel& ch : team) ch.send(total);
```

A budget limits questions exactly as in an interactor:

```cpp
eo::budget queries(ctl, 100, "queries");
queries.spend();
```

## Warnings

The rules are those of every eolymp.h program, and the codes are the interactor's plus one:

| Code | Fires when |
| --- | --- |
| EO101, EO103, EO104, EO105, EO107, EO111 | reads without a name or bounds, as everywhere |
| EO204 | `eo::wrong` carries no message |
| EO205 | a fraction outside [0, 1], or negative points, was clamped |
| EO213 | on the judge, `TEST_COST` is missing or not a number, so the cost of `eo::points` is a guess |
| EO401 | more than 100,000 round trips (a note), or more than 500,000 (a warning) |
| EO402 | more than 10,000 round trips with no `eo::budget` declared |
| EO403 | a budget was declared and never spent |
| EO405 | the controller accepted without reading anything from any instance |
| EO408 | an instance was started and never talked to |

## Testing a controller locally

A controller cannot be run by hand the way an interactor can: it needs a judge on the other
end of the control channel. This repository's `tests/e2e/serve.cpp` is a hundred lines that
play that part — it makes the FIFOs, answers `SPAWN`, starts a solution on each pair, and
refuses past the limit — and the repository's own gate uses it on every build to relay a
secret across three real instances and to check that the limit, a silent instance and a lying
one each end the way they should.

## Porting an existing controller

| Existing code | eolymp.h |
| --- | --- |
| EGOI's `controller_lib.h`: `eolymp_spawn` and the `FILE*` pair it returns | `ctl.spawn()` and an `eo::channel&` |
| reading an instance with `fscanf` | `ch.read_int(low, high, name)` |
| writing with `fprintf` and `fflush` | `ch.send(…)` |
| a CMS manager's `result(…)`, or a fraction printed to a file | `eo::score(f, …)` |
| `grade(score, msg, admin_msg)` | `eo::score(f, "…")`, and `eo::log` for the jury's eyes |

The handshake itself is the same one the ported managers use — the same request, the same
reply, the same order of opening the two ends — so a port is a rewrite of the dialogue, not
of the plumbing.

## Not here yet

- `eolymp-cms.h`, the adapter that lets a CMS manager compile unchanged on top of
  `eo::controller`.
EO814 (hostile instances) and EO815 (the same solution twice) are `eo-judge check`'s, and
[eo-judge](judge.md) refuses a `COMMUNICATION` problem until it can drive several instances,
so neither runs on a controller yet. The end-to-end gate does a small version of EO814.

## Reference card

| Member | Does |
| --- | --- |
| `eo::controller ctl(argc, argv)` | makes the program a controller |
| `ctl.input`, `ctl.jury`, `ctl.has_jury()` | the test and its answer file |
| `ctl.instance_limit()` | the most instances per test |
| `ctl.spawn()` | starts an instance, returns its `eo::channel&` |
| `ch.read_…`, `ch.send(…)`, `ch.close()`, `ch.index()` | talk to one instance |
| `eo::budget b(ctl, limit, name)` | a query limit |
| `ctl.value("name", x)`, `ctl.rng()`, `ctl.cost()`, `ctl.round_trips()` | the rest |
| `eo::accept`, `eo::score`, `eo::points`, `eo::wrong`, `eo::jury_error` | end the test |
