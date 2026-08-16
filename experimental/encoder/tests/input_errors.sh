#!/bin/sh
set -eu

encoder=$1
build_directory=$2
input="$build_directory/input-errors.i400"
output="$build_directory/input-errors.obu"
error="$build_directory/input-errors.stderr"

expect_status()
{
    expected=$1
    shift
    set +e
    "$@" >/dev/null 2>"$error"
    actual=$?
    set -e
    if test "$actual" -ne "$expected"; then
        echo "input errors: expected status $expected, got $actual: $*" >&2
        cat "$error" >&2
        exit 1
    fi
}

dd if=/dev/zero of="$input" bs=1441 count=1 2>/dev/null
rm -f "$output"
expect_status 5 "$encoder" --i400-8 "$input" --width 1441 --height 1 "$output"
test ! -e "$output"
grep -q 'supports 1..1440 pixels per axis' "$error"

printf 'sentinel' >"$output"
dd if=/dev/zero of="$input" bs=1442 count=1 2>/dev/null
expect_status 5 "$encoder" --i400-8 "$input" --width 1441 --height 1 \
    --stride 1442 "$output"
test "$(cat "$output")" = sentinel

printf '\001\002\003\004\005' >"$input"
expect_status 4 "$encoder" --i400-8 "$input" --width 3 --height 2 "$output"
grep -q 'file size' "$error"
test "$(cat "$output")" = sentinel

printf '\001\002\003\004\005\006\007' >"$input"
expect_status 4 "$encoder" --i400-8 "$input" --width 3 --height 2 "$output"
grep -q 'file size' "$error"

expect_status 3 "$encoder" --i400-8 "$input" --width 3 --height 2 \
    --stride 2 "$output"
expect_status 3 "$encoder" --i400-8 "$input" --width 0 --height 2 "$output"
expect_status 3 "$encoder" --i400-8 "$input" --width 16385 --height 2 "$output"
expect_status 3 "$encoder" --i400-8 "$input" --width nope --height 2 "$output"
expect_status 3 "$encoder" --i400-8 "$input" --width 16384 --height 16384 \
    --stride 16385 "$output"
expect_status 3 "$encoder" --i400-8 "$input" --width 3 --height 2 \
    --qindex 0 "$output"
expect_status 3 "$encoder" --i400-8 "$input" --width 3 --height 2 \
    --qindex 256 "$output"
expect_status 4 "$encoder" --i400-8 "$build_directory/missing.i400" \
    --width 1 --height 1 "$output"
expect_status 2 "$encoder" --i400-8 "$input" --height 2 --width 3 "$output"

dd if=/dev/zero of="$input" bs=63 count=1 2>/dev/null
printf sentinel >"$output"
expect_status 4 "$encoder" --experimental-m4-ac "$input" "$output"
grep -q 'exactly 64 bytes' "$error"
test "$(cat "$output")" = sentinel

test -f tests/i400_prepared_reference.txt
grep -q '^path=images/prepared/' tests/i400_prepared_reference.txt
if find tests -type f \( -name '*.i400' -o -name '*.gray8' \) | grep -q .; then
    echo "input errors: photographic raw data must not be tracked in tests" >&2
    exit 1
fi

echo "input errors: PASS"
