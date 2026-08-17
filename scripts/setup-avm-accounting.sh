#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vendor_dir="$root/vendor/avm"
avm_build_dir="$root/build/reference/avm-accounting"
census_build_dir="$root/build/reference/census-accounting"
revision=966a7d7cd6fcf60360caf5dc413b2aeeb65e144d

for tool in git cmake ninja make clang; do
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
if test -n "$(git -C "$vendor_dir" status --short)"; then
    echo "error: vendor/avm has local modifications" >&2
    exit 1
fi

cmake -S "$vendor_dir" -B "$avm_build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=OFF \
    -DENABLE_EXAMPLES=OFF \
    -DENABLE_TESTS=OFF \
    -DCONFIG_ACCOUNTING=1 \
    -DCONFIG_DIP_EXT_PRUNING=0 \
    -DCONFIG_INSPECTION=1 \
    -DCONFIG_ML_PART_SPLIT=0 \
    -DCONFIG_TENSORFLOW_LITE=0
cmake --build "$avm_build_dir" --target avm --parallel

make -C "$root" census \
    AVM_SOURCE_DIR="$vendor_dir" \
    AVM_BUILD_DIR="$avm_build_dir" \
    CENSUS_BUILD_DIR="$census_build_dir"

echo "Accounting census is ready at $census_build_dir/av2-census"