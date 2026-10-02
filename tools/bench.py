#!/usr/bin/env python3
"""How many instructions the main paths of the header take: reading, checking and generating.

Each program reads or writes one kind of value in bulk, the way a jury program does, and runs
under `perf stat -e instructions:u`, which counts what the program itself executes and so gives
nearly the same number on every run. With a git revision as the argument, the same programs
are built against the headers of that revision too, and the table shows the change.
"""
import pathlib
import random
import shutil
import subprocess
import sys

from common import ROOT, compiler, standard

PROGRAMS = {
    "validator read_ints": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 10000000, "n");
    v.read_eoln();
    std::vector<int> const a = v.read_ints(n, 1, 1000000000, "a");
    v.read_eoln();
}
""", "ints"),
    "validator read_longs": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 10000000, "n");
    v.read_eoln();
    std::vector<long long> const a = v.read_longs(n, 1, 1000000000000000000LL, "a");
    v.read_eoln();
}
""", "longs"),
    "validator read_reals": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 10000000, "n");
    v.read_eoln();
    std::vector<double> const x = v.read_reals(n, -1e6, 1e6, 0, 6, "x");
    v.read_eoln();
}
""", "reals"),
    "validator read_token": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 10000000, "n");
    v.read_eoln();
    for (int at = 0; at < n; at++) {
        v.read_token(1, 15, eo::charset("a-z"), "w");
        v.read_eoln();
    }
}
""", "words"),
    "validator read_line": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(1, 10000000, "n");
    v.read_eoln();
    for (int at = 0; at < n; at++) v.read_line(1, 100, eo::charset("a-z "), "line");
}
""", "lines"),
    "validator read_tree": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    int const n = v.read_int(2, 10000000, "n");
    v.read_eoln();
    v.read_tree(n, "edge");
}
""", "tree"),
    "validator cases": ("""
int main(int argc, char** argv) {
    eo::validator v(argc, argv);
    v.features({"small", "large"});
    int const t = v.read_int(1, 10000000, "t");
    v.read_eoln();
    eo::sum_limit total(1000000000000000000LL, "the sum of x");
    v.cases(t, [&] {
        int const x = v.read_int(1, 1000000000, "x");
        v.read_eoln();
        total += x;
        v.saw(x < 500000000 ? "small" : "large");
        v.stat("largest", x);
    });
}
""", "cases"),
    "checker tokens": ("""
int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    c.input.skip_rest("only the answer matters");
    c.tokens();
}
""", "check-words"),
    "checker reals": ("""
int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    c.input.skip_rest("only the answer matters");
    c.reals(1e-6);
}
""", "check-reals"),
    "checker lines": ("""
int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    c.input.skip_rest("only the answer matters");
    c.lines();
}
""", "check-lines"),
    "checker read_longs": ("""
int main(int argc, char** argv) {
    eo::checker c(argc, argv);
    int const n = c.input.read_int(1, 10000000, "n");
    auto const both = c.read_both([&](eo::stream& s) { return s.read_longs(n, eo::any, "a"); });
    if (both.first != both.second) eo::wrong("the values differ");
    eo::accept();
}
""", "check-longs"),
    "generator ints": ("""
int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 10000000);
    g.out.line(n);
    g.out.line(g.rng().ints(n, 1, 1000000000));
}
""", "-n=2000000"),
    "generator reals": ("""
int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 10000000);
    for (int at = 0; at < n; at++) g.out.line(g.rng().real(-1e9, 1e9));
}
""", "-n=500000"),
    "generator fixed": ("""
int main(int argc, char** argv) {
    eo::generator g(argc, argv);
    int const n = g.option<int>("n", 1, 10000000);
    for (int at = 0; at < n; at++) g.out.line(eo::fixed(g.rng().real(-1e9, 1e9), 6));
}
""", "-n=500000"),
}


def inputs(where: pathlib.Path) -> dict:
    dice = random.Random(2026)
    made = {}

    def keep(name, text):
        path = where / name
        path.write_text(text)
        made[name] = path

    count = 2000000
    keep("ints", f"{count}\n" + " ".join(str(dice.randint(1, 10 ** 9)) for _ in range(count)) + "\n")
    count = 1000000
    keep("longs", f"{count}\n" + " ".join(str(dice.randint(1, 10 ** 18)) for _ in range(count)) + "\n")
    count = 500000
    reals = [f"{dice.uniform(-1e6, 1e6):.6f}" for _ in range(count)]
    keep("reals", f"{count}\n" + " ".join(reals) + "\n")
    count = 1000000
    words = ["".join(chr(97 + dice.randrange(26)) for _ in range(dice.randint(1, 15))) for _ in range(count)]
    keep("words", f"{count}\n" + "\n".join(words) + "\n")
    count = 200000
    lines = [" ".join(words[dice.randrange(len(words))] for _ in range(dice.randint(1, 6))) for _ in range(count)]
    keep("lines", f"{count}\n" + "\n".join(line[:100] for line in lines) + "\n")
    count = 500000
    edges = [(dice.randint(1, at - 1), at) for at in range(2, count + 1)]
    keep("tree", f"{count}\n" + "".join(f"{u} {v}\n" for u, v in edges))
    count = 1000000
    keep("cases", f"{count}\n" + "".join(f"{dice.randint(1, 10 ** 9)}\n" for _ in range(count)))
    keep("one", "1\n")
    keep("words-out", " ".join(words) + "\n")
    keep("reals-out", " ".join(reals) + "\n")
    keep("lines-out", "\n".join(lines) + "\n")
    keep("longs-out", " ".join(made["longs"].read_text().split("\n", 1)[1].split()) + "\n")
    return made


def arguments(kind: str, made: dict) -> list:
    if kind.startswith("-"):
        return [kind]
    if kind.startswith("check-"):
        what = kind[len("check-"):]
        answer = str(made[f"{what}-out"])
        return [str(made["longs"] if what == "longs" else made["one"]), answer, answer]
    return [str(made[kind])]


def count_instructions(binary: pathlib.Path, argv: list, scratch: pathlib.Path) -> float:
    counted = scratch / "perf.txt"
    run = subprocess.run(["perf", "stat", "-x,", "-e", "instructions:u", "-o", str(counted), str(binary), *argv],
                         stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if run.returncode != 0:
        raise SystemExit(f"bench: {binary.name} {' '.join(argv)} exited with {run.returncode}: {run.stderr.strip()}")
    for line in counted.read_text().splitlines():
        fields = line.split(",")
        if len(fields) > 2 and fields[2].startswith("instructions"):
            if not fields[0].isdigit():
                raise SystemExit(f"bench: perf could not count instructions here ({fields[0]}); "
                                 "it needs perf_event_paranoid <= 2 or root")
            return int(fields[0]) / 1e9
    raise SystemExit("bench: perf printed no instruction count")


def build(headers: pathlib.Path, where: pathlib.Path, every: bool = True) -> dict:
    where.mkdir(parents=True, exist_ok=True)
    running = {}
    for at, (name, (body, _)) in enumerate(PROGRAMS.items()):
        source = where / f"program{at}.cpp"
        source.write_text('#include "eolymp.h"\n' + body)
        binary = where / f"program{at}"
        running[name] = (binary, subprocess.Popen([*compiler(), f"-std={standard()}", "-O2", f"-I{headers}", "-o",
                                                   str(binary), str(source)], stderr=subprocess.DEVNULL if not every
                                                  else None))
    built = {name: binary for name, (binary, job) in running.items() if job.wait() == 0}
    if every and len(built) != len(PROGRAMS):
        raise SystemExit(f"bench: the programs do not build against {headers}")
    return built


def headers_of(revision: str, where: pathlib.Path) -> pathlib.Path:
    where.mkdir(parents=True, exist_ok=True)
    for header in ("eolymp.h", "eolymp-shapes.h"):
        shown = subprocess.run(["git", "show", f"{revision}:{header}"], cwd=ROOT, capture_output=True)
        if shown.returncode != 0:
            raise SystemExit(f"bench: git has no {header} at {revision}")
        (where / header).write_bytes(shown.stdout)
    return where


def main() -> int:
    if shutil.which("perf") is None:
        print("bench: perf is not on the path; it counts the instructions", file=sys.stderr)
        return 1
    base = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else None
    scratch = ROOT / "build" / "bench"
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    made = inputs(scratch)
    now = build(ROOT, scratch / "now")
    before = build(headers_of(base, scratch / "base" / "include"), scratch / "base", False) if base else None
    title = f"{'':24} {'G instructions':>15}" + (f" {base:>15} {'change':>8}" if base else "")
    print(title)
    for name, (_, kind) in PROGRAMS.items():
        argv = arguments(kind, made)
        counted = count_instructions(now[name], argv, scratch)
        line = f"{name:24} {counted:15.3f}"
        if before is not None and name in before:
            then = count_instructions(before[name], argv, scratch)
            line += f" {then:15.3f} {100 * (counted - then) / then:+7.1f}%"
        elif before is not None:
            line += f" {'not in ' + base:>15}"
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
