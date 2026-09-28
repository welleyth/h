package main

import (
	"encoding/json"
	"fmt"
	"regexp"
	"strings"
)

type Warning struct {
	Source   string `json:"source"`
	Code     string `json:"code"`
	Severity string `json:"severity"`
	At       string `json:"at"`
	Count    int    `json:"count"`
	Message  string `json:"message"`
}

func (w Warning) String() string {
	repeat := ""
	if w.Count > 1 {
		repeat = fmt.Sprintf(" (%d times)", w.Count)
	}
	where := w.At
	if where == "" {
		where = w.Source
	}
	return fmt.Sprintf("%s %s %s: %s%s", w.Severity, w.Code, where, w.Message, repeat)
}

var spoken = regexp.MustCompile(`^(warning|note) (EO[0-9]{3}) ([^ ]+) (.*?)(?: \(([0-9]+) times\))?$`)

type reported struct {
	Version  int `json:"version"`
	Warnings []struct {
		Code  string `json:"code"`
		At    string `json:"at"`
		Count int    `json:"count"`
	} `json:"warnings"`
}

func warningsIn(source string, text string) []Warning {
	said := map[string]string{}
	var found []Warning

	for _, line := range strings.Split(text, "\n") {
		if parts := spoken.FindStringSubmatch(line); parts != nil {
			count := 1
			if parts[5] != "" {
				fmt.Sscanf(parts[5], "%d", &count)
			}
			key := parts[2] + " " + parts[3]
			said[key] = parts[4]
			found = append(found, Warning{Source: source, Code: parts[2], Severity: parts[1],
				At: parts[3], Count: count, Message: parts[4]})
			continue
		}

		rest, marked := strings.CutPrefix(line, "eo-report ")
		if !marked {
			continue
		}
		var structured reported
		if err := json.Unmarshal([]byte(rest), &structured); err != nil {
			continue
		}
		found = found[:0]
		for _, one := range structured.Warnings {
			severity := "warning"
			if strings.HasPrefix(one.Code, "EO") {
				severity = severityOf(one.Code, text)
			}
			found = append(found, Warning{Source: source, Code: one.Code, Severity: severity,
				At: one.At, Count: one.Count, Message: said[one.Code+" "+one.At]})
		}
		return found
	}

	return found
}

func severityOf(code, text string) string {
	if strings.Contains(text, "note "+code+" ") {
		return "note"
	}
	return "warning"
}

func hasWarning(list []Warning, code string) bool {
	for _, one := range list {
		if one.Code == code {
			return true
		}
	}
	return false
}
