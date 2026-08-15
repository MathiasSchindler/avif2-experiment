#!/bin/sh
set -eu

encoder=$1
build_directory=$2
output="$build_directory/smoke.output"

"$encoder" --constant-128-8x8 "$output"
test "$(wc -c < "$output")" -eq 21
test "$(sha256sum "$output" | awk '{print $1}')" = \
    "4c13e82fce9975d573b92b8ca2da25e1fed90308ac6857ec4d2d92dba0268b11"

if "$encoder" "$output" >/dev/null 2>&1; then
    echo "smoke: invalid CLI unexpectedly succeeded" >&2
    exit 1
fi
