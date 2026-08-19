# Spatial-still 8-bit decoder plan

**Status:** active implementation plan as of 19 August 2026

This plan extends the decoder from its current admitted 8-bit still subsets to
the `spatial-still-8bit` profile defined in
[`../../images/featurematrix.md`](../../images/featurematrix.md). The target is
broad spatial syntax inside a narrow structure: exactly one independently
decodable closed-loop key picture, 8-bit I400, I420, or I444, with no temporal
reference state.

The authoritative description of what the production decoder supports remains
[`../capabilities.md`](../capabilities.md). Completing an intermediate milestone
must not widen that contract until the milestone's exact-parity, malformed-input,
resource, and platform gates pass.

## Baseline and evidence

The decoder already supports most of the spatial coding foundation: extended
and uneven partitions, SDP, the admitted intra prediction modes, IntraBC,
transform partitioning, reached primary and secondary transforms, CCTX,
single-picture TCQ, delta-Q, non-extended ALT_Q segmentation, predefined
non-segmented quantization matrices, and the admitted deblock, CDEF, CCSO, GDF,
PC-Wiener, and Wiener-nonsep paths.

The first concrete blocker is Sequence Header extended segmentation. The parser
records `enable_ext_seg` and then rejects it in `src/carriage.c`. Downstream
state is fixed at eight segments, frame segmentation accepts only ALT_Q, and
quantization stores only one Y/U/V matrix tuple.

Four ignored pinned-AVM pilot cases provide the first integration evidence:

| Case | Format | QP |
| --- | --- | ---: |
| Baya Weaver | I420 | 128 |
| Baya Weaver | I420 | 152 |
| Baya Weaver | I420 | 192 |
| Scarlet Flycatcher | I420 | 152 |

They live under
`images/reference/avm-966a7d7cd6fcf60360caf5dc413b2aeeb65e144d/spatial-still-8bit/`.
They are diagnostic vectors, not the normative profile boundary. Encoder flags
do not prove that syntax was emitted, and exact parity on these four streams is
not sufficient to advertise the profile.

All reference output must use pinned AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. Each retained case must bind the
source, encoded OBU, decoded planes, dimensions, pixel format, encoder command,
AVM binary hashes, and output hashes.

## Milestone 0: reached-syntax accounting

**Status:** implemented on 19 August 2026.

Establish a reproducible way to identify each concrete stream's next rejected
syntax without weakening production admission.

- Add stage and reason diagnostics for Sequence Header, Frame Header, tile,
  leaf, coefficient, reconstruction, and filter rejection.
- Keep diagnostics caller-owned or test-only; do not introduce process-global
  mutable decode state.
- Record parsed feature values and entropy endpoints needed to distinguish an
  unsupported tuple from parser desynchronization.
- Add a runner that compares production output and stage checkpoints with the
  pinned AVM evidence.
- Freeze the first blocker and subsequent reached blockers for all four pilot
  streams in focused tests.

**Exit gate:** all four pilots report deterministic, source-bound failure
frontiers; malformed truncations preserve the correct public status and do not
mutate output beyond the documented boundary.

`Av2hError` now carries a caller-owned diagnostic stage, reason, stable syntax
identifier, parsed value, OBU byte offset, entropy tell, plane, and block
coordinates. Sequence and Frame Header parsers assign named syntax frontiers;
tile, leaf, coefficient, reconstruction, and filter failures are normalized
from the existing private frame result without process-global state. The hosted
decoder emits the same data as one JSON record with `--diagnostics`.

`make spatial-still-frontiers` binds all four pilots to
`tests/spatial-still-frontiers.json`, pinned AVM revision and encoded/output
SHA-256 values, final entropy tells, and luma/chroma leaf counts. It also makes
a canonical length-delimited truncation inside each frame payload. All four
truncations reach a deterministic leaf frontier, return `AV2H_TRUNCATED`, and
create no output file. Public decode tests additionally cover every container
prefix and require the caller's output sentinel to remain unchanged. Exact
final-plane hashes are the production-versus-pinned-AVM output checkpoint;
intermediate filter-plane evidence remains owned by the existing filter
evidence runner rather than duplicated here.

## Milestone 1: extended segmentation

**Status:** implemented on 19 August 2026 for extended signaling, 16 segment
IDs, ALT_Q, and segment-adjusted lossless routing.

Implement the complete still-applicable extended-segmentation syntax rather
than merely bypassing the current sequence admission check.

- Parse Sequence Header extended-segmentation and sequence-level segmentation
  information with validated feature values.
- Expand sequence, frame, leaf, CDF, and segment-map state from 8 to 16
  segments where extended segmentation applies.
- Implement the corresponding segment-ID alphabet, CDF initialization and
  adaptation, predictor, negative deinterleave, and bounds.
- Preserve the existing non-extended eight-segment ALT_Q bitstream behavior.
- Implement all segmentation features applicable to an independently decoded
  key picture, including their signedness, range, precedence, and lossless
  interactions; reject temporal or inter-only features outside the profile.
- Apply per-segment state consistently to quantization, transform/lossless
  decisions, prediction, reconstruction, and filtering.
- Account segment maps in caller-owned workspace with checked dimensions and
  allocation arithmetic.

**Exit gate:** focused 1-, 8-, 9-, and 16-segment vectors pass, including edge
IDs, predictor contexts, inactive segments, sequence/frame signaling choices,
ALT_Q limits, and malformed feature combinations. Existing ALT_Q fixtures and
the complete admitted reference corpus remain exact.

The implementation uses 16-entry sequence/frame state, dynamic 8/16 header
loops, and AVM's extension-flag plus lower/upper 8-symbol adaptive CDFs. Direct
entropy vectors cover IDs 8 and 15, all three spatial contexts, predictor edge
behavior, invalid bounds, and the one-segment case. Block-local luma/chroma
syntax, transform geometry, and commit scheduling use the segment-adjusted Q
index, including mixed lossy/lossless pictures. The four pinned I420 pilots and
the existing eight-segment AQ fixture decode byte-exactly under ASan/UBSan.

## Milestone 2: segmented quantization matrices

**Status:** implemented on 19 August 2026 for one through four picture QM
tuples, per-segment selection, and bounded custom-QM OBUs.

Generalize quantization-matrix parsing and application to the syntax enabled by
segmentation.

- Parse the picture QM count and its dependent index width.
- Store all signaled Y/U/V predefined-matrix tuples rather than one global
  tuple.
- Parse and validate each segment's matrix selection.
- Select the matrix from the decoded segment ID for every transformed block and
  plane.
- Cover separate U/V delta-Q, block delta-Q, segment ALT_Q, TCQ, transform
  skip, FSC, and lossless precedence.
- Retain predefined matrix ID 15 as flat and preserve existing non-segmented
  matrix parity.

Custom quantization-matrix OBUs are part of the complete profile but are a
separate data-ingestion step within this milestone. Their parser must validate
matrix dimensions, coefficient ranges, references, duplicate definitions, and
workspace size before exposing a matrix to dequantization.

**Exit gate:** exact vectors cover one through four QM tuples, per-segment
selection on all admitted planes, flat and custom matrices, separate U/V
selection, rectangular transforms, and malformed or missing matrix references.

The implementation follows the pinned AVM syntax: segmented pictures signal a
two-bit count minus one, non-segmented pictures retain the exact one-tuple bit
layout, and only non-lossless segments consume a matrix index. Luma and chroma
coefficient paths select that tuple from the decoded segment ID; ID 15 remains
flat, while IDTX/FSC and lossless paths bypass weighting as before.

Type-22 OBUs use fixed caller-owned frame state for the 5,760-byte worst-case
fundamental payload. The parser validates the 15-bit ID map, duplicate IDs,
plane copies, 8x8 symmetry, 4x8 transpose references, signed coefficient
deltas, repeat-to-end markers, trailing bits, and chroma-plane availability.
Custom fundamentals weight 4x4, 8x8, 8x4, and 4x8 transforms; larger adjusted
transforms retain the pinned predefined matrices. Hosted ASan/UBSan units, the
1,206-case public fuzz smoke, predefined-QM/AQ/delta-Q parity, forced
freestanding and no-libc builds, and all 102 tracked reference locks pass.

## Milestone 3: coefficient and spatial-tool completion

**Status:** implemented on 19 August 2026.

Close coding-tool gaps that become reachable after segmentation and segmented
QM support.

- Implement parity-hiding coefficient syntax and its legal interaction with
  ordinary coefficient coding and TCQ.
- Audit and complete chroma FSC for I420 and I444, including CfL/MHCCP,
  transform-skip, CCTX, and chroma transform geometry interactions.
- Replace reached-tuple allow lists with bounded normative dispatch where the
  specification defines complete tables or algorithms.
- Cover every legal primary, secondary, cross-component, transform-type, and
  transform-partition choice in the profile envelope.
- Complete still-applicable prediction and IntraBC geometry combinations.
- Keep impossible symbols, unavailable references, coefficient overflow, and
  unsupported out-of-profile syntax fail-closed before unsafe mutation.

The current pilot encodes use single-picture TCQ, for which pinned AVM forces
parity hiding off. Therefore pilot parity does not discharge the parity-hiding
requirement.

Parity hiding now follows the pinned frame and transform gates: it is disabled
for coded-lossless pictures, TCQ, IDTX/FSC, chroma, and transforms with at most
four coded coefficients. Eligible luma blocks decode scan position zero after
the remaining coefficients, derive its parity from capped base levels, and use
the dedicated hidden-base, hidden-DC-sign, and even high-range paths. The
retained 16x16 I444 fixture enables parity hiding with TCQ disabled and matches
pinned AVM exactly; public decode also locks its workspace, output layout, and
truncation behavior.

The existing bounded transform, reconstruction, and prediction matrices cover
the admitted primary and secondary transforms, FSC/IDTX geometries, CfL and
MHCCP prediction, CCTX nonzero propagation, transform partitions, and IntraBC
reference geometry. Sequence-level parity advertisement no longer rejects FSC
or chroma blocks, where the parity syntax is inapplicable. The four spatial
pilots remain sample-exact and retain deterministic truncation frontiers.

**Exit gate:** individual-family vectors and representative cross-products are
sample-exact for I400, I420, and I444; sanitizer and bounded fuzz gates cover
coefficient counts, scan bounds, transform geometry, and prediction references.

## Milestone 4: complete 8-bit restoration

**Status:** implemented on 19 August 2026; the profile exit gate is complete.

Extend restoration from the currently admitted PC-Wiener and Wiener-nonsep
subsets to every still-applicable 8-bit combination.

- Parse and execute switchable restoration and all legal per-plane and per-unit
  tool selections.
- Support legal luma and chroma classes for I400, I420, and I444.
- Validate restoration-unit geometry, tile boundaries, class counts, filter
  references, and frame-level versus per-unit filter selection.
- Preserve normative filter order and immutable source stages where later
  filters classify against earlier output.
- Extend checked workspace accounting for worst-case unit maps, coefficients,
  padded sources, and destination planes.

**Exit gate:** stage-local and final-plane hashes match pinned AVM for each
restoration family and representative switchable combinations, with malformed
unit maps and parameters rejected before destination mutation.

Switchable luma restoration now uses the pinned AVM binary-search syntax and
adaptive CDF defaults, stores the selected tool for every unit, and composes
PC-Wiener before Wiener-nonsep without allowing either stage to overwrite units
owned by the other. Fixed-tool syntax and CDF behavior remain unchanged.
I400, I420, and I444 unit geometry is admitted; chroma Wiener uses format-aware
luma geometry, while switchable restoration remains correctly restricted to
luma.

Top-level filter validation now checks every active plane's unit dimensions,
count, tap storage, and legal fixed or switchable tool references before any
destination stage is mutated. The added switchable CDF state is covered by the
64-bit layout lock (70,072-byte `Av2hTileDecoder`), checked frame-workspace
accounting, ASan/UBSan tests, forced-freestanding compilation, and Linux
no-libc parity.

Pinned-AVM evidence includes exact existing PC-Wiener and I420 Wiener-nonsep
vectors, an exact 896x598 I400 Wiener-nonsep vector with 35 units, and an exact
320x320 switchable vector whose 3x3 luma map contains eight Wiener-nonsep units
and one PC-Wiener unit. Three length truncations of the switchable stream fail
without creating output. The switchable final-plane SHA-256 is
`05106be4786a46ffb63e4b0b59246427bce975b2caced87b56ef65149e226f87`;
the I400 hash is
`52728f282547295784ea853108f53744befad48a319559c183fd6e02cd830cb5`.

The I444 reconstruction blocker was a chroma reference-availability error:
64-sample transforms gathered a bottom-left smooth extension even though AVM
disallows chroma top-right and bottom-left extensions beyond 32 samples. The
8-bit and high-bit-depth gathering paths now enforce that limit, with focused
64x64 smooth and directional coverage. A fully filter-free 896x598 I444 control
is sample-exact on all planes with SHA-256
`3cd80fc776fb414b7a388632dd55eb57c9e78067b9c1c55a72472d5573727af8`;
the corresponding all-plane Wiener vector is sample-exact with SHA-256
`e8190c3eba2d019e6f28adaf8ac33f92751c23e5e8257c17216f9b22dff4e883`.

## Milestone 5: spatial tiles and tile groups

Generalize the current one-tile parser to all legal one-picture spatial tile
layouts within documented resource limits.

- Parse Sequence Header tile information, uniform and non-uniform tile layouts,
  tile groups, tile sizes, and context-update signaling applicable to the
  profile.
- Decode each tile with the required entropy reset and independently bounded
  payload reader.
- Apply prediction, IntraBC, loop-filter boundary, and restoration rules at tile
  edges.
- Support multiple tile groups without introducing temporal frame state.
- Define checked limits for dimensions, tile count, group count, and workspace;
  resource limits must be distinct from unsupported-syntax failures.
- Keep single-tile output and workspace behavior stable.

**Exit gate:** one-tile, multi-tile, non-uniform, and multi-group vectors decode
exactly; reordered, overlapping, truncated, duplicate, and oversized tile data
fail closed. Parallel tile execution is optional and must not be required for
profile conformance.

## Milestone 6: lossless, crop, and signaling closure

Prove the profile envelope beyond ordinary lossy I420 photographs.

- Cover every legal base-Q-zero combination and the required disabling or
  bypass behavior for transforms, quantization, and filters.
- Validate coded versus display dimensions and legal cropping without exposing
  padding samples in public output.
- Complete legal 8-bit color-description and chroma-location signaling for
  I400, I420, and I444.
- Verify odd dimensions, minimum and maximum admitted dimensions, superblock
  sizes, and plane geometry.
- Reject 4:2:2, 10/12-bit, film grain, temporal pictures, multiple operating
  points, and other out-of-profile structures explicitly.

**Exit gate:** lossless output is sample-exact across all three chroma formats;
crop and color metadata round-trip through query/decode; boundary and malformed
vectors pass under sanitizers and strict workspace caps.

## Milestone 7: conformance and release admission

Turn the implemented feature set into a durable public capability rather than
a corpus-specific exception.

- Complete the planned recipe matrix: baseline QPs, I400/I444 format coverage,
  QP spectrum, multiple image classes, tile layouts, lossless, and adversarial
  dimensions.
- Add independent or hand-authored vectors for syntax not emitted reliably by
  pinned AVM.
- Cover individual features plus representative cross-products, especially
  segmentation with QMs, delta-Q, TCQ/parity hiding, FSC/CCTX, IntraBC,
  restoration, tiles, and lossless.
- Add mutation and truncation tests at every variable-length syntax boundary.
- Lock exact native output and run the same corpus through public decode,
  Linux no-libc, WebAssembly, and supported Windows builds.
- Run ASan/UBSan, bounded fuzzing, strict undefined-symbol checks, workspace
  accounting, reproducible regeneration, and reference-lock validation.
- Update `docs/capabilities.md`, `images/featurematrix.md`, the recipe status,
  reference lock, README surfaces, and security/resource documentation in the
  same admission change.

**Exit gate:** every legal tested tuple inside the profile envelope decodes
sample-exactly, malformed and out-of-envelope vectors fail deterministically,
all platform gates pass, and `spatial-still-8bit` is explicitly advertised.

## Profile completion criteria

The profile is complete only when all milestones pass. In particular:

- decoding the four pilot streams is an integration checkpoint, not profile
  completion;
- enabling an encoder flag is not evidence that the corresponding syntax was
  decoded;
- support for each feature in isolation does not imply support for its legal
  cross-products;
- a resource limit may bound dimensions, tile count, or allocation, but must
  not masquerade as a syntax limitation; and
- unknown, malformed, temporal, or otherwise out-of-profile input must continue
  to fail closed.

Until Milestone 7, production capability documentation must name only the
smaller tuples whose exact evidence has passed. Completed milestone results
should be recorded in this file or a focused evidence document without
rewriting earlier checkpoints as though they had always been supported.
