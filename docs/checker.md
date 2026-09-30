# Writing a checker with eolymp.h

Eolymp runs every submission on every test. After each run the **checker** looks at three
files — the test, what the submission printed, and the jury's answer — and decides what the
run is worth: accepted, a wrong answer, a partial score, or a jury error, which says the
problem is broken rather than the contestant.

A problem needs a checker whenever comparing the output with the answer token by token is
not enough: several answers are correct, the answer is a real number, or the task gives
partial credit.

This page describes the library as it is built. Everything here works; nothing here is
planned, and §Not here yet lists what is still missing.

## A first checker

The task: print the sum of `n` numbers.

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    long long expected = c.jury.read_long(eo::any, "sum");
    long long found = c.output.read_long(eo::any, "sum");
    if (found != expected) eo::wrong("the sum is {}, not {}", expected, found);
    eo::accept("the sum is {}", found);
}
```

- `eo::checker c(argc, argv)` makes the program a checker. It finds the three files, reads
  the test's cost, and takes over the log.
- `c.input` is the test, `c.jury` the answer file, `c.output` what the contestant printed.
- `eo::any` says "any value a `long long` can hold is fine"; without it the library asks for
  bounds (§Names and bounds).
- `eo::wrong` and `eo::accept` end the program with the right exit code and message.

Nothing else is needed. The library rejects an output that does not start with an integer,
turns a malformed answer file into a jury error, checks that nothing but whitespace follows
the answer, and writes the log in the shape the judge parses.

## Setting up

Include the header before anything else, because organiser code sometimes contains
`#define int long long`:

```cpp
#include <eolymp.h>
```

It needs C++17 and builds unchanged as C++20 and C++23.

**The checker attaches nothing.** The judge's C++ runtime carries the header at
`/usr/include/eolymp.h`, so the angle-bracket `#include` finds it.

Build and run it like any C++ program, with the header beside the source. The three files
come from the environment the judge sets, or from the command line when it does not:

```bash
g++ -std=c++17 -O2 -I. -o checker checker.cpp
```

```bash
TEST_COST=40 ./checker input.txt output.txt answer.txt
```

## How the judge runs it

| | Value |
| --- | --- |
| arguments | `input.txt output.txt answer.txt`; the legacy checker type swaps the last two, and the library notices |
| environment | `INPUT_FILE`, `OUTPUT_FILE`, `ANSWER_FILE`, `TEST_COST`, `TEST_GROUP`, `TEST_INDEX`, `TEST_ID` |
| time | 10 s of wall time |
| log | stdout and stderr merged into `checker.log`, kept for every run |

| Exit code | Result |
| --- | --- |
| 0 | ACCEPTED, the test's full cost |
| 1 or 2 | WRONG_ANSWER, 0 points |
| 7 | a partial score, read from `points <number>` in the log, awarded as `min(cost, points)` |
| anything else | VERIFICATION_FAILURE |

The input and the answer reach the checker with CRLF line endings already converted to LF.
**The contestant's output is not converted**, so it may hold `\r`; the library drops a
trailing one when it reads a line.

Two consequences the library takes care of:

- **A full score must exit 0, not 7.** An exit 7 is not an accept even at full points, and a
  testset that needs every test passed then awards nothing. `eo::score(1.0)` exits 0.
- **Points are absolute, not a percentage.** A test worth 11 that receives `points 90.9` is
  clamped to 11. `eo::score` takes a fraction of `TEST_COST` and multiplies; a percentage
  given to it by mistake is clamped to full marks with warning EO205, which calls it a likely
  percentage, and `EOLYMP_STRICT=1` or `eo-judge --strict` makes that fatal while you prepare.

A run that earns a partial score is labelled **`PARTIALLY_CORRECT`**, so the verdict matches
the score instead of reading as a plain wrong answer.

**A scored zero and a wrong answer are different runs.** `eo::score(0)` writes `points 0` and
exits 7, so it is a scored run that happened to earn nothing, and reads as
`PARTIALLY_CORRECT`; `eo::wrong(…)` exits 1 and means the output is not a valid answer at all.
They pay the same and read differently. Use `wrong` when the output is malformed or plainly
incorrect, and `score(0)` when the formula simply came out at nothing.

**A partial score on a test that carries no points reads as ACCEPTED**, because the points it
earns equal that test's cost. So the layout "the subtask's whole value on the first test and 0
on the rest" turns every test after the first into a free pass for a scoring checker: give a
partially scored subtask its value on **every** test and score the testset `WORST`.

## The three streams

| Stream | Holds | A failed read is | A number with no bounds |
| --- | --- | --- | --- |
| `c.input` | the test | a jury error | fine: the validator has checked it |
| `c.jury` | the answer file | a jury error | warning EO103 |
| `c.output` | what the contestant printed | a wrong answer | warning EO103 |

So the same call on two streams means two different things, and you never decide which:

```
wrong answer: output.txt, line 1, sum: expected an integer, found "abc"
jury error: answer.txt, line 1, sum: expected an integer, found "abc"
```

All three are **lenient about whitespace**: any amount of spaces, tabs, line breaks and `\r`
may separate two values, and a token read does not care which line it is on. `read_line` is
the one read that looks at lines.

## Reading

Bounds are inclusive, and come first; the name is last.

| Call | Reads |
| --- | --- |
| `s.read_int(low, high, name)` | an integer in `[low, high]` that fits an `int` |
| `s.read_long(low, high, name)` | an integer in `[low, high]` |
| `s.read_real(low, high, name)` | a real number in `[low, high]` |
| `s.read_token(least, most, eo::charset("a-z"), name)` | a token of that length over those characters |
| `s.read_line(least, most, eo::charset("a-z "), name)` | the rest of the line, without its line break |
| `s.read_choice({"YES", "NO"}, name)` | a token equal to one of the choices |
| `s.read_choice({"YES", "NO"}, eo::any_case, name)` | the same, ignoring letter case, returning the choice as you wrote it |
| `s.read_ints(count, low, high, name)` | `count` integers |
| `s.read_longs(count, low, high, name)` | the same, 64-bit |
| `s.read_reals(count, low, high, name)` | `count` real numbers |
| `s.read_tokens(count, least, most, eo::charset("a-z"), name)` | `count` tokens |

`eo::any` replaces the bounds where a value really may be anything its type holds:
`read_long(eo::any, "sum")`, `read_ints(n, eo::any, "a")`, `read_line(eo::any, "rest")`.

### Names and bounds

| Call | What happens |
| --- | --- |
| `read_int(1, n, "k")` | the normal form |
| `read_int(eo::any, "k")` | no bounds, on purpose, no warning |
| `read_int(1, n, eo::unnamed)` | no name, on purpose, no warning |
| `read_int("k")` | works, with warning EO103 |
| `read_int(1, n)` | works, with warning EO101 |

**Bounds on the contestant's output are not a formality.** A contestant who prints
`2147483647` where a count is expected makes an unbounded checker size a vector to match, or
loop for ever, and the checker then crashes or times out: a system failure where a wrong
answer belonged. A bounded read rejects the value on the spot, as a wrong answer that names
the value.

Names can be literals, `std::string`, or built with `eo::fmt("v[{}]", i)`. The elements of a
bulk read are named after the array, counting from 1: `a[1]`, `a[2]`. When a loop reads a
sequence one value at a time, name the elements with **`eo::element("a", i)`** rather than
`eo::fmt`: a message still says `a[3]`, but coverage keeps one entry called `a` instead of one
per element, which on 200,000 values is the difference between 2.4 MB and 39 MB.

### What counts as a number

An integer is an optional `-` followed by digits, with no `+`, no leading zeros and no `-0`. Real numbers may carry an exponent, because many languages print
small numbers that way, and may be a zero written with a minus, such as `-0.000000`, which
`printf("%.6f", -1e-9)` prints; it reads as 0. To take contestants as they come, or to refuse an exponent:

```cpp
c.output.numbers(eo::lenient);
c.output.reals(eo::plain);
```

A number read consumes the number, not the rest of the token, so anything it does not
consume is still there to be read — and, at the end, to be complained about.

### Looking ahead

`s.at_eof()` is true when only whitespace is left; `s.at_eoln()` when only spaces and tabs
come before the next line break. Both **skip the whitespace they look past**, so a
`read_line` after one starts at the first non-blank.

## Verdicts and scores

```cpp
eo::accept("the path has weight {}", w);
eo::wrong("vertex {} is visited twice", v);
eo::score(eo::ratio(good, total), "{} of {} pairs are correct", good, total);
eo::points(7.5, "a bonus for the short answer");
eo::jury_error("the jury's path is not simple");
```

| Call | Exit | The log starts with | Result |
| --- | --- | --- | --- |
| `eo::accept(…)` | 0 | `ok` | ACCEPTED, the full cost |
| `eo::wrong(…)` | 1 | `wrong answer` | WRONG_ANSWER, 0 |
| `eo::score(f)` with f = 1 | 0 | `ok` | ACCEPTED |
| `eo::score(f)` with 0 < f < 1 | 7 | `points <f × cost>` | PARTIALLY_CORRECT, f × cost points |
| `eo::score(f)` with f = 0 | 7 | `points 0` | PARTIALLY_CORRECT, 0 points; ACCEPTED on a test worth 0 |
| `eo::points(p)` | as `eo::score(p / cost)` | | |
| `eo::jury_error(…)` | 3 | `jury error` | VERIFICATION_FAILURE |

**`eo::score` takes a fraction of the test, not points.** On a test worth 40,
`eo::score(0.5)` gives 20; on one worth 7 it gives 3.5. The subtask weights stay in the
testset configuration, and the checker never hard-codes 100.

**Use `eo::ratio(a, b)`** for "a out of b": it is exact, so `eo::ratio(n, n)` is exactly 1 and
a perfect answer is an accept. A fraction outside [0, 1], an infinity included, is clamped
with warning EO205, which says that a finite one of 2 or more looks like a percentage; one
that is NaN is a jury error, and one within 10⁻⁹ of 1 gets EO206. So do points
that the judge, which reads them as a 32-bit float, would round up to the full cost and so
count as an accept.

**Rounding.** Some tasks round the score, and a documented full score can be unreachable
without it:

```cpp
eo::score(quality, eo::round_to(0), "D = {}", d);
```

**A partial score pays only in the right testset mode.** Under `ALL` a partially scored run
is not a pass and the whole testset pays nothing; partial scores need `EACH` or `WORST`. On a
test that carries no points — a sample, a stress run — any score, 0 included, reaches the
test's cost of 0, so the judge counts the run as accepted; the library says so with warning
EO208. An answer that earns nothing should end with `eo::wrong`.

**Every path must end in a verdict.** Returning from `main` without one is a jury error.

## The log

The judge's parser gives up if any line before `points` ends in whitespace, so the library
takes over the checker's stdout and stderr and writes the verdict line first, whatever the
checker printed earlier:

```
points 20 half marks
a line that ends in a space 

and a blank line above
n = 3
eolymp.h 2.1.1
```

Either of those lines would kill the parse if it reached the log before the verdict. Print
freely with `std::cout`, `printf` or `eo::log("n = {}", n)`; debug output cannot cost a score.
More than 64 KB of it gets note EO210, because stored logs are truncated.

## Reading the jury's answer and the contestant's with one function

When several answers are correct the checker has to verify the contestant's answer on its
own. The same verification should run on the jury's, so that a broken answer file is caught
as a jury error instead of silently failing everyone.

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

`read_both` calls the reader on `c.jury` first and then on `c.output`, and returns both
results. Inside it, a failed read, `a.wrong(…)` and a bare `eo::wrong(…)` are all blamed on
the stream being read: a jury error during the first call, a wrong answer during the second.
Three rules hold by construction: read the jury first, verify the jury's answer too, and
report a jury-side problem as a jury error.

`c.optimum(by_the_jury, found, eo::minimize)` covers all three outcomes and ends the program:

| Outcome | Verdict |
| --- | --- |
| equal | accept |
| the contestant is worse | `wrong answer the answer is 17; the optimum is 12` |
| the contestant is better | a jury error: `the contestant's 9 beats the jury's 12` |

`eo::maximize` is the other direction.

## Declaring how many answers there are

```cpp
c.answers(eo::unique);
c.answers(eo::many);
```

This is a declaration, not a check. With `eo::many` the library warns (EO212) when the
checker only compares with the jury's answer, because such a checker rejects the other
correct ones.

## Ready-made comparisons

Each of these gives the verdict and ends the program.

| Call | Accepts when |
| --- | --- |
| `c.tokens()` | the output has exactly the answer's tokens, in order; whitespace does not matter, letter case does |
| `c.lines()` | the output has the answer's lines, ignoring trailing spaces, tabs and carriage returns |
| `c.reals(eps)` | token by token: numbers agree within an absolute or relative error of `eps`, other tokens are equal; a token longer than 4096 characters and than the answer's is wrong |
| `c.yes_no(certificate)` | a `YES`/`NO` answer, with a certificate after `YES` |
| `c.yes_no(certificate, "POSSIBLE", "IMPOSSIBLE")` | the same with other words |

Eolymp also has built-in `TOKENS` and `LINES` checkers that need no program at all. Use them
when they are enough — but note that the built-in `TOKENS` fails with a system failure on a
token over 64 KB, where `c.tokens()` has no such limit: it reads a contestant token only one
character past the answer's, so a huge token costs no memory.

`yes_no` reads the first token of each side, ignoring case, and then:

| The jury says | The contestant says | Result |
| --- | --- | --- |
| NO | NO | accept |
| YES | NO | wrong answer: a solution exists |
| NO | YES, and the certificate is valid | a jury error: the contestant found one the jury says does not exist |
| NO | YES, and the certificate is not | wrong answer |
| YES | YES | the certificate is checked, on both sides |

## What is left over

When the checker accepts or scores, the library looks at what is left of each file.

**The contestant's output.** Anything but whitespace is a wrong answer, "extra output after
the answer". When a task tolerates it, say so once:

```cpp
c.output.trailing(eo::ignore);
```

**The answer file.** Anything left raises warning EO203, which usually means the checker and
the answer files disagree about the format — a secret token on the first line, or an `OK`
line, are the classic cases. When only part of it is meant to be read:

```cpp
c.jury.skip_rest("only the first line is compared; the rest is the jury's certificate");
```

The reason is not optional.

## What the checker knows about the test

| Call | Returns | In a stress run | Locally |
| --- | --- | --- | --- |
| `c.cost()` | the test's points | 0 | 100 |
| `c.group()` | the index of its testset | 0 | 0 |
| `c.index()` | its number in that testset | the iteration | 0 |
| `c.test_id()` | its id | empty | empty |

When scoring depends on the subtask, key it off `c.group()`. Guessing from the cost breaks as
soon as two subtasks are worth the same.

`c.cases(t, body)` runs `body` `t` times and puts `case i:` in front of every message raised
inside it:

```
wrong answer: case 2: output.txt, line 1, answer: expected an integer, found "x"
```

## Warnings

A warning is about the problem, not about one run, and **it never changes a verdict**. Each
carries a stable code, a line and a fix: the line of your source for a read, a bound or a
clamped score, and the header's own line for how the run ended — EO201–EO204, EO206's
rounding, EO208, EO210 and EO212. Each is counted once per call site;
the report is capped at thirty lines. They appear as compiler warnings where they can be, on
stderr locally, and in `checker.log` after the verdict on the judge, followed by one
machine-readable `eo-report` line.

| Code | Fires when |
| --- | --- |
| EO101 | a value is read without a name |
| EO103 | a number from the answer file or the output is read without bounds |
| EO104 | the bounds are the whole range of the type, without `eo::any` |
| EO105 | a bound does not fit the type, as in `read_int(1, 3000000000)` |
| EO106 | a bound is one away from a round number (a note) |
| EO107 | one name is read with different bounds, or as two different kinds, at two places |
| EO111 | a token over 1 MB from the contestant was held in memory (a note) |
| EO201 | the checker passed the run without reading any of the output |
| EO202 | the checker read neither the input nor the answer |
| EO203 | the answer file still holds something when the checker finished |
| EO204 | `eo::wrong` or `eo::jury_error` carries no message |
| EO205 | a fraction outside [0, 1], or negative points, was clamped |
| EO206 | a fraction is a hair below full marks, or its points round up to the cost on the judge; use `eo::ratio` |
| EO207 | `eo::points` exceeded the test's cost |
| EO208 | a partial score, 0 included, on a test worth 0 points, which the judge counts as accepted |
| EO210 | more than 64 KB was printed before the verdict (a note) |
| EO211 | the checker runs as the legacy type (a note) |
| EO212 | the problem declares `eo::many`, but the checker only compares with the jury |
| EO213 | on the judge, `TEST_COST` is missing or not a number, so the cost is a guess |

`EOLYMP_STRICT=1` turns every warning into a jury error while you prepare a problem. Notes
stay notes. State the intent where there is a way to — `eo::any`, `eo::unnamed`,
`trailing(eo::ignore)`, `skip_rest`, `answers` — and otherwise silence one code in a scope,
with a reason that the report prints:

```cpp
eo::allow quiet("EO103", "k is checked against n two lines below");
```

## Not here yet

Still missing:

- A pattern syntax. Use a charset and a length, or `read_choice`.

## Reference card

| Member | Does |
| --- | --- |
| `eo::checker c(argc, argv)` | makes the program a checker |
| `c.input`, `c.jury`, `c.output` | the three streams |
| `c.read_both(reader)` | reads the jury's answer, then the output, with one function |
| `c.answers(eo::unique)`, `c.answers(eo::many)` | how many answers are correct |
| `c.optimum(by_the_jury, found, eo::minimize)`, `eo::maximize` | compare and end |
| `c.tokens()`, `c.lines()`, `c.reals(eps)`, `c.yes_no(certificate)` | ready-made comparisons |
| `c.cost()`, `c.group()`, `c.index()`, `c.test_id()` | the test |
| `c.cases(t, body)` | numbers the messages of a multi-test output |

| Stream member | Does |
| --- | --- |
| `read_int`, `read_long`, `read_real` | numbers |
| `read_token`, `read_line`, `read_choice` | text |
| `read_ints`, `read_longs`, `read_reals`, `read_tokens` | several values |
| `at_eof()`, `at_eoln()` | look ahead, skipping whitespace |
| `wrong(…)` | a verdict blamed on this stream |
| `numbers(eo::lenient)`, `reals(eo::plain)` | number syntax |
| `trailing(eo::ignore)` | allow text after the answer |
| `skip_rest("why")` | leave the rest of the answer unread on purpose |

| Function | Does |
| --- | --- |
| `eo::accept`, `eo::wrong`, `eo::score`, `eo::points`, `eo::jury_error` | end with a verdict |
| `eo::ratio(a, b)`, `eo::round_to(d)` | an exact fraction, and rounding |
| `eo::close_enough(expected, found, eps)` | compare reals |
| `eo::fmt("…", args)`, `eo::log("…", args)` | build a string, write a line to the log |
| `eo::any`, `eo::unnamed`, `eo::charset("a-z")` | no bounds, no name, allowed characters |
| `eo::element(name, index)` | names one element of a sequence, without a coverage entry of its own |
| `eo::all_distinct`, `eo::is_sorted`, `eo::is_permutation`, `eo::is_tree`, `eo::is_connected`, `eo::is_simple_graph` | structural checks |
| `eo::allow name("EO103", "why")` | silence one warning code in a scope |
| `eo::version()`, `EOLYMP_H_VERSION` | the library's version, as a string |
| `EOLYMP_H_VERSION_MAJOR`, `_MINOR`, `_PATCH` | the same version as three numbers, for `#if` |
