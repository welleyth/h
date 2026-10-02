package main

import (
	"reflect"
	"regexp"
	"strings"
	"testing"
)

func TestDescribePrintsOneLinePerTest(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "describe", "testdata/features")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	lines := strings.Split(strings.TrimSuffix(out, "\n"), "\n")
	if len(lines) != 5 {
		t.Fatalf("printed %d lines:\n%s", len(lines), out)
	}
	for at, want := range []string{
		`0:1    10 B  path,star  n=3 edge.max_degree=2 edge.leaves=2 edge.depth=1 edge.diameter=2`,
		`1:1    18 B  path       n=5 edge.max_degree=2 edge.leaves=2 edge.depth=4 edge.diameter=4`,
		`1:2    22 B  star       n=6 edge.max_degree=5 edge.leaves=5 edge.depth=1 edge.diameter=2`,
		`2:1  [0-9.]+ KB  -          n=1000 edge.max_degree=[0-9]+ edge.leaves=[0-9]+ edge.depth=[0-9]+ edge.diameter=[0-9]+`,
		`2:2  [0-9.]+ KB  path       n=1000 edge.max_degree=2 edge.leaves=2 edge.depth=999 edge.diameter=999`,
	} {
		if !regexp.MustCompile("^"+regexp.QuoteMeta(want)+"$").MatchString(lines[at]) &&
			!regexp.MustCompile("^"+want+"$").MatchString(lines[at]) {
			t.Errorf("line %d is %q, not %q", at+1, lines[at], want)
		}
	}
}

func TestDescribeReportsEveryTestInJSON(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "describe", "--json", "testdata/features")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	got := decoded(t, out)
	if got.Tests == nil || len(*got.Tests) != 5 || got.Exit != 0 {
		t.Fatalf("reported %+v", got)
	}
	want := testDescription{Group: 1, Test: 2, Bytes: 22, Features: []string{"star"},
		Stats: map[string]int64{"n": 6, "edge.max_degree": 5, "edge.leaves": 5, "edge.depth": 1, "edge.diameter": 2}}
	if !reflect.DeepEqual((*got.Tests)[2], want) {
		t.Errorf("test 1:2 is %+v, not %+v", (*got.Tests)[2], want)
	}
	if got.Declared == nil || !reflect.DeepEqual(*got.Declared, []string{"caterpillar", "path", "star"}) {
		t.Errorf("declared is %v", got.Declared)
	}
	_, out, _ = invokeIn(t.TempDir(), "describe", "--json", "testdata/extremes")
	if got = decoded(t, out); got.Declared == nil || len(*got.Declared) != 0 || !strings.Contains(out, `"declared": []`) ||
		!strings.Contains(out, `"features": []`) {
		t.Errorf("a validator that declares no feature reported %s", out)
	}
	blind := relocated(t, "testdata/extremes", func(problem *Problem) { problem.Validator = nil })
	_, out, _ = invokeIn(t.TempDir(), "describe", "--json", blind)
	if got = decoded(t, out); got.Declared != nil || strings.Contains(out, "declared") ||
		!strings.Contains(out, `"features": null`) {
		t.Errorf("a problem with no validator reported %s", out)
	}
	empty := relocated(t, "testdata/extremes", func(problem *Problem) { problem.Testsets = nil })
	code, out, _ = invokeIn(t.TempDir(), "describe", "--json", empty)
	if got = decoded(t, out); code != 0 || got.Tests == nil || len(*got.Tests) != 0 || !strings.Contains(out, `"tests": []`) {
		t.Errorf("a problem with no tests exited %d and reported %s", code, out)
	}
}

func TestDescribeShowsAnInvalidTestAndAProblemWithNoValidator(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	wide := relocated(t, "testdata/extremes", func(problem *Problem) {
		problem.Testsets[0].Tests[1].Generator.Arguments = []string{"-n=2000"}
	})
	code, out, errs := invokeIn(t.TempDir(), "describe", wide)
	if code != 0 || out != "1:1  2 B  n=1\n1:2  5 B  invalid: line 1, n: 2000 is above 1000\n" {
		t.Errorf("exit %d, printed %q, said %q", code, out, errs)
	}
	blind := relocated(t, "testdata/extremes", func(problem *Problem) { problem.Validator = nil })
	code, out, errs = invokeIn(t.TempDir(), "describe", blind)
	if code != 0 || out != "1:1  2 B\n1:2  5 B\n" {
		t.Errorf("without a validator: exit %d, printed %q, said %q", code, out, errs)
	}
	code, _, errs = invoke("describe", "--solution", "echo", "testdata/extremes")
	if code != 2 || !strings.Contains(errs, "flag provided but not defined: -solution") {
		t.Errorf("describe --solution exited %d, said %q", code, errs)
	}
}

func TestDescribeGoesOnPastABrokenGeneratorAndABrokenValidator(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := relocated(t, "testdata/features", func(problem *Problem) {
		problem.Testsets[1].Tests[0].Generator.Arguments = []string{"-n=1", "-shape=path"}
		problem.Validator.Source = strings.TrimSuffix(problem.Validator.Source, "validator.cpp") + "crashing.cpp"
	})
	code, out, errs := invokeIn(t.TempDir(), "describe", dir)
	lines := strings.Split(strings.TrimSuffix(out, "\n"), "\n")
	if code != 3 || len(lines) != 5 || !strings.Contains(errs, "3 test(s) could not be generated, or the validator broke on them") {
		t.Fatalf("exit %d, printed %q, said %q", code, out, errs)
	}
	if !strings.HasPrefix(lines[1], "1:1       -  not generated: the generator gen -n=1 -shape=path exited 3: -n=1 is below 2") {
		t.Errorf("the test that was not generated reads %q", lines[1])
	}
	if !strings.HasPrefix(lines[2], "1:2    22 B  n=6 edge.max_degree=5") {
		t.Errorf("the test after it reads %q", lines[2])
	}
	if !strings.HasPrefix(lines[3], "2:1  ") || !strings.Contains(lines[3], "  validator broke: eolymp.h: ") ||
		!strings.Contains(lines[3], `saw("caterpillar") names a feature that was never declared`) {
		t.Errorf("the test the validator broke on reads %q", lines[3])
	}
}
