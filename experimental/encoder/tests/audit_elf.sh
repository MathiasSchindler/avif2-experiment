#!/bin/sh
set -eu

binary=$1
readelf_tool=${READELF:-readelf}

if "$readelf_tool" -lW "$binary" | grep -q 'INTERP'; then
    echo "ELF audit: interpreter found" >&2
    exit 1
fi
if "$readelf_tool" -lW "$binary" | grep -q 'DYNAMIC'; then
    echo "ELF audit: dynamic segment found" >&2
    exit 1
fi
if "$readelf_tool" -dW "$binary" 2>&1 | grep -q 'NEEDED'; then
    echo "ELF audit: dynamic dependency found" >&2
    exit 1
fi
if "$readelf_tool" -Ws "$binary" |
    awk '$7 == "UND" && $8 != "" { found = 1 } END { exit found ? 0 : 1 }'
then
    echo "ELF audit: undefined symbol found" >&2
    "$readelf_tool" -Ws "$binary" |
        awk '$7 == "UND" && $8 != "" { print }' >&2
    exit 1
fi
echo "ELF audit: static, no interpreter/dynamic deps/undefined symbols"
