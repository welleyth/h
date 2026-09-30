package main

import (
	"embed"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path"
	"path/filepath"
	"slices"
	"strings"
)

//go:embed templates
var templates embed.FS

var kinds = []string{"program", "interactive", "phases"}

func initProblem(args []string, out, errs io.Writer) int {
	flags := flag.NewFlagSet("init", flag.ContinueOnError)
	kind := flags.String("type", "program", "the kind of problem to write")
	asJSON := flags.Bool("json", false, "refused: init writes files")
	dir, code, parsed := onePositional(flags, args, out, errs)
	if !parsed {
		return code
	}
	if *asJSON {
		fmt.Fprintln(errs, "eo-judge: --json applies to run, check, lint and stress; init prints only the files it wrote")
		return 2
	}
	if !slices.Contains(kinds, *kind) {
		fmt.Fprintf(errs, "eo-judge: --type is %q; it is one of %s\n", *kind, strings.Join(kinds, ", "))
		return 2
	}
	if present, err := os.ReadDir(dir); err == nil && len(present) > 0 {
		fmt.Fprintf(errs, "eo-judge: %s already holds %s; init writes a problem into a new or empty directory\n",
			dir, present[0].Name())
		return 2
	}
	written, err := writeTemplate(*kind, dir)
	if err != nil {
		fmt.Fprintln(errs, "eo-judge:", err)
		return 3
	}
	fmt.Fprintf(out, "eo-judge: wrote %s %s problem to %s: %s\nnext: eo-judge run %s\n", article(*kind), *kind, dir,
		strings.Join(written, ", "), dir)
	return 0
}

func writeTemplate(kind, dir string) ([]string, error) {
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return nil, err
	}
	entries, err := fs.ReadDir(templates, path.Join("templates", kind))
	if err != nil {
		return nil, err
	}
	var written []string
	for _, entry := range entries {
		body, err := templates.ReadFile(path.Join("templates", kind, entry.Name()))
		if err != nil {
			return nil, err
		}
		if err := os.WriteFile(filepath.Join(dir, entry.Name()), body, 0o644); err != nil {
			return nil, err
		}
		written = append(written, entry.Name())
	}
	return written, nil
}

func article(word string) string {
	if strings.ContainsRune("aeiou", rune(word[0])) {
		return "an"
	}
	return "a"
}
