package main

import (
	"context"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"
)

func TestAnAttachedHeaderIsFoundWithAngleBrackets(t *testing.T) {
	t.Parallel()
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
	built, err := hostToolchain().build(context.Background(), problem, "checker", &Program{Source: "checker.cpp", Files: []string{"attached_helper.h"}},
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
	built, err := hostToolchain().build(context.Background(), problem, "checker", &Program{Source: "checker.cpp", Files: []string{"attached_helper.h"}},
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
	slow := filepath.Join(dir, "slow-compiler")
	child, pid := lingering(dir)
	if err := os.WriteFile(slow, []byte("#!/bin/sh\n"+child+"sleep 30\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Parallel()
	if err := os.WriteFile(filepath.Join(dir, "a.cpp"), []byte("int main() {}\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()
	started := time.Now()
	_, err := toolchain{cxx: slow}.build(ctx, &Problem{dir: dir}, "slow", &Program{Source: "a.cpp"}, t.TempDir())
	if err == nil || !strings.Contains(err.Error(), "the build of slow was interrupted") {
		t.Fatalf("said %v", err)
	}
	if spent := time.Since(started); spent > 5*time.Second {
		t.Errorf("the build took %v to stop", spent)
	}
	awaitGone(t, pid, "a process the compiler started outlived the build")
}

func TestATimeLimitStopsEveryProcessTheProgramStarted(t *testing.T) {
	t.Parallel()
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
	t.Parallel()
	child, pid := lingering(t.TempDir())
	started := time.Now()
	status, err := run(context.Background(), "/bin/sh", Invocation{
		Args: []string{"-c", child + "echo started"}, LimitMS: 10000})
	if err != nil {
		t.Fatal(err)
	}
	if status.TimedOut || status.ExitCode != 0 || string(status.Stdout) != "started\n" {
		t.Errorf("status %+v", status)
	}
	if waited := time.Since(started); waited > 900*time.Millisecond {
		t.Errorf("the run took %v; it waited for the child", waited)
	}
	awaitGone(t, pid, "the child outlived the run")
}

func TestTheSameProgramIsBuiltOnceUnderEveryName(t *testing.T) {
	t.Parallel()
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

func TestAHangingCompilerDoesNotHangTheVersionCheck(t *testing.T) {
	dir := t.TempDir()
	hanging := filepath.Join(dir, "hanging-compiler")
	child, pid := lingering(dir)
	if err := os.WriteFile(hanging, []byte("#!/bin/sh\n"+child+"sleep 30\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Parallel()
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()
	started := time.Now()
	if said := versionOf(ctx, hanging); said != "" {
		t.Errorf("a compiler that never answered has the version %q", said)
	}
	if spent := time.Since(started); spent > 5*time.Second {
		t.Errorf("the version check took %v to stop", spent)
	}
	awaitGone(t, pid, "a process the compiler started outlived the version check")
}

func lingering(dir string) (string, string) {
	pid := filepath.Join(dir, "lingering.pid")
	return "sh -c 'echo $$ > " + pid + ".part && mv " + pid + ".part " + pid + " && exec sleep 30' &\n" +
		"until [ -f " + pid + " ]; do sleep 0.01; done\n", pid
}

func awaitGone(t *testing.T, pidFile, complaint string) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for ; time.Now().Before(deadline); time.Sleep(20 * time.Millisecond) {
		body, err := os.ReadFile(pidFile)
		if err != nil {
			continue
		}
		pid, err := strconv.Atoi(strings.TrimSpace(string(body)))
		if err != nil {
			t.Fatal(err)
		}
		if syscall.Kill(pid, 0) != nil {
			return
		}
	}
	if _, err := os.Stat(pidFile); err == nil {
		t.Error(complaint)
	}
}

func TestACompilersVersionIsAskedOnce(t *testing.T) {
	dir := t.TempDir()
	calls := filepath.Join(dir, "calls")
	counted := filepath.Join(dir, "counted-compiler")
	script := "#!/bin/sh\necho called >> " + calls + "\necho 'counted 1.0'\n"
	if err := os.WriteFile(counted, []byte(script), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Parallel()
	for range 3 {
		if said := versionOf(context.Background(), counted); said != "counted 1.0" {
			t.Fatalf("the version is %q", said)
		}
	}
	body, err := os.ReadFile(calls)
	if err != nil {
		t.Fatal(err)
	}
	if asked := strings.Count(string(body), "called"); asked != 1 {
		t.Errorf("the compiler was asked for its version %d times", asked)
	}
}
