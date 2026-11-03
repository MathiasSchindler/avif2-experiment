# Refactoring M2: prediction admission and dispatch

**Status:** complete, 2 November 2026.

This implements [M2](../history/refactor.md#m2-simplify-prediction-admission-and-dispatch)
after [M1](refactoring-m1.md), without expanding the
[capability contract](../capabilities.md).

## Resulting rules

The [dispatcher](../../src/decoder/intra_prediction.c) shrinks from 1,205 to
385 lines, including its compatibility helper and the existing U16 entry
points.

- A prediction-owned geometry predicate checks the existing five supported
  dimensions on both axes. It replaces the repeated DC rectangle enumeration
  and repeated dimension pairs, not the feature restrictions.
- Canonical U8 directional prediction has one family rule and one call to the
  existing directional-feature owner. Plain prediction retains its existing
  permissive direct-helper IBP behavior; nonplain features retain the exact
  angle-delta, MRL, multiline and IBP bounds.
- DC and the three smooth modes share a geometry/feature preflight, then an
  explicit mode switch. U8 Paeth retains its original bounded-dimension rule;
  it is not accidentally narrowed to the transform-size set.
- The U16 entry point checks pointers, stride and geometry once, preserves
  multiline precedence, and removes only subsequently redundant MRL/multiline
  checks from its directional zones. Its different zone-specific IBP rules
  and DC/direct-helper behavior remain unchanged.
- Noncanonical directional direct-helper behavior is isolated in
  `predict_legacy_directional`. This includes the existing bounded U8
  horizontal behavior on non-transform dimensions. These compatibility cases
  are not new parser or public-stream admission.

The original dispatcher had 93 first-match guards. A compiler-delimited
predicate-class inventory identified **56 shadowed guards**, which were
removed. Three additional plain-direction IDIF forwarding paths were merged:
4x4 directional prediction, 32x32 vertical with IBP, and 4x8 D203. In these
cases the feature owner's existing non-multiline path delegates to the same
IDIF kernel with the same angle.

The large MRL exception gate was replaced with explicit compatibility paths
and a default rejection. One subtle retained case is 8x8 horizontal,
MRL index 2, no multiline, with nonboolean IBP: it previously passed an
exception gate into the horizontal kernel. Differential route checking caught
this case, and its pixels/status are now frozen independently.

No numerical kernels or normative tables were deleted or rewritten to
justify a dispatch simplification. Existing private numerical entry points
remain; broader interface cleanup is still M6.

## Shared setup

The [private prediction header](../../src/decoder/internal/intra_prediction.h)
now owns the identical small MRL-angle adjustment used by U8 feature setup,
U16 dispatch, and U16 secondary prediction.

The [edge owner](../../src/decoder/intra_prediction_edges.c) shares the three
byte-identical U16 negative-reference preparation blocks through one typed
helper. Capacity validation still precedes this preparation. Corner
selection, negative-reference fallback, extension order, zone-specific
filtering and IBP handling remain in their original order. U8 and U16 are not
forced through an untyped buffer or macro-generated kernel.

No new translation unit or build-source manifest is needed for M2.

## Characterization and equivalence

The [baseline unit](../../tests/refactor_baseline_unit.c) retains every
original M0 expectation. Its new `test_prediction_dispatch` adds **84,340
direct calls in 28 frozen groups**, measured against the unchanged M1
production library before consolidation:

- All 25 dimension pairs, including 4x64 and 64x4.
- Both sample widths and modes 0-13, including the unknown-mode boundary.
- Angle deltas -3 through 3 and MRL indices 0-3, with selected multiline/IBP
  combinations and all four edge-availability states.
- Out-of-range deltas -4/+4 and 25 focused legacy compatibility vectors.
- Zero/non-transform/oversized dimensions, multiline value 2 and MRL 255.
- Existing padded stride 67, explicit-endian hashes, sample limits, padding
  and end guards.

This is selected cross-product coverage, not every possible flag/reference
combination. Direct-helper successes are characterization, not additional AVM
or public-profile admission. Some historically accepted private inputs are
not legal stream tuples.

The M0 inventory's original 48 printed groups remain byte-identical.
The new groups are still checked when `--inventory` is used, but do not change
that output format. Default execution checks **97,908 total direct calls**.
No existing expected digest was regenerated.

Pre-M2 dispatcher SHA-256:
`2e64468454d31483dc61119d9cfcd3182d62639f59dff9ac5e476dccf8f6378f`.

A separate compiler-AST-based before/after probe compared **30,792,960**
kernel/angle/status routes across U8 luma, U16 luma and U16 chroma:

- Dimension representatives: `{0,1,4,8,16,32,64,65,65535}`.
- Mode representatives: `-1` through `13`.
- Angle representatives: `{-128,-4,-3,-2,-1,0,1,2,3,4,127}`.
- MRL representatives: `{0,1,2,3,4,255}`.
- Multiline and IBP representatives: `{0,1,2,255}` independently.
- Four zero/nonzero edge-count states and Intra-DIP off/on.

These are equivalence classes of the actual dispatch comparisons, including
noncanonical scalar fields. Numerical calls were instrumented, retaining the
unchanged D203 fallback rejection guard and the feature owner's pure IDIF
forwarding calculation. Every route matched. This probe complements, rather
than replaces, frozen pixel tests; it is not malformed-kernel memory-safety
or AVM conformance evidence.

## Verified gates

Host: macOS ARM64, 64-bit pointers/`size_t`, Clang 22.1.8, ASan/UBSan and
strict C11 warnings. Diagnostics-disabled means the macro is absent.

| Gate | Result |
| --- | --- |
| Focused prediction/reconstruction/transform/public/layout units | Pass |
| Complete `test-c`, diagnostics absent and enabled | Both pass: 25 host units, runtime unit, 2,460 malformed cases, output-policy and six SHA-256 checks |
| M0 and M2 prediction snapshots | All 97,908 direct calls pass unchanged expected values |
| Original M0 inventory | Identical to M0 and between both M2 configurations |
| Resource/layout and failure behavior | Existing workspace, alignment, status, diagnostic and caller-buffer assertions pass |
| Spatial admission, both configurations | 16 exact fixtures, eight source-bound witnesses and 11,850 malformed cases pass per configuration |
| Native photographic parity | 88 legacy and 28 AVM-default references exact |
| Native filter parity | Tiny, odd, ordinary and high-detail Y/U/V exact |
| Reference locks | 130 valid records; 116 local manifests present |
| Freestanding symbols and source-table evidence | Fresh core and complete evidence gate pass |
| WebAssembly | Fresh wasm32 module; 16 spatial fixtures exact under Node |
| Windows x86-64 and ARM64 | Fresh cross-builds; decoder/smoke imports remain exactly 10/1, KERNEL32.dll only |
| Python discovery | 76 tests: 75 pass, one unchanged missing-artifact error |
| Existing current-oracle Baya replay | Four cases pass; zero errors, skips or partial results |

No encoded fixture, golden pixel, reference lock, public signature/layout,
workspace size or oracle revision was changed.

### Reproduction

Use fresh build directories when changing flags; object rules do not reliably
track flag or header changes.

```sh
make -j4 HOST_BUILD_DIR=build/refactor-m2-diag-absent \
  test-c spatial-still-admission
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m2-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission
build/refactor-m2-diag-absent/refactor-baseline-unit --inventory \
  > build/refactor-m2-diag-absent/inventory.csv
build/refactor-m2-diag-on/refactor-baseline-unit --inventory \
  > build/refactor-m2-diag-on/inventory.csv
cmp build/refactor-m0-diag-absent/inventory.csv \
  build/refactor-m2-diag-absent/inventory.csv
cmp build/refactor-m2-diag-absent/inventory.csv \
  build/refactor-m2-diag-on/inventory.csv
make NATIVE_BUILD_DIR=build/refactor-m2-native \
  HOST_BUILD_DIR=build/refactor-m2-diag-absent \
  test-freestanding test-decoder-evidence
python3 tools/verify_image_references.py --suite photographic-8bit \
  --require-files --decoder build/refactor-m2-diag-absent/av2heif-decoder --jobs 4
python3 tools/verify_image_references.py --suite avm-default-8bit \
  --require-files --decoder build/refactor-m2-diag-absent/av2heif-decoder --jobs 4
python3 tools/run_filter_parity.py \
  --decoder build/refactor-m2-diag-absent/av2heif-decoder --jobs 4
python3 experimental/wasm/build.py --output-directory build/refactor-m2-wasm
node tools/validate_spatial_still_wasm.js build/refactor-m2-wasm/decoder.wasm
```

The Windows builds use `make windows` with separate
`WINDOWS_BUILD_DIR=build/refactor-m2-windows-x86_64` and
`WINDOWS_ARCH=arm64 WINDOWS_BUILD_DIR=build/refactor-m2-windows-arm64`, followed
by the existing import checker as in [M1](refactoring-m1.md#reproduction).

### Moving-oracle replay

```sh
python3 tools/check_avm_current.py \
  --decoder build/refactor-m2-diag-absent/av2heif-decoder \
  --suite photographic-8bit --source baya-weaver --preparation le896 \
  --mode both --output-dir build/refactor-m2-current-baya
```

The existing oracle was neither fetched nor advanced:

- Revision: `edaf3ebc7ce3666290003f70a66efe5a508982a1`.
- Encoder SHA-256: `1463b6e289bc2b0887f687de69bae4284570d72620cfa0cefd0648ecd369cc69`.
- Decoder SHA-256: `a2081a6207c82cceac9c44ec4646416309bb9bcdc36002b23408c365d1a29121`.
- Pinned baseline remains `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

I400 QP152, I420 QP96/QP152 and I444 QP152 all pass. Existing streams match
their pinned decoded references; all four fresh encodes match the current
oracle's pixels exactly. The replay output directory must be new.

## Incomplete gates and limits

The following gates were rerun and still fail only on absent media:

- `make test`: passes C tests, then cannot read
  `images/artifacts/chimp-93x111-qp064.obu`.
- Python discovery: missing
  `images/artifacts/emerald-192x96-qp208-decoded.i420`.
- `make test-reference-images`: missing target-baseline Baya manifest.
  All 14 target-baseline manifests remain absent.
- Full 10-bit WebAssembly validator: missing
  `images/artifacts/10bit/avm-reference/manifest.json`.

No failure was relabeled as a pass or hidden by regenerating media. Other M0
coverage limits remain. Windows runtime, Linux no-libc runtime, browser UI,
maximum-image decoding and peak-stack measurement were not executed here;
full 10-bit Wasm runtime parity and native 32-bit layouts are not claimed.
