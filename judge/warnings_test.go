package main

import "testing"

func TestWarningsFromAJudgeLog(t *testing.T) {
	t.Parallel()
	log := `points 6.5585 1009 of 2000 degrees
eolymp.h 1.0.0
note EO106 source.cpp:10 the bounds 1..1999 are one away from a round number (2000 times)
warning EO203 ./eolymp.h:2717 the answer file still holds "40"
eo-report {"version":1,"warnings":[{"code":"EO106","at":"source.cpp:10","count":2000},{"code":"EO203","at":"./eolymp.h:2717","count":1}]}
`
	found := warningsIn("checker", log)
	if len(found) != 2 {
		t.Fatalf("got %d warnings, want 2", len(found))
	}
	if !hasWarning(found, "EO106") || !hasWarning(found, "EO203") {
		t.Fatalf("missing a code: %v", found)
	}
	for _, one := range found {
		if one.Code == "EO106" {
			if one.Count != 2000 {
				t.Errorf("EO106 counted %d, want 2000", one.Count)
			}
			if one.Severity != "note" {
				t.Errorf("EO106 is a note, got %q", one.Severity)
			}
			if one.Message == "" {
				t.Error("EO106 lost its message")
			}
		}
	}
}

func TestALogWithNoWarningsGivesNone(t *testing.T) {
	t.Parallel()
	if found := warningsIn("checker", "ok all 12 degrees\neolymp.h 1.0.0\n"); len(found) != 0 {
		t.Fatalf("got %v", found)
	}
}

func TestTheSpokenLinesAloneStillParse(t *testing.T) {
	t.Parallel()
	found := warningsIn("validator", "validator.cpp:4: note EO106: the bounds are odd\n")
	if len(found) != 0 {
		t.Fatalf("the local format is not the judge format, got %v", found)
	}
	found = warningsIn("validator", "note EO106 validator.cpp:4 the bounds are odd\n")
	if len(found) != 1 || found[0].At != "validator.cpp:4" {
		t.Fatalf("got %v", found)
	}
}

func TestAReportGivesBackAWindowsPathWithSpacesAndQuotes(t *testing.T) {
	t.Parallel()
	log := `wrong answer the sum is 7
eolymp.h 2.5.0
warning EO101 C:\Users\author\my "best" checker.cpp:7 read_int(1, n) has no name
eo-report {"version":1,"warnings":[{"code":"EO101","at":"C:\\Users\\author\\my \"best\" checker.cpp:7","count":1}]}
`
	found := warningsIn("checker", log)
	if len(found) != 1 {
		t.Fatalf("got %d warnings, want 1: %v", len(found), found)
	}
	if want := `C:\Users\author\my "best" checker.cpp:7`; found[0].At != want {
		t.Errorf("at %q, want %q", found[0].At, want)
	}
	if found[0].Code != "EO101" || found[0].Count != 1 {
		t.Errorf("got %v", found[0])
	}
}
