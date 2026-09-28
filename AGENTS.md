# Working in this repository

`eolymp.h` is a C++17 header that jury programs for the [Eolymp](https://www.eolymp.com)
judge are written against — validators, checkers, interactors, controllers and generators.
Everything lives in `namespace eo`. There are three things to know before changing anything.

## 1. The headers are generated

`eolymp.h` and `eolymp-shapes.h` are **build output**, committed because the judge's C++
runtime ships these exact bytes. Edit `src/`, then run `make`. The `amalgamation-check` part
of the gate fails if the committed headers are not what `src/` generates, so a change made
directly in `eolymp.h` is lost and breaks the build.

| Path | Holds |
| --- | --- |
| `src/*.h` | the library, one file per layer; `core.h` carries `EOLYMP_H_VERSION` |
| `src/shapes/*.h` | the opt-in test shapes |
| `judge/*.go` | `eo-judge`, the emulator; its own Go module, standard library only |
| `tests/` | one translation unit, `tests/all.cpp`, including `tests/*.inc`; plus the e2e and hostile suites |
| `tests/fuzz/` | libFuzzer harnesses, one property each; `make fuzz` builds and runs them with clang++ |
| `tests/live/` | two whole problems; the `eo-judge` tests run them end to end as fixtures |
| `tools/` | the amalgamator and every gate |
| `docs/` | the guides; `docs/warnings.md` is every warning code |

## 2. `make check` is the whole gate

```bash
make check      # the C++ gate: 9 parts, what CI runs on four toolchains
make judge      # gofmt, go vet and the eo-judge tests
make mutants    # a changed operator or bound must make the suite fail
make sanitize   # the suite and the end-to-end programs under ASan and UBSan
make fuzz       # six libFuzzer harnesses, 45 s each; needs clang++
make version    # changed headers or eo-judge need a raised version
```

CI runs all six: `make check` on g++, clang++, musl and macOS, `make judge`,
`make mutants` and `make sanitize` once each, `make fuzz` for 45 s a harness on every push
and pull request and for 30 minutes a harness every night, and `make version` on every pull
request.

Two parts fail for reasons worth knowing in advance:

- **`coverage` demands that every line of both headers runs at least once.** A new branch
  needs a test in the same change. The gate needs GNU `gcov`; under LLVM's emulation it
  reports lines it cannot see and says so rather than failing.
- **`codes` demands that every warning code the sources raise has a row in
  `docs/warnings.md`.** A new code without a row fails the build.

Run `make check` on its own and read its exit code. Piping it into `grep` inside an `&&`
chain returns grep's status, which has hidden a real failure here before.

## 3. The judge's behaviour is read from the judge, not from the design

The platform decides what the library has to do, and it is the only authority. The sources
to read are:

| Question | Read |
| --- | --- |
| how a checker's exit code and log become a score | `agent` `internal/judge/checker/program.go` |
| how an interactor's exit code becomes a verdict | `agent` `internal/judge/runner/script.go` |
| when a run is blocked, skipped or stopped | `agent` `internal/judge/admissioner/` |
| how runs add up into a group and a submission | `atlas` `internal/services/submissions/submission_reporter.go` |
| the wire format of anything | `contracts` `src/eolymp/**` |

Those checkouts lag `origin/main` by a long way, so read `git show origin/main:<path>` rather
than the working tree. `eo-judge` copies `readPoints` from the agent verbatim and a test
diffs the copy against `origin/main`. **CI cannot run that test** — it has no checkout of the
private agent repository, so there the test skips. Run `make pin` on a machine that has one;
with `AGENT_REPO` set the test fails instead of skipping.

## Releasing

`EOLYMP_H_VERSION` in `src/core.h` is where the version is written, with
`EOLYMP_H_VERSION_MAJOR`, `_MINOR` and `_PATCH` below it; a `static_assert` fails every build
while the three numbers disagree with the string. `version` in `judge/main.go` is the same
number for eo-judge, and `make version` and an eo-judge test fail while it differs; the same test
holds the `version` default in `action.yml`, the eo-judge release the GitHub Action downloads,
to it too. Change all six, run `make`, and merging to `main` publishes the release: once every other `check` job
has passed on that commit, the `release` job reads the version, refuses to publish headers
that are not what `src/` generates, and creates the tag `v<version>` on that commit with both
headers attached, then `judge/v<version>` with eo-judge's binaries. It does nothing for a tag
that already exists, so an ordinary merge is a no-op. Add a `## <version>` section to
[CHANGELOG.md](CHANGELOG.md) in the same change, listing every verdict it changes; the
release uses that section as its notes.

A release is what the judge's C++ runtime pins to, so the version has to move in the same
change as the behaviour, and `make version` — which CI runs on every pull request — fails a
change to the headers or to eo-judge that leaves the version where it was. Semantic
versioning, and the promise in
[docs/README.md](docs/README.md#versions): nothing that changes a verdict changes within a
major version.

## Looking a warning code up

Every code has one self-contained row in [docs/warnings.md](docs/warnings.md):

```bash
grep EO807 docs/warnings.md
```

The row says what fired it, who reported it, and what to do. All 77 codes are built.

## Using the library on a problem

Both headers are in the judge's C++ runtime, in `/usr/include/`, so a program includes them
and carries nothing in `files`. The runtime image is the version: it holds one release of
each, and rebuilding it moves every program on the judge to what it then carries. The image
is built from [eolymp/runtime-cpp](https://github.com/eolymp/runtime-cpp), which keeps its
copy of both headers under `include/`; a release reaches problems when that copy is updated
and the runtime is rebuilt.

Before uploading, run the problem locally:

```bash
eo-judge run   <problem>   # the score every solution would get
eo-judge check <problem>   # what only the whole problem shows
```

[docs/judge.md](docs/judge.md) has the `problem.json` format.

## House style

- **No comments.** Say it in the naming and the structure. The exceptions are a lint
  suppression, which states its reason, and a `TODO` that was asked for.
- **Documentation is written when it is asked for**, by name. A stale page is reported in the
  change description and left alone.
- Messages are sentences an author can act on: what is wrong, the values involved, and one
  line saying what to do instead.
