#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vendor_dir="$root/vendor/avm"
build_dir="$root/build/reference/avm"
revision=966a7d7cd6fcf60360caf5dc413b2aeeb65e144d
source_url=https://github.com/AOMediaCodec/avm.git

if ! command -v git >/dev/null 2>&1; then
    echo "error: git is required" >&2
    exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake is required" >&2
    exit 1
fi
if ! command -v ninja >/dev/null 2>&1; then
    echo "error: ninja is required" >&2
    exit 1
fi

if test ! -d "$vendor_dir/.git"; then
    git clone --no-checkout "$source_url" "$vendor_dir"
    git -C "$vendor_dir" checkout --detach "$revision"
fi

actual_revision=$(git -C "$vendor_dir" rev-parse HEAD)
if test "$actual_revision" != "$revision"; then
    echo "error: vendor/avm is at $actual_revision, expected $revision" >&2
    exit 1
fi
if test -n "$(git -C "$vendor_dir" status --short)"; then
    echo "error: vendor/avm has local modifications" >&2
    exit 1
fi

cmake -S "$vendor_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=OFF \
    -DENABLE_EXAMPLES=ON \
    -DENABLE_TESTS=OFF \
    -DCONFIG_TENSORFLOW_LITE=0
cmake --build "$build_dir" --target avmenc avmdec --parallel

echo "AVM reference tools are ready in $build_dir"