#!/bin/sh
set -eu

encoder=$1
build_directory=$2
directory="$build_directory/atomic-output"
input="$directory/input.i400"
output="$directory/output.obu"
error="$directory/error.stderr"

rm -rf "$directory"
mkdir -p "$directory"

expect_output_failure()
{
    set +e
    (trap '' 25; ulimit -f 0; exec "$@") >/dev/null 2>"$error"
    actual=$?
    set -e
    if test "$actual" -ne 7; then
        echo "atomic output: expected status 7, got $actual: $*" >&2
        cat "$error" >&2
        exit 1
    fi
}

assert_preserved_and_clean()
{
    test "$(cat "$output")" = sentinel
    if find "$directory" -maxdepth 1 -name 'output.obu.tmp.*' | grep -q .; then
        echo "atomic output: temporary output was not cleaned up" >&2
        exit 1
    fi
}

printf sentinel >"$output"
expect_output_failure "$encoder" --constant-128-8x8 "$output"
assert_preserved_and_clean

dd if=/dev/zero of="$input" bs=64 count=1 2>/dev/null
expect_output_failure "$encoder" --i400-8 "$input" --width 8 --height 8 \
    "$output"
assert_preserved_and_clean

set +e
sh -c '
    index=0
    while test "$index" -lt 128; do
        printf collision >"$3.tmp.$$.$index"
        index=$((index + 1))
    done
    exec "$1" --constant-128-8x8 "$3"
' sh "$encoder" unused "$output" >/dev/null 2>"$error"
actual=$?
set -e
test "$actual" -eq 7
test "$(cat "$output")" = sentinel
test "$(find "$directory" -maxdepth 1 -name 'output.obu.tmp.*' |
    wc -l)" -eq 128
rm -f "$output".tmp.*

mkdir "$directory/destination-directory"
set +e
"$encoder" --constant-128-8x8 "$directory/destination-directory" \
    >/dev/null 2>"$error"
actual=$?
set -e
test "$actual" -eq 7
test -d "$directory/destination-directory"
if find "$directory" -maxdepth 1 \
    -name 'destination-directory.tmp.*' | grep -q .; then
    echo "atomic output: rename-failure temporary was not cleaned up" >&2
    exit 1
fi

if test "$(id -u)" -ne 0; then
    mkdir "$directory/read-only"
    printf sentinel >"$directory/read-only/output.obu"
    chmod 0555 "$directory/read-only"
    set +e
    "$encoder" --constant-128-8x8 "$directory/read-only/output.obu" \
        >/dev/null 2>"$error"
    actual=$?
    set -e
    test "$actual" -eq 7
    test "$(cat "$directory/read-only/output.obu")" = sentinel
    if find "$directory/read-only" -maxdepth 1 \
        -name 'output.obu.tmp.*' | grep -q .; then
        echo "atomic output: read-only temporary was not cleaned up" >&2
        exit 1
    fi
    chmod 0755 "$directory/read-only"
fi

echo "atomic output: PASS"
