package main

import (
	"os"
	"path/filepath"
	"testing"
)

func problemWith(t *testing.T, role, body string) *Problem {
	t.Helper()
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "one.cpp"), []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	problem := &Problem{dir: dir}
	switch role {
	case "script":
		problem.Scripts = map[string]*Program{"gen": {Source: "one.cpp"}}
	case "interactor":
		problem.Interactor = &Program{Source: "one.cpp"}
	default:
		problem.Checker = &Program{Source: "one.cpp"}
	}
	return problem
}

func TestLintFindsAClockInAGenerator(t *testing.T) {
	t.Parallel()
	found := Lint(problemWith(t, "script", "int main() { srand(time(0)); return rand(); }"))
	if !fired(found, "EO501") {
		t.Fatalf("EO501 did not fire: %v", found)
	}
}

func TestLintFindsPrintingInAnInteractor(t *testing.T) {
	t.Parallel()
	found := Lint(problemWith(t, "interactor", "#include <cstdio>\nint main() { printf(\"hi\"); }"))
	if !fired(found, "EO401") {
		t.Fatalf("EO401 did not fire: %v", found)
	}
}

func TestLintIgnoresAStringAndAComment(t *testing.T) {
	t.Parallel()
	found := Lint(problemWith(t, "script", "int main() { /* rand() */ const char* s = \"rand()\"; return 0; }"))
	if fired(found, "EO501") {
		t.Fatalf("EO501 fired on a comment and a string: %v", found)
	}
}

func TestLintLeavesOtherRolesAlone(t *testing.T) {
	t.Parallel()
	found := Lint(problemWith(t, "checker", "#include <cstdio>\nint main() { printf(\"ok\"); }"))
	if fired(found, "EO401") {
		t.Fatal("a checker is allowed to print")
	}
}
