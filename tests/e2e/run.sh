#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${E2E_BUILD:-$root/build/e2e}
mkdir -p "$build"
warnings=${WARNINGS:-$(sed -n 's/^WARNINGS := //p' "$root/Makefile")}
jobs=
build_with() {
    name=$1
    shift
    "$@" &
    jobs="$jobs $!:$name"
}
build_one() {
    name=$1
    shift
    build_with "$name" ${CXX:-c++} -std=${CXXSTD:-c++17} "$@" -o "$build/$name" "$root/tests/e2e/$name.cpp"
}
build_one exit_codes -O2 $warnings
build_one exits -O2 $warnings
build_one dies -O2 $warnings
build_one validator -O2 $warnings
build_one checker -O2 $warnings
build_one swallowing_checker -O2 $warnings
build_one play -O1 $warnings
build_one interactor -O2 $warnings
build_one solution -O2
build_one hostile -O2
build_one stock_checker -O2 $warnings
build_one phased -O2 $warnings
build_one phased_solution -O2
build_one generator -O2 $warnings
build_one shaper -O2 $warnings
build_one shape_validator -O2 $warnings
build_one serve -O1 $warnings
build_one relay -O2 $warnings
build_one relay_solution -O2
build_one bulk -O2 $warnings
build_one bulk_solution -O2
fused=
if grep -qw fma /proc/cpuinfo 2>/dev/null; then
    fused=ok
    for compiler in "${CXX:-c++}" clang++; do
        command -v "${compiler%% *}" > /dev/null 2>&1 || continue
        drawer=$((${drawer:-0} + 1))
        fused="$fused $drawer"
        echo "$compiler" > "$build/real_bits_$drawer.by"
        build_with "real_bits_$drawer" $compiler -std=${CXXSTD:-c++17} -O2 -march=haswell -ffp-contract=fast \
            -o "$build/real_bits_$drawer" "$root/tests/e2e/real_bits.cpp"
    done
fi
gcc_says=$(g++ --version 2>/dev/null | head -1)
clang_says=$(clang++ --version 2>/dev/null | head -1)
crossed=
if [ -n "$gcc_says" ] && [ -n "$clang_says" ] && [ "$gcc_says" != "$clang_says" ]; then
    crossed=yes
    for pair in gcc:g++ clang:clang++; do
        build_with "shaper_${pair%%:*}" ${pair#*:} -std=${CXXSTD:-c++17} -O2 -o "$build/shaper_${pair%%:*}" \
            "$root/tests/e2e/shaper.cpp"
        build_with "digest_${pair%%:*}" ${pair#*:} -std=${CXXSTD:-c++17} -O2 -o "$build/digest_${pair%%:*}" \
            "$root/tests/e2e/shapes_digest.cpp"
    done
fi
unbuilt=
for job in $jobs; do
    wait "${job%%:*}" || unbuilt="$unbuilt ${job#*:}"
done
if [ -n "$unbuilt" ]; then
    echo "e2e: these programs did not build, and their compiler errors are above:$unbuilt" >&2
    exit 1
fi
set +e

failures=0
log="$build/run.log"
pass() {
    echo "e2e: $1"
}
fail() {
    echo "e2e: $1" >&2
    [ -s "$2" ] && echo "     log: $(head -1 "$2")" >&2
    failures=$((failures + 1))
    return 1
}
expect_run() {
    label=$1
    wanted_code=$2
    wanted=$3
    shift 3
    "$@" > "$log" 2>&1
    code=$?
    [ "$code" = "$wanted_code" ] || { fail "$label exited $code, expected $wanted_code" "$log"; return 1; }
    case $(cat "$log") in
        $wanted) return 0 ;;
    esac
    fail "$label printed \"$(head -1 "$log")\", expected \"$wanted\""
}
expect_death() {
    label=$1
    wanted_code=$2
    wanted=$3
    shift 3
    { "$@" > "$log" 2>&1 & wait $!; } 2>/dev/null
    code=$?
    [ "$code" = "$wanted_code" ] || { fail "$label exited $code, expected $wanted_code" "$log"; return 1; }
    case $(cat "$log") in
        $wanted) return 0 ;;
    esac
    fail "$label printed \"$(head -1 "$log")\", expected \"$wanted\""
}

if ${CXX:-c++} -std=${CXXSTD:-c++17} $warnings -fsyntax-only "$root/tests/e2e/discarded.cpp" > "$log" 2>&1; then
    fail "an eo::allow, eo::sum_limit or eo::budget written as a bare statement compiled quietly"
elif [ "$(grep -cE 'discarded.cpp:[0-9]+:[0-9]+: (error|warning): ignoring.*nodiscard' "$log")" != 3 ]; then
    fail "a scope object written as a bare statement did not give three nodiscard diagnostics" "$log"
    cat "$log" >&2
else
    pass "a scope object written as a bare statement is a compiler warning"
fi

expect_run "exit_codes accept" 0 "*" "$build/exit_codes" accept
expect_run "exit_codes invalid" 3 "*line 2: n is 7*" "$build/exit_codes" invalid
expect_run "exit_codes library" 3 "*eolymp.h: two roles in one program*" "$build/exit_codes" library
released=$(python3 "$root/tools/version.py" --print) || fail "tools/version.py cannot read the version"
expect_run "exit_codes version" 0 "${released:-no version}" "$build/exit_codes" version
expect_run "exit_codes anything" 1 "*unknown request*" "$build/exit_codes" anything

validate() {
    label=$1
    wanted_code=$2
    wanted=$3
    shift 3
    expect_run "validator: $label" "$wanted_code" "$wanted" "$build/validator" "$build/input.txt" "$@"
}

printf '3\n1 2 3\n' > "$build/input.txt"
validate "a valid test" 0 "*" --group 1
printf '7\n1 2 3 4 5 6 7\n' > "$build/input.txt"
validate "a test too big for its subtask" 3 "*line 1, n: 7 is above 5*" --group 0
printf '3\n1 2 2\n' > "$build/input.txt"
validate "a repeated value" 3 "*a: elements 2 and 3 are both 2*" --group 1
printf '3\n1 2 3' > "$build/input.txt"
validate "a missing line break" 3 "*expected a line break*" --group 1
printf '3\n1 2 3\n' > "$build/input.txt"
validate "an unknown subtask" 3 "*no subtask 9; known: 0, 1*" --group 9
printf '3\n1 2 3\n' > "$build/input.txt"
validate "no group at all" 0 "*"
printf '3\n1 2 3\n7\n' > "$build/input.txt"
validate "trailing content" 3 "*expected the end of the input*" --group 1

check() {
    label=$1
    wanted_code=$2
    wanted=$3
    expect_run "checker: $label" "$wanted_code" "$wanted" env EOLYMP=1 TEST_COST=40 "$build/checker" \
        "$build/cin.txt" "$build/cout.txt" "$build/cans.txt"
}

printf '3\n' > "$build/cin.txt"
printf '10 20 30\n' > "$build/cans.txt"
printf '10 20 30\n' > "$build/cout.txt"
check "a correct answer" 0 "ok 3 of 3*"
printf '10 99 30\n' > "$build/cout.txt"
check "a partial answer" 7 "points 26.66666667*"
printf '99 99 99\n' > "$build/cout.txt"
check "a wrong answer" 1 "wrong answer none of the 3*"
printf '10 20\n' > "$build/cout.txt"
check "a short answer" 1 "wrong answer: output.txt*"
printf 'x\n' > "$build/cans.txt"
printf '10 20 30\n' > "$build/cout.txt"
check "a broken answer file" 3 "jury error: answer.txt*"

printf '3\n' > "$build/sin.txt"
printf '5 5 5\n' > "$build/sout.txt"
expect_run "a checker that swallowed an exception" 3 "*an exception left the checker before its verdict*" \
    env EOLYMP=1 TEST_COST=40 "$build/swallowing_checker" "$build/sin.txt" "$build/sout.txt" "$build/sin.txt" &&
    pass "an exception the checker swallowed is a jury error, not an accept"

printf '10 20 30\n' > "$build/cans.txt"
printf '10 99 30\n' > "$build/cout.txt"
check "a partial answer again" 7 "points 26.66666667*"
cp "$log" "$build/checker.log"
if command -v go >/dev/null 2>&1; then
    read_back=$(cd "$root/tests/e2e" && go run readpoints.go "$build/checker.log" 2>&1)
    case $read_back in
        26.6666*) pass "the judge's own parser reads $read_back out of a log with a trailing space and a blank line" ;;
        *) fail "the judge's own parser read \"$read_back\" from the log, expected 26.6666..." "$build/checker.log" ;;
    esac
else
    pass "readpoints skipped, go is not installed"
fi

printf '1000 723\n' > "$build/iin.txt"

play_it() {
    label=$1
    wanted=$2
    shift 2
    line=$(TEST_COST=40 "$build/play" "$build/interactor" "$build/iin.txt" "$build/isummary.txt" -- "$@" \
           2>"$build/interactor.log")
    got=$(echo "$line" | sed 's/.*interactor \([0-9-]*\).*/\1/')
    [ "$got" = "$wanted" ] || fail "the interactor exited $got against $label, expected $wanted" "$build/interactor.log"
}

play_it "a correct solution" 0 "$build/solution"
if [ -s "$build/isummary.txt" ]; then
    expect_run "the stock checker on the interactor's summary" 0 "ok*queries" env TEST_COST=40 \
        "$build/stock_checker" "$build/iin.txt" "$build/isummary.txt" "$build/iin.txt" &&
        pass "the stock checker reads the interactor's summary and says \"$(head -1 "$log")\""
else
    fail "the interactor wrote no summary" "$build/interactor.log"
fi

for mode in silent garbage outofrange wrongguess greedy deaf waiting; do
    play_it "a $mode solution" 1 "$build/hostile" "$mode"
done
pass "seven badly behaved solutions all got a wrong answer, never an interaction failure"

play_to_the_end() {
    label=$1
    jury=$2
    wanted=$3
    shift 3
    ended=$(TEST_COST=40 "$build/play" --wait "$jury" "$build/iin.txt" "$build/isummary.txt" -- "$@" \
            2>"$build/ended.log")
    case $ended in
        *"$wanted"*) return 0 ;;
    esac
    fail "$label ended \"$ended\", expected \"$wanted\"" "$build/ended.log"
}
play_to_the_end "a solution that keeps asking past its budget" "$build/interactor" \
    "interactor 1 solution signal 13" "$build/hostile" stubborn &&
    pass "a solution that keeps asking past its budget dies of SIGPIPE once the interactor has gone"

printf '123456789\n' > "$build/pin.txt"
expect_run "phase 1 of the phased interactor" 0 "*interactor 0 *" env TEST_COST=40 "$build/play" "$build/phased" \
    "$build/pin.txt" "$build/phandoff.txt" -- "$build/phased_solution"
expect_run "phase 2 of the phased interactor" 0 "*interactor 0 *" env TEST_COST=40 "$build/play" "$build/phased" \
    "$build/phandoff.txt" "$build/psummary.txt" -- "$build/phased_solution"
expect_run "the checker on the chained run" 0 "*" env TEST_COST=40 "$build/stock_checker" "$build/pin.txt" \
    "$build/psummary.txt" "$build/pin.txt" &&
    pass "two phases chained through the handoff, and the checker says \"$(head -1 "$log")\""

expect_run "the checker on a handoff where a summary belongs" 3 "*set run_count to 2*" env TEST_COST=40 \
    "$build/stock_checker" "$build/pin.txt" "$build/phandoff.txt" "$build/pin.txt" &&
    pass "a handoff where a summary belongs names the missing run_count"

generate() {
    into=$1
    shift
    "$build/generator" "$@" > "$build/$into"
}
expect_run "the generator" 0 "*" generate generated.txt -n=20 -max=1000
expect_run "the generator run again" 0 "*" generate generated_again.txt -n=20 -max=1000
if cmp -s "$build/generated.txt" "$build/generated_again.txt"; then
    pass "the generator gives the same bytes twice for the same arguments"
else
    fail "the generator gave two different tests for the same arguments"
fi
expect_run "the generator with -seed=2" 0 "*" generate generated_seeded.txt -n=20 -max=1000 -seed=2
if cmp -s "$build/generated.txt" "$build/generated_seeded.txt"; then
    fail "-seed did not change the test"
fi
expect_run "the validator on the generated test" 0 "*" "$build/validator" "$build/generated.txt" --group 1 &&
    pass "the validator accepts what the generator wrote"
generate generated_syntax.txt n=5 2> "$build/generated_syntax.err"
if grep -q "is not an option" "$build/generated_syntax.err" && [ ! -s "$build/generated_syntax.txt" ]; then
    pass "an argument that is not an option is refused on stderr, and the test stays empty"
else
    fail "an argument that is not an option is not refused on stderr alone"
fi
quick="quick_exit(0)"
[ "$(uname -s)" = Darwin ] && quick="exit(0) standing in for quick_exit(0), which macOS lacks,"
printf '1\ngarbage\n' > "$build/exits_in.txt"
printf '5\n' > "$build/exits_out.txt"
expect_run "a checker that calls exit(0) before its verdict" 3 "jury error the checker ended without a verdict*" \
    env ROLE=checker EOLYMP=1 TEST_COST=40 \
    "$build/exits" "$build/exits_in.txt" "$build/exits_out.txt" "$build/exits_out.txt" &&
    expect_run "a checker that calls quick_exit(0) before its verdict" 3 \
        "jury error the checker ended without a verdict*" env ROLE=quick EOLYMP=1 TEST_COST=40 \
        "$build/exits" "$build/exits_in.txt" "$build/exits_out.txt" "$build/exits_out.txt" &&
    expect_run "a checker never destroyed that returns 0" 3 "jury error the checker ended without a verdict*" \
        env ROLE=leaked EOLYMP=1 TEST_COST=40 \
        "$build/exits" "$build/exits_in.txt" "$build/exits_out.txt" "$build/exits_out.txt" &&
    expect_run "a validator that calls exit(0) half-way through its test" 3 "*expected the end of the input*" \
        env ROLE=validator "$build/exits" "$build/exits_in.txt" &&
    expect_run "an interactor that calls exit(0) before its verdict" 3 \
        "*jury error the interactor ended without a verdict*" env ROLE=interactor TEST_COST=40 \
        "$build/exits" "$build/exits_in.txt" "$build/exits_summary.txt" < /dev/null &&
    expect_run "a controller that calls exit(0) before its verdict" 3 \
        "jury error the controller ended without a verdict*" env ROLE=controller TEST_COST=40 \
        "$build/exits" "$build/exits_in.txt" "$build/exits_summary.txt" &&
    expect_run "a generator that calls exit(0) after a line" 0 "" env ROLE=generator \
        sh -c "\"$build/exits\" -n=7 > \"$build/exits_test.txt\"" &&
    if [ "$(cat "$build/exits_test.txt")" = "7" ]; then
        pass "exit(0), $quick and a leaked checker are jury errors, a validator's exit runs its end checks, and a generator's writes what it holds"
    else
        fail "a generator that called exit(0) after a line did not write it"
    fi
dies() {
    label=$1
    wanted_code=$2
    wanted=$3
    shift 3
    expect_death "$label" "$wanted_code" "$wanted" env "$@" EOLYMP=1 TEST_COST=40 \
        "$build/dies" "$build/exits_out.txt" "$build/exits_out.txt" "$build/exits_out.txt"
}
case "${CXX:-c++}" in
    *-fsanitize*)
        pass "the deaths of a checker and an interactor skipped, the sanitizers take those signals themselves" ;;
    *)
        dies "a checker ended by an exception nothing caught" 3 \
            "jury error an exception nothing caught ended the checker: vector::at: 7 >= 3
printed before the end
logged before the end
said on stderr
eolymp.h *" ROLE=throws &&
            dies "a checker ended by a thrown int" 3 \
                "jury error an exception nothing caught ended the checker, and it is not a std::exception*" \
                ROLE=throws_int &&
            dies "a checker that aborts" 134 "" ROLE=aborts &&
            dies "a checker killed by SIGSEGV" 139 "" ROLE=segfaults &&
            dies "a checker killed by SIGFPE" 136 "" ROLE=divides &&
            expect_death "an interactor ended by an exception nothing caught" 3 \
                "*jury error an exception nothing caught ended the interactor: the interactor lost count*" \
                env ROLE=interactor TEST_COST=40 "$build/dies" "$build/exits_in.txt" "$build/exits_summary.txt" &&
            pass "an exception nothing caught is a jury error that says what it was, in a log that keeps what the checker printed; a signal still leaves an empty log"
        ;;
esac

expect_run "the generator given an option it never declared" 3 "*unknown option -oops*" \
    generate generated_bad.txt -n=20 -oops=1 &&
    expect_run "a large generator given an option it never declared" 3 "*unknown option -oops*" \
        generate generated_bad_large.txt -n=200000 -oops=1 &&
    if [ -s "$build/generated_bad.txt" ] || [ -s "$build/generated_bad_large.txt" ]; then
        fail "a generator given an option it never declared wrote part of its test"
    else
        pass "an undeclared option stops the generator before it writes"
    fi
if [ -n "$fused" ]; then
    for built in ${fused#ok}; do
        drawn=$("$build/real_bits_$built")
        if [ "$drawn" != 14174797998470170970 ]; then
            fail "rng.real under $(cat "$build/real_bits_$built.by") with fused multiply-add drew $drawn"
            fused=
        fi
    done
    [ -n "$fused" ] && pass "rng.real draws the same bits with fused multiply-add allowed"
else
    pass "rng.real under fused multiply-add skipped, this CPU has none"
fi
if [ ! -w /dev/full ]; then
    pass "a generator writing to a full disk skipped, there is no /dev/full"
else
    if expect_run "a generator writing to a full disk" 3 "the test could not be written: *" \
        sh -c '"$0" -n=20 -max=1000 > /dev/full' "$build/generator"; then
        case $(cat "$log") in
            "the test could not be written: No space left on device"* | \
            "the test could not be written: an earlier write to stdout failed"*)
                pass "a generator whose test cannot be written fails" ;;
            *) fail "a generator writing to a full disk gave another reason" "$log" ;;
        esac
    fi
fi

shape() {
    into=$1
    shift
    "$build/shaper" "$@" > "$build/$into"
}
for kind in random path star caterpillar; do
    expect_run "the $kind generator" 0 "*" shape "shaped_$kind.txt" -n=400 "-shape=$kind" -maxw=1000
    expect_run "the $kind generator run again" 0 "*" shape shaped_again.txt -n=400 "-shape=$kind" -maxw=1000
    cmp -s "$build/shaped_$kind.txt" "$build/shaped_again.txt" ||
        fail "the $kind generator gave two different trees for the same arguments"
    expect_run "the validator on the $kind tree" 0 "*" "$build/shape_validator" "$build/shaped_$kind.txt" --group 1
done
pass "four tree shapes are trees, and each is the same bytes twice"

climbing=$(tail -n +2 "$build/shaped_path.txt" | awk '$1 < $2' | wc -l | tr -d ' ')
if [ "$climbing" = 399 ] || [ "$climbing" = 0 ]; then
    fail "presented left the edges in construction order"
fi
tail -n +2 "$build/shaped_path.txt" | cut -d" " -f1,2 > "$build/shaped_path_edges.txt"
tail -n +2 "$build/shaped_star.txt" | cut -d" " -f1,2 > "$build/shaped_star_edges.txt"
if cmp -s "$build/shaped_path_edges.txt" "$build/shaped_star_edges.txt"; then
    fail "two different shapes presented the same edges"
fi
pass "presented relabelled the path, turned edges and shuffled them"

if [ -n "$crossed" ]; then
    for compiler in gcc clang; do
        expect_run "the caterpillar generator built by $compiler" 0 "*" sh -c \
            '"$0" -n=400 -shape=caterpillar -maxw=1000 > "$1"' "$build/shaper_$compiler" "$build/shaped_$compiler.txt"
        expect_run "the shapes digest built by $compiler" 0 "*" sh -c '"$0" > "$1"' "$build/digest_$compiler" \
            "$build/digest_$compiler.txt"
    done
    if cmp -s "$build/shaped_gcc.txt" "$build/shaped_clang.txt"; then
        pass "the same tree comes out of g++ and clang++"
    else
        fail "g++ and clang++ built different trees from the same arguments"
    fi
    if cmp -s "$build/digest_gcc.txt" "$build/digest_clang.txt"; then
        pass "every sorted and sampled shape is the same under g++ and clang++"
    else
        fail "the shapes differ between g++ and clang++: $(cmp "$build/digest_gcc.txt" "$build/digest_clang.txt" 2>&1 |
             head -1)"
    fi
else
    pass "the cross-compiler checks need two different compilers; here g++ and clang++ are one"
fi

mkdir -p "$build/com"
printf '3 12345\n' > "$build/com/in.txt"

serve_it() {
    label=$1
    limit=$2
    wanted=$3
    controller=$4
    test=$5
    shift 5
    line=$(TEST_COST=40 "$build/serve" "$build/com" "$limit" "$controller" "$test" \
           "$build/com/summary.txt" -- "$@" 2>"$build/com/log.txt")
    got=$(echo "$line" | sed 's/controller \([0-9-]*\).*/\1/')
    [ "$got" = "$wanted" ] || fail "the controller exited $got against $label, expected $wanted" "$build/com/log.txt"
}

rm -f "$build/com/summary.txt"
serve_it "a correct relay" 10 0 "$build/relay" "$build/com/in.txt" "$build/relay_solution"
expect_run "the checker on the relay's summary" 0 "*" env TEST_COST=40 "$build/stock_checker" \
    "$build/com/in.txt" "$build/com/summary.txt" "$build/com/in.txt" &&
    pass "three instances relayed a secret over real pipes, and the checker says \"$(head -1 "$log")\""
serve_it "an instance beyond the limit" 2 3 "$build/relay" "$build/com/in.txt" "$build/relay_solution"
serve_it "an instance that says nothing" 10 1 "$build/relay" "$build/com/in.txt" "$build/relay_solution" mute
serve_it "an instance that lies" 10 1 "$build/relay" "$build/com/in.txt" "$build/relay_solution" liar
pass "the limit is a jury error, and a silent or lying instance is a wrong answer"

printf '100000\n' > "$build/com/bulk.txt"
serve_it "an instance that answers each of 100000 lines as it reads them" 1 0 "$build/bulk" \
    "$build/com/bulk.txt" "$build/bulk_solution" &&
    pass "a controller sent 100000 lines before reading and took in the answers meanwhile"

if [ "$failures" != 0 ]; then
    echo "e2e: $failures checks failed" >&2
    exit 1
fi
pass "exit codes and messages are what the judge would see"
