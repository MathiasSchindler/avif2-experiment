#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 NM OBJECT..." >&2
    exit 2
fi

nm_tool=$1
shift

"$nm_tool" -P "$@" | awk '
NF >= 2 && $2 ~ /^[Uu]$/ {
    undefined[$1] = 1
    next
}
NF >= 2 && $2 ~ /^[A-Za-z]$/ {
    defined[$1] = 1
}
END {
    failed = 0
    for (symbol in undefined) {
        if (!(symbol in defined)) {
            print symbol > "/dev/stderr"
            failed = 1
        }
    }
    exit failed
}' || {
    echo "freestanding objects have undefined symbols" >&2
    exit 1
}

echo "freestanding objects have no undefined symbols"