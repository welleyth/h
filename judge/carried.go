package main

import (
	"bytes"
	"crypto/sha256"
	"embed"
	"encoding/hex"
	"os"
	"path/filepath"
	"sync"
)

//go:embed include/eolymp.h include/eolymp-shapes.h
var carried embed.FS

var carriedHeaders = []string{"eolymp.h", "eolymp-shapes.h"}

var carriedSum = sync.OnceValue(func() string {
	sum := sha256.New()
	for _, name := range carriedHeaders {
		body, err := carried.ReadFile("include/" + name)
		if err != nil {
			panic(err)
		}
		field(sum, name, string(body))
	}
	return hex.EncodeToString(sum.Sum(nil))[:16]
})

func (tools toolchain) carriedDirs(work string) []string {
	local := filepath.Join(work, "include")
	if tools.cache == "" {
		return []string{local}
	}
	return []string{filepath.Join(tools.cache, "include", carriedSum()), local}
}

func (tools toolchain) writeCarried(work string) (string, error) {
	var err error
	for _, dir := range tools.carriedDirs(work) {
		if err = writeHeaders(dir); err == nil {
			return dir, nil
		}
	}
	return "", err
}

func writeHeaders(dir string) error {
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	for _, name := range carriedHeaders {
		body, err := carried.ReadFile("include/" + name)
		if err != nil {
			return err
		}
		path := filepath.Join(dir, name)
		if present, err := os.ReadFile(path); err == nil && bytes.Equal(present, body) {
			continue
		}
		written, err := os.CreateTemp(dir, name+".*")
		if err != nil {
			return err
		}
		_, err = written.Write(body)
		if closed := written.Close(); err == nil {
			err = closed
		}
		if err == nil {
			err = os.Rename(written.Name(), path)
		}
		if err != nil {
			os.Remove(written.Name())
			return err
		}
	}
	return nil
}
