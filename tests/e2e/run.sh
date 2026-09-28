#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${E2E_BUILD:-$root/build/e2e}
mkdir -p "$build"
pids=
build_one() {
    ${CXX:-c++} -std=${CXXSTD:-c++17} "$@" &
    pids="$pids $!"
}
build_one -O2 -Wall -Wextra -Werror -o "$build/exit_codes" "$root/tests/e2e/exit_codes.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/validator" "$root/tests/e2e/validator.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/checker" "$root/tests/e2e/checker.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/swallowing_checker" "$root/tests/e2e/swallowing_checker.cpp"
build_one -O1 -Wall -Wextra -Werror -o "$build/play" "$root/tests/e2e/play.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/interactor" "$root/tests/e2e/interactor.cpp"
build_one -O2 -o "$build/solution" "$root/tests/e2e/solution.cpp"
build_one -O2 -o "$build/hostile" "$root/tests/e2e/hostile.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/stock_checker" "$root/tests/e2e/stock_checker.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/phased" "$root/tests/e2e/phased.cpp"
build_one -O2 -o "$build/phased_solution" "$root/tests/e2e/phased_solution.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/generator" "$root/tests/e2e/generator.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/shaper" "$root/tests/e2e/shaper.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/shape_validator" "$root/tests/e2e/shape_validator.cpp"
build_one -O1 -Wall -Wextra -Werror -o "$build/serve" "$root/tests/e2e/serve.cpp"
build_one -O2 -Wall -Wextra -Werror -o "$build/relay" "$root/tests/e2e/relay.cpp"
build_one -O2 -o "$build/relay_solution" "$root/tests/e2e/relay_solution.cpp"
for pid in $pids; do wait "$pid"; done

failures=0
if ${CXX:-c++} -std=${CXXSTD:-c++17} -Wall -Wextra -Werror -fsyntax-only "$root/tests/e2e/discarded.cpp" \
        > "$build/discarded.log" 2>&1; then
    echo "e2e: an eo::allow, eo::sum_limit or eo::budget written as a bare statement compiled quietly" >&2
    failures=$((failures + 1))
elif [ "$(grep -cE 'discarded.cpp:[0-9]+:[0-9]+: (error|warning): ignoring.*nodiscard' "$build/discarded.log")" != 3 ]; then
    echo "e2e: a scope object written as a bare statement did not give three nodiscard diagnostics:" >&2
    cat "$build/discarded.log" >&2
    failures=$((failures + 1))
else
    echo "e2e: a scope object written as a bare statement is a compiler warning"
fi
expect() {
    request=$1
    wanted_code=$2
    wanted_text=$3
    text=$("$build/exit_codes" "$request" 2>&1) && code=0 || code=$?
    if [ "$code" != "$wanted_code" ]; then
        echo "e2e: $request exited $code, expected $wanted_code" >&2
        failures=$((failures + 1))
    fi
    case $text in
        *"$wanted_text"*) ;;
        *) echo "e2e: $request printed \"$text\", expected \"$wanted_text\"" >&2
           failures=$((failures + 1)) ;;
    esac
}

expect accept 0 ""
expect invalid 3 "line 2: n is 7"
expect library 3 "eolymp.h: two roles in one program"
released=$(sed -n 's/^#define EOLYMP_H_VERSION "\(.*\)"$/\1/p' "$root/src/core.h")
expect version 0 "${released:?src/core.h defines no EOLYMP_H_VERSION}"
expect anything 1 "unknown request"

check_validator() {
    label=$1
    wanted_code=$2
    wanted_text=$3
    shift 3
    text=$("$build/validator" "$build/input.txt" "$@" 2>&1) && code=0 || code=$?
    if [ "$code" != "$wanted_code" ]; then
        echo "e2e: validator $label exited $code, expected $wanted_code" >&2
        failures=$((failures + 1))
    fi
    case $text in
        *"$wanted_text"*) ;;
        *) echo "e2e: validator $label printed \"$text\", expected \"$wanted_text\"" >&2
           failures=$((failures + 1)) ;;
    esac
}

printf '3\n1 2 3\n' > "$build/input.txt"
check_validator "a valid test" 0 "" --group 1
printf '7\n1 2 3 4 5 6 7\n' > "$build/input.txt"
check_validator "a test too big for its subtask" 3 "line 1, n: 7 is above 5" --group 0
printf '3\n1 2 2\n' > "$build/input.txt"
check_validator "a repeated value" 3 "a: elements 2 and 3 are both 2" --group 1
printf '3\n1 2 3' > "$build/input.txt"
check_validator "a missing line break" 3 "expected a line break" --group 1
printf '3\n1 2 3\n' > "$build/input.txt"
check_validator "an unknown subtask" 3 "no subtask 9; known: 0, 1" --group 9
printf '3\n1 2 3\n' > "$build/input.txt"
check_validator "no group at all" 0 ""
printf '3\n1 2 3\n7\n' > "$build/input.txt"
check_validator "trailing content" 3 "expected the end of the input" --group 1

check_checker() {
    label=$1
    wanted_code=$2
    wanted_text=$3
    EOLYMP=1 TEST_COST=40 "$build/checker" "$build/cin.txt" "$build/cout.txt" "$build/cans.txt" \
        > "$build/checker.log" 2>&1 && code=0 || code=$?
    if [ "$code" != "$wanted_code" ]; then
        echo "e2e: checker $label exited $code, expected $wanted_code" >&2
        failures=$((failures + 1))
    fi
    case $(head -1 "$build/checker.log") in
        "$wanted_text"*) ;;
        *) echo "e2e: checker $label logged \"$(head -1 "$build/checker.log")\", expected \"$wanted_text\"" >&2
           failures=$((failures + 1)) ;;
    esac
}

printf '3\n' > "$build/cin.txt"
printf '10 20 30\n' > "$build/cans.txt"
printf '10 20 30\n' > "$build/cout.txt"
check_checker "a correct answer" 0 "ok 3 of 3"
printf '10 99 30\n' > "$build/cout.txt"
check_checker "a partial answer" 7 "points 26.66666667"
printf '99 99 99\n' > "$build/cout.txt"
check_checker "a wrong answer" 1 "wrong answer none of the 3"
printf '10 20\n' > "$build/cout.txt"
check_checker "a short answer" 1 "wrong answer: output.txt"
printf 'x\n' > "$build/cans.txt"
printf '10 20 30\n' > "$build/cout.txt"
check_checker "a broken answer file" 3 "jury error: answer.txt"

printf '3\n' > "$build/sin.txt"
printf '5 5 5\n' > "$build/sout.txt"
EOLYMP=1 TEST_COST=40 "$build/swallowing_checker" "$build/sin.txt" "$build/sout.txt" "$build/sin.txt" \
    > "$build/swallow.log" 2>&1 && code=0 || code=$?
if [ "$code" = 3 ] && grep -q "an exception left the checker before its verdict" "$build/swallow.log"; then
    echo "e2e: an exception the checker swallowed is a jury error, not an accept"
else
    echo "e2e: a checker that swallowed an exception exited $code, expected 3" >&2
    failures=$((failures + 1))
fi

printf '10 20 30\n' > "$build/cans.txt"
printf '10 99 30\n' > "$build/cout.txt"
check_checker "a partial answer again" 7 "points 26.66666667"
if command -v go >/dev/null 2>&1; then
    read_back=$(cd "$root/tests/e2e" && go run readpoints.go "$build/checker.log")
    case $read_back in
        26.6666*) echo "e2e: the judge's own parser reads $read_back out of a log with a trailing space and a blank line" ;;
        *) echo "e2e: the judge's own parser read \"$read_back\" from the log, expected 26.6666..." >&2
           failures=$((failures + 1)) ;;
    esac
else
    echo "e2e: readpoints skipped, go is not installed"
fi

printf '1000 723\n' > "$build/iin.txt"

play_it() {
    label=$1
    wanted=$2
    shift 2
    line=$(TEST_COST=40 "$build/play" "$build/interactor" "$build/iin.txt" "$build/isummary.txt" -- "$@" \
           2>"$build/interactor.log")
    got=$(echo "$line" | sed 's/.*interactor \([0-9-]*\).*/\1/')
    if [ "$got" != "$wanted" ]; then
        echo "e2e: the interactor exited $got against $label, expected $wanted" >&2
        echo "     log: $(head -1 "$build/interactor.log")" >&2
        failures=$((failures + 1))
    fi
}

play_it "a correct solution" 0 "$build/solution"
if [ -s "$build/isummary.txt" ]; then
    verdict=$(TEST_COST=40 "$build/stock_checker" "$build/iin.txt" "$build/isummary.txt" "$build/iin.txt" \
              2>&1) && code=0 || code=$?
    case "$code:$verdict" in
        0:ok*queries) echo "e2e: the stock checker reads the interactor's summary and says \"$verdict\"" ;;
        *) echo "e2e: the stock checker exited $code saying \"$verdict\"" >&2
           failures=$((failures + 1)) ;;
    esac
else
    echo "e2e: the interactor wrote no summary" >&2
    failures=$((failures + 1))
fi

for mode in silent garbage outofrange wrongguess greedy deaf waiting; do
    play_it "a $mode solution" 1 "$build/hostile" "$mode"
done
echo "e2e: seven badly behaved solutions all got a wrong answer, never an interaction failure"

printf '123456789\n' > "$build/pin.txt"
TEST_COST=40 "$build/play" "$build/phased" "$build/pin.txt" "$build/phandoff.txt" -- "$build/phased_solution" \
    > /dev/null 2>"$build/phase1.log"
TEST_COST=40 "$build/play" "$build/phased" "$build/phandoff.txt" "$build/psummary.txt" -- \
    "$build/phased_solution" > /dev/null 2>"$build/phase2.log"
chained=$(TEST_COST=40 "$build/stock_checker" "$build/pin.txt" "$build/psummary.txt" "$build/pin.txt" 2>&1) \
    && chained_code=0 || chained_code=$?
if [ "$chained_code" = 0 ]; then
    echo "e2e: two phases chained through the handoff, and the checker says \"$chained\""
else
    echo "e2e: the chained run ended $chained_code saying \"$chained\"" >&2
    failures=$((failures + 1))
fi

short=$(TEST_COST=40 "$build/stock_checker" "$build/pin.txt" "$build/phandoff.txt" "$build/pin.txt" 2>&1) \
    && short_code=0 || short_code=$?
case "$short_code:$short" in
    3:*"set run_count to 2"*) echo "e2e: a handoff where a summary belongs names the missing run_count" ;;
    *) echo "e2e: run_count too small gave $short_code saying \"$short\"" >&2
       failures=$((failures + 1)) ;;
esac

"$build/generator" -n=20 -max=1000 > "$build/generated.txt" 2>"$build/generator.log"
"$build/generator" -n=20 -max=1000 > "$build/generated_again.txt" 2>/dev/null
if cmp -s "$build/generated.txt" "$build/generated_again.txt"; then
    echo "e2e: the generator gives the same bytes twice for the same arguments"
else
    echo "e2e: the generator gave two different tests for the same arguments" >&2
    failures=$((failures + 1))
fi
"$build/generator" -n=20 -max=1000 -seed=2 > "$build/generated_seeded.txt" 2>/dev/null
if cmp -s "$build/generated.txt" "$build/generated_seeded.txt"; then
    echo "e2e: -seed did not change the test" >&2
    failures=$((failures + 1))
fi
if "$build/validator" "$build/generated.txt" --group 1 > "$build/validated.log" 2>&1; then
    echo "e2e: the validator accepts what the generator wrote"
else
    echo "e2e: the validator rejected the generated test: $(head -1 "$build/validated.log")" >&2
    failures=$((failures + 1))
fi
if "$build/generator" -n=20 -oops=1 > /dev/null 2>"$build/generator_bad.log"; then
    echo "e2e: the generator accepted an option it never declared" >&2
    failures=$((failures + 1))
else
    case $(cat "$build/generator_bad.log") in
        *"unknown option -oops"*) echo "e2e: an undeclared option stops the generator before it writes" ;;
        *) echo "e2e: the generator said \"$(cat "$build/generator_bad.log")\"" >&2
           failures=$((failures + 1)) ;;
    esac
fi
if grep -qw fma /proc/cpuinfo 2>/dev/null; then
    fused=ok
    for compiler in ${CXX:-c++} clang++; do
        command -v "${compiler%% *}" > /dev/null 2>&1 || continue
        $compiler -std=${CXXSTD:-c++17} -O2 -march=haswell -ffp-contract=fast -o "$build/real_bits" \
            "$root/tests/e2e/real_bits.cpp"
        drawn=$("$build/real_bits")
        if [ "$drawn" != 14174797998470170970 ]; then
            echo "e2e: rng.real under $compiler with fused multiply-add drew $drawn" >&2
            failures=$((failures + 1))
            fused=
        fi
    done
    [ -n "$fused" ] && echo "e2e: rng.real draws the same bits with fused multiply-add allowed"
else
    echo "e2e: rng.real under fused multiply-add skipped, this CPU has none"
fi
if [ ! -w /dev/full ]; then
    echo "e2e: a generator writing to a full disk skipped, there is no /dev/full"
else
    "$build/generator" -n=20 -max=1000 > /dev/full 2>"$build/generator_full.log" && code=0 || code=$?
    case "$code $(cat "$build/generator_full.log")" in
        "3 the test could not be written: No space left on device"* | \
        "3 the test could not be written: an earlier write to stdout failed"*)
            echo "e2e: a generator whose test cannot be written fails" ;;
        *) echo "e2e: a generator writing to a full disk exited $code: $(cat "$build/generator_full.log")" >&2
           failures=$((failures + 1)) ;;
    esac
fi

for kind in random path star caterpillar; do
    "$build/shaper" -n=400 "-shape=$kind" -maxw=1000 > "$build/shaped_$kind.txt" 2>/dev/null
    "$build/shaper" -n=400 "-shape=$kind" -maxw=1000 > "$build/shaped_again.txt" 2>/dev/null
    if ! cmp -s "$build/shaped_$kind.txt" "$build/shaped_again.txt"; then
        echo "e2e: the $kind generator gave two different trees for the same arguments" >&2
        failures=$((failures + 1))
    fi
    if ! "$build/shape_validator" "$build/shaped_$kind.txt" --group 1 > "$build/shaped.log" 2>&1; then
        echo "e2e: the validator rejected the $kind tree: $(head -1 "$build/shaped.log")" >&2
        failures=$((failures + 1))
    fi
done
echo "e2e: four tree shapes are trees, and each is the same bytes twice"

climbing=$(tail -n +2 "$build/shaped_path.txt" | awk '$1 < $2' | wc -l | tr -d ' ')
if [ "$climbing" = 399 ] || [ "$climbing" = 0 ]; then
    echo "e2e: presented left the edges in construction order" >&2
    failures=$((failures + 1))
fi
tail -n +2 "$build/shaped_path.txt" | cut -d" " -f1,2 > "$build/shaped_path_edges.txt"
tail -n +2 "$build/shaped_star.txt" | cut -d" " -f1,2 > "$build/shaped_star_edges.txt"
if cmp -s "$build/shaped_path_edges.txt" "$build/shaped_star_edges.txt"; then
    echo "e2e: two different shapes presented the same edges" >&2
    failures=$((failures + 1))
fi
echo "e2e: presented relabelled the path, turned edges and shuffled them"

gcc_says=$(g++ --version 2>/dev/null | head -1)
clang_says=$(clang++ --version 2>/dev/null | head -1)
if [ -n "$gcc_says" ] && [ -n "$clang_says" ] && [ "$gcc_says" != "$clang_says" ]; then
    g++ -std=${CXXSTD:-c++17} -O2 -o "$build/shaper_gcc" "$root/tests/e2e/shaper.cpp"
    clang++ -std=${CXXSTD:-c++17} -O2 -o "$build/shaper_clang" "$root/tests/e2e/shaper.cpp"
    "$build/shaper_gcc" -n=400 -shape=caterpillar -maxw=1000 > "$build/shaped_gcc.txt" 2>/dev/null
    "$build/shaper_clang" -n=400 -shape=caterpillar -maxw=1000 > "$build/shaped_clang.txt" 2>/dev/null
    if cmp -s "$build/shaped_gcc.txt" "$build/shaped_clang.txt"; then
        echo "e2e: the same tree comes out of g++ and clang++"
    else
        echo "e2e: g++ and clang++ built different trees from the same arguments" >&2
        failures=$((failures + 1))
    fi

    g++ -std=${CXXSTD:-c++17} -O2 -o "$build/digest_gcc" "$root/tests/e2e/shapes_digest.cpp"
    clang++ -std=${CXXSTD:-c++17} -O2 -o "$build/digest_clang" "$root/tests/e2e/shapes_digest.cpp"
    "$build/digest_gcc" > "$build/digest_gcc.txt"
    "$build/digest_clang" > "$build/digest_clang.txt"
    if cmp -s "$build/digest_gcc.txt" "$build/digest_clang.txt"; then
        echo "e2e: every sorted and sampled shape is the same under g++ and clang++"
    else
        echo "e2e: the shapes differ between g++ and clang++: $(cmp "$build/digest_gcc.txt" \
             "$build/digest_clang.txt" 2>&1 | head -1)" >&2
        failures=$((failures + 1))
    fi
else
    echo "e2e: the cross-compiler checks need two different compilers; here g++ and clang++ are one"
fi

mkdir -p "$build/com"
printf '3 12345\n' > "$build/com/in.txt"

serve_it() {
    label=$1
    limit=$2
    wanted=$3
    shift 3
    line=$(TEST_COST=40 "$build/serve" "$build/com" "$limit" "$build/relay" "$build/com/in.txt" \
           "$build/com/summary.txt" -- "$@" 2>"$build/com/log.txt")
    got=$(echo "$line" | sed 's/controller \([0-9-]*\).*/\1/')
    if [ "$got" != "$wanted" ]; then
        echo "e2e: the controller exited $got against $label, expected $wanted" >&2
        echo "     log: $(head -1 "$build/com/log.txt")" >&2
        failures=$((failures + 1))
    fi
}

rm -f "$build/com/summary.txt"
serve_it "a correct relay" 10 0 "$build/relay_solution"
relayed=$(TEST_COST=40 "$build/stock_checker" "$build/com/in.txt" "$build/com/summary.txt" \
          "$build/com/in.txt" 2>&1) && relayed_code=0 || relayed_code=$?
if [ "$relayed_code" = 0 ]; then
    echo "e2e: three instances relayed a secret over real pipes, and the checker says \"$relayed\""
else
    echo "e2e: the relay ended $relayed_code saying \"$relayed\"" >&2
    failures=$((failures + 1))
fi
serve_it "an instance beyond the limit" 2 3 "$build/relay_solution"
serve_it "an instance that says nothing" 10 1 "$build/relay_solution" mute
serve_it "an instance that lies" 10 1 "$build/relay_solution" liar
echo "e2e: the limit is a jury error, and a silent or lying instance is a wrong answer"

if [ "$failures" != 0 ]; then
    echo "e2e: $failures checks failed" >&2
    exit 1
fi
echo "e2e: exit codes and messages are what the judge would see"
