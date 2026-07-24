#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
base_url=https://av2.aomedia.org/v1.0.0
target_dir="$root/docs/spec/av2-v1.0.0"
attachments_dir="$target_dir/attachments"
temporary_file=$(mktemp "${TMPDIR:-/tmp}/av2-spec.XXXXXX")

cleanup() {
    rm -f "$temporary_file"
}
trap cleanup EXIT HUP INT TERM

if ! command -v curl >/dev/null 2>&1; then
    echo "error: curl is required" >&2
    exit 1
fi

mkdir -p "$attachments_dir"
curl --fail --location --silent --show-error \
    "$base_url/index.html" -o "$temporary_file"
mv "$temporary_file" "$target_dir/index.html"

grep -Eo 'href="\./attachments/[^"]+"' "$target_dir/index.html" |
    sed 's|^href="\./attachments/||; s|"$||' |
    LC_ALL=C sort -u >"$temporary_file"
if ! test -s "$temporary_file"; then
    echo "error: no specification attachments found" >&2
    exit 1
fi
while IFS= read -r filename; do
    case "$filename" in
        ""|*/*|*".."*)
            echo "error: unsafe attachment path: $filename" >&2
            exit 1
            ;;
    esac
    curl --fail --location --silent --show-error \
        "$base_url/attachments/$filename" \
        -o "$attachments_dir/$filename"
    ln -sf "attachments/$filename" "$target_dir/$filename"
done <"$temporary_file"

cat >"$target_dir/SOURCE.txt" <<EOF
AV2 Bitstream & Decoding Process Specification v1.0.0
$base_url/index.html
EOF

echo "AV2 specification is ready in $target_dir"
