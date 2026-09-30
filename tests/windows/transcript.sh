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
program() {
    echo "$launch $bin/$1$suffix"
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
echo "== $count scenarios"
