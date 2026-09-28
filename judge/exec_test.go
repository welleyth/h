package main

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestAnAttachedHeaderIsFoundWithAngleBrackets(t *testing.T) {
	needsACompiler(t)
	dir := t.TempDir()
	write := func(name, body string) {
		if err := os.WriteFile(filepath.Join(dir, name), []byte(body), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write("attached_helper.h", "inline int answer() { return 42; }\n")
	write("checker.cpp", "#include <attached_helper.h>\nint main() { return answer() == 42 ? 0 : 1; }\n")
	problem := &Problem{dir: dir}
	built, err := build(context.Background(), problem, "checker", &Program{Source: "checker.cpp", Files: []string{"attached_helper.h"}},
		t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(built.Exe); err != nil {
		t.Error(err)
	}
}

func TestTheSystemCopyOfAHeaderWinsOverAnAttachedOne(t *testing.T) {
	needsACompiler(t)
	dir := t.TempDir()
	system := t.TempDir()
	write := func(name, body string) {
		if err := os.WriteFile(name, []byte(body), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	write(filepath.Join(system, "attached_helper.h"), "inline int answer() { return 7; }\n")
	write(filepath.Join(dir, "attached_helper.h"), "inline int answer() { return 42; }\n")
	write(filepath.Join(dir, "checker.cpp"), "#include <attached_helper.h>\nint main() { return answer(); }\n")
	t.Setenv("CPLUS_INCLUDE_PATH", system)
	problem := &Problem{dir: dir}
	built, err := build(context.Background(), problem, "checker", &Program{Source: "checker.cpp", Files: []string{"attached_helper.h"}},
		t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	status, err := run(context.Background(), built.Exe, Invocation{Dir: built.Dir, LimitMS: 5000})
	if err != nil {
		t.Fatal(err)
	}
	if status.ExitCode != 7 {
		t.Errorf("the program used the attached header, exiting %d", status.ExitCode)
	}
}

func TestAnInterruptStopsABuildAndWhatItStarted(t *testing.T) {
	dir := t.TempDir()
	marker := filepath.Join(dir, "still-running")
	slow := filepath.Join(dir, "slow-compiler")
	script := "#!/bin/sh\n(sleep 2; touch " + marker + ") &\nsleep 30\n"
	if err := os.WriteFile(slow, []byte(script), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "a.cpp"), []byte("int main() {}\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()
	started := time.Now()
	_, err := buildWith(ctx, slow, &Problem{dir: dir}, "slow", &Program{Source: "a.cpp"}, t.TempDir())
	if err == nil || !strings.Contains(err.Error(), "the build of slow was interrupted") {
		t.Fatalf("said %v", err)
	}
	if spent := time.Since(started); spent > 5*time.Second {
		t.Errorf("the build took %v to stop", spent)
	}
	time.Sleep(2500 * time.Millisecond)
	if _, err := os.Stat(marker); err == nil {
		t.Error("a process the compiler started outlived the build")
	}
}

func TestATimeLimitStopsEveryProcessTheProgramStarted(t *testing.T) {
	started := time.Now()
	status, err := run(context.Background(), "/bin/sh", Invocation{Args: []string{"-c", "sleep 30 & sleep 30"},
		LimitMS: 300})
	if err != nil {
		t.Fatal(err)
	}
	if !status.TimedOut {
		t.Error("the run did not time out")
	}
	if waited := time.Since(started); waited > 5*time.Second {
		t.Errorf("the run took %v; a child kept it alive", waited)
	}
}

func TestAChildLeftBehindIsStoppedWhenTheProgramEnds(t *testing.T) {
	marker := filepath.Join(t.TempDir(), "still-here")
	started := time.Now()
	status, err := run(context.Background(), "/bin/sh", Invocation{
		Args: []string{"-c", "(sleep 1; touch " + marker + ") & echo started"}, LimitMS: 10000})
	if err != nil {
		t.Fatal(err)
	}
	if status.TimedOut || status.ExitCode != 0 || string(status.Stdout) != "started\n" {
		t.Errorf("status %+v", status)
	}
	if waited := time.Since(started); waited > 900*time.Millisecond {
		t.Errorf("the run took %v; it waited for the child", waited)
	}
	time.Sleep(1500 * time.Millisecond)
	if _, err := os.Stat(marker); err == nil {
		t.Error("the child outlived the run")
	}
}

func TestTheSameProgramIsBuiltOnceUnderEveryName(t *testing.T) {
	needsACompiler(t)
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "gen.cpp"), []byte("int main() {}\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	same := &Program{Source: "gen.cpp", Runtime: "cpp:17"}
	problem := &Problem{dir: dir, Scripts: map[string]*Program{"a": same, "b": {Source: "gen.cpp"}},
		Solutions: []*Solution{{Name: "full", Source: "gen.cpp"}}}
	shop := NewWorkspace(problem, t.TempDir())
	if err := shop.BuildAll(context.Background(), problem.Solutions); err != nil {
		t.Fatal(err)
	}
	a, b, full := shop.Programs["script.a"], shop.Programs["script.b"], shop.Programs["solution.full"]
	if a == nil || b == nil || full == nil {
		t.Fatalf("built %v", shop.Programs)
	}
	if a.Exe != b.Exe || a.Exe != full.Exe {
		t.Errorf("one program was built more than once: %s, %s, %s", a.Exe, b.Exe, full.Exe)
	}
	if a.Name != "script.a" || b.Name != "script.b" || full.Name != "solution.full" {
		t.Errorf("names %s, %s, %s", a.Name, b.Name, full.Name)
	}
	problem.Checker = &Program{Source: "missing.cpp"}
	if err := NewWorkspace(problem, t.TempDir()).BuildAll(context.Background(), nil); err == nil {
		t.Error("a program that cannot be built was not reported")
	}
}
