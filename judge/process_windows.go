//go:build windows

package main

import (
	"context"
	"fmt"
	"os"
	"os/exec"
)

const onWindows = `eo-judge does not run natively on Windows. Install WSL2 and run the Linux eo-judge inside it,
on a checkout in the Linux file system: https://github.com/eolymp/h/blob/main/docs/judge.md#on-windows
`

func init() {
	fmt.Fprint(os.Stderr, onWindows)
	os.Exit(2)
}

func grouped(ctx context.Context, name string, args ...string) *exec.Cmd {
	return exec.CommandContext(ctx, name, args...)
}

func killGroup(command *exec.Cmd) error { return command.Process.Kill() }

const (
	lockExclusive = 1
	lockNoWait    = 2
)

func lockFile(*os.File, int) error { return nil }

func unlockFile(*os.File) error { return nil }
