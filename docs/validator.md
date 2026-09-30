# Writing a validator with eolymp.h

A **validator** reads one test input and says whether it is a legal input for the problem:
every value inside the statement's constraints, every line laid out the way the statement
describes, and every structural promise kept.

It matters for two reasons. A test outside the constraints fails correct solutions, and the
contestant is blamed for the jury's mistake. A test with the wrong layout fails solutions in
some languages only: a C++ solution reading with `cin` cannot tell `5 3` on one line from `5`
and `3` on two, while a Python solution doing `n, m = map(int, input().split())` crashes on
the second. So a validator checks every space and every line break, not just the values.

This page describes the library as it is built. Everything here works; nothing here is
planned, and §Not here yet lists what is still missing.

## A first validator

The input: `n` on the first line, then `n` integers on the second, separated by single
spaces, with `1 ≤ n ≤ 200000` and `1 ≤ aᵢ ≤ 10⁹`.

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int n = v.read_int(1, 200000, "n");
    v.read_eoln();
    std::vector<int> a = v.read_ints(n, 1, 1000000000, "a");
    v.read_eoln();
}
```

- `eo::validator v(argc, argv)` makes the program a validator. `v` is also the input: you
  read from it directly.
- `read_int(1, 200000, "n")` reads one integer and checks it. The name `"n"` appears in every
  message about that value.
- `read_eoln()` reads exactly one line break. Nothing is skipped silently: a space before the
  line break, or a missing line break, makes the test invalid.
- When `main` returns, the library checks that the input has ended. Anything left, even an
  empty line, makes the test invalid.

An invalid test prints one line and exits with 3:

```
line 2, a[5]: 1000000001 is above 1000000000
line 1: expected a line break after n, found a space
line 3: expected the end of the input, found "7"
```

## Setting up

Include the header before anything else, because organiser code sometimes contains
`#define int long long`:

```cpp
#include <eolymp.h>
```

It needs C++17 and builds unchanged as C++20 and C++23; below C++17 it stops at one
`#error` that says so.

**The validator attaches nothing.** The judge's C++ runtime carries the header at
`/usr/include/eolymp.h`, so the angle-bracket `#include` finds it. The runtime image is the
version: the problem compiles against the release that image holds.

Build and run it like any C++ program, with the header beside the source:

```bash
g++ -std=c++17 -O2 -I. -o validator validator.cpp
```

```bash
./validator test.txt --group 2
```

The input may come from a file named on the command line or from stdin, which is how the
judge passes it. Exit 0 means valid. Any other exit means invalid, and the judge keeps the
first 200 bytes of the output as the test's status.

## How the judge runs it

| | Value |
| --- | --- |
| arguments | `input.txt --group <testset index>`. In a stress run: `input.txt` only, with no group |
| stdin | the test input, with CRLF line endings already converted to LF |
| environment | `EOLYMP=1` |
| time | 30 s |
| result | exit 0 is valid; anything else is invalid, and the first 200 bytes of the output become the message |

Locally the library folds CRLF the same way the judge does, so a local run gives the judge's
answer, and adds note EO110 so you know the file has them.

## Reading

Nothing skips whitespace. Every space and every line break in the input is read by a call in
your code, so the layout a test must have can be read off the validator.

### Separators

| Call | Reads |
| --- | --- |
| `v.read_space()` | exactly one space |
| `v.read_eoln()` | exactly one line break |
| `v.read_char(c)` | exactly the character `c`, for formats such as `12:30` |
| `v.read_eof()` | the end of the input; optional, see below |

### Values

Bounds are inclusive, and come first; the name is last.

| Call | Reads | Returns |
| --- | --- | --- |
| `v.read_int(low, high, name)` | an integer in `[low, high]` that fits an `int` | `int` |
| `v.read_long(low, high, name)` | an integer in `[low, high]` | `long long` |
| `v.read_real(low, high, least, most, name)` | a decimal in `[low, high]` with between `least` and `most` digits after the point | `double` |
| `v.read_token(least, most, eo::charset("a-z"), name)` | a token of that length made only of those characters | `std::string` |
| `v.read_line(least, most, eo::charset("a-z "), name)` | the rest of the line, and its line break | `std::string` |
| `v.read_token(eo::pattern("[A-Z][a-z]*"), name)` | a token that matches the [pattern](#patterns) | `std::string` |
| `v.read_line(eo::pattern("[a-z]+( [a-z]+)*"), name)` | the rest of the line, which matches the pattern, and its line break | `std::string` |
| `v.read_choice({"insert", "erase"}, name)` | a token equal to one of the choices | `std::string` |
| `v.read_ints(count, low, high, name)` | `count` integers separated by single spaces | `std::vector<int>` |
| `v.read_longs(count, low, high, name)` | the same, 64-bit | `std::vector<long long>` |
| `v.read_reals(count, low, high, least, most, name)` | `count` decimals | `std::vector<double>` |
| `v.read_tokens(count, least, most, eo::charset("a-z"), name)` | `count` tokens | `std::vector<std::string>` |
| `v.read_tokens(count, eo::pattern("[a-z]{1,3}"), name)` | `count` tokens that match the pattern | `std::vector<std::string>` |
| `v.read_grid(rows, cols, eo::charset(".#"), name)` | `rows` lines of exactly `cols` characters from the charset, each with its line break | `std::vector<std::string>` |

Two rules about line breaks:

- **The bulk reads stay inside the line.** They read the single spaces *between* their
  values, not a space before the first and not the line break after the last. End the line
  yourself with `read_eoln()`.
- **`read_line` reads its own line break,** as do `read_grid`, `read_tree`, `read_graph` and
  `read_edges`.
  Do not call `read_eoln()` after them.

`read_grid` names each row after the grid, `grid[2]`, and says what is wrong with it:
`line 3, grid[2]: the line is 4 characters long, not 5..5`. A row is a line, so a charset that
holds a space allows spaces inside a row.

A charset is single characters and ranges: `eo::charset("a-z")` is the 26 letters and
`eo::charset("a-z0-9_")` is 37 characters. A `-` between two characters makes a range, and the
range has to ascend, so `charset("z-a")` is a jury error before the first read. So is the `)- `
inside `charset("()- ")`, which reads as a range from `)` to the space: the library says `the
character range ")- " in charset("()- ") runs backwards`. A `-` that should be a character of
its own goes first or last, where there is nothing for it to join: `charset("() -")` is the two
brackets, a space and a hyphen.

With `count` equal to 0 a bulk read reads nothing, so a line holding "0 values" is an empty
line: `read_ints(0, …)` followed by `read_eoln()` expects exactly `\n`.

### What counts as a number

An integer is an optional `-` followed by digits. `+5`, `007` and `-0` are invalid, and so is a value too large for the type it is read into. Real numbers are plain
decimals — `3`, `3.25`, `-0.5` — with no exponent, no infinity and no NaN;
`read_real(0.0, 1.0, 1, 6, "p")` accepts `0.5` and `0.123456` but not `1` or `0.1234567`.

**A number read consumes the number, not the rest of the token.** That is what makes
`read_char(':')` work on `12:30`. Anything the number does not consume is still yours to
read, so nothing invalid slips through, and when what follows a number is not the space or
the line break the validator expects, the message names the whole token and the value, as
it would for any other malformed number: on the input `9a`, `read_int` returns 9 and the
next `read_eoln()` fails with
`line 1, n: expected an integer, found "9a": it has a character that cannot be part of the number`.
`1e5` read as an int, and `1e-3`, `0,5` or `1.5e3` read as a real, are named the same way. A
bound is checked first, so `100e5` read with bounds 1..10 is `100 is above 10`. The reason is
the whole token's, so it can stand where a narrower one would have: `-0x` is "a character
that cannot be part of the number" rather than "zero written with a minus", and a number too
long for its type followed by a letter the same, rather than "does not fit".

### Patterns

`eo::pattern("[a-z]{1,10}")` is testlib's pattern syntax. `v.read_token(p, name)`,
`v.read_tokens(count, p, name)` and `v.read_line(p, name)` read what must match it, and
`p.matches(token)` says whether a whole token matches; a pattern never matches part of one.

| Write | Matches |
| --- | --- |
| `a`, `\.`, `\[` | that character; a backslash before any symbol means the symbol |
| `[a-z_]`, `[^0-9]` | one character of the class, or one outside it; a `-` first or last is itself |
| `x?`, `x*`, `x+` | at most one, any number, at least one |
| `x{3}`, `x{1,10}`, `x{2,}` | exactly 3, from 1 to 10, at least 2 |
| `YES\|NO`, `(ab\|cd){2}` | either side; a group repeats as one |

A class is a set of bytes: `[a-z]` is 26 bytes, and a UTF-8 letter is two or more bytes, each
of which a class or a repeat counts on its own.

**What a match costs.** Matching never backtracks: it keeps the set of places in the pattern
that the token so far can have reached, so its cost is the token's length times the number of
those places that are live at once, which the pattern bounds. A repeat of a character or a
class is one place at any count, so `[a-z]{1,1000000}` costs what `[a-z]` costs; a repeated
group is one copy per count, so `(ab|cd){1,100}` holds up to 100 copies of its group. A pattern
whose places would number more than 4,096 is refused where it is made, and the largest pattern
in these pages, the tests and testlib's own examples needs 27. A pattern with a longest match,
`[a-z]{1,10}` or `(YES|NO)`, stops a read one character past it, as a read with a stated
maximum length does, so a contestant's endless token costs no more than the pattern; one
without, such as `[a-z]+( [a-z]+)*`, reads the whole token or line, and a line of a million
characters against a pattern of 300 live places is 300 million steps. Building a pattern
takes a few microseconds, so build it once, before the loop that uses it.

A read's message names the value and the pattern:
`line 1, first: "anna" does not match "[A-Z][a-z]{0,9}"`.

A pattern that does not parse is refused where it is made, with the column and what to write
instead: `eo::pattern("[a-z") does not parse at column 1: the [ that opens here has no ] to
close it`. So are the things testlib would read in a way nobody means: a `]`, `}` or `{` that
opens or closes nothing, a repeat with nothing before it (`*a`, `a|+`), a repeat of a repeat
(`a**`, `a{2}{3}`, `a+?`), a count above 1,000,000,000, a backslash at the end, a backslash
before a letter or a digit, `^` and `$`, an empty class, and groups nested more than 50 deep. Under
C++20 with `-DEOLYMP_CHECK_PATTERNS`, a literal pattern is parsed while the program builds, and
one that does not parse stops the build at `a_pattern_that_does_not_parse`.

It differs from testlib's in five places:

- **It matches as a regular expression does.** testlib's matcher takes as many characters as
  a repeat allows and never gives one back, so `[a-z]*a` matches `ba` here and nothing there.
- **A space is a space.** testlib drops every space a backslash does not quote, so its
  `[a-z ]` is `[a-z]`; `\ ` is a space in both. A ported `readToken("[a-z] {1,5}")` therefore
  matches no token here, since a token holds no blank, and warning EO113 says so.
- **`{2,}` is two or more**, where testlib reads it as exactly two.
- **A group can be repeated, and can stand anywhere:** testlib refuses `(ab|cd){2}` and
  `x(ab|cd)y`.
- **`\d`, `\w` and the like, `^`, `$` and an empty class are refused,** where testlib reads
  the first three as the letter or the character itself, and `[]` as a class of nothing.

As in testlib, `.` is a dot and not "any character", so `[^ ]` or a class says what may
stand there.

### Looking ahead

| Call | True when | Consumes |
| --- | --- | --- |
| `v.at_eoln()` | the next character is a line break | nothing |
| `v.at_eof()` | the input has ended | nothing |

They are for formats whose length is not given, such as a line of integers of any length:

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    std::vector<int> xs;
    xs.push_back(v.read_int(1, 100, "x"));
    while (!v.at_eoln()) {
        v.read_space();
        xs.push_back(v.read_int(1, 100, "x"));
    }
    v.read_eoln();
}
```

Neither consumes anything, and neither skips whitespace before looking, so a trailing blank
line is never accepted by accident.

### Names and bounds

Both can be left out, and the call still works — but the library warns, at compile time and
again when it runs:

| Call | What happens |
| --- | --- |
| `read_int(1, n, "k")` | the normal form |
| `read_int(eo::any, "k")` | "any `int`": no bounds, on purpose, no warning |
| `read_int(1, n, eo::unnamed)` | no name, on purpose, no warning |
| `read_int("k")` | works, with warning EO102: the validator accepts anything that parses |
| `read_int(1, n)` | works, with warning EO101: messages cannot name the value |
| `read_int()` | works, with both |

Names can be literals, `std::string`, or built with `eo::fmt`:

```cpp
int x = v.read_int(1, n, eo::fmt("x[{}]", i));
```

The elements of a bulk read are named after the array, counting from 1: `a[1]`, `a[2]`.

**Reading a sequence one value at a time: use `eo::element`, not `eo::fmt`.** A name built with
`eo::fmt("a[{}]", i)` is a *different* name for every element, and coverage keeps one entry per
name — 200,000 of them for 200,000 values, which cost 39 MB where 2.4 MB was enough.
`eo::element("a", i)` names the element the same way in a message and keeps `a` as the one
name coverage records:

```cpp
int x = v.read_int(1, n, eo::element("a", i));
```

A refusal still says `a[3]`; EO107 and the bound coverage see one value called `a`.

## When a separator is missing

Two value reads in a row with nothing between them can never both succeed, because a value
stops where the next one would start. So when that happens the library knows the fault is in
the validator's code, not in the test, and says which call is missing, with the line of your
source:

For a test case written `n k` on one line and `n` values on the next, leaving out the
`read_space()` between `n` and `k`, or the `read_eoln()` after `k`, gives:

```
validator.cpp:9: case 1: line 2, k: a space follows n; read it with read_space()
validator.cpp:11: case 1: line 2, a[1]: a line break follows k; read it with read_eoln()
```

Every correctly formatted test hits a missing separator on the line where it happens, so the
mistake shows up on the first test you run, naming the value and the fix rather than blaming
the test.

## The end of the input

The end of the input is checked **when `v` goes out of scope**, that is, when `main` returns.
`v.read_eof()` does the same check where you call it; it is accepted but not required. Calling `exit(0)` early runs the same final checks, so an early exit cannot accept
a test that was only half read. There is no "Validator must end with readEof" failure to trip
over.

`std::quick_exit` runs them only where the C library offers `at_quick_exit`, as glibc and musl
do and macOS does not, so a `quick_exit` that leaves a test half read is refused on the judge
and accepted on a Mac. `std::_Exit` and `std::abort` run nothing anywhere.

Every `eo::sum_limit` is checked at the same point.

## Subtasks

On Eolymp each subtask is a testset, and the judge calls the validator with
`--group <testset index>`. One validator checks every test against its own subtask through a
table:

```cpp
#include <eolymp.h>

struct limits {
    int n;
    long long a;
    bool distinct;
};

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    limits const& L = v.subtasks<limits>({
        {0, {200000, 1000000000, false}},
        {1, {10, 100, false}},
        {2, {200000, 1000000000, true}},
    }).without_group({200000, 1000000000, false});

    int t = v.read_int(1, 10000, "t");
    v.read_eoln();
    eo::sum_limit total(200000, "sum of n");
    v.cases(t, [&] {
        int n = v.read_int(1, L.n, "n");
        v.read_space();
        long long k = v.read_long(1, L.a, "k");
        v.read_eoln();
        std::vector<long long> a = v.read_longs(n, 1, k, "a");
        v.read_eoln();
        if (L.distinct) v.require(eo::all_distinct(a), "a");
        total += n;
    });
}
```

- **`limits` is your own struct.** Put in it whatever differs between subtasks.
- **`without_group` is required.** It is the row used when there is no group, as in a stress run
  or a local run. Make it the loosest row, the full constraints of the problem.
- **Group 0 is a testset too** — usually the examples shown on the statement page — so list
  it.
- **A group missing from the table** makes the test invalid: `no subtask 7; known: 0, 1, 2`.
  A testset added to the problem cannot slip through unchecked.
- **Two rows with the same limits** get warning EO302, which usually means a copy and paste.
  Group 0 is left out of the comparison, because the examples usually have the full limits of
  the last subtask.
  It needs `operator==` on your struct; without one the check is skipped.
- **A problem with subtasks and no table** raises EO301 when a test arrives with `--group 2`
  or higher.

When the constraints cannot be written as a table, read the group directly. `v.group()`
returns a `std::optional<int>`: the testset index, or nothing in a stress or local run.

## Multi-test inputs and running totals

`v.cases(t, body)` runs `body` `t` times and puts `case i:` in front of every message raised
inside it, so an invalid test says which case is wrong: `case 1234: line 99, r: …`.

`eo::sum_limit total(limit, name)` adds up whatever you give it with `+=` and checks the
total once, at the end: `sum of n is 200005, above 200000`. Several can live side by side. A
validator that uses `cases` with no `sum_limit` at all gets note EO304, because most
multi-test statements bound the sum of `n`.

## Conditions and structure

Anything the reads cannot express goes through `v.require`:

```cpp
v.require(l <= r, "l = {} is greater than r = {}", l, r);
```

These check a whole collection and carry their own message:

| Check | True when |
| --- | --- |
| `eo::all_distinct(values)` | no two elements are equal |
| `eo::is_sorted(values)` | the elements never decrease |
| `eo::is_permutation(values)` | the elements are 1..n, each once |
| `eo::is_tree(n, edges)` | the edges form a tree on vertices 1..n |
| `eo::is_connected(n, edges)` | the graph on 1..n is connected |
| `eo::is_simple_graph(n, edges)` | no loops and no repeated edges |

`v.require(eo::all_distinct(a), "a")` gives, for example,
`a: elements 3 and 7 are both 42`. The results are `[[nodiscard]]`, so computing a check and
ignoring it is a compiler warning — `eo::is_tree(n, edges);` on its own line checks nothing.

These read and check in one call, with vertices numbered from 1:

| Call | Reads | Checks |
| --- | --- | --- |
| `v.read_tree(n, "edge")` | `n - 1` lines, each `u v` | every vertex in 1..n, and that the edges form a tree |
| `v.read_graph(n, m, flags, "edge")` | `m` lines, each `u v` | every vertex in 1..n, plus the flags |
| `v.read_edges(m, n, "edge")` | `m` lines, each `u v` | every vertex in 1..n, each read as a bounded value |
| `v.read_permutation(n, "p")` | `n` integers inside the current line | a permutation of 1..n |

Flags combine with `|`: `eo::simple`, `eo::connected`, and `eo::any_graph` for neither.
`read_tree`, `read_graph` and `read_edges` read whole lines; `read_permutation` stays inside
the line like `read_ints`, so end the line yourself.

`read_edges` is the plain list: no shape is checked, and a vertex out of range is refused where
it stands, `line 4, edge[4]: 9 is above 5`, with its bounds recorded for coverage under the
name. `read_tree` and `read_graph` check the whole list once it is read, and name the edge in
their message instead: `edge: edge 4 is (1, 9), outside 1..5`.

**Weights.** Each of the three takes `eo::weighted(low, high)` before the name, reads lines of
`u v w` with `w` a `long long` in those bounds, and returns `std::vector<eo::weighted_edge>`,
whose members are `u`, `v` and `w`. A weight is named after the list with `.w`, so a refusal
says `edge.w[3]: 0 is below 1` and coverage records `edge.w`:

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int n = v.read_int(2, 1000, "n");
    v.read_eoln();
    std::vector<eo::weighted_edge> edges = v.read_tree(n, eo::weighted(1, 1000000000), "edge");
    int m = v.read_int(0, 1000, "m");
    v.read_eoln();
    std::vector<eo::edge> queries = v.read_edges(m, n, "query");
}
```

## Coverage and features

A validator is the only program that reads every test, so it is where a problem's coverage is
measured. Every read with a name and bounds records whether the value reached its lower and
its upper bound:

| Read | Recorded as | Reaches a bound when |
| --- | --- | --- |
| `read_int`, `read_long`, `read_ints`, `read_longs` | `int` | the value equals the bound |
| `read_real`, `read_reals` | `real` | the value equals the bound exactly |
| `read_token`, `read_line`, `read_tokens`, `read_grid` | `length` | the text is that many characters long |
| `read_edges` | `int`, and the weights as `int` under the name with `.w` | a vertex is 1 or `n`, a weight equals a bound |

A bound that depends on another value counts as well: `read_int(1, n, "x")` reaches its upper
bound on a test where some `x` equals that test's `n`. A value read with `eo::any` has no
bound to reach and is not recorded.

Because the bounds come from `subtasks<Limits>`, the record is automatically per subtask: in
subtask 1 the bound on `n` really is `1..10`, and the validator was run with `--group 1`.

For shapes the statement promises, declare the feature once and mark each test that has it:

```cpp
v.feature("path");
if (is_path) v.saw("path");
```

`./validator input.txt --group 1 --eo-describe` validates as usual and also prints what it
recorded **for that one test**, one line each:

```
eo-describe group 1
eo-describe value n int 1 200000 low=no high=yes
eo-describe value p real 0 1 low=no high=no
eo-describe value s length 1 8 low=yes high=no
eo-describe feature path seen=yes
```

`eo-judge check` adds those up across the tests and reports EO807. Because each block
describes one test rather than a running total, the same records answer both questions an
author has: whether a bound is reached anywhere — "no test reaches n = 200000 in subtask 2",
warning EO807 — and whether two bounds are reached *together*, which a running total could
never show.

## Warnings

A warning is about the problem, not about one test. **It never makes a test invalid.** Each
one carries a stable code and a fix, and the line of your source when it is about a call; one
about the whole run, such as EO301 or EO304, names only the validator. Each is counted once
per call site; the report is capped at thirty lines, warnings before notes.

They appear as compiler warnings where they can be, on stderr when a local run ends, and on
the judge in the validator's output, which the validation results keep for valid tests as well
as invalid ones.

| Code | Fires when |
| --- | --- |
| EO101 | a value is read without a name |
| EO102 | a number is read without bounds |
| EO104 | the bounds are the whole range of the type, without `eo::any` |
| EO105 | a bound does not fit the type, as in `read_int(1, 3000000000)` |
| EO106 | a bound is one away from a round number, such as `200001` (a note) |
| EO107 | one name is read with different bounds, or as two different kinds, at two places |
| EO108 | a token or a line is read with no length or no charset |
| EO109 | a real number is read with no rule on its digits |
| EO110 | a local input has CRLF line endings (a note) |
| EO301 | a test comes with `--group 2` or higher and the validator has no table |
| EO302 | two rows of the table other than group 0 have the same limits |
| EO304 | `cases` is used with no `sum_limit` (a note) |

**Strict mode** turns every warning into an invalid test. Notes stay notes. Turn it on with
`EOLYMP_STRICT=1` while preparing a problem, so that nothing is uploaded with a warning
nobody read.

**State the intent first.** `eo::any` for a value that really may be anything its type holds,
`eo::unnamed` for one that needs no name. For the rest, a scoped suppression, which needs a
reason and is listed in the report with the number of times it was used:

```cpp
eo::allow quiet("EO106", "the statement really says n <= 200001");
```

## Not here yet

Nothing a validator reads is missing. The whole-problem checks EO806–EO811 and the
configuration checks EO9xx are in [judge.md](judge.md); they read the coverage this validator
records.

## Reference card

| Member | Does |
| --- | --- |
| `eo::validator v(argc, argv)` | makes the program a validator; `v` is also the input |
| `read_int`, `read_long`, `read_real` | numbers |
| `read_token`, `read_line`, `read_choice` | text, by a length and a charset or by an `eo::pattern` |
| `read_ints`, `read_longs`, `read_reals`, `read_tokens` | several values on one line |
| `read_grid(rows, cols, charset, name)` | a grid, one row per line |
| `read_space()`, `read_eoln()`, `read_char(c)`, `read_eof()` | separators |
| `at_eoln()`, `at_eof()` | look ahead without consuming |
| `subtasks<Limits>({rows}).without_group(row)` | the constraints table |
| `group()` | the testset index, if any |
| `cases(t, body)` | numbers the messages of a multi-test input |
| `require(cond, "…", args)`, `require(check, name)` | a condition that must hold |
| `read_tree(n, name)`, `read_graph(n, m, flags, name)`, `read_permutation(n, name)` | structural readers |
| `read_edges(m, n, name)` | an edge list with its vertices bounded |
| `eo::weighted(low, high)` before the name of `read_tree`, `read_graph` or `read_edges` | the same with a weight on every edge |
| `feature(name)`, `saw(name)` | declared features |
| `invalid(name, text)` | reject this test with your own message |

| Function | Does |
| --- | --- |
| `eo::sum_limit total(limit, name)` | a total checked at the end |
| `eo::all_distinct`, `eo::is_sorted`, `eo::is_permutation`, `eo::is_tree`, `eo::is_connected`, `eo::is_simple_graph` | structural checks |
| `eo::simple`, `eo::connected`, `eo::any_graph` | flags for `read_graph` |
| `eo::edge`, `eo::weighted_edge` | what the edge readers return: `u`, `v`, and `w` |
| `eo::any`, `eo::unnamed` | "no bounds, on purpose", "no name, on purpose" |
| `eo::charset("a-z")` | allowed characters: single ones and ranges |
| `eo::pattern("[a-z]{1,10}")` | testlib's pattern syntax; `matches(token)` |
| `eo::element(name, index)` | names one element of a sequence, without a coverage entry of its own |
| `eo::fmt("…", args)` | build a string with `{}` placeholders |
| `eo::allow name("EO106", "why")` | silence one warning code in a scope |
| `eo::version()`, `EOLYMP_H_VERSION` | the library's version, as a string |
| `EOLYMP_H_VERSION_MAJOR`, `_MINOR`, `_PATCH` | the same version as three numbers, for `#if` |
