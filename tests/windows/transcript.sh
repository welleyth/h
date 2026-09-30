#!/bin/sh
bin=$(cd "$1" && pwd)
case $(uname -s) in
    MINGW* | MSYS* | CYGWIN*) suffix=.exe ;;
    *) suffix= ;;
esac
suffix=${SUFFIX-$suffix}
launch=${LAUNCH:-}
work="$bin/work"
rm -rf "$work"
mkdir -p "$work"
cd "$work" || exit 1
count=0
run() {
    label=$1
    shift
    count=$((count + 1))
    "$@" > "raw-out.$count" 2> "raw-err.$count"
    code=$?
    for stream in out err; do
        sed 's#[^ "]*eolymp\(-shapes\)\{0,1\}\.h:#eolymp\1.h:#g' "raw-$stream.$count" > "$stream.$count"
    done
    printf '== %s\nexit %s\n-- stdout %s' "$label" "$code" "$(cksum < "out.$count")"
    printf '\n'
    cat -A "out.$count" | head -40
    printf -- '-- stderr %s' "$(cksum < "err.$count")"
    printf '\n'
    cat -A "err.$count" | head -40
}
path() {
    echo "$bin/$1$suffix"
}
program() {
    echo "$launch $(path "$1")"
}
validate() {
    label=$1
    shift
    run "validator: $label" $(program validator) in.txt "$@"
}
printf '3\n1 2 3\n' > in.txt
validate "a valid test" --group 1
printf '7\n1 2 3 4 5 6 7\n' > in.txt
validate "a test too big for its subtask" --group 0
printf '3\n1 2 2\n' > in.txt
validate "a repeated value" --group 1
printf '3\n1 2 3' > in.txt
validate "a missing line break" --group 1
printf '3\n1 2 3\n' > in.txt
validate "an unknown subtask" --group 9
printf '3\n1 2 3\n7\n' > in.txt
validate "trailing content" --group 1
printf '3\r\n1 2 3\r\n' > in.txt
validate "CRLF line breaks" --group 1
EOLYMP=1 validate "CRLF line breaks on the judge" --group 1
printf '3\n1 2\r3\n' > in.txt
validate "a lone CR" --group 1
printf '3\n1 2 3\n\032\n' > in.txt
validate "a Ctrl-Z after the test" --group 1
printf '3\n1 2\0323\n' > in.txt
validate "a Ctrl-Z inside a line" --group 1
printf '3\n1 2 3\n\032' > in.txt
validate "a Ctrl-Z as the last byte" --group 1
printf '3\r\n1 2 3\r\n' > in.txt
run "validator: CRLF on stdin" sh -c "$(program validator) --group 1 < in.txt"
printf '3\n1 2 3\n\032\n' > in.txt
run "validator: a Ctrl-Z on stdin" sh -c "$(program validator) --group 1 < in.txt"
for test in "LF:5\\n" "CRLF:5\\r\\n" "a Ctrl-Z after the test:5\\n\\032\\n" "CRLF, then a Ctrl-Z:5\\r\\n\\032"; do
    printf "${test#*:}" > fin.txt
    run "validator: freopen on stdin, ${test%%:*}" env FREOPEN_IN=fin.txt $(program freopen_validator)
done
printf '3\r\n1 2 3\r\n' > in.txt
run "validator: CRLF through a pipe" sh -c "cat in.txt | $(program validator) --group 1"
printf '3\n1 2 3\n7\n' > in.txt
run "validator: trailing content through a pipe" sh -c "cat in.txt | $(program validator) --group 1"

check() {
    label=$1
    shift
    run "checker: $label" env EOLYMP=1 TEST_COST=40 "$@" $(program checker) cin.txt cout.txt cans.txt
}
printf '3\n' > cin.txt
printf '10 20 30\n' > cans.txt
printf '10 20 30\n' > cout.txt
check "a correct answer"
printf '10 99 30\n' > cout.txt
check "a partial answer"
check "a partial answer with no TEMP or TMP" TEMP= TMP=
printf '99 99 99\n' > cout.txt
check "a wrong answer"
printf '10 20\n' > cout.txt
check "a short answer"
printf '10 20 30\r\n' > cout.txt
check "an answer with CRLF"
printf '10 20 30\n\032' > cout.txt
check "an answer with a Ctrl-Z after it"
printf '10 20\03230\n' > cout.txt
check "an answer with a Ctrl-Z inside it"
printf 'x\n' > cans.txt
printf '10 20 30\n' > cout.txt
check "a broken answer file"
printf '10 20 30\n' > cans.txt
printf '10 99 30\n' > cout.txt
run "checker: a partial answer, not on the judge" env TEST_COST=40 $(program checker) cin.txt cout.txt cans.txt
printf '3\n' > sin.txt
printf '5 5 5\n' > sout.txt
run "a checker that swallows an exception" env EOLYMP=1 TEST_COST=40 $(program swallowing_checker) sin.txt sout.txt \
    sin.txt
raw() {
    label=$1
    printf "$2" > rout.txt
    printf 'x\n' > rans.txt
    run "raw checker: $label" env EOLYMP=1 $(program raw_checker) cin.txt rout.txt rans.txt
}
raw "CRLF" 'abc\r\n'
raw "a lone CR" 'a\rb\n'
raw "a Ctrl-Z" 'ab\032cd\n'
raw "a Ctrl-Z, then more lines" 'ab\032\ncd\n'
raw "a NUL" 'a\000b\n'
lines() {
    label=$1
    printf "$2" > lout.txt
    printf "$3" > lans.txt
    run "lines(exact): $label" env EOLYMP=1 $(program lines_checker) cin.txt lout.txt lans.txt
}
lines "the same" 'a b\nc\n' 'a b\nc\n'
lines "CRLF output" 'a b\r\nc\r\n' 'a b\nc\n'
lines "CRLF answer" 'a b\nc\n' 'a b\r\nc\r\n'
lines "a lone CR" 'a\rb\nc\n' 'a b\nc\n'
lines "a Ctrl-Z, then more" 'a b\n\032c\n' 'a b\n'
tokens() {
    label=$1
    printf "$2" > tout.txt
    printf "$3" > tans.txt
    run "tokens(): $label" env EOLYMP=1 $(program tokens_checker) cin.txt tout.txt tans.txt
}
tokens "CRLF" 'a\r\nb\r\n' 'a\nb\n'
tokens "a Ctrl-Z, then more" 'a\n\032b\n' 'a\n'

generate() {
    label=$1
    shift
    run "generator: $label" sh -c "$(program generator) $* > gen.txt; code=\$?; cat -A gen.txt | head -8; exit \$code"
}
generate "plain" -n=20 -max=1000
generate "with a seed" -n=20 -max=1000 -seed=2
generate "sorted" -n=20 -max=1000 -shape=sorted
generate "an unknown option" -n=20 -oops=1
generate "an argument that is not an option" n=5
run "generator: freopen on stdout" sh -c "FREOPEN_OUT=fg.txt $(program freopen_generator) -n=3; code=\$?; cat -A fg.txt; exit \$code"
run "generator: through a pipe" sh -c "$(program generator) -n=5 -max=9 | cat -A"
run "generator: --eo-describe" $(program generator) --eo-describe
run "generator: 200000 values" sh -c "$(program generator) -n=200000 > big.txt; code=\$?; cksum < big.txt; exit \$code"
run "generator: a write behind g.out" sh -c "$(program leaky_generator) -n=5 > leak.txt; code=\$?; cat -A leak.txt; exit \$code"
run "shaper: a caterpillar" sh -c "$(program shaper) -n=400 -shape=caterpillar -maxw=1000 | cksum"
run "shaper: a star" sh -c "$(program shaper) -n=300 -shape=star -maxw=50 | cksum"
run "shape validator on a random tree" sh -c "$(program shaper) -n=50 > tree.txt; $(program shape_validator) tree.txt --group 1"
run "shapes digest" sh -c "$(program shapes_digest) | cksum"
run "rng.real bits" $(program real_bits)
run "reals written and read" $(program reals)
run "checked arithmetic" sh -c "$(program arithmetic) | cksum"
run "exit codes: accept" $(program exit_codes) accept
run "exit codes: invalid" $(program exit_codes) invalid
run "exit codes: library" $(program exit_codes) library
run "exit codes: anything" $(program exit_codes) anything
printf '1\ngarbage\n' > ein.txt
printf '5\n' > eout.txt
run "a checker that calls exit(0)" env ROLE=checker EOLYMP=1 TEST_COST=40 $(program exits) ein.txt eout.txt eout.txt
run "a checker that calls quick_exit(0)" env ROLE=quick EOLYMP=1 TEST_COST=40 $(program exits) ein.txt eout.txt \
    eout.txt
run "a checker never destroyed" env ROLE=leaked EOLYMP=1 TEST_COST=40 $(program exits) ein.txt eout.txt eout.txt
run "a validator that calls exit(0)" env ROLE=validator $(program exits) ein.txt
run "a generator that calls exit(0)" sh -c "ROLE=generator $(program exits) -n=7 > et.txt; code=\$?; cat -A et.txt; exit \$code"
run "an interactor that calls exit(0)" sh -c "ROLE=interactor TEST_COST=40 $(program exits) ein.txt esummary.txt < /dev/null"

printf '1000 723\n' > iin.txt
dialogue() {
    label=$1
    shift
    run "$label" sh -c "TEST_COST=40 timeout 300 $(program play) $* | sed 's/ solution .*//'"
}
play() {
    label=$1
    shift
    dialogue "interactor: $label" "$(path interactor) iin.txt isummary.txt -- $*"
}
play "a correct solution" "$(path solution)"
run "interactor: the summary it wrote" cat isummary.txt
run "the stock checker on that summary" env TEST_COST=40 $(program stock_checker) iin.txt isummary.txt iin.txt
for mode in silent garbage outofrange wrongguess greedy deaf waiting; do
    play "a $mode solution" "$(path hostile) $mode"
done
dialogue "interactor: a solution that keeps asking past its budget" \
    "--wait $(path interactor) iin.txt isummary.txt -- $(path hostile) stubborn"
printf '123456789\n' > pin.txt
dialogue "phase 1" "$(path phased) pin.txt phandoff.txt -- $(path phased_solution)"
run "phase 1: the handoff it wrote" cat phandoff.txt
dialogue "phase 2" "$(path phased) phandoff.txt psummary.txt -- $(path phased_solution)"
run "the stock checker on the chained run" env TEST_COST=40 $(program stock_checker) pin.txt psummary.txt pin.txt
run "the stock checker on a handoff where a summary belongs" env TEST_COST=40 $(program stock_checker) pin.txt \
    phandoff.txt pin.txt
for lines in 10 5000 100000 1000000; do
    printf '%s\n' "$lines" > bulk.txt
    dialogue "interactor: $lines lines sent before the first answer is read" \
        "$(path bulk_interactor) bulk.txt bsummary.txt -- $(path bulk_solution)"
    run "interactor: the summary of $lines lines" cat bsummary.txt
done
talk() {
    label=$1
    printf '%s\n' "$2" > talk.txt
    shift 2
    dialogue "interactor: $label" "$(path dialogue_interactor) talk.txt tsummary.txt -- $(path dialogue_solution) $*"
}
talk "a flood of 16 MB and 2 bytes during a 1 MB send (EO409)" "1 0" flood 2
talk "a solution that ends after reading 100 of 300000 lines" "2 300000" early
talk "a solution that answers and ends after reading 100 of 300000 lines" "2 300000" early_answer
talk "a solution that closes its input and answers" "2 300000" closer
talk "a slow solution, 50 rounds of 20 ms" "3 50" slow
talk "last words of 60000 bytes to a solution that sleeps 4 s" "4 0" sleeper
run "interactor: last words to a solution that never reads, given up within 4 whole seconds" sh -c "
    printf '4 0\n' > talk.txt
    started=\$(date +%s)
    TEST_COST=40 timeout 300 $(program play) $(path dialogue_interactor) talk.txt tsummary.txt -- \
        $(path dialogue_solution) sleeper_forever | sed 's/ solution .*//'
    [ \$((\$(date +%s) - started)) -le 4 ] && echo 'within 4 s' || echo 'too slow'"
talk "a solution still sending 1 MB when the interactor accepts (EO404)" "7 0" chatter
talk "100000 round trips" "5 100000" echo
printf '3\n' > cin.txt
printf '10 20 30\n' > cans.txt
printf '10 99 30\n' > cout.txt
mkdir -p "tëst temp"
native() {
    if command -v cygpath > /dev/null; then cygpath -w "$1"; else echo "$1"; fi
}
check "a partial answer with TEMP in a folder that does not exist" TEMP="$(native "$PWD/no such folder")" \
    TMP="$(native "$PWD/no such folder")"
check "a partial answer with TEMP in a folder with a space and a letter of the ANSI code page" \
    TEMP="$(native "$PWD/tëst temp")" TMP="$(native "$PWD/tëst temp")"
run "the held output leaves no scratch file behind" sh -c "find . -name 'eolymp-checker-output-*' | wc -l"
mkdir -p com
printf '3 12345\n' > com/in.txt
serve() {
    label=$1
    limit=$2
    shift 2
    run "controller: $label" sh -c "TEST_COST=40 timeout 120 $(program serve) com $limit $*"
}
serve "a correct relay" 10 "$(path relay) com/in.txt com/summary.txt -- $(path relay_solution)"
run "controller: the summary it wrote" cat com/summary.txt
run "the stock checker on the relay's summary" env TEST_COST=40 $(program stock_checker) com/in.txt com/summary.txt \
    com/in.txt
serve "an instance beyond the limit" 2 "$(path relay) com/in.txt com/summary.txt -- $(path relay_solution)"
serve "an instance that says nothing" 10 "$(path relay) com/in.txt com/summary.txt -- $(path relay_solution) mute"
serve "an instance that lies" 10 "$(path relay) com/in.txt com/summary.txt -- $(path relay_solution) liar"
for lines in 1000 100000; do
    printf '%s\n' "$lines" > com/bulk.txt
    serve "$lines lines sent to an instance that answers each as it reads it" 1 \
        "$(path bulk) com/bulk.txt com/summary.txt -- $(path bulk_solution)"
done
run "a controller that calls exit(0)" env ROLE=controller TEST_COST=40 $(program exits) ein.txt esummary.txt
echo "== $count scenarios"
