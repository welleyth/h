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
