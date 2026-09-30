package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math"
	"os"
	"path/filepath"
	"reflect"
	"strings"
)

type Program struct {
	Source  string   `json:"source"`
	Runtime string   `json:"runtime"`
	Files   []string `json:"files"`
}

type Generator struct {
	Script    string   `json:"script"`
	Arguments []string `json:"arguments"`
}

type Test struct {
	Index           int        `json:"index"`
	Score           float64    `json:"score"`
	Example         bool       `json:"example"`
	Input           string     `json:"input"`
	Answer          string     `json:"answer"`
	Generator       *Generator `json:"generator"`
	AnswerGenerator string     `json:"answerGenerator"`
}

type Testset struct {
	Index          int     `json:"index"`
	ScoringMode    string  `json:"scoringMode"`
	FeedbackPolicy string  `json:"feedbackPolicy"`
	DependencyMode string  `json:"dependencyMode"`
	Dependencies   []int   `json:"dependencies"`
	TimeLimit      int     `json:"timeLimit"`
	MemoryLimit    int64   `json:"memoryLimit"`
	Tests          []*Test `json:"tests"`
}

type Solution struct {
	Name   string `json:"name"`
	Source string `json:"source"`
	Type   string `json:"type"`
	Scores string `json:"scores"`
}

type ValidatorTest struct {
	Input  *string `json:"input"`
	File   string  `json:"file"`
	Expect string  `json:"expect"`
	Group  *int    `json:"group"`
}

type CheckerTest struct {
	Input  string        `json:"input"`
	Output string        `json:"output"`
	Answer string        `json:"answer"`
	Expect CheckerExpect `json:"expect"`
	Cost   *float64      `json:"cost"`
	Group  *int          `json:"group"`
}

type CheckerExpect struct {
	Verdict string   `json:"-"`
	Points  *float64 `json:"points"`
}

func (e *CheckerExpect) UnmarshalJSON(body []byte) error {
	if json.Unmarshal(body, &e.Verdict) == nil {
		return nil
	}
	var scored struct {
		Points *float64 `json:"points"`
	}
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&scored); err != nil {
		return fmt.Errorf(`an expect is "ACCEPTED", "WRONG_ANSWER", "PARTIAL", "FAILURE" or {"points": x}: %w`, err)
	}
	if scored.Points == nil {
		return errors.New(`{"points": x} needs its x, the points the checker pays`)
	}
	e.Points = scored.Points
	return nil
}

func (e CheckerExpect) MarshalJSON() ([]byte, error) {
	if e.Points != nil {
		return json.Marshal(map[string]float64{"points": *e.Points})
	}
	return json.Marshal(e.Verdict)
}

type Problem struct {
	Title               string              `json:"title"`
	Type                string              `json:"type"`
	RunCount            int                 `json:"runCount"`
	TimeLimit           int                 `json:"timeLimit"`
	CPULimit            int                 `json:"cpuLimit"`
	MemoryLimit         int64               `json:"memoryLimit"`
	InstanceLimit       int                 `json:"instanceLimit"`
	InteractorTimeLimit int                 `json:"interactorTimeLimit"`
	Unique              bool                `json:"uniqueAnswer"`
	ExactFormat         bool                `json:"exactFormat"`
	Checker             *Program            `json:"checker"`
	Validator           *Program            `json:"validator"`
	Interactor          *Program            `json:"interactor"`
	Scripts             map[string]*Program `json:"scripts"`
	Solutions           []*Solution         `json:"solutions"`
	Testsets            []*Testset          `json:"testsets"`
	ValidatorTests      []*ValidatorTest    `json:"validatorTests"`
	CheckerTests        []*CheckerTest      `json:"checkerTests"`

	dir string
}

func (p *Problem) Dir() string { return p.dir }

func (p *Problem) Path(name string) string {
	if filepath.IsAbs(name) {
		return name
	}
	return filepath.Join(p.dir, name)
}

func (p *Problem) Interactive() bool {
	return p.Type == "INTERACTIVE"
}

func (p *Problem) Solution(name string) *Solution {
	for _, one := range p.Solutions {
		if one.Name == name {
			return one
		}
	}
	return nil
}

func (p *Problem) Judged(only string) []*Solution {
	var judged []*Solution
	for _, one := range p.Solutions {
		if one.Name == only || (only == "" && one.Type != "DONT_RUN") {
			judged = append(judged, one)
		}
	}
	return judged
}

func (p *Problem) Testset(index int) *Testset {
	for _, one := range p.Testsets {
		if one.Index == index {
			return one
		}
	}
	return nil
}

func (t *Testset) Limit(p *Problem) int {
	if t.TimeLimit == 0 {
		return p.TimeLimit
	}
	return t.TimeLimit
}

func (s *Solution) Expected() (float64, bool) {
	if s.Scores == "" {
		if s.Type == "CORRECT" {
			return 100, true
		}
		return 0, false
	}
	var want float64
	if _, err := fmt.Sscanf(s.Scores, "%g", &want); err != nil {
		return 0, false
	}
	return want, true
}

func LoadProblem(dir string) (*Problem, error) {
	body, err := os.ReadFile(filepath.Join(dir, "problem.json"))
	if err != nil {
		return nil, err
	}

	problem := &Problem{RunCount: 1, dir: dir}
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(problem); err != nil {
		return nil, fmt.Errorf("problem.json: %w", err)
	}
	if err := decoder.Decode(&struct{}{}); err != io.EOF {
		return nil, fmt.Errorf("problem.json: something follows the problem's closing brace")
	}
	var raw any
	if err := json.Unmarshal(body, &raw); err != nil {
		return nil, fmt.Errorf("problem.json: %w", err)
	}
	if err := exactNames(raw, reflect.TypeOf(problem)); err != nil {
		return nil, fmt.Errorf("problem.json: %w", err)
	}
	if err := repeatedKey(json.NewDecoder(bytes.NewReader(body))); err != nil {
		return nil, fmt.Errorf("problem.json: %w", err)
	}

	absent(&problem.Type, "UNKNOWN_TYPE", "PROGRAM")
	if problem.RunCount < 1 {
		problem.RunCount = 1
	}
	for _, testset := range problem.Testsets {
		absent(&testset.ScoringMode, "", "EACH")
		absent(&testset.FeedbackPolicy, "UNKNOWN_FEEDBACK_POLICY", "COMPLETE")
		absent(&testset.DependencyMode, "UNKNOWN_DEPENDENCY_MODE", "FULLY_ACCEPTED")
	}
	for _, solution := range problem.Solutions {
		absent(&solution.Type, "UNSET", "")
	}
	if err := problem.checkNames(); err != nil {
		return nil, fmt.Errorf("problem.json: %w", err)
	}
	return problem, nil
}

func exactNames(value any, kind reflect.Type) error {
	for kind.Kind() == reflect.Pointer {
		kind = kind.Elem()
	}
	var inside []any
	switch kind.Kind() {
	case reflect.Struct:
		object, _ := value.(map[string]any)
		for key, one := range object {
			field, spelt := fieldCalled(kind, key)
			if spelt != key {
				return fmt.Errorf("the field %q is spelt %q; field names are case-sensitive", spelt, key)
			}
			if err := exactNames(one, field.Type); err != nil {
				return err
			}
		}
		return nil
	case reflect.Slice:
		inside, _ = value.([]any)
	case reflect.Map:
		object, _ := value.(map[string]any)
		for _, one := range object {
			inside = append(inside, one)
		}
	}
	for _, one := range inside {
		if err := exactNames(one, kind.Elem()); err != nil {
			return err
		}
	}
	return nil
}

func fieldCalled(kind reflect.Type, key string) (reflect.StructField, string) {
	var near reflect.StructField
	spelt := ""
	for at := 0; at < kind.NumField(); at++ {
		field := kind.Field(at)
		name := strings.Split(field.Tag.Get("json"), ",")[0]
		if name == key {
			return field, name
		}
		if strings.EqualFold(name, key) {
			near, spelt = field, name
		}
	}
	return near, spelt
}

func absent(value *string, unknown, otherwise string) {
	if *value == "" || *value == unknown {
		*value = otherwise
	}
}

func oneOf(what, value string, allowed ...string) error {
	for _, one := range allowed {
		if value == one {
			return nil
		}
	}
	return fmt.Errorf("%s is %q; it is one of %s", what, value, strings.Join(allowed, ", "))
}

func plainName(what, name string) error {
	if name == "" || name == "." || name == ".." || strings.ContainsAny(name, "/\\\x00") {
		return fmt.Errorf("%s %q cannot be a file name; use letters, digits, dots, dashes and underscores",
			what, name)
	}
	if len(name) > 240 {
		return fmt.Errorf("%s %q... is %d bytes long; it becomes part of a directory name, so it holds at most 240",
			what, name[:16], len(name))
	}
	return nil
}

func repeatedKey(decoder *json.Decoder) error {
	token, err := decoder.Token()
	if err != nil {
		return err
	}
	delim, opens := token.(json.Delim)
	if !opens {
		return nil
	}
	seen := map[string]bool{}
	for decoder.More() {
		if delim == '{' {
			key, err := decoder.Token()
			if err != nil {
				return err
			}
			name, _ := key.(string)
			if seen[name] {
				return fmt.Errorf("%q appears twice in one object; the second would silently replace the first", name)
			}
			seen[name] = true
		}
		if err := repeatedKey(decoder); err != nil {
			return err
		}
	}
	_, err = decoder.Token()
	return err
}

func (p *Problem) checkNames() error {
	if err := oneOf("type", p.Type, "UNKNOWN_TYPE", "PROGRAM", "FUNCTION", "OUTPUT", "SQL", "ML", "QUIZ",
		"INTERACTIVE", "COMMUNICATION", "WIDGET"); err != nil {
		return err
	}
	if p.Type != "PROGRAM" && p.Type != "COMMUNICATION" && !p.Interactive() {
		return fmt.Errorf("eo-judge does not run %s problems", p.Type)
	}
	for _, testset := range p.Testsets {
		where := fmt.Sprintf("testset %d's ", testset.Index)
		if err := oneOf(where+"scoringMode", testset.ScoringMode, "NO_SCORE", "EACH", "ALL", "WORST",
			"BEST"); err != nil {
			return err
		}
		if err := oneOf(where+"feedbackPolicy", testset.FeedbackPolicy,
			"UNKNOWN_FEEDBACK_POLICY", "ICPC", "ICPC_EXPANDED", "COMPLETE"); err != nil {
			return err
		}
		if err := oneOf(where+"dependencyMode", testset.DependencyMode,
			"UNKNOWN_DEPENDENCY_MODE", "FULLY_ACCEPTED", "FIRST_POINT"); err != nil {
			return err
		}
	}
	for name := range p.Scripts {
		if err := plainName("the script", name); err != nil {
			return err
		}
	}
	named := map[string]bool{}
	for _, solution := range p.Solutions {
		if err := plainName("the solution", solution.Name); err != nil {
			return err
		}
		if named[solution.Name] {
			return fmt.Errorf("two solutions are called %q; each needs a name of its own", solution.Name)
		}
		named[solution.Name] = true
		if solution.Type == "" {
			continue
		}
		if err := oneOf(fmt.Sprintf("solution %q's type", solution.Name), solution.Type, "UNSET",
			"CORRECT", "INCORRECT", "WRONG_ANSWER", "TIMEOUT", "OVERFLOW", "TIMEOUT_OR_ACCEPTED",
			"OVERFLOW_OR_ACCEPTED", "DONT_RUN", "FAILURE"); err != nil {
			return err
		}
	}
	if err := p.checkValidatorTests(); err != nil {
		return err
	}
	return p.checkCheckerTests()
}

func (p *Problem) checkValidatorTests() error {
	if len(p.ValidatorTests) > 0 && p.Validator == nil {
		return errors.New("the problem has validatorTests and no validator to run them")
	}
	for at, test := range p.ValidatorTests {
		name := fmt.Sprintf("validator test %d", at+1)
		if err := oneOf(name+"'s expect", test.Expect, "VALID", "INVALID"); err != nil {
			return err
		}
		switch {
		case test.Input != nil && test.File != "":
			return fmt.Errorf("%s has both an input and a file; give one", name)
		case test.Input == nil && test.File == "":
			return fmt.Errorf("%s has neither an input nor a file; give one", name)
		}
		if err := p.knownGroup(name, test.Group); err != nil {
			return err
		}
	}
	return nil
}

func (p *Problem) checkCheckerTests() error {
	if len(p.CheckerTests) > 0 && p.Checker == nil {
		return errors.New("the problem has checkerTests and no checker to run them")
	}
	for at, test := range p.CheckerTests {
		name := fmt.Sprintf("checker test %d", at+1)
		if test.Expect.Points == nil {
			if err := oneOf(name+"'s expect", test.Expect.Verdict, "ACCEPTED", "WRONG_ANSWER", "PARTIAL",
				"FAILURE"); err != nil {
				return fmt.Errorf(`%w, or {"points": x}`, err)
			}
		}
		if points := test.Expect.Points; points != nil {
			if err := judgePoints(name+` expects {"points": %g}`, *points, "a run pays 0 or more"); err != nil {
				return err
			}
		}
		if test.Cost != nil {
			if err := judgePoints(name+"'s cost is %g", *test.Cost, "a test is worth 0 or more"); err != nil {
				return err
			}
		}
		cost := 100.0
		if test.Cost != nil {
			cost = *test.Cost
		}
		if points := test.Expect.Points; points != nil && *points > cost {
			return fmt.Errorf(`%s expects {"points": %g}, more than its cost of %g; a run pays at most what the test is worth`,
				name, *points, cost)
		}
		if err := p.knownGroup(name, test.Group); err != nil {
			return err
		}
	}
	return nil
}

func judgePoints(what string, value float64, floor string) error {
	said := fmt.Sprintf(what, value)
	if value < 0 || math.Signbit(value) {
		return fmt.Errorf("%s; %s", said, floor)
	}
	if value > math.MaxFloat32 {
		return fmt.Errorf("%s, more than the judge's points can hold", said)
	}
	return nil
}

func (p *Problem) knownGroup(name string, group *int) error {
	if group != nil && p.Testset(*group) == nil {
		return fmt.Errorf("%s's group is %d, and the problem has no testset %d", name, *group, *group)
	}
	return nil
}
