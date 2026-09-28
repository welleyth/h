package main

import (
	"fmt"
	"strings"
)

var passes = map[string][]Verdict{
	"WRONG_ANSWER":        {Accepted, WrongAnswer, Partial},
	"TIMEOUT":             {Accepted, TimeLimit},
	"TIMEOUT_OR_ACCEPTED": {Accepted, TimeLimit},
}

var needs = map[string][]Verdict{
	"WRONG_ANSWER": {WrongAnswer, Partial},
	"TIMEOUT":      {TimeLimit},
	"FAILURE":      {Failure},
}

func unchecked(kind string) string {
	switch kind {
	case "OVERFLOW", "OVERFLOW_OR_ACCEPTED":
		return fmt.Sprintf("%s is not checked, because eo-judge does not measure memory", kind)
	}
	return ""
}

func breaks(solution *Solution, attempt *Attempt) string {
	kind := solution.Type
	switch {
	case kind == "" || kind == "DONT_RUN" || unchecked(kind) != "":
		return ""
	case solution.Scores != "":
		if why := scoresBreak(solution, attempt); why != "" {
			return why
		}
	case kind == "CORRECT" && (attempt.Verdict != Accepted || attempt.Score != 100):
		return fmt.Sprintf("it ends %s at %g, not ACCEPTED at 100", attempt.Verdict, attempt.Score)
	}
	if kind == "INCORRECT" && attempt.Verdict == Accepted {
		return "every run was accepted"
	}
	return runsBreak(kind, attempt)
}

func scoresBreak(solution *Solution, attempt *Attempt) string {
	want, parsed := solution.Expected()
	if !parsed {
		return fmt.Sprintf("its scores are %q, which is not a number", solution.Scores)
	}
	if attempt.Score != Points(want) {
		return fmt.Sprintf("it scores %g, and its scores say %g", attempt.Score, want)
	}
	return ""
}

func runsBreak(kind string, attempt *Attempt) string {
	allowed, limited := passes[kind]
	seen := map[Verdict]bool{}
	for _, group := range attempt.Groups {
		for _, run := range group.Runs {
			seen[run.Verdict] = true
			if limited && run.Verdict != Skipped && !among(run.Verdict, allowed) {
				return fmt.Sprintf("test %d:%d is %s", run.Group, run.Index, run.Verdict)
			}
		}
	}
	if wanted := needs[kind]; len(wanted) > 0 && !anyOf(seen, wanted) {
		return fmt.Sprintf("no run is %s", eitherOf(wanted))
	}
	return ""
}

func among(verdict Verdict, list []Verdict) bool {
	for _, one := range list {
		if one == verdict {
			return true
		}
	}
	return false
}

func anyOf(seen map[Verdict]bool, list []Verdict) bool {
	for _, one := range list {
		if seen[one] {
			return true
		}
	}
	return false
}

func eitherOf(list []Verdict) string {
	names := make([]string, len(list))
	for at, one := range list {
		names[at] = string(one)
	}
	return strings.Join(names, " or ")
}
