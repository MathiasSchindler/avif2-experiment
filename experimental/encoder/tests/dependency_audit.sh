#!/bin/sh
set -eu

encoder_directory=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repository_root=$(CDPATH= cd -- "$encoder_directory/../.." && pwd)

fail()
{
    printf '%s\n' "dependency audit: $*" >&2
    exit 1
}

if find "$encoder_directory" -type l -print | grep -q .; then
    fail "symbolic links are not allowed in the isolated encoder tree"
fi

implementation_files=$(find \
    "$encoder_directory/include" \
    "$encoder_directory/src" \
    "$encoder_directory/platform" \
    -type f -print)

if grep -vE 'AVMDEC|PROJECT_DECODER|check-validators' \
    "$encoder_directory/Makefile" |
    grep -nEi \
        '(^|[^[:alnum:]_])(decoder|av2heif|src/decoder)([^[:alnum:]_]|$)' ||
    grep -nEi \
        '(^|[^[:alnum:]_])(decoder|av2heif|src/decoder)([^[:alnum:]_]|$)' \
        $implementation_files; then
    fail "decoder source, header, target, or library reference found"
fi

if grep -vE 'AVMDEC|PROJECT_DECODER' "$encoder_directory/Makefile" |
    grep -nE '(^|[[:space:]=])\.\./|-[IL][[:space:]]*\.\.'; then
    fail "source, include, or link path escapes experimental/encoder"
fi

if grep -nE \
    '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]\.\./' \
    $implementation_files; then
    fail "implementation include path escapes experimental/encoder"
fi

core_files=$(find "$encoder_directory/src" -type f -name '*.c' -print)
if grep -nE \
    '(^|[^[:alnum:]_])(mmap|munmap|malloc|calloc|realloc|free)[[:space:]]*\(' \
    $core_files; then
    fail "codec core contains allocation or mapping calls"
fi
if grep -nE \
    '\[(ENC_M3_MAX_PIXELS|ENC_M3_MAX_BLOCKS)\]' \
    "$encoder_directory/src/m3.c" "$encoder_directory/src/main.c"; then
    fail "M3 geometry storage regressed to fixed stack arrays"
fi

for build_file in "$repository_root/Makefile" "$repository_root/CMakeLists.txt"; do
    if grep -nEi 'experimental/encoder|encoder[-_]experiment|av2[-_]?encoder' \
        "$build_file"; then
        fail "top-level decoder build references the encoder experiment"
    fi
done

if git -C "$repository_root" rev-parse --verify HEAD >/dev/null 2>&1; then
    if ! git -C "$repository_root" diff --quiet HEAD -- Makefile CMakeLists.txt; then
        fail "top-level decoder build files have uncommitted modifications"
    fi
fi

printf '%s\n' "dependency audit: PASS"
