package main

import (
	"crypto/sha1"
	"encoding/hex"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
)

type Finding struct {
	Code     string
	Severity string
	Where    string
	Message  string
	Fix      string
}

func (f Finding) String() string {
	where := ""
	if f.Where != "" {
		where = f.Where + ": "
	}
	return fmt.Sprintf("%s%s %s: %s\n  %s", where, f.Severity, f.Code, f.Message, f.Fix)
}

func (f Finding) before(other Finding) bool {
	switch {
	case f.Severity != other.Severity:
		return f.Severity == "warning"
	case f.Code != other.Code:
		return f.Code < other.Code
	case f.Where != other.Where:
		return naturalLess(f.Where, other.Where)
	}
	return f.Message < other.Message
}

func naturalLess(a, b string) bool {
	for a != "" && b != "" {
		x, y := leadingDigits(a), leadingDigits(b)
		if x == "" || y == "" {
			if a[0] != b[0] {
				return a[0] < b[0]
			}
			a, b = a[1:], b[1:]
			continue
		}
		if p, q := strings.TrimLeft(x, "0"), strings.TrimLeft(y, "0"); p != q {
			if len(p) != len(q) {
				return len(p) < len(q)
			}
			return p < q
		}
		if x != y {
			return len(x) < len(y)
		}
		a, b = a[len(x):], b[len(y):]
	}
	return len(a) < len(b)
}

func leadingDigits(text string) string {
	end := 0
	for end < len(text) && text[end] >= '0' && text[end] <= '9' {
		end++
	}
	return text[:end]
}

type Findings []Finding

func (f *Findings) warn(code, where, message, fix string) {
	*f = append(*f, Finding{Code: code, Severity: "warning", Where: where, Message: message, Fix: fix})
}

func (f *Findings) note(code, where, message, fix string) {
	*f = append(*f, Finding{Code: code, Severity: "note", Where: where, Message: message, Fix: fix})
}

var quoted = regexp.MustCompile(`(?m)^\s*#\s*include\s*"([^"]+)"`)

func namedPrograms(problem *Problem) map[string]*Program {
	out := map[string]*Program{}
	if problem.Checker != nil {
		out["checker"] = problem.Checker
	}
	if problem.Validator != nil {
		out["validator"] = problem.Validator
	}
	if problem.Interactor != nil {
		out["interactor"] = problem.Interactor
	}
	for name, script := range problem.Scripts {
		out[scriptName(name)] = script
	}
	for _, solution := range problem.Solutions {
		out[solutionName(solution.Name)] = &Program{Source: solution.Source}
	}
	return out
}

func scriptName(name string) string { return "script." + name }

func solutionName(name string) string { return "solution." + name }

func label(program string) string {
	role, name, named := strings.Cut(program, ".")
	if !named {
		return program
	}
	return role + " " + name
}

func Configuration(problem *Problem) Findings {
	var found Findings
	rows, total := 0, float64(0)
	for _, testset := range problem.Testsets {
		rows += len(testset.Tests)
		total += testsetChecks(testset, &found)
	}
	if rows > 1200 {
		found.warn("EO909", "", fmt.Sprintf("the problem has %d test rows", rows),
			"Basecamp stops judging above about 1200; merge tests or drop some")
	}
	if len(problem.Testsets) > 0 && total != 100 {
		found.warn("EO907", "", fmt.Sprintf("the testsets add up to %g, not 100", total),
			"make the scores add up to the problem's total")
	}
	if problem.Interactive() && problem.TimeLimit == 0 {
		found.warn("EO902", "", "an interactive problem with no wall time limit",
			"the interactor is given the wall limit plus a second and nothing else bounds it")
	}
	if problem.RunCount > 1 && !problem.Interactive() {
		found.warn("EO908", "", fmt.Sprintf("runCount is %d on a %s problem", problem.RunCount, problem.Type),
			"run_count chains an interactor's output into the next run; the problem must be interactive")
	}
	headerChecks(problem, &found)
	return found
}

func testsetChecks(testset *Testset, found *Findings) float64 {
	where := fmt.Sprintf("testset %d", testset.Index)
	if testset.ScoringMode == "EACH" && strings.HasPrefix(testset.FeedbackPolicy, "ICPC") {
		found.warn("EO901", where, "an EACH testset with ICPC feedback",
			"ICPC stops after the first test worth nothing, so the rest score 0; use COMPLETE")
	}
	for _, on := range testset.Dependencies {
		if on == testset.Index {
			found.warn("EO904", where, "the testset depends on itself",
				"remove it from its own dependencies; nothing will ever run")
		}
	}

	most, sum := float64(0), float64(0)
	for _, test := range testset.Tests {
		most, sum = max(most, test.Score), sum+test.Score
	}
	switch {
	case testset.Index == 0:
		if firstTest(testset, func(test *Test) bool { return test.Score != 0 }) != nil {
			found.warn("EO905", where, "an examples testset carries points",
				"samples are shown, not scored; set the score to 0")
		}
		if test := firstTest(testset, func(test *Test) bool { return !test.Example }); test != nil {
			found.warn("EO905", where, fmt.Sprintf("test %d in the examples testset is not flagged as an example", test.Index),
				"set example on every test of testset 0")
		}
		return 0
	case testset.ScoringMode == "WORST":
		if test := firstTest(testset, func(test *Test) bool { return test.Score != most }); test != nil {
			found.warn("EO906", where,
				fmt.Sprintf("a WORST testset where test %d is worth %g and the most is %g", test.Index, test.Score, most),
				"under WORST the group takes the smallest test score, so every test carries the full value")
		}
		return most
	}
	return sum
}

func firstTest(testset *Testset, matches func(*Test) bool) *Test {
	for _, test := range testset.Tests {
		if matches(test) {
			return test
		}
	}
	return nil
}

func headerChecks(problem *Problem, found *Findings) {
	copies := map[string]map[string]bool{}
	for name, program := range namedPrograms(problem) {
		if program.Source == "" {
			continue
		}
		body, err := os.ReadFile(problem.Path(program.Source))
		if err != nil {
			continue
		}
		quotedChecks(problem, label(name), program, string(body), found)
		for _, one := range program.Files {
			header := filepath.Base(one)
			if !strings.HasPrefix(header, "eolymp") {
				continue
			}
			body, err := os.ReadFile(problem.Path(one))
			if err != nil {
				continue
			}
			sum := sha1.Sum(body)
			if copies[header] == nil {
				copies[header] = map[string]bool{}
			}
			copies[header][hex.EncodeToString(sum[:])] = true
		}
	}
	for header, sums := range copies {
		if len(sums) > 1 {
			found.note("EO910", "", fmt.Sprintf("the programs carry %d different copies of %s", len(sums), header),
				"attach one release to every program of a problem")
		}
	}
}

func quotedChecks(problem *Problem, name string, program *Program, body string, found *Findings) {
	attached := map[string]bool{}
	for _, one := range program.Files {
		attached[filepath.Base(one)] = true
	}
	for _, match := range quoted.FindAllStringSubmatch(body, -1) {
		wanted := filepath.Base(match[1])
		if !attached[wanted] && !fileBeside(problem, program, wanted) && !slices.Contains(carriedHeaders, wanted) {
			found.warn("EO903", name, fmt.Sprintf("it includes %q with no matching files entry", match[1]),
				"attach the header to the program, or the first run fails to compile")
		}
	}
}

func fileBeside(problem *Problem, program *Program, name string) bool {
	_, err := os.Stat(filepath.Join(filepath.Dir(problem.Path(program.Source)), name))
	return err == nil
}
