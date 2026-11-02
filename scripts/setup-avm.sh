#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
vendor_dir="$root/vendor/avm"
build_dir="$root/build/reference/avm"
revision=966a7d7cd6fcf60360caf5dc413b2aeeb65e144d
source_url=https://github.com/AOMediaCodec/avm.git

current=0
case "${1:-}" in
    "") ;;
    --current)
        current=1
        vendor_dir="$root/vendor/avm-current"
        build_dir="$root/build/reference/avm-current"
        ;;
    *)
        echo "usage: $0 [--current]" >&2
        exit 1
        ;;
esac
if test "$#" -gt 1; then
    echo "usage: $0 [--current]" >&2
    exit 1
fi

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
    if test "$current" -eq 1; then
        git clone --depth 1 --branch main "$source_url" "$vendor_dir"
    else
        git clone --no-checkout "$source_url" "$vendor_dir"
        git -C "$vendor_dir" checkout --detach "$revision"
    fi
fi

if test -n "$(git -C "$vendor_dir" status --short)"; then
    echo "error: $vendor_dir has local modifications" >&2
    exit 1
fi
if test "$current" -eq 1; then
    git -C "$vendor_dir" fetch --depth 1 "$source_url" main
    git -C "$vendor_dir" checkout --detach FETCH_HEAD
    revision=$(git -C "$vendor_dir" rev-parse HEAD)
fi
actual_revision=$(git -C "$vendor_dir" rev-parse HEAD)
if test "$actual_revision" != "$revision"; then
    echo "error: $vendor_dir is at $actual_revision, expected $revision" >&2
    exit 1
fi

cmake -S "$vendor_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=OFF \
    -DENABLE_EXAMPLES=ON \
    -DENABLE_TESTS=OFF \
    -DCONFIG_INSPECTION=1 \
    -DCONFIG_TENSORFLOW_LITE=0
if test "$current" -eq 1; then
    cmake --build "$build_dir" --target avmenc avmdec --parallel "${AVM_JOBS:-4}"
    printf '%s\n' "$revision" > "$build_dir/oracle-revision.txt"
else
    cmake --build "$build_dir" --target avmenc avmdec --parallel
fi

echo "AVM reference tools are ready in $build_dir"