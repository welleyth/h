# Changelog

## Unreleased

No verdict changes. Everything below is new API, which a program meets only when it calls it,
and new eo-judge output: a generator can carry a secret salt, a validator lists its features
as testlib's does and records numbers per test, and eo-judge shows both, in `check -v`, in a
new `describe` command, in notes EO825 and EO827 and in warning EO826. A program that calls
none of it writes and reads byte for byte what 2.3.0 did, and only its `--eo-describe` output
gains lines; EO501 fires on one more case, below. One new name in `eo`, `salt`, which matters
only to a program that says `using namespace eo;`, and one macro, `EOLYMP_COLD`. The header's
own line numbers in warnings move, since it grew; otherwise `run` and `check` print what 2.3.0
printed on every fixture, apart from the new lines below.

### New for problem authors

- **`eo::salt`, a secret of the problem's own.** The seed of a test is derived from its
  arguments, and the library is public, so anyone who guesses `-n=200000 -shape=path` can
  rebuild a stock test byte for byte and hard-code its answer. `eo::generator g(argc, argv,
  eo::salt("…"))` mixes exactly 32 hex digits from the source into every stream: the seed
  becomes SipHash-2-4 of the arguments keyed by the salt, so the arguments alone reproduce
  nothing, and one test's seed says nothing about another's. A stress run's appended seed is
  folded in after it. A salt that is empty, of another length or not hex is the library's
  error, with the line: `give it 32 hex digits; eo-judge init writes one`. Without a salt
  every test is the one 2.3.0 generates. See [What is secret and what is
  not](docs/generator.md#what-is-secret-and-what-is-not).
- **`v.features({"path", "star"})`** declares several features at once, exactly as a
  `v.feature` call for each, so the two mix and a name declared twice is declared once.
  `v.saw` still marks, and a name never declared now stops it with the line and the names that
  were declared: `saw("pth") names a feature that was never declared; it declares path, star`.
  testlib's `addFeature` is `v.feature` and its `feature`, which marks, is `v.saw`.
- **`v.stat("depth", d)`** records a number for the test, the larger kept when it is recorded
  twice. Some come free: every named integer read exactly once in the test, outside a
  sequence, under its name (`n`, `m`, `t`), every token or line read once as `name.length`,
  the total of every `sum_limit`, and, under `--eo-describe` only, `.max_degree`, `.leaves`,
  `.depth` and `.diameter` from `read_tree` and `.max_degree` and `.components` from
  `read_graph`, each under the read's name, as `edge.depth`. On the judge, in `make bench`'s
  new `validator cases`, a mark costs about 160 instructions and a stat about 25, against
  about 530 for the case's own reads; the shapes cost one test of a flag. See [Coverage and
  features](docs/validator.md#coverage-and-features).

### What a message or a log says

- A generator's `--eo-describe` ends, after a line break of its own, with `eo-describe
  randomness drawn=yes salt=no`: whether that run drew from any stream, a stream that was
  copied counting and one only asked for not, and whether the generator has a salt, `public`
  for one the library publishes. Given a test's arguments, `gen <args> --eo-describe` runs on
  them and writes none of the test.
- **EO501 counts draws.** A stress run that asks for a stream with `g.rng` and never draws from
  it now gets the warning, as one that never asks does; one that draws from a copy of a stream
  does not. A warning, so no verdict changes.
- A validator's `--eo-describe` prints `eo-describe stat <name> <value>` after its features.

### eo-judge

- **`eo-judge init` writes a fresh salt into every generator**, 32 hex digits from
  `crypto/rand`, so no two problems it makes share a test.
- **EO825 is a note**: `check` notes a generator that draws randomness with no salt. A note
  does not fail `--strict`, nor the Action's `strict: true`, so every problem that passed
  `check --strict` under 2.3.0 still does; the note's fix says a salt is for a new problem,
  since adding one regenerates every random test. It runs each distinct set of stored
  arguments with `--eo-describe` until one draws or the generator is salted; a generator that
  never draws is never named.
- **EO827**, a note: a generator that draws with a salt eolymp.h publishes, the templates'
  placeholder or one of the pages' or the fixtures'. A test holds every salt in the
  repository's pages and examples to the header's list.
- **`check -v` shows which test has which feature**, a table per testset, and `check --json`
  carries it as `coverage`. A validator that declares no feature prints exactly what 2.3.0
  printed.
- **`requires`** on a testset in `problem.json`, `"requires": ["star"]`, names features some
  test of it must have; **EO826** reports one that none has, one the validator does not
  declare, listing the names it does, and `requires` on a problem with no validator. It is
  read as strictly as the rest of the file.
- **`eo-judge describe <problem>`** prints one line per test, its size, the features marked
  and its stats, `1:2  22 B  star  n=6 edge.max_degree=5 edge.leaves=5 edge.depth=1
  edge.diameter=2`, and with `--json` the same as `tests`, with `declared`, the validator's
  features. It generates the inputs only and runs no solution. A test that could not be
  generated, or that the validator broke on, is shown as such, the others are described all
  the same, and it exits 3. See [docs/judge.md](docs/judge.md#describe).

### Speed and size

- `make bench` against 2.3.0: every path within 2%, the most `read_token` at +1.2%, `read_tree`
  at +1.1% and `read_line` at +0.9%; `generator fixed` takes 5.9% fewer instructions.
- `make budget`: the validator's -O2 object is 204 KB here, from 198 KB, and 5.6 times the
  standard headers' compile time, as before; on CI's musl leg it is 209 KB. The checker's is
  unchanged at 183 KB.

### For maintainers

- **`EOLYMP_COLD`**, `[[gnu::cold]]` under GCC and clang, marks the code that only
  `--eo-describe` runs and the errors of `saw` and `stat`. 2.3.0 sits close to GCC's
  `--param inline-unit-growth` at -O2: a few more lines in the validator's `describe()` made
  GCC stop inlining `diagnostics::shared()` into the readers, and `read_line` paid 4.8% for
  code it never ran.
- **`make cliff`**, run by `make check`, counts the inlining GCC refuses for that limit in
  every bench program that 2.3.0 kept under it, and fails when one refuses any. It holds for
  GCC 14.2.0 on musl, CI's musl job, and skips elsewhere; the three programs 2.3.0 already
  pushed past the limit are left out, since their counts vary between builds of one GCC.
- **`make bench`** gains `validator cases`, a million cases each marked with `v.saw` and
  measured with `v.stat`; a program a base revision cannot build is listed as not there.
- 25 new mutants, for the salt, SipHash, the drawn-stream check, `v.features` and the stats;
  `make mutants` kills 63 of 63.
- Fixtures `judge/testdata/salts`, a generator of each kind for EO825, and
  `judge/testdata/features`, a tree problem whose validator lists its shapes.

## 2.3.0

Two behaviours of the header change, both listed first: a jury program that sets a numeric
locale whose decimal point is not a dot now reads and writes reals with a dot, as every other
program always has; and a tolerance allows `1e-15` more for rounding, which turns a wrong
answer at the boundary of `eo::close_enough`, `c.reals(eps)` and `eo::within(eps)` into an
accept. One eo-judge verdict changes: a run with a relative `--work`, which was broken, now
judges as a run without it does. Everything else is new API, which a program meets only when
it calls it; messages and logs that say more; new warnings, EO113 from the header and EO822,
EO823, EO824, EO911, EO912 and EO913 from `eo-judge check`; warnings counted per file; and four
new names in `eo`, `pattern`, `any_order`, `big` and `absolute`, which matter only to a program
that says `using namespace eo;`. With this release every call a testlib validator, checker,
interactor or generator makes has its counterpart, mapped in `docs/testlib.md`.

The header also builds on Windows, with MSVC for x64 and x86, clang-cl and mingw-w64, and
judges there as it does on Linux. That changes nothing on Linux: the Windows work leaves every
verdict, score, message, exit code, warning and byte as the rest of this release has it, apart
from the `eo-report` fix below, which touches only a path holding a quote, a backslash or a
control character. eo-judge gains the stress run and tests for the jury's own validator and
checker, the two things Polygon had that it did not, and runs the two problem types the UCPC
practice session needed: output-only problems, where the contestant uploads a file per test,
and function problems, where the judge wraps the contestant's function in a code template.
Those two types add one member to the header, `c.output_only`, and change no verdict: a
program judges every run as it would without them, and eo-judge judges every problem that
loaded before them as it did. The header's own line numbers in warnings move, since it grew.

### What changes for a program

- **A real is read and written with a dot in every locale.** A jury program that called
  `setlocale(LC_NUMERIC, …)` with a locale whose decimal point is a comma, `de_DE` or
  `uk_UA`, read `1.5` as 1, since `strtod` stopped at the dot, and wrote `eo::fixed(1.5, 2)`
  as `1,50` and a checker's points as `points 20,5`, which the judge cannot read. It now reads
  1.5 and writes `1.50` and `points 20.5`, whatever the locale. A validator that refused
  `1.5` under such a locale accepts it now; a program that never sets a numeric locale sees no
  change.
- **A tolerance allows 1e-15 more, for rounding.** `eo::close_enough`, and with it
  `c.reals(eps)`, `eo::compare` and `c.optimum` with `eo::within(eps)`, called `0.500001`
  against `0.5` at `1e-6` wrong, since the difference is `1.0000000000287557e-06` in doubles,
  and accepted `0.499999`, whose difference rounds the other way; `1000.1` against `1000` at
  `1e-4` was wrong for the same reason. A value now also counts as within the tolerance when
  its difference is up to `eps + 1e-15`, or up to `(eps + 1e-15)` times the expected value,
  as testlib's does; every comparison 2.2.1 accepted, by `|difference| <= eps` or by
  `|difference| / |expected| <= eps`, is still accepted. So the change only turns a wrong
  answer at the boundary into an accept: over 6.9 million pairs built within a few ULPs of
  the boundary, from `1e-300` to `1e300`, none went the other way.

### New for problem authors

- **Patterns, in testlib's syntax.** `eo::pattern("[a-z]{1,10}")`: characters, a backslash
  before any symbol, classes of bytes with ranges and `[^…]`, `?`, `*`, `+`, `{n}`, `{n,m}`,
  `{n,}`, alternatives and groups. `v.read_token(p, name)`, `v.read_tokens(count, p, name)`
  and `v.read_line(p, name)` read what must match it in a validator, the same three on every
  stream of a checker, an interactor and a controller, and `r.pattern(p)` draws from it, one
  construct at a time, each uniformly. A message names the token and the pattern:
  `line 1, first: "anna" does not match "[A-Z][a-z]{0,9}"`. Matching never backtracks: it
  costs the token's length times the places in the pattern live at once, a repeat of a class
  is one place at any count, and a pattern of more than 4,096 places is refused where it is
  made; the largest in the pages, the tests and testlib's examples has 27. A read against a
  pattern with a longest match stops one character past it, and memory is bounded by the
  pattern, not the token. A pattern that does not parse is refused with its column, at
  compile time for a literal under C++20 with `-DEOLYMP_CHECK_PATTERNS`. The syntax departs
  from testlib's in five places, all where testlib's matcher is a trap; `docs/validator.md`
  lists them, with every refusal. A draw is refused, with its line, when the pattern has a
  class with nothing visible in it or could draw more than 100,000,000 characters.
- **Warning EO113** fires when a token is read against a pattern whose every match holds a
  blank, as a ported `readToken("[a-z] {1,5}")` is once its space is kept: no token matches.
- **Six ready-made comparisons**, so that each of testlib's 21 stock checkers is one call:
  `c.tokens(eo::any_case)`, `c.tokens(eo::any_order)` (`uncmp`), `c.integers()` (`ncmp`,
  `icmp`), `c.integers(eo::big)` (`hcmp`), `c.yes_no()` (`yesno`, `nyesno`) and
  `c.reals(eps, eo::absolute)` (`rcmp`, `acmp`, `rncmp`), whose error allows `1e-15` more.
- **One case of a multi-test input as a test of its own.** `./validator test.txt --eo-case=k`
  validates the test and writes case k to stdout with the count written as 1, as testlib's
  `--testCase` does, and `--eo-describe` gives each case's bytes, as its markup does. The
  count is the one integer before the first case whose value v.cases was given; when there is
  none, or more than one, the flag is refused, as it is when given twice.
- **`c.output_only("why")`** says that a checker of an `OUTPUT` problem reads neither the input
  nor the answer on purpose — every test is the same task, any valid answer is accepted — so
  neither EO202 nor EO203 is raised; EO201 stays. See
  [docs/checker.md](docs/checker.md#output-only-problems).

### What a message or a log says

- **A malformed number is named whole.** `1e5` read as an int, and `1e-3`, `0,5` or `1.5e3`
  read as a real, said `expected a line break after n, found "e"`; they now say
  `line 1, n: expected an integer, found "1e5": it has a character that cannot be part of the
  number`. Only the message changes: the same read refuses the same test. The reason given is
  the whole token's, so it can stand where a narrower one would have: `-0x` has "a character
  that cannot be part of the number" rather than "zero written with a minus".
- **A checker that dies still leaves a log.** An exception nothing caught ends a checker, an
  interactor or a controller as a jury error that quotes up to 200 bytes of its `what()`,
  exit 3 where it was SIGABRT; and a checker on the judge that dies of `SIGSEGV`, `SIGABRT`,
  `SIGFPE`, `SIGBUS` or `SIGILL`, a stack overflow included, writes its verdict line and what
  it held, and then hands the signal to the handler that was there before, the C library's,
  which ends the program, or a sanitizer's, which reports. On the judge each `eo::log` line
  is flushed until 64 KB, all a stored log keeps, so it reaches that log; what `printf` still
  buffers does not. The checker's log was empty in both
  cases. Every one of these runs was, and is, a VERIFICATION_FAILURE or an
  INTERACTION_FAILURE.
- **A hyphen that reads as a backwards range is told where to go.** `charset("()- ")`, and
  the same class in a pattern, are refused as before, and now say to write the range's low
  end first or to put a `-` that stands for itself first or last.
- **A warning is counted at its file's line.** The same warning on line 12 of two files is two
  warnings, where it was one counted twice under the first file's name.
- **A validator's lookahead waits for the pipe.** A message that quotes what follows reads up
  to 64 bytes of it, or to the end of the input, before quoting, so a test given on a pipe
  that pauses is refused with the same text as the same test given as a file.
- **`eo-report` is JSON for every path.** A warning raised in a file whose path holds `"`, `\`
  or a control character, as every Windows path does, made the line invalid JSON, and
  eo-judge fell back to the spoken lines, which split at spaces. The path is now escaped.

### eo-judge

- **A relative `--work` works.** The validator, the checker and the interactor were given the
  test's paths relative to eo-judge's directory while running in their own, so none of them
  found its files: every test read invalid, and every run of an interactive problem was a
  `RUNTIME_ERROR`. The workspace is now made absolute first. This is the verdict the release
  changes in eo-judge, and only under a relative `--work`.
- **A `--work` that is the problem's directory, holds it or lies inside it is refused**, since
  eo-judge clears the directories it makes there and removed the problem's own `tests/` or
  `stress/`; and one eo-judge uses a `--work` at a time, a second exiting 3.
- **EO806 tells a validator that refused a test from one that broke.** eolymp.h's validator
  exits 3 for both, and `check` said "the validator could not run" for every test an eolymp.h
  validator refused. It now says "the validator rejects it" unless the first line starts with
  `eolymp.h: ` or a signal killed the validator, which is then named. Which tests are valid does
  not change, but a warning count can: a refused test gave the same line, "the validator could
  not run", with its group and without one, and the report merged the two into one warning;
  now "the validator rejects it" and "the test is invalid with no --group" are two.
- **`eo-judge stress <problem> --args '-n=[1..8] -max=[1..100]'`** runs the platform's stress
  on your machine: every `[a..b]` becomes a random integer on every iteration, a 16-hex-digit
  seed is appended, and each input is generated, validated with no `--group`, answered by the
  reference and judged for every compared solution, the checker given a test worth 0. It stops
  at the first `COUNTEREXAMPLE`, a solution that breaks its type as the platform reads it, and
  at an `INVALID` input, the generator's fault, or a `BROKEN` iteration, unless `--continue`
  says to go past those two. It prints the resolved arguments as a `"generator"` line that
  pastes into `problem.json` and makes the same input again. `--gen`, `--reference`,
  `--solution` (repeatable), `--iterations` (100) and `--timeout` (300 s) follow `run_stress`;
  `--arg` gives one argument with spaces in it, `--work` keeps the iteration it stopped at, `-v`
  lists every iteration, and `--json` puts a `stress` object into the usual report. The
  warnings of the generator and the checker, EO208 for a partial score on a test worth
  nothing among them, are reported once each. It exits 1 when it stopped at an iteration, 0
  when none was found, and 3 when the timeout came before any iteration passed. See
  [docs/judge.md](docs/judge.md#stress).
- **`validatorTests`** in `problem.json`, `[{"input": "…" | "file": "…", "expect": "VALID" |
  "INVALID", "group": k}]`, and **`checkerTests`**, `[{"input", "output", "answer", "expect":
  "ACCEPTED" | "WRONG_ANSWER" | "PARTIAL" | "FAILURE" | {"points": x}, "cost", "group"}]`, are
  the tests Polygon keeps for a validator and a checker. `eo-judge check` runs them and reports
  every one the program answers otherwise as the new warnings **EO911** and **EO912**; `run`
  does not read them. Both are read as strictly as the rest of `problem.json`, and a validator
  test's CRLF is folded as a test's is.
- `eo-judge init` writes one test of each kind into every template.
- `init --json`'s refusal names `stress` among the commands that take the flag.
- **eo-judge runs `OUTPUT` problems.** A solution gives the files it would upload,
  `"outputs": {"1": "one.txt", "1:2": "two.txt"}`, keyed by `"group:index"` or by an index no
  other testset shares, and a file that cannot be read is refused at load. `run` and `check`
  give each file to the checker as the run's output, and a test with no file is judged as an
  empty one, saying so. `check` tries an empty output on every test, not only the first
  (EO802), and the jury's answer of the next test as this test's output, which a checker that
  never looks at the input accepts: new warning **EO822**. `stress` refuses the type. See
  [docs/judge.md](docs/judge.md#output-only-problems).
- **eo-judge runs `FUNCTION` problems.** `problem.json` takes `"templates": [{"runtime",
  "header", "source", "footer"}]`, one per runtime, and a solution takes a `"runtime"`; a
  solution is built from its runtime's header, its source and the footer, concatenated as the
  judge does, and a solution without a runtime takes the one C++ template. A solution with its
  own `main()` stops `run` with the compilation error a contestant gets on the judge, and the
  message names the template. `check` reads every C++ template: a header that does not end
  with a `#line` directive and a line break, or a footer that does not start with a line break
  (new warning **EO913**), a stub that does not compile or is judged as anything but a wrong
  answer (**EO823**), and a template in which a whole program compiles (**EO824**). `stress`
  runs the type, and its refusal of the other types now reads "eo-judge stress runs PROGRAM and
  FUNCTION problems only, and this one is INTERACTIVE". On every type, a solution's `runtime`
  now sets the C++ standard it is compiled with; one that names a runtime other than C++, or
  any runtime on an `OUTPUT` problem, is refused. See
  [docs/judge.md](docs/judge.md#function-problems) and the new
  [docs/templates.md](docs/templates.md), with the `#line` pattern for C++ and the Python and
  Java ones.
- **`eo-judge run --transcript`**, on an interactive problem, prints every run's dialogue
  under it, phase by phase, for a statement's example interaction; `--json` carries it as a
  run's `transcript`. The relays it adds keep every broken pipe a run would meet, but cost time:
  a run of many round trips can take up to twice as long, so judge with `-v` for verdicts.

### Windows

- **The header builds and judges on Windows.** Validators, checkers, generators, interactors,
  phases and controllers build with `cl` and `clang-cl` under `/W4 /WX` and with mingw-w64
  under `-Wall -Wextra -Werror`, in C++17 and later, before or after `<windows.h>`, with or
  without `NOMINMAX`; the header never includes it. Standard input, output and error are binary
  and every file is opened in binary, and set to binary again when the library takes one over,
  so a `freopen` of the author's cannot undo it. CRLF and Ctrl-Z mean on Windows what they mean
  on the judge: the jury's CRLF is folded with an EO110 note, the contestant's output is read
  as it is, and a generator writes `\n`. An interactor and a controller take in the other
  side's answers while they write, from one standing writer thread per pipe, so a large send
  does not deadlock; a controller's instances are named pipes. What is Windows' own, the 1 MB
  default stack first, is in [docs/README.md](docs/README.md#windows).
- **eo-judge on Windows** stops at once and says to run it under WSL2, which
  [docs/judge.md](docs/judge.md#on-windows) describes; it runs on Linux and macOS as before.

### Speed and size

Instructions, from `make bench`, 2.2.1 → 2.3.0: a checker's `reals()` 0.95 → 0.84 G and
`eo::fixed` 0.51 → 0.45 G, since the fast paths no longer ask `localeconv` about every
number; a validator's `read_ints` 1.056 → 1.064 G, `read_token` 0.572 → 0.566 G,
`read_reals` 0.712 → 0.727 G, `read_tree` 0.396 → 0.401 G; `tokens()` 0.694 → 0.703 G. The
header is larger: `eolymp.h` has 6,669 lines against 5,008 and 263,614 bytes against
196,293, and the validator `make budget` builds leaves an object of 198 KB against 171 KB,
most of it the whole-token message and `--eo-case`, which every validator carries.
**A validator now builds about 15% slower, 16% under musl:** the one `make budget` builds takes
2.6 s of compiler CPU time with `-O2` against 2.3 s, and 4.0 s against 3.45 s under musl,
because every validator compiles the whole-token message, `--eo-case` and the pattern engine
whether it uses them or not; marking their paths cold saved under 1%.
`make budget` now measures against all the standard headers `eolymp.h` includes, where its
list had fallen a fifth behind; the list is fixed in `tools/budget.py`, and the gate fails when
`eolymp.h`'s includes stop matching it. Each program is the fastest of three builds, the
ceiling is ratcheted from 9 to 8.5 times the baseline, where the validator is under 6, and an
object over 220 KB fails.

### For maintainers

- `tests/fuzz/pattern_fuzz.cpp` checks `eo::pattern` against `std::regex_match` and against a
  direct reading of the pattern as the positions each part can end at, draws four strings
  from every pattern and requires each to match, and feeds raw bytes to the parser.
- The pattern tests count the steps a match visits, so linearity is checked without a clock,
  and pin the size of the queues a match holds.
- Twelve mutants join `tools/mutants.py`: seven on the pattern engine, the pattern read and the
  new comparisons, three on the checked arithmetic written by hand for `cl`, and two on
  `c.output_only`; all 38 are killed.
- `tests/e2e/dies.cpp` is a checker and an interactor that die of an exception, an abort,
  three signals and a stack overflow, run in judge mode; the e2e runner waits for them in the
  background, since dash prints "Aborted" into a dying program's own output. The sanitizers
  take those signals themselves, so a sanitized run skips them.
- The locale tests run where `de_DE`, `fr_FR`, `ru_RU` or `uk_UA` is installed, as on the
  macOS runner and, since check.yml generates `de_DE.UTF-8`, the Linux ones; elsewhere they
  say they were skipped, and the helpers that move the point are tested with `,` and a
  two-byte point everywhere.
- Every call to the operating system is in `src/os.h`, with a POSIX branch that is the code the
  header had before it built on Windows and a Windows branch; the amalgamator leaves an include
  inside `#if` where it is, and the C++17 check reads `_MSVC_LANG`. GCC's checked-arithmetic
  builtins and `__builtin_unreachable` go through `core.h`, with portable versions for `cl` that
  the suite compares with the builtins.
- `make transcript` runs the file roles, the interactor, the phases and the controller on 138
  scenarios and writes each run's exit code and output bytes. CI's `windows` job builds the
  same programs on `windows-latest` with mingw-w64, `cl` for x64 and for x86, and `clang-cl`,
  fails on any byte that differs from Linux's, runs `tests/all.cpp` there without the 17 tests
  that need `fork`, `setrlimit`, signals, FIFOs or `O_NONBLOCK`, and runs eo-judge's Windows
  build. The scenarios include the paths only Windows takes: EO409 while writing, last words
  to a solution that sleeps or never reads, solutions that stop reading, the checker's
  scratch file with a missing `TEMP`, and a `freopen` of the standard streams; and this
  release's new features: patterns read and drawn, every stock comparison, `--eo-case` and
  `--eo-describe`, and the log a checker held when it aborts.
- `make judge` also builds eo-judge for Windows. `tests/hostile` builds both headers after the
  macros `<windows.h>` defines.
- A run's core, `try`, takes a prepared input, a limit, the metadata and a directory, so a
  stress iteration is judged by the code a test is; `checked` reads a checker's exit code and
  log for a solution's run and for a checker test alike; `validatorBroke` is the one place a
  validator's breakdown is told from its refusal.
- The workspace lock is the cache's `flock`, through `lockPath`.
- `Evaluate` takes the `*Solution`, and `Problem.programOf` is the one place a solution's
  program is made: its source, its runtime and its template.
- A compilation error is a `*notCompiled`, kept through the build cache, so `check` can tell a
  program that does not compile from a broken workspace.
- `judge/testdata/stress` and `judge/testdata/authored` are the fixtures of the stress run and
  the authored tests; `judge/testdata/output` (n queens) and `judge/testdata/function` (the
  largest sum of two elements, with the practice session's C++, Python and Java templates)
  those of the two problem types.

### Left as it was

- A native eo-judge for Windows, which would need a job object per run and its tests ported
  off `/bin/sh`: WSL2 runs the Linux one.
- Non-ASCII paths outside the ANSI code page, which `_open` cannot open; a UTF-8 manifest or
  `_wopen` would, when an author needs it.
- A real read after `fesetround` under MSVC or clang-cl, where the UCRT's `strtod` rounds an
  exact decimal such as `1.5` one step away under `FE_UPWARD`; the default rounding reads
  every real the same on every platform.

## 2.2.1

No verdict and no score changes: a program built against 2.2.1 judges every run as it did
under 2.2.0, with the same messages, exit codes, warnings and warning counts, and a generator
writes the same bytes. The header is faster. It is not shorter: `eolymp.h` has 5,008 lines
against 2.2.0's 5,002, and 196,293 bytes against 198,331.

### For problem authors

- **Reading is faster.** Instructions to read, from `make bench`, 2.2.0 → 2.2.1: a validator's
  `read_ints` 2.67 → 1.06 G for 2 million values, `read_longs` 1.93 → 0.67 G, `read_reals`
  1.16 → 0.71 G, `read_line` 0.42 → 0.18 G, `read_tree` 1.08 → 0.40 G; a checker's
  `tokens()` 2.17 → 0.69 G, `reals()` 2.31 → 0.95 G, `lines()` 0.78 → 0.19 G, `read_longs`
  on both files 3.10 → 1.03 G. `tokens()` on 105 MB of 11.6 million words takes 8.1 G
  instructions, where testlib's `wcmp` takes 19.2 G and 2.2.0 took 25.8 G. An interactor runs
  about 850 user-space instructions of its own per round trip instead of about 1,400
  (`perf stat -e instructions:u` over 200,000 round trips with an echoing solution).
- **Writing `eo::fixed` is faster**: a generator writing `eo::fixed(x, 6)` runs 2.09 → 0.51 G
  instructions for 500,000 lines.
- Integers are parsed where they lie in the buffer, a real is read with `std::from_chars` and
  `eo::fixed` written with `std::to_chars` where the library has them, and only while the
  rounding mode is to-nearest and the decimal point is `.`, since `strtod` and `printf` follow
  both; elsewhere, and on macOS's libc++, the old paths run. Lines are taken a run at a time,
  names are not copied for every element, and a tree is accepted after one union-find pass.
- Because the programs finish sooner, the warnings about time, EO209 for a checker, EO303 for a
  validator and EO504 for a generator, fire less often on the same test.
- A controller's `eo::channel` is an `eo::stream`, so it has every read a checker's streams
  have, `read_line`, `read_ints`, `read_reals`, `read_tokens`, `at_eoln` and 2.2.0's
  `read_edges`, `read_tree`, `read_graph` and `read_grid` among them, and
  `numbers(eo::lenient)`, `reals(eo::plain)` and `wrong(…)`. `skip_rest` and `trailing`, which
  only a checker's closing checks look at, are deleted on a channel.
- The header builds without a warning under `-Wpedantic -Wconversion -Wsign-conversion
  -Wold-style-cast -Wuseless-cast`.

### For maintainers

- `make bench` prints the instructions of the main read, check and generate paths under
  `perf`, and `make bench BASE=<revision>` the change against another revision.
- `tests/pinned.inc` pins what the reader, the comparisons, the formatting and the roles say
  today, byte for byte, through buffers of every size and under other rounding modes and
  locales, and what 2.2.0 added on those paths: EO106 on computed bounds, edges, weights,
  grids and `c.lines(eo::exact)`.
- The interactor and the controller share one base for their dialogue, the three roles one
  lookup of their files and one fail-closed ending, the readers one loop for arrays and one for
  choices, and the sizes the header repeats have names.
- A warning code is known by its text: two spellings of one code at one site are counted
  together. The library raises its codes as literals, which compilers merge, so no program
  sees the difference.
- The hostile gate builds a program that uses every role, 2.2.0's additions included, under
  the strict warnings above, and,
  with libstdc++, a program that includes only `eolymp.h` and uses `std::function`,
  `std::unordered_map`, `std::hash`, `std::bind`, `std::not_fn` and `std::invoke`, which it
  has always got through the header. CI runs `make judge` on macOS as well as Linux.
- `a_write_cut_short_by_a_signal_is_resumed` no longer hangs the suite. Its `SIGALRM`
  handler read the pipe the interactor writes to and waited for bytes, and the only writer is
  the thread it interrupts, so a tick that found the pipe empty waited for ever: on every run
  with libc++ at `-O0` on Linux, where building the interactor outlasts the first 20 ms tick,
  now and then in macOS's `-O0` `tests-c++20`, and under load anywhere. The handler now
  drains without waiting, and the test counts every byte the write delivers, so a library
  that dropped the rest of a send a signal cut short would fail it.
- A watchdog stops the suite when one test runs longer than `EOT_TEST_SECONDS` (120 by
  default, `0` turns it off), and says which test it was, instead of letting CI wait an hour.
- Left as they were:
  - An interactor still polls before each write. The poll is the only call that tells both
    whether the write can go ahead and whether the solution's output must be taken in first.
    Pipes refuse `RWF_NOWAIT` on Linux 6.1, a non-blocking standard output would break jury
    code that writes to it, and writing or taking in at another moment changes the round trips
    and EO409s the log reports.
  - EO204 still warns about an empty `jury_error` message in a checker and not in an
    interactor or a controller: making the roles agree adds or drops a warning on some runs.
  - The simplification removed about the lines it was planned to remove, and the faster code
    added about as many: `src/` is 555 lines in and 539 out against 2.2.0, where the plan
    counted −214 for the simplification alone.

## 2.2.0

One verdict changes, for one kind of program: a jury program that ends before its verdict,
through `std::exit` or `std::quick_exit` or with its role object never destroyed, now fails
closed, where it used to pass whatever its exit code said. It is listed first below, with every
kind of program it touches. Everything else is new API, which a program only meets when it
calls it; bytes that broken or edge generators wrote; warnings that fire where they should;
eo-judge; and nine new names in `eo`, which matter only to a program that says
`using namespace eo;`.

### What changes for a program

- **Any `exit()` before a verdict fails closed.** A checker, an interactor or a controller that
  calls `std::exit` or `std::quick_exit` before its verdict is a jury error, exit 3, `the
  checker ended without a verdict: exit() was called, or the checker was never destroyed`,
  as returning from `main` without one has been since 2.0.0. That includes an `exit(1)`,
  `exit(2)` or `exit(7)` meant as a wrong answer or as points, and a role object that is never
  destroyed, made with `new` and leaked or kept past `main`: a leaked checker's exit 0 was an
  accept and is a jury error now. A validator's early exit, and a leaked validator, run the end
  checks, the end of the input and every `sum_limit`, as validator.md promised, so `exit(0)`
  after half a test is invalid instead of valid. A generator's early exit, and a leaked
  generator, check the arguments and write what `g.out` still holds: all of a test under a
  megabyte, which was lost, and the tail of a bigger one, whose first megabytes were already
  written. `std::_Exit` and `std::abort` run nothing and cannot be caught, and
  `std::quick_exit` is caught only where the C library offers `at_quick_exit`: glibc and musl,
  so the judge, and not macOS, where a `quick_exit(0)` still reads as an accept.
- **A generator given an option it does not declare writes nothing.** It exits 3 with
  `unknown option -m` as before, but no longer writes the first megabytes of a test over
  1 MB before it does. A generator that declares an option after its first megabyte of output
  writes the same bytes as before, later.
- **A generator's argument errors go to stderr.** `n=5 is not an option` and `this program
  already has a generator` were printed on stdout, into the test, before the generator had
  pointed its reports at stderr; the generation failed with exit 3 as it still does, and
  eo-judge, which shows a failed generator's stderr, now shows why.
- **A generator line no longer ends in a space before an empty value.** `g.out.line("a", "")`
  wrote `a ` and now writes `a`, which a validator's `read_eoln()` accepts. Only the
  separators before values that print nothing at the end of a line go; a line whose last value
  prints anything is byte for byte what it was.
- **`g.require` exits 4.** A generator whose options do not go together still fails its
  generation, with the same message on stderr; the exit code says why, for eo-judge's EO813.

### New

- **New names in `eo`**: `exact`, `exact_t`, `within`, `tolerance`, `compare`, `standing`,
  `weighted`, `weighted_edge` and `weight_bounds`. A program that writes `eo::` sees nothing
  new. One that says `using namespace eo;` beside `using namespace std;`, or beside names of
  its own, can find an unqualified use ambiguous now; that pattern is outside the compatibility
  promise, as README.md now says, and `eo::` is the fix.
- **Edge lists in one call.** `read_edges(m, n, name)` on the validator and on every checker
  and interactor stream, with its vertices bounded to 1..n, and `eo::weighted(low, high)` before
  the name for a `long long` weight on every edge, returned as `eo::weighted_edge {u, v, w}`.
  `read_tree` and `read_graph` take `eo::weighted` too.
- **`read_tree` and `read_graph` on checker and interactor streams**, with the validator's
  checks and messages, blamed on the stream: a wrong answer on the output, a jury error on the
  answer file.
- **`read_grid(rows, cols, charset, name)`**: lines of exactly `cols` characters on a validator,
  tokens on a stream.
- **`g.option<std::optional<T>>(name, low, high)`**, an option with no default: `std::nullopt`
  when it is left out; `--eo-describe` marks it `optional`.
- **`shapes::cocircular(draw, count, limit)`**, the same points as `cocircular(draw, count)` when
  they fit inside `limit`, and a jury error when they do not. `cocircular(draw, count)` draws
  the same bytes as before.
- **A tolerant optimum.** `c.optimum(by_the_jury, found, direction, eo::within(eps))` treats two
  reals as equal within `eps`, absolutely or relatively, the rule of `eo::close_enough`; and
  `eo::compare(found, by_the_jury, direction)`, with `eo::within(eps)` for reals, returns
  `eo::standing::better`, `equal` or `worse` and ends nothing. The optimum without a tolerance
  compares with `==` as before, and warns (EO214) when its type is a real.
- **`c.lines(eo::exact)`** compares line k with line k, blank lines and indentation included.
  `c.lines()` is unchanged, and checker.md now says exactly what it ignores.
- **Compile-time help.** A value the library cannot print, such as a struct in `g.out.line`,
  stops the build at one `static_assert` that says so, instead of a page of template errors.
  Under C++20, `-DEOLYMP_CHECK_PATTERNS` checks every literal message at compile time, so
  `eo::wrong("got %d", x)` does not build; it is opt-in, so every program that built still
  builds. Below C++17 the header stops at one `#error`, `eolymp.h needs C++17 or later: build
  with -std=c++17`, instead of hundreds of errors from inside it. A program that built
  before builds the same code.
- checker.md gains **recipes**: half marks for the right value with a bad certificate, a real
  optimum within a tolerance, and a tolerance on one side only. README.md and testlib.md list
  the names `eo` shares with `std`, and say `using namespace eo;` is outside the compatibility
  promise.

### Warnings

- **EO214 (new)**: `c.optimum` or `eo::compare` compared two reals with `==`; give it
  `eo::within(eps)`. The verdict is the one `==` gives, as before.
- **EO106 no longer fires on a bound computed from the input.** A bound within one of a value
  already read under a name, `read_int(1, n - 1, "m")` or `read_int(1, q, "x")`, is taken as
  computed; a literal such as `200001` is noted as before.
- **EO813 tries an extreme with every stored test's arguments** until the generator accepts
  it, and takes a refusal by `g.require` (exit 4) as "not with these options" rather than as a
  failure. A generator whose options depend on each other, `-m` no more than `-n`, is no
  longer blamed for extremes that other tests' options allow.
- **EO302 leaves group 0 out.** The examples' row, which validator.md tells authors to list and
  which usually has the full limits, is no longer called a copy and paste of the last subtask.

### eo-judge

- **A validator gets the test on stdin as well as by its path**, as on the judge. A validator
  that reads stdin, which is every testlib validator, called every test invalid under
  eo-judge; an eolymp.h validator reads the path and sees no difference.

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
