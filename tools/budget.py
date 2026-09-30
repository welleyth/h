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

CEILING_RATIO = 9.0
CEILING_KB = 220

BASELINE = """\
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
int main() { return 0; }
"""


def first_program(page: pathlib.Path) -> str:
    for code in cpp_blocks(page):
        if "int main" in code:
            return code
    raise SystemExit(f"budget: {page} has no program to measure")


def cpu_of_children():
    usage = resource.getrusage(resource.RUSAGE_CHILDREN)
    return usage.ru_utime + usage.ru_stime


def fastest(command, root, runs=3, enough=0.0):
    best = float("inf")
    for _ in range(runs):
        before = cpu_of_children()
        subprocess.run(command, cwd=root, check=True, env={**os.environ, "CCACHE_DISABLE": "1"})
        best = min(best, cpu_of_children() - before)
        if best <= enough:
            break
    return best


def main() -> int:
    root = ROOT
    build = root / "build" / "budget"
    build.mkdir(parents=True, exist_ok=True)
    programs = {
        "baseline": BASELINE,
        "validator": first_program(root / "docs" / "README.md"),
        "checker": first_program(root / "docs" / "checker.md"),
    }
    measured = {}
    for name, code in programs.items():
        source = build / f"{name}.cpp"
        source.write_text(code)
        target = build / f"{name}.o"
        ceiling = 0.0 if name == "baseline" else CEILING_RATIO * measured["baseline"][0]
        compile_time = fastest([*compiler(), f"-std={standard()}", f"-I{root}", "-O2", "-c", "-o", str(target),
                                str(source)], root, enough=ceiling)
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
