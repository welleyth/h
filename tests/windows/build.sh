#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
bin=$1
mkdir -p "$bin"
case $(uname -s) in
    MINGW* | MSYS* | CYGWIN*) suffix=.exe ;;
    *) suffix= ;;
esac
suffix=${SUFFIX-$suffix}
standard=${CXXSTD:-c++17}
warnings=$(sed -n 's/^WARNINGS := //p' Makefile)
jobs=
build() {
    name=$1
    source=$2
    ${CXX:-c++} -std=$standard -O2 $warnings $EXTRA -o "$bin/$name$suffix" "$source" > "$bin/$name.log" 2>&1 &
    jobs="$jobs $!:$name"
}
for name in exit_codes exits validator checker swallowing_checker generator shaper shape_validator shapes_digest \
    real_bits; do
    build "$name" "tests/e2e/$name.cpp"
done
for name in raw_checker lines_checker tokens_checker leaky_generator freopen_validator freopen_generator reals \
    arithmetic; do
    build "$name" "tests/windows/$name.cpp"
done
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
