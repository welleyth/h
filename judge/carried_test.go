package main

import (
	"context"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"
)

func TestTheCarriedHeadersAreTheRepositorys(t *testing.T) {
	t.Parallel()
	for _, name := range carriedHeaders {
		ours, err := os.ReadFile(filepath.Join("..", name))
		if err != nil {
			t.Fatal(err)
		}
		inside, err := carried.ReadFile("include/" + name)
		if err != nil {
			t.Fatal(err)
		}
		if string(ours) != string(inside) {
			t.Errorf("eo-judge carries another %s than the repository's; run make", name)
		}
	}
}

func TestAProgramFindsTheCarriedHeadersWithNothingAttached(t *testing.T) {
	t.Parallel()
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "angled.cpp"), "#include <eolymp.h>\n#include <eolymp-shapes.h>\n"+
		"int main() { return EOLYMP_H_VERSION_MAJOR; }\n")
	writeFile(t, filepath.Join(dir, "quoted.cpp"), "#include \"eolymp.h\"\n"+
		"int main() { return EOLYMP_H_VERSION_MAJOR; }\n")
	writeFile(t, filepath.Join(dir, "eolymp.h"), "#define EOLYMP_H_VERSION_MAJOR 99\n")
	for _, one := range []struct {
		name   string
		cache  string
		source string
		files  []string
		exit   int
	}{
		{"angled", t.TempDir(), "angled.cpp", nil, versionMajor(t)},
		{"quoted", t.TempDir(), "quoted.cpp", nil, versionMajor(t)},
		{"uncached", "", "angled.cpp", nil, versionMajor(t)},
		{"attached", t.TempDir(), "quoted.cpp", []string{"eolymp.h"}, 99},
	} {
		t.Run(one.name, func(t *testing.T) {
			t.Parallel()
			problem := &Problem{dir: filepath.Join(dir, "nowhere")}
			program := &Program{Source: filepath.Join(dir, one.source)}
			for _, file := range one.files {
				program.Files = append(program.Files, filepath.Join(dir, file))
			}
			built, err := toolchain{cxx: compiler(), cache: one.cache}.build(context.Background(), problem, "checker",
				program, t.TempDir())
			if err != nil {
				t.Fatal(err)
			}
			if got := exitOf(t, built); got != one.exit {
				t.Errorf("%s with %v attached exits %d, not %d", one.source, one.files, got, one.exit)
			}
		})
	}
}

func versionMajor(t *testing.T) int {
	t.Helper()
	major, err := strconv.Atoi(strings.Split(version, ".")[0])
	if err != nil {
		t.Fatal(err)
	}
	return major
}

func TestAnInstalledHeaderWinsOverTheCarriedOneAndIsNoticed(t *testing.T) {
	needsACompiler(t)
	dir, system := t.TempDir(), t.TempDir()
	t.Setenv("CPLUS_INCLUDE_PATH", system)
	writeFile(t, filepath.Join(dir, "checker.cpp"), "#include <eolymp.h>\nint main() { return EOLYMP_H_VERSION_MAJOR; }\n")
	tools := toolchain{cxx: compiler(), cache: t.TempDir()}
	buildOnce := func() *Built {
		built, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", &Program{Source: "checker.cpp"},
			t.TempDir())
		if err != nil {
			t.Fatal(err)
		}
		return built
	}
	if got := exitOf(t, buildOnce()); got != versionMajor(t) {
		t.Fatalf("the carried header was not used: exit %d", got)
	}
	writeFile(t, filepath.Join(system, "eolymp.h"), "#define EOLYMP_H_VERSION_MAJOR 77\n")
	if got := exitOf(t, buildOnce()); got != 77 {
		t.Errorf("an eolymp.h installed after the build was not seen: exit %d", got)
	}
}

func TestAWarningFromTheCarriedHeaderNamesTheHeader(t *testing.T) {
	t.Parallel()
	shop := &Workspace{Dir: "/work", Problem: &Problem{}, Programs: map[string]*Built{},
		tools: toolchain{cache: "/cache"}}
	at := filepath.Join(shop.tools.carriedDirs(shop.Dir)[0], "eolymp.h") + ":3449"
	if got := shop.named(Warning{At: at}); got != "eolymp.h:3449" {
		t.Errorf("%s is named %q", at, got)
	}
	shop.tools.cache = ""
	if got := shop.named(Warning{At: "/work/include/eolymp.h:12"}); got != "eolymp.h:12" {
		t.Errorf("without a cache it is named %q", got)
	}
}

func TestAQuotedCarriedHeaderNeedsNoFilesEntry(t *testing.T) {
	t.Parallel()
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "checker.cpp"), "#include \"eolymp.h\"\n#include \"eolymp-shapes.h\"\n#include \"mine.h\"\n")
	found := Configuration(&Problem{dir: dir, Checker: &Program{Source: "checker.cpp"}})
	for _, one := range found {
		if one.Code == "EO903" && !strings.Contains(one.Message, "mine.h") {
			t.Errorf("%s", one)
		}
	}
	if !fired(found, "EO903") {
		t.Error("an unattached mine.h did not raise EO903")
	}
}

func TestTheCarriedHeadersAreWrittenOnceAndWhereTheyCanBe(t *testing.T) {
	t.Parallel()
	work, cache := t.TempDir(), t.TempDir()
	tools := toolchain{cache: cache}
	dir, err := tools.writeCarried(work)
	if err != nil || dir != tools.carriedDirs(work)[0] {
		t.Fatalf("wrote to %q: %v", dir, err)
	}
	path := filepath.Join(dir, "eolymp.h")
	early := time.Now().Add(-time.Hour)
	if err := os.Chtimes(path, early, early); err != nil {
		t.Fatal(err)
	}
	if _, err := tools.writeCarried(work); err != nil {
		t.Fatal(err)
	}
	if info, err := os.Stat(path); err != nil || !info.ModTime().Equal(early) {
		t.Errorf("an unchanged carried header was written again: %v", err)
	}
	writeFile(t, path, "damaged\n")
	if _, err := tools.writeCarried(work); err != nil {
		t.Fatal(err)
	}
	if body, _ := os.ReadFile(path); string(body) == "damaged\n" {
		t.Error("a damaged carried header was kept")
	}
	blocked := toolchain{cache: filepath.Join(path, "below")}
	if dir, err := blocked.writeCarried(work); err != nil || dir != filepath.Join(work, "include") {
		t.Errorf("with a cache that cannot hold them the headers went to %q: %v", dir, err)
	}
}

func TestAPrunedCarriedDirectoryRebuildsNothing(t *testing.T) {
	needsACompiler(t)
	dir := t.TempDir()
	writeFile(t, filepath.Join(dir, "checker.cpp"), "#include <eolymp.h>\nint main() { return EOLYMP_H_VERSION_MAJOR; }\n")
	wrapper, compiled := counting(t)
	t.Parallel()
	tools := toolchain{cxx: wrapper, cache: t.TempDir()}
	for range 2 {
		if _, err := tools.build(context.Background(), &Problem{dir: dir}, "checker", &Program{Source: "checker.cpp"},
			t.TempDir()); err != nil {
			t.Fatal(err)
		}
		if err := os.RemoveAll(filepath.Join(tools.cache, "include")); err != nil {
			t.Fatal(err)
		}
	}
	if got := compiled(); got != 1 {
		t.Errorf("the carried headers written again made %d builds of one program", got)
	}
}
