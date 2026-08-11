# 10-bit implementation WP10 public decode

## Result

10B-WP10 is complete. Canonical HEIF query and decode now admit consistent
profile-0 10-bit 4:2:0 metadata and return tightly packed little-endian U16
Y, U, and V planes. The retained 16x16 fixture decodes byte-for-byte to the
pinned AVM reference through `av2h_query` and `av2h_decode`.

The public format name is `yuv420p10le`, with the matching raw suffix
`.yuv420p10le`. Hosted, Linux no-libc, Windows, and WebAssembly adapters select
byte-explicit workspace and output APIs. BMP and PNG preview output remains
8-bit-only instead of silently reducing U16 samples. WebAssembly reports bit
depth, bytes per sample, and byte order alongside the preserved planar bytes;
the browser performs an explicit display-only reduction to canvas pixels.

## Canonical contract

For the retained 16x16 4:2:0 fixture, query reports 10-bit U16 little-endian
storage and three YUV planes. Y is 16x16 at offset 0 with 32-byte rows and 512
bytes total. U and V are 8x8 with 16-byte rows and 128 bytes each at offsets
512 and 640. Total output is 768 bytes. Workspace size is 26497 bytes with
alignment 4. Final output has FNV-1a-64 `61dd9ffdb54a4439` and exactly matches
the retained AVM bytes.

Metadata admission continues to require agreement between `av2C`, the
Sequence Header, and three-channel `pixi`. A `pixi` depth mismatch is invalid
HEIF data. Recognized but unimplemented profile, chroma, and depth combinations
remain unsupported; the retained 10-bit 4:2:2 and 12-bit 4:2:0 negative OBUs
lock those statuses.

## Failure contract

Focused public tests cover stale input fingerprints, stale plane layout and
sample-width bindings, misaligned or undersized workspace, undersized output,
workspace/output/info/error overlaps, every shorter input prefix, and output
nonmutation on every preflight failure. Existing 8-bit public fixtures retain
their exact layouts and output hashes.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp10
```

It runs container, output-layout, pixel-format, preview, public-decode, and
workspace-accounting tests under the selected host preset; compiles the strict
freestanding surface; verifies retained unsupported and malformed fixtures;
requires exact successful WP0 10-bit raw output; and builds and verifies exact
10-bit WebAssembly output and result metadata. The complete regression gate
remains `make test`.