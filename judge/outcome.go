package main

import (
	"encoding/json"
	"io"
)

type outcome struct {
	Version  string             `json:"version"`
	Problem  string             `json:"problem"`
	Invalid  []invalidTest      `json:"invalid,omitempty"`
	Attempts []attemptResult    `json:"attempts"`
	Stress   *stressReport      `json:"stress,omitempty"`
	Coverage *coverageReport    `json:"coverage,omitempty"`
	Declared *[]string          `json:"declared,omitempty"`
	Tests    *[]testDescription `json:"tests,omitempty"`
	Findings []findingResult    `json:"findings"`
	Exit     int                `json:"exit"`
	Error    string             `json:"error,omitempty"`
}

type invalidTest struct {
	Group int    `json:"group"`
	Test  int    `json:"test"`
	Why   string `json:"why"`
}

type attemptResult struct {
	Name    string        `json:"name"`
	Type    string        `json:"type"`
	Verdict Verdict       `json:"verdict"`
	Score   Points        `json:"score"`
	Groups  []groupResult `json:"groups"`
	Breaks  string        `json:"breaks,omitempty"`
}

type groupResult struct {
	Index   int         `json:"index"`
	Verdict Verdict     `json:"verdict"`
	Score   Points      `json:"score"`
	Cost    Points      `json:"cost"`
	Runs    []runResult `json:"runs"`
}

type runResult struct {
	Test       int      `json:"test"`
	Verdict    Verdict  `json:"verdict"`
	MS         int      `json:"ms"`
	Message    string   `json:"message"`
	Transcript []string `json:"transcript,omitempty"`
}

type findingResult struct {
	Code    string `json:"code"`
	Level   string `json:"level"`
	Where   string `json:"where"`
	Message string `json:"message"`
	Fix     string `json:"fix"`
}

func newOutcome(problem string) *outcome {
	return &outcome{Version: version, Problem: problem, Attempts: []attemptResult{}, Findings: []findingResult{}}
}

func (o *outcome) attempt(solution *Solution, attempt *Attempt) {
	one := attemptResult{Name: attempt.Name, Type: solution.Type, Verdict: attempt.Verdict, Score: attempt.Score,
		Groups: []groupResult{}}
	for _, group := range attempt.Groups {
		summary := groupResult{Index: group.Index, Verdict: group.Verdict, Score: group.Score, Cost: group.Cost,
			Runs: []runResult{}}
		for _, run := range group.Runs {
			summary.Runs = append(summary.Runs,
				runResult{Test: run.Index, Verdict: run.Verdict, MS: run.Wall, Message: run.Message,
					Transcript: run.Transcript})
		}
		one.Groups = append(one.Groups, summary)
	}
	o.Attempts = append(o.Attempts, one)
}

func (o *outcome) findings(kept Findings) {
	for _, one := range kept {
		o.Findings = append(o.Findings, findingResult{Code: one.Code, Level: one.Severity, Where: one.Where,
			Message: one.Message, Fix: one.Fix})
	}
}

func (o *outcome) write(out io.Writer) error {
	body, err := json.MarshalIndent(o, "", "  ")
	if err != nil {
		return err
	}
	_, err = out.Write(append(body, '\n'))
	return err
}
