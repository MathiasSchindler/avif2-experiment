# Blockiness reduction milestones

This document turns the current visual-quality recommendation into an ordered,
validator-backed implementation plan. The encoder already has production luma
and chroma DCT_DCT AC, directional prediction, Smooth/Paeth, and U/V plane
quantizer deltas, but those tools are exposed by separate search modes. The
next objective is to make those tools cooperate before adding more isolated
coding features.

The recommended order is:

1. combined block-local prediction and AC RD search;
2. broader DCT_DCT residual search;
3. bounded deblocking;
4. limited partition and transform search;
5. CfL and chroma mode search;
6. CDEF and restoration; and
7. additional transform types and advanced coefficient tools.

Full general partition search is deliberately not the first step. The current
fixed traversal, exact syntax writers, and validator-backed predictors should
first be assembled into one additive quality path.

## Admission rules

Every milestone follows these rules:

- Keep the previous production path as an explicit candidate.
- Make every decision deterministic with documented tie-breaking.
- Use exact emitted bytes for final rate decisions.
- Compare active-crop distortion per plane.
- Require encoder-local reconstruction to match the pinned AVM decoder.
- Require pinned AVM and project-decoder output to match byte for byte.
- Preserve caller-owned workspace, static/no-libc operation, and bounded
  memory.
- Retain gates, reports, and real-image streams under
  `experimental/encoder/`.
- Admit a feature only when the locked corpus has no aggregate regression at
  the declared rate boundary.

## M0 - Diagnose the visible artifact — complete

The first milestone determines whether Baya Weaver blockiness is primarily a
residual-capacity problem, a block-boundary problem, or both.

### M0.1 - Lock the real-image inputs

- Use the 1440x960 Baya Weaver I400, I420, and I444 prepared inputs.
- Record input hashes, dimensions, qindex, search mode, encoder hash, and both
  decoder hashes.
- Keep qindex 100 as the continuity point with existing evidence.

### M0.2 - Produce a bounded qindex sweep

- Encode current `--search bounded` at several qindices above and below 100.
- Include points that approximately match the sizes of existing `off`,
  `intra`, and directional streams.
- Record bytes, per-plane SSE, PSNR, runtime, workspace, and output hashes.
- Compare quality at approximately equal bytes, not only at equal qindex.

### M0.3 - Add perceptual diagnostics

- Add a deterministic block-boundary discontinuity metric at coded 4x4, 8x8,
  and 16x16 boundaries.
- Separate boundary measurements from ordinary image gradients so real edges
  are not automatically counted as artifacts.
- Add deterministic SSIM when practical; do not replace SSE or exact hashes.
- Retain representative decoded crops for visual inspection.

### M0.4 - Classify the blocker

- If lower-qindex bounded AC removes the blocks, prioritize residual breadth.
- If boundaries remain despite substantially lower quantization, prioritize
  deblocking and partition/transform adaptation.
- If both improve the image, continue with the milestone order below.
- Repeat the comparison after M1 with combined Smooth-plus-AC candidates.

**Exit:** a frozen report identifies the dominant Baya Weaver failure mode and
contains deterministic local/AVM/project-exact streams at every measured
point.

### M0 result

`make -C experimental/encoder blockiness-m0` freezes 33 Baya Weaver
1440x960 cases in `metrics/blockiness-m0.json`: bounded qindices 60, 80, 100,
120, 150, 165, 180, and 220 plus q100 `off`, `intra`, and `directional`
anchors for I400, I420, and I444. Every stream is deterministic, encoder-local
SSE matches decoded source SSE, and pinned AVM/project reconstruction is byte
exact. All 33 `.obu` files and 36 luma PGM crops are retained under
`metrics/blockiness-m0/`; the report freezes input, stream, decoded-plane, and
tool hashes.

The dominant diagnosis is **residual capacity**. Moving bounded search from
q100 to q60 increases aggregate stream size by 61.63% but improves aggregate
luma SSE by only 3.93%; the lower-quantization 8x8 boundary/interior
gradient-error ratio is 1.068, below the frozen 1.15 boundary blocker. This
means the current eight-position/eight-nonzero/magnitude-63 residual search is
saturating: spending substantially more rate through qindex alone does not
remove proportionate error, while persistent boundary-specific excess is not
yet demonstrated. Proceed with M1 and M2 before bounded deblocking in M3, then
repeat this gate after M1 as required above.

M0 also exposed and fixed a full-size I400 Smooth local-oracle defect. The
emitted square/two-way transform partitions keep extended transform edges
available, but the oracle had capped every Smooth transform to in-block edges.
A locked 32x32 Smooth regression now proves encoder-local, pinned-AVM, and
project reconstruction equality.

## M1 - Combined block-local prediction and AC RD search

Create one additive quality path, provisionally `--search rd`, instead of
choosing between mutually exclusive whole-image AC, intra, directional, and
plane-qdelta modes.

### M1.1 - Transactional candidate state

- Define a candidate transaction for writer position, range state, mutable
  CDFs, coefficient contexts, reconstructed edges, and mode contexts.
- Support trial, rollback, and commit without heap allocation.
- Prove that rollback restores every byte and context field.
- Preserve the existing complete-stream oracle for cross-checking local
  decisions.

### M1.2 - Block-local predictor candidates

- Enumerate DC, admitted directional modes, Smooth, Smooth-V, Smooth-H, and
  Paeth for each legal production leaf.
- Form every predictor from the reconstruction available at that exact decode
  position.
- Keep the current predictor as the first candidate and stable tie winner.
- Do not make chroma mode search part of this step.

### M1.3 - AC residuals for every predictor

- Run the existing DCT_DCT AC search against each luma predictor candidate.
- Reconstruct every candidate before measuring distortion.
- Evaluate skip/DC-only and AC-coded residuals in the same candidate set.
- Keep current bounded coefficient limits during this milestone so predictor
  integration can be validated independently of coefficient expansion.

### M1.4 - Exact RD selection

- Compute distortion from active source and reconstructed samples.
- Derive final rate from exact syntax and range-writer output.
- Use one documented integer RD objective and stable ordering.
- Reject candidates that violate the configured byte or no-regression bound.
- Verify local choices against complete-stream trial results on small locked
  fixtures.

### M1.5 - Add color and plane qdelta

- Preserve the selected luma decisions while coding existing chroma AC.
- Apply U/V plane-qdelta search as an outer complete-stream trial.
- Retain `(0,0)` as the first qdelta candidate.
- Preserve Y reconstruction exactly across qdelta trials.

### M1.6 - Production gate

- Cover I400, I420, and I444, odd dimensions, multiple superblocks, and the
  1440-per-axis ceiling.
- Include patterns where DC, directional, Smooth, Paeth, skip, and AC each
  win at least once.
- Include Baya Weaver and the full prepared corpus.
- Compare `rd` against the best applicable existing search result per case.

**Exit:** one deterministic search combines block-local luma prediction,
DCT_DCT residuals, chroma AC, and frame-level qdelta. It has exact
local/AVM/project parity and Pareto-improves at least one locked real image
without aggregate per-plane regression at equal-or-lower bytes.

### M1 result — complete

`make -C experimental/encoder blocky-m1-full-gate` passes all 27 production
cases: nine targeted cases, the 1440x1440 Smooth ceiling, Baya Weaver
I400/I420/I444, and all 14 selected full prepared images. Every selected
stream is deterministic, encoder-local SSE matches decoded source SSE, and
pinned AVM/project reconstruction is byte exact. The 15 comparable full real
images move from 274,261 to 268,055 bytes and aggregate per-plane SSE from
`[1,080,593,021, 13,183,144, 1,824,526]` to
`[1,049,089,614, 13,183,144, 1,824,526]`.

There are six strict full-image Pareto wins. Baya Weaver I420 and I444 each
save 1,126 bytes and 1,224,282 Y SSE. Chinnathurai Church, Jinan Grand
Theater, Pitti Ceiling, and Succisa Flower Bud provide four additional full
prepared-corpus wins. Emitted winners cover DC, both directional modes, all
Smooth modes, Paeth, skip, DC-only, and AC; their frozen mode counts are
`[11388,3910,3730,4010,2442,2909,1155]` and residual counts are
`[16309,2575,10660]` in the documented stable order. Six color cases execute
all nine qdelta trials, for 54 total, with Y reconstruction preserved.

The locked Scarlet Flycatcher crop remains supporting evidence only: its
800-byte/967,184-SSE RD stream beats bounded at 806 bytes/1,653,563 SSE, but a
crop alone does not satisfy M1.6. Admission instead rests on the complete
Baya/full-prepared gate above. The full run took 2:44:46 wall time, used at
most 34,669,792 caller-owned workspace bytes, and is frozen in
`metrics/blocky-m1-gate.json` (SHA-256
`8228d19e8512853aa80aa9f02085247a20baed0ae2a1f07dad7885eaf2c90fb0`).

M2 is released for implementation by this result, but no M2 code is included
in the M1 admission change.

## M2 - Broader DCT_DCT residual search

Status: **released by M1 admission; not started**.

The current bounded AC search uses eight low-frequency positions, at most
eight nonzero coefficients, and coefficient magnitude 63. Expand those limits
before introducing additional transform kernels.

### M2.1 - Broader scan positions

- Add a staged search beyond the first eight low-frequency positions.
- Validate scan order, EOB classes, contexts, and reconstruction independently
  for every production transform shape.
- Keep the eight-position search as a candidate and compatibility bound.

### M2.2 - More nonzero coefficients

- Raise the per-transform nonzero limit in bounded steps.
- Measure workspace and runtime growth at each step.
- Require exact coefficient-context commits after every trial and selection.

### M2.3 - Greater coefficient magnitude

- Raise the magnitude-63 search bound in bounded steps.
- Exercise base, BR, remainder, sign, and clipping boundaries.
- Add targeted high-contrast and textured fixtures that require larger
  residual levels.

### M2.4 - EOB and coefficient refinement

- Search EOB placement using exact emitted rate.
- Add deterministic coefficient-level RD refinement or a small bounded
  trellis.
- Include zeroing, magnitude increment/decrement, and sign-preserving trials.
- Keep runtime and state growth explicitly capped.

### M2.5 - Real-image gate

- Repeat the M0 Baya Weaver sweep at approximately equal bytes.
- Require lower block-boundary energy or better SSIM in addition to the
  existing SSE and parity checks.
- Report gains separately for luma, I420 chroma, and I444 chroma.

**Exit:** the combined RD path can select broader DCT_DCT residuals with exact
syntax and reconstruction, improves at least one texture-heavy prepared image,
and does not regress the locked aggregate at equal-or-lower bytes.

## M3 - Bounded deblocking

Deblocking directly targets visible transform and prediction boundaries. Add
it before CfL and before the wider restoration toolset.

### M3.1 - Normative filter kernel

- Implement luma horizontal and vertical edge filtering first.
- Add chroma filtering after luma hashes match both decoders.
- Cover frame edges, subsampling, odd dimensions, transform boundaries,
  partition boundaries, and superblock boundaries.
- Keep unfiltered reconstruction available for diagnostics.

### M3.2 - Syntax and local reconstruction

- Add the required sequence/frame syntax without enabling unrelated filters.
- Apply filtering in normative decoder order.
- Produce post-filter local planes and compare their hashes with both
  decoders.
- Keep deblocking off as the first candidate.

### M3.3 - Bounded level search

- Trial a small deterministic shortlist of horizontal and vertical levels.
- Include zero and the current best level in every search.
- Measure exact stream bytes after signaling.
- Select using SSE plus the frozen block-boundary metric; report both inputs
  to the decision.

### M3.4 - Deblocking gate

- Use synthetic isolated edges, flat block grids, textures, and real images.
- Require exact post-filter local/AVM/project hashes for every selected level.
- Require off to win on at least one case and nonzero filtering to win on at
  least one blocking-target case.
- Repeat the Baya Weaver visual crops and metric report.

**Exit:** `off` and bounded nonzero deblocking levels are deterministic search
candidates, all post-filter planes are validator-exact, and at least one
locked blockiness case improves without an aggregate rate-distortion
regression.

## M4 - Limited partition and transform search

Status: **implemented experiment; admission blocked**.

Do not begin with the entire AV2 partition universe. Add a small set of legal
choices around the existing fixed tree and production transform shapes.

### M4.1 - Candidate tree representation

- Represent the current fixed tree as explicit caller-owned candidate state.
- Separate partition syntax, transform syntax, prediction, and reconstruction
  state.
- Freeze the current tree as the first candidate and compatibility oracle.

### M4.2 - Bounded partition alternatives

- Add selected split/no-split choices using already supported legal syntax.
- Start with 16x16 versus 8x8 decisions.
- Add horizontal and vertical rectangular alternatives near directional
  edges.
- Cap candidate count and recursion depth.

### M4.3 - Bounded transform alternatives

- Trial existing 4x4-through-16x16 square and rectangular DCT_DCT shapes.
- Compare 16x16 against 8x8 subdivision where syntax permits.
- Reuse M1 predictor/residual transactions and exact RD selection.
- Commit neighbor, CDF, and coefficient state only for the winner.

### M4.4 - Partition/transform gate

- Include smooth gradients, directional edges, checkerboards, fine textures,
  odd boundaries, and full prepared images.
- Require multiple block and transform sizes to be selected by production
  search.
- Report candidate count, runtime, workspace, bytes, per-plane distortion,
  boundary metric, and selected-tree statistics.

**Exit:** the encoder makes deterministic bounded partition and transform
choices, selects both larger and smaller structures on locked fixtures, and
improves at least one prepared image with exact local/AVM/project parity.

### M4 result — blocked

`--search partition-rd` reuses the M1 live transaction and keeps the admitted
fixed four-8x8 tree first with stable tie wins. Explicit caller-owned shape
state trials a capped 16x16 no-transform-split DCT_DCT alternative at bounded
I400 positions. Native rollback tests cover repeated split/large trials, and
the parity suite covers flat/smooth, directional edge, checker/fine texture,
odd boundaries, and multiple superblocks. Both 8x8 and 16x16 candidates win
during locked synthetic search.

Admission is blocked rather than claimed. Exact decoding exposed context/order
failures when an emitted 16x16 winner was followed by later leaves, and color
transform-tree changes require a matching chroma derivation. The final safety
guard therefore emits admitted `rd` whenever any 16x16 candidate was selected;
I420/I444 keep the fixed tree. On Baya Weaver q100, all final streams match
`rd` exactly: I400 15,356 bytes / Y SSE 52,513,443; I420 19,500 bytes /
`[54,185,612,1,395,987,274,340]`; I444 21,599 bytes /
`[54,185,612,11,787,157,1,550,186]`. Candidate trials were 116,160 /
518,400 / 518,400, with I400 selecting eight 16x16 candidates before the
safety fallback and color selecting none. Exact hashes, SSIM, 8x8 boundary
metrics, runtime, workspace, and dual-decoder parity are frozen in
`metrics/blocky-m4-partition-gate.json`. Because there is no honest Pareto or
objective improvement, M4 remains blocked; M2 and M3 remain unimplemented.

## M5 - CfL and chroma mode search

Add chroma prediction after the combined luma path, broader residuals, and
plane quantizer control provide a trustworthy comparator.

### M5.1 - Chroma mode infrastructure

- Add block-local chroma predictor state and exact mode syntax.
- Admit DC plus the reconstruction-safe directional and Smooth-family chroma
  candidates incrementally.
- Preserve DC as the first candidate.

### M5.2 - CfL predictor

- Implement normative luma subsampling and AC extraction.
- Search legal alpha values independently for U and V.
- Cover I420 and I444 geometry, odd dimensions, and boundary extension.
- Keep CfL off as a candidate for every block.

### M5.3 - Combined chroma RD

- Evaluate predictor, chroma DCT_DCT residual, qdelta, and exact syntax rate
  together.
- Preserve selected luma reconstruction and syntax.
- Report U and V decisions independently.

### M5.4 - Chroma gate

- Include correlated and anti-correlated luma/chroma fixtures, neutral chroma,
  saturated edges, and prepared color images.
- Require DC, non-DC chroma, and CfL wins on targeted cases.
- Require no Y change and no aggregate chroma regression at the declared rate
  boundary.

**Exit:** I420/I444 production search can select DC, admitted chroma modes, or
CfL per block with exact parity and measurable prepared-image chroma benefit.

## M6 - CDEF and restoration

Continue the filter sequence only after bounded deblocking is stable.

### M6.1 - CDEF

- Implement normative direction analysis and filtering kernels.
- Keep CDEF off as the first candidate.
- Add a bounded frame-level strength shortlist.
- Require exact post-CDEF hashes from both decoders.

### M6.2 - Restoration interface

- Add a disabled-by-default restoration-unit representation and bounded
  scratch layout.
- Preserve unfiltered, deblocked, and CDEF outputs for stage diagnostics.
- Admit one restoration type at a time.

### M6.3 - Restoration kernels and search

- Implement each normative kernel independently.
- Search a small deterministic parameter set using exact signaling rate.
- Keep restoration off as a candidate per admitted unit.
- Validate frame edges, unit edges, chroma subsampling, and odd dimensions.

### M6.4 - Filter-chain gate

- Validate each stage alone before validating combinations.
- Require local hashes after each enabled stage where a decoder-equivalent
  observation is available.
- Report SSE, SSIM, boundary metric, bytes, runtime, and workspace.

**Exit:** CDEF and each admitted restoration choice are independently
validator-exact, deterministic, bounded, and selected only when they beat the
off candidate under the declared objective.

## M7 - Additional transforms and advanced coefficient tools

Add transform diversity only after M1 and M4 provide a reliable block-local
search foundation.

### M7.1 - Additional inverse/forward kernels

- Add ADST, flip-ADST, and identity kernels one transform size at a time.
- Validate forward coefficients, inverse reconstruction, clipping, and basis
  orientation against independent vectors.
- Keep DCT_DCT as the first candidate.

### M7.2 - Transform-type syntax and contexts

- Add exact transform-set and transform-type signaling.
- Cover every admitted intra mode, block shape, and reduced-set condition.
- Preserve transactional CDF and neighbor-state rollback.

### M7.3 - Transform-type RD search

- Evaluate admitted transform types inside the M1/M4 candidate transaction.
- Use exact emitted bytes and reconstructed distortion.
- Cap transform candidates by block shape and prediction mode.

### M7.4 - Advanced coefficient tools

- Add one tool at a time, such as secondary transforms, TCQ, or coefficient
  parity hiding.
- Retain the tool-off stream as an exact candidate.
- Add independent syntax, reconstruction, and state-transition vectors before
  production admission.

### M7.5 - Final quality gate

- Rerun the complete prepared corpus and Baya Weaver qindex/size sweep.
- Compare against the best retained result from every earlier milestone.
- Require deterministic exact parity, bounded resources, and no aggregate
  regression at equal-or-lower bytes.

**Exit:** additional transform and coefficient tools are selected by the same
combined block-local RD search, each tool has an exact off fallback, and the
final corpus demonstrates incremental benefit beyond DCT_DCT alone.

## Completion criterion

This plan is complete when Baya Weaver and the locked prepared corpus are
encoded by one additive production search that combines prediction, residual,
qdelta, bounded block structure, and admitted filters. The final report must
show exact encoder-local/AVM/project reconstruction, deterministic streams,
bounded runtime and workspace, and quality comparisons at approximately equal
bytes. A lower qindex alone is not counted as a coding-tool improvement, but
the M0 sweep remains the reference for deciding whether each milestone reduces
the original visible blockiness.