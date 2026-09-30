# Every warning code

This is the page to look a code up in. Each row is self-contained, so
`grep EO807 docs/warnings.md` answers the whole question: what fired it, who reported it,
and what to do about it.

**A warning never changes a verdict.** It cannot make a test invalid or an answer wrong.
`EOLYMP_STRICT=1` in the environment, or `eo-judge --strict`, makes the first warning fatal;
that is for a preparation loop, not for the judge. `eo::allow quiet("EO106", "the statement
really says n ≤ 200001");` silences one code for the rest of its scope and needs a reason,
which the report lists, so a silence stays visible. It has to be a named object: written as
a bare statement it would end at the semicolon, and the compiler warns about that.

**Where a warning comes out** — the stderr block locally, one line plus an `eo-report` JSON
line on the judge, and the channel each role can afford to write to — is in
[README.md](README.md#how-a-warning-reaches-you).

**Who reports it** in the tables below is one of:

| Reporter | Means |
| --- | --- |
| compiler | a `[[deprecated]]` or `[[nodiscard]]` attribute; you see it before the program runs |
| the program | the checker, validator, interactor, controller or generator, while it runs |
| `eo-judge check` | the emulator, reading the whole problem; **never appears in a judge log** |
| `eo-judge lint` | a textual scan of the source, for what no run can see |

All 81 designed codes are built.

## EO1xx — reading a value

Every role reads through the same engine, so these fire anywhere.

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO101` | warning | compiler, the program | a value is read without a name | name it, or say `eo::unnamed` if it genuinely needs none; a failure otherwise says "integer" instead of naming the value |
| `EO102` | warning | compiler | a validator reads a number with no bounds | give the bounds, or say `eo::any` |
| `EO103` | warning | compiler | a checker reads a number with no bounds | give the bounds, or say `eo::any`; an unbounded read is how a hostile output crashes a checker |
| `EO104` | warning | the program | the bounds are the whole range of the type | say `eo::any` if any value is allowed, so the intent is on the page |
| `EO105` | warning | the program | a bound does not fit the type, as in `read_int(1, 3000000000)` | read a wider type with `read_long` |
| `EO106` | note | the program | a bound is one away from a round number, such as `200001` | compare it with the statement. A bound within one of a value the program has already read under a name, such as `n - 1` or `n` itself, is taken as computed from it and raises nothing |
| `EO107` | warning | the program | one name is read with different bounds, or as two different kinds, at two places | constrain it one way, or read it one way |
| `EO108` | warning | compiler, the program | a token or a line is read with no length or no charset | give a length and the characters it may hold, or say `eo::any` |
| `EO109` | warning | the program | a real number is read with no rule on its digits | say how many digits follow the point: `read_real(low, high, least, most, name)` |
| `EO110` | note | the program | a local input has CRLF line endings | the judge converts them and so does a local run, so this is a note about the file, not the test |
| `EO111` | note | the program | a token over 1 MB was held in memory | bound its length if the format allows |
| `EO112` | warning | the program | a message has more or fewer `{}` than values, or a lone `{` or `}`, such as a printf-style `"%d"` | write one `{}` for each value and `{{` or `}}` for a brace; the message keeps every value, the extra ones appended, and the verdict stands; under C++20, `-DEOLYMP_CHECK_PATTERNS` makes a literal message like that a compile error instead |
| `EO113` | warning | the program | a token is read against a pattern whose every match holds a space, a tab or a line break, as testlib's `readToken("[a-z] {1,5}")` does when ported: testlib drops the space, this library keeps it, and no token can match | read a line with `read_line(pattern, name)`, or drop the space the way testlib did |

## EO2xx — the checker

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO201` | warning | the program | the checker passed the run without reading any of the output | read the output, or use a built-in checker |
| `EO202` | warning | the program | the checker read neither the input nor the answer | a verdict that cannot depend on the test is not a checker |
| `EO203` | warning | the program | the answer file still holds unread content when the checker finished | read it, or say why not: `c.jury.skip_rest("...")` |
| `EO204` | warning | the program | a wrong answer carries no message | say what was wrong with it; the message is what the author sees in the log |
| `EO205` | warning | the program | a score outside 0..1, or negative `eo::points`, was clamped; a score of 2 or more is called a likely percentage or points | keep the formula inside the test; Eolymp reads a fraction of the test cost, not a percentage: use `eo::ratio(a, b)`, or `eo::points` for points |
| `EO206` | warning | the program | a score is a hair below full marks, from floating-point division, or its points are below the cost but round up to it in the judge's 32-bit float, so the run counts as ACCEPTED | use `eo::ratio(a, b)`, which is exact, or `eo::accept` for full marks |
| `EO207` | warning | the program | more points were given than the test is worth | the judge clamps it to the cost; scale the formula instead |
| `EO208` | warning | the program | a partial score, 0 included, on a test worth 0 points, such as a sample: the judge counts any points as reaching a cost of 0, so the run is ACCEPTED | end an answer that earns nothing with `eo::wrong`, which the sample then shows as a wrong answer |
| `EO209` | warning | the program | the checker ran for more than half of the judge's 10 000 ms wall limit | a slower machine or a busy judge would not finish it in time |
| `EO210` | note | the program | the checker printed a large amount before its verdict | stored logs are truncated; print after the verdict line |
| `EO211` | note | the program | the checker runs as the legacy type, which swaps its last two arguments | the ordinary `PROGRAM` type is the norm |
| `EO212` | warning | the program | the problem declares many answers and the checker only compares with the jury's | compare properties, not the jury's text, or declare `eo::unique` |
| `EO213` | warning | the program, on the judge | `TEST_COST` is missing, and the test is taken to be worth 100 points, or is not a number, and it is taken to be worth 0; points and partial scores then follow from the wrong cost | the judge sets `TEST_COST` for every checker, interactor and controller; report its configuration |
| `EO214` | warning | the program | `c.optimum(by_the_jury, found, direction)` or `eo::compare(found, by_the_jury, direction)` compared two reals with `==`, so a correct answer that rounding moved is a wrong answer or a jury error; the verdict is the one `==` gives, as before | say how close is equal: `c.optimum(by_the_jury, found, direction, eo::within(1e-6))`, which is the rule of `eo::close_enough` |

## EO3xx — the validator

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO301` | warning | the program | a test is in a subtask and the validator declares no table | declare `v.subtasks<Limits>({...})`, which is what buys the per-subtask checks |
| `EO302` | warning | the program | two subtasks other than group 0 have identical limits; group 0, the examples, usually shares the full limits and is not compared | one of them is probably a copy and paste |
| `EO303` | warning | the program | the validator ran for more than half of the judge's 30 000 ms wall limit | a slower machine or a busy judge would not finish it in time |
| `EO304` | note | the program | `cases` is used with no `sum_limit` | most multi-test statements bound the sum of n |
| `EO305` | warning | compiler | a structural check was computed and its result ignored, as in `eo::is_tree(…)` with no `v.require` | pass it to `v.require`; the check is `[[nodiscard]]` |

## EO4xx — interaction

Raised by `eo::interactor`, `eo::controller` and `eo::phases`.

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO401` | warning | `eo-judge lint` | an interactor or controller prints with `cout`, `printf` or `puts` | that writes into the solution's input; send with `it.send` and log with `eo::log` |
| `EO402` | warning | the program | round trips were answered with no `eo::budget` declared | declare the statement's limit with `eo::budget`, which then enforces it |
| `EO403` | warning | the program | a budget was declared and never spent | spend it before every reply, or drop it |
| `EO404` | note | the program | the solution was still sending when the interactor finished | the protocol has a step the statement does not describe |
| `EO405` | warning | the program | the controller accepted without reading anything from any instance | read what they sent |
| `EO406` | note | the program | the test has an answer file the interactor never read | drop it, or read it |
| `EO407` | warning | the program | a `run_count` handoff is large | the judge copies it between runs, so keep it small |
| `EO408` | warning | the program | an instance was started and never talked to | spawn it where it is needed, or drop it |
| `EO409` | warning | the program | while an interactor was still writing to the solution, or a controller to one of its instances, the solution or that instance sent more than 16 MB of answers, which the library stops taking in, so the two can wait for each other until the time limit; the 16 MB are counted for each instance, and only the instance being written to is taken in from; it is written to stderr the moment it is raised, because a run killed at the limit never reaches the report | read the solution's or the instance's answers between sends instead of sending everything first |

## EO5xx — the generator

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO501` | warning | the program, `eo-judge lint` | a stress run made no random draw, or the source uses `rand()`, `time()` or `std::random_device` | draw from `g.rng()`; every iteration of a stress run must get a different test, and every rerun the same one |
| `EO502` | warning | the program, `eo-judge lint` | the test is very large, or the source uses `std::shuffle` or an unordered container | those depend on the standard library, not only on the seed, so GCC and clang disagree |
| `EO503` | warning | the program | bytes reached stdout without going through `g.out` | write the test with `g.out.line`, which is what the writer buffers and terminates |
| `EO504` | warning | the program | the generator ran for more than half of the judge's 60 000 ms wall limit | a slower machine or a busy judge would not finish it in time |
| `EO505` | warning | the program | a shape threw away more than 3 of every 4 random draws (past the first 1 000) | a slightly denser request fails outright; ask for fewer, or build it another way |
| `EO506` | warning | compiler | a random draw's result was ignored | every draw is `[[nodiscard]]`, because a discarded draw still shifts the stream |
| `EO507` | note | the program | the default random stream is used alongside named ones | an added draw shifts every later draw of the default stream; name them all |

## EO8xx — what only the whole problem shows

These come from `eo-judge check`. No single program can see them, and **none of them ever
appears in a judge log**. The hostile outputs and hostile clients ship with the emulator, so
the author writes no test code for any of them.

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO801` | warning | `eo-judge check` | the checker does not accept a jury answer, run as `checker(input, answer, answer)` | the checker and the answer files disagree; fix it before a contestant meets it |
| `EO802` | warning | `eo-judge check` | the checker accepts an empty output | it is not reading the contestant's answer |
| `EO803` | warning | `eo-judge check` | the checker accepts the input echoed back as the output | it is not comparing enough |
| `EO804` | warning | `eo-judge check` | the checker accepts the jury answer with one token changed, on a problem declaring `eo::unique` | the answer is declared unique, so this must be wrong |
| `EO805` | warning | `eo-judge check` | a hostile output makes the checker crash, hang or report a jury error | a contestant's output must give a wrong answer and nothing else; bound every read |
| `EO806` | warning | `eo-judge check` | a test is invalid, with its testset's `--group` or with none; the validator is said to have broken rather than refused the test when the first line it prints starts with `eolymp.h: `, the library's own error, or a signal killed it | a stress run passes no group, so this input would be called invalid |
| `EO807` | warning | `eo-judge check` | a named bound is never reached, at either end, in some subtask | generate a test that reaches it; this is "the maximal tests really are maximal" check |
| `EO808` | warning | `eo-judge check` | a feature declared with `v.feature` is marked by no test | generate one, or stop declaring it |
| `EO809` | warning | `eo-judge check` | two tests in one testset are byte for byte the same | drop one, or generate a different test |
| `EO810` | warning | `eo-judge check` | a test is invalid under a testset that depends on its own | the groups do not nest the way the scoring assumes |
| `EO811` | note | `eo-judge check` | a subtask has fewer than two tests | one input decides the whole subtask |
| `EO812` | warning | `eo-judge check` | a generator gives different bytes twice, or under GCC and clang | unspecified argument order, `std::shuffle`, unordered iteration or signed `char` |
| `EO813` | warning | `eo-judge check` | an extreme value of a declared `g.option` does not produce a valid test. Each extreme replaces the option in the stored tests' arguments, one test after another, until the generator accepts it; a refusal by `g.require`, exit 4, moves on to the next test, and an extreme that every stored test's options refuse that way is not reported; any other failure, a crash or a timeout, is reported at once | overflow and edge cases at the bounds |
| `EO814` | warning | `eo-judge check` | a hostile client gets an interaction failure rather than a wrong answer or a time limit | the clients exit at once, print garbage, go silent and flood the pipe |
| `EO815` | warning | `eo-judge check` | the same solution run twice gives a different verdict or score | something in the problem uses the clock or unseeded randomness |
| `EO816` | warning | `eo-judge check` | a correct solution uses more than half of a limit | a rejudge on a slower machine would fail it |
| `EO817` | warning | `eo-judge check` | a checker that gives partial scores sits on an `ALL` testset | `ALL` pays nothing unless every test passes, so the fractions are thrown away |
| `EO818` | warning | `eo-judge check` | the checker rejects its own answer rewritten with CRLF and trailing spaces | a contestant's output is not normalised; accept the whitespace or declare an exact format |
| `EO819` | warning | `eo-judge check` | a subtask no attached solution fails, or a correct solution that does not score full marks | a subtask every solution passes tests nothing; add one that should lose it |
| `EO820` | warning | `eo-judge check` | a subtask no attached solution passes | check the tests and the limits; nobody can score it |
| `EO821` | note | `eo-judge check` | the problem has fewer than two correct solutions | a reference no stress run can cross-check |

## EO9xx — configuration

Also from `eo-judge check`, reading the problem's configuration rather than its tests, and
running the tests `problem.json` declares for the problem's own programs. These are the checks
the platform should eventually make when a problem is saved.

| Code | Severity | Reporter | Fires when | What to do |
| --- | --- | --- | --- | --- |
| `EO901` | warning | `eo-judge check` | an `EACH` testset carries `ICPC` or `ICPC_EXPANDED` feedback | ICPC stops after the first test worth nothing, so the rest score 0; use `COMPLETE` |
| `EO902` | warning | `eo-judge check` | an interactive problem has no wall `timeLimit` | the interactor is given the wall limit plus a second and nothing else bounds it |
| `EO903` | warning | `eo-judge check` | a program includes a quoted header with no matching `files[]` entry, other than `eolymp.h` and `eolymp-shapes.h`, which the judge's runtime carries | attach it, or the first run fails to compile and shows up only as a submission failure |
| `EO904` | warning | `eo-judge check` | a testset is listed among its own dependencies | nothing in it will ever run |
| `EO905` | warning | `eo-judge check` | an examples testset carries points, or an example is not flagged as one | samples are shown, not scored |
| `EO906` | warning | `eo-judge check` | a `WORST` testset whose tests do not all carry the testset's full value | the group takes the smallest test score, so every test must carry it |
| `EO907` | warning | `eo-judge check` | the testset costs do not add up to the problem's total | make them add up, or full marks are unreachable |
| `EO908` | warning | `eo-judge check` | `runCount` is above 1 on a problem that is not interactive | `run_count` chains an interactor's output into the next run |
| `EO909` | warning | `eo-judge check` | the problem has more than about 1,200 test rows | Basecamp stops judging above that |
| `EO910` | note | `eo-judge check` | the programs of one problem carry different copies of a header, counting the copy eo-judge carries for a program that attaches none | attach the same release to every program, or none to use the one the judge carries |
| `EO911` | warning | `eo-judge check` | a test in `validatorTests` gets the other answer from the validator than its `expect`: a `VALID` input refused, an `INVALID` one accepted, or the validator broken on either, its first line starting with `eolymp.h: ` or killed by a signal, or out of its 30 s. `run` does not read them | the validator and the test disagree; fix the validator, or the test's `expect` if the validator is right |
| `EO912` | warning | `eo-judge check` | a test in `checkerTests` gets another verdict or score from the checker than its `expect`: `ACCEPTED`, `WRONG_ANSWER`, `PARTIAL`, `FAILURE`, or `{"points": x}`, the points the run pays. `run` does not read them | the checker and the test disagree; fix the checker, or the test's `expect` if the checker is right |
