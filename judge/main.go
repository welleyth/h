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
  eo-judge version                           the version of eo-judge

  --strict   make every warning fatal
  --work     keep the workspace in this directory
  -v         print every run of every test after its testset

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

	command := args[0]
	switch command {
	case "version", "--version", "-version":
		fmt.Fprintf(out, "eo-judge %s\n", version)
		return 0
	case "help", "-h", "-help", "--help":
		fmt.Fprint(out, usage)
		return 0
	}
	flags := flag.NewFlagSet(command, flag.ContinueOnError)
	flags.SetOutput(errs)
	strict := flags.Bool("strict", false, "make every warning fatal")
	deep := flags.Bool("deep", false, "run the slow hostile outputs")
	only := flags.String("solution", "", "judge one solution by name")
	work := flags.String("work", "", "keep the workspace here")
	verbose := flags.Bool("v", false, "print every run")
	flags.Usage = func() { fmt.Fprint(errs, usage) }
	var positional []string
	for rest := args[1:]; ; {
		if err := flags.Parse(rest); err != nil {
			if errors.Is(err, flag.ErrHelp) {
				fmt.Fprint(out, usage)
				return 0
			}
			return 2
		}
		if flags.NArg() == 0 {
			break
		}
		positional = append(positional, flags.Arg(0))
		rest = flags.Args()[1:]
	}

	if len(positional) != 1 {
		fmt.Fprint(errs, usage)
		return 2
	}
	dir := positional[0]

	problem, err := LoadProblem(dir)
	if err != nil {
		fmt.Fprintln(errs, "eo-judge:", err)
		return 3
	}

	if *only != "" && problem.Solution(*only) == nil {
		var known []string
		for _, one := range problem.Solutions {
			known = append(known, one.Name)
		}
		fmt.Fprintf(errs, "eo-judge: the problem has no solution called %q; it has %s\n", *only,
			strings.Join(known, ", "))
		return 2
	}

	if command == "lint" {
		return report(out, Lint(problem), *strict)
	}

	space := *work
	if space == "" {
		space, err = os.MkdirTemp(temp, "eo-judge-")
		if err != nil {
			fmt.Fprintln(errs, "eo-judge:", err)
			return 3
		}
		defer os.RemoveAll(space)
	} else if err := os.MkdirAll(space, 0o755); err != nil {
		fmt.Fprintln(errs, "eo-judge:", err)
		return 3
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	go func() {
		<-ctx.Done()
		stop()
	}()
	shop := NewWorkspace(problem, space)
	shop.Temp = temp

	switch command {
	case "check":
		found, err := shop.Check(ctx, *deep)
		if err != nil {
			fmt.Fprintln(errs, "eo-judge:", err)
			return 3
		}
		return report(out, append(found, Lint(problem)...), *strict)
	case "run":
		return runProblem(ctx, shop, *only, *strict, *verbose, out, errs)
	default:
		fmt.Fprint(errs, usage)
		return 2
	}
}

func runProblem(ctx context.Context, shop *Workspace, only string, strict, verbose bool, out, errs io.Writer) int {
	judged := shop.Problem.Judged(only)
	if err := shop.BuildAll(ctx, judged); err != nil {
		fmt.Fprintln(errs, "eo-judge:", err)
		return 3
	}
	if err := shop.Generate(ctx); err != nil {
		fmt.Fprintln(errs, "eo-judge:", err)
		return 3
	}
	if err := shop.Validate(ctx); err != nil {
		fmt.Fprintln(errs, "eo-judge:", err)
		return 3
	}

	var found Findings
	invalid := 0
	for _, made := range shop.sorted() {
		if shop.Problem.Validator != nil && !made.Valid {
			invalid++
			fmt.Fprintf(out, "test %d:%d is invalid: %s\n", made.Group, made.Test.Index, made.Why)
		}
		found = append(found, shop.findingsOf(made.Warnings)...)
	}
	if invalid > 0 {
		fmt.Fprintf(out, "\n%d test(s) the validator refuses\n", invalid)
	}

	for _, solution := range judged {
		attempt, err := shop.Evaluate(ctx, solution.Name, &Program{Source: solution.Source})
		if err != nil {
			fmt.Fprintln(errs, "eo-judge:", err)
			return 3
		}
		fmt.Fprintf(out, "\n%s: %s, %g\n", solution.Name, attempt.Verdict, attempt.Score)
		for _, group := range attempt.Groups {
			fmt.Fprintf(out, "  testset %-2d %-20s %7.4g of %-7.4g", group.Index, group.Verdict, group.Score, group.Cost)
			fmt.Fprintf(out, "  %s\n", tally(group))
			if verbose {
				for _, one := range group.Runs {
					fmt.Fprintf(out, "    %d:%d %s %dms %s\n", one.Group, one.Index, one.Verdict, one.Wall, one.Message)
				}
			}
		}
		for _, group := range attempt.Groups {
			for _, one := range group.Runs {
				found = append(found, shop.findingsOf(one.Warnings)...)
			}
		}
	}

	fmt.Fprintln(out)
	return report(out, found, strict)
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
	out := ""
	for at, one := range kinds {
		if at > 0 {
			out += ", "
		}
		out += one
	}
	return out
}

func report(out io.Writer, found Findings, strict bool) int {
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
