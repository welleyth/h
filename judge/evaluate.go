package main

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
)

type Attempt struct {
	Name    string
	Verdict Verdict
	Score   Points
	Groups  []*GroupResult
}

func (w *Workspace) Evaluate(ctx context.Context, name string, source *Program) (*Attempt, error) {
	built, err := w.Build(ctx, "solution."+name, source)
	if err != nil {
		return nil, err
	}

	checker, err := w.Build(ctx, "checker", w.Problem.Checker)
	if err != nil {
		return nil, err
	}

	var interactor *Built
	if w.Problem.Interactive() {
		if interactor, err = w.Build(ctx, "interactor", w.Problem.Interactor); err != nil {
			return nil, err
		}
	}

	gate := admissionFor(w.Problem)
	results := map[string]*RunResult{}

	waiting := w.plan()
	for len(waiting) > 0 {
		var blocked []*Planned
		for _, one := range waiting {
			switch gate.Admit(one) {
			case Blocked:
				blocked = append(blocked, one)
				continue
			case Rejected:
				results[reference(one)] = &RunResult{Group: one.Group, Index: one.Test.Index,
					Cost: Points(one.Test.Score), Verdict: Skipped}
				continue
			}

			result, err := w.judge(ctx, one, built, checker, interactor)
			if err != nil {
				return nil, err
			}
			results[reference(one)] = result
			gate.Notify(one, result)
		}

		if len(blocked) == len(waiting) {
			for _, one := range blocked {
				results[reference(one)] = &RunResult{Group: one.Group, Index: one.Test.Index,
					Cost: Points(one.Test.Score), Verdict: Skipped}
			}
			break
		}
		waiting = blocked
	}

	attempt := &Attempt{Name: name}
	for _, testset := range w.Problem.Testsets {
		var runs []*RunResult
		for _, test := range testset.Tests {
			if found, known := results[reference(&Planned{Group: testset.Index, Test: test})]; known {
				runs = append(runs, found)
			}
		}
		attempt.Groups = append(attempt.Groups, summarizeGroup(testset.Index, testset.ScoringMode, runs))
	}
	attempt.Verdict, attempt.Score = summarizeSubmission(attempt.Groups)
	return attempt, nil
}

func (w *Workspace) plan() []*Planned {
	var out []*Planned
	for _, testset := range w.Problem.Testsets {
		for _, test := range testset.Tests {
			out = append(out, &Planned{Group: testset.Index, Test: test})
		}
	}
	return out
}

func (w *Workspace) judge(ctx context.Context, one *Planned, solution, checker, interactor *Built) (*RunResult, error) {
	made := w.Tests[reference(one)]
	testset := w.Problem.Testset(one.Group)
	limit, _ := testset.Limit(w.Problem)

	result := &RunResult{Group: one.Group, Index: one.Test.Index, Cost: Points(one.Test.Score)}

	work := filepath.Join(w.Dir, "runs", solution.Name, fmt.Sprintf("%d-%d", one.Group, one.Test.Index))
	if err := os.MkdirAll(work, 0o755); err != nil {
		return nil, err
	}
	output := filepath.Join(work, "output.txt")

	var status, jury *Status
	var err error
	if interactor != nil {
		status, jury, err = w.interact(ctx, one, made, solution, interactor, work, output, limit)
	} else {
		status, err = w.batch(ctx, made, solution, work, output, limit)
	}
	if err != nil {
		return nil, err
	}
	if err := made.intact(solution.Name); err != nil {
		return nil, err
	}

	result.Wall = status.Wall
	result.Memory = status.Memory

	verdict := Accepted
	switch {
	case status.TimedOut:
		verdict = TimeLimit
	case status.Signal || status.ExitCode != 0:
		verdict = RuntimeFail
		result.Message = fmt.Sprintf("exit %d", status.ExitCode)
	}

	if verdict != Accepted {
		result.Verdict = verdict
		return result, nil
	}

	if jury != nil {
		said, _ := os.ReadFile(filepath.Join(work, "interactor.log"))
		result.Warnings = warningsIn("interactor", string(said))
		switch jury.ExitCode {
		case 0:
		case 1, 2:
			result.Verdict = WrongAnswer
			result.Message = firstLine(string(said))
			return result, nil
		default:
			result.Verdict = Failure
			result.Message = firstLine(string(said))
			return result, nil
		}
	}

	return w.check(ctx, one, made, checker, work, output, result)
}

func (w *Workspace) batch(ctx context.Context, made *Prepared, solution *Built, work, output string, limit int) (*Status, error) {
	input, err := os.Open(made.Input)
	if err != nil {
		return nil, err
	}
	defer input.Close()

	file, err := os.Create(output)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	alone, err := os.MkdirTemp(w.Temp, "eo-judge-run-")
	if err != nil {
		return nil, err
	}
	defer os.RemoveAll(alone)
	return run(ctx, solution.Exe, Invocation{Dir: alone, Stdin: input, Stdout: file, LimitMS: limit})
}

func (w *Workspace) check(ctx context.Context, one *Planned, made *Prepared, checker *Built,
	work, output string, result *RunResult) (*RunResult, error) {
	log := filepath.Join(work, "checker.log")
	file, err := os.Create(log)
	if err != nil {
		return nil, err
	}

	status, err := run(ctx, checker.Exe, Invocation{
		Args: []string{made.Input, output, made.Answer}, Dir: work, Stdout: file, Stderr: file,
		LimitMS: checkerLimit,
		Env: map[string]string{
			"EOLYMP": "1", "INPUT_FILE": made.Input, "OUTPUT_FILE": output, "ANSWER_FILE": made.Answer,
			"TEST_ID": reference(one), "TEST_COST": fmt.Sprint(one.Test.Score),
			"TEST_INDEX": fmt.Sprint(one.Test.Index), "TEST_GROUP": fmt.Sprint(one.Group),
		},
	})
	file.Close()
	if err != nil {
		return nil, err
	}

	said, _ := os.ReadFile(log)
	result.Message = firstLine(string(said))
	result.Warnings = warningsIn("checker", string(said))

	switch status.ExitCode {
	case 0:
		result.Verdict = Accepted
	case 1, 2:
		result.Verdict = WrongAnswer
	case 7:
		points, err := readPoints(bytes.NewReader(said))
		if err != nil {
			result.Verdict = Failure
			result.Message = err.Error()
			return result, nil
		}
		result.Fraction = points
		result.Verdict = verdictOfPoints(points, result.Cost)
	default:
		result.Verdict = Failure
	}

	result.Score = scoreOfRun(result.Verdict, result.Cost, result.Fraction)
	if result.Verdict == Accepted {
		result.Fraction = result.Cost
	}
	return result, nil
}

func (w *Workspace) interact(ctx context.Context, one *Planned, made *Prepared, solution, interactor *Built,
	work, output string, limit int) (*Status, *Status, error) {
	input := made.Input
	var last, lastJury *Status

	for phase := 1; phase <= w.Problem.RunCount; phase++ {
		summary := output
		if phase < w.Problem.RunCount {
			summary = filepath.Join(work, fmt.Sprintf("handoff-%d.txt", phase))
		}

		status, jury, err := w.onePhase(ctx, input, summary, solution, interactor, work, limit,
			w.metadata(one), w.answerFor(made))
		if err != nil {
			return nil, nil, err
		}
		last, lastJury = status, jury
		if status.TimedOut || status.Signal || status.ExitCode != 0 || jury.ExitCode != 0 {
			return status, jury, nil
		}
		input = summary
	}
	return last, lastJury, nil
}

func (w *Workspace) metadata(one *Planned) map[string]string {
	if one == nil {
		return map[string]string{"EOLYMP": "1"}
	}
	return map[string]string{
		"EOLYMP": "1", "TEST_ID": reference(one), "TEST_COST": fmt.Sprint(one.Test.Score),
		"TEST_INDEX": fmt.Sprint(one.Test.Index), "TEST_GROUP": fmt.Sprint(one.Group),
	}
}

func (w *Workspace) answerFor(made *Prepared) string {
	if made == nil || (made.Test.Answer == "" && made.Test.AnswerGenerator == "") {
		return ""
	}
	return made.Answer
}

func (w *Workspace) onePhase(ctx context.Context, input, summary string, solution, interactor *Built,
	work string, limit int, env map[string]string, answer string) (*Status, *Status, error) {
	toJury, fromPlayer, err := os.Pipe()
	if err != nil {
		return nil, nil, err
	}
	toPlayer, fromJury, err := os.Pipe()
	if err != nil {
		return nil, nil, err
	}

	juryLog, err := os.Create(filepath.Join(work, "interactor.log"))
	if err != nil {
		return nil, nil, err
	}
	defer juryLog.Close()

	arguments := []string{input, summary}
	if answer != "" {
		arguments = append(arguments, answer)
	}

	alone, err := os.MkdirTemp(w.Temp, "eo-judge-run-")
	if err != nil {
		return nil, nil, err
	}
	defer os.RemoveAll(alone)

	done := make(chan *Status, 1)
	go func() {
		status, _ := run(ctx, interactor.Exe, Invocation{
			Args: arguments, Dir: work, Stdin: toJury, Stdout: fromJury, Stderr: juryLog,
			LimitMS: limit + 1000,
			Env:     env,
		})
		toJury.Close()
		fromJury.Close()
		done <- status
	}()

	status, err := run(ctx, solution.Exe, Invocation{
		Dir: alone, Stdin: toPlayer, Stdout: fromPlayer, Stderr: io.Discard, LimitMS: limit,
	})
	toPlayer.Close()
	fromPlayer.Close()
	jury := <-done

	if err != nil {
		return nil, nil, err
	}
	if jury == nil {
		jury = &Status{}
	}
	return status, jury, nil
}
