package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
)

func (w *Workspace) authoredDir() (string, error) {
	dir := filepath.Join(w.Dir, "authored")
	return dir, os.MkdirAll(dir, 0o755)
}

func (w *Workspace) validatorTestChecks(ctx context.Context, found *Findings) error {
	if len(w.Problem.ValidatorTests) == 0 {
		return nil
	}
	built, err := w.Build(ctx, "validator", w.Problem.Validator)
	if err != nil {
		return err
	}
	dir, err := w.authoredDir()
	if err != nil {
		return err
	}
	for at, test := range w.Problem.ValidatorTests {
		where := fmt.Sprintf("validator test %d", at+1)
		body := []byte{}
		if test.Input != nil {
			body = []byte(*test.Input)
		} else if body, err = os.ReadFile(w.Problem.Path(test.File)); err != nil {
			return fmt.Errorf("%s: %w", where, err)
		}
		path := filepath.Join(dir, fmt.Sprintf("validator-%d.txt", at+1))
		if err := os.WriteFile(path, normalise(body), 0o644); err != nil {
			return err
		}
		expected := "it is expected " + test.Expect
		var flags []string
		if test.Group != nil {
			flags = []string{"--group", fmt.Sprint(*test.Group)}
			expected += fmt.Sprintf(" in testset %d", *test.Group)
		}
		status, err := validating(ctx, built, path, flags...)
		if err != nil {
			return err
		}
		fix := "the validator and the test disagree; fix the validator, or the test's expect if the validator is right"
		switch {
		case status.TimedOut:
			found.warn("EO911", where, fmt.Sprintf("%s, and the validator did not finish in %d s", expected,
				validatorLimit/1000), fix)
		case test.Expect == "VALID" && status.ExitCode != 0:
			found.warn("EO911", where, fmt.Sprintf("%s, and the validator refuses it: %s", expected,
				firstLine(string(status.Stdout)+string(status.Stderr))), fix)
		case test.Expect == "INVALID" && status.ExitCode == 0:
			found.warn("EO911", where, expected+", and the validator accepts it", fix)
		}
	}
	return nil
}

func (w *Workspace) checkerTestChecks(ctx context.Context, found *Findings) error {
	if len(w.Problem.CheckerTests) == 0 {
		return nil
	}
	checker, err := w.Build(ctx, "checker", w.Problem.Checker)
	if err != nil {
		return err
	}
	dir, err := w.authoredDir()
	if err != nil {
		return err
	}
	for at, test := range w.Problem.CheckerTests {
		work := filepath.Join(dir, fmt.Sprintf("checker-%d", at+1))
		if err := os.MkdirAll(work, 0o755); err != nil {
			return err
		}
		made := &Prepared{Input: filepath.Join(work, "input.txt"), Answer: filepath.Join(work, "answer.txt")}
		output := filepath.Join(work, "output.txt")
		for path, body := range map[string]string{made.Input: test.Input, made.Answer: test.Answer, output: test.Output} {
			if err := os.WriteFile(path, []byte(body), 0o644); err != nil {
				return err
			}
		}
		cost, group := 100.0, 0
		if test.Cost != nil {
			cost = *test.Cost
		}
		if test.Group != nil {
			group = *test.Group
		}
		status, said, err := runChecker(ctx, checker, made, output, work, map[string]string{
			"EOLYMP": "1", "TEST_ID": "", "TEST_COST": fmt.Sprint(cost), "TEST_INDEX": fmt.Sprint(at + 1),
			"TEST_GROUP": fmt.Sprint(group),
		})
		if err != nil {
			return err
		}
		result := &RunResult{Cost: Points(cost), Message: firstLine(string(said))}
		checked(result, status, said)
		if status.TimedOut {
			result.Message = fmt.Sprintf("it did not finish in %d s", checkerLimit/1000)
		}
		if why := checkerTestBreaks(test, result); why != "" {
			found.warn("EO912", fmt.Sprintf("checker test %d", at+1), why,
				"the checker and the test disagree; fix the checker, or the test's expect if the checker is right")
		}
	}
	return nil
}

func checkerTestBreaks(test *CheckerTest, result *RunResult) string {
	gives := fmt.Sprintf("the checker gives %s, %g of %g points", result.Verdict, result.Score, result.Cost)
	if result.Message != "" {
		gives += ": " + result.Message
	}
	if want := test.Expect.Points; want != nil {
		if result.Verdict == Failure || result.Score != Points(*want) {
			return fmt.Sprintf("it is expected to pay %g of %g points, and %s", *want, result.Cost, gives)
		}
		return ""
	}
	verdict := Verdict(test.Expect.Verdict)
	if verdict == "PARTIAL" {
		verdict = Partial
	}
	if result.Verdict != verdict {
		return fmt.Sprintf("it is expected %s, and %s", test.Expect.Verdict, gives)
	}
	return ""
}
