# Refactoring M0: behavioral and resource baseline

**Status:** complete, 2 November 2026. This freezes local evidence before
M1-M8; it is not a new codec admission or a claim that every gate passes.

Production baseline: commit `4e8662e9527668e17dcfbc632434aea49b7a7e25`.
No production sources, AVM revisions, encoder recipes, retained media, golden
pixels, or reference locks were changed. The new
[baseline unit](../../tests/refactor_baseline_unit.c) runs in `make test-c`.

The measured type labels below retain their original M0 names. [Cleanup
C3](../history/cleanup.md) renamed the general-purpose decoder types without
changing their layouts or baseline values; current inventory output uses the
canonical names.

## Contract and retained regression

[Capabilities](../capabilities.md) remains authoritative:

- Supported public outputs: 8-bit I400/I420/I444; 10-bit 4:2:0 profile 0 and
  4:4:4 profile 4, stored as little-endian 16-bit samples.
- Unsupported: 10-bit monochrome, I422, 12-bit, video/inter/temporal or layered
  structures, film grain, auxiliary alpha, grids, container transforms, and
  restoration outside the separately admitted 10-bit profiles.
- Support for a tool or geometry is not admission of its entire cross-product.
  Raw spatial-tile/multiple-group support must not broaden canonical public
  HEIF carriage accidentally.

The retained [Baya stream](../../tests/fixtures/baya-weaver-current-i420-qp152.obu)
and [provenance](../../tests/fixtures/baya-weaver-current-i420-qp152.json) remain
unchanged. This 896x598 I420, QP 152 regression has:

| Measurement | Frozen value |
| --- | --- |
| Encoded bytes | 4,889 |
| Encoded SHA-256 | `dc7eeffcccbcc20654d5e189f362b701701dfa2f5a63f542fdedf6c9a5f14ff3` |
| Decoded bytes | 803,712 |
| Decoded SHA-256 | `7324889ae632f9e4f75eab47dcc8e430e1df8efde8c9ba68e23884bf5f492b1f` |
| Public workspace size / alignment | 5,511,999 / 4 bytes |
| Public output FNV-1a64 | `6fe0610ca8d389e1` |

`test_dc_4x64_and_64x4_prediction` retains all 16 AVM-output-bound cases:
two shapes, four edge-availability states, IBP off/on.
`test_public_decode` and the SHA-256 check in `make test-c` retain full-image
coverage. Attribution remains Tisha Mukherjee, CC-BY-SA-4.0; this photographic
fixture is not CC0. No moving-oracle refresh was needed for M0.

## Prediction inventory

Here **tested** means an executing assertion; **implemented** means an existing
code path, not proof of every tuple; **unsupported** identifies a measured
entry-point rejection or an explicit public-profile exclusion. New snapshots
characterize the unchanged implementation, not additional AVM conformance.

`test_prediction_matrix` freezes **13,568 direct luma calls** in 48 groups:
12,472 OK, 200 unsupported, and 896 invalid-argument results. Each group locks
status counts and an ordered FNV-1a64 digest of inputs, statuses, and active
output samples, including failure-buffer contents. Samples are hashed in
explicit byte order. Every call also checks output padding and end guards;
successful 10-bit samples must be at most 1023.

| Family | Tested dimensions and settings | Observed boundary |
| --- | --- | --- |
| Basic 8-bit luma | All 25 width/height pairs from `{4,8,16,32,64}`; all 13 modes; no/top/left/both edges; IBP 0/1; angle delta and MRL zero | 2,600 OK |
| Basic 10-bit luma helper | Same 2,600 tuples | 2,400 OK; 200 DC calls return `AV2H_UNSUPPORTED` |
| Directional 8/10-bit luma | 8x8, 16x16, 8x32, 32x8; eight directional modes; angle deltas -3 through 3; MRL 0-3; multiline 0/1; IBP 0/1; both edges and secondary/negative references | 6,272 OK; 896 `AV2H_INVALID_ARGUMENT` for multiline enabled with MRL zero |
| Intra-DIP 8-bit | All 25 shapes; six modes and their transposes; all four edge states; IBP/MRL zero | 1,200 OK |
| DC 8-bit oracle regression | 4x64 and 64x4; all four edge states; IBP 0/1 | 16 existing AVM-output-bound cases |
| Reference preparation / plane integration | Existing `test_cardinal_prediction_without_references`, `test_luma_smooth_partial_left_extension`, `test_live_plane_reconstruction`, and `test_tx8x8_d203_mrl_reconstruction` | Active focused vectors, not every tile/readiness/MRL cross-product |
| Chroma DC/CfL/MHCCP | Existing `test_cfl_explicit_geometry_filter_matrix`, `test_chroma_geometry_through_64`, `test_i444_mhccp_long_left_reference`, and `test_i444_mhccp_long_top_reference` | Active geometry/filter and selected long-reference vectors; broad MHCCP and chroma-angle matrices remain disabled |
| 10-bit reconstruction/integration | Existing `test_10bit_wp5_frame_fixture`, `test_10bit_wp6_tiny_matrix`, and 10-bit `test_10bit_public_decode` fixtures | Active samples include plane-owned DC; not a new exhaustive 10-bit feature matrix |
| Excluded public tuples | 10-bit monochrome, I422, 12-bit | Unsupported by contract; internal storage queries do not admit them |

Mode order in the snapshot arrays is DC, vertical, horizontal, D45, D135,
D113, D157, D203, D67, smooth, smooth-vertical, smooth-horizontal, Paeth.
Directional groups use modes 1-8 in that order. `--inventory` prints each
group's actual counts and digest **while still checking the frozen values**.
It cannot regenerate or waive expectations.

The direct 10-bit luma helper's DC rejection is intentional characterization:
DC is implemented in plane integration and the chroma helper, not that entry
point. It does **not** make 10-bit DC streams unsupported. Likewise a direct
kernel returning OK does not prove that the parser/plane dispatcher admits
the tuple, that an enabled flag changes the output, or that AVM agrees.

### Implemented but not exhaustively characterized

MRL with missing/partial edges, all edge-filter/smooth-neighbor flags, arbitrary
readiness/tile boundaries, broad chroma angle/MHCCP combinations, and the full
transform/prediction cross-product remain partial coverage. The new luma
matrix does not replace these missing integration/oracle witnesses. Intra-DIP
is characterized only through the 8-bit entry point.

The following functions are still behind `if (0)` in
[reconstruct_unit.c](../../tests/reconstruct_unit.c) and are **not passing tests**:

- `test_d45_minus_one_16x4_prediction`
- `test_d45_8x4_ibp_prediction`
- `test_d113_16x64_prediction`
- `test_luma_directional_feature_matrix`
- `test_chroma_angle_delta_matrix`
- `test_adst_dct_geometry_matrix`
- `test_requested_transform_geometry_matrix`
- `test_mhccp_geometry_direction_matrix`

M0 neither enables stale expectations nor silently counts these as evidence.
Any extraction touching their untested combinations needs focused additional
evidence before simplifying admission or arithmetic.

## Failure and diagnostic characterization

The new `test_unsupported_reconstruction` rejects an all-zero 4x4 block with
inconsistent coefficient storage at both sample widths. It checks
`AV2H_UNSUPPORTED` at block reconstruction and plane integration, complete
reconstruction/pixel nonmutation, and unchanged readiness maps. With
diagnostics enabled, luma stage 2 and inverse-transform stage 10 are locked.
With diagnostics disabled, the accessor returns null and the entire supplied
diagnostic record stays untouched.

Existing `test_frame_unsupported_status` separately locks an entropy mutation's
`AV2H_UNSUPPORTED` cause, luma-parse stage, tell 171, and unchanged full output.
It is a parse-stage regression, not a reconstruction-stage stream witness.

[Public boundary tests](../../tests/public_decode_unit.c) now additionally
check the entire output after every truncated-container attempt, not just its
first byte, and hash the full workspace around overlap rejections. Both 8-bit
and 10-bit paths exercise short workspace/output, alignment, stale bindings,
workspace/output aliasing, info/workspace aliasing, and error/output aliasing.
The 10-bit check also includes the trailing output guard.

Other retained named failure coverage includes:

- `test_leb128_and_obus`: truncation, noncanonical/overwide syntax, status,
  offset, and error context.
- `test_frame_decode_workspace_model`: short/misaligned/stale arena rejection
  without modifying caller records.
- `test_frame_workspace_prepare_rejection`: preflight/nonmutation.
- `test_gdf_frame`: overlapping filter planes.
- `test_fail_closed`: invalid filtering inputs.
- `test_emerald_reconstruct_invalid_preserves_output`: rejected block output.

## Resource baseline

Sizes below are bytes, frozen for 64-bit pointer/`size_t` hosts. No maximum-size
image is allocated or decoded by these query tests. Each synthetic frame has a
64-pixel superblock, zeroed optional filter settings, and explicit plane/chroma
counts. U8/U16 describe storage, not public admission; in particular the U16
I400 query is **not** 10-bit monochrome support.

`test_resource_baseline` locks all three quantities at both widths:

| Dimensions / chroma | Filter U8 / U16 | Decode arena U8 / U16 | Visible output U8 / U16 |
| --- | ---: | ---: | ---: |
| 17x15 I420 | 2,163 / 3,315 | 22,327 / 28,666 | 399 / 798 |
| 17x15 I400 | 1,105 / 1,873 | 20,963 / 26,724 | 255 / 510 |
| 17x15 I444 | 3,315 / 5,619 | 24,055 / 32,122 | 765 / 1,530 |
| 896x598 I420 | 3,024,420 / 4,637,220 | 5,512,732 / 7,936,543 | 803,712 / 1,607,424 |
| 896x598 I400 | 1,545,740 / 2,620,940 | 3,697,004 / 5,314,413 | 535,808 / 1,071,616 |
| 896x598 I444 | 4,637,220 / 7,862,820 | 7,931,932 / 12,774,943 | 1,607,424 / 3,214,848 |
| 16384x16384 I420 | 1,510,146,048 / 2,315,452,416 | 2,735,546,888 / 3,943,511,051 | 402,653,184 / 805,306,368 |
| 16384x16384 I400 | 771,817,472 / 1,308,688,384 | 1,829,298,696 / 2,634,609,673 | 268,435,456 / 536,870,912 |
| 16384x16384 I444 | 2,315,452,416 / 3,926,065,152 | 3,943,506,440 / 6,359,430,155 | 805,306,368 / 1,610,612,736 |

Filter alignment is 2; decode-arena alignment is 4. The synthetic arena query
is not the public Baya workspace query: real frame settings and public
bookkeeping differ. Existing `test_maximum_workspace` separately checks
resource-policy limits; a successful internal size query is not permission to
exceed public workspace limits. Maximum-size snapshots explicitly report
unavailability on 32-bit `size_t`; exact 32-bit layouts are not claimed.

The existing [layout unit](../../tests/layout_unit.c) preserves public layouts,
arithmetic widths, transform/entropy state, scratch, planes and filter layouts.
The new `test_private_layout_baseline` adds caller-owned integration types.
Representative and newly added assertions are:

| Type | Size / alignment |
| --- | ---: |
| `Av2hInfo` / `Av2hError` | 248 / 8; 64 / 8 |
| `Av2hSequenceState` / `Av2hFrameState` | 390 / 2; 11,088 / 8 |
| `Av2hTileDecoder` | 70,088 / 8 |
| `Av2hReconstructionScratch` / `Av2hReconstructionScratchU16` | 16,896 / 4; 21,504 / 4 |
| `Av2hFrameDecodeWorkspace` / `Av2hFrameDecodeResult` | 832 / 8; 144 / 8 |
| `Av2hFilterWorkspace` / `Av2hFilterWorkspaceStorage` | 672 / 8; 56 / 8 |
| `Av2hDecodeDiagnostics` | 216 / 8 |
| `Av2hFirstLeafMode` / `Av2hFirstChromaMode` | 1,760 / 8; 112 / 8 |
| `Av2hLumaReferences` / `Av2hLumaReferencesU16` | 80 / 8; 96 / 8 |
| `Av2hFirstReconstruction` / `Av2hFirstReconstructionU16` | 12,292 / 2; 24,580 / 4 |

Diagnostics on/off produce identical prediction, resource, and layout
inventories. These are storage/layout baselines, not peak-stack measurements.

## Extraction-to-test map

Every listed unit was run by the native C gate in both diagnostic configurations.
Resource references below point to the fixed assertions above, not estimates.

| Planned extraction | Named active behavioral tests | Resource baseline / unit command owners |
| --- | --- | --- |
| M1 edge preparation | `test_luma_smooth_partial_left_extension`, `test_cardinal_prediction_without_references`, `test_prediction_matrix` | Reference records and reconstruction scratch; `reconstruct-unit refactor-baseline-unit` |
| M1 nondirectional / directional kernels | `test_prediction_matrix`, `test_dc_4x64_and_64x4_prediction`, `test_luma_zero_angle_frontier_matrix` | Scratch, reference records, guarded output strides; same units |
| M1 Intra-DIP/data | `test_first_leaf_intra_dip_without_references`, DIP snapshot groups | Scratch and guarded output; same units |
| M2 admission/dispatch | Basic/directional status snapshots, `test_frame_unsupported_status`, `test_fsc_generic_selection_requires_identity` | Same scratch/arena/layout values; same units |
| M3 block/residual reconstruction | `test_unsupported_reconstruction`, `test_dct_adst_geometry_matrix`, `test_adst_flipadst_geometry_matrix`, `test_luma_ist_frontier_matrix` | Reconstruction records and both scratch types; same units plus `transform-unit` |
| M3 plane/palette/IntraBC integration | `test_live_plane_reconstruction`, `test_palette_prediction`, `test_skipped_intrabc_geometry_matrix`, `test_10bit_wp6_tiny_matrix` | Plane, mode and decode-arena sizes; `reconstruct-unit refactor-baseline-unit` |
| M4 workspace planning | `test_storage_width_accounting`, `test_frame_decode_workspace_model`, `test_resource_baseline` | Exact arena/filter/output table; `workspace-accounting-unit filter-unit refactor-baseline-unit` |
| M4 visible/parallel output | `test_odd_u16_visible_crop`, `test_visible_i420_crop`, `test_u8_width_parity`, `test_u16_width_parity` | Output sizes/strides/guards; `workspace-accounting-unit filter-unit output-parallel-unit output-layout-unit` |
| M4 metadata/orchestration | `test_filter_pipeline`, `test_10bit_wp9_gdf_stage`, `test_public_decode`, `test_10bit_public_decode` | Filter state, frame workspace/result and public fixture sizes; `filter-unit reconstruct-unit public-decode-unit` |
| M5 OBU/sequence/carriage | `test_leb128_and_obus`, `test_sequence_headers`, `test_av2c_and_carriage`, `test_content_interpretation` | Sequence/public metadata layouts; `substrate-unit layout-unit` |
| M5 container reader/writer | `test_golden_and_round_trip`, `test_writer_errors`, `test_dialect_detection`, `test_query_and_limits` | Public info/error/image layouts and fixed writer capacity checks; `container-unit layout-unit` |
| M6 headers and shared math | `test_64_bit_decoder_layout`, `test_private_layout_baseline`, arithmetic/sample units | All retained/new type and scalar-width assertions; `layout-unit refactor-baseline-unit substrate-unit sample-math-unit sample-ops-unit` |
| M7 error initialization/context | `test_public_decode`, `test_10bit_public_decode`, `test_frame_unsupported_status`, `test_unsupported_reconstruction`, `test_fail_closed` | Error/result/diagnostic record sizes, output/arena guards; `public-decode-unit reconstruct-unit refactor-baseline-unit filter-unit` |
| M8 integration | Public fixture decodes, spatial witnesses/fuzz, available photographic parity | All preceding baselines; aggregate commands below, with missing-media failures retained |

For any row, use the existing selector runner with its listed unit owners:

```sh
make run-host-tests \
  TESTS='refactor-baseline-unit reconstruct-unit transform-unit layout-unit'
```

## Reproducible validation and observed results

Host: macOS ARM64, 64-bit pointers/`size_t`, Homebrew Clang 22.1.8.
Native C builds use C11, `-Wall -Wextra -Wpedantic -Werror`, ASan and UBSan.
Python discovery used Python 3.14.7.

Use separate, previously unused build directories when changing flags; object
rules do not track compiler-flag changes. **Disabled means the diagnostics
macro is absent.** Defining `AV2HEIF_ENABLE_DIAGNOSTICS=0` still enables current
production diagnostics, because their guards test whether it is defined.

```sh
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m0-diag-absent \
  HOST_CFLAGS="$flags" test-c spatial-still-admission
make -j4 HOST_BUILD_DIR=build/refactor-m0-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission

build/refactor-m0-diag-absent/refactor-baseline-unit --inventory \
  > build/refactor-m0-diag-absent/inventory.csv
build/refactor-m0-diag-on/refactor-baseline-unit --inventory \
  > build/refactor-m0-diag-on/inventory.csv
cmp build/refactor-m0-diag-absent/inventory.csv \
  build/refactor-m0-diag-on/inventory.csv

make test-reference-locks test-freestanding
python3 tools/run_filter_parity.py \
  --decoder build/host-sanitized/av2heif-decoder --jobs 4
python3 tools/verify_image_references.py --suite photographic-8bit \
  --require-files --decoder build/host-sanitized/av2heif-decoder --jobs 4
python3 tools/verify_image_references.py --suite avm-default-8bit \
  --require-files --decoder build/host-sanitized/av2heif-decoder --jobs 4
```

| Gate | Observed result |
| --- | --- |
| Isolated `test-c`, diagnostics absent and enabled | Both pass: 25 host units, runtime unit, 2,460 malformed-input cases, BMP/PNG/raw policy, six SHA-256 parity checks including Baya |
| Prediction/resource/layout inventory comparison | Identical; frozen assertions pass in both builds |
| Spatial admission, both configurations | 16 exact fixtures, eight source-bound witnesses, 11,850 deterministic malformed-input cases pass |
| Reference locks | 130 records valid; 116 local manifests present |
| Freestanding symbol gate | Pass; no undefined symbols |
| Filter parity | tiny, odd, ordinary and high-detail exact in Y/U/V |
| Legacy photographic suite | 88/88 exact |
| AVM-default photographic suite | 28/28 exact |
| `make test`, before and after M0 | Fails at the same first absent artifact OBU after passing `test-c` |
| Python discovery, before and after M0 | 70 tests: 69 pass, one missing-artifact error |
| Complete required-media photographic gate | Fails on absent target-baseline manifests; not a full pass |

### Missing media: not waived

These are pre-existing availability failures, not new expected successes or
skips introduced by M0:

- `make test-corpus`: zero artifact OBUs are locally available under
  `images/artifacts/`; the 31-case
  [manifest](../../tests/artifact-parity.sha256) fails first at
  `chimp-93x111-qp064.obu`.
- Python discovery: `test_emerald_q208_final_artifact_hashes` cannot read
  `images/artifacts/emerald-192x96-qp208-decoded.i420`.
- Separately invoked monochrome and I444 corpus validators fail because
  `images/artifacts-8bit-mono/manifest.json` and
  `images/artifacts-8bit-444/manifest.json` are absent. The public I444
  corpus validator fails on the same absent I444 manifest.
- `make test-reference-images`: all 14
  [target-baseline records](../../images/recipes/target-baseline-reference-lock.json)
  lack their local manifests. All use `le896-i420-qp152`, for sources
  `baya-weaver`, `chinnathurai-church`, `elbrus-milky-way`, `emerald-swift`,
  `finland-warbirds`, `freiburg-minster-columns`, `hong-kong-harbour-night`,
  `jinan-grand-theater`, `pastel-chalks`, `pitti-ceiling`,
  `rome-ponte-blue-hour`, `scarlet-flycatcher`, `succisa-flower-bud`, and
  `toco-toucan`. The retained current-oracle Baya fixture does not replace
  the missing pinned target-baseline Baya reference.

Reproduce the incomplete gates without weakening their requirements:

```sh
make test
python3 -m unittest discover -s tests -p 'test_*.py'
make test-reference-images
python3 tools/validate_8bit_mono_corpus.py \
  --decoder build/host-sanitized/av2heif-decoder --jobs 4
python3 tools/validate_8bit_444_corpus.py \
  --decoder build/host-sanitized/av2heif-decoder --jobs 4
python3 tools/validate_8bit_444_public.py \
  --artifact-tool build/host-sanitized/av2heif-artifact --jobs 4
```

No ignored media were regenerated to turn these failures green. Existing
spatial source-hash witnesses still pass unchanged.

### Platform/runtime limits

M0 ran hosted macOS ARM64 only, plus the freestanding symbol gate. It did not
re-run Linux no-libc runtime, Windows, WebAssembly/browser runtime, 32-bit
layouts, maximum-image decoding, or peak-stack measurements. Historical
cross-platform evidence is not relabeled as a current M0 run. M8 must execute
the relevant platform gates on suitable hosts; a successful cross-build is
not runtime parity.
