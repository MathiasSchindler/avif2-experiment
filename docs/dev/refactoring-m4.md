# Refactoring M4: frame responsibility owners

Completed 2 November 2026.

## Ownership

The former 1,482-line storage and 964-line orchestration owners are now:

| Owner | Lines | Responsibility |
| --- | ---: | --- |
| [`frame_workspace.c`](../../src/decoder/frame_workspace.c) | 1,225 | Arena planning/binding, capacities, overlap validation, storage initialization, preflight, and completeness checks |
| [`frame_copy.c`](../../src/decoder/frame_copy.c) | 204 | Planar output sizing/binding, visible-plane cropping/copying, little-endian U16 serialization, and existing execution dispatch |
| [`frame_filter_metadata.c`](../../src/decoder/frame_filter_metadata.c) | 121 | Per-transform deblock and restoration-skip metadata commits |
| [`frame_output.c`](../../src/decoder/frame_output.c) | 884 | Explicit tile traversal, pending chroma scheduling, reconstruction commits, filter handoff/order, and failure reporting |

Typed output validators remain with storage validation. The output owner calls
them before copying any plane; a late-plane capacity or overlap failure still
prevents all output writes. Metadata storage/reset remains a workspace concern,
while transform metadata commits belong to the new filter-metadata owner.

The private
[`frame_filter_metadata.h`](../../src/decoder/internal/frame_filter_metadata.h)
declares the two metadata commits. Existing consumers retain transitive access
through [`frame_output.h`](../../src/decoder/frame_output.h). Broader private
header narrowing remains M6.

Two small local helpers assemble the common luma/chroma failure fields.
Path-specific fields, diagnostic getters, failure-stage selection, and the
existing failure-return macro stay at their callers. There is no new decode-loop
callback framework, workspace structure, allocation, or public API.

The authoritative [`CORE_SOURCES`](../../Makefile) includes both new owners.
Hosted, freestanding, WebAssembly, and Windows builds use that source list.

## Preserved behavior

- Arena allocation order, offsets, sizes/alignment, coded capacities, overlap
  checks, and query/initialization agreement are unchanged.
- Copying retains visible cropping, padded source strides, untouched output
  tails, and explicit little-endian U16 serialization.
- Output dispatch retains the 64 KiB threshold and 16 KiB target row chunk,
  clamped to at least one row. Missing callbacks and execution width at most one
  retain serial copying.
- Callback failures still return invalid-data status. Chunks completed before
  failure can remain written; output copying has not become atomic.
- Deblock commits retain chroma coordinate scaling, clipping, quantizer width,
  and partial writes before an occupied-cell rejection.
- Restoration-skip commits retain output-geometry clipping and store the
  existing all-zero value unchanged, including unusual private-helper inputs.
- Luma's primary-set symbol is populated only on parse failure. U8 chroma
  reconstruction still omits the chroma-mode index; its CfL-filter/MHCCP fields
  remain absent from the U16 counterpart.
- Tile traversal, pending chroma, readiness/deblock commits, filter ordering,
  status precedence, diagnostics, pixels, public layouts, fixtures, and both
  oracle revisions are unchanged.

A source comparison against the pre-extraction snapshots verified byte-identical
bodies for all **26 retained storage functions**, six output sizing/binding/copy
functions, and both metadata commits. The frame-group decode function is
byte-identical outside the five extracted failure-context sections after
normalizing the restoration helper's name.

A temporary differential probe also compared the old and new failure assembly:
**5,120 exact whole-record comparisons**, 2,560 with diagnostics absent and 2,560
enabled. It varied leaf/diagnostic data, null diagnostics, and both chroma planes,
starting result records with a nonzero pattern to detect newly touched fields.
This supplements, rather than replaces, the persistent frame-result tests.

## Regression coverage

[`frame_refactor_unit.c`](../../tests/frame_refactor_unit.c) combines
pre-extraction characterizations with independent copy/metadata expectations:

- **48 workspace initializations:** I420/I400/I444, both sample widths, 17x15
  and 128x65 dimensions, 64/128 superblocks, and restoration absent/present.
  Six frozen hashes cover relative arena offsets, layout sizes/alignment, and
  coded capacities on the 64-bit host ABI.
- **150 output-copy scenario groups:** odd cropping, padded strides, output
  tails, both sample widths and all layouts, dispatch boundaries, absent
  execution/callbacks, serial and reverse-chunk callbacks, callback failures
  before/after work, and the wide-row minimum-chunk clamp. Independent byte
  expectations include raw U16 values above the 10-bit range. Short output,
  insufficient late-plane capacity, and overlap rejection are also checked.
- **2,800 successful deblock combinations:** all 25 geometries, chroma scales,
  clipped edges, skip/zero/lossless flags, and a wide quantizer. An additional
  occupied-cell case freezes the existing partial-write behavior.
- **150 restoration metadata combinations:** all geometries, clipping, and
  all-zero values 0/1/2, plus 300 invalid-position/stride rejections.
- **65 frame-result characterizations:** the retained high-detail I444 fixture
  and each bit flip in its first eight entropy bytes. Frozen hashes cover
  status, every scalar result field, diagnostics-pointer presence, and output.
  Both diagnostic configurations retain 49 successes, ten luma-parse failures,
  one luma-reconstruction failure, and five chroma-parse failures. These failed
  decodes leave output unchanged.

Existing workspace, output-parallel, filter, public-decode, and spatial tests
remain in place. All **97,908** prediction calls and **21,600** M3 plane
characterizations retain their original expected values. No source-bound
witness, golden output, reference lock, or profile admission was changed.

## Verified gates

Host: macOS ARM64, strict C11 warnings, Clang 22.1.8, ASan/UBSan.
Diagnostics-disabled means the macro is absent.

| Gate | Result |
| --- | --- |
| Full `test-c`, diagnostics absent and enabled | Both pass: 28 host units, runtime unit, 2,460 malformed cases, output-policy and six SHA-256 checks |
| Frozen M0 inventory and resource/layout checks | Inventories byte-identical to M0 and between both configurations |
| Spatial admission, both configurations | 16 exact fixtures, eight source-bound witnesses, and 11,850 malformed cases pass per configuration |
| Native photographic parity | 88 legacy and 28 AVM-default references exact |
| Native filter parity | Tiny, odd, ordinary, and high-detail Y/U/V exact |
| Reference locks | 130 valid records; 116 local manifests present |
| Freestanding symbols and source-table evidence | Fresh core has no undefined symbols; evidence gate passes |
| WebAssembly | Fresh wasm32 module; all 16 spatial fixtures exact under Node |
| Windows x86-64 and ARM64 | Fresh cross-builds; decoder/smoke imports remain exactly 10/1, KERNEL32.dll only |
| Existing current-oracle Baya replay | Four cases pass in both existing-stream and fresh-encode modes; no errors, skips, or partial results |

The oracle was not fetched, rebuilt, or advanced. Current remains
`edaf3ebc7ce3666290003f70a66efe5a508982a1`; pinned remains
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

### Reproduction

Use fresh build directories when changing flags or headers. Run the two spatial
configurations sequentially because their generated paths are shared.

```sh
make -j4 HOST_BUILD_DIR=build/refactor-m4-diag-absent \
  test-c spatial-still-admission
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m4-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission
make NATIVE_BUILD_DIR=build/refactor-m4-native \
  HOST_BUILD_DIR=build/refactor-m4-diag-absent \
  test-freestanding test-decoder-evidence
make test-reference-locks
python3 experimental/wasm/build.py --output-directory build/refactor-m4-wasm
node tools/validate_spatial_still_wasm.js build/refactor-m4-wasm/decoder.wasm
python3 tools/check_avm_current.py \
  --decoder build/refactor-m4-diag-absent/av2heif-decoder \
  --suite photographic-8bit --source baya-weaver --preparation le896 \
  --mode both --output-dir build/refactor-m4-current-baya
```

The oracle output directory must be new. Photographic, filter, Windows, and
inventory commands follow [M2's reproduction](refactoring-m2.md#reproduction),
substituting the `refactor-m4-*` build directories.

## Incomplete gates and limits

The following gates were rerun and retain the same missing-media blockers:

- Aggregate `make test`: C checks pass, then corpus parity cannot read
  `images/artifacts/chimp-93x111-qp064.obu`; the 31 legacy corpus OBUs are absent.
- Python discovery: 75 of 76 tests pass; one errors because
  `images/artifacts/emerald-192x96-qp208-decoded.i420` is absent.
- Target-baseline parity: all 14 local manifests are absent; the first failure
  is Baya I420 QP152.
- Full 10-bit Wasm manifest validation: absent
  `images/artifacts/10bit/avm-reference/manifest.json`.

These are not passing aggregate gates. No fixtures were regenerated and no
checks were weakened or silently skipped.

Output-dispatch cases use mock callbacks, not a new real-threaded runtime or
performance measurement. Windows execution, Linux no-libc execution, browser
runtime, maximum-image decoding, peak-stack measurement, and native 32-bit
layout validation remain untested. Large workspace queries and cross-builds do
not establish those claims.
