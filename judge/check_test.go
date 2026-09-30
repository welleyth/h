package main

import (
	"context"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
	"time"
)

func needsACompiler(t *testing.T) {
	t.Helper()
	if testing.Short() {
		t.Skip("short mode does not compile")
	}
	if _, err := exec.LookPath(compiler()); err != nil {
		t.Skipf("no %s on the path", compiler())
	}
}

func workshop(t *testing.T, dir string) *Workspace {
	t.Helper()
	problem, err := LoadProblem(dir)
	if err != nil {
		t.Fatal(err)
	}
	return NewWorkspace(problem, t.TempDir())
}

func TestCheckFindsWhatIsWrongWithABrokenProblem(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	shop := workshop(t, "testdata/broken")
	found, err := shop.Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	found = append(found, Lint(shop.Problem)...)

	for _, code := range []string{
		"EO501", "EO801", "EO802", "EO807", "EO809", "EO818",
		"EO819", "EO820", "EO821", "EO901", "EO904", "EO907", "EO811",
	} {
		if !fired(found, code) {
			t.Errorf("%s did not fire on the broken problem", code)
		}
	}
}

func TestCheckReadsEveryAnswerNotOnlyTheFirst(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	shop := workshop(t, "testdata/answers")
	found, err := shop.Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}

	where := ""
	for _, one := range found {
		if one.Code == "EO801" {
			where = one.Where
		}
	}
	if where != "test 1:2" {
		t.Fatalf("EO801 fired at %q; the broken answer is on the second test", where)
	}
}

func TestRunReproducesTheJudgeOnABatchProblem(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	shop := workshop(t, "../tests/live/degrees")
	judged := judgeAll(t, shop)

	if got := judged["full"]; got.Verdict != Accepted || got.Score != 100 {
		t.Errorf("full scored %v at %v, the judge says ACCEPTED at 100", got.Verdict, got.Score)
	}
	got := judged["leaves"]
	if got.Verdict != Partial {
		t.Errorf("leaves is %v, the judge says PARTIALLY_CORRECT", got.Verdict)
	}
	if got.Score < 55.7 || got.Score > 55.8 {
		t.Errorf("leaves scored %v, the judge says 55.7675", got.Score)
	}
	if got := judged["zeros"]; got.Verdict != Partial || got.Score != 0 {
		t.Errorf("zeros scored %v at %v, the judge says PARTIALLY_CORRECT at 0", got.Verdict, got.Score)
	}
}

func TestRunReproducesTheJudgeOnAnInteractiveProblem(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	shop := workshop(t, "../tests/live/guess")
	judged := judgeAll(t, shop)

	for name, want := range map[string]struct {
		verdict Verdict
		score   Points
	}{
		"binary": {Accepted, 100},
		"linear": {RuntimeFail, 40},
		"polite": {WrongAnswer, 40},
	} {
		got := judged[name]
		if got == nil {
			t.Fatalf("%s was never judged", name)
		}
		if got.Verdict != want.verdict || got.Score != want.score {
			t.Errorf("%s scored %v at %v, the judge says %v at %v",
				name, got.Verdict, got.Score, want.verdict, want.score)
		}
	}
}

func TestACrashAfterAnAcceptedDialogueIsNotForgiven(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	shop := workshop(t, "../tests/live/guess")
	got := judgeAll(t, shop)["crasher"]
	if got == nil {
		t.Fatal("crasher was never judged")
	}
	if got.Verdict != RuntimeFail || got.Score != 0 {
		t.Fatalf("crasher scored %v at %v; the agent returns the solution's own status "+
			"before it reads the interactor, so a clean dialogue does not excuse exit 3",
			got.Verdict, got.Score)
	}
}

func judgeAll(t *testing.T, shop *Workspace) map[string]*Attempt {
	t.Helper()
	ctx := context.Background()
	if err := shop.BuildAll(ctx, nil); err != nil {
		t.Fatal(err)
	}
	if err := shop.Generate(ctx); err != nil {
		t.Fatal(err)
	}
	if err := shop.Validate(ctx); err != nil {
		t.Fatal(err)
	}
	for _, made := range shop.sorted() {
		if shop.Problem.Validator != nil && !made.Valid {
			t.Fatalf("test %d:%d is invalid: %s", made.Group, made.Test.Index, made.Why)
		}
	}

	out := map[string]*Attempt{}
	for _, solution := range shop.Problem.Solutions {
		attempt, err := shop.Evaluate(ctx, solution.Name, &Program{Source: solution.Source})
		if err != nil {
			t.Fatal(err)
		}
		out[solution.Name] = attempt
	}
	return out
}

func TestAValidatorThatReadsStdinGetsTheTestThere(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "validator.cpp"), "#include <cstdio>\n"+
		"int main() {\n"+
		"    int n = 0;\n"+
		"    if (std::scanf(\"%d\", &n) != 1) { std::puts(\"no n on stdin\"); return 1; }\n"+
		"    return n == 7 ? 0 : 1;\n}\n")
	writeFile(t, filepath.Join(dir, "01.in"), "7\n")
	writeFile(t, filepath.Join(dir, "01.ans"), "7\n")
	writeFile(t, filepath.Join(dir, "problem.json"), `{"type": "PROGRAM",
		"validator": {"source": "validator.cpp"},
		"testsets": [{"index": 1, "tests": [{"index": 1, "score": 100, "input": "01.in", "answer": "01.ans"}]}]}`)
	shop := workshop(t, dir)
	ctx := context.Background()
	if err := shop.Generate(ctx); err != nil {
		t.Fatal(err)
	}
	if err := shop.Validate(ctx); err != nil {
		t.Fatal(err)
	}
	for _, made := range shop.sorted() {
		if !made.Valid || made.Why != "" {
			t.Errorf("test %d:%d: valid %v, %q; the judge gives the validator the test on stdin too",
				made.Group, made.Test.Index, made.Valid, made.Why)
		}
	}
}

func TestTheCopiedPointsParserMatchesTheAgent(t *testing.T) {
	t.Parallel()
	agent := os.Getenv("AGENT_REPO")
	demanded := agent != ""
	if agent == "" {
		agent = filepath.Join("..", "..", "agent")
	}
	if _, err := os.Stat(filepath.Join(agent, ".git")); err != nil {
		if demanded {
			t.Fatalf("AGENT_REPO is %q and has no checkout in it: %v", agent, err)
		}
		t.Skip("no agent checkout to compare against; set AGENT_REPO to demand one")
	}

	said, err := exec.Command("git", "-C", agent, "show",
		"origin/main:internal/judge/checker/program.go").Output()
	if err != nil {
		if demanded {
			t.Fatalf("cannot read the agent's source: %v", err)
		}
		t.Skipf("cannot read the agent's source: %v", err)
	}

	theirs := between(string(said), "func (c *Program) readPoints", "\n}\n")
	ours, err := os.ReadFile("points.go")
	if err != nil {
		t.Fatal(err)
	}
	mine := between(string(ours), "func readPoints", "\n}\n")

	if squeeze(theirs) != squeeze(mine) {
		t.Fatalf("the copy has drifted from the agent's parser:\n--- agent\n%s\n--- here\n%s",
			theirs, mine)
	}
}

func between(text, from, to string) string {
	start := strings.Index(text, from)
	if start < 0 {
		return ""
	}
	text = text[start:]
	if stop := strings.Index(text, to); stop >= 0 {
		text = text[:stop]
	}
	if open := strings.Index(text, "{"); open >= 0 {
		text = text[open:]
	}
	return text
}

func squeeze(text string) string {
	var out []string
	for _, line := range strings.Split(text, "\n") {
		trimmed := strings.TrimSpace(line)
		if trimmed == "" {
			continue
		}
		trimmed = strings.ReplaceAll(trimmed, "c.action.Workspace().Open(filename, syscall.O_RDONLY, 0)",
			"os.Open(filename)")
		out = append(out, trimmed)
	}
	return strings.Join(out, "\n")
}

func TestABrokenValidatorIsNotBlamedOnTheTests(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	dir := t.TempDir()
	for _, name := range []string{"problem.json", "checker.cpp", "generator.cpp", "solution.cpp"} {
		body, err := os.ReadFile(filepath.Join("testdata/broken", name))
		if err != nil {
			t.Fatal(err)
		}
		if name == "problem.json" {
			header, err := filepath.Abs("../eolymp.h")
			if err != nil {
				t.Fatal(err)
			}
			body = []byte(strings.ReplaceAll(string(body), "../../../eolymp.h", header))
		}
		if err := os.WriteFile(filepath.Join(dir, name), body, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	validator := "#include \"eolymp.h\"\n\nint main(int argc, char** argv) {\n" +
		"    eo::validator v(argc, argv);\n" +
		"    eo::charset const bad(\"A-Za-z()- \");\n" +
		"    (void)bad;\n" +
		"    v.read_int(1, 1000, \"n\");\n}\n"
	if err := os.WriteFile(filepath.Join(dir, "validator.cpp"), []byte(validator), 0o644); err != nil {
		t.Fatal(err)
	}

	problem, err := LoadProblem(dir)
	if err != nil {
		t.Fatal(err)
	}
	found, err := NewWorkspace(problem, t.TempDir()).Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}

	broken, blamed := 0, 0
	for _, one := range found {
		if one.Code != "EO806" {
			continue
		}
		if strings.Contains(one.Message, "the validator could not run") {
			broken++
		}
		if strings.Contains(one.Message, "the validator rejects it") {
			blamed++
		}
	}
	if broken == 0 {
		t.Error("a validator that raises a jury error was not reported as broken")
	}
	if blamed != 0 {
		t.Errorf("%d tests were blamed for a broken validator", blamed)
	}
}

func TestASolutionCannotWriteTheSummary(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "run", "testdata/forged")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if !strings.Contains(out, "forger: PARTIALLY_CORRECT, 1\n") {
		t.Errorf("printed %q", out)
	}
}

func TestASolutionCannotReachTheAnswersByRelativePath(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "run", "testdata/poisoned", "--solution", "walker")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if !strings.Contains(out, "walker: WRONG_ANSWER, 0\n") {
		t.Errorf("printed %q", out)
	}
}

func TestAnAnswerRewrittenDuringARunStopsTheRun(t *testing.T) {
	needsACompiler(t)
	temporary := t.TempDir()
	t.Setenv("TMPDIR", temporary)
	code, out, errs := invokeIn(temporary, "run", "testdata/poisoned", "--solution", "poisoner")
	if code != 3 || !strings.Contains(errs, "01-001.ans changed while solution.poisoner ran") {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	if strings.Contains(out, "poisoner: ACCEPTED") {
		t.Errorf("printed %q", out)
	}
}

func TestCheckSaysTheSameEveryTime(t *testing.T) {
	t.Parallel()
	needsACompiler(t)

	shop := workshop(t, "testdata/broken")
	first := ""
	for at := 0; at < 6; at++ {
		found, err := shop.Check(context.Background(), false)
		if err != nil {
			t.Fatal(err)
		}
		var out strings.Builder
		report(&out, append(found, Lint(shop.Problem)...), false)
		if at == 0 {
			first = out.String()
		} else if out.String() != first {
			t.Fatalf("check %d printed\n%s\nand check 1 printed\n%s", at+1, out.String(), first)
		}
	}
}

func TestFindingsAreOrderedByPlaceThenMessage(t *testing.T) {
	t.Parallel()
	found := Findings{
		{Code: "EO801", Severity: "warning", Where: "test 1:10", Message: "b"},
		{Code: "EO801", Severity: "warning", Where: "test 1:2", Message: "b"},
		{Code: "EO801", Severity: "warning", Where: "test 1:2", Message: "a"},
		{Code: "EO807", Severity: "note", Where: "testset 1", Message: "a"},
		{Code: "EO807", Severity: "warning", Where: "testset 2", Message: "a"},
		{Code: "EO807", Severity: "warning", Where: "testset 10", Message: "a"},
	}
	var out strings.Builder
	report(&out, found, false)
	var order []string
	for _, line := range strings.Split(out.String(), "\n") {
		if strings.Contains(line, " EO8") {
			order = append(order, line)
		}
	}
	want := []string{
		"test 1:2: warning EO801: a", "test 1:2: warning EO801: b", "test 1:10: warning EO801: b",
		"testset 2: warning EO807: a", "testset 10: warning EO807: a", "testset 1: note EO807: a",
	}
	if strings.Join(order, "\n") != strings.Join(want, "\n") {
		t.Errorf("ordered\n%s", strings.Join(order, "\n"))
	}
}

func TestNaturalOrderComparesRunsOfDigitsAsNumbers(t *testing.T) {
	t.Parallel()
	for _, pair := range [][2]string{
		{"test 1:2", "test 1:10"}, {"test 2:1", "test 10:1"}, {"a", "b"}, {"a", "a1"},
		{"test 1:1", "test 1:01"}, {"x9", "x10"}, {"9", "a"}, {"", "a"},
	} {
		if !naturalLess(pair[0], pair[1]) || naturalLess(pair[1], pair[0]) {
			t.Errorf("%q and %q are ordered the wrong way", pair[0], pair[1])
		}
	}
	if naturalLess("test 1:2", "test 1:2") {
		t.Error("a place is before itself")
	}
}

func TestAFailedGeneratorNamesTheTestAndItsArguments(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "gen.cpp"), "#include <cstdio>\n#include <cstring>\n"+
		"int main(int argc, char** argv) {\n"+
		"    if (argc > 1 && std::strcmp(argv[1], \"-n=0\") == 0) { std::fprintf(stderr, \"n is 0\\n\"); return 3; }\n"+
		"    std::printf(\"1\\n\");\n}\n")
	writeFile(t, filepath.Join(dir, "answer.cpp"), "int main() { return 4; }\n")
	for _, one := range []struct{ arguments, answer, said string }{
		{`"-n=0", "-seed=7"`, "", "test 1:1: the generator gen -n=0 -seed=7 exited 3: n is 0"},
		{`"-n=1"`, `, "answerGenerator": "answer"`, "test 1:1: the answer generator answer exited 4"},
	} {
		writeFile(t, filepath.Join(dir, "problem.json"), `{"type": "PROGRAM",
			"scripts": {"gen": {"source": "gen.cpp"}, "answer": {"source": "answer.cpp"}},
			"testsets": [{"index": 1, "tests": [{"index": 1, "score": 100,
				"generator": {"script": "gen", "arguments": [`+one.arguments+`]}`+one.answer+`}]}]}`)
		code, _, errs := invokeIn(t.TempDir(), "run", dir)
		if code != 3 || errs != "eo-judge: "+one.said+"\n" {
			t.Errorf("exit %d, said %q, not %q", code, errs, one.said)
		}
	}
}

func TestEveryTestThatCannotBeMadeIsNamedBeforeTheRunStops(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "gen.cpp"), "#include <cstdio>\n#include <cstring>\n"+
		"int main(int argc, char** argv) {\n"+
		"    if (argc > 1 && std::strcmp(argv[1], \"-n=0\") == 0) return 3;\n"+
		"    std::printf(\"1\\n\");\n}\n")
	writeFile(t, filepath.Join(dir, "solution.cpp"), "int main() {}\n")
	writeFile(t, filepath.Join(dir, "problem.json"), `{"type": "PROGRAM",
		"scripts": {"gen": {"source": "gen.cpp"}, "answer": {"source": "solution.cpp"}},
		"solutions": [{"name": "main", "source": "solution.cpp"}],
		"testsets": [{"index": 1, "tests": [
			{"index": 1, "score": 50, "generator": {"script": "gen", "arguments": ["-n=0"]}, "answerGenerator": "answer"},
			{"index": 2, "score": 25, "generator": {"script": "gen", "arguments": ["-n=1"]}, "answerGenerator": "answer"},
			{"index": 3, "score": 25, "input": "missing.txt", "answerGenerator": "answer"}]}]}`)
	for _, command := range []string{"run", "check"} {
		code, out, errs := invokeIn(t.TempDir(), command, dir)
		want := "eo-judge: 2 tests could not be made, so nothing was judged:\n" +
			"  test 1:1: the generator gen -n=0 exited 3\n" +
			"  test 1:3: open " + filepath.Join(dir, "missing.txt") + ": no such file or directory\n"
		if code != 3 || errs != want || strings.Contains(out, "main:") {
			t.Errorf("%s exited %d, printed %q, said %q", command, code, out, errs)
		}
	}
}

func TestAGeneratorThatTimesOutIsNotTriedAgain(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "slow.cpp"), "int main() { for (;;) {} }\n")
	writeFile(t, filepath.Join(dir, "gen.cpp"), "#include <cstdio>\nint main() { std::printf(\"1\\n\"); }\n")
	writeFile(t, filepath.Join(dir, "problem.json"), `{"type": "PROGRAM",
		"scripts": {"slow": {"source": "slow.cpp"}, "gen": {"source": "gen.cpp"}},
		"testsets": [{"index": 1, "tests": [
			{"index": 1, "score": 30, "generator": {"script": "slow", "arguments": ["-n=1"]}, "answer": "gen.cpp"},
			{"index": 2, "score": 30, "generator": {"script": "gen"}, "answerGenerator": "slow"},
			{"index": 3, "score": 40, "generator": {"script": "slow", "arguments": ["-n=2"]}, "answer": "gen.cpp"}]}]}`)
	shop := workshop(t, dir)
	shop.generatorLimit = 300
	started := time.Now()
	err := shop.Generate(context.Background())
	want := "3 tests could not be made, so nothing was judged:\n" +
		"  test 1:1: the generator slow -n=1 did not finish in 0.3 s\n" +
		"  test 1:2: not tried, since slow did not finish on test 1:1\n" +
		"  test 1:3: not tried, since slow did not finish on test 1:1"
	if err == nil || err.Error() != want {
		t.Errorf("said %v", err)
	}
	if spent := time.Since(started); spent > 3*time.Second {
		t.Errorf("generating took %v", spent)
	}
}

func TestCheckFindsAnInteractorThatFailsOnABadClient(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	found, err := workshop(t, "testdata/fragile").Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	blamed := map[string]bool{}
	for _, one := range found {
		if one.Code == "EO814" && one.Where == "interactor" {
			blamed[strings.SplitN(one.Message, " ends the run", 2)[0]] = true
		}
	}
	for _, client := range []string{"a client that exits at once", "a client that prints garbage"} {
		if !blamed[client] {
			t.Errorf("EO814 did not fire for %s: %v", client, found)
		}
	}
}

func TestCheckTriesAnOptionExtremeWithEveryTestsArguments(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	found, err := workshop(t, "testdata/dependent").Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	var extremes []string
	for _, one := range found {
		if one.Code == "EO813" {
			extremes = append(extremes, one.Where+": "+one.Message)
		}
	}
	if len(extremes) != 0 {
		t.Errorf("EO813 said %q; m=1000 generates with -n=1000, and n=1 is refused by g.require, not broken", extremes)
	}
}

func TestCheckStillBlamesAnExtremeThatFailsForAnotherReason(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := t.TempDir()
	runs := filepath.Join(dir, "runs.txt")
	writeFile(t, filepath.Join(dir, "gen.cpp"), "#include <cstdio>\n#include <cstring>\n"+
		"int main(int argc, char** argv) {\n"+
		"    if (argc > 1 && std::strcmp(argv[1], \"--eo-describe\") == 0) {\n"+
		"        std::printf(\"eo-describe option n an integer 1..9\\n\");\n        return 0;\n    }\n"+
		"    if (argc > 1 && std::strcmp(argv[1], \"-n=9\") == 0) {\n"+
		"        std::FILE* ran = std::fopen(\""+runs+"\", \"a\");\n"+
		"        std::fputs(\"n=9\\n\", ran);\n        std::fclose(ran);\n"+
		"        std::fprintf(stderr, \"n is 9\\n\");\n        return 3;\n    }\n"+
		"    if (argc > 1 && std::strcmp(argv[1], \"-n=1\") == 0) { std::fprintf(stderr, \"too few\\n\"); return 4; }\n"+
		"    std::printf(\"1\\n\");\n}\n")
	writeFile(t, filepath.Join(dir, "answer.cpp"), "int main() {}\n")
	writeFile(t, filepath.Join(dir, "problem.json"), `{"type": "PROGRAM",
		"scripts": {"gen": {"source": "gen.cpp"}, "answer": {"source": "answer.cpp"}},
		"testsets": [{"index": 1, "tests": [
			{"index": 1, "score": 40, "generator": {"script": "gen", "arguments": ["-n=5", "-k=1"]}, "answerGenerator": "answer"},
			{"index": 2, "score": 30, "generator": {"script": "gen", "arguments": ["-n=5", "-k=2"]}, "answerGenerator": "answer"},
			{"index": 3, "score": 30, "generator": {"script": "gen", "arguments": ["-n=6", "-k=3"]}, "answerGenerator": "answer"}]}]}`)
	found, err := workshop(t, dir).Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	var extremes []string
	for _, one := range found {
		if one.Code == "EO813" {
			extremes = append(extremes, one.Message)
		}
	}
	if len(extremes) != 1 || extremes[0] != "n=9 does not generate: exit 3: n is 9" {
		t.Errorf("EO813 said %q; n=9 fails with exit 3, and n=1 is only refused with exit 4", extremes)
	}
	ran, err := os.ReadFile(runs)
	if err != nil || string(ran) != "n=9\n" {
		t.Errorf("the failing extreme ran %q; one failure other than exit 4 is reported at once", ran)
	}
}

func TestCheckFindsAnOptionExtremeTheValidatorRefuses(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	found, err := workshop(t, "testdata/extremes").Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	var extremes []string
	for _, one := range found {
		if one.Code == "EO813" {
			extremes = append(extremes, one.Where+": "+one.Message)
		}
	}
	if len(extremes) != 1 || !strings.HasPrefix(extremes[0], "script gen: n=2000 produces an invalid test: ") {
		t.Errorf("EO813 said %q; only n=2000 is outside the validator's range", extremes)
	}
}

func messagesOf(found Findings, code string) map[string]bool {
	said := map[string]bool{}
	for _, one := range found {
		if one.Code == code {
			said[one.Where+": "+one.Message] = true
		}
	}
	return said
}

func withPrefix(said map[string]bool, prefix string) bool {
	for one := range said {
		if strings.HasPrefix(one, prefix) {
			return true
		}
	}
	return false
}

func TestHowAValidatorsRefusalAndItsBreakdownAreTold(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	refusing := relocated(t, "testdata/stress", func(problem *Problem) {
		problem.Testsets[0].Tests[1].Generator.Arguments = []string{"-n=100", "-max=1000"}
	})
	found, err := workshop(t, refusing).Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	said := messagesOf(found, "EO806")
	if len(said) != 2 || !withPrefix(said, "test 1:2: the validator rejects it: line 2, a[") ||
		!withPrefix(said, "test 1:2: the test is invalid with no --group: line 2, a[") {
		t.Errorf("a test the validator refuses: %v", said)
	}

	broken := filepath.Join(t.TempDir(), "validator.cpp")
	body := "#include <eolymp.h>\n\nint main(int argc, char** argv) {\n    eo::validator v(argc, argv);\n" +
		"    eo::charset const bad(\"A-Za-z()- \");\n    (void)bad;\n}\n"
	if err := os.WriteFile(broken, []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	input := "5\n"
	dir := relocated(t, "testdata/stress", func(problem *Problem) {
		problem.Validator.Source = broken
		problem.ValidatorTests = []*ValidatorTest{{Input: &input, Expect: "INVALID"}}
	})
	found, err = workshop(t, dir).Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	said = messagesOf(found, "EO806")
	if !withPrefix(said, "test 1:1: the validator could not run: eolymp.h: the character range \")- \" in charset(\"A-Za-z()- \") runs backwards") {
		t.Errorf("a broken validator: %v", said)
	}
	if said := messagesOf(found, "EO911"); len(said) != 1 || !withPrefix(said, "validator test 1: it is expected INVALID, "+
		"and the validator could not run: eolymp.h: the character range \")- \" in charset(\"A-Za-z()- \") runs backwards") {
		t.Errorf("an INVALID validator test on a broken validator: %v", said)
	}

	code, out, errs := invokeIn(t.TempDir(), "stress", dir, "--args", "-n=[1..8] -max=[1..100]", "--solution", "twin")
	if code != 1 || !strings.Contains(out, "\niteration 1: BROKEN: the validator could not run: eolymp.h: the character range") {
		t.Errorf("stress on a broken validator exited %d, printed %q, said %q", code, out, errs)
	}

	aborting := filepath.Join(t.TempDir(), "validator.cpp")
	if err := os.WriteFile(aborting, []byte("#include <csignal>\nint main() { std::raise(SIGTERM); }\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	killed := relocated(t, "testdata/stress", func(problem *Problem) { problem.Validator.Source = aborting })
	found, err = workshop(t, killed).Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	if said := messagesOf(found, "EO806"); !withPrefix(said, "test 1:1: the validator could not run: it was killed by signal 15, ") {
		t.Errorf("a validator a signal killed: %v", said)
	}
	code, out, errs = invokeIn(t.TempDir(), "stress", killed, "--args", "-n=[1..8] -max=[1..100]", "--solution", "twin")
	if code != 1 || !regexp.MustCompile(`\niteration 1: BROKEN: the validator could not run: it was killed by signal 15, [a-z]`).MatchString(out) {
		t.Errorf("stress on a validator a signal killed exited %d, printed %q, said %q", code, out, errs)
	}
}
