package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"testing"
)

func TestARangeBecomesANumberInItAndASeedIsAppended(t *testing.T) {
	t.Parallel()
	pattern, err := parseArguments(strings.Fields("-n=[1..8] -x=[-3..-1],[5..5] -name=[a..b] -m=7"))
	if err != nil {
		t.Fatal(err)
	}
	seed := regexp.MustCompile(`^[0-9a-f]{16}$`)
	shape := regexp.MustCompile(`^-x=(-[0-9]+),5$`)
	seen := map[string]bool{}
	for range 200 {
		args := pattern.resolve()
		if len(args) != 5 || args[2] != "-name=[a..b]" || args[3] != "-m=7" || !seed.MatchString(args[4]) {
			t.Fatalf("resolved to %q", args)
		}
		n, err := strconv.Atoi(strings.TrimPrefix(args[0], "-n="))
		if err != nil || n < 1 || n > 8 {
			t.Fatalf("resolved -n=[1..8] to %q", args[0])
		}
		x := shape.FindStringSubmatch(args[1])
		if x == nil {
			t.Fatalf("resolved -x=[-3..-1],[5..5] to %q", args[1])
		}
		if low, _ := strconv.Atoi(x[1]); low < -3 || low > -1 {
			t.Fatalf("resolved -x=[-3..-1],[5..5] to %q", args[1])
		}
		seen[args[0]] = true
		seen[args[4]] = true
	}
	if len(seen) < 8+150 {
		t.Errorf("200 draws gave only %d distinct values of -n and seeds", len(seen))
	}
}

func TestARangeThatCannotBeDrawnFromIsRefused(t *testing.T) {
	t.Parallel()
	for args, want := range map[string]string{
		"-n=[8..1]":                               "[8..1] is empty; write the smaller end first, as [1..8]",
		"-n=[1..99999999999999999999]":            "[1..99999999999999999999] does not fit in 64 bits",
		"-n=[1..3] -m=[-99999999999999999999..0]": "[-99999999999999999999..0] does not fit in 64 bits",
	} {
		if _, err := parseArguments(strings.Fields(args)); err == nil || err.Error() != want {
			t.Errorf("%q: %v, not %q", args, err, want)
		}
	}
	pattern, err := parseArguments(strings.Fields("-n=[-9223372036854775808..9223372036854775807]"))
	if err != nil {
		t.Fatal(err)
	}
	if args := pattern.resolve(); !regexp.MustCompile(`^-n=-?[0-9]+$`).MatchString(args[0]) {
		t.Errorf("the whole 64-bit range resolved to %q", args)
	}
}

func stressing(t *testing.T, args ...string) (int, string, string) {
	t.Helper()
	needsACompiler(t)
	return invokeIn(t.TempDir(), append([]string{"stress", "testdata/stress"}, args...)...)
}

var resolved = regexp.MustCompile(`\n  "generator": \{"script": "gen", "arguments": \["-n=([0-9]+)", "-max=([0-9]+)", "[0-9a-f]{16}"\]\}\n`)

func TestStressStopsAtACorrectSolutionThatIsWrong(t *testing.T) {
	t.Parallel()
	work := filepath.Join(t.TempDir(), "work")
	code, out, errs := stressing(t, "--gen", "gen", "--args", "-n=[1..8] -max=[1..100]", "--iterations", "40",
		"--solution", "pairs", "--work", work)
	if code != 1 {
		t.Fatalf("exit %d, printed %q, said %q", code, out, errs)
	}
	at := regexp.MustCompile(`\niteration ([0-9]+): COUNTEREXAMPLE\n  pairs: WRONG_ANSWER [0-9]+ms, which breaks its type CORRECT: wrong answer the sum is [0-9]+, not [0-9]+\n`).FindStringSubmatch(out)
	args := resolved.FindStringSubmatch(out)
	if at == nil || args == nil {
		t.Fatalf("printed %q", out)
	}
	if n, _ := strconv.Atoi(args[1]); n < 3 {
		t.Errorf("-n=%d cannot tell a sum of two from a sum of all: %q", n, out)
	}
	kept := filepath.Join(work, "stress", at[1])
	if !strings.Contains(out, "\n  kept in "+kept+": input.txt, answer.txt, pairs/output.txt\n") {
		t.Errorf("printed %q", out)
	}
	input, err := os.ReadFile(filepath.Join(kept, "input.txt"))
	if err != nil || !strings.HasPrefix(string(input), args[1]+"\n") {
		t.Errorf("the kept input is %q (%v), for -n=%s", input, err, args[1])
	}
	for _, name := range []string{"answer.txt", "pairs/output.txt"} {
		if body, err := os.ReadFile(filepath.Join(kept, name)); err != nil || len(body) == 0 {
			t.Errorf("%s is %q (%v)", name, body, err)
		}
	}
	if !strings.HasSuffix(out, "eo-judge: iteration "+at[1]+" of 40 is a counterexample\n") {
		t.Errorf("printed %q", out)
	}
	left, err := os.ReadDir(filepath.Join(work, "stress"))
	if err != nil || len(left) != 1 {
		t.Errorf("the workspace keeps %d iterations (%v); only the counterexample stays", len(left), err)
	}

	var pasted struct{ Generator Generator }
	line := strings.TrimSpace(resolved.FindString(out))
	if err := json.Unmarshal([]byte("{"+line+"}"), &pasted); err != nil {
		t.Fatalf("%q does not paste into problem.json: %v", line, err)
	}
	shop := workshop(t, "testdata/stress")
	gen, err := shop.script(context.Background(), pasted.Generator.Script)
	if err != nil {
		t.Fatal(err)
	}
	again, err := shop.generateOnce(context.Background(), gen, pasted.Generator.Arguments)
	if err != nil || string(again) != string(input) {
		t.Errorf("the pasted arguments generate %q (%v), and the stress kept %q", again, err, input)
	}

	code, out, errs = stressing(t, "--args", "-n=[3..8] -max=[1..100]", "--solution", "pairs", "--work", work)
	if code != 1 || !strings.Contains(out, "\n  kept in "+filepath.Join(work, "stress", "1")+": ") {
		t.Errorf("a second stress in the same workspace exited %d, printed %q, said %q", code, out, errs)
	}
}

func TestStressPassesSolutionsThatKeepTheirTypes(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[1..8] -max=[1..100]", "--iterations", "25",
		"--solution", "twin", "--solution", "first", "-v")
	if code != 0 {
		t.Fatalf("exit %d, printed %q, said %q", code, out, errs)
	}
	if !strings.HasPrefix(out, "stress: gen -n=[1..8] -max=[1..100] against brute, comparing twin, first; "+
		"at most 25 iterations in 300 s\n") {
		t.Errorf("printed %q", out)
	}
	if lines := regexp.MustCompile(`(?m)^  [0-9]+ PASSED gen -n=[0-9]+ -max=[0-9]+ [0-9a-f]{16}$`).FindAllString(out, -1); len(lines) != 25 {
		t.Errorf("-v listed %d iterations: %q", len(lines), out)
	}
	if !strings.HasSuffix(out, "eo-judge: 25 iterations passed, with no counterexample\n") {
		t.Errorf("printed %q", out)
	}
}

func TestStressComparesEverySolutionButTheReferenceByDefault(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[5..8] -max=[1..100]", "--iterations", "5")
	if code != 1 {
		t.Fatalf("exit %d, printed %q, said %q", code, out, errs)
	}
	if !strings.HasPrefix(out, "stress: gen -n=[5..8] -max=[1..100] against brute, comparing twin, pairs, first, "+
		"crashing, untyped, patient; at most 5 iterations in 300 s\n") {
		t.Errorf("printed %q", out)
	}
	for _, want := range []string{
		`\niteration 1: COUNTEREXAMPLE\n`,
		`\n  twin: ACCEPTED [0-9]+ms: ok the sum is [0-9]+, test 1 of group 0\n`,
		`\n  pairs: WRONG_ANSWER [0-9]+ms, which breaks its type CORRECT: `,
		`\n  first: WRONG_ANSWER [0-9]+ms: wrong answer the sum is `,
		`\n  crashing: RUNTIME_ERROR [0-9]+ms, which breaks its type WRONG_ANSWER: exit 3\n`,
		`\n  untyped: WRONG_ANSWER [0-9]+ms: `,
	} {
		if !regexp.MustCompile(want).MatchString(out) {
			t.Errorf("no %s in %q", want, out)
		}
	}
	if strings.Contains(out, "kept in") {
		t.Errorf("a stress without --work says it kept files: %q", out)
	}
}

func TestStressPrintsItsResultAsJSON(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[3..8] -max=[1..100]", "--iterations", "3",
		"--solution", "pairs", "--solution", "first", "--json")
	got := decoded(t, out)
	if code != 1 || got.Exit != 1 || got.Stress == nil || errs != "" {
		t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
	}
	report := got.Stress
	if report.Generator != "gen" || strings.Join(report.Arguments, " ") != "-n=[3..8] -max=[1..100]" ||
		report.Reference != "brute" || strings.Join(report.Solutions, " ") != "pairs first" ||
		report.Iterations != 3 || report.Passed != 0 || report.Deadline {
		t.Errorf("reported %+v", report)
	}
	stopped := report.Stopped
	if stopped == nil || stopped.Index != 1 || stopped.Verdict != "COUNTEREXAMPLE" || len(stopped.Arguments) != 3 ||
		stopped.Kept != "" || len(stopped.Results) != 2 {
		t.Fatalf("stopped at %+v", stopped)
	}
	pairs, first := stopped.Results[0], stopped.Results[1]
	if pairs.Solution != "pairs" || pairs.Type != "CORRECT" || pairs.Verdict != WrongAnswer || !pairs.Unexpected ||
		!strings.HasPrefix(pairs.Message, "wrong answer the sum is ") {
		t.Errorf("pairs did %+v", pairs)
	}
	if first.Solution != "first" || first.Type != "WRONG_ANSWER" || first.Verdict != WrongAnswer || first.Unexpected {
		t.Errorf("first did %+v", first)
	}
	body, _ := json.Marshal(stopped.Arguments)
	if !regexp.MustCompile(`^\["-n=[3-8]","-max=[0-9]+","[0-9a-f]{16}"\]$`).Match(body) {
		t.Errorf("the arguments are %s", body)
	}
}

func TestStressBlamesAnInvalidInputOnTheGenerator(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[90..100] -max=[900..1000]", "--solution", "twin", "--json")
	got := decoded(t, out)
	if code != 1 || got.Stress == nil || got.Stress.Stopped == nil {
		t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
	}
	stopped := got.Stress.Stopped
	if stopped.Index != 1 || stopped.Verdict != "INVALID" || len(stopped.Results) != 0 ||
		!regexp.MustCompile(`^the validator refuses the input: line 2, a\[[0-9]+\]: [0-9]+ is above 100$`).MatchString(stopped.Why) {
		t.Errorf("stopped at %+v", stopped)
	}

	code, out, _ = stressing(t, "--args", "-n=[90..100] -max=[900..1000]", "--solution", "twin")
	if code != 1 || !strings.Contains(out, "\niteration 1: INVALID, the generator's fault: the validator refuses the input: line 2, a[") ||
		!resolved.MatchString(out) ||
		!strings.HasSuffix(out, "eo-judge: iteration 1 of 100 is INVALID: the generator made an input the validator refuses\n") {
		t.Errorf("exit %d, printed %q", code, out)
	}
}

func TestStressStopsAtAGeneratorThatFails(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[101..199]", "--solution", "twin")
	if code != 1 || !regexp.MustCompile(`\niteration 1: BROKEN: the generator gen -n=1[0-9][0-9] [0-9a-f]{16} exited 3: .*-n=1[0-9][0-9] is above 100`).MatchString(out) ||
		!strings.HasSuffix(out, "eo-judge: iteration 1 of 100 is BROKEN\n") {
		t.Errorf("exit %d, printed %q, said %q", code, out, errs)
	}
}

func TestStressEndsAtItsTimeoutWithoutBlamingTheRunItCut(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[1..8] -max=[1..100]", "--solution", "patient", "--timeout", "2",
		"--json")
	got := decoded(t, out)
	if code != 0 || got.Stress == nil || errs != "" {
		t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
	}
	if report := got.Stress; !report.Deadline || report.Stopped != nil || report.Passed < 1 || report.Passed > 4 {
		t.Errorf("reported %+v", report)
	}

	code, out, _ = stressing(t, "--args", "-n=[1..8] -max=[1..9]", "--solution", "patient", "--timeout", "2")
	if code != 0 || !regexp.MustCompile(`\neo-judge: the 2 s timeout ended the stress after [1-4] iterations passed, with no counterexample\n$`).MatchString(out) {
		t.Errorf("exit %d, printed %q", code, out)
	}
}

func TestStressRefusesWhatItCannotRun(t *testing.T) {
	t.Parallel()
	for _, one := range []struct {
		args []string
		code int
		want string
	}{
		{[]string{"--gen", "sums"}, 2, `the problem has no script called "sums"; it has gen, sum`},
		{[]string{"--args", "-n=[3..1]"}, 2, "[3..1] is empty; write the smaller end first, as [1..3]"},
		{[]string{"--reference", "first"}, 2, `the reference must be a CORRECT solution, and "first" is WRONG_ANSWER`},
		{[]string{"--reference", "none"}, 2, `the problem has no solution called "none"; it has brute, twin, pairs, first, crashing, untyped, patient, parked, above, zero, eights, sluggish`},
		{[]string{"--solution", "none"}, 2, `the problem has no solution called "none"; it has brute, twin, pairs, first, crashing, untyped, patient, parked, above, zero, eights, sluggish`},
		{[]string{"--solution", "brute"}, 2, "brute is the reference; compare it with another solution"},
		{[]string{"--solution", "untyped"}, 2, "no verdict breaks the type of untyped, which has none; " +
			"stress stops only where a solution breaks its type, so declare the one you suspect CORRECT"},
		{[]string{"--iterations", "0"}, 2, "--iterations is 0; it is at least 1"},
		{[]string{"--timeout", "-1"}, 2, "--timeout is -1; it is at least 1 second"},
		{[]string{"--strict"}, 2, "flag provided but not defined: -strict"},
	} {
		code, out, errs := invokeIn(t.TempDir(), append([]string{"stress", "testdata/stress"}, one.args...)...)
		if code != one.code || out != "" || !strings.Contains(errs, one.want) {
			t.Errorf("%q: exit %d, printed %q, said %q", one.args, code, out, errs)
		}
	}

	code, out, errs := invoke("stress", "../tests/live/guess")
	if code != 3 || out != "" || !strings.Contains(errs, "eo-judge stress runs PROGRAM problems") {
		t.Errorf("an interactive problem: exit %d, printed %q, said %q", code, out, errs)
	}
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "problem.json"), []byte(`{"solutions": [{"name": "a", "source": "a.cpp"}]}`), 0o644); err != nil {
		t.Fatal(err)
	}
	code, _, errs = invoke("stress", dir)
	if code != 2 || !strings.Contains(errs, "a stress run needs a CORRECT solution as its reference; the problem has none") {
		t.Errorf("no reference: exit %d, said %q", code, errs)
	}
	code, out, _ = invoke("stress", "--json", dir)
	if got := decoded(t, out); code != 2 || got.Exit != 2 || !strings.Contains(got.Error, "needs a CORRECT solution") {
		t.Errorf("no reference, as JSON: exit %d, reported %+v", code, got)
	}
}

func declared(types map[string]string) func(*Problem) {
	return func(problem *Problem) {
		for _, one := range problem.Solutions {
			if kind, known := types[one.Name]; known {
				one.Type = kind
			}
		}
	}
}

func stressingIn(t *testing.T, dir string, args ...string) (int, string, string) {
	t.Helper()
	needsACompiler(t)
	return invokeIn(t.TempDir(), append([]string{"stress", dir}, args...)...)
}

func TestStressGivesTheCheckerATestWorthNothingAndReportsWhatItSays(t *testing.T) {
	t.Parallel()
	dir := relocated(t, "testdata/stress", declared(map[string]string{"above": "CORRECT"}))
	code, out, errs := stressingIn(t, dir, "--args", "-n=[1..8] -max=[1..100]", "--solution", "above",
		"--iterations", "5", "--json")
	got := decoded(t, out)
	if code != 0 || got.Stress == nil || got.Stress.Passed != 5 {
		t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
	}
	warned := 0
	for _, one := range got.Findings {
		if one.Code == "EO208" && one.Level == "warning" &&
			strings.Contains(one.Message, "this test is worth 0 points, so the judge counts this score of 0.5 as accepted") {
			warned++
		}
	}
	if warned != 1 || len(got.Findings) != 1 {
		t.Errorf("a partial score on a test worth 0 reported %+v", got.Findings)
	}

	code, out, _ = stressingIn(t, dir, "--args", "-n=[1..8] -max=[1..100]", "--solution", "above", "--iterations", "2")
	if code != 0 || !strings.Contains(out, " warning EO208: this test is worth 0 points") {
		t.Errorf("exit %d, printed %q", code, out)
	}
}

func TestStressDropsEveryIterationThatPassed(t *testing.T) {
	t.Parallel()
	dir := relocated(t, "testdata/stress", declared(map[string]string{"eights": "CORRECT"}))
	for attempt := 0; attempt < 10; attempt++ {
		work := filepath.Join(t.TempDir(), "work")
		code, out, errs := stressingIn(t, dir, "--args", "-n=[1..8] -max=[1..100]", "--solution", "eights",
			"--solution", "twin", "--iterations", "500", "--work", work, "--json")
		got := decoded(t, out)
		if code != 1 || got.Stress == nil || got.Stress.Stopped == nil {
			t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
		}
		stopped := got.Stress.Stopped
		if stopped.Index < 3 {
			continue
		}
		if got.Stress.Passed != stopped.Index-1 || stopped.Verdict != "COUNTEREXAMPLE" || stopped.Arguments[0] != "-n=8" {
			t.Errorf("reported %+v", got.Stress)
		}
		if twin := stopped.Results[1]; !strings.HasSuffix(twin.Message, fmt.Sprintf(", test %d of group 0", stopped.Index)) {
			t.Errorf("the checker was told another index than iteration %d: %q", stopped.Index, twin.Message)
		}
		left, err := os.ReadDir(filepath.Join(work, "stress"))
		if err != nil || len(left) != 1 || left[0].Name() != strconv.Itoa(stopped.Index) {
			t.Errorf("after %d passed iterations the workspace keeps %v (%v)", got.Stress.Passed, left, err)
		}
		return
	}
	t.Fatal("ten stresses all stopped before their third iteration")
}

func TestStressCallsACheckerThatFailsBroken(t *testing.T) {
	t.Parallel()
	dir := relocated(t, "testdata/stress", declared(map[string]string{"zero": "CORRECT"}))
	code, out, errs := stressingIn(t, dir, "--args", "-n=[1..8] -max=[1..100]", "--reference", "zero",
		"--solution", "twin", "--json")
	got := decoded(t, out)
	if code != 1 || got.Stress == nil || got.Stress.Stopped == nil {
		t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
	}
	stopped := got.Stress.Stopped
	if stopped.Index != 1 || stopped.Verdict != "BROKEN" ||
		stopped.Why != "the checker failed on the output of twin: jury error: answer.txt, line 1, sum: 0 is below 1" ||
		len(stopped.Results) != 1 || stopped.Results[0].Verdict != Failure {
		t.Errorf("stopped at %+v", stopped)
	}
}

func TestStressCallsAValidatorThatHangsBroken(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	hanging := filepath.Join(t.TempDir(), "validator.cpp")
	if err := os.WriteFile(hanging, []byte("#include <unistd.h>\nint main() { ::sleep(5); }\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	dir := relocated(t, "testdata/stress", func(problem *Problem) { problem.Validator.Source = hanging })
	shop := workshop(t, dir)
	shop.validatorLimit = 300
	plan, why := (&stressOptions{args: "-n=[1..8] -max=[1..100]", iterations: 1, timeout: 60}).plan(shop.Problem)
	if why != "" {
		t.Fatal(why)
	}
	if err := shop.BuildAll(context.Background(), append([]*Solution{plan.reference}, plan.compared...)); err != nil {
		t.Fatal(err)
	}
	one, err := shop.iterate(context.Background(), plan, 1)
	if err != nil || one.Verdict != "BROKEN" || one.Why != "the validator did not finish in 0.3 s" {
		t.Errorf("iterated to %+v (%v)", one, err)
	}
}

func TestATimeoutBeforeAnyIterationPassedSaysWhatItCut(t *testing.T) {
	t.Parallel()
	dir := relocated(t, "testdata/stress", declared(map[string]string{"sluggish": "CORRECT"}))
	code, out, errs := stressingIn(t, dir, "--args", "-n=[1..8] -max=[1..100]", "--solution", "sluggish",
		"--timeout", "1")
	want := "eo-judge: the 1 s timeout ended the stress in iteration 1 while the solution sluggish ran, " +
		"before any iteration passed; give it a longer --timeout\n"
	if code != 3 || errs != want || strings.Contains(out, "no counterexample") {
		t.Errorf("exit %d, printed %q, said %q", code, out, errs)
	}
	code, out, _ = stressingIn(t, dir, "--args", "-n=[1..8] -max=[1..100]", "--solution", "sluggish",
		"--timeout", "1", "--json")
	if got := decoded(t, out); code != 3 || got.Exit != 3 || got.Stress == nil || !got.Stress.Deadline ||
		got.Error != strings.TrimSuffix(strings.TrimPrefix(want, "eo-judge: "), "\n") {
		t.Errorf("exit %d, reported %+v", code, got)
	}
}

func TestEachArgumentMayBeGivenWhole(t *testing.T) {
	t.Parallel()
	pattern, err := parseArguments([]string{"-title=a b [1..2]", "-n=[3..3]"})
	if err != nil {
		t.Fatal(err)
	}
	if args := pattern.resolve(); len(args) != 3 || !regexp.MustCompile(`^-title=a b [12]$`).MatchString(args[0]) ||
		args[1] != "-n=3" {
		t.Errorf("resolved to %q", args)
	}
	if line := pasteable("gen", []string{"-title=a \"b\"", "x"}); line != `"generator": {"script": "gen", "arguments": ["-title=a \"b\"", "x"]}` {
		t.Errorf("pasted %s", line)
	}

	code, out, errs := stressing(t, "--arg", "-n=[3..8]", "--arg", "-max=[1..100]", "--solution", "pairs", "--json")
	got := decoded(t, out)
	if code != 1 || got.Stress == nil || strings.Join(got.Stress.Arguments, "|") != "-n=[3..8]|-max=[1..100]" ||
		got.Stress.Stopped == nil || got.Stress.Stopped.Verdict != "COUNTEREXAMPLE" {
		t.Errorf("exit %d, reported %+v, said %q", code, got, errs)
	}
	code, out, _ = stressing(t, "--arg", "-n=[3..8]", "--arg", "-max=[1..100]", "--arg", "-title=a b", "--arg", "",
		"--solution", "twin")
	if code != 1 || !strings.HasPrefix(out, `stress: gen -n=[3..8] -max=[1..100] "-title=a b" "" against brute, `) ||
		!regexp.MustCompile(`\niteration 1: BROKEN: the generator gen -n=[3-8] -max=[0-9]+ "-title=a b" "" [0-9a-f]{16} exited 3: `).MatchString(out) {
		t.Errorf("an argument with a space exited %d, printed %q", code, out)
	}
	code, _, errs = invokeIn(t.TempDir(), "stress", "testdata/stress", "--args", "-n=1", "--arg", "-max=1")
	if code != 2 || !strings.Contains(errs, "give the generator's arguments with --args or with --arg, not both") {
		t.Errorf("both exited %d, said %q", code, errs)
	}
}

func TestContinueGoesPastAnInvalidInput(t *testing.T) {
	t.Parallel()
	code, out, errs := stressing(t, "--args", "-n=[3..8] -max=[90..150]", "--solution", "twin", "--continue",
		"--iterations", "60", "--json")
	got := decoded(t, out)
	if code != 1 || got.Stress == nil || got.Stress.Stopped != nil {
		t.Fatalf("exit %d, reported %+v, said %q", code, got, errs)
	}
	report := got.Stress
	if report.Passed < 1 || len(report.Failed) < 1 || report.Passed+len(report.Failed) != 60 {
		t.Errorf("reported %+v", report)
	}
	for _, one := range report.Failed {
		if one.Verdict != "INVALID" || !strings.HasPrefix(one.Why, "the validator refuses the input: ") {
			t.Errorf("went past %+v", one)
		}
	}

	code, out, _ = stressing(t, "--args", "-n=[3..8] -max=[90..150]", "--solution", "twin", "--continue",
		"--iterations", "60")
	if code != 1 || !regexp.MustCompile(`\neo-judge: [0-9]+ iterations passed and [0-9]+ did not, with no counterexample\n$`).MatchString(out) ||
		!strings.Contains(out, "INVALID, the generator's fault") {
		t.Errorf("exit %d, printed %q", code, out)
	}
	code, out, _ = stressing(t, "--args", "-n=[101..200]", "--solution", "pairs", "--continue", "--iterations", "3")
	if code != 1 || !strings.HasSuffix(out, "eo-judge: 0 iterations passed and 3 did not, with no counterexample\n") {
		t.Errorf("exit %d, printed %q", code, out)
	}
}
