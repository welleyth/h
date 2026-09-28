package main

import (
	"os"
	"regexp"
	"strings"
)

type rule struct {
	roles   []string
	pattern *regexp.Regexp
	code    string
	says    string
	fix     string
}

var rules = []rule{
	{[]string{"script"}, regexp.MustCompile(`\brand\s*\(`), "EO501",
		"rand() is seeded by the C library, not by the generator",
		"draw from g.rng(), which the judge and a local run agree on"},
	{[]string{"script"}, regexp.MustCompile(`\btime\s*\(\s*(0|NULL|nullptr)\s*\)`), "EO501",
		"the clock decides what this generator writes",
		"a test must be the same bytes on every run"},
	{[]string{"script"}, regexp.MustCompile(`std::random_device|random_device`), "EO501",
		"std::random_device is not reproducible", "draw from g.rng()"},
	{[]string{"script"}, regexp.MustCompile(`std::shuffle|(^|[^.\w])shuffle\s*\(`), "EO502",
		"std::shuffle depends on the standard library, not only on the seed",
		"use the library's own shuffle so GCC and clang agree"},
	{[]string{"script"}, regexp.MustCompile(`unordered_map|unordered_set`), "EO502",
		"iterating an unordered container gives a different order on another build",
		"sort before writing, or use an ordered container"},
	{[]string{"interactor", "controller"}, regexp.MustCompile(`std::cout|\bprintf\s*\(|\bputs\s*\(`), "EO401",
		"printing with cout or printf writes into the solution's input",
		"send with it.send, and log with eo::log"},
}

func Lint(problem *Problem) Findings {
	var found Findings
	for name, program := range namedPrograms(problem) {
		if program.Source == "" {
			continue
		}
		body, err := os.ReadFile(problem.Path(program.Source))
		if err != nil {
			continue
		}
		text := stripped(string(body))
		role, _, _ := strings.Cut(name, ".")
		for _, one := range rules {
			if !plays(role, one.roles) || !one.pattern.MatchString(text) {
				continue
			}
			found.warn(one.code, label(name), one.says, one.fix)
		}
	}
	return found
}

func plays(role string, roles []string) bool {
	for _, one := range roles {
		if one == role {
			return true
		}
	}
	return false
}

var comments = regexp.MustCompile(`(?s)/\*.*?\*/`)
var lineComments = regexp.MustCompile(`//[^\n]*`)
var strings_ = regexp.MustCompile(`"(\\.|[^"\\])*"`)

func stripped(text string) string {
	text = comments.ReplaceAllString(text, " ")
	text = lineComments.ReplaceAllString(text, " ")
	return strings_.ReplaceAllString(text, ` "" `)
}
