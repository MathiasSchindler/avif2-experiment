#!/bin/sh
set -eu

encoder=$1
build_directory=$2
directory="$build_directory/cli-contract"
error="$directory/error.stderr"

rm -rf "$directory"
mkdir -p "$directory"

expect_status()
{
    expected=$1
    shift
    set +e
    "$@" >/dev/null 2>"$error"
    actual=$?
    set -e
    if test "$actual" -ne "$expected"; then
        echo "cli contract: expected status $expected, got $actual: $*" >&2
        cat "$error" >&2
        exit 1
    fi
}

dd if=/dev/zero of="$directory/source.part-8x8.i400" \
    bs=64 count=1 2>/dev/null
"$encoder" "$directory/source.part-8x8.i400" >/dev/null 2>"$error"
test -f "$directory/source.part-8x8.obu"
test ! -e "$directory/source.part-8x8.i400.obu"

"$encoder" --i400-8 "$directory/source.part-8x8.i400" \
    --width 8 --height 8 "$directory/legacy.obu" >/dev/null 2>"$error"
cmp "$directory/source.part-8x8.obu" "$directory/legacy.obu"

"$encoder" --qindex 100 -o "$directory/override.obu" \
    "$directory/source.part-8x8.i400" >/dev/null 2>"$error"
cmp "$directory/source.part-8x8.obu" "$directory/override.obu"

dd if=/dev/zero of="$directory/plain.i400" bs=64 count=1 2>/dev/null
"$encoder" "$directory/plain.i400" --width 8 --height 8 \
    --output "$directory/explicit.obu" >/dev/null 2>"$error"
cmp "$directory/source.part-8x8.obu" "$directory/explicit.obu"

mkdir "$directory/manifest"
dd if=/dev/zero of="$directory/manifest/plain.i400" bs=64 count=1 2>/dev/null
cat >"$directory/manifest/manifest.json" <<'EOF'
{
  "schema_version": 1,
  "dimensions": [8, 8],
  "outputs": {
    "gray8": {
      "filename": "plain.i400",
      "byte_size": 64
    }
  }
}
EOF
"$encoder" "$directory/manifest/plain.i400" >/dev/null 2>"$error"
test -f "$directory/manifest/plain.obu"

dd if=/dev/zero of="$directory/missing.i400" bs=64 count=1 2>/dev/null
expect_status 3 "$encoder" "$directory/missing.i400"
grep -q 'raw geometry missing' "$error"
test ! -e "$directory/missing.obu"

dd if=/dev/zero of="$directory/a-8x8-b-4x16.i400" \
    bs=64 count=1 2>/dev/null
expect_status 3 "$encoder" "$directory/a-8x8-b-4x16.i400"
grep -q 'geometry is ambiguous' "$error"

mkdir "$directory/conflict"
dd if=/dev/zero of="$directory/conflict/source-8x8.i400" \
    bs=64 count=1 2>/dev/null
cat >"$directory/conflict/manifest.json" <<'EOF'
{
  "dimensions": [4, 16],
  "outputs": {"gray8": {"filename": "source-8x8.i400"}}
}
EOF
expect_status 3 "$encoder" "$directory/conflict/source-8x8.i400"
grep -q 'geometry conflict' "$error"

dd if=/dev/zero of="$directory/color-8x8.i444" bs=192 count=1 2>/dev/null
"$encoder" "$directory/color-8x8.i444" >/dev/null 2>"$error"
test -f "$directory/color-8x8.obu"

cp fixtures/image-input/solid-rgba.png "$directory/color.png"
"$encoder" "$directory/color.png" >/dev/null 2>"$error"
test -f "$directory/color.obu"

cp fixtures/image-input/solid-color.bmp "$directory/color.bmp"
"$encoder" "$directory/color.bmp" >/dev/null 2>"$error"
test -f "$directory/color.obu"

cp fixtures/image-input/unsupported-depth.png "$directory/unsupported.png"
expect_status 5 "$encoder" "$directory/unsupported.png"
grep -q 'unsupported BMP/PNG subset' "$error"
test ! -e "$directory/unsupported.obu"

printf '\211PNG\r\n\032\n' >"$directory/signature-only.data"
expect_status 4 "$encoder" "$directory/signature-only.data"
grep -q 'malformed BMP/PNG input' "$error"
test ! -e "$directory/signature-only.data.obu"

printf 'not a png' >"$directory/bad.png"
expect_status 3 "$encoder" "$directory/bad.png"
grep -q 'does not match its extension' "$error"

printf unknown >"$directory/unknown.dat"
expect_status 3 "$encoder" "$directory/unknown.dat"
grep -q 'unknown input format' "$error"

cp "$directory/source.part-8x8.i400" "$directory/collision-8x8.i400"
expect_status 3 "$encoder" "$directory/collision-8x8.i400" \
    --output "$directory/collision-8x8.i400"
test "$(wc -c <"$directory/collision-8x8.i400")" -eq 64
grep -q 'must not be the input path' "$error"

ln "$directory/collision-8x8.i400" "$directory/collision.obu"
expect_status 3 "$encoder" "$directory/collision-8x8.i400" \
    --output "$directory/collision.obu"
test "$(wc -c <"$directory/collision-8x8.i400")" -eq 64
grep -q 'resolves to the input file' "$error"

dd if=/dev/zero of="$directory/m4.i400" bs=64 count=1 2>/dev/null
expect_status 3 "$encoder" --experimental-m4-ac \
    "$directory/m4.i400" "$directory/m4.i400"
test "$(wc -c <"$directory/m4.i400")" -eq 64

echo "cli contract: PASS"
