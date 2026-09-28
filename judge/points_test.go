package main

import (
	"io"
	"strings"
	"testing"
)

func writeLog(t *testing.T, body string) io.Reader {
	t.Helper()
	return strings.NewReader(body)
}

func TestReadPoints(t *testing.T) {
	t.Parallel()
	for _, one := range []struct {
		name  string
		log   string
		want  float32
		fails bool
	}{
		{name: "the verdict line alone", log: "points 25 matched 10 of 40\n", want: 25},
		{name: "a fraction", log: "points 6.5585 1009 of 2000 degrees\n", want: 6.5585},
		{name: "nothing to read", log: "ok all 12 degrees\n", fails: true},
		{name: "a word on an earlier line", log: "wrong answer\npoints 3\n", fails: true},
		{name: "a blank line before the score", log: "ok\n\npoints 3\n", fails: true},
		{name: "a trailing space before the score", log: "ok \npoints 3\n", fails: true},
		{name: "a word on the same line", log: "ok points 3\n", want: 3},
		{name: "an empty log", log: "", fails: true},
	} {
		t.Run(one.name, func(t *testing.T) {
			got, err := readPoints(writeLog(t, one.log))
			if one.fails {
				if err == nil {
					t.Fatalf("expected a failure, got %v", got)
				}
				return
			}
			if err != nil {
				t.Fatal(err)
			}
			if got != one.want {
				t.Fatalf("got %v, want %v", got, one.want)
			}
		})
	}
}

func TestReadPointsTakesTheFirstScore(t *testing.T) {
	t.Parallel()
	got, err := readPoints(writeLog(t, "points 10 first\npoints 90 second\n"))
	if err != nil {
		t.Fatal(err)
	}
	if got != 10 {
		t.Fatalf("got %v, want the first score", got)
	}
}
