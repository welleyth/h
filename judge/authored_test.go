package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func authoredFindings(t *testing.T, dir, code string) map[string]string {
	t.Helper()
	shop := workshop(t, dir)
	found, err := shop.Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	said := map[string]string{}
	for _, one := range found {
		if one.Code == code {
			if one.Severity != "warning" || one.Fix == "" {
				t.Errorf("%s is %+v", code, one)
			}
			said[one.Where] = one.Message
		}
	}
	return said
}

func TestCheckRunsTheValidatorTestsTheAuthorWrote(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	said := authoredFindings(t, "testdata/authored", "EO911")
	want := map[string]string{
		"validator test 5": "it is expected VALID, and the validator refuses it: line 2, a[1]: 500 is above 100",
		"validator test 6": "it is expected INVALID in testset 1, and the validator accepts it",
	}
	if len(said) != len(want) {
		t.Errorf("EO911 fired on %v", said)
	}
	for where, message := range want {
		if said[where] != message {
			t.Errorf("%s: %q, not %q", where, said[where], message)
		}
	}
}

func TestRunLeavesTheAuthorsTestsAlone(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := relocated(t, "testdata/authored", func(problem *Problem) {
		problem.ValidatorTests[2].File = "no-such-file.txt"
	})
	code, out, errs := invokeIn(t.TempDir(), "run", dir)
	if code != 0 || strings.Contains(out, "validator test") || !strings.Contains(out, "sum: ACCEPTED, 100") {
		t.Errorf("run exited %d, printed %q, said %q", code, out, errs)
	}
	code, _, errs = invokeIn(t.TempDir(), "check", dir)
	if code != 3 || !strings.Contains(errs, "validator test 3: open ") || !strings.Contains(errs, "no-such-file.txt") {
		t.Errorf("check exited %d, said %q", code, errs)
	}
}

func TestAValidatorTestIsReadStrictly(t *testing.T) {
	t.Parallel()
	validator := `"validator": {"source": "v.cpp"}, "testsets": [{"index": 1}], `
	for body, want := range map[string]string{
		validator + `"validatorTests": [{"input": "1\n", "expect": "VALID", "grup": 1}]}`:       `unknown field "grup"`,
		validator + `"validatorTests": [{"Input": "1\n", "expect": "VALID"}]}`:                  `the field "input" is spelt "Input"`,
		validator + `"validatorTests": [{"input": "1\n", "expect": "valid"}]}`:                  `validator test 1's expect is "valid"; it is one of VALID, INVALID`,
		validator + `"validatorTests": [{"input": "1\n"}]}`:                                     `validator test 1's expect is ""; it is one of VALID, INVALID`,
		validator + `"validatorTests": [{"input": "1\n", "file": "a.txt", "expect": "VALID"}]}`: "validator test 1 has both an input and a file; give one",
		validator + `"validatorTests": [{"expect": "VALID"}, {"expect": "VALID"}]}`:             "validator test 1 has neither an input nor a file; give one",
		validator + `"validatorTests": [{"input": "", "expect": "INVALID", "group": 2}]}`:       "validator test 1's group is 2, and the problem has no testset 2",
		`"validatorTests": [{"input": "", "expect": "INVALID"}]}`:                               "the problem has validatorTests and no validator to run them",
	} {
		if err := loading(t, `{"type": "PROGRAM", `+body); err == nil || !strings.Contains(err.Error(), want) {
			t.Errorf("%s: %v, not %q", body, err, want)
		}
	}
	problem := loaded(t, `{"type": "PROGRAM", `+validator+`"validatorTests": [{"input": "", "expect": "INVALID", "group": 1}, {"file": "a.txt", "expect": "VALID"}]}`)
	if tests := problem.ValidatorTests; len(tests) != 2 || tests[0].Input == nil || *tests[0].Input != "" ||
		*tests[0].Group != 1 || tests[1].File != "a.txt" || tests[1].Group != nil {
		t.Errorf("read %+v", problem.ValidatorTests)
	}
}

func TestAValidatorTestIsFoldedAsTheJudgeFoldsATest(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	lines := filepath.Join(t.TempDir(), "crlf.txt")
	if err := os.WriteFile(lines, []byte("1\r\n5\r\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	crlf := "2\r\n1 2\r\n"
	dir := relocated(t, "testdata/authored", func(problem *Problem) {
		problem.ValidatorTests = []*ValidatorTest{{File: lines, Expect: "VALID"}, {Input: &crlf, Expect: "VALID"}}
	})
	shop := workshop(t, dir)
	found := Findings{}
	if err := shop.validatorTestChecks(context.Background(), &found); err != nil {
		t.Fatal(err)
	}
	for at, want := range []string{"1\n5\n", "2\n1 2\n"} {
		given, err := os.ReadFile(filepath.Join(shop.Dir, "authored", fmt.Sprintf("validator-%d.txt", at+1)))
		if err != nil || string(given) != want {
			t.Errorf("validator test %d was given %q (%v), not %q", at+1, given, err, want)
		}
	}
	if len(found) != 0 {
		t.Errorf("found %+v", found)
	}
}

func TestCheckRunsTheCheckerTestsTheAuthorWrote(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	said := authoredFindings(t, "testdata/authored", "EO912")
	want := map[string]string{
		"checker test 7": "it is expected WRONG_ANSWER, and the checker gives ACCEPTED, 100 of 100 points: ok the sum is 3",
		"checker test 8": "it is expected to pay 100 of 100 points, and the checker gives PARTIALLY_CORRECT, 50 of 100 points: " +
			"points 50 4 is one away from 3",
		"checker test 9": "it is expected PARTIAL, and the checker gives ACCEPTED, 0 of 0 points: ",
	}
	if len(said) != len(want) {
		t.Errorf("EO912 fired on %v", said)
	}
	for where, message := range want {
		if !strings.HasPrefix(said[where], message) {
			t.Errorf("%s: %q, not %q", where, said[where], message)
		}
	}
}

func TestACheckerTestIsReadStrictly(t *testing.T) {
	t.Parallel()
	checker := `"checker": {"source": "c.cpp"}, "testsets": [{"index": 1}], `
	for body, want := range map[string]string{
		checker + `"checkerTests": [{"output": "1", "expect": "ACCEPTED", "ouput": "1"}]}`:       `unknown field "ouput"`,
		checker + `"checkerTests": [{"Answer": "1", "expect": "ACCEPTED"}]}`:                     `the field "answer" is spelt "Answer"`,
		checker + `"checkerTests": [{"expect": {"Points": 1}}]}`:                                 `the field "points" is spelt "Points"`,
		checker + `"checkerTests": [{"expect": {"points": 1, "of": 2}}]}`:                        `unknown field "of"`,
		checker + `"checkerTests": [{"expect": {}}]}`:                                            `{"points": x} needs its x`,
		checker + `"checkerTests": [{"expect": 7}]}`:                                             `an expect is "ACCEPTED", "WRONG_ANSWER", "PARTIAL", "FAILURE" or {"points": x}`,
		checker + `"checkerTests": [{"expect": "PARTIALLY_CORRECT"}]}`:                           `checker test 1's expect is "PARTIALLY_CORRECT"; it is one of ACCEPTED, WRONG_ANSWER, PARTIAL, FAILURE, or {"points": x}`,
		checker + `"checkerTests": [{"output": "1"}]}`:                                           `checker test 1's expect is ""; it is one of`,
		checker + `"checkerTests": [{"expect": "ACCEPTED", "cost": -1}]}`:                        "checker test 1's cost is -1; a test is worth 0 or more",
		checker + `"checkerTests": [{"expect": "ACCEPTED", "cost": -0.0}]}`:                      "checker test 1's cost is -0; a test is worth 0 or more",
		checker + `"checkerTests": [{"expect": "ACCEPTED", "cost": 1e40}]}`:                      "checker test 1's cost is 1e+40, more than the judge's points can hold",
		checker + `"checkerTests": [{"expect": {"points": -2}}]}`:                                `checker test 1 expects {"points": -2}; a run pays 0 or more`,
		checker + `"checkerTests": [{"expect": {"points": -0}}]}`:                                `checker test 1 expects {"points": -0}; a run pays 0 or more`,
		checker + `"checkerTests": [{"expect": {"points": 101}}]}`:                               `checker test 1 expects {"points": 101}, more than its cost of 100; a run pays at most what the test is worth`,
		checker + `"checkerTests": [{"expect": {"points": 21}, "cost": 20}]}`:                    `checker test 1 expects {"points": 21}, more than its cost of 20`,
		checker + `"checkerTests": [{"expect": {"points": 1e40}}]}`:                              `checker test 1 expects {"points": 1e+40}, more than the judge's points can hold`,
		checker + `"checkerTests": [{"expect": "ACCEPTED"}, {"expect": "FAILURE", "group": 3}]}`: "checker test 2's group is 3, and the problem has no testset 3",
		`"checkerTests": [{"expect": "ACCEPTED"}]}`:                                              "the problem has checkerTests and no checker to run them",
	} {
		if err := loading(t, `{"type": "PROGRAM", `+body); err == nil || !strings.Contains(err.Error(), want) {
			t.Errorf("%s: %v, not %q", body, err, want)
		}
	}
	problem := loaded(t, `{"type": "PROGRAM", `+checker+`"checkerTests": [{"input": "1", "expect": {"points": 2.5}, "cost": 5, "group": 1}, {"expect": "WRONG_ANSWER"}]}`)
	tests := problem.CheckerTests
	if len(tests) != 2 || tests[0].Input != "1" || tests[0].Expect.Points == nil || *tests[0].Expect.Points != 2.5 ||
		*tests[0].Cost != 5 || *tests[0].Group != 1 || tests[1].Expect.Verdict != "WRONG_ANSWER" || tests[1].Cost != nil {
		t.Errorf("read %+v", tests)
	}
	body, err := json.Marshal(tests)
	if err != nil || !strings.Contains(string(body), `"expect":{"points":2.5}`) || !strings.Contains(string(body), `"expect":"WRONG_ANSWER"`) {
		t.Errorf("wrote %s (%v)", body, err)
	}
}

func TestACheckerTestPaysExactlyWhatItExpects(t *testing.T) {
	t.Parallel()
	twenty, zero := 20.0, 0.0
	for _, one := range []struct {
		want   *float64
		result RunResult
		breaks bool
	}{
		{&twenty, RunResult{Verdict: Partial, Cost: 40, Score: 20}, false},
		{&twenty, RunResult{Verdict: Accepted, Cost: 40, Score: 40}, true},
		{&twenty, RunResult{Verdict: Partial, Cost: 40, Score: 10}, true},
		{&zero, RunResult{Verdict: WrongAnswer, Cost: 40}, false},
		{&zero, RunResult{Verdict: Failure, Cost: 40}, true},
	} {
		why := checkerTestBreaks(&CheckerTest{Expect: CheckerExpect{Points: one.want}}, &one.result)
		if (why != "") != one.breaks {
			t.Errorf("expecting %g, %+v gave %q", *one.want, one.result, why)
		}
	}
}
