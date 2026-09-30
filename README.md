# eolymp.h

One header that jury programs for the [Eolymp](https://www.eolymp.com) judge are written
against. Everything lives in `namespace eo`: no global names, no macros beyond the include
guard and the version, nothing that redefines part of the C library.

**This release supports every kind of jury program Eolymp runs**: validators, checkers,
interactors, controllers and generators, including problems that run in phases and problems
whose instances run side by side. A second, opt-in header holds the test shapes a generator
draws from, and `eo-judge` runs a whole problem the way the judge does, before it is
uploaded.

**It builds on Windows too**, with MSVC for x64 and x86, clang-cl and mingw-w64, at `/W4 /WX`
and `-Wall -Wextra -Werror`, and there it gives the verdicts, messages and exit codes it gives
on Linux: CI runs the same scenarios on both and fails on a single byte of difference. How,
and the few things that are Windows' own, are in [docs/README.md](docs/README.md#windows).

```cpp
#include <eolymp.h>

int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int n = v.read_int(1, 200000, "n");
    v.read_eoln();
    std::vector<int> a = v.read_ints(n, 1, 1000000000, "a");
    v.read_eoln();
    v.require(eo::all_distinct(a), "a");
}
```

Leave out the `read_eoln()` and the first test you run says so, naming the call and the line
of your source rather than blaming the test:

```
validator.cpp:6: line 1, a[1]: a line break follows n; read it with read_eoln()
```

## Where to go

| | |
| --- | --- |
| [docs/validator.md](docs/validator.md) | writing a validator: everything that is built, and nothing that is not |
| [docs/checker.md](docs/checker.md) | writing a checker: the three streams, scores as a fraction of the test, and the log |
| [docs/interactor.md](docs/interactor.md) | writing an interactor: sends that do not deadlock the pair, the summary the checker reads, and `run_count` phases |
| [docs/generator.md](docs/generator.md) | writing a generator: declared options, named random streams, and a writer that cannot leave a trailing space |
| [docs/controller.md](docs/controller.md) | writing a controller: spawning instances, channels, and the flush and large sends that keep them all moving |
| [docs/README.md](docs/README.md) | the repository: what the library gives you, how a problem gets the header, what each directory holds, and what every gate proves |
| [docs/warnings.md](docs/warnings.md) | every warning code, one self-contained row each |
| [docs/testlib.md](docs/testlib.md) | coming from testlib: each call beside its eolymp.h counterpart, and what behaves differently |

## Building

`eolymp.h` is generated from `src/` and committed, because it is the file the judge's C++
runtime ships.
Edit `src/`, then:

```bash
make
```

```bash
make check
```

`make check` is the whole C++ gate, and CI runs it on GCC, clang, musl and macOS beside
`make judge`, `make transcript` and its comparison on Windows, `make mutants`,
`make sanitize`, `make fuzz` and, on a pull request, `make version`. What each part proves is in
[docs/README.md](docs/README.md#building-and-testing).

## Where to read

| You want | Read |
| --- | --- |
| to write a jury program | [docs/](docs/) — one guide per kind |
| to know what a warning code means | [docs/warnings.md](docs/warnings.md), one row per code |
| to move a problem from testlib | [docs/testlib.md](docs/testlib.md) |
| to know what changed in a release | [CHANGELOG.md](CHANGELOG.md), every verdict change listed |
| to run a whole problem before uploading it | [docs/judge.md](docs/judge.md) |
| to change this repository | [AGENTS.md](AGENTS.md) |

MIT, in [LICENSE](LICENSE).
