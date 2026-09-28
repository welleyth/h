package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/signal"
	"sort"
	"strings"
	"syscall"
)

const version = "2.0.0"

const usage = `eo-judge runs an Eolymp problem the way the judge does.

  eo-judge run <problem> [--solution name]   build, generate, validate, judge, score
  eo-judge check <problem> [--deep]          the whole-problem and configuration checks
  eo-judge lint <problem>                    what the header cannot see
  eo-judge init <dir> [--type program|interactive|phases]
                                             write a new problem that run passes
  eo-judge version                           the version of eo-judge

  --strict   make every warning fatal
  --work     keep the workspace in this directory
  -v         print every run of every test after its testset
  --json     print the result as one JSON object instead of text
  --expect   with run, exit 1 when a solution breaks its declared type

Flags may come before or after the problem.
`

func main() {
	os.Exit(realMain(os.Args[1:], "", os.Stdout, os.Stderr))
}

func realMain(args []string, temp string, out, errs io.Writer) int {
	if len(args) < 1 {
		fmt.Fprint(errs, usage)
		return 2
	}

	switch args[0] {
	case "version", "--version", "-version":
		fmt.Fprintf(out, "eo-judge %s\n", version)
		return 0
	case "help", "-h", "-help", "--help":
		fmt.Fprint(out, usage)
		return 0
	}
	switch args[0] {
	case "init":
		return initProblem(args[1:], out, errs)
	case "run", "check", "lint":
	default:
		fmt.Fprintf(errs, "eo-judge: there is no command %q\n\n%s", args[0], usage)
		return 2
	}
	opts, code, parsed := parse(args, out, errs)
	if !parsed {
		return code
	}
	one := &session{options: opts, temp: temp, out: out, errs: errs, result: newOutcome(opts.dir)}
	if opts.json {
		one.out = io.Discard
	}
	code = one.run()
	if opts.json {
		one.result.Exit = code
		if err := one.result.write(out); err != nil {
			fmt.Fprintln(errs, "eo-judge:", err)
			return 3
		}
	}
	return code
}

type options struct {
	command, dir, only, work            string
	strict, deep, verbose, json, expect bool
}

func parse(args []string, out, errs io.Writer) (options, int, bool) {
	opts := options{command: args[0]}
	flags := flag.NewFlagSet(opts.command, flag.ContinueOnError)
	flags.BoolVar(&opts.strict, "strict", false, "make every warning fatal")
	flags.BoolVar(&opts.deep, "deep", false, "run the slow hostile outputs")
	flags.StringVar(&opts.only, "solution", "", "judge one solution by name")
	flags.StringVar(&opts.work, "work", "", "keep the workspace here")
	flags.BoolVar(&opts.verbose, "v", false, "print every run")
	flags.BoolVar(&opts.json, "json", false, "print the result as JSON")
	flags.BoolVar(&opts.expect, "expect", false, "fail when a solution breaks its declared type")
	dir, code, parsed := onePositional(flags, args[1:], out, errs)
	opts.dir = dir
	return opts, code, parsed
}

func onePositional(flags *flag.FlagSet, args []string, out, errs io.Writer) (string, int, bool) {
	flags.SetOutput(errs)
	flags.Usage = func() { fmt.Fprint(errs, usage) }
	var positional []string
	for rest := args; ; {
		if err := flags.Parse(rest); err != nil {
			if errors.Is(err, flag.ErrHelp) {
				fmt.Fprint(out, usage)
				return "", 0, false
			}
			return "", 2, false
		}
		if flags.NArg() == 0 {
			break
		}
		positional = append(positional, flags.Arg(0))
		rest = flags.Args()[1:]
	}
	if len(positional) != 1 {
		fmt.Fprint(errs, usage)
		return "", 2, false
	}
	return positional[0], 0, true
}

type session struct {
	options
	temp      string
	out, errs io.Writer
	result    *outcome
}

func (s *session) fail(err error) int {
	fmt.Fprintln(s.errs, "eo-judge:", err)
	s.result.Error = err.Error()
	return 3
}

func (s *session) report(found Findings) int {
	s.result.findings(ordered(found))
	return report(s.out, found, s.strict)
}

func (s *session) run() int {
	if s.expect && s.command != "run" {
		s.result.Error = "--expect judges solutions, so it applies to run only; use eo-judge run --expect"
		fmt.Fprintln(s.errs, "eo-judge:", s.result.Error)
		return 2
	}
	problem, err := LoadProblem(s.dir)
	if err != nil {
		return s.fail(err)
	}

	if s.only != "" && problem.Solution(s.only) == nil {
		var known []string
		for _, one := range problem.Solutions {
			known = append(known, one.Name)
		}
		s.result.Error = fmt.Sprintf("the problem has no solution called %q; it has %s", s.only,
			strings.Join(known, ", "))
		fmt.Fprintln(s.errs, "eo-judge:", s.result.Error)
		return 2
	}

	if s.command == "lint" {
		return s.report(Lint(problem))
	}
	if problem.Type == "COMMUNICATION" {
		return s.fail(errors.New("eo-judge does not run COMMUNICATION problems yet; judge one on Eolymp, " +
			"and lint reads its sources"))
	}

	space := s.work
	if space == "" {
		space, err = os.MkdirTemp(s.temp, "eo-judge-")
		if err != nil {
			return s.fail(err)
		}
		defer os.RemoveAll(space)
	} else if err := os.MkdirAll(space, 0o755); err != nil {
		return s.fail(err)
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	go func() {
		<-ctx.Done()
		stop()
	}()
	shop := NewWorkspace(problem, space)
	shop.Temp = s.temp

	switch s.command {
	case "check":
		found, err := shop.Check(ctx, s.deep)
		if err != nil {
			return s.fail(err)
		}
		return s.report(append(found, Lint(problem)...))
	default:
		return s.judge(ctx, shop)
	}
}

func (s *session) judge(ctx context.Context, shop *Workspace) int {
	out := s.out
	judged := shop.Problem.Judged(s.only)
	if err := shop.BuildAll(ctx, judged); err != nil {
		return s.fail(err)
	}
	if err := shop.Generate(ctx); err != nil {
		return s.fail(err)
	}
	if err := shop.Validate(ctx); err != nil {
		return s.fail(err)
	}

	var found Findings
	invalid := 0
	for _, made := range shop.sorted() {
		if shop.Problem.Validator != nil && !made.Valid {
			invalid++
			fmt.Fprintf(out, "test %d:%d is invalid: %s\n", made.Group, made.Test.Index, made.Why)
			s.result.Invalid = append(s.result.Invalid, invalidTest{made.Group, made.Test.Index, made.Why})
		}
		found = append(found, shop.findingsOf(made.Warnings)...)
	}
	if invalid > 0 {
		fmt.Fprintf(out, "\n%d test(s) the validator refuses\n", invalid)
	}

	broken := 0
	for _, solution := range judged {
		attempt, err := shop.Evaluate(ctx, solution.Name, &Program{Source: solution.Source})
		if err != nil {
			return s.fail(err)
		}
		s.result.attempt(solution, attempt)
		s.print(attempt)
		if s.expect && s.expected(solution, attempt) != "" {
			broken++
		}
		for _, group := range attempt.Groups {
			for _, one := range group.Runs {
				found = append(found, shop.findingsOf(one.Warnings)...)
			}
		}
	}

	fmt.Fprintln(out)
	code := s.report(found)
	if broken > 0 {
		fmt.Fprintf(out, "eo-judge: %d solution(s) break their declared type\n", broken)
		code = max(code, 1)
	}
	return code
}

func (s *session) print(attempt *Attempt) {
	fmt.Fprintf(s.out, "\n%s: %s, %g\n", attempt.Name, attempt.Verdict, attempt.Score)
	for _, group := range attempt.Groups {
		fmt.Fprintf(s.out, "  testset %-2d %-20s %7.4g of %-7.4g", group.Index, group.Verdict, group.Score, group.Cost)
		fmt.Fprintf(s.out, "  %s\n", tally(group))
		if s.verbose {
			for _, one := range group.Runs {
				fmt.Fprintf(s.out, "    %d:%d %s %dms %s\n", one.Group, one.Index, one.Verdict, one.Wall, one.Message)
			}
		}
	}
}

func (s *session) expected(solution *Solution, attempt *Attempt) string {
	why := breaks(solution, attempt)
	if why != "" {
		fmt.Fprintf(s.out, "  it is declared %s, but %s\n", solution.Type, why)
		s.result.Attempts[len(s.result.Attempts)-1].Breaks = why
	} else if note := unchecked(solution.Type); note != "" {
		fmt.Fprintf(s.out, "  %s\n", note)
	}
	return why
}

func tally(group *GroupResult) string {
	counted := map[Verdict]int{}
	for _, one := range group.Runs {
		counted[one.Verdict]++
	}
	kinds := make([]string, 0, len(counted))
	for verdict, count := range counted {
		kinds = append(kinds, fmt.Sprintf("%d %s", count, verdict))
	}
	sort.Strings(kinds)
	return strings.Join(kinds, ", ")
}

func ordered(found Findings) Findings {
	seen := map[string]bool{}
	var kept Findings
	for _, one := range found {
		key := one.Code + "|" + one.Where + "|" + one.Message
		if seen[key] {
			continue
		}
		seen[key] = true
		kept = append(kept, one)
	}
	sort.Slice(kept, func(i, j int) bool { return kept[i].before(kept[j]) })
	return kept
}

func report(out io.Writer, found Findings, strict bool) int {
	kept := ordered(found)
	warnings := 0
	for _, one := range kept {
		fmt.Fprintln(out, one)
		if one.Severity == "warning" {
			warnings++
		}
	}

	fmt.Fprintf(out, "\neo-judge: %d warning(s), %d note(s)\n", warnings, len(kept)-warnings)
	if strict && warnings > 0 {
		return 1
	}
	return 0
}
