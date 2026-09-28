package main

import "testing"

func fired(found Findings, code string) bool {
	for _, one := range found {
		if one.Code == code {
			return true
		}
	}
	return false
}

func TestConfigurationChecks(t *testing.T) {
	t.Parallel()
	problem := &Problem{
		Type: "PROGRAM", RunCount: 3, TimeLimit: 0,
		Testsets: []*Testset{
			{Index: 0, ScoringMode: "EACH", Tests: []*Test{{Index: 1, Score: 5}}},
			{Index: 1, ScoringMode: "EACH", FeedbackPolicy: "ICPC", Dependencies: []int{1},
				Tests: []*Test{{Index: 1, Score: 20}}},
			{Index: 2, ScoringMode: "WORST",
				Tests: []*Test{{Index: 1, Score: 30}, {Index: 2, Score: 10}}},
		},
	}

	found := Configuration(problem)
	for _, code := range []string{"EO901", "EO904", "EO905", "EO906", "EO907", "EO908"} {
		if !fired(found, code) {
			t.Errorf("%s did not fire", code)
		}
	}
	if fired(found, "EO902") {
		t.Error("EO902 is for interactive problems only")
	}
}

func TestAnEachTestsetWithExpandedIcpcFeedbackIsFlagged(t *testing.T) {
	t.Parallel()
	problem := &Problem{Type: "PROGRAM", RunCount: 1, Testsets: []*Testset{
		{Index: 1, ScoringMode: "EACH", FeedbackPolicy: "ICPC_EXPANDED", Tests: []*Test{{Index: 1, Score: 100}}},
	}}
	if !fired(Configuration(problem), "EO901") {
		t.Fatal("EO901 did not fire")
	}
}

func TestAnInteractiveProblemNeedsAWallLimit(t *testing.T) {
	t.Parallel()
	problem := &Problem{Type: "INTERACTIVE", RunCount: 1}
	if !fired(Configuration(problem), "EO902") {
		t.Fatal("EO902 did not fire")
	}
	problem.TimeLimit = 1000
	if fired(Configuration(problem), "EO902") {
		t.Fatal("EO902 fired with a wall limit set")
	}
}

func TestTooManyTestRows(t *testing.T) {
	t.Parallel()
	testset := &Testset{Index: 1, ScoringMode: "EACH"}
	for at := 1; at <= 1300; at++ {
		testset.Tests = append(testset.Tests, &Test{Index: at})
	}
	if !fired(Configuration(&Problem{Type: "PROGRAM", Testsets: []*Testset{testset}}), "EO909") {
		t.Fatal("EO909 did not fire")
	}
}
