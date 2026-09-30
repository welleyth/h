package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"math"
	"math/rand/v2"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
)

const (
	stressPassed         = "PASSED"
	stressCounterexample = "COUNTEREXAMPLE"
	stressInvalid        = "INVALID"
	stressBroken         = "BROKEN"
)

type names []string

func (n *names) String() string { return strings.Join(*n, ",") }

func (n *names) Set(value string) error {
	*n = append(*n, value)
	return nil
}

type stressOptions struct {
	gen, args, reference string
	each, compared       names
	iterations, timeout  int
	keepGoing            bool
}

func (o *stressOptions) register(flags *flag.FlagSet) {
	flags.StringVar(&o.gen, "gen", "", "the generator script")
	flags.StringVar(&o.args, "args", "", "the generator's arguments, with [a..b] ranges")
	flags.Var(&o.each, "arg", "one argument of the generator, again for the next")
	flags.BoolVar(&o.keepGoing, "continue", false, "go on past an INVALID or BROKEN iteration")
	flags.StringVar(&o.reference, "reference", "", "the CORRECT solution whose output is the answer")
	flags.Var(&o.compared, "solution", "a solution to compare with the reference")
	flags.IntVar(&o.iterations, "iterations", 100, "at most this many inputs")
	flags.IntVar(&o.timeout, "timeout", 300, "at most this many seconds for the whole run")
}

type stressPlan struct {
	gen        string
	pattern    argumentPattern
	reference  *Solution
	compared   []*Solution
	iterations int
	timeout    int
}

func (o *stressOptions) plan(problem *Problem) (*stressPlan, string) {
	if o.iterations < 1 {
		return nil, fmt.Sprintf("--iterations is %d; it is at least 1", o.iterations)
	}
	if o.timeout < 1 {
		return nil, fmt.Sprintf("--timeout is %d; it is at least 1 second", o.timeout)
	}
	reference, why := referenceOf(problem, o.reference)
	if why != "" {
		return nil, why
	}
	compared, why := comparedWith(problem, reference, o.compared)
	if why != "" {
		return nil, why
	}
	gen, why := generatorOf(problem, o.gen)
	if why != "" {
		return nil, why
	}
	words := strings.Fields(o.args)
	if len(o.each) > 0 {
		if o.args != "" {
			return nil, "give the generator's arguments with --args or with --arg, not both"
		}
		words = o.each
	}
	pattern, err := parseArguments(words)
	if err != nil {
		return nil, err.Error()
	}
	return &stressPlan{gen: gen, pattern: pattern, reference: reference, compared: compared,
		iterations: o.iterations, timeout: o.timeout}, ""
}

func generatorOf(problem *Problem, named string) (string, string) {
	scripts := make([]string, 0, len(problem.Scripts))
	for name := range problem.Scripts {
		scripts = append(scripts, name)
	}
	sort.Strings(scripts)
	if named != "" {
		if _, known := problem.Scripts[named]; !known {
			return "", fmt.Sprintf("the problem has no script called %q; it has %s", named, listed(scripts))
		}
		return named, ""
	}
	used := map[string]bool{}
	for _, testset := range problem.Testsets {
		for _, test := range testset.Tests {
			if test.Generator != nil {
				used[test.Generator.Script] = true
			}
		}
	}
	if len(used) == 1 {
		for name := range used {
			if _, known := problem.Scripts[name]; known {
				return name, ""
			}
		}
	}
	return "", fmt.Sprintf("name the generator with --gen; the problem has the scripts %s", listed(scripts))
}

func listed(names []string) string {
	if len(names) == 0 {
		return "none"
	}
	return strings.Join(names, ", ")
}

func solutionNames(problem *Problem) string {
	var known []string
	for _, one := range problem.Solutions {
		known = append(known, one.Name)
	}
	return listed(known)
}

func referenceOf(problem *Problem, named string) (*Solution, string) {
	if named == "" {
		for _, one := range problem.Solutions {
			if one.Type == "CORRECT" {
				return one, ""
			}
		}
		return nil, "a stress run needs a CORRECT solution as its reference; the problem has none"
	}
	reference := problem.Solution(named)
	if reference == nil {
		return nil, fmt.Sprintf("the problem has no solution called %q; it has %s", named, solutionNames(problem))
	}
	if reference.Type != "CORRECT" {
		return nil, fmt.Sprintf("the reference must be a CORRECT solution, and %q is %s", named, typeOf(reference))
	}
	return reference, ""
}

func typeOf(solution *Solution) string {
	if solution.Type == "" {
		return "untyped"
	}
	return solution.Type
}

func comparedWith(problem *Problem, reference *Solution, named names) ([]*Solution, string) {
	var compared []*Solution
	for _, name := range named {
		one := problem.Solution(name)
		if one == nil {
			return nil, fmt.Sprintf("the problem has no solution called %q; it has %s", name, solutionNames(problem))
		}
		if one == reference {
			return nil, fmt.Sprintf("%s is the reference; compare it with another solution", name)
		}
		compared = append(compared, one)
	}
	if len(named) == 0 {
		for _, one := range problem.Solutions {
			if one != reference && one.Type != "DONT_RUN" {
				compared = append(compared, one)
			}
		}
	}
	if len(compared) == 0 {
		return nil, fmt.Sprintf("the problem has no solution to compare %s with; name one with --solution",
			reference.Name)
	}
	var unbreakable []string
	for _, one := range compared {
		if !breakable(one.Type) {
			unbreakable = append(unbreakable, one.Name+", which "+kindOf(one))
		}
	}
	if len(unbreakable) == len(compared) {
		what := "the type of " + unbreakable[0]
		if len(unbreakable) > 1 {
			what = "the types of " + strings.Join(unbreakable, "; ")
		}
		return nil, fmt.Sprintf("no verdict breaks %s; stress stops only where a solution breaks its type, "+
			"so declare the one you suspect CORRECT", what)
	}
	return compared, ""
}

func kindOf(solution *Solution) string {
	if solution.Type == "" {
		return "has none"
	}
	return "is " + solution.Type
}

func breakable(kind string) bool {
	_, limited := passes[kind]
	return kind == "CORRECT" || limited
}

func breaksItsType(kind string, verdict Verdict) bool {
	if kind == "CORRECT" {
		return verdict != Accepted
	}
	allowed, limited := passes[kind]
	return limited && !among(verdict, allowed)
}

var rangeToken = regexp.MustCompile(`\[(-?[0-9]+)\.\.(-?[0-9]+)\]`)

type span struct{ low, high int64 }

func (s span) draw() int64 {
	width := uint64(s.high) - uint64(s.low)
	if width == math.MaxUint64 {
		return int64(rand.Uint64())
	}
	return s.low + int64(rand.Uint64N(width+1))
}

type argumentPattern struct {
	words []string
	spans map[string]span
}

func parseArguments(words []string) (argumentPattern, error) {
	pattern := argumentPattern{words: append([]string{}, words...), spans: map[string]span{}}
	for _, word := range pattern.words {
		for _, token := range rangeToken.FindAllStringSubmatch(word, -1) {
			low, lowErr := strconv.ParseInt(token[1], 10, 64)
			high, highErr := strconv.ParseInt(token[2], 10, 64)
			if lowErr != nil || highErr != nil {
				return pattern, fmt.Errorf("%s does not fit in 64 bits", token[0])
			}
			if low > high {
				return pattern, fmt.Errorf("%s is empty; write the smaller end first, as [%d..%d]", token[0], high, low)
			}
			pattern.spans[token[0]] = span{low, high}
		}
	}
	return pattern, nil
}

func (p argumentPattern) resolve() []string {
	out := make([]string, 0, len(p.words)+1)
	for _, word := range p.words {
		out = append(out, rangeToken.ReplaceAllStringFunc(word, func(token string) string {
			return strconv.FormatInt(p.spans[token].draw(), 10)
		}))
	}
	return append(out, fmt.Sprintf("%016x", rand.Uint64()))
}

type stressReport struct {
	Generator  string       `json:"generator"`
	Arguments  []string     `json:"arguments"`
	Reference  string       `json:"reference"`
	Solutions  []string     `json:"solutions"`
	Iterations int          `json:"iterations"`
	Passed     int          `json:"passed"`
	Deadline   bool         `json:"deadline"`
	Stopped    *stressRun   `json:"stopped,omitempty"`
	Failed     []*stressRun `json:"failed,omitempty"`
}

type stressRun struct {
	Index     int            `json:"index"`
	Verdict   string         `json:"verdict"`
	Arguments []string       `json:"arguments"`
	Why       string         `json:"why,omitempty"`
	Kept      string         `json:"kept,omitempty"`
	Results   []stressResult `json:"results"`

	warnings []Warning
	files    []string
	stage    string
}

type stressResult struct {
	Solution   string  `json:"solution"`
	Type       string  `json:"type"`
	Verdict    Verdict `json:"verdict"`
	MS         int     `json:"ms"`
	Message    string  `json:"message"`
	Unexpected bool    `json:"unexpected"`
}

func (s *session) stressTest(ctx context.Context, shop *Workspace) int {
	plan := s.planned
	report := &stressReport{Generator: plan.gen, Arguments: append([]string{}, plan.pattern.words...),
		Reference: plan.reference.Name, Solutions: []string{}, Iterations: plan.iterations}
	for _, one := range plan.compared {
		report.Solutions = append(report.Solutions, one.Name)
	}
	s.result.Stress = report
	fmt.Fprintf(s.out, "stress: %s against %s, comparing %s; at most %d iterations in %d s\n",
		callOf(plan.gen, plan.pattern.words), plan.reference.Name,
		strings.Join(report.Solutions, ", "), plan.iterations, plan.timeout)

	if err := shop.BuildAll(ctx, append([]*Solution{plan.reference}, plan.compared...)); err != nil {
		return s.fail(err)
	}
	if err := os.RemoveAll(filepath.Join(shop.Dir, "stress")); err != nil {
		return s.fail(err)
	}
	limited, stop := context.WithTimeout(ctx, time.Duration(plan.timeout)*time.Second)
	defer stop()

	var found Findings
	cut := ""
	for index := 1; index <= plan.iterations; index++ {
		one, err := shop.iterate(limited, plan, index)
		if ctx.Err() != nil {
			return s.fail(errors.New("the stress was interrupted"))
		}
		if limited.Err() != nil {
			report.Deadline, cut = true, one.stage
			break
		}
		if err != nil {
			return s.fail(err)
		}
		found = append(found, shop.findingsOf(one.warnings)...)
		if s.verbose {
			fmt.Fprintf(s.out, "  %d %s %s\n", index, one.Verdict,
				callOf(plan.gen, one.Arguments))
		}
		if one.Verdict == stressPassed {
			report.Passed++
			if err := os.RemoveAll(shop.iterationDir(index)); err != nil {
				return s.fail(err)
			}
			continue
		}
		s.printIteration(plan, one)
		if !s.stress.keepGoing || one.Verdict == stressCounterexample {
			report.Stopped = one
			break
		}
		report.Failed = append(report.Failed, one)
	}

	if len(found) > 0 {
		fmt.Fprintln(s.out)
	}
	code := s.report(found)
	if report.Stopped != nil || len(report.Failed) > 0 {
		code = max(code, 1)
	}
	if report.Deadline && report.Passed == 0 && len(report.Failed) == 0 {
		return max(code, s.fail(fmt.Errorf("the %d s timeout ended the stress in iteration 1 while %s ran, "+
			"before any iteration passed; give it a longer --timeout", plan.timeout, cut)))
	}
	unpassed := ""
	if len(report.Failed) > 0 {
		unpassed = fmt.Sprintf(" and %d did not", len(report.Failed))
	}
	switch one := report.Stopped; {
	case one == nil && report.Deadline:
		fmt.Fprintf(s.out, "eo-judge: the %d s timeout ended the stress after %d iterations passed%s, "+
			"with no counterexample\n", plan.timeout, report.Passed, unpassed)
	case one == nil:
		fmt.Fprintf(s.out, "eo-judge: %d iterations passed%s, with no counterexample\n", report.Passed, unpassed)
	case one.Verdict == stressCounterexample:
		fmt.Fprintf(s.out, "eo-judge: iteration %d of %d is a counterexample\n", one.Index, plan.iterations)
	case one.Verdict == stressInvalid:
		fmt.Fprintf(s.out, "eo-judge: iteration %d of %d is INVALID: the generator made an input the validator refuses\n",
			one.Index, plan.iterations)
	default:
		fmt.Fprintf(s.out, "eo-judge: iteration %d of %d is %s\n", one.Index, plan.iterations, one.Verdict)
	}
	return code
}

func (s *session) printIteration(plan *stressPlan, one *stressRun) {
	if s.work != "" {
		one.Kept = filepath.Join(s.work, "stress", strconv.Itoa(one.Index))
	}
	switch one.Verdict {
	case stressCounterexample:
		fmt.Fprintf(s.out, "\niteration %d: %s\n", one.Index, one.Verdict)
	case stressInvalid:
		fmt.Fprintf(s.out, "\niteration %d: %s, the generator's fault: %s\n", one.Index, one.Verdict, one.Why)
	default:
		fmt.Fprintf(s.out, "\niteration %d: %s: %s\n", one.Index, one.Verdict, one.Why)
	}
	for _, result := range one.Results {
		line := fmt.Sprintf("  %s: %s %dms", result.Solution, result.Verdict, result.MS)
		if result.Unexpected {
			line += ", which breaks its type " + result.Type
		}
		if result.Message != "" {
			line += ": " + result.Message
		}
		fmt.Fprintln(s.out, line)
	}
	fmt.Fprintf(s.out, "  %s\n", pasteable(plan.gen, one.Arguments))
	if one.Kept != "" {
		fmt.Fprintf(s.out, "  kept in %s: %s\n", one.Kept, strings.Join(one.files, ", "))
	}
}

func callOf(script string, args []string) string {
	words := []string{script}
	for _, one := range args {
		if strings.ContainsAny(one, " \t\n\"'\\") || one == "" {
			one = strconv.Quote(one)
		}
		words = append(words, one)
	}
	return strings.Join(words, " ")
}

func pasteable(script string, args []string) string {
	quoted := make([]string, len(args))
	for at, one := range args {
		body, _ := json.Marshal(one)
		quoted[at] = string(body)
	}
	name, _ := json.Marshal(script)
	return fmt.Sprintf(`"generator": {"script": %s, "arguments": [%s]}`, name, strings.Join(quoted, ", "))
}

func (w *Workspace) iterationDir(index int) string {
	return filepath.Join(w.Dir, "stress", strconv.Itoa(index))
}

func stressMetadata(index int) map[string]string {
	return map[string]string{"EOLYMP": "1", "TEST_ID": "", "TEST_COST": "0", "TEST_INDEX": strconv.Itoa(index),
		"TEST_GROUP": "0"}
}

func (w *Workspace) iterate(ctx context.Context, plan *stressPlan, index int) (*stressRun, error) {
	one := &stressRun{Index: index, Arguments: plan.pattern.resolve(), Results: []stressResult{},
		stage: "the generator " + plan.gen}
	dir := w.iterationDir(index)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return one, err
	}
	made := &Prepared{Input: filepath.Join(dir, "input.txt"), Answer: filepath.Join(dir, "answer.txt")}

	why, err := w.generateInto(ctx, plan.gen, one, made.Input)
	if err != nil {
		return one, err
	}
	one.files = append(one.files, "input.txt")
	if why != "" {
		one.Verdict, one.Why = stressBroken, why
		return one, nil
	}

	if w.Problem.Validator != nil {
		one.stage = "the validator"
		validator, err := w.Build(ctx, "validator", w.Problem.Validator)
		if err != nil {
			return one, err
		}
		status, err := validatingWithin(ctx, validator, w.validatorLimit, made.Input)
		if err != nil {
			return one, err
		}
		switch {
		case status.TimedOut:
			one.Verdict, one.Why = stressBroken, "the validator "+ended(status, w.validatorLimit)
			return one, nil
		case status.ExitCode != 0:
			one.Verdict = stressInvalid
			one.Why = "the validator refuses the input: " + firstLine(string(status.Stdout)+string(status.Stderr))
			return one, nil
		}
	}

	limit := w.Problem.TimeLimit
	if limit <= 0 {
		limit = 10000
	}
	one.stage = "the reference " + plan.reference.Name
	reference := w.Programs[solutionName(plan.reference.Name)]
	status, err := w.batch(ctx, made, reference, dir, made.Answer, limit)
	if err != nil {
		return one, err
	}
	one.files = append(one.files, "answer.txt")
	if status.TimedOut || status.Signal || status.ExitCode != 0 {
		one.Verdict, one.Why = stressBroken, fmt.Sprintf("the reference %s %s", plan.reference.Name, ended(status, limit))
		return one, nil
	}
	if err := made.seal(); err != nil {
		return one, err
	}

	checker, err := w.Build(ctx, "checker", w.Problem.Checker)
	if err != nil {
		return one, err
	}
	one.Verdict = stressPassed
	for _, solution := range plan.compared {
		one.stage = "the solution " + solution.Name
		at := trial{made: made, limit: limit, env: stressMetadata(index), work: filepath.Join(dir, solution.Name)}
		result, err := w.try(ctx, at, w.Programs[solutionName(solution.Name)], checker, nil, &RunResult{})
		if err != nil {
			return one, err
		}
		one.warnings = append(one.warnings, result.Warnings...)
		one.files = append(one.files, solution.Name+"/output.txt")
		unexpected := breaksItsType(solution.Type, result.Verdict)
		one.Results = append(one.Results, stressResult{Solution: solution.Name, Type: solution.Type,
			Verdict: result.Verdict, MS: result.Wall, Message: result.Message, Unexpected: unexpected})
		switch {
		case result.Verdict == Failure && one.Why == "":
			one.Verdict = stressBroken
			one.Why = fmt.Sprintf("the checker failed on the output of %s: %s", solution.Name, result.Message)
		case unexpected && one.Verdict == stressPassed:
			one.Verdict = stressCounterexample
		}
	}
	return one, nil
}

func (w *Workspace) generateInto(ctx context.Context, gen string, one *stressRun, input string) (string, error) {
	built, err := w.script(ctx, gen)
	if err != nil {
		return "", err
	}
	file, err := os.Create(input)
	if err != nil {
		return "", err
	}
	status, err := built.jury(ctx, w.generatorLimit, Invocation{Args: one.Arguments, Stdout: file})
	closed := file.Close()
	call := callOf(gen, one.Arguments)
	if err != nil {
		return "", fmt.Errorf("the generator %s: %w", call, err)
	}
	one.warnings = warningsIn(gen, string(status.Stderr))
	if status.ExitCode != 0 {
		return fmt.Sprintf("the generator %s %s", call, ended(status, w.generatorLimit)), nil
	}
	if closed != nil {
		return "", fmt.Errorf("the input could not be written: %w; check the space left for the workspace", closed)
	}
	return "", nil
}
