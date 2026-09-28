package main

import "testing"

func runs(verdicts ...Verdict) []*RunResult {
	var out []*RunResult
	for at, verdict := range verdicts {
		one := &RunResult{Group: 1, Index: at + 1, Cost: 10, Verdict: verdict}
		if verdict == WrongAnswer {
			one.Fraction = 4
		}
		one.Score = scoreOfRun(verdict, one.Cost, one.Fraction)
		out = append(out, one)
	}
	return out
}

func TestScoreOfRun(t *testing.T) {
	t.Parallel()
	if got := scoreOfRun(Accepted, 10, 0); got != 10 {
		t.Fatalf("accepted pays the whole cost, got %v", got)
	}
	if got := scoreOfRun(WrongAnswer, 10, 4); got != 4 {
		t.Fatalf("a fraction pays itself, got %v", got)
	}
	if got := scoreOfRun(WrongAnswer, 10, 40); got != 10 {
		t.Fatalf("a fraction above the cost is clamped, got %v", got)
	}
	if got := scoreOfRun(TimeLimit, 10, 9); got != 0 {
		t.Fatalf("anything else pays nothing, got %v", got)
	}
	if got := scoreOfRun(Partial, 10, 4); got != 4 {
		t.Fatalf("a partial pays its points, got %v", got)
	}
}

func TestPointsDecideTheVerdictTheWayTheAgentDoes(t *testing.T) {
	t.Parallel()
	for _, one := range []struct {
		points, cost Points
		want         Verdict
	}{
		{4, 10, Partial},
		{0, 10, Partial},
		{10, 10, Accepted},
		{12, 10, Accepted},
		{0, 0, Accepted},
		{0.5, 0, Accepted},
	} {
		if got := verdictOfPoints(one.points, one.cost); got != one.want {
			t.Errorf("points %v of %v is %v, the agent says %v", one.points, one.cost, got, one.want)
		}
	}
}

func TestAPartialOnAZeroCostTestLetsAnAllGroupPay(t *testing.T) {
	t.Parallel()
	first := &RunResult{Cost: 10, Verdict: Accepted, Score: 10}
	free := &RunResult{Cost: 0, Verdict: verdictOfPoints(0, 0)}
	group := summarizeGroup(1, "ALL", []*RunResult{first, free})
	if group.Verdict != Accepted || group.Score != 10 {
		t.Fatalf("the judge pays this group 10 as ACCEPTED, got %v at %v", group.Verdict, group.Score)
	}

	partial := &RunResult{Cost: 10, Verdict: Partial, Score: 7}
	group = summarizeGroup(1, "ALL", []*RunResult{partial, free})
	if group.Verdict != Partial || group.Score != 0 {
		t.Fatalf("a partial on a costed test pays an ALL group nothing, got %v at %v", group.Verdict, group.Score)
	}
}

func TestGroupScoring(t *testing.T) {
	t.Parallel()
	for _, one := range []struct {
		mode string
		want Points
	}{
		{"EACH", 24},
		{"ALL", 0},
		{"BEST", 10},
		{"WORST", 4},
		{"NO_SCORE", 0},
	} {
		t.Run(one.mode, func(t *testing.T) {
			group := summarizeGroup(1, one.mode, runs(Accepted, WrongAnswer, Accepted))
			if group.Score != one.want {
				t.Fatalf("%s gave %v, want %v", one.mode, group.Score, one.want)
			}
		})
	}
}

func TestAllPaysWhenEveryRunPasses(t *testing.T) {
	t.Parallel()
	group := summarizeGroup(1, "ALL", runs(Accepted, Accepted))
	if group.Score != 20 || group.Verdict != Accepted {
		t.Fatalf("ALL gave %v at %v", group.Verdict, group.Score)
	}
}

func TestWorstShowsNothingUntilEveryRunIsIn(t *testing.T) {
	t.Parallel()
	list := runs(Accepted, Accepted)
	list = append(list, &RunResult{Group: 1, Index: 3, Cost: 10, Verdict: Skipped})
	group := summarizeGroup(1, "WORST", list)
	if group.Score != 0 {
		t.Fatalf("WORST scored %v before the group finished", group.Score)
	}
}

func TestSubmissionTakesTheFirstFailingVerdict(t *testing.T) {
	t.Parallel()
	groups := []*GroupResult{
		summarizeGroup(1, "EACH", runs(Accepted)),
		summarizeGroup(2, "EACH", runs(TimeLimit)),
		summarizeGroup(3, "EACH", runs(WrongAnswer)),
	}
	verdict, total := summarizeSubmission(groups)
	if verdict != TimeLimit {
		t.Fatalf("got %v, want the first failing group's verdict", verdict)
	}
	if total != 14 {
		t.Fatalf("total %v, want 14", total)
	}
}

func TestATestsetCostsWhatItsModeCanPay(t *testing.T) {
	t.Parallel()
	costs := func(values ...Points) []*RunResult {
		var out []*RunResult
		for at, one := range values {
			out = append(out, &RunResult{Group: 1, Index: at + 1, Cost: one, Verdict: Accepted, Score: one})
		}
		return out
	}
	for _, one := range []struct {
		mode string
		runs []*RunResult
		cost Points
	}{
		{"EACH", costs(10, 20, 30), 60},
		{"ALL", costs(10, 20, 30), 60},
		{"WORST", costs(20, 20), 20},
		{"WORST", costs(30, 10), 10},
		{"BEST", costs(10, 25), 25},
		{"NO_SCORE", costs(10, 20), 0},
		{"EACH", nil, 0},
	} {
		group := summarizeGroup(1, one.mode, one.runs)
		if group.Cost != one.cost {
			t.Errorf("%s over %d runs costs %g, not %g", one.mode, len(one.runs), group.Cost, one.cost)
		}
		if one.runs != nil && group.Verdict == Accepted && group.Score != group.Cost {
			t.Errorf("%s pays %g for every run accepted, and costs %g", one.mode, group.Score, group.Cost)
		}
	}
}
