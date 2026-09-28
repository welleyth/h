package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

var hostileClients = map[string]string{
	"a client that exits at once": `int main() { return 0; }`,
	"a client that prints garbage": `
#include <cstdio>
int main() { for (int at = 0; at < 100; at++) { std::printf("@@@ not a command\n"); std::fflush(stdout); } return 0; }`,
	"a client that goes silent": `
#include <unistd.h>
int main() { ::sleep(30); return 0; }`,
	"a client that floods the pipe": `
#include <cstdio>
int main() { for (;;) { std::printf("? 1\n"); std::fflush(stdout); } }`,
}

func (w *Workspace) interactiveChecks(ctx context.Context, found *Findings) error {
	if !w.Problem.Interactive() || w.Problem.Interactor == nil {
		return nil
	}
	interactor, err := w.Build(ctx, "interactor", w.Problem.Interactor)
	if err != nil {
		return err
	}

	made := w.sorted()
	if len(made) == 0 {
		return nil
	}
	first := made[0]
	limit, _ := w.Problem.Testset(first.Group).Limit(w.Problem)

	for name, body := range hostileClients {
		dir := filepath.Join(w.Dir, "hostile", keyOf(name))
		if err := os.MkdirAll(dir, 0o755); err != nil {
			return err
		}
		source := filepath.Join(dir, "source.cpp")
		if err := os.WriteFile(source, []byte(body), 0o644); err != nil {
			return err
		}
		client, err := w.tools.build(ctx, &Problem{dir: dir}, "client", &Program{Source: "source.cpp"}, dir)
		if err != nil {
			return err
		}

		work := filepath.Join(dir, "run")
		if err := os.MkdirAll(work, 0o755); err != nil {
			return err
		}
		status, jury, err := w.onePhase(ctx, first.Input, filepath.Join(work, "summary.txt"),
			client, interactor, work, limit, w.metadata(nil), "")
		if err != nil {
			return err
		}
		_ = status

		said, _ := os.ReadFile(filepath.Join(work, "interactor.log"))
		if jury.ExitCode >= 3 {
			found.warn("EO814", "interactor",
				fmt.Sprintf("%s ends the run with %q", name, firstLine(string(said))),
				"a badly behaved client must give a wrong answer or a time limit, never an interaction failure")
		}
	}
	return nil
}

func (w *Workspace) solutionChecks(ctx context.Context, found *Findings) error {
	if len(w.Problem.Solutions) == 0 {
		found.note("EO821", "", "the problem has no attached solutions",
			"a reference no stress run can cross-check")
		return nil
	}

	correct := 0
	for _, one := range w.Problem.Solutions {
		if one.Type == "CORRECT" {
			correct++
		}
	}
	if correct < 2 {
		found.note("EO821", "", fmt.Sprintf("the problem has %d correct solution(s)", correct),
			"a second correct solution is what a stress run compares the reference with")
	}

	passed := map[int]bool{}
	failed := map[int]bool{}

	for _, one := range w.Problem.Judged("") {
		attempt, err := w.Evaluate(ctx, one.Name, &Program{Source: one.Source})
		if err != nil {
			return err
		}

		for _, group := range attempt.Groups {
			if group.Verdict == Skipped {
				continue
			}
			if group.Verdict == Accepted {
				passed[group.Index] = true
			} else {
				failed[group.Index] = true
			}
		}

		if one.Type == "CORRECT" {
			if want, known := one.Expected(); known && attempt.Score != Points(want) {
				found.warn("EO819", "solution "+one.Name,
					fmt.Sprintf("it is declared correct and scores %g, not %g", attempt.Score, want),
					"a reference that does not score full marks is the first thing to fix")
			}
			w.headroom(found, one, attempt)
		}

		twice, err := w.Evaluate(ctx, one.Name, &Program{Source: one.Source})
		if err != nil {
			return err
		}
		if twice.Verdict != attempt.Verdict || twice.Score != attempt.Score {
			found.warn("EO815", "solution "+one.Name,
				fmt.Sprintf("two runs gave %s at %g and %s at %g",
					attempt.Verdict, attempt.Score, twice.Verdict, twice.Score),
				"something in the problem uses the clock or unseeded randomness")
		}
	}

	for _, testset := range w.Problem.Testsets {
		if testset.Index == 0 {
			continue
		}
		where := fmt.Sprintf("testset %d", testset.Index)
		if !failed[testset.Index] {
			found.warn("EO819", where, "no attached solution fails this subtask",
				"a subtask every solution passes tests nothing; add one that should lose it")
		}
		if !passed[testset.Index] {
			found.warn("EO820", where, "no attached solution passes this subtask",
				"a subtask nobody can score; check the tests and the limits")
		}
	}
	return nil
}

func (w *Workspace) headroom(found *Findings, solution *Solution, attempt *Attempt) {
	for _, group := range attempt.Groups {
		testset := w.Problem.Testset(group.Index)
		if testset == nil {
			continue
		}
		limit, _ := testset.Limit(w.Problem)
		for _, one := range group.Runs {
			if one.Verdict == Skipped || limit == 0 {
				continue
			}
			if one.Wall*2 > limit {
				found.warn("EO816", "solution "+solution.Name,
					fmt.Sprintf("test %d:%d uses %d ms of the %d ms limit", one.Group, one.Index, one.Wall, limit),
					"a reference under half the limit survives a slower machine and a rejudge")
			}
		}
	}
}

func (w *Workspace) Check(ctx context.Context, deep bool) (Findings, error) {
	found := Configuration(w.Problem)

	if err := w.BuildAll(ctx, w.Problem.Judged("")); err != nil {
		return found, err
	}
	if err := w.Generate(ctx); err != nil {
		return found, err
	}
	if err := w.Validate(ctx, true); err != nil {
		return found, err
	}
	for _, made := range w.sorted() {
		if !made.Valid && w.Problem.Validator != nil {
			where := fmt.Sprintf("test %d:%d", made.Group, made.Test.Index)
			if made.Broken {
				found.warn("EO806", where,
					fmt.Sprintf("the validator could not run: %s", made.Why),
					"the validator is broken, so every test reads invalid — fix the validator, not the tests")
			} else {
				found.warn("EO806", where,
					fmt.Sprintf("the validator rejects it: %s", made.Why), "fix the test or the validator")
			}
		}
		found = append(found, w.findingsOf(made.Warnings)...)
	}

	if err := w.checkerChecks(ctx, &found, deep); err != nil {
		return found, err
	}
	if err := w.structureChecks(ctx, &found); err != nil {
		return found, err
	}
	if err := w.coverageChecks(ctx, &found); err != nil {
		return found, err
	}
	if err := w.generatorChecks(ctx, &found); err != nil {
		return found, err
	}
	if err := w.interactiveChecks(ctx, &found); err != nil {
		return found, err
	}
	if err := w.solutionChecks(ctx, &found); err != nil {
		return found, err
	}
	return found, nil
}

func (w *Workspace) findingsOf(warnings []Warning) Findings {
	var out Findings
	for _, one := range warnings {
		out = append(out, Finding{Code: one.Code, Severity: one.Severity, Where: w.named(one),
			Message: one.Message, Fix: "reported by " + one.Source})
	}
	return out
}

func (w *Workspace) named(one Warning) string {
	names := make([]string, 0, len(w.Programs))
	for name := range w.Programs {
		names = append(names, name)
	}
	sort.Strings(names)
	for _, name := range names {
		built := w.Programs[name]
		if rest, inside := strings.CutPrefix(one.At, built.Source); inside {
			if source := w.sourceOf(name); source != "" {
				return source + rest
			}
		}
		if rest, inside := strings.CutPrefix(one.At, filepath.Dir(built.Source)+string(os.PathSeparator)); inside {
			return filepath.Join(strings.TrimPrefix(built.Dir, w.Dir+string(os.PathSeparator)), rest)
		}
	}
	return strings.TrimPrefix(one.At, w.Dir+string(os.PathSeparator))
}

func (w *Workspace) sourceOf(name string) string {
	if program, known := namedPrograms(w.Problem)[strings.Replace(name, ".", " ", 1)]; known {
		return program.Source
	}
	switch name {
	case "checker", "validator", "interactor":
		if program, known := namedPrograms(w.Problem)[name]; known {
			return program.Source
		}
	}
	return ""
}
