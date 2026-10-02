package main

import (
	"fmt"
	"io"
	"sort"
	"strings"
)

type described struct {
	group, test int
	features    map[string]bool
}

type testFeatures struct {
	Group    int      `json:"group"`
	Test     int      `json:"test"`
	Features []string `json:"features"`
}

type coverageReport struct {
	Features []string       `json:"features"`
	Tests    []testFeatures `json:"tests"`
}

func (w *Workspace) coverage() *coverageReport {
	declared := map[string]bool{}
	for _, one := range w.described {
		for name := range one.features {
			declared[name] = true
		}
	}
	if len(declared) == 0 {
		return nil
	}
	report := &coverageReport{Features: sortedKeys(declared)}
	for _, one := range w.described {
		seen := []string{}
		for _, name := range report.Features {
			if one.features[name] {
				seen = append(seen, name)
			}
		}
		report.Tests = append(report.Tests, testFeatures{Group: one.group, Test: one.test, Features: seen})
	}
	return report
}

func sortedKeys(set map[string]bool) []string {
	keys := make([]string, 0, len(set))
	for key := range set {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	return keys
}

func printCoverage(out io.Writer, report *coverageReport) {
	for at := 0; at < len(report.Tests); {
		group := report.Tests[at].Group
		end := at
		for end < len(report.Tests) && report.Tests[end].Group == group {
			end++
		}
		heading := fmt.Sprintf("features of testset %d", group)
		width := len(heading)
		for _, one := range report.Tests[at:end] {
			width = max(width, len(fmt.Sprintf("  %d:%d", one.Group, one.Test)))
		}
		fmt.Fprintf(out, "%-*s  %s\n", width, heading, strings.Join(report.Features, "  "))
		for _, one := range report.Tests[at:end] {
			seen := map[string]bool{}
			for _, name := range one.Features {
				seen[name] = true
			}
			cells := make([]string, len(report.Features))
			for column, name := range report.Features {
				mark := "."
				if seen[name] {
					mark = "x"
				}
				cells[column] = fmt.Sprintf("%-*s", len(name), mark)
			}
			row := fmt.Sprintf("%-*s  %s", width, fmt.Sprintf("  %d:%d", one.Group, one.Test), strings.Join(cells, "  "))
			fmt.Fprintln(out, strings.TrimRight(row, " "))
		}
		fmt.Fprintln(out)
		at = end
	}
}
