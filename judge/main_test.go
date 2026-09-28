package main

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func TestMain(m *testing.M) {
	if os.Getenv("EO_JUDGE_CACHE") != "" {
		os.Exit(m.Run())
	}
	dir, err := os.MkdirTemp("", "eo-judge-test-cache-")
	if err != nil {
		panic(err)
	}
	os.Setenv("EO_JUDGE_CACHE", dir)
	code := m.Run()
	os.RemoveAll(dir)
	os.Exit(code)
}

func invoke(args ...string) (int, string, string) {
	return invokeIn("", args...)
}

func invokeIn(temp string, args ...string) (int, string, string) {
	var out, errs bytes.Buffer
	code := realMain(args, temp, &out, &errs)
	return code, out.String(), errs.String()
}

func TestNoCommandPrintsTheUsage(t *testing.T) {
	t.Parallel()
	code, _, errs := invoke()
	if code != 2 || !strings.Contains(errs, "eo-judge run <problem>") {
		t.Errorf("exit %d, said %q", code, errs)
	}
	for _, problem := range []string{"testdata/broken", t.TempDir()} {
		code, _, errs = invoke("fly", problem)
		if code != 2 || !strings.HasPrefix(errs, `eo-judge: there is no command "fly"`) ||
			!strings.Contains(errs, "eo-judge run <problem>") {
			t.Errorf("an unknown command on %s exited %d, said %q", problem, code, errs)
		}
	}
	code, _, _ = invoke("run")
	if code != 2 {
		t.Errorf("a command without a problem exited %d", code)
	}
}

func TestAProblemThatCannotBeReadIsAnError(t *testing.T) {
	t.Parallel()
	code, _, errs := invoke("lint", t.TempDir())
	if code != 3 || !strings.Contains(errs, "problem.json") {
		t.Errorf("exit %d, said %q", code, errs)
	}
}

func TestLintReportsToTheGivenWriter(t *testing.T) {
	t.Parallel()
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
	t.Parallel()
	needsACompiler(t)
	temporary := t.TempDir()
	code, _, errs := invokeIn(temporary, "run", uncompilable(t))
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
	t.Parallel()
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
	t.Parallel()
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
	t.Parallel()
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
	t.Parallel()
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
	t.Parallel()
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
	t.Parallel()
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
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "run", "testdata/forged", "-v")
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
	t.Parallel()
	core, err := os.ReadFile("../src/core.h")
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(core), "#define EOLYMP_H_VERSION \""+version+"\"\n") {
		t.Errorf("eo-judge is %s, and src/core.h has another EOLYMP_H_VERSION; the two are one version", version)
	}
	action, err := os.ReadFile("../action.yml")
	if err != nil {
		t.Fatal(err)
	}
	downloaded := regexp.MustCompile(`(?m)^  version:\n(?:    .*\n)*?    default: (\S+)\n`).FindStringSubmatch(string(action))
	if downloaded == nil || downloaded[1] != version {
		t.Errorf("eo-judge is %s, and action.yml downloads %v by default; the action installs this release", version, downloaded)
	}
}

func TestACommunicationProblemIsRefusedBeforeAnythingRuns(t *testing.T) {
	t.Parallel()
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

func decoded(t *testing.T, printed string) outcome {
	t.Helper()
	var got outcome
	decoder := json.NewDecoder(strings.NewReader(printed))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&got); err != nil {
		t.Fatalf("printed %q, which is not the JSON report: %v", printed, err)
	}
	if got.Version != version {
		t.Errorf("the report is version %q", got.Version)
	}
	return got
}

func TestRunPrintsItsResultAsJSON(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "run", "../tests/live/guess", "--json")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	got := decoded(t, out)
	if got.Problem != "../tests/live/guess" || got.Exit != 0 || got.Error != "" || len(got.Attempts) != 4 {
		t.Fatalf("reported %+v", got)
	}
	linear := got.Attempts[1]
	if linear.Name != "linear" || linear.Verdict != RuntimeFail || linear.Score != 40 || len(linear.Groups) != 1 {
		t.Fatalf("linear is %+v", linear)
	}
	group := linear.Groups[0]
	if group.Index != 1 || group.Cost != 100 || group.Score != 40 || len(group.Runs) != 5 {
		t.Fatalf("its testset is %+v", group)
	}
	if run := group.Runs[2]; run.Test != 3 || run.Verdict != RuntimeFail || run.Message != "exit 1" {
		t.Errorf("its third run is %+v", run)
	}
	if got.Attempts[0].Type != "CORRECT" {
		t.Errorf("binary has the type %q", got.Attempts[0].Type)
	}
}

func TestCheckAndLintPrintTheirFindingsAsJSON(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, _ := invokeIn(t.TempDir(), "check", "testdata/broken", "--json")
	got := decoded(t, out)
	if code != 0 || got.Exit != 0 || len(got.Attempts) != 0 {
		t.Fatalf("exit %d, reported %+v", code, got)
	}
	found := map[string]bool{}
	for _, one := range got.Findings {
		found[one.Code+" "+one.Level+" "+one.Where] = true
	}
	for _, want := range []string{"EO801 warning test 1:2", "EO821 note ", "EO501 warning script gen"} {
		if !found[want] {
			t.Errorf("no %s in %+v", want, got.Findings)
		}
	}

	code, out, _ = invoke("lint", "--json", "--strict", "testdata/broken")
	got = decoded(t, out)
	if code != 1 || got.Exit != 1 || len(got.Findings) != 1 || got.Findings[0].Fix == "" {
		t.Errorf("a strict lint exited %d and reported %+v", code, got)
	}
}

func TestAnErrorIsInTheJSONReportToo(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "run", "--json", uncompilable(t))
	got := decoded(t, out)
	if code != 3 || got.Exit != 3 || !strings.Contains(got.Error, "checker does not compile") {
		t.Errorf("exit %d, reported %+v", code, got)
	}
	if !strings.Contains(errs, "checker does not compile") {
		t.Errorf("said %q", errs)
	}
	code, out, _ = invoke("lint", "--json", t.TempDir())
	if got := decoded(t, out); code != 3 || got.Exit != 3 || !strings.Contains(got.Error, "problem.json") {
		t.Errorf("a missing problem exited %d, reported %+v", code, got)
	}
}

func TestInitWritesAProblemThatPassesRunAndCheck(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	for _, kind := range kinds {
		t.Run(kind, func(t *testing.T) {
			t.Parallel()
			dir := filepath.Join(t.TempDir(), "new")
			code, out, errs := invoke("init", dir, "--type", kind)
			if code != 0 || !strings.Contains(out, "wrote "+article(kind)+" "+kind+" problem to "+dir) {
				t.Fatalf("exit %d, printed %q, said %q", code, out, errs)
			}
			code, out, errs = invokeIn(t.TempDir(), "run", "--expect", "--strict", dir)
			if code != 0 || strings.Contains(out, "declared") || !strings.Contains(out, ": ACCEPTED, 100\n") {
				t.Errorf("run exited %d, printed %q, said %q", code, out, errs)
			}
			code, out, errs = invokeIn(t.TempDir(), "check", "--strict", dir)
			if code != 0 || !strings.Contains(out, "eo-judge: 0 warning(s)") {
				t.Errorf("check exited %d, printed %q, said %q", code, out, errs)
			}
		})
	}
}

func TestInitWritesOnlyIntoANewOrEmptyDirectory(t *testing.T) {
	t.Parallel()
	dir := t.TempDir()
	if code, _, errs := invoke("init", dir); code != 0 {
		t.Fatalf("an empty directory: exit %d, said %q", code, errs)
	}
	code, out, errs := invoke("init", dir, "--type", "interactive")
	if code != 2 || out != "" || !strings.Contains(errs, "already holds checker.cpp") {
		t.Errorf("a second init exited %d, printed %q, said %q", code, out, errs)
	}
	code, _, errs = invoke("init", filepath.Join(dir, "other"), "--type", "batch")
	if code != 2 || !strings.Contains(errs, `--type is "batch"; it is one of program, interactive, phases`) {
		t.Errorf("an unknown type exited %d, said %q", code, errs)
	}
	if code, _, _ := invoke("init"); code != 2 {
		t.Errorf("init with no directory exited %d", code)
	}
	code, out, errs = invoke("init", filepath.Join(dir, "other"), "--json")
	if code != 2 || out != "" || errs != "eo-judge: --json applies to run, check and lint; init prints only the files it wrote\n" {
		t.Errorf("init --json exited %d, printed %q, said %q", code, out, errs)
	}
	if code, out, _ := invoke("init", filepath.Join(dir, "third"), "--type", "interactive"); code != 0 ||
		!strings.Contains(out, "wrote an interactive problem") {
		t.Errorf("exit %d, printed %q", code, out)
	}
}
