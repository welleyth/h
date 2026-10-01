#!/usr/bin/env python3
"""What the header costs the programs the pages show: compile time and object size.

The validator is the first example in docs/README.md and the checker the first in
docs/checker.md, so the measurement follows what an author actually writes. Each is compared
with a file that includes only the standard headers eolymp.h uses.
"""
import os
import pathlib
import resource
import subprocess
import sys

from common import ROOT, compiler, cpp_blocks, standard

CEILING_RATIO = 8.5
CEILING_KB = 220


STANDARD_HEADERS = [
    "<algorithm>", "<array>", "<cerrno>", "<cfenv>", "<charconv>", "<chrono>", "<climits>",
    "<clocale>", "<cmath>", "<csignal>", "<cstddef>", "<cstdint>", "<cstdio>", "<cstdlib>",
    "<cstring>", "<exception>", "<fcntl.h>", "<functional>", "<initializer_list>", "<iterator>",
    "<limits>", "<map>", "<memory>", "<new>", "<optional>", "<poll.h>", "<set>", "<signal.h>",
    "<string>", "<string_view>", "<sys/ioctl.h>", "<sys/stat.h>", "<sys/syscall.h>",
    "<system_error>", "<type_traits>", "<unistd.h>", "<utility>", "<vector>"
]


def baseline(root: pathlib.Path) -> str:
    included = [line[len("#include "):] for line in (root / "eolymp.h").read_text().splitlines()
                if line.startswith("#include <")]
    if sorted(included) != sorted(STANDARD_HEADERS):
        added = sorted(set(included) - set(STANDARD_HEADERS))
        dropped = sorted(set(STANDARD_HEADERS) - set(included))
        changes = [f"now includes {', '.join(added)}"] if added else []
        changes += [f"no longer includes {', '.join(dropped)}"] if dropped else []
        raise SystemExit(f"budget: eolymp.h {' and '.join(changes)}, against STANDARD_HEADERS in tools/budget.py; "
                         f"change the list with the header, since the baseline the ceiling divides by is built "
                         f"from it")
    return "".join(f"#include {header}\n" for header in STANDARD_HEADERS) + "int main() { return 0; }\n"


def first_program(page: pathlib.Path) -> str:
    for code in cpp_blocks(page):
        if "int main" in code:
            return code
    raise SystemExit(f"budget: {page} has no program to measure")


def cpu_of_children():
    usage = resource.getrusage(resource.RUSAGE_CHILDREN)
    return usage.ru_utime + usage.ru_stime


def fastest(command, root, runs=3):
    best = float("inf")
    for _ in range(runs):
        before = cpu_of_children()
        subprocess.run(command, cwd=root, check=True, env={**os.environ, "CCACHE_DISABLE": "1"})
        best = min(best, cpu_of_children() - before)
    return best


def main() -> int:
    root = ROOT
    build = root / "build" / "budget"
    build.mkdir(parents=True, exist_ok=True)
    programs = {
        "baseline": baseline(root),
        "validator": first_program(root / "docs" / "README.md"),
        "checker": first_program(root / "docs" / "checker.md"),
    }
    measured = {}
    for name, code in programs.items():
        source = build / f"{name}.cpp"
        source.write_text(code)
        target = build / f"{name}.o"
        compile_time = fastest([*compiler(), f"-std={standard()}", f"-I{root}", "-O2", "-c", "-o", str(target),
                                str(source)], root)
        measured[name] = (compile_time, target.stat().st_size)
    base = measured["baseline"][0]
    worst = 0.0
    for name in ("validator", "checker"):
        compile_time, size = measured[name]
        worst = max(worst, compile_time / base)
        print(f"budget: the {name} takes {compile_time:.2f}s of compiler CPU time to build with -O2, into "
              f"{size // 1024} KB, {compile_time / base:.1f} times the standard headers alone, which take "
              f"{base:.2f}s")
    largest = max(measured[name][1] for name in ("validator", "checker"))
    if largest > CEILING_KB * 1024:
        print(f"budget: an -O2 object is {largest // 1024} KB, above the ceiling of {CEILING_KB} KB",
              file=sys.stderr)
        return 1
    if worst > CEILING_RATIO:
        print(f"budget: an -O2 build takes {worst:.1f} times the compiler CPU time of the standard headers "
              f"alone, above the ceiling of {CEILING_RATIO}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
