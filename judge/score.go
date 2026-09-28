package main

type Verdict string

const (
	Accepted    Verdict = "ACCEPTED"
	WrongAnswer Verdict = "WRONG_ANSWER"
	Partial     Verdict = "PARTIALLY_CORRECT"
	TimeLimit   Verdict = "TIME_LIMIT_EXCEEDED"
	RuntimeFail Verdict = "RUNTIME_ERROR"
	Failure     Verdict = "FAILURE"
	Skipped     Verdict = "SKIPPED"
)

const juryError = 3

type RunResult struct {
	Group    int
	Index    int
	Cost     Points
	Verdict  Verdict
	Fraction Points
	Score    Points
	Wall     int
	Message  string
	Warnings []Warning
}

type Points = float32

func verdictOfPoints(points, cost Points) Verdict {
	if points >= cost {
		return Accepted
	}
	return Partial
}

func scoreOfRun(verdict Verdict, cost, reported Points) Points {
	switch verdict {
	case Accepted:
		return cost
	case WrongAnswer, Partial:
		if reported < cost {
			return reported
		}
		return cost
	default:
		return 0
	}
}

type GroupResult struct {
	Index       int
	ScoringMode string
	Cost        Points
	Score       Points
	Verdict     Verdict
	Runs        []*RunResult
}

func summarizeGroup(index int, mode string, runs []*RunResult) *GroupResult {
	group := &GroupResult{Index: index, ScoringMode: mode, Cost: costOf(mode, runs), Verdict: Accepted, Runs: runs}

	scores := make([]Points, 0, len(runs))
	complete := true
	for _, run := range runs {
		if run.Verdict == Skipped {
			complete = false
			continue
		}
		scores = append(scores, run.Score)
		if run.Verdict != Accepted && group.Verdict == Accepted {
			group.Verdict = run.Verdict
		}
	}

	if len(scores) == 0 {
		group.Verdict = Skipped
		return group
	}

	switch mode {
	case "NO_SCORE":
	case "BEST":
		group.Score = scores[0]
		for _, one := range scores {
			if one > group.Score {
				group.Score = one
			}
		}
	case "WORST":
		if complete {
			group.Score = scores[0]
			for _, one := range scores {
				if one < group.Score {
					group.Score = one
				}
			}
		}
	case "ALL":
		if complete && group.Verdict == Accepted {
			for _, one := range scores {
				group.Score += one
			}
		}
	default:
		for _, one := range scores {
			group.Score += one
		}
	}

	return group
}

func costOf(mode string, runs []*RunResult) Points {
	if mode == "NO_SCORE" || len(runs) == 0 {
		return 0
	}
	cost := runs[0].Cost
	for _, run := range runs[1:] {
		switch mode {
		case "WORST":
			cost = min(cost, run.Cost)
		case "BEST":
			cost = max(cost, run.Cost)
		default:
			cost += run.Cost
		}
	}
	return cost
}

func summarizeSubmission(groups []*GroupResult) (Verdict, Points) {
	verdict := Accepted
	total := Points(0)
	for _, group := range groups {
		if group.Verdict == Skipped {
			continue
		}
		if verdict == Accepted && group.Verdict != Accepted {
			verdict = group.Verdict
		}
		total += group.Score
	}
	return verdict, total
}
