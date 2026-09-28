package main

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

type Built struct {
	Name   string
	Exe    string
	Dir    string
	Source string
}

type Status struct {
	ExitCode int
	Signal   bool
	Wall     int
	TimedOut bool
	Stdout   []byte
	Stderr   []byte
}

func compiler() string {
	if named := os.Getenv("CXX"); named != "" {
		return named
	}
	return "c++"
}

func standard(runtime string) string {
	switch {
	case strings.Contains(runtime, "cpp:23"):
		return "c++23"
	case strings.Contains(runtime, "cpp:20"):
		return "c++20"
	default:
		return "c++17"
	}
}

func copyFile(from, to string) error {
	body, err := os.ReadFile(from)
	if err != nil {
		return err
	}
	return os.WriteFile(to, body, 0o644)
}

type toolchain struct {
	cxx   string
	cache string
}

func hostToolchain() toolchain {
	return toolchain{cxx: compiler(), cache: writable(cacheRoot())}
}

func (tools toolchain) build(ctx context.Context, problem *Problem, name string, program *Program,
	work string) (*Built, error) {
	if program == nil || program.Source == "" {
		return nil, fmt.Errorf("%s has no source", name)
	}

	dir := filepath.Join(work, name)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return nil, err
	}

	if err := copyFile(problem.Path(program.Source), filepath.Join(dir, "source.cpp")); err != nil {
		return nil, err
	}
	var files []string
	for _, one := range program.Files {
		if err := copyFile(problem.Path(one), filepath.Join(dir, filepath.Base(one))); err != nil {
			return nil, fmt.Errorf("%s needs %s: %w", name, one, err)
		}
		files = append(files, filepath.Base(one))
	}

	headers, err := tools.writeCarried(work)
	if err != nil {
		return nil, err
	}
	made := compilation{name: name, args: []string{"-std=" + standard(program.Runtime), "-O2"},
		headers: headers, files: files}
	if tools.cache != "" {
		keyed := append(append([]string{}, made.args...), "-idirafter", headers)
		if key, err := cacheKey(tools.cxx, keyed, dir, files); err == nil {
			entry := filepath.Join(tools.cache, key[:2], key)
			cached, err := tools.buildInto(ctx, made, dir, entry)
			if err != nil {
				return nil, err
			}
			if cached {
				return &Built{Name: name, Exe: filepath.Join(entry, "program"), Dir: dir,
					Source: filepath.Join(entry, "source.cpp")}, nil
			}
		}
	}
	if err := tools.compile(ctx, made, dir, "program", nil); err != nil {
		return nil, err
	}
	return &Built{Name: name, Exe: filepath.Join(dir, "program"), Dir: dir, Source: filepath.Join(dir, "source.cpp")}, nil
}

type compilation struct {
	name    string
	args    []string
	headers string
	files   []string
}

func (tools toolchain) compile(ctx context.Context, made compilation, dir, exe string, extra []string) error {
	name := made.name
	line := append(append(append([]string{}, made.args...), extra...), "-idirafter", dir, "-idirafter", made.headers,
		"-o", filepath.Join(dir, exe), filepath.Join(dir, "source.cpp"))
	said, err := grouped(ctx, tools.cxx, line...).CombinedOutput()
	if ctx.Err() != nil {
		return fmt.Errorf("the build of %s was interrupted", name)
	}
	if err != nil {
		said := strings.TrimSpace(string(said))
		if said == "" {
			said = err.Error()
		}
		return fmt.Errorf("%s does not compile:\n%s", name, said)
	}
	return nil
}

type Invocation struct {
	Args    []string
	Env     map[string]string
	Dir     string
	Stdin   io.Reader
	Stdout  io.Writer
	Stderr  io.Writer
	LimitMS int
}

func run(ctx context.Context, exe string, call Invocation) (*Status, error) {
	limit := call.LimitMS
	if limit <= 0 {
		limit = 10000
	}

	inner, stop := context.WithTimeout(ctx, time.Duration(limit)*time.Millisecond)
	defer stop()

	command := grouped(inner, exe, call.Args...)
	command.Dir = call.Dir
	command.Env = append(os.Environ(), flatten(call.Env)...)
	command.Stdin = call.Stdin

	var out, errs bytes.Buffer
	if call.Stdout != nil {
		command.Stdout = call.Stdout
	} else {
		command.Stdout = &out
	}
	if call.Stderr != nil {
		command.Stderr = call.Stderr
	} else {
		command.Stderr = &errs
	}

	started := time.Now()
	err := command.Run()
	elapsed := int(time.Since(started).Milliseconds())
	if command.Process != nil {
		killGroup(command)
	}
	if errors.Is(err, exec.ErrWaitDelay) {
		err = nil
	}

	status := &Status{Wall: elapsed, Stdout: out.Bytes(), Stderr: errs.Bytes()}
	if state := command.ProcessState; state != nil {
		status.ExitCode = state.ExitCode()
		if wait, ok := state.Sys().(syscall.WaitStatus); ok && wait.Signaled() {
			status.Signal = true
		}
	}
	if inner.Err() == context.DeadlineExceeded {
		status.TimedOut = true
		return status, nil
	}
	if err != nil && status.ExitCode == 0 {
		return status, err
	}
	return status, nil
}

const (
	checkerLimit   = 10000
	validatorLimit = 30000
	generatorLimit = 60000
)

func (b *Built) jury(ctx context.Context, limit int, call Invocation) (*Status, error) {
	call.Dir, call.LimitMS, call.Env = b.Dir, limit, map[string]string{"EOLYMP": "1"}
	return run(ctx, b.Exe, call)
}

func grouped(ctx context.Context, name string, args ...string) *exec.Cmd {
	command := exec.CommandContext(ctx, name, args...)
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	command.Cancel = func() error { return killGroup(command) }
	command.WaitDelay = 250 * time.Millisecond
	return command
}

func killGroup(command *exec.Cmd) error {
	return syscall.Kill(-command.Process.Pid, syscall.SIGKILL)
}

func flatten(env map[string]string) []string {
	out := make([]string, 0, len(env))
	for key, value := range env {
		out = append(out, key+"="+value)
	}
	return out
}

func reference(run *Planned) string {
	return fmt.Sprintf("%d:%d", run.Group, run.Test.Index)
}
