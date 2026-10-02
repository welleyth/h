package main

import (
	"context"
	"fmt"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
)

var describedStat = regexp.MustCompile(`^eo-describe stat (\S+) (-?[0-9]+)$`)

type testDescription struct {
	Group    int              `json:"group"`
	Test     int              `json:"test"`
	Bytes    int64            `json:"bytes"`
	Features []string         `json:"features"`
	Stats    map[string]int64 `json:"stats"`
	Invalid  string           `json:"invalid,omitempty"`
	Broken   string           `json:"broken,omitempty"`
	Unmade   string           `json:"unmade,omitempty"`

	declared []string
	order    []string
}

func (s *session) describeTests(ctx context.Context, shop *Workspace) int {
	shop.inputsOnly = true
	generated := shop.Generate(ctx)
	if generated != nil && (ctx.Err() != nil || len(shop.unmade) == 0) {
		return s.fail(generated)
	}
	var validator *Built
	if shop.Problem.Validator != nil {
		built, err := shop.Build(ctx, "validator", shop.Problem.Validator)
		if err != nil {
			return s.fail(err)
		}
		validator = built
	}
	described := []testDescription{}
	declared := map[string]bool{}
	failed := 0
	for _, testset := range shop.Problem.Testsets {
		for _, test := range testset.Tests {
			key := reference(&Planned{Group: testset.Index, Test: test})
			made, known := shop.Tests[key]
			if !known {
				described = append(described, testDescription{Group: testset.Index, Test: test.Index,
					Unmade: shop.unmade[key]})
				failed++
				continue
			}
			one, err := describeOne(ctx, validator, made)
			if err != nil {
				return s.fail(err)
			}
			for _, name := range one.declared {
				declared[name] = true
			}
			if one.Broken != "" {
				failed++
			}
			described = append(described, one)
		}
	}
	s.result.Tests = &described
	if validator != nil {
		names := sortedKeys(declared)
		s.result.Declared = &names
	}
	printDescriptions(s.out, described)
	if failed > 0 {
		s.result.Error = fmt.Sprintf("%d test(s) could not be generated, or the validator broke on them", failed)
		fmt.Fprintln(s.errs, "eo-judge:", s.result.Error)
		return 3
	}
	return 0
}

func describeOne(ctx context.Context, validator *Built, made *Prepared) (testDescription, error) {
	one := testDescription{Group: made.Group, Test: made.Test.Index, Stats: map[string]int64{}}
	info, err := os.Stat(made.Input)
	if err != nil {
		return one, err
	}
	one.Bytes = info.Size()
	if validator == nil {
		return one, nil
	}
	one.Features = []string{}
	status, err := validating(ctx, validator, made.Input, "--group", fmt.Sprint(made.Group), "--eo-describe")
	if err != nil {
		return one, err
	}
	if status.ExitCode != 0 && validatorBroke(status) {
		one.Broken = validatorSaid(status)
		return one, nil
	}
	if status.ExitCode != 0 {
		one.Invalid = validatorSaid(status)
		return one, nil
	}
	for _, line := range strings.Split(string(status.Stdout), "\n") {
		if parts := describedFeature.FindStringSubmatch(line); parts != nil {
			one.declared = append(one.declared, parts[1])
			if parts[2] == "yes" {
				one.Features = append(one.Features, parts[1])
			}
		}
		if parts := describedStat.FindStringSubmatch(line); parts != nil {
			value, err := strconv.ParseInt(parts[2], 10, 64)
			if err != nil {
				continue
			}
			if _, known := one.Stats[parts[1]]; !known {
				one.order = append(one.order, parts[1])
			}
			one.Stats[parts[1]] = value
		}
	}
	return one, nil
}

func sizeOf(bytes int64) string {
	switch {
	case bytes < 1<<10:
		return fmt.Sprintf("%d B", bytes)
	case bytes < 1<<20:
		return fmt.Sprintf("%.1f KB", float64(bytes)/(1<<10))
	}
	return fmt.Sprintf("%.1f MB", float64(bytes)/(1<<20))
}

func printDescriptions(out io.Writer, described []testDescription) {
	labels, sizes, features := 0, 0, 0
	declared := false
	for _, one := range described {
		labels = max(labels, len(fmt.Sprintf("%d:%d", one.Group, one.Test)))
		sizes = max(sizes, len(sizeCell(one)))
		features = max(features, len(featureCell(one)))
		declared = declared || len(one.declared) > 0
	}
	for _, one := range described {
		row := fmt.Sprintf("%-*s  %*s", labels, fmt.Sprintf("%d:%d", one.Group, one.Test), sizes, sizeCell(one))
		var said []string
		switch {
		case one.Unmade != "":
			said = []string{"not generated: " + one.Unmade}
		case one.Broken != "":
			said = []string{"validator broke: " + one.Broken}
		case one.Invalid != "":
			said = []string{"invalid: " + one.Invalid}
		default:
			if declared {
				row += fmt.Sprintf("  %-*s", features, featureCell(one))
			}
			for _, name := range one.order {
				said = append(said, fmt.Sprintf("%s=%d", name, one.Stats[name]))
			}
		}
		if len(said) > 0 {
			row += "  " + strings.Join(said, " ")
		}
		fmt.Fprintln(out, strings.TrimRight(row, " "))
	}
}

func sizeCell(one testDescription) string {
	if one.Unmade != "" {
		return "-"
	}
	return sizeOf(one.Bytes)
}

func featureCell(one testDescription) string {
	if len(one.Features) == 0 {
		return "-"
	}
	return strings.Join(one.Features, ",")
}
