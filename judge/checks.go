package main

import (
	"bytes"
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

type probe struct {
	exit int
	log  string
}

func (w *Workspace) probeChecker(ctx context.Context, made *Prepared, output string, cost float64) (probe, error) {
	checker, err := w.Build(ctx, "checker", w.Problem.Checker)
	if err != nil {
		return probe{}, err
	}
	work := filepath.Join(w.Dir, "probe")
	if err := os.MkdirAll(work, 0o755); err != nil {
		return probe{}, err
	}
	status, said, err := runChecker(ctx, checker, made, output, work, map[string]string{
		"EOLYMP": "1", "TEST_ID": "probe", "TEST_COST": fmt.Sprint(cost), "TEST_INDEX": "1", "TEST_GROUP": "1",
	})
	if err != nil {
		return probe{}, err
	}
	if status.TimedOut {
		return probe{exit: -1, log: "the checker did not finish"}, nil
	}
	return probe{exit: status.ExitCode, log: firstLine(string(said))}, nil
}

func hostileOutputs(answer, input []byte, deep bool) map[string][]byte {
	size := 2 << 20
	if deep {
		size = 100 << 20
	}
	flood := bytes.Repeat([]byte("123456789 "), size/10)

	changed := append([]byte(nil), answer...)
	if fields := bytes.Fields(changed); len(fields) > 0 {
		changed = bytes.Replace(changed, fields[0], []byte("999999999"), 1)
	}

	half := answer
	if len(half) > 1 {
		half = half[:len(half)/2]
	}

	return map[string][]byte{
		"nothing at all":        {},
		"the input echoed back": input,
		"a huge count":          []byte("2147483647\n"),
		"negative numbers":      []byte("-1 -2147483648\n"),
		"garbage":               []byte("hello world\n@@@\n"),
		"a NUL byte":            append([]byte("1\x002\n"), answer...),
		"a truncated answer":    half,
		"a flood of tokens":     flood,
		"one token changed":     changed,
	}
}

func spacedOut(answer []byte) []byte {
	lines := bytes.Split(bytes.TrimRight(answer, "\n"), []byte("\n"))
	var out bytes.Buffer
	for _, line := range lines {
		out.Write(line)
		out.WriteString("  \r\n")
	}
	return out.Bytes()
}

func (w *Workspace) checkerChecks(ctx context.Context, found *Findings, deep bool) error {
	if w.Problem.Checker == nil || w.Problem.Interactive() {
		return nil
	}

	partial := false
	for at, made := range w.sorted() {
		answer, err := os.ReadFile(made.Answer)
		if err != nil {
			return err
		}
		input, err := os.ReadFile(made.Input)
		if err != nil {
			return err
		}
		where := fmt.Sprintf("test %d:%d", made.Group, made.Test.Index)

		got, err := w.probeChecker(ctx, made, made.Answer, made.Test.Score)
		if err != nil {
			return err
		}
		if got.exit != 0 {
			found.warn("EO801", where, fmt.Sprintf("the checker does not accept its own answer: %s", got.log),
				"the checker and the answer files disagree; a contestant cannot pass this test")
		}

		if !w.Problem.ExactFormat {
			loose := filepath.Join(w.Dir, "probe", "loose.txt")
			if err := os.WriteFile(loose, spacedOut(answer), 0o644); err != nil {
				return err
			}
			got, err := w.probeChecker(ctx, made, loose, made.Test.Score)
			if err != nil {
				return err
			}
			if got.exit != 0 {
				found.warn("EO818", where,
					fmt.Sprintf("the checker rejects its own answer with CRLF and trailing spaces: %s", got.log),
					"a contestant's output is not normalised; accept the whitespace or declare an exact format")
			}
		}

		if at > 0 {
			continue
		}

		for name, body := range hostileOutputs(answer, input, deep) {
			if name == "one token changed" && !w.Problem.Unique {
				continue
			}
			path := filepath.Join(w.Dir, "probe", "hostile.txt")
			if err := os.WriteFile(path, body, 0o644); err != nil {
				return err
			}
			got, err := w.probeChecker(ctx, made, path, made.Test.Score)
			if err != nil {
				return err
			}
			if got.exit == 7 {
				partial = true
			}
			switch {
			case got.exit == 0 && name == "nothing at all":
				found.warn("EO802", where, "the checker accepts an empty output",
					"it is not reading the contestant's answer")
			case got.exit == 0 && name == "the input echoed back":
				found.warn("EO803", where, "the checker accepts the input echoed back as the output",
					"it is not comparing enough")
			case got.exit == 0 && name == "one token changed":
				found.warn("EO804", where, "the checker accepts the answer with one token changed",
					"the problem declares a unique answer, so this must be wrong")
			case got.exit == 0:
				continue
			case got.exit == 1 || got.exit == 2 || got.exit == 7:
				continue
			default:
				found.warn("EO805", where,
					fmt.Sprintf("%s makes the checker exit %d: %s", name, got.exit, got.log),
					"a contestant's output must give a wrong answer, never a crash or a jury error")
			}
		}
	}

	if partial {
		for _, testset := range w.Problem.Testsets {
			if testset.ScoringMode == "ALL" {
				found.warn("EO817", fmt.Sprintf("testset %d", testset.Index),
					"a checker that gives partial scores sits on an ALL testset",
					"ALL pays nothing unless every test passes, so the fractions are thrown away")
			}
		}
	}
	return nil
}

func (w *Workspace) sorted() []*Prepared {
	var out []*Prepared
	for _, testset := range w.Problem.Testsets {
		for _, test := range testset.Tests {
			if made, known := w.Tests[reference(&Planned{Group: testset.Index, Test: test})]; known {
				out = append(out, made)
			}
		}
	}
	return out
}

func (w *Workspace) structureChecks(ctx context.Context, found *Findings) error {
	seen := map[string]string{}
	for _, made := range w.sorted() {
		body, err := os.ReadFile(made.Input)
		if err != nil {
			return err
		}
		key := fmt.Sprintf("%d %s", made.Group, keyOf(string(body)))
		where := fmt.Sprintf("test %d:%d", made.Group, made.Test.Index)
		if earlier, known := seen[key]; known {
			found.warn("EO809", where, fmt.Sprintf("it is byte for byte the same as %s", earlier),
				"drop it, or generate a different test")
		} else {
			seen[key] = where
		}
	}

	for _, testset := range w.Problem.Testsets {
		if testset.Index == 0 {
			continue
		}
		if len(testset.Tests) < 2 {
			found.note("EO811", fmt.Sprintf("testset %d", testset.Index),
				fmt.Sprintf("the subtask has %d test(s)", len(testset.Tests)),
				"one input decides the whole subtask")
		}
	}

	if w.Problem.Validator == nil {
		return nil
	}

	built, err := w.Build(ctx, "validator", w.Problem.Validator)
	if err != nil {
		return err
	}

	for _, made := range w.sorted() {
		where := fmt.Sprintf("test %d:%d", made.Group, made.Test.Index)
		status, err := built.jury(ctx, validatorLimit, Invocation{Args: []string{made.Input}})
		if err != nil {
			return err
		}
		if status.ExitCode == juryError {
			found.warn("EO806", where,
				fmt.Sprintf("the validator could not run: %s", firstLine(string(status.Stdout)+string(status.Stderr))),
				"the validator is broken, so every test reads invalid — fix the validator, not the tests")
		} else if status.ExitCode != 0 {
			found.warn("EO806", where,
				fmt.Sprintf("the test is invalid with no --group: %s", firstLine(string(status.Stdout)+string(status.Stderr))),
				"a stress run passes no group, so this input would be called invalid")
		}

		for _, testset := range w.Problem.Testsets {
			if !dependsOn(w.Problem, testset, made.Group) {
				continue
			}
			status, err := built.jury(ctx, validatorLimit,
				Invocation{Args: []string{made.Input, "--group", fmt.Sprint(testset.Index)}})
			if err != nil {
				return err
			}
			if status.ExitCode != 0 {
				found.warn("EO810", where,
					fmt.Sprintf("testset %d depends on testset %d but the test is invalid there: %s",
						testset.Index, made.Group, firstLine(string(status.Stdout)+string(status.Stderr))),
					"the groups do not nest the way the scoring assumes")
			}
		}
	}
	return nil
}

func dependsOn(problem *Problem, testset *Testset, group int) bool {
	for _, one := range testset.Dependencies {
		if one == group {
			return true
		}
	}
	return false
}

var describedValue = regexp.MustCompile(`^eo-describe value (\S+) (\S+) (\S+) (\S+) low=(yes|no) high=(yes|no)$`)
var describedFeature = regexp.MustCompile(`^eo-describe feature (.+) seen=(yes|no)$`)

func (w *Workspace) coverageChecks(ctx context.Context, found *Findings) error {
	if w.Problem.Validator == nil {
		return nil
	}

	type reach struct{ low, high bool }
	bounds := map[int]map[string]*reach{}
	kinds := map[int]map[string]string{}
	limits := map[int]map[string][2]string{}
	features := map[string]bool{}

	for _, made := range w.sorted() {
		said, err := w.describe(ctx, made)
		if err != nil {
			return err
		}
		for _, line := range strings.Split(said, "\n") {
			if parts := describedValue.FindStringSubmatch(line); parts != nil {
				if bounds[made.Group] == nil {
					bounds[made.Group] = map[string]*reach{}
					kinds[made.Group] = map[string]string{}
					limits[made.Group] = map[string][2]string{}
				}
				at := bounds[made.Group][parts[1]]
				if at == nil {
					at = &reach{}
					bounds[made.Group][parts[1]] = at
					kinds[made.Group][parts[1]] = parts[2]
					limits[made.Group][parts[1]] = [2]string{parts[3], parts[4]}
				} else if known := limits[made.Group][parts[1]]; known != [2]string{parts[3], parts[4]} {
					limits[made.Group][parts[1]] = [2]string{"*", "*"}
				}
				at.low = at.low || parts[5] == "yes"
				at.high = at.high || parts[6] == "yes"
			}
			if parts := describedFeature.FindStringSubmatch(line); parts != nil {
				features[parts[1]] = features[parts[1]] || parts[2] == "yes"
			}
		}
	}

	for group, named := range bounds {
		if group == 0 {
			continue
		}
		for name, at := range named {
			where := fmt.Sprintf("testset %d", group)
			kind := kinds[group][name]
			edge := limits[group][name]
			if !at.low {
				found.note("EO807", where, boundMessage(kind, name, edge[0], "lower"),
					"the smallest case a subtask admits is where off-by-one lives")
			}
			if !at.high {
				found.warn("EO807", where, boundMessage(kind, name, edge[1], "upper"),
					"a maximal test that is not maximal; generate one that reaches it")
			}
		}
	}

	for name, seen := range features {
		if !seen {
			found.warn("EO808", "", fmt.Sprintf("no test has the feature %q", name),
				"generate one, or stop declaring it")
		}
	}
	return nil
}

func boundMessage(kind, name, edge, end string) string {
	if edge == "*" {
		return fmt.Sprintf("no test reaches %s at its %s bound, which is computed per test", name, end)
	}
	if kind == "length" {
		return fmt.Sprintf("no test has %s of length %s", name, edge)
	}
	return fmt.Sprintf("no test reaches %s = %s", name, edge)
}

var describedOption = regexp.MustCompile(`^eo-describe option (\S+) an? (integer|number) (\S+)\.\.(\S+)`)

func (w *Workspace) generatorChecks(ctx context.Context, found *Findings) error {
	for name, script := range w.Problem.Scripts {
		built, err := w.Build(ctx, "script."+name, script)
		if err != nil {
			return err
		}
		where := "script " + name

		used := w.argumentsFor(name)
		if len(used) == 0 {
			continue
		}

		first, err := w.generateOnce(ctx, built, used[0])
		if err != nil {
			return err
		}
		again, err := w.generateOnce(ctx, built, used[0])
		if err != nil {
			return err
		}
		if !bytes.Equal(first, again) {
			found.warn("EO812", where, "two runs with the same arguments give different bytes",
				"unspecified argument order, std::shuffle, unordered iteration or signed char")
		}

		if other := otherCompiler(ctx, w.tools.cxx); other != "" {
			twin, err := toolchain{cxx: other, cache: w.tools.cache}.build(ctx, w.Problem, "twin."+name, script, w.Dir)
			if err != nil {
				found.note("EO812", where, fmt.Sprintf("it does not build with %s: %v", other, err),
					"a generator has to build with both compilers the judge may use")
			} else {
				crossed, err := w.generateOnce(ctx, twin, used[0])
				if err != nil {
					return err
				}
				if !bytes.Equal(first, crossed) {
					found.warn("EO812", where,
						fmt.Sprintf("%s and %s give different bytes for the same arguments", w.tools.cxx, other),
						"the test depends on the standard library, not only on the seed")
				}
			}
		}

		status, err := built.jury(ctx, validatorLimit, Invocation{Args: []string{"--eo-describe"}})
		if err != nil {
			return err
		}
		for _, line := range strings.Split(string(status.Stdout), "\n") {
			parts := describedOption.FindStringSubmatch(line)
			if parts == nil {
				continue
			}
			for _, edge := range []string{parts[3], parts[4]} {
				args := withOption(used[0], parts[1], edge)
				body, err := w.generateOnce(ctx, built, args)
				if err != nil {
					found.warn("EO813", where, fmt.Sprintf("%s=%s does not generate: %v", parts[1], edge, err),
						"the extremes of a declared option must produce a valid test")
					continue
				}
				if why := w.validateBody(ctx, body); why != "" {
					found.warn("EO813", where,
						fmt.Sprintf("%s=%s produces an invalid test: %s", parts[1], edge, why),
						"the extremes of a declared option must produce a valid test")
				}
			}
		}
	}
	return nil
}

func withOption(args []string, name, value string) []string {
	wanted := "-" + name + "="
	out := make([]string, 0, len(args)+1)
	replaced := false
	for _, one := range args {
		if strings.HasPrefix(one, wanted) {
			out = append(out, wanted+value)
			replaced = true
			continue
		}
		out = append(out, one)
	}
	if !replaced {
		out = append(out, wanted+value)
	}
	return out
}

func (w *Workspace) argumentsFor(name string) [][]string {
	var out [][]string
	for _, testset := range w.Problem.Testsets {
		for _, test := range testset.Tests {
			if test.Generator != nil && test.Generator.Script == name {
				out = append(out, test.Generator.Arguments)
			}
		}
	}
	return out
}

func (w *Workspace) generateOnce(ctx context.Context, built *Built, args []string) ([]byte, error) {
	var out bytes.Buffer
	status, err := built.jury(ctx, generatorLimit, Invocation{Args: args, Stdout: &out})
	if err != nil {
		return nil, err
	}
	if status.ExitCode != 0 {
		return nil, fmt.Errorf("exit %d: %s", status.ExitCode, firstLine(string(status.Stderr)))
	}
	return out.Bytes(), nil
}

func (w *Workspace) validateBody(ctx context.Context, body []byte) string {
	if w.Problem.Validator == nil {
		return ""
	}
	built, err := w.Build(ctx, "validator", w.Problem.Validator)
	if err != nil {
		return err.Error()
	}
	path := filepath.Join(w.Dir, "probe", "extreme.txt")
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err.Error()
	}
	if err := os.WriteFile(path, body, 0o644); err != nil {
		return err.Error()
	}
	status, err := built.jury(ctx, validatorLimit, Invocation{Args: []string{path}})
	if err != nil {
		return err.Error()
	}
	if status.ExitCode == 0 {
		return ""
	}
	return firstLine(string(status.Stdout) + string(status.Stderr))
}

func otherCompiler(ctx context.Context, cxx string) string {
	mine := versionOf(ctx, cxx)
	for _, candidate := range []string{"g++", "clang++"} {
		said := versionOf(ctx, candidate)
		if said == "" || said == mine {
			continue
		}
		return candidate
	}
	return ""
}

var versions = struct {
	sync.Mutex
	said map[string]string
}{said: map[string]string{}}

const versionLimit = 10 * time.Second

func versionOf(ctx context.Context, name string) string {
	versions.Lock()
	defer versions.Unlock()
	if said, known := versions.said[name]; known {
		return said
	}
	said := ""
	if path, err := exec.LookPath(name); err == nil {
		limited, stop := context.WithTimeout(ctx, versionLimit)
		defer stop()
		if out, err := grouped(limited, path, "--version").Output(); err == nil {
			said = firstLine(string(out))
		}
	}
	if ctx.Err() == nil {
		versions.said[name] = said
	}
	return said
}
