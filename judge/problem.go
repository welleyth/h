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
	"sort"
	"strings"
)

type Program struct {
	Source  string   `json:"source"`
	Runtime string   `json:"runtime"`
	Files   []string `json:"files"`

	wrapped *Template
}

type Template struct {
	Runtime string `json:"runtime"`
	Header  string `json:"header"`
	Source  string `json:"source"`
	Footer  string `json:"footer"`
}

func (t *Template) cpp() bool { return cppRuntime(t.Runtime) }

func cppRuntime(runtime string) bool { return strings.HasPrefix(runtime, "cpp:") }

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
	Index          int      `json:"index"`
	ScoringMode    string   `json:"scoringMode"`
	FeedbackPolicy string   `json:"feedbackPolicy"`
	DependencyMode string   `json:"dependencyMode"`
	Dependencies   []int    `json:"dependencies"`
	TimeLimit      int      `json:"timeLimit"`
	MemoryLimit    int64    `json:"memoryLimit"`
	Tests          []*Test  `json:"tests"`
	Requires       []string `json:"requires"`
}

type Solution struct {
	Name    string            `json:"name"`
	Source  string            `json:"source"`
	Runtime string            `json:"runtime"`
	Outputs map[string]string `json:"outputs"`
	Type    string            `json:"type"`
	Scores  string            `json:"scores"`

	uploaded map[string]string
	template *Template
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
	Templates           []*Template         `json:"templates"`
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

func (p *Problem) Output() bool {
	return p.Type == "OUTPUT"
}

func (p *Problem) Function() bool {
	return p.Type == "FUNCTION"
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

func (p *Problem) programOf(solution *Solution) *Program {
	program := &Program{Source: solution.Source, Runtime: solution.Runtime, wrapped: solution.template}
	if program.wrapped != nil {
		program.Runtime = program.wrapped.Runtime
	}
	return program
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
	if p.Type != "PROGRAM" && p.Type != "COMMUNICATION" && !p.Interactive() && !p.Output() && !p.Function() {
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
		required := map[string]bool{}
		for _, name := range testset.Requires {
			if name == "" {
				return fmt.Errorf("%srequires holds an empty name; give each feature the name v.features declares it by",
					where)
			}
			if required[name] {
				return fmt.Errorf("%srequires lists %q twice; name each feature once", where, name)
			}
			required[name] = true
		}
	}
	for name := range p.Scripts {
		if err := plainName("the script", name); err != nil {
			return err
		}
	}
	if err := p.checkTemplates(); err != nil {
		return err
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
		if err := p.checkOutputs(solution); err != nil {
			return err
		}
		if err := p.checkRuntime(solution); err != nil {
			return err
		}
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

func (p *Problem) checkTemplates() error {
	if p.Output() && len(p.Templates) > 0 {
		return errors.New("an OUTPUT problem has no templates: its contestants upload files, not code")
	}
	if p.Function() && len(p.Templates) == 0 {
		return errors.New(`a FUNCTION problem needs templates, one per runtime: {"runtime": "cpp:20-gnu14", ` +
			`"header": …, "source": …, "footer": …}; the judge compiles header, solution and footer as one file`)
	}
	first := map[string]int{}
	for at, template := range p.Templates {
		if template.Runtime == "" {
			return fmt.Errorf("template %d has no runtime", at+1)
		}
		if earlier, twice := first[template.Runtime]; twice {
			return fmt.Errorf("templates %d and %d are both for %q; the judge keeps one template per runtime",
				earlier, at+1, template.Runtime)
		}
		first[template.Runtime] = at + 1
		if !p.Function() && (template.Header != "" || template.Footer != "") {
			return fmt.Errorf("template %d has a header or a footer, which the judge wraps around a submission "+
				"on a FUNCTION problem only; on a %s problem a template is the source a contestant starts from",
				at+1, p.Type)
		}
	}
	return nil
}

func (p *Problem) checkRuntime(solution *Solution) error {
	if p.Output() && solution.Runtime != "" {
		return fmt.Errorf("solution %q of an OUTPUT problem has a runtime; its outputs are files, not code",
			solution.Name)
	}
	if solution.Runtime != "" && !cppRuntime(solution.Runtime) {
		return fmt.Errorf("solution %q is written for %q, and eo-judge builds C++ solutions only", solution.Name,
			solution.Runtime)
	}
	if !p.Function() {
		return nil
	}
	if solution.Runtime != "" {
		for _, template := range p.Templates {
			if template.Runtime == solution.Runtime {
				solution.template = template
				return nil
			}
		}
		return fmt.Errorf("solution %q is written for %q, and the problem has no template for it; the judge "+
			"wraps a FUNCTION problem's solution in its runtime's template", solution.Name, solution.Runtime)
	}
	var cpp []string
	for _, template := range p.Templates {
		if template.cpp() {
			cpp = append(cpp, template.Runtime)
			solution.template = template
		}
	}
	switch len(cpp) {
	case 0:
		return fmt.Errorf("solution %q gives no runtime, and the problem has no template for a C++ runtime; "+
			"eo-judge builds C++ solutions only", solution.Name)
	case 1:
		return nil
	}
	solution.template = nil
	return fmt.Errorf("solution %q gives no runtime, and the problem has templates for %d C++ runtimes, %s and %s; "+
		"give it a runtime", solution.Name, len(cpp), strings.Join(cpp[:len(cpp)-1], ", "), cpp[len(cpp)-1])
}

func (p *Problem) checkOutputs(solution *Solution) error {
	if !p.Output() {
		if solution.Outputs != nil {
			return fmt.Errorf("solution %q has outputs, which only an OUTPUT problem's solutions give; a %s "+
				"problem's solution is a source", solution.Name, p.Type)
		}
		return nil
	}
	if solution.Source != "" || solution.Outputs == nil {
		return fmt.Errorf("solution %q of an OUTPUT problem gives its answer files in outputs, not a source: "+
			`"outputs": {"1": "one.txt", "2": "two.txt"}`, solution.Name)
	}
	keys := make([]string, 0, len(solution.Outputs))
	for key := range solution.Outputs {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	solution.uploaded = map[string]string{}
	given, named := map[string]string{}, map[string]string{}
	for _, key := range keys {
		if solution.Outputs[key] == "" {
			return fmt.Errorf("solution %q gives an empty file name for test %q", solution.Name, key)
		}
		test, err := p.testCalled(solution.Name, key)
		if err != nil {
			return err
		}
		if earlier, twice := given[test]; twice {
			return fmt.Errorf("solution %q gives two outputs for test %s, as %q and %q", solution.Name, test,
				earlier, key)
		}
		given[test], named[key] = key, test
		solution.uploaded[test] = solution.Outputs[key]
	}
	for _, key := range keys {
		test := named[key]
		if err := readable(p.Path(solution.Outputs[key])); err != nil {
			return fmt.Errorf("solution %q gives %s for test %s, and it cannot be read: %w", solution.Name,
				solution.Outputs[key], test, err)
		}
	}
	return nil
}

func readable(path string) error {
	file, err := os.Open(path)
	if err != nil {
		return err
	}
	defer file.Close()
	info, err := file.Stat()
	switch {
	case err != nil:
		return err
	case info.IsDir():
		return errors.New("it is a directory, not a file")
	case !info.Mode().IsRegular():
		return errors.New("it is not a regular file")
	}
	return nil
}

func (p *Problem) testCalled(solution, key string) (string, error) {
	var found []string
	for _, testset := range p.Testsets {
		for _, test := range testset.Tests {
			full := reference(&Planned{Group: testset.Index, Test: test})
			if key == full || key == fmt.Sprint(test.Index) {
				found = append(found, full)
			}
		}
	}
	switch {
	case len(found) == 0:
		return "", fmt.Errorf("solution %q gives an output for test %q, and the problem has no such test; "+
			`name a test "group:index", or by its index when no other testset has one`, solution, key)
	case len(found) > 1:
		var groups []string
		for _, one := range found {
			group, _, _ := strings.Cut(one, ":")
			groups = append(groups, group)
		}
		return "", fmt.Errorf("solution %q gives an output for test %q, which is test %s of testsets %s; write %s",
			solution, key, key, strings.Join(groups, " and "), `"`+strings.Join(found, `" or "`)+`"`)
	}
	return found[0], nil
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
