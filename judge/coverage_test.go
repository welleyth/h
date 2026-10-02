package main

import (
	"context"
	"reflect"
	"strings"
	"testing"
)

func TestCheckPrintsWhichTestHasWhichFeatureUnderV(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "check", "-v", "testdata/features")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	table := "features of testset 0  caterpillar  path  star\n" +
		"  0:1                  .            x     x\n" +
		"\n" +
		"features of testset 1  caterpillar  path  star\n" +
		"  1:1                  .            x     .\n" +
		"  1:2                  .            .     x\n" +
		"\n" +
		"features of testset 2  caterpillar  path  star\n" +
		"  2:1                  .            .     .\n" +
		"  2:2                  .            x     .\n" +
		"\n"
	if !strings.HasPrefix(out, table) {
		t.Errorf("check -v printed\n%s\nwhich does not start with\n%s", out, table)
	}
	code, plain, _ := invokeIn(t.TempDir(), "check", "testdata/features")
	if code != 0 || strings.Contains(plain, "features of testset") || plain != strings.TrimPrefix(out, table) {
		t.Errorf("check without -v printed\n%s", plain)
	}
}

func TestCheckReportsTheFeaturesOfEveryTestInJSON(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	code, out, errs := invokeIn(t.TempDir(), "check", "--json", "testdata/features")
	if code != 0 {
		t.Fatalf("exit %d, said %q", code, errs)
	}
	got := decoded(t, out)
	want := &coverageReport{
		Features: []string{"caterpillar", "path", "star"},
		Tests: []testFeatures{
			{Group: 0, Test: 1, Features: []string{"path", "star"}},
			{Group: 1, Test: 1, Features: []string{"path"}},
			{Group: 1, Test: 2, Features: []string{"star"}},
			{Group: 2, Test: 1, Features: []string{}},
			{Group: 2, Test: 2, Features: []string{"path"}},
		},
	}
	if !reflect.DeepEqual(got.Coverage, want) {
		t.Errorf("coverage is %+v, not %+v", got.Coverage, want)
	}
}

func TestAProblemWithNoFeaturesPrintsNoCoverage(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	_, verbose, _ := invokeIn(t.TempDir(), "check", "-v", "testdata/extremes")
	_, plain, _ := invokeIn(t.TempDir(), "check", "testdata/extremes")
	if verbose != plain {
		t.Errorf("check -v printed\n%s\nand check\n%s", verbose, plain)
	}
	_, out, _ := invokeIn(t.TempDir(), "check", "--json", "testdata/extremes")
	if strings.Contains(out, "coverage") || decoded(t, out).Coverage != nil {
		t.Errorf("check --json printed %s", out)
	}
}

func TestCheckWarnsWhenATestsetLacksAFeatureProblemJSONRequires(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := relocated(t, "testdata/features", func(problem *Problem) {
		problem.Testsets[1].Requires = []string{"path", "star"}
		problem.Testsets[2].Requires = []string{"star", "spider"}
	})
	shop := workshop(t, dir)
	found, err := shop.Check(context.Background(), false)
	if err != nil {
		t.Fatal(err)
	}
	var said []string
	for _, one := range found {
		if one.Code == "EO826" {
			said = append(said, one.Where+": "+one.Message+"; "+one.Fix)
		}
	}
	want := []string{
		`testset 2: no test has the feature "star", which problem.json requires of this testset; ` +
			"generate a test that has it, or move one here",
		`testset 2: problem.json requires the feature "spider" of this testset, and the validator does not ` +
			"declare it; it declares caterpillar, path, star; declare it with v.features, or correct the name in " +
			"requires",
	}
	if strings.Join(said, "\n") != strings.Join(want, "\n") {
		t.Errorf("EO826 said\n%s\nnot\n%s", strings.Join(said, "\n"), strings.Join(want, "\n"))
	}
	blind := relocated(t, "testdata/features", func(problem *Problem) {
		problem.Validator = nil
		problem.Testsets[1].Requires = []string{"path"}
	})
	shop = workshop(t, blind)
	if found, err = shop.Check(context.Background(), false); err != nil {
		t.Fatal(err)
	}
	said = nil
	for _, one := range found {
		if one.Code == "EO826" {
			said = append(said, one.Where+": "+one.Message)
		}
	}
	if strings.Join(said, "\n") != "testset 1: problem.json requires features of this testset, and the problem has "+
		"no validator to mark them" {
		t.Errorf("EO826 without a validator said %q", said)
	}
}
