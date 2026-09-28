package main

import (
	"bytes"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func invoke(args ...string) (int, string, string) {
	var out, errs bytes.Buffer
	code := realMain(args, &out, &errs)
	return code, out.String(), errs.String()
}

func TestNoCommandPrintsTheUsage(t *testing.T) {
	code, _, errs := invoke()
	if code != 2 || !strings.Contains(errs, "eo-judge run <problem>") {
		t.Errorf("exit %d, said %q", code, errs)
	}
	code, _, errs = invoke("fly", "testdata/broken")
	if code != 2 || !strings.Contains(errs, "eo-judge run <problem>") {
		t.Errorf("an unknown command exited %d, said %q", code, errs)
	}
	code, _, _ = invoke("run")
	if code != 2 {
		t.Errorf("a command without a problem exited %d", code)
	}
}

func TestAProblemThatCannotBeReadIsAnError(t *testing.T) {
	code, _, errs := invoke("lint", t.TempDir())
	if code != 3 || !strings.Contains(errs, "problem.json") {
		t.Errorf("exit %d, said %q", code, errs)
	}
}

func TestLintReportsToTheGivenWriter(t *testing.T) {
	code, out, _ := invoke("lint", "testdata/broken")
	if code != 0 || !strings.Contains(out, "eo-judge:") {
		t.Errorf("exit %d, printed %q", code, out)
	}
	code, _, _ = invoke("lint", "--strict", "testdata/broken")
	if code != 1 {
		t.Errorf("a strict lint with warnings exited %d", code)
	}
}

func uncompilable(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	write := func(name, body string) {
		if err := os.WriteFile(filepath.Join(dir, name), []byte(body), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("problem.json", `{"type": "PROGRAM", "checker": {"source": "checker.cpp"}}`)
	write("checker.cpp", "this is not C++\n")
	return dir
}

func TestTheWorkspaceIsRemovedWhenARunFails(t *testing.T) {
	needsACompiler(t)
	temporary := t.TempDir()
	t.Setenv("TMPDIR", temporary)
	code, _, errs := invoke("run", uncompilable(t))
	if code != 3 || !strings.Contains(errs, "does not compile") {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	left, err := os.ReadDir(temporary)
	if err != nil {
		t.Fatal(err)
	}
	if len(left) != 0 {
		t.Errorf("the run left %d entries in the temporary directory, such as %s", len(left), left[0].Name())
	}
}

func TestAWorkspaceThatWasAskedForIsKept(t *testing.T) {
	needsACompiler(t)
	kept := filepath.Join(t.TempDir(), "work")
	code, _, _ := invoke("check", "--work", kept, uncompilable(t))
	if code != 3 {
		t.Fatalf("exit %d", code)
	}
	if _, err := os.Stat(filepath.Join(kept, "checker")); err != nil {
		t.Errorf("the workspace was not kept: %v", err)
	}
}

func TestVersionAndHelpExitZero(t *testing.T) {
	for _, args := range [][]string{{"version"}, {"--version"}} {
		code, out, _ := invoke(args...)
		if code != 0 || out != "eo-judge "+version+"\n" {
			t.Errorf("%v exited %d, printed %q", args, code, out)
		}
	}
	for _, args := range [][]string{{"help"}, {"-h"}, {"--help"}, {"run", "-h"}, {"lint", "--help"}} {
		code, out, _ := invoke(args...)
		if code != 0 || !strings.Contains(out, "eo-judge run <problem>") {
			t.Errorf("%v exited %d, printed %q", args, code, out)
		}
	}
	code, _, errs := invoke("run", "--no-such-flag", "testdata/broken")
	if code != 2 || !strings.Contains(errs, "no-such-flag") {
		t.Errorf("an unknown flag exited %d, said %q", code, errs)
	}
}

func TestFlagsMayFollowTheProblem(t *testing.T) {
	code, _, _ := invoke("lint", "testdata/broken", "--strict")
	if code != 1 {
		t.Errorf("--strict after the problem was ignored: exit %d", code)
	}
	code, _, _ = invoke("lint", "testdata/broken", "testdata/answers")
	if code != 2 {
		t.Errorf("two problems exited %d", code)
	}
}

func TestAnUnknownSolutionIsAUsageError(t *testing.T) {
	for _, command := range []string{"run", "check", "lint"} {
		code, out, errs := invoke(command, "../tests/live/guess", "--solution", "nosuch")
		if code != 2 || out != "" {
			t.Errorf("%s: exit %d, printed %q", command, code, out)
		}
		if !strings.Contains(errs, `no solution called "nosuch"; it has binary, linear, polite, crasher`) {
			t.Errorf("%s: said %q", command, errs)
		}
	}
}

func TestOnlyTheSolutionNamedAfterTheProblemIsJudged(t *testing.T) {
	needsACompiler(t)
	code, out, errs := invoke("run", "../tests/live/guess", "--solution", "binary")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if !strings.Contains(out, "binary: ACCEPTED, 100") || strings.Contains(out, "linear:") {
		t.Errorf("printed %q", out)
	}
}

func TestADontRunSolutionIsNotEvenBuilt(t *testing.T) {
	needsACompiler(t)
	dir := t.TempDir()
	write := func(name, body string) {
		if err := os.WriteFile(filepath.Join(dir, name), []byte(body), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("problem.json", `{"type": "PROGRAM", "solutions": [{"name": "parked", "source": "parked.cpp", "type": "DONT_RUN"}]}`)
	write("parked.cpp", "this is not C++\n")
	for _, command := range []string{"run", "check"} {
		if code, _, errs := invoke(command, dir); code != 0 {
			t.Errorf("%s exited %d, said %q", command, code, errs)
		}
	}
	if code, _, errs := invoke("run", dir, "--solution", "parked"); code != 3 || !strings.Contains(errs, "does not compile") {
		t.Errorf("naming it exited %d, said %q", code, errs)
	}
}

func TestVerboseListsEveryRun(t *testing.T) {
	needsACompiler(t)
	t.Setenv("TMPDIR", t.TempDir())
	code, out, errs := invoke("run", "testdata/forged", "-v")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if !regexp.MustCompile(`\n    1:1 PARTIALLY_CORRECT \d+ms points 1 one point for answering\n`).MatchString(out) {
		t.Errorf("printed %q", out)
	}
	_, quiet, _ := invoke("run", "testdata/forged")
	if strings.Contains(quiet, "    1:1 ") {
		t.Errorf("a run without -v listed its runs: %q", quiet)
	}
}

func TestEoJudgeCarriesTheRepositorysVersion(t *testing.T) {
	core, err := os.ReadFile("../src/core.h")
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(core), "#define EOLYMP_H_VERSION \""+version+"\"\n") {
		t.Errorf("eo-judge is %s, and src/core.h has another EOLYMP_H_VERSION; the two are one version", version)
	}
}

func TestACommunicationProblemIsRefusedBeforeAnythingRuns(t *testing.T) {
	dir := t.TempDir()
	body := `{"type": "COMMUNICATION", "interactor": {"source": "controller.cpp"},
		"solutions": [{"name": "full", "source": "full.cpp"}]}`
	if err := os.WriteFile(filepath.Join(dir, "problem.json"), []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	for _, command := range []string{"run", "check", "lint"} {
		code, out, errs := invoke(command, dir)
		if code != 3 || out != "" || !strings.Contains(errs, "does not run COMMUNICATION problems yet") {
			t.Errorf("%s exited %d, printed %q, said %q", command, code, out, errs)
		}
	}
}
