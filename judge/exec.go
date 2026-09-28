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
	Name string
	Exe  string
	Dir  string
}

type Status struct {
	ExitCode int
	Signal   bool
	Wall     int
	Memory   int64
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
	cxx string
}

func hostToolchain() toolchain {
	return toolchain{cxx: compiler()}
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
	for _, one := range program.Files {
		if err := copyFile(problem.Path(one), filepath.Join(dir, filepath.Base(one))); err != nil {
			return nil, fmt.Errorf("%s needs %s: %w", name, one, err)
		}
	}

	exe := filepath.Join(dir, "program")
	command := grouped(ctx, tools.cxx, "-std="+standard(program.Runtime), "-O2", "-idirafter", dir,
		"-o", exe, filepath.Join(dir, "source.cpp"))
	said, err := command.CombinedOutput()
	if ctx.Err() != nil {
		return nil, fmt.Errorf("the build of %s was interrupted", name)
	}
	if err != nil {
		return nil, fmt.Errorf("%s does not compile:\n%s", name, strings.TrimSpace(string(said)))
	}

	return &Built{Name: name, Exe: exe, Dir: dir}, nil
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
		if usage, ok := state.SysUsage().(*syscall.Rusage); ok {
			status.Memory = int64(usage.Maxrss)
		}
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
