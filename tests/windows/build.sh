#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
bin=$1
kind=${2:-gnu}
mkdir -p "$bin"
case $(uname -s) in
    MINGW* | MSYS* | CYGWIN*) suffix=.exe ;;
    *) suffix= ;;
esac
suffix=${SUFFIX-$suffix}
standard=${CXXSTD:-c++17}
warnings=$(sed -n 's/^WARNINGS := //p' Makefile)
jobs=
build_with() {
    name=$1
    source=$2
    strict=$3
    if [ "$kind" = msvc ]; then
        checked="-W4 -WX"
        [ -z "$strict" ] && checked=-D_CRT_SECURE_NO_WARNINGS
        ${CXX:-cl} -nologo -std:$standard $checked -EHsc -O2 -Fe:"$bin/$name.exe" -Fo:"$bin/$name.obj" "$source" \
            > "$bin/$name.log" 2>&1 &
    else
        ${CXX:-c++} -std=$standard -O2 $strict $EXTRA -o "$bin/$name$suffix" "$source" > "$bin/$name.log" 2>&1 &
    fi
    jobs="$jobs $!:$name"
}
build() {
    build_with "$1" "$2" "$warnings"
}
for name in exit_codes exits validator checker swallowing_checker generator shaper shape_validator shapes_digest \
    real_bits interactor stock_checker phased; do
    build "$name" "tests/e2e/$name.cpp"
done
for name in raw_checker lines_checker tokens_checker leaky_generator freopen_validator freopen_generator reals \
    arithmetic bulk_interactor; do
    build "$name" "tests/windows/$name.cpp"
done
for name in solution hostile phased_solution bulk_solution; do
    build_with "$name" "tests/e2e/$name.cpp"
done
if [ -n "$suffix" ]; then
    build_with play tests/windows/play.cpp
else
    build play tests/e2e/play.cpp
fi
if [ -n "$suffix" ]; then
    for name in windows_first windows_last; do
        build "$name" "tests/windows/$name.cpp"
    done
fi
unbuilt=
for job in $jobs; do
    wait "${job%%:*}" || unbuilt="$unbuilt ${job#*:}"
done
if [ -n "$unbuilt" ]; then
    for name in $unbuilt; do
        cat "$bin/$name.log" >&2
    done
    echo "windows: these programs did not build, and their compiler errors are above:$unbuilt" >&2
    exit 1
fi
echo "windows: every program built in $bin"
