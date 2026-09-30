#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$root/build/hostile
mkdir -p "$build"
CXX=${CXX:-c++}
CXXSTD=${CXXSTD:-c++17}

build_and_run() {
    name=$1
    shift
    $CXX -std=$CXXSTD -O1 "$@" -o "$build/$name" "$root/tests/hostile/$name.cpp"
    "$build/$name" < /dev/null
    echo "hostile: $name builds and runs"
}

build_and_run organiser_names -Wall -Wextra -Wshadow -Werror

if printf '#include <cstddef>\n#ifndef __GLIBCXX__\n#error\n#endif\n' | $CXX -std=$CXXSTD -fsyntax-only -x c++ - 2>/dev/null; then
    build_and_run header_alone -Wall -Wextra -Werror
else
    echo "hostile: header_alone skipped, this standard library is not libstdc++, which the judge runs"
fi

if echo '#include <bits/stdc++.h>' | $CXX -std=$CXXSTD -fsyntax-only -x c++ - 2>/dev/null; then
    build_and_run after_bits -Wall -Wextra -Werror
else
    echo "hostile: after_bits skipped, this standard library has no <bits/stdc++.h>"
fi
