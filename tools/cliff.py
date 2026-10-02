#!/usr/bin/env python3
"""Whether GCC still inlines what the main paths need: its unit growth limit, counted.

At -O2, GCC lets inlining grow a translation unit by --param inline-unit-growth and then
refuses the rest, so code that no path runs can still push a hot call out of line: 2.4.0 met
it when a few lines in the validator's describe() cost read_line 4.8%. Each bench program is
built with -fopt-info-inline-missed, and the refusals that name that limit are counted.

The count depends on the compiler and its library down to the version, so the gate holds only
on the one that CI's musl job runs, GCC 14.2.0 with musl's headers, and says it skipped
anywhere else. A program that 2.3.0 kept under the limit must stay under it. The three that
2.3.0 already pushed past it are left out: their counts differ by a few between two builds of
the same GCC 14.2.0, here 25 and 27, so no ceiling on them would hold.
"""
import pathlib
import subprocess
import sys
import tempfile

from bench import PROGRAMS
from common import ROOT, compiler, standard

PINNED = {"__GNUC__": "14", "__GNUC_MINOR__": "2", "__GNUC_PATCHLEVEL__": "0", "__GLIBCXX__": "20240801"}

PAST_THE_LIMIT = {"validator read_reals", "validator read_tree", "checker read_longs"}

REFUSED = "inline-unit-growth limit reached"


def identity() -> dict:
    shown = subprocess.run([*compiler(), "-dM", "-E", "-x", "c++", "-"], input="#include <cstddef>\n",
                           capture_output=True, text=True)
    macros = {}
    for line in shown.stdout.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[0] == "#define":
            macros[parts[1]] = parts[2]
    return macros


def main() -> int:
    macros = identity()
    if "__clang__" in macros or "__GLIBC__" in macros or any(macros.get(k) != v for k, v in PINNED.items()):
        found = ".".join(macros.get(k, "?") for k in ("__GNUC__", "__GNUC_MINOR__", "__GNUC_PATCHLEVEL__"))
        print(f"cliff: skipped; the counts hold for GCC 14.2.0 on musl, as CI's musl job has it, and this "
              f"compiler is {'clang' if '__clang__' in macros else 'GCC ' + found}"
              f"{' with glibc' if '__GLIBC__' in macros else ''}")
        return 0
    failed = 0
    with tempfile.TemporaryDirectory(prefix="eolymp-cliff-") as scratch:
        where = pathlib.Path(scratch)
        for at, (name, (body, _)) in enumerate(PROGRAMS.items()):
            if name in PAST_THE_LIMIT:
                continue
            source = where / f"program{at}.cpp"
            source.write_text('#include "eolymp.h"\n' + body)
            missed = where / f"program{at}.txt"
            subprocess.run([*compiler(), f"-std={standard()}", "-O2", f"-I{ROOT}", f"-fopt-info-inline-missed={missed}",
                            "-c", "-o", str(where / f"program{at}.o"), str(source)], check=True)
            count = missed.read_text().count(REFUSED)
            if count > 0:
                failed += 1
                print(f"cliff: {name} has {count} inlining refusals for {REFUSED!r}; GCC is out of room for the "
                      f"unit, so mark the code no path runs EOLYMP_COLD", file=sys.stderr)
    if failed == 0:
        print(f"cliff: {len(PROGRAMS) - len(PAST_THE_LIMIT)} programs, none refused inlining for the unit's growth")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
