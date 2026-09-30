//go:build !windows

package main

import (
	"context"
	"os"
	"os/exec"
	"syscall"
	"time"
)

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

const (
	lockExclusive = syscall.LOCK_EX
	lockNoWait    = syscall.LOCK_NB
)

func lockFile(file *os.File, how int) error { return syscall.Flock(int(file.Fd()), how) }

func unlockFile(file *os.File) error { return syscall.Flock(int(file.Fd()), syscall.LOCK_UN) }
