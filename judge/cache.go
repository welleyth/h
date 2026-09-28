package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"hash"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"strings"
	"sync"
	"syscall"
	"time"
)

const cacheLayout = "eo-judge build cache 1"

var compilerEnvironment = []string{"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "GCC_EXEC_PREFIX", "COMPILER_PATH"}

func cacheRoot() string {
	named := os.Getenv("EO_JUDGE_CACHE")
	if named == "off" {
		return ""
	}
	if named == "" {
		base, err := os.UserCacheDir()
		if err != nil {
			return ""
		}
		named = filepath.Join(base, "eo-judge")
	}
	absolute, err := filepath.Abs(named)
	if err != nil {
		return ""
	}
	return absolute
}

func writable(dir string) string {
	if dir == "" || os.MkdirAll(dir, 0o755) != nil {
		return ""
	}
	probe, err := os.CreateTemp(dir, ".writable-*")
	if err != nil {
		return ""
	}
	probe.Close()
	os.Remove(probe.Name())
	return dir
}

func cacheKey(cxx string, args []string, dir string, files []string) (string, error) {
	path, err := exec.LookPath(cxx)
	if err != nil {
		return "", err
	}
	resolved, err := filepath.EvalSymlinks(path)
	if err != nil {
		return "", err
	}
	if resolved, err = filepath.Abs(resolved); err != nil {
		return "", err
	}
	info, err := os.Stat(resolved)
	if err != nil {
		return "", err
	}
	sum := sha256.New()
	field(sum, cacheLayout, resolved, fmt.Sprint(info.Size()), fmt.Sprint(info.ModTime().UnixNano()))
	field(sum, args...)
	for _, name := range compilerEnvironment {
		value, set := os.LookupEnv(name)
		field(sum, name, fmt.Sprint(set), value)
	}
	for _, name := range append([]string{"source.cpp"}, files...) {
		body, err := os.ReadFile(filepath.Join(dir, name))
		if err != nil {
			return "", err
		}
		field(sum, name, string(body))
	}
	return hex.EncodeToString(sum.Sum(nil))[:32], nil
}

func field(sum hash.Hash, parts ...string) {
	for _, one := range parts {
		fmt.Fprintf(sum, "%d:", len(one))
		sum.Write([]byte(one))
	}
}

type manifest struct {
	Read   []stamp  `json:"read"`
	Absent []string `json:"absent"`
}

type stamp struct {
	Path  string `json:"path"`
	Size  int64  `json:"size"`
	MTime int64  `json:"mtime"`
}

func (tools toolchain) writeManifest(ctx context.Context, entry, deps, carried string, shadowed []string) error {
	rule, err := os.ReadFile(deps)
	if err != nil {
		return err
	}
	var read []stamp
	for _, path := range dependencies(string(rule)) {
		path, err := filepath.Abs(path)
		if err != nil {
			return err
		}
		if strings.HasPrefix(path, entry+string(os.PathSeparator)) ||
			strings.HasPrefix(path, carried+string(os.PathSeparator)) {
			continue
		}
		info, err := os.Stat(path)
		if err != nil {
			return err
		}
		read = append(read, stamp{Path: path, Size: info.Size(), MTime: info.ModTime().UnixNano()})
	}
	var absent []string
	slices.Sort(shadowed)
	shadowed = slices.Compact(shadowed)
	if len(shadowed) > 0 {
		dirs, err := searchPath(ctx, tools.cxx)
		if err != nil {
			return err
		}
		for _, dir := range dirs {
			for _, one := range shadowed {
				path := filepath.Join(dir, one)
				if _, err := os.Lstat(path); errors.Is(err, fs.ErrNotExist) {
					absent = append(absent, path)
				}
			}
		}
	}
	body, err := json.Marshal(manifest{Read: read, Absent: absent})
	if err != nil {
		return err
	}
	written := filepath.Join(entry, "manifest.json.new")
	if err := os.WriteFile(written, body, 0o644); err != nil {
		return err
	}
	return os.Rename(written, filepath.Join(entry, "manifest.json"))
}

func manifestHolds(entry string) bool {
	if _, err := os.Stat(filepath.Join(entry, "program")); err != nil {
		return false
	}
	body, err := os.ReadFile(filepath.Join(entry, "manifest.json"))
	if err != nil {
		return false
	}
	var held manifest
	if json.Unmarshal(body, &held) != nil {
		return false
	}
	for _, path := range held.Absent {
		if _, err := os.Lstat(path); !errors.Is(err, fs.ErrNotExist) {
			return false
		}
	}
	for _, one := range held.Read {
		info, err := os.Stat(one.Path)
		if err != nil || info.Size() != one.Size || info.ModTime().UnixNano() != one.MTime {
			return false
		}
	}
	return true
}

func dependencies(rule string) []string {
	_, rest, found := strings.Cut(rule, ": ")
	if !found {
		return nil
	}
	rest = strings.ReplaceAll(rest, "\\\n", " ")
	rest = strings.ReplaceAll(rest, "\\ ", "\x00")
	var out []string
	for _, one := range strings.Fields(rest) {
		out = append(out, strings.ReplaceAll(one, "\x00", " "))
	}
	return out
}

func lockEntry(entry string) (func(), error) {
	if err := os.MkdirAll(entry, 0o755); err != nil {
		return nil, err
	}
	file, err := os.OpenFile(entry+".lock", os.O_CREATE|os.O_RDWR, 0o644)
	if err != nil {
		return nil, err
	}
	if err := syscall.Flock(int(file.Fd()), syscall.LOCK_EX); err != nil {
		file.Close()
		return nil, err
	}
	return func() {
		syscall.Flock(int(file.Fd()), syscall.LOCK_UN)
		file.Close()
	}, nil
}

var searchPaths = struct {
	sync.Mutex
	dirs map[string][]string
}{dirs: map[string][]string{}}

func searchPath(ctx context.Context, cxx string) ([]string, error) {
	key := cxx
	for _, name := range compilerEnvironment {
		key += "\x00" + os.Getenv(name)
	}
	searchPaths.Lock()
	defer searchPaths.Unlock()
	if dirs, known := searchPaths.dirs[key]; known {
		return dirs, nil
	}
	limited, stop := context.WithTimeout(ctx, probeLimit)
	defer stop()
	said, err := grouped(limited, cxx, "-E", "-v", "-x", "c++", os.DevNull).CombinedOutput()
	if err != nil {
		return nil, fmt.Errorf("%s -E -v: %w", cxx, err)
	}
	_, list, found := strings.Cut(string(said), "#include <...> search starts here:\n")
	if !found {
		return nil, fmt.Errorf("%s -E -v names no search path", cxx)
	}
	list, _, _ = strings.Cut(list, "End of search list.")
	var dirs []string
	for _, line := range strings.Split(list, "\n") {
		line = strings.TrimSpace(strings.TrimSuffix(strings.TrimSpace(line), "(framework directory)"))
		if line != "" {
			dirs = append(dirs, line)
		}
	}
	searchPaths.dirs[key] = dirs
	return dirs, nil
}

func (tools toolchain) buildInto(ctx context.Context, made compilation, dir, entry string) (bool, error) {
	unlock, err := lockEntry(entry)
	if err != nil {
		return false, nil
	}
	defer unlock()
	if manifestHolds(entry) {
		now := time.Now()
		os.Chtimes(entry, now, now)
		return true, nil
	}
	os.Remove(filepath.Join(entry, "manifest.json"))
	for _, one := range append([]string{"source.cpp"}, made.files...) {
		if err := copyFile(filepath.Join(dir, one), filepath.Join(entry, one)); err != nil {
			return false, nil
		}
	}
	fresh := fmt.Sprintf("program.%d.new", os.Getpid())
	deps := filepath.Join(entry, fmt.Sprintf("program.%d.d", os.Getpid()))
	defer os.Remove(deps)
	defer os.Remove(filepath.Join(entry, fresh))
	if err := tools.compile(ctx, made, entry, fresh, []string{"-MD", "-MF", deps}); err != nil {
		inside := entry + string(os.PathSeparator)
		return false, errors.New(strings.ReplaceAll(err.Error(), inside, dir+string(os.PathSeparator)))
	}
	if err := os.Rename(filepath.Join(entry, fresh), filepath.Join(entry, "program")); err != nil {
		return false, nil
	}
	shadowed := append(append([]string{}, made.files...), carriedHeaders...)
	if tools.writeManifest(ctx, entry, deps, made.headers, shadowed) != nil {
		os.Remove(filepath.Join(entry, "manifest.json"))
	}
	return true, nil
}
