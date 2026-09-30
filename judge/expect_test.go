package main

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func relocated(t *testing.T, fixture string, change func(*Problem)) string {
	t.Helper()
	whole, err := filepath.Abs(fixture)
	if err != nil {
		t.Fatal(err)
	}
	problem, err := LoadProblem(whole)
	if err != nil {
		t.Fatal(err)
	}
	absolute := func(program *Program) {
		if program == nil {
			return
		}
		program.Source = problem.Path(program.Source)
		for at, one := range program.Files {
			program.Files[at] = problem.Path(one)
		}
	}
	for _, program := range []*Program{problem.Checker, problem.Validator, problem.Interactor} {
		absolute(program)
	}
	for _, script := range problem.Scripts {
		absolute(script)
	}
	for _, solution := range problem.Solutions {
		solution.Source = problem.Path(solution.Source)
	}
	for _, testset := range problem.Testsets {
		for _, test := range testset.Tests {
			for _, path := range []*string{&test.Input, &test.Answer} {
				if *path != "" {
					*path = problem.Path(*path)
				}
			}
		}
	}
	for _, test := range problem.ValidatorTests {
		if test.File != "" {
			test.File = problem.Path(test.File)
		}
	}
	change(problem)
	body, err := json.Marshal(problem)
	if err != nil {
		t.Fatal(err)
	}
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "problem.json"), body, 0o644); err != nil {
		t.Fatal(err)
	}
	return dir
}

func typed(types map[string]string, scores map[string]string) func(*Problem) {
	return func(problem *Problem) {
		for _, one := range problem.Solutions {
			if kind, known := types[one.Name]; known {
				one.Type = kind
			}
			one.Scores = scores[one.Name]
		}
	}
}

func TestExpectFailsASolutionThatBreaksItsType(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := relocated(t, "../tests/live/guess", typed(map[string]string{
		"binary": "CORRECT", "linear": "WRONG_ANSWER", "polite": "WRONG_ANSWER", "crasher": "INCORRECT",
	}, nil))
	code, out, errs := invokeIn(t.TempDir(), "run", dir, "--expect")
	if code != 1 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if !strings.Contains(out, "linear: RUNTIME_ERROR, 40\n") ||
		!strings.Contains(out, "  it is declared WRONG_ANSWER, but test 1:3 is RUNTIME_ERROR\n") ||
		!strings.HasSuffix(out, "eo-judge: 1 solution(s) break their declared type\n") {
		t.Errorf("printed %q", out)
	}
	if strings.Count(out, "it is declared") != 1 {
		t.Errorf("blamed more than linear: %q", out)
	}

	code, out, _ = invokeIn(t.TempDir(), "run", dir)
	if code != 0 || strings.Contains(out, "declared") {
		t.Errorf("without --expect: exit %d, printed %q", code, out)
	}

	code, out, _ = invokeIn(t.TempDir(), "run", dir, "--expect", "--json", "--solution", "linear")
	got := decoded(t, out)
	if code != 1 || got.Exit != 1 || got.Attempts[0].Breaks != "test 1:3 is RUNTIME_ERROR" {
		t.Errorf("exit %d, reported %+v", code, got)
	}
}

func TestExpectHoldsWhenEverySolutionKeepsItsType(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := relocated(t, "../tests/live/degrees", typed(map[string]string{
		"full": "CORRECT", "leaves": "WRONG_ANSWER", "zeros": "INCORRECT",
	}, map[string]string{"leaves": "55.767494"}))
	code, out, errs := invokeIn(t.TempDir(), "run", dir, "--expect")
	if code != 0 || strings.Contains(out, "declared") {
		t.Errorf("exit %d, printed %q, said %q", code, out, errs)
	}
}

func TestExpectIsForRunOnly(t *testing.T) {
	t.Parallel()
	for _, command := range []string{"check", "lint"} {
		code, _, errs := invoke(command, "testdata/broken", "--expect")
		if code != 2 || !strings.Contains(errs, "applies to run only") {
			t.Errorf("%s --expect exited %d, said %q", command, code, errs)
		}
	}
}

func attemptOf(verdict Verdict, score Points, runs ...Verdict) *Attempt {
	group := &GroupResult{Index: 1}
	for at, one := range runs {
		group.Runs = append(group.Runs, &RunResult{Group: 1, Index: at + 1, Verdict: one})
	}
	return &Attempt{Verdict: verdict, Score: score, Groups: []*GroupResult{group}}
}

func TestEachTypeAssertsWhatTheDocsSay(t *testing.T) {
	t.Parallel()
	ok, wa, part, tle, re, fail, skip := Accepted, WrongAnswer, Partial, TimeLimit, RuntimeFail, Failure, Skipped
	for _, one := range []struct {
		kind, scores string
		attempt      *Attempt
		breaks       string
	}{
		{"CORRECT", "", attemptOf(ok, 100, ok, ok), ""},
		{"CORRECT", "", attemptOf(part, 31.5, ok, part), "it ends PARTIALLY_CORRECT at 31.5, not ACCEPTED at 100"},
		{"CORRECT", "", attemptOf(wa, 100, ok, wa), "it ends WRONG_ANSWER at 100, not ACCEPTED at 100"},
		{"CORRECT", "60", attemptOf(part, 60, ok, part), ""},
		{"CORRECT", "60", attemptOf(ok, 100, ok, ok), "it scores 100, and its scores say 60"},
		{"INCORRECT", "", attemptOf(ok, 100, ok, ok), "every run was accepted"},
		{"INCORRECT", "", attemptOf(re, 50, ok, re), ""},
		{"WRONG_ANSWER", "", attemptOf(wa, 0, wa, skip), ""},
		{"WRONG_ANSWER", "", attemptOf(part, 10, ok, part), ""},
		{"WRONG_ANSWER", "", attemptOf(ok, 100, ok, ok), "no run is WRONG_ANSWER or PARTIALLY_CORRECT"},
		{"WRONG_ANSWER", "", attemptOf(tle, 0, wa, tle), "test 1:2 is TIME_LIMIT_EXCEEDED"},
		{"TIMEOUT", "", attemptOf(tle, 50, ok, tle, skip), ""},
		{"TIMEOUT", "", attemptOf(ok, 100, ok, ok), "no run is TIME_LIMIT_EXCEEDED"},
		{"TIMEOUT", "", attemptOf(wa, 0, tle, wa), "test 1:2 is WRONG_ANSWER"},
		{"TIMEOUT_OR_ACCEPTED", "", attemptOf(ok, 100, ok, ok), ""},
		{"TIMEOUT_OR_ACCEPTED", "", attemptOf(tle, 50, ok, tle), ""},
		{"TIMEOUT_OR_ACCEPTED", "", attemptOf(re, 50, ok, re), "test 1:2 is RUNTIME_ERROR"},
		{"FAILURE", "", attemptOf(fail, 0, ok, fail), ""},
		{"FAILURE", "", attemptOf(re, 0, re), "no run is FAILURE"},
		{"OVERFLOW", "", attemptOf(ok, 100, ok), ""},
		{"OVERFLOW_OR_ACCEPTED", "", attemptOf(re, 0, re), ""},
		{"DONT_RUN", "", attemptOf(re, 0, re), ""},
		{"", "", attemptOf(re, 0, re), ""},
		{"INCORRECT", "half", attemptOf(re, 0, re), `its scores are "half", which is not a number`},
	} {
		got := breaks(&Solution{Name: "s", Type: one.kind, Scores: one.scores}, one.attempt)
		if got != one.breaks {
			t.Errorf("%s with scores %q: %q, want %q", one.kind, one.scores, got, one.breaks)
		}
	}
}
