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
	for _, args := range [][]string{{"version", "--json"}, {"--version", "-json"}} {
		code, out, _ := invoke(args...)
		var said struct{ Version string }
		if code != 0 || json.Unmarshal([]byte(out), &said) != nil || said.Version != version {
			t.Errorf("%v exited %d, printed %q", args, code, out)
		}
	}
	for _, args := range [][]string{{"version", "--strict"}, {"--version", "--json", "extra"}} {
		code, out, errs := invoke(args...)
		if code != 2 || out != "" || !strings.HasPrefix(errs, "eo-judge: version takes only --json, not ") {
			t.Errorf("%v exited %d, printed %q, said %q", args, code, out, errs)
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
	if err := os.WriteFile(filepath.Join(dir, "controller.cpp"), []byte("int main() { printf(\"x\"); }\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	for _, command := range []string{"run", "check"} {
		code, out, errs := invoke(command, dir)
		if code != 3 || out != "" || !strings.Contains(errs, "does not run COMMUNICATION problems yet") {
			t.Errorf("%s exited %d, printed %q, said %q", command, code, out, errs)
		}
	}
	code, out, errs := invoke("lint", dir)
	if code != 0 || !strings.Contains(out, "interactor: warning EO401") {
		t.Errorf("lint exited %d, printed %q, said %q", code, out, errs)
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
	if run := group.Runs[2]; run.Test != 3 || run.Verdict != RuntimeFail ||
		run.Message != "exit 1; the interactor said: wrong answer more than 20 queries" {
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
	if code != 2 || out != "" || errs != "eo-judge: --json applies to run, check, lint and stress; init prints only the files it wrote\n" {
		t.Errorf("init --json exited %d, printed %q, said %q", code, out, errs)
	}
	if code, out, _ := invoke("init", filepath.Join(dir, "third"), "--type", "interactive"); code != 0 ||
		!strings.Contains(out, "wrote an interactive problem") {
		t.Errorf("exit %d, printed %q", code, out)
	}
}

func TestVerboseShowsTheInteractorNextToACrash(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "run", "../tests/live/guess", "--solution", "crasher", "-v")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if !regexp.MustCompile(`\n    1:2 RUNTIME_ERROR \d+ms exit 3; the interactor said: ok 9 queries\n`).MatchString(out) {
		t.Errorf("printed %q", out)
	}
}

func TestARelativeWorkspaceIsWhereTheProgramsFindTheirFiles(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	here, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	relative, err := filepath.Rel(here, filepath.Join(t.TempDir(), "work"))
	if err != nil {
		t.Fatal(err)
	}
	code, out, errs := invoke("run", "testdata/authored", "--work", relative)
	if code != 0 || strings.Contains(out, "invalid") || !strings.Contains(out, "sum: ACCEPTED, 100") {
		t.Errorf("run --work %s exited %d, printed %q, said %q", relative, code, out, errs)
	}
	code, out, errs = invoke("stress", "testdata/stress", "--args", "-n=[3..8] -max=[1..100]", "--solution", "pairs",
		"--work", relative)
	if code != 1 || !strings.Contains(out, "iteration 1: COUNTEREXAMPLE") ||
		!strings.Contains(out, "kept in "+filepath.Join(relative, "stress", "1")+": ") {
		t.Errorf("stress --work %s exited %d, printed %q, said %q", relative, code, out, errs)
	}
}

func TestInitWritesATestForTheValidatorAndOneForTheChecker(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	for _, kind := range kinds {
		t.Run(kind, func(t *testing.T) {
			t.Parallel()
			dir := filepath.Join(t.TempDir(), "new")
			if code, _, errs := invoke("init", dir, "--type", kind); code != 0 {
				t.Fatalf("exit %d, said %q", code, errs)
			}
			problem, err := LoadProblem(dir)
			if err != nil {
				t.Fatal(err)
			}
			if len(problem.ValidatorTests) != 1 || len(problem.CheckerTests) != 1 {
				t.Fatalf("%d validator and %d checker tests", len(problem.ValidatorTests), len(problem.CheckerTests))
			}
			flipped := relocated(t, dir, func(problem *Problem) {
				problem.ValidatorTests[0].Expect = "VALID"
				problem.CheckerTests[0].Expect = CheckerExpect{Verdict: "FAILURE"}
			})
			said := map[string]string{}
			for _, code := range []string{"EO911", "EO912"} {
				for where, message := range authoredFindings(t, flipped, code) {
					said[where] = message
				}
			}
			if !strings.HasPrefix(said["validator test 1"], "it is expected VALID, and the validator refuses it: ") ||
				!strings.HasPrefix(said["checker test 1"], "it is expected FAILURE, and the checker gives ") {
				t.Errorf("the flipped tests found %v", said)
			}
		})
	}
}

func problemWithADirectory(t *testing.T, name string) string {
	t.Helper()
	dir := relocated(t, "testdata/stress", func(*Problem) {})
	if err := os.MkdirAll(filepath.Join(dir, name), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, name, "keep.txt"), []byte("the author's\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	return dir
}

func TestAWorkspaceThatOverlapsTheProblemIsRefused(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	for command, name := range map[string]string{"run": "tests", "check": "tests", "stress": "stress"} {
		dir := problemWithADirectory(t, name)
		link := filepath.Join(t.TempDir(), "link")
		if err := os.Symlink(dir, link); err != nil {
			t.Fatal(err)
		}
		for work, want := range map[string]string{
			dir:                           "is the problem's own directory",
			link:                          "is the problem's own directory",
			filepath.Dir(dir):             "holds the problem " + dir,
			filepath.Join(dir, "w"):       "is inside the problem " + dir,
			filepath.Join(link, "a", "b"): "is inside the problem " + dir,
		} {
			args := []string{command, dir, "--work", work}
			if command == "stress" {
				args = append(args, "--args", "-n=[1..8] -max=[1..100]", "--solution", "twin", "--iterations", "1")
			}
			code, out, errs := invokeIn(t.TempDir(), args...)
			if code != 2 || out != "" || !strings.Contains(errs, "eo-judge: --work "+work+" "+want+
				"; eo-judge clears the directories it makes in its workspace, so give it a directory outside the problem") {
				t.Errorf("%s --work %s exited %d, printed %q, said %q", command, work, code, out, errs)
			}
		}
		if _, err := os.Stat(filepath.Join(dir, name, "keep.txt")); err != nil {
			t.Errorf("%s: %v", command, err)
		}
	}
	code, _, errs := invokeIn(t.TempDir(), "run", "testdata/authored", "--work", filepath.Join(t.TempDir(), "authored"))
	if code != 0 {
		t.Errorf("a workspace beside the problem exited %d, said %q", code, errs)
	}
}
