#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vendor_dir="$root/vendor/avm"
source_dir="$root/build/reference/avm-negative-source"
build_dir="$root/build/reference/avm-negative"
revision=966a7d7cd6fcf60360caf5dc413b2aeeb65e144d

for tool in git cmake ninja tar; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool is required" >&2
        exit 1
    fi
done
if test ! -d "$vendor_dir/.git"; then
    echo "error: vendor/avm is absent; run scripts/setup-avm.sh first" >&2
    exit 1
fi
actual_revision=$(git -C "$vendor_dir" rev-parse HEAD)
if test "$actual_revision" != "$revision"; then
    echo "error: vendor/avm is at $actual_revision, expected $revision" >&2
    exit 1
fi

cmake -E remove_directory "$source_dir"
cmake -E make_directory "$source_dir"
git -C "$vendor_dir" archive --format=tar "$revision" | tar -xf - -C "$source_dir"
printf '%s\n' "$revision" > "$source_dir/.av2heif-revision"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=OFF \
    -DENABLE_EXAMPLES=ON \
    -DENABLE_TESTS=OFF \
    -DCONFIG_TESTONLY_12BIT_SUPPORT=1
cmake --build "$build_dir" --target avmenc avmdec --parallel

echo "AVM negative-fixture tools from $revision are ready in $build_dir"