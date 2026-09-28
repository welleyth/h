package main

import "testing"

func planOf(group int, count int) []*Planned {
	var out []*Planned
	for at := 1; at <= count; at++ {
		out = append(out, &Planned{Group: group, Test: &Test{Index: at, Score: 10}})
	}
	return out
}

func TestIcpcStopsAfterATestWorthNothing(t *testing.T) {
	t.Parallel()
	gate := &showstopper{group: 1}
	plan := planOf(1, 3)

	gate.Notify(plan[0], &RunResult{Verdict: Accepted, Score: 10})
	if gate.Admit(plan[1]) != Admitted {
		t.Fatal("an accepted test must not stop the group")
	}

	gate.Notify(plan[1], &RunResult{Verdict: Partial, Score: 4})
	if gate.Admit(plan[2]) != Admitted {
		t.Fatal("a partial score must not stop the group")
	}

	gate.Notify(plan[2], &RunResult{Verdict: Partial, Score: 0})
	if gate.Admit(plan[0]) != Rejected {
		t.Fatal("a scored zero stops the rest")
	}

	wrong := &showstopper{group: 1}
	wrong.Notify(plan[0], &RunResult{Verdict: WrongAnswer})
	if wrong.Admit(plan[1]) != Rejected {
		t.Fatal("a wrong answer stops the rest")
	}
}

func TestIcpcLeavesOtherGroupsAlone(t *testing.T) {
	t.Parallel()
	gate := &showstopper{group: 1}
	gate.Notify(planOf(1, 1)[0], &RunResult{Verdict: RuntimeFail})
	if gate.Admit(planOf(2, 1)[0]) != Admitted {
		t.Fatal("the stop belongs to one group")
	}
}

func TestIcpcExpandedStopsLikeIcpc(t *testing.T) {
	t.Parallel()
	problem := &Problem{Testsets: []*Testset{{Index: 1, FeedbackPolicy: "ICPC_EXPANDED"}}}
	gate := admissionFor(problem)
	plan := planOf(1, 2)
	gate.Notify(plan[0], &RunResult{Verdict: WrongAnswer})
	if gate.Admit(plan[1]) != Rejected {
		t.Fatal("ICPC_EXPANDED stops after a wrong answer")
	}
}

func TestDependencyBlocksUntilTheBlockersPass(t *testing.T) {
	t.Parallel()
	first := planOf(1, 2)
	gate := &dependency{group: 2, mode: "FULLY_ACCEPTED", blockers: map[string]bool{
		reference(first[0]): true, reference(first[1]): true,
	}}

	later := planOf(2, 1)[0]
	if gate.Admit(later) != Blocked {
		t.Fatal("a dependent group waits")
	}
	gate.Notify(first[0], &RunResult{Verdict: Accepted})
	if gate.Admit(later) != Blocked {
		t.Fatal("it waits for every blocker")
	}
	gate.Notify(first[1], &RunResult{Verdict: Accepted})
	if gate.Admit(later) != Admitted {
		t.Fatal("every blocker passed, so it runs")
	}
}

func TestDependencyRejectsWhenABlockerFails(t *testing.T) {
	t.Parallel()
	first := planOf(1, 1)
	gate := &dependency{group: 2, mode: "FULLY_ACCEPTED",
		blockers: map[string]bool{reference(first[0]): true}}
	gate.Notify(first[0], &RunResult{Verdict: WrongAnswer, Score: 5})
	if gate.Admit(planOf(2, 1)[0]) != Rejected {
		t.Fatal("FULLY_ACCEPTED wants every blocker accepted")
	}
}

func TestFirstPointUnblocksOnAPartialScore(t *testing.T) {
	t.Parallel()
	first := planOf(1, 1)
	gate := &dependency{group: 2, mode: "FIRST_POINT",
		blockers: map[string]bool{reference(first[0]): true}}
	gate.Notify(first[0], &RunResult{Verdict: Partial, Score: 0})
	if gate.Admit(planOf(2, 1)[0]) != Blocked {
		t.Fatal("a scored zero is not a first point")
	}
	gate.Notify(first[0], &RunResult{Verdict: Partial, Score: 5})
	if gate.Admit(planOf(2, 1)[0]) != Admitted {
		t.Fatal("one point is enough under FIRST_POINT")
	}
}
