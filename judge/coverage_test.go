package main

import (
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
