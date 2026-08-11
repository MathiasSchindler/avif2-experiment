# 10-bit image implementation plan

## Purpose

Implement sample-exact 10-bit 4:2:0 and 4:4:4 decoding after the structural
preparation in `10bitplan-prepwork.md`. This plan owns normative high-bit-depth
syntax, sample storage, reconstruction arithmetic, filters, public output, and
release evidence. It does not repeat preparation work that has already landed.

The implementation remains freestanding C11, allocator-free, libc-free, and
free of external production dependencies. Pinned AVM is the primary reference
oracle, but production behavior must be justified by the AV2 specification and
locked evidence rather than copied from incidental AVM implementation details.

## Starting point

Preparation WP0 through WP12 is complete. In particular, the repository now
has:

- a public pixel-format model that distinguishes bit depth, storage width, and
  byte order;
- byte-based public plane layouts and output/workspace sizes;
- sample-based typed-kernel stride contracts;
- checked accounting for one-byte and two-byte sample-backed workspace regions;
- widened Q-index storage and plumbing without 10-bit syntax admission;
- centralized sample-domain constants and explicit `_u8` sample operations;
- deterministic synthetic 10-bit sources and pinned AVM OBU/output references;
- canonical container validation that recognizes 10-bit metadata but keeps the
  public decoder fail-closed; and
- an aggregate release gate proving byte-identical 8-bit I400, I420, and I444
  behavior after preparation.

The remaining hard-coded 8-bit frame gate, pixel-format admission, workspace
binding, sample structures, reconstruction kernels, and filter kernels are an
intentional implementation boundary. They are not evidence that preparation
is incomplete.

## Output contract

The public 10-bit outputs are tightly packed headerless planar YUV:

- `yuv420p10le`: full-resolution Y followed by half-resolution U and V, using
  ceil-divided chroma dimensions for odd luma dimensions;
- `yuv444p10le`: full-resolution Y, U, and V; and
- each sample stored as an unsigned 16-bit little-endian word whose value is in
  the inclusive range 0 through 1023.

Plane offsets, row strides, capacities, workspace sizes, and API buffer sizes
are bytes. Typed reconstruction and filter kernels use `uint16_t` samples and
sample-count strides. No API or internal contract may call this output I420 or
I444 without also identifying its 10-bit little-endian storage.

## Implementation rules

- Make one format/domain dispatch decision at an owning boundary, then call
  typed `_u8` or `_u16` operations. Do not index sample planes through `void *`
  and do not add per-sample callbacks to normative inner loops.
- Keep entropy CDFs, syntax values, modes, readiness maps, filter controls, and
  other non-sample state narrow unless their normative numeric domain requires
  widening.
- Introduce each 16-bit owner with an exact 10-bit vector. Do not duplicate or
  parameterize the entire decoder before a reached path needs it.
- Treat prediction, transform, reconstruction, and filter intermediate widths
  as normative arithmetic. A wider final store does not by itself make an
  8-bit kernel correct at 10-bit.
- Keep 8-bit behavior on its existing typed path. Shared helpers are appropriate
  only where the operation and rounding rules are actually depth-independent.
- Change 10-bit frame admission and the 9-bit base-Q-index read atomically. An
  8-bit read in a high-bit-depth frame shifts all following frame syntax.
- Keep unsupported 12-bit, 4:2:2, restoration, film grain, segmentation, and
  multiframe forms fail-closed at their current owning boundaries.
- Prefer scalar sample-exact code until both 10-bit formats pass their release
  gates. Performance specialization is later work.

## Evidence strategy

The compact matrix under `tests/fixtures/10bit/avm-reference/` is the primary
implementation corpus. The first stream is the separately constrained
`tiny-constrained-16x16-yuv420p10le-qp208`: it has two observed coding blocks,
DC luma and chroma prediction, DCT_DCT transforms, base Q-index 256, and all
unrelated sample tools and filters explicitly disabled. Its pinned decoded
output is 768 bytes. `docs/dev/10bit-wp0.md` records the complete contract and
explains why the earlier QP 144 candidate was rejected.

Expansion should use the retained synthetic matrix in increasing ownership
order:

1. tiny constrained 4:2:0;
2. tiny quantizer and transform variants;
3. prediction, CfL, cross-component, and filter-bearing variants;
4. odd geometry and boundary extremes;
5. high-detail synthetic input;
6. photographic 4:2:0 artifacts; and
7. the corresponding 4:4:4 matrix.

Each parity test compares exact little-endian samples and records useful
intermediate checkpoints. A final-file hash alone is sufficient for a release
gate but insufficient for diagnosing the first arithmetic divergence.

## 10B-WP0: Lock the first vertical-slice oracle

**Status:** Implemented on 2026-08-11. The constrained fixture, exact planar
oracle, block/syntax/filter trace, first-mismatch comparator, regeneration
command, and current unsupported/nonmutation check are recorded in
`docs/dev/10bit-wp0.md`.

**Objective:** Turn one constrained 10-bit 4:2:0 stream into a precise failing
test before production admission changes.

**Work:**

- Pin the selected OBU, decoded output, hashes, dimensions, plane offsets, and
  expected 768-byte output size.
- Record sequence-header and frame-header endpoints, the 9-bit base Q-index,
  tile entropy endpoint, block order, transform descriptors, prediction modes,
  and active filter state.
- Add a private differential harness that can compare Y, U, and V separately
  and report the first mismatching sample.
- Add negative controls proving that the current public path still reports
  unsupported before output mutation.
- Document which coding tools are absent or identity operations in this stream;
  do not infer absence only from a census omission.

**Exit gate:** Complete. The test fails solely at the documented 10-bit support
boundary, the AVM oracle is reproducible, and every syntax and filter
assumption needed for the first slice is explicit.

## 10B-WP1: Admit 10-bit 4:2:0 syntax and format metadata

**Status:** Implemented on 2026-08-11. The profile-0 10-bit 4:2:0
descriptor, 9-bit Q-index parsing, exact frame/tile endpoints, negative syntax
controls, public query contract, and deliberate workspace rejection are
recorded in `docs/dev/10bit-wp1.md`.

**Objective:** Parse the constrained stream without syntax desynchronization
while keeping reconstruction and public decode deliberately unavailable.

**Work:**

- Add the valid 10-bit, two-byte, little-endian pixel-format descriptor.
- Extend pixel-format validation and plane-layout tests for 10-bit 4:2:0.
- Admit only the normative sequence profile/chroma/depth combination selected
  for this phase.
- Read the high-bit-depth base Q-index with its normative 9-bit width and carry
  the complete value through frame, tile, deblock, and dequantization state.
- Revalidate every following frame-header endpoint against the pinned oracle.
- Retain explicit rejection for 12-bit, 4:2:2, unsupported profile/depth/chroma
  combinations, quantization matrices, and lossless base-Q-zero frames.

**Exit gate:** Complete. The first stream reaches the exact frame and tile
payload boundaries with base Q-index 256, malformed/truncated variants fail at
the owning parser, and public query exposes its format metadata while decode
remains explicitly unsupported and nonmutating at workspace binding.

## 10B-WP2: Bind two-byte sample storage

**Status:** Implemented on 2026-08-11. Typed U16 reconstruction, filter,
scratch, and output owners; byte-aware initialization; alignment and extent
validation; sample operations; and the deliberate reconstruction rejection are
recorded in `docs/dev/10bit-wp2.md`.

**Objective:** Convert prepared two-byte size accounting into typed, bounded
workspace and output bindings.

**Work:**

- Add a byte-width-aware frame-workspace initializer matching the existing
  byte-width-aware query.
- Bind `uint16_t` reconstructed planes, predictor scratch, reference samples,
  filter stage planes, and final output without widening control maps or
  coefficient state.
- Define and enforce alignment for every 16-bit sample-backed allocation.
- Introduce typed `_u16` fill, copy, crop, reference-load, predictor-commit,
  reconstructed-load, clip/store, and direct load/store operations beside the
  `_u8` operations.
- Preserve byte-based API strides and sample-based typed-kernel strides at each
  conversion boundary.
- Add one-byte/two-byte workspace canaries, exact-size success, one-byte-short
  failure, alignment failure, overlap failure, and arithmetic-overflow tests.

**Exit gate:** Complete. Query and initialization agree exactly for both sample widths;
all sample-backed regions are two-byte sized and aligned in the 10-bit case;
unrelated maps retain their previous sizes; and every 8-bit workspace size is
unchanged.

## 10B-WP3: Implement the 10-bit sample and quantizer domain

**Status:** Implemented on 2026-08-11. The production 10-bit sample domain,
depth-aware Q-index and dequantizer rules, DC/AC delta composition, coefficient
saturation, lossless predicate, deblock quantizer inputs, signed intermediate
contracts, and exact boundary vectors are recorded in
`docs/dev/10bit-wp3.md`.

**Objective:** Establish normative constants, quantization behavior, clipping,
and intermediate ranges before broad reconstruction work.

**Work:**

- Add the 10-bit sample domain with midpoint 512 and maximum 1023.
- Implement high-bit-depth quantizer/dequantizer lookup or derivation from the
  pinned specification, including Q-index values above 255.
- Audit DC/AC deltas, chroma Q-index derivation, lossless detection, coefficient
  saturation, and dequantized coefficient bounds.
- Define explicit signed intermediate types for dequantization, transforms,
  prediction sums, and reconstruction adds.
- Add boundary vectors around Q-index 255/256 and sample values 255/256,
  511/512, and 1022/1023.
- Keep the established 8-bit quantizer functions and results byte-identical.

**Exit gate:** Complete. Focused vectors match specification-derived and pinned-AVM
values across the retained 10-bit Q-index range, sanitizers report no overflow
or invalid shift, and the 8-bit quantizer suite remains exact.

## 10B-WP4: Reconstruct the first 10-bit luma plane

**Status:** Implemented. The constrained U16 luma owner, depth-aware reached
inverse transform, exact block checkpoints, integrated private-frame boundary,
and validation evidence are recorded in `docs/dev/10bit-wp4.md`.

**Objective:** Produce sample-exact unfiltered Y for the constrained fixture.

**Work:**

- Add typed 16-bit luma reference collection and default-reference values.
- Implement the reached DC prediction path in the 10-bit domain.
- Audit primary inverse-transform scaling, rounding, coefficient clamps, and
  reconstruction clipping for the reached DCT_DCT sizes.
- Commit reconstructed samples and readiness state through typed 16-bit plane
  operations.
- Record block-local predictor, residual, and reconstructed hashes or sample
  checkpoints to localize parity failures.

**Exit gate:** Complete. Every reached luma block and the complete visible
unfiltered Y plane match pinned AVM sample-for-sample, tile entropy ends at
tell 192, and prediction uses only reconstructed neighbors or the normative
10-bit midpoint.

## 10B-WP5: Reconstruct the first 10-bit 4:2:0 frame

**Status:** Implemented. Typed U16 chroma references, the reached 8x4
DC/DCT_DCT path, exact unfiltered U/V parity, tightly packed private output,
and the retained public filter boundary are recorded in
`docs/dev/10bit-wp5.md`.

**Objective:** Extend the constrained slice through U and V while preserving
the existing 4:2:0 geometry model.

**Work:**

- Add typed 16-bit chroma references and reached chroma prediction paths.
- Audit luma-to-chroma coordinate conversion, coded-edge extension, odd-size
  ceil division, transform ownership, and U-to-V context dependencies.
- Apply 10-bit chroma quantization, inverse transforms, clipping, and plane
  commits.
- Produce tightly packed private `yuv420p10le` output with exact Y/U/V offsets
  and little-endian sample representation.
- Compare each unfiltered plane and its visible crop independently.

**Exit gate:** Complete. The selected fixture reaches tile-decode completion;
its complete unfiltered Y, U, and V planes and tightly packed private
`yuv420p10le` output match pinned AVM exactly.

## 10B-WP6: Expand prediction and transform coverage

**Status:** Implemented. The retained 22-fixture tiny matrix, exact
pre-filter Y/U/V parity, coded-lossless DPCM/FSC and reversible transform
paths, bounded U16 prediction dispatch, and unchanged public filter boundary
are recorded in `docs/dev/10bit-wp6.md`.

**Objective:** Cover the retained 4:2:0 prediction and transform matrix before
filter integration obscures reconstruction failures.

**Work:**

- Add reached directional, cardinal, Smooth, Paeth, IBP, MRL, palette, and
  IntraBC 16-bit prediction paths owner by owner.
- Implement 10-bit CfL, MHCCP, CCTX, FSC, and cross-chroma behavior with
  explicit signed intermediates and depth-dependent offsets.
- Audit primary and secondary inverse transforms for depth-dependent shifts,
  rounding, output-position maps, clamps, and large-transform intermediates.
- Retain bounded dispatch and fail closed for any reached transform kernel that
  lacks a locked 10-bit vector.
- Add tiny quantizer/transform variants before odd or photographic fixtures.

**Exit gate:** Complete. All 22 retained tiny 10-bit 4:2:0 fixtures match
pinned AVM on complete unfiltered planes. The matrix reaches seven transform
geometries and nine primary transform types; focused unit vectors retain the
specialized prediction, secondary-transform, CfL, CCTX, FSC, palette, and
IntraBC arithmetic needed by the current release corpus.

## 10B-WP7: Implement 10-bit deblock

**Status:** Implemented. Typed U16 edge arithmetic, shared deblock metadata,
odd-geometry and boundary vectors, and exact retained AVM stage parity are
recorded in `docs/dev/10bit-wp7.md`. Public 10-bit decode remains at the
nonmutating filter boundary until the later mandatory stages are complete.

**Objective:** Make the first mandatory filter stage sample-exact without
changing syntax/control storage.

**Work:**

- Derive bit-depth-adjusted thresholds and clipping from normative formulas.
- Audit edge selection, transform metadata, Q-index use, signed differences,
  rounding, and strong/weak filter intermediates.
- Add horizontal/vertical, luma/chroma, frame-edge, odd-geometry, minimum,
  maximum, and no-op vectors.
- Compare stage output against recorded AVM deblock evidence before enabling
  later stages.

**Exit gate:** Every retained deblock vector and selected full-frame deblocked
plane matches AVM exactly at both sample widths.

## 10B-WP8: Implement 10-bit CDEF and CCSO

**Status:** Implemented. Typed U16 CDEF direction/filter arithmetic, typed
CCSO classification and stores, bounded deblock-to-CCSO dispatch, odd-geometry
vectors, and exact retained AVM stage parity are recorded in
`docs/dev/10bit-wp8.md`. Public 10-bit decode remains at the nonmutating filter
boundary until WP9 completes GDF and final output.

**Objective:** Port the mandatory directional and cross-component filter
arithmetic to the 10-bit sample domain.

**Work:**

- Audit CDEF direction search, primary/secondary taps, constrain operations,
  signed rounding, damping, clipping, and zero-strength behavior.
- Audit CCSO source classification, luma/chroma sample access, band/edge
  offsets, unit controls, signed additions, and clipping.
- Keep CDEF indices and CCSO controls narrow and retain their existing unit
  geometry.
- Add isolated boundary vectors and stage-local full-frame comparisons.

**Exit gate:** Complete. Seven CDEF-only, one CCSO-only, and two combined
filter-bearing tiny 4:2:0 outputs match pinned AVM exactly. Focused vectors
retain boundary-extreme arithmetic, controls, clipping, and odd visible/coded
geometry.

## 10B-WP9: Implement 10-bit GDF and final private output

**Status:** Implemented. Typed U16 GDF arithmetic, bounded mode controls,
disjoint caller-owned stage buffers, normative full-filter dispatch, delayed
visible crop, exact private raw output, and retained AVM evidence are recorded
in `docs/dev/10bit-wp9.md`. Public HEIF admission remains at the nonmutating
filter boundary for WP10.

**Objective:** Complete the accepted-profile filter pipeline and visible crop.

**Work:**

- Audit GDF box sums, variance, coefficients, bit-depth reduction, signed
  rounding, block flags, and sample clipping.
- Preserve raw and bit-depth-reduced sums as distinct values where required by
  the normative calculation.
- Validate stage-buffer disjointness and coded-plane padding for two-byte
  samples.
- Run deblock, CDEF, CCSO, and GDF in normative order, then crop visible planes
  into exact little-endian output.
- Keep restoration unsupported and reject it before output mutation.

**Exit gate:** Complete. All 22 supported tiny 10-bit 4:2:0 streams match AVM
on final private output; retained active-GDF stage evidence and direct scalar
oracles lock GDF behavior; restoration and undersized-output failures preserve
caller output.

## 10B-WP10: Enable canonical public 10-bit 4:2:0 decode

**Objective:** Expose the completed private path through canonical HEIF query
and decode contracts.

**Work:**

- Accept consistent 10-bit `av2C`, Sequence Header, and three-channel `pixi`
  metadata for the supported 4:2:0 profile.
- Report exact `yuv420p10le` format, plane byte offsets/strides/sizes, total
  output size, workspace size, and workspace alignment.
- Preserve invalid-data diagnostics for metadata mismatches and unsupported
  diagnostics for recognized but unimplemented formats.
- Verify immutable query-to-decode binding, overlap handling, stale inputs,
  exact buffer bounds, every shorter input prefix, and no preflight mutation.
- Update hosted tools and platform adapters to identify and preserve 16-bit
  little-endian planar output without implicit downconversion.

**Exit gate:** Canonical tiny 10-bit 4:2:0 HEIF decodes sample-exactly through
the public API; malformed, stale, overlapping, truncated, and undersized cases
fail deterministically; and all public 8-bit contracts remain unchanged.

## 10B-WP11: Complete 10-bit 4:2:0 corpus coverage

**Objective:** Expand from the vertical slice to the synthetic and photographic
4:2:0 release matrix.

**Work:**

- Add odd geometry, boundary extremes, high detail, retained Q-index sweeps,
  CfL/prediction stress, transform stress, and filter stress.
- Decode the project-authored photographic `yuv420p10le` artifacts at the
  retained dimensions and quantizers.
- Require exact native, public canonical-HEIF, Windows, and WebAssembly planar
  parity where those environments are available.
- Run sanitizer, strict freestanding, undefined-symbol, Linux no-libc, output
  arithmetic, and two-width regression gates.
- Add public-boundary fuzz seeds for both sample widths and the new metadata
  combinations.

**Exit gate:** The complete supported 10-bit 4:2:0 matrix matches pinned AVM
sample-for-sample; all 8-bit I400/I420/I444 bytes remain unchanged; and release
artifacts retain their freestanding dependency contract.

## 10B-WP12: Integrate the 10-bit 4:4:4 sample model

**Objective:** Combine the proven 10-bit sample domain with the proven 4:4:4
geometry as an independently validated format.

**Work:**

- Admit only the normative 10-bit 4:4:4 profile/signaling combination.
- Construct and report tightly packed `yuv444p10le` layouts with three
  full-resolution 16-bit planes.
- Re-audit full-resolution chroma block/transform ownership, neighbor
  availability, directional references, coded-edge extension, and U/V context
  dependencies.
- Re-audit CfL, MHCCP, CCTX, FSC, palette, and IntraBC where sample width and
  full-resolution chroma geometry meet.
- Fold substantial format-specific branches back into the established geometry
  or typed sample-owner abstractions instead of creating a fourth decoder path.

**Exit gate:** Constrained tiny and odd 10-bit 4:4:4 fixtures match AVM on
unfiltered Y, U, and V while 4:2:0, monochrome, and every 8-bit format remain
exact.

## 10B-WP13: Complete 10-bit 4:4:4 filters and public output

**Objective:** Finish mandatory filters and canonical HEIF exposure for
full-resolution 10-bit chroma.

**Work:**

- Validate full-resolution deblock metadata and traversal for U and V.
- Validate CDEF direction/strength behavior, CCSO source mapping and unit
  controls, and GDF grids at full chroma resolution.
- Add exact public layout, workspace, alignment, overflow, truncation,
  nonmutation, and metadata-consistency tests for `yuv444p10le`.
- Update platform and hosted output paths without adding production RGB
  conversion or lossy downconversion.
- Run tiny, odd, boundary, high-detail, and photographic 4:4:4 parity.

**Exit gate:** Canonical 10-bit 4:4:4 fixtures match pinned AVM after every
filter and through the public API, including the maximum retained dimensions.

## 10B-WP14: 10-bit image release gate

**Objective:** Prove that both new sample/chroma combinations are complete,
bounded, portable, and regression-free within the accepted profile.

**Work:**

- Run the complete 8-bit and 10-bit private/public fixture matrices.
- Run sanitizer, fuzz, strict freestanding, undefined-symbol, Linux no-libc,
  Windows cross-build/runtime where available, and WebAssembly parity gates.
- Recheck public ABI expectations, format descriptors, exact workspace/output
  sizes, maximum-dimension arithmetic, and generated artifact reproducibility.
- Document supported profiles, byte order, plane packing, caller alignment,
  unsupported combinations, diagnostics, and platform limitations.
- Record final corpus hashes and per-platform evidence in a development report.

**Exit gate:** All supported I400, I420, I444, `yuv420p10le`, and
`yuv444p10le` outputs are sample-exact against their locked references; every
unsupported depth/chroma/tool combination fails closed; and production builds
remain CC0, allocator-free, freestanding, and free of vendored dependencies.

## Non-goals

- 12-bit samples.
- 4:2:2 chroma.
- Lossless base-Q-zero unless scheduled as a separate syntax/arithmetic slice.
- Restoration, film grain, sequence or frame segmentation, and new tile forms.
- Production RGB conversion, color management, or implicit 8-bit previews.
- Alpha, grids, metadata expansion, and multiframe/video behavior.
- SIMD or architecture-specific high-bit-depth kernels before scalar release
  parity is complete.

## Completion boundary

Completion of 10B-WP11 changes the supported public profile to include 10-bit
4:2:0. Completion of 10B-WP14 additionally includes 10-bit 4:4:4. Neither gate
implies support for 4:2:2, 12-bit, lossless, restoration, or other excluded
coding tools.

Later work should add one independently testable axis at a time. Any substantial
format branch discovered during 10-bit 4:4:4 integration should be resolved in
the shared geometry or typed sample-owner design before another pixel format is
added.