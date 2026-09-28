package main

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func exitOf(t *testing.T, built *Built) int {
	t.Helper()
	status, err := run(context.Background(), built.Exe, Invocation{Dir: built.Dir, LimitMS: 5000})
	if err != nil {
		t.Fatal(err)
	}
	return status.ExitCode
}

func writeFile(t *testing.T, path, body string) {
	t.Helper()
	if err := os.WriteFile(path, []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
}

func counting(t *testing.T) (string, func() int) {
	t.Helper()
	dir := t.TempDir()
	calls := filepath.Join(dir, "calls")
	wrapper := filepath.Join(dir, "counting-compiler")
	script := "#!/bin/sh\ncase \" $* \" in *\" -o \"*) echo built >> " + calls + ";; esac\nexec " + compiler() + " \"$@\"\n"
	if err := os.WriteFile(wrapper, []byte(script), 0o755); err != nil {
		t.Fatal(err)
	}
	return wrapper, func() int {
		body, _ := os.ReadFile(calls)
		return strings.Count(string(body), "built")
	}
}

func TestACachedBuildIsNotCompiledAgain(t *testing.T) {
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "checker.cpp"), "int main() { return 5; }\n")
	wrapper, compiled := counting(t)
	t.Parallel()
	tools := toolchain{cxx: wrapper, cache: t.TempDir()}
	program := &Program{Source: "checker.cpp"}
	for range 3 {
		built, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", program, t.TempDir())
		if err != nil {
			t.Fatal(err)
		}
		if got := exitOf(t, built); got != 5 {
			t.Fatalf("the program exits %d", got)
		}
		if !strings.HasPrefix(built.Exe, tools.cache) || !strings.HasPrefix(built.Source, tools.cache) {
			t.Errorf("the program was built at %s from %s, outside the cache", built.Exe, built.Source)
		}
	}
	if got := compiled(); got != 1 {
		t.Errorf("three builds of one program compiled it %d times", got)
	}

	writeFile(t, filepath.Join(dir, "checker.cpp"), "int main() { return 6; }\n")
	built, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", program, t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	if got := exitOf(t, built); got != 6 {
		t.Errorf("a changed source was served from the cache: exit %d", got)
	}
}

func TestWithoutACacheAProgramIsBuiltInTheWorkspace(t *testing.T) {
	needsACompiler(t)
	dir, work := t.TempDir(), t.TempDir()
	writeFile(t, filepath.Join(dir, "checker.cpp"), "int main() { return 5; }\n")
	wrapper, compiled := counting(t)
	t.Parallel()
	tools := toolchain{cxx: wrapper}
	for range 2 {
		built, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", &Program{Source: "checker.cpp"}, work)
		if err != nil {
			t.Fatal(err)
		}
		if built.Exe != filepath.Join(work, "checker", "program") || exitOf(t, built) != 5 {
			t.Errorf("built %+v", built)
		}
	}
	if got := compiled(); got != 2 {
		t.Errorf("two builds without a cache compiled %d times", got)
	}
}

func TestACompileErrorNamesTheWorkspaceNotTheCache(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir, work := t.TempDir(), t.TempDir()
	writeFile(t, filepath.Join(dir, "checker.cpp"), "this is not C++\n")
	tools := toolchain{cxx: compiler(), cache: t.TempDir()}
	_, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", &Program{Source: "checker.cpp"}, work)
	if err == nil || !strings.Contains(err.Error(), "checker does not compile") {
		t.Fatalf("said %v", err)
	}
	if strings.Contains(err.Error(), tools.cache) || !strings.Contains(err.Error(), filepath.Join(work, "checker", "source.cpp")) {
		t.Errorf("said %v", err)
	}
}

func TestTheCacheSeesAHeaderTheSystemGainsOrChanges(t *testing.T) {
	needsACompiler(t)
	dir, system := t.TempDir(), t.TempDir()
	t.Setenv("CPLUS_INCLUDE_PATH", system)
	writeFile(t, filepath.Join(dir, "attached_helper.h"), "inline int answer() { return 42; }\n")
	writeFile(t, filepath.Join(system, "other.h"), "inline int other() { return 0; }\n")
	writeFile(t, filepath.Join(dir, "checker.cpp"),
		"#include <attached_helper.h>\n#include <other.h>\nint main() { return answer() + other(); }\n")
	tools := toolchain{cxx: compiler(), cache: t.TempDir()}
	program := &Program{Source: "checker.cpp", Files: []string{"attached_helper.h"}}
	buildOnce := func() *Built {
		built, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", program, t.TempDir())
		if err != nil {
			t.Fatal(err)
		}
		return built
	}

	if got := exitOf(t, buildOnce()); got != 42 {
		t.Fatalf("the attached header was not used: exit %d", got)
	}
	if got := exitOf(t, buildOnce()); got != 42 {
		t.Fatalf("a cached build exits %d", got)
	}

	writeFile(t, filepath.Join(system, "other.h"), "inline int other() { return 1; }\n")
	later := time.Now().Add(time.Second)
	if err := os.Chtimes(filepath.Join(system, "other.h"), later, later); err != nil {
		t.Fatal(err)
	}
	if got := exitOf(t, buildOnce()); got != 43 {
		t.Errorf("a changed system header was served from the cache: exit %d", got)
	}

	writeFile(t, filepath.Join(system, "attached_helper.h"), "inline int answer() { return 7; }\n")
	if got := exitOf(t, buildOnce()); got != 8 {
		t.Errorf("a system header that now wins over the attached one was not seen: exit %d", got)
	}
}

func TestTheCacheIsWhereTheEnvironmentSays(t *testing.T) {
	t.Setenv("EO_JUDGE_CACHE", "off")
	if got := cacheRoot(); got != "" {
		t.Errorf("EO_JUDGE_CACHE=off keeps a cache in %q", got)
	}
	t.Setenv("EO_JUDGE_CACHE", "relative")
	if got := cacheRoot(); !filepath.IsAbs(got) || filepath.Base(got) != "relative" {
		t.Errorf("EO_JUDGE_CACHE=relative keeps the cache in %q", got)
	}
	t.Setenv("EO_JUDGE_CACHE", "")
	t.Setenv("XDG_CACHE_HOME", "/somewhere")
	t.Setenv("HOME", "/home/someone")
	if got, base := cacheRoot(), filepath.Base(cacheRoot()); got == "" || base != "eo-judge" {
		t.Errorf("with no EO_JUDGE_CACHE the cache is %q", got)
	}
}

func TestAWarningInACachedProgramNamesTheSource(t *testing.T) {
	shop := &Workspace{Dir: "/work", Problem: &Problem{Checker: &Program{Source: "checker.cpp"}},
		Programs: map[string]*Built{"checker": {Name: "checker", Dir: "/work/checker",
			Source: "/cache/ab/abcd/source.cpp"}}}
	for at, want := range map[string]string{
		"/cache/ab/abcd/source.cpp:10": "checker.cpp:10",
		"/cache/ab/abcd/eolymp.h:3449": "checker/eolymp.h:3449",
		"/work/tests/01-001.in":        "tests/01-001.in",
	} {
		if got := shop.named(Warning{At: at}); got != want {
			t.Errorf("%s is named %q, not %q", at, got, want)
		}
	}
}

func TestACacheThatCannotBeWrittenIsNoCache(t *testing.T) {
	needsACompiler(t)
	file := filepath.Join(t.TempDir(), "a-file")
	writeFile(t, file, "not a directory\n")
	problem := t.TempDir()
	writeFile(t, filepath.Join(problem, "checker.cpp"), "int main() { return 0; }\n")
	writeFile(t, filepath.Join(problem, "01.in"), "1\n")
	writeFile(t, filepath.Join(problem, "problem.json"), `{"type": "PROGRAM", "checker": {"source": "checker.cpp"},
		"solutions": [{"name": "main", "source": "checker.cpp"}],
		"testsets": [{"index": 1, "tests": [{"index": 1, "score": 100, "input": "01.in", "answer": "01.in"}]}]}`)
	for _, place := range []string{file, filepath.Join(file, "below")} {
		t.Setenv("EO_JUDGE_CACHE", place)
		if got := hostToolchain().cache; got != "" {
			t.Errorf("EO_JUDGE_CACHE=%s keeps a cache in %q", place, got)
		}
		code, out, errs := invoke("run", problem)
		if code != 0 || !strings.Contains(out, "main: ACCEPTED, 100\n") {
			t.Errorf("with EO_JUDGE_CACHE=%s: exit %d, printed %q, said %q", place, code, out, errs)
		}
	}
}

func TestACompilerThatCannotStartSaysWhy(t *testing.T) {
	t.Parallel()
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "a.cpp"), "int main() {}\n")
	missing := filepath.Join(dir, "no-such-compiler")
	_, err := toolchain{cxx: missing}.build(context.Background(), &Problem{dir: dir}, "checker",
		&Program{Source: "a.cpp"}, t.TempDir())
	if err == nil || !strings.Contains(err.Error(), "checker does not compile:\n") ||
		!strings.Contains(err.Error(), missing) {
		t.Errorf("said %v", err)
	}
}
