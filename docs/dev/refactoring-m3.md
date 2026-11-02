# Refactoring M3: block reconstruction and plane integration

Completed 2 November 2026.

## Ownership

The former 3,080-line reconstruction owner is now:

| Owner | Lines | Responsibility |
| --- | ---: | --- |
| [`reconstruct_block.c`](../../src/decoder/reconstruct_block.c) | 1,370 | Supplied-predictor reconstruction: lossless selection, legacy and generic inverse-transform selection, residual composition, clipping, and inverse cross-component transform |
| [`reconstruct.c`](../../src/decoder/reconstruct.c) | 1,801 | Typed picture-plane validation, reference gathering, prediction preparation, palette and IntraBC integration, visible writes, readiness commits, and existing diagnostic accessors |

The new private
[`reconstruct_block.h`](../../src/decoder/internal/reconstruct_block.h)
exposes the extracted U16 luma and U8/U16 chroma block entry points. They accept
contiguous predictors covering the full output geometry, without a plane,
readiness map, partition node, or entropy decoder. The existing U8 luma entry
points retain their supplied-predictor stride and signatures.

U16 lossless and DPCM reconstruction previously embedded in plane integration
now belongs to the block owner. Chroma's predictor-only path, post-CCTX
all-zero-prefix adjustment, and transform dispatch also belong there.
Existing inverse-transform and secondary-transform kernels remain in their
original owners, with unchanged arithmetic. Existing private entry points and
declarations remain available; broader header narrowing belongs to M6.

The shared 4x4-cell readiness queries and readiness-marking loop live in the
plane integration owner. U8 and U16 retain typed plane access and wrappers;
there is no runtime-polymorphic sample/plane abstraction.

The authoritative [`CORE_SOURCES`](../../Makefile) includes the new owner.
Hosted, freestanding, WebAssembly, and Windows builds use that source list.

## Preserved behavior

- Predictor/output geometry remains distinct from coefficient geometry:
  output can be 64x64 while coefficient storage is capped at 32x32.
- Luma and chroma keep their existing, different lossless and DPCM selection
  rules, including signed floor-shift versus division rounding.
- U/V inverse cross-component transform remains at the same scheduling point;
  coefficient nonzero counts and chroma's effective prefix retain their
  existing treatment.
- References, palette maps, tile sentinels, CfL/IntraBC preparation, and
  prediction admission remain integration responsibilities.
- Plane pixels are committed before their visible readiness rectangle.
  Failures preserve the destination plane and readiness map in the frozen
  integration cases.
- The optional U8 chroma predictor plane is still written **before**
  reconstruction validation. A later coefficient failure can therefore leave
  predictor-plane output, just as before this milestone.
- Status distinctions, diagnostic stage timing, public interfaces, scratch
  layouts, workspace sizes, fixtures, golden pixels, locks, and both oracle
  revisions are unchanged.

This is an ownership extraction, not new profile admission or a normalization
of unusual private-helper inputs.

## Independent regression coverage

[`reconstruction_plane_unit.c`](../../tests/reconstruction_plane_unit.c)
freezes **21,600** calls measured before production extraction:

- Six paths: U8/U16 luma, U8/U16 I420 chroma, and U8/U16 I444 chroma.
- All 25 transform geometries and four prediction modes: DC, vertical,
  smooth, and D135.
- Six reference/position cases: no neighbours, left only, above only, both,
  unavailable/tile-sentinel cells, and a partial bottom-right visible block.
- Six coefficient/transform cases: zero residual, nonzero DCT, lossless,
  incomplete coefficient count, stray all-zero coefficient, and identity.

Each path has 3,600 calls. Status counts and explicit-endian hashes cover the
entire padded destination, readiness map, and optional chroma predictor plane.
Separate frozen hashes cover luma, chroma, and inverse-transform diagnostic
stages with diagnostics absent and enabled. Successes additionally check
untouched pixels outside the visible block and the exact readiness rectangle;
failures check destination/readiness nonmutation.

| Path | OK | Unsupported | Invalid data |
| --- | ---: | ---: | ---: |
| U8 luma | 2,400 | 1,200 | 0 |
| U16 luma | 2,400 | 600 | 600 |
| U8 I420 chroma | 3,000 | 600 | 0 |
| U16 I420 chroma | 2,400 | 600 | 600 |
| U8 I444 chroma | 3,000 | 600 | 0 |
| U16 I444 chroma | 2,400 | 600 | 600 |

These are implementation characterizations, not additional AVM conformance
claims. Existing private-path differences were preserved, not reclassified.

[`reconstruction_block_unit.c`](../../tests/reconstruction_block_unit.c)
tests supplied predictors without tile orchestration:

- All 25 zero-residual geometries through all four typed luma/chroma paths,
  including full output expansion and untouched output-array tails.
- Both U16 selectors against the unchanged generic kernel over all 25
  geometries and five primary transform types.
- Independent horizontal/vertical DPCM residual and clipping expectations,
  including the existing U8/U16 negative rounding difference.
- U16 lossless WHT with and without DPCM, lossless chroma identity/DPCM,
  malformed-input statuses, and selected failure-output nonmutation.

The existing reconstruction unit and source-bound witnesses remain unchanged.
All **97,908** M0/M2 prediction calls retain their original expected values.

## Verified gates

Host: macOS ARM64, strict C11 warnings, Clang 22.1.8, ASan/UBSan.
Diagnostics-disabled means the macro is absent.

| Gate | Result |
| --- | --- |
| Full `test-c`, diagnostics absent and enabled | Both pass: 27 host units, runtime unit, 2,460 malformed cases, output-policy and six SHA-256 checks |
| Frozen M0 inventory and resource/layout checks | Unchanged; inventories identical to M0 and between both configurations |
| Spatial admission, both configurations | 16 exact fixtures, eight source-bound witnesses, and 11,850 malformed cases pass per configuration |
| Native photographic parity | 88 legacy and 28 AVM-default references exact |
| Native filter parity | Tiny, odd, ordinary, and high-detail Y/U/V exact |
| Reference locks | 130 valid records; 116 local manifests present |
| Freestanding symbols and source-table evidence | Fresh core has no undefined symbols; complete evidence gate passes |
| WebAssembly | Fresh wasm32 module; all 16 spatial fixtures exact under Node |
| Windows x86-64 and ARM64 | Fresh cross-builds; decoder/smoke imports remain exactly 10/1, KERNEL32.dll only |
| Existing current-oracle Baya replay | Four cases pass in both existing-stream and fresh-encode modes; no errors, skips, or partial results |

The current oracle was not fetched, rebuilt, or advanced. It remains revision
`edaf3ebc7ce3666290003f70a66efe5a508982a1`; the pinned baseline remains
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

### Reproduction

Use fresh build directories when changing flags or headers.

```sh
make -j4 HOST_BUILD_DIR=build/refactor-m3-diag-absent \
  test-c spatial-still-admission
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m3-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission
make NATIVE_BUILD_DIR=build/refactor-m3-native \
  HOST_BUILD_DIR=build/refactor-m3-diag-absent \
  test-freestanding test-decoder-evidence
python3 experimental/wasm/build.py --output-directory build/refactor-m3-wasm
node tools/validate_spatial_still_wasm.js build/refactor-m3-wasm/decoder.wasm
python3 tools/check_avm_current.py \
  --decoder build/refactor-m3-diag-absent/av2heif-decoder \
  --suite photographic-8bit --source baya-weaver --preparation le896 \
  --mode both --output-dir build/refactor-m3-current-baya
```

The oracle output directory must be new. Photographic, filter, Windows, and
inventory commands follow [M2's reproduction](refactoring-m2.md#reproduction),
substituting the `refactor-m3-*` build directories.

## Incomplete gates and limits

These gates were rerun and still fail on the same absent media:

- `make test`: passes C tests, then cannot read
  `images/artifacts/chimp-93x111-qp064.obu`.
- Python discovery: 76 tests, 75 pass, one error for missing
  `images/artifacts/emerald-192x96-qp208-decoded.i420`.
- `make test-reference-images`: missing target-baseline Baya manifest;
  all 14 target-baseline manifests remain unavailable.
- Full 10-bit WebAssembly validator: missing
  `images/artifacts/10bit/avm-reference/manifest.json`.

No fixtures were regenerated and no expectation was weakened to hide these
failures. Windows runtime, Linux no-libc runtime, browser UI, maximum-image
decoding, peak-stack measurement, and native 32-bit layouts were not tested.
Native U16 reconstruction/prediction and the 8-bit Wasm spatial runtime pass;
full 10-bit Wasm corpus parity remains unverified.
