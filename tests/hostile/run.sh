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
build_and_run windows_macros -Wall -Wextra -Wshadow -Werror

if printf '#include <cstddef>\n#ifndef __GLIBCXX__\n#error\n#endif\n' | $CXX -std=$CXXSTD -fsyntax-only -x c++ - 2>/dev/null; then
    build_and_run header_alone -Wall -Wextra -Werror
else
    echo "hostile: header_alone skipped, this standard library is not libstdc++, which the judge runs"
fi

strict="-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Werror"
if echo 'int main() {}' | $CXX -Wuseless-cast -Werror -fsyntax-only -x c++ - 2>/dev/null; then
    strict="$strict -Wuseless-cast"
fi
build_and_run strict $strict

if echo '#include <bits/stdc++.h>' | $CXX -std=$CXXSTD -fsyntax-only -x c++ - 2>/dev/null; then
    build_and_run after_bits -Wall -Wextra -Werror
else
    echo "hostile: after_bits skipped, this standard library has no <bits/stdc++.h>"
fi

refused_with() {
    name=$1
    said=$2
    shift 2
    if $CXX -std=$CXXSTD -fsyntax-only "$@" "$root/tests/hostile/$name.cpp" > "$build/$name.log" 2>&1; then
        echo "hostile: $name builds, and it must not" >&2
        exit 1
    fi
    if ! grep -q "$said" "$build/$name.log"; then
        echo "hostile: $name fails to build without saying \"$said\":" >&2
        cat "$build/$name.log" >&2
        exit 1
    fi
    echo "hostile: $name is refused at compile time, saying \"$said\""
}

refused_with unprintable "eolymp.h cannot print this type"

refused_with both_namespaces "ambiguous"
build_and_run qualified_names -Wall -Wextra -Wshadow -Werror

for standard in c++11 c++14; do
    refused_with too_old "eolymp.h needs C++17 or later: build with -std=c++17" -std=$standard
    if [ "$(grep -c "error:" "$build/too_old.log")" -ne 1 ]; then
        echo "hostile: too_old at $standard fails with more than the one error that names the standard:" >&2
        cat "$build/too_old.log" >&2
        exit 1
    fi
done

checked="-std=c++20 -DEOLYMP_CHECK_PATTERNS"
if printf '#ifndef __cpp_consteval\n#error\n#endif\n' | $CXX $checked -fsyntax-only -x c++ - 2>/dev/null; then
    build_and_run checked_patterns $checked -Wall -Wextra -Wshadow -Werror
    refused_with pattern_count a_message_needs_one_placeholder_for_each_value $checked
    refused_with pattern_brace a_message_needs_two_braces_to_print_one $checked
    refused_with pattern_syntax a_pattern_that_does_not_parse $checked
else
    echo "hostile: the compile-time check of messages skipped, this compiler has no consteval"
fi
