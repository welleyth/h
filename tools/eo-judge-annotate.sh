#!/bin/sh
set -eu

if [ $# -ne 1 ]; then
    echo "usage: eo-judge-annotate.sh <problem> < report.json" >&2
    exit 2
fi
problem=${1#./}
report=$(cat)

printf '%s\n' "$report" | jq -r '
    (.attempts[] |
        "\(.name): \(.verdict), \(.score)",
        (.groups[] | "  testset \(.index) \(.verdict) \(.score) of \(.cost)"),
        (select(.breaks) | "  it is declared \(.type), but \(.breaks)")),
    (.findings[] | (if .where != "" then .where + ": " else "" end) + "\(.level) \(.code): \(.message)\n  \(.fix)"),
    (select(.error) | "eo-judge: \(.error)")'

separator=$(printf '\037')
printf '%s\n' "$report" | jq -r '
    def data: gsub("%"; "%25") | gsub("\r"; "%0D") | gsub("\n"; "%0A");
    def property: data | gsub(":"; "%3A") | gsub(","; "%2C");
    (.findings[] | [
        (if .level == "warning" then "warning" else "notice" end), .where, (.code | property),
        ((if .where != "" then .where + ": " else "" end) + .message + " (" + .fix + ")" | data)]),
    (.attempts[] | select(.breaks) | ["error", "", ("solution " + .name | property),
        ("solution " + .name + " is declared " + .type + ", but " + .breaks | data)]),
    (select(.error) | ["error", "", "eo-judge", (.error | data)])
    | join("\u001f")' |
    while IFS=$separator read -r level where title text; do
        place="file=$problem/problem.json"
        file=${where%:*}
        line=${where##*:}
        case $line in
            '' | *[!0-9]*) ;;
            *) if [ "$file" != "$where" ] && [ -f "$problem/$file" ]; then place="file=$problem/$file,line=$line"; fi ;;
        esac
        echo "::$level $place,title=$title::$text"
    done
