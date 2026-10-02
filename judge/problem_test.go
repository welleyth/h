package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestEveryProblemInTheRepositoryLoads(t *testing.T) {
	t.Parallel()
	dirs, err := filepath.Glob("testdata/*")
	if err != nil {
		t.Fatal(err)
	}
	live, err := filepath.Glob("../tests/live/*")
	if err != nil {
		t.Fatal(err)
	}
	for _, dir := range append(dirs, live...) {
		if _, err := os.Stat(filepath.Join(dir, "problem.json")); err != nil {
			continue
		}
		if _, err := LoadProblem(dir); err != nil {
			t.Errorf("%s: %v", dir, err)
		}
	}
}

func loading(t *testing.T, body string) error {
	t.Helper()
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "problem.json"), []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	_, err := LoadProblem(dir)
	return err
}

func TestAMisspeltFieldOrNameIsRefused(t *testing.T) {
	t.Parallel()
	for body, said := range map[string]string{
		`{"type": "PROGRAM", "timeLimitMs": 1000}`:                               `unknown field "timeLimitMs"`,
		`{"type": "PROGRAMS"}`:                                                   `type is "PROGRAMS"; it is one of UNKNOWN_TYPE, PROGRAM, FUNCTION`,
		`{"testsets": [{"index": 2, "scoringMode": "WORSE"}]}`:                   `testset 2's scoringMode is "WORSE"`,
		`{"testsets": [{"index": 1, "feedbackPolicy": "ICPS"}]}`:                 `testset 1's feedbackPolicy is "ICPS"; it is one of UNKNOWN_FEEDBACK_POLICY, ICPC`,
		`{"testsets": [{"index": 1, "dependencyMode": "FIRST"}]}`:                `testset 1's dependencyMode is "FIRST"`,
		`{"solutions": [{"name": "full", "source": "a.cpp", "type": "CORECT"}]}`: `solution "full"'s type is "CORECT"; it is one of UNSET, CORRECT, INCORRECT`,
		`{"testsets": [{"index": 1, "tests": [{"index": 1, "scor": 5}]}]}`:       `unknown field "scor"`,
		`{"type": "PROGRAM"} {"type": "INTERACTIVE"}`:                            `something follows the problem's closing brace`,
		`{"type": "PROGRAM"} trailing`:                                           `something follows the problem's closing brace`,
		`{"Title": "a"}`:                                                         `the field "title" is spelt "Title"`,
		`{"testsets": [{"index": 1, "SCORINGMODE": "EACH"}]}`:                    `the field "scoringMode" is spelt "SCORINGMODE"`,
		`{"scripts": {"gen": {"Source": "gen.cpp"}}}`:                            `the field "source" is spelt "Source"`,
	} {
		err := loading(t, body)
		if err == nil || !strings.Contains(err.Error(), said) {
			t.Errorf("%s gave %v, want %q", body, err, said)
		}
	}
	for _, name := range []string{"../escape", "a/b", "..", ".", "", "back\\\\slash"} {
		body := `{"solutions": [{"name": "` + name + `", "source": "a.cpp"}]}`
		if err := loading(t, body); err == nil || !strings.Contains(err.Error(), "cannot be a file name") {
			t.Errorf("the solution name %q gave %v", name, err)
		}
	}
	if err := loading(t, `{"scripts": {"../gen": {"source": "gen.cpp"}}}`); err == nil ||
		!strings.Contains(err.Error(), `the script "../gen" cannot be a file name`) {
		t.Errorf("a script name with a path gave %v", err)
	}
	if err := loading(t, `{"type": "INTERACTIVE", "solutions": [{"name": "a", "type": "INCORRECT"}]}`); err != nil {
		t.Error(err)
	}
}

func TestRequiredFeaturesAreReadStrictly(t *testing.T) {
	t.Parallel()
	for body, said := range map[string]string{
		`{"testsets": [{"index": 2, "requires": ["path", "path"]}]}`: `testset 2's requires lists "path" twice; name each feature once`,
		`{"testsets": [{"index": 1, "requires": [""]}]}`:             `testset 1's requires holds an empty name; give each feature the name v.features declares it by`,
		`{"testsets": [{"index": 1, "requires": "path"}]}`:           `cannot unmarshal string`,
		`{"testsets": [{"index": 1, "Requires": ["path"]}]}`:         `the field "requires" is spelt "Requires"`,
	} {
		err := loading(t, body)
		if err == nil || !strings.Contains(err.Error(), said) {
			t.Errorf("%s gave %v, want %q", body, err, said)
		}
	}
	if err := loading(t, `{"testsets": [{"index": 1, "requires": ["path", "star"]}]}`); err != nil {
		t.Error(err)
	}
}

func TestARepeatedOrOverlongNameIsRefused(t *testing.T) {
	t.Parallel()
	for body, said := range map[string]string{
		`{"solutions": [{"name": "a", "source": "a.cpp"}, {"name": "a", "source": "b.cpp"}]}`: `two solutions are called "a"`,
		`{"scripts": {"gen": {"source": "a.cpp"}, "gen": {"source": "b.cpp"}}}`:               `"gen" appears twice in one object`,
		`{"type": "PROGRAM", "testsets": [], "type": "INTERACTIVE"}`:                          `"type" appears twice in one object`,
		`{"solutions": [{"name": "` + strings.Repeat("x", 241) + `", "source": "a.cpp"}]}`:    `is 241 bytes long; it becomes part of a directory name, so it holds at most 240`,
	} {
		err := loading(t, body)
		if err == nil || !strings.Contains(err.Error(), said) {
			t.Errorf("%.80s gave %v, want %q", body, err, said)
		}
	}
	if err := loading(t, `{"solutions": [{"name": "`+strings.Repeat("x", 240)+`", "source": "a.cpp"}]}`); err != nil {
		t.Error(err)
	}
}

func loaded(t *testing.T, body string) *Problem {
	t.Helper()
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "problem.json"), []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	problem, err := LoadProblem(dir)
	if err != nil {
		t.Fatalf("%s gave %v", body, err)
	}
	return problem
}

func TestEveryValueThePlatformExportsLoads(t *testing.T) {
	t.Parallel()
	for _, mode := range []string{"NO_SCORE", "EACH", "ALL", "WORST", "BEST"} {
		loaded(t, `{"testsets": [{"index": 1, "scoringMode": "`+mode+`"}]}`)
	}
	for _, policy := range []string{"ICPC", "ICPC_EXPANDED", "COMPLETE"} {
		loaded(t, `{"testsets": [{"index": 1, "feedbackPolicy": "`+policy+`"}]}`)
	}
	loaded(t, `{"testsets": [{"index": 1, "dependencyMode": "FIRST_POINT"}]}`)
	for _, kind := range []string{"CORRECT", "INCORRECT", "WRONG_ANSWER", "TIMEOUT", "OVERFLOW",
		"TIMEOUT_OR_ACCEPTED", "OVERFLOW_OR_ACCEPTED", "DONT_RUN", "FAILURE"} {
		loaded(t, `{"solutions": [{"name": "a", "source": "a.cpp", "type": "`+kind+`"}]}`)
	}
	for _, kind := range []string{"PROGRAM", "INTERACTIVE", "OUTPUT"} {
		if got := loaded(t, `{"type": "`+kind+`"}`).Type; got != kind {
			t.Errorf("%s loaded as %s", kind, got)
		}
	}
}

func TestAnUnknownPlatformValueIsTheFieldLeftOut(t *testing.T) {
	t.Parallel()
	problem := loaded(t, `{"type": "UNKNOWN_TYPE", "testsets": [{"index": 1,
		"feedbackPolicy": "UNKNOWN_FEEDBACK_POLICY", "dependencyMode": "UNKNOWN_DEPENDENCY_MODE"}],
		"solutions": [{"name": "a", "source": "a.cpp", "type": "UNSET"}]}`)
	testset := problem.Testsets[0]
	if problem.Type != "PROGRAM" || testset.ScoringMode != "EACH" || testset.FeedbackPolicy != "COMPLETE" ||
		testset.DependencyMode != "FULLY_ACCEPTED" || problem.Solutions[0].Type != "" {
		t.Errorf("loaded %+v %+v %+v", problem, testset, problem.Solutions[0])
	}
}

func TestAProblemTypeEoJudgeCannotRunIsRefused(t *testing.T) {
	t.Parallel()
	for _, kind := range []string{"SQL", "ML", "QUIZ", "WIDGET"} {
		err := loading(t, `{"type": "`+kind+`"}`)
		if err == nil || !strings.Contains(err.Error(), "eo-judge does not run "+kind+" problems") {
			t.Errorf("%s gave %v", kind, err)
		}
	}
	if err := loading(t, `{"type": "COMMUNICATION"}`); err != nil {
		t.Errorf("COMMUNICATION, which lint reads, gave %v", err)
	}
}

func TestADontRunSolutionIsJudgedOnlyWhenNamed(t *testing.T) {
	t.Parallel()
	problem := loaded(t, `{"solutions": [{"name": "a", "source": "a.cpp", "type": "CORRECT"},
		{"name": "b", "source": "b.cpp", "type": "DONT_RUN"}, {"name": "c", "source": "c.cpp"}]}`)
	var names []string
	for _, one := range problem.Judged("") {
		names = append(names, one.Name)
	}
	if strings.Join(names, " ") != "a c" {
		t.Errorf("judged %v", names)
	}
	if named := problem.Judged("b"); len(named) != 1 || named[0].Name != "b" {
		t.Errorf("naming b judged %v", named)
	}
}

func TestTheExampleInThePageLoads(t *testing.T) {
	t.Parallel()
	page, err := os.ReadFile("../docs/judge.md")
	if err != nil {
		t.Fatal(err)
	}
	text := string(page)
	start := strings.Index(text, "```json\n")
	if start < 0 {
		t.Fatal("docs/judge.md has no problem.json example")
	}
	body := text[start+len("```json\n"):]
	body = body[:strings.Index(body, "```")]
	if err := loading(t, body); err != nil {
		t.Error(err)
	}
}
