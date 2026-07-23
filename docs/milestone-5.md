# Milestone 5: private decoder foundation and tiny fixture

Milestone 5 is implemented. The freestanding AV2 entropy decoder, Sequence
Header state, locked Closed Loop Key frame-header parser, and one-tile boundary
derivation are independently checked. All eleven tiny luma blocks and all four
tiny chroma leaves reconstruct sample-exactly through tell 725 using live
reconstructed references and caller-owned state.

A private bounded tile orchestrator owns the range decoder, adaptive CDFs,
luma and chroma partition traversals, neighbor maps, and a compact caller-owned
colocated-luma mode map. No tiny-fixture leaf receives fixture-supplied syntax
contexts. Private focused kernels execute deblock, CDEF, CCSO, and
accepted-profile GDF, while restoration remains unsupported and fail-closed.
Frame-wide larger-fixture reconstruction, filtered fixture parity, final
workspace sizing, and public API exposure are intentionally assigned to
Milestones 6 through 10 rather than remaining hidden inside Milestone 5.

The Milestone 5 exit gate is unfiltered sample parity for the complete tiny
fixture under sanitizers and the strict freestanding dependency gate. That gate
passes. Filter-kernel vectors are supporting evidence, not a claim of filtered
fixture parity.

## Fixture inventory

The pinned AVM v1.0.0 inspection build was run over IVF equivalents generated
from the four locked I420 sources with the exact laboratory encoder options.
The traces are local artifacts under `build/m5/inventory/`.

The union is substantially broader than a minimal DC-only profile:

- 4x4 through large rectangular block and transform shapes;
- every one of AVM's 16 reported primary transform types;
- primary and secondary transform signaling, CCTX, forward and reverse
  coefficient paths, EOB classes, Rice and Exp-Golomb values;
- directional, smooth, Paeth, MRL, CFL, and intraBC prediction;
- skipped and non-skipped transform blocks;
- active CDEF plus CCSO syntax; and
- odd dimensions and intraBC displacement vectors.

Palette is not used, segmentation remains zero, delta Q is absent, and every
fixture is one shown closed-random-access 8-bit 4:2:0 frame at base Q index
128. These observations narrow implementation, but no syntax path may be
accepted until every reachable reconstruction and filter operation is present.

## Entropy decoder

`src/decoder/range.c` implements the normalized AV2 arithmetic decoder with:

- inverse-Q15 CDF symbol decoding for alphabets of 2 through 16 symbols;
- AV2 probability scaling, including per-symbol minimum-range increments;
- exact bypass literals up to 32 bits;
- adaptive inverse-CDF updates with saturating observation count and three
  time-interval rate offsets;
- input supplied as one or more immutable spans;
- checked tell and truncation state; and
- no allocation, libc, system headers, global mutable state, or runtime helper
  dependency.

The module is internal until frame and tile ownership is defined. It does not
weaken carriage acceptance or expose a partial public decoder.

## Independent oracle

`tools/m5_entropy_oracle.c` is hosted-only and links the pinned AVM build to
generate deterministic evidence. The production module does not include or
link AVM code.

The frozen mixed transcript is:

```text
bytes: 2b 79 82 40
tell:  1, 4, 9, 10, 12, 13, 15, 25, 26
```

It combines 2-, 3-, and 5-symbol inverse CDFs with 1-, 5-, and 10-bit bypass
literals. The adaptive transcript uses bytes `38 79 80` and checks every
decoded symbol, updated CDF entry, observation count, and tell position across
eight updates. The frozen transcript is also decoded across every byte split,
and every shorter prefix must report truncation.

`tests/range_unit.c` runs under AddressSanitizer and UndefinedBehaviorSanitizer.
The normal strict gate compiles the module with `-ffreestanding -nostdinc` and
includes it in the zero-undefined-symbol relocatable core.

## Raw syntax and tile framing

The checked bit reader now implements UVLC and signed VLC values, inverse
signed literals, quasi-uniform symbols, and strict zero-padding alignment.
Focused sanitizer tests cover maximum-length and truncated UVLC prefixes,
signed boundaries, complete non-power-of-two quasi-uniform alphabets, and
valid and invalid alignment padding. Carriage parsing uses these shared
primitives rather than maintaining private copies.

`src/decoder/tile.c` implements the locked one-tile tile-group boundary. Given
the parsed uncompressed-header length, it validates the first tile-group flag,
skips the parsed syntax, consumes only zero alignment padding, and returns the
exact entropy span. Fixture-derived tests cover all four payload offsets and
reject bad padding, a non-first group, empty entropy data, and truncation.

## Sequence Header state

The existing public Sequence Header summary remains unchanged. A private
decoder state now retains the dimensions and tool flags needed to parse frame
syntax and initialize reconstruction. Its values are checked for all four
locked Sequence Headers against the hosted AVM inspection build.

The common profile enables SDP, MRL, FSC/IDTX, IST, CFL, MHCCP, and CCTX, while
TCQ, parity hiding, restoration, extended segmentation, sequence segmentation
data, and sequence tile data are disabled. Filter availability varies by
fixture: CDEF, GDF, and CCSO therefore remain real parser branches rather than
profile constants. The frame traces likewise show both disabled and enabled
intraBC.

## Closed Loop Key frame header

`src/decoder/frame.c` parses the complete uncompressed header reachable by the
locked still-picture profile. It cross-checks the multi-frame and Sequence
Header IDs, derives dimensions, consumes screen-content and intraBC controls,
derives one-tile signaling from dimensions and superblock size, and parses
quantization, deblock, GDF, CDEF, CCSO, transform mode, and reduced-transform
set syntax. Unsupported segmentation, delta-Q, custom quant matrices,
restoration, lossless coding, and multi-tile paths fail closed.

The parser independently derives uncompressed-header lengths of 49, 148, 101,
and 65 bits for the tiny, odd, ordinary, and high-detail fixtures. Those values
match the pinned AVM oracle and are used only as test expectations. Production
code contains no fixture-specific offsets. Every shorter byte prefix is
rejected, and the parsed length feeds the strict tile-boundary parser directly.

`tests/frame_unit.c` and `tests/tile_unit.c` run under AddressSanitizer and
UndefinedBehaviorSanitizer. The parser is also part of the strict
`-ffreestanding -nostdinc` zero-undefined-symbol core.

## Initial CDF state

The production CDF primitive expands normative compact rows for alphabets of
2 through 16 symbols into the inverse-CDF and three adaptive rate offsets used
by the range decoder. The parameter-index expansion follows
the pinned specification's `Para_Adjustment_List`, whose 125 entries are the
base-5 product of `{0, -1, -2, 1, 1}`.

The normative attachments used for partition, first-leaf mode, initial
coefficient syntax, and transform-type syntax
initialization are locked by
URL and SHA-256 in `spec/cdf-attachments.lock`. They remain ignored reference
inputs under `build/reference/`; no downloaded header is included by the core.
`tools/check_m5_partition_cdfs.py`, `tools/check_m5_leaf_cdfs.py`,
`tools/check_m5_coeff_cdfs.py`, and `tools/check_m5_transform_cdfs.py`
semantically compare the locked rows with pinned AVM's hosted defaults.
`tests/cdf_unit.c` independently checks representative binary, 4-, 6-, and
8-symbol rows plus malformed specifications under sanitizers and the
freestanding dependency gate.

The complete partition-only CDF state occupies 6,336 caller-owned bytes. Its
tracked production constants are generated by
`tools/generate_m5_partition_cdfs.py`; regeneration from the hash-locked
attachments is byte-identical.

## First partition traversal

The private partition reader consumes `do_split`, `rect_type`, and
`do_ext_partition` with caller-owned range and CDF state. It accepts the
reached leaf and ordinary horizontal/vertical paths and fails closed on
unvalidated extended partitions.

Hosted AVM inspection established an important edge distinction. The 160x120
and 96x80 fixtures signal at their 64x64 root with contexts `(12, 24, 12)`.
The 16x16 and 17x15 roots instead pass through four and three normative
edge-forced ancestors, respectively; their first signaled nodes use contexts
`(4, 0, 4)` and `(8, 8, 8)`. All four fixture-derived entropy prefixes decode
to the same values and CDF mutations as the pinned oracle under sanitizers.

The project-owned geometry layer defines the 29 normative AV2 block sizes,
ordinary horizontal and vertical child sizes, partition size adjustments,
MI-grid rounding, and midpoint boundary forcing. A 32-node caller-owned stack
performs depth-first traversal through forced, signaled, and implied
decisions. It yields one leaf at a time and requires an explicit commit after
the interleaved leaf work; that commit updates bounded above/left partition
state before traversal may resume. The four locked first paths contain 8, 8,
3, and 7 partition nodes and end at 8x4, 8x4, 32x32, and 8x8 leaves,
respectively.

Tests freeze every first-path node's block size, partition type,
forced/implied provenance, selected CDF behavior, and integer entropy tell. In
particular, edge-forced and implied choices do not advance entropy or adapt a
CDF. The same traversal now yields and commits all eleven tiny luma leaves and
all four separate chroma-tree leaves around their interleaved syntax, then
reports each tree exhausted at tells 415 and 725. Calling next before commit,
committing a different leaf, stack overflow, and out-of-bounds geometry fail
closed. Post-Milestone-5 work has advanced the ordinary fixture to six
committed luma leaves; that checkpoint belongs to Milestone 7.

## First-leaf luma mode

The private luma-mode continuation owns the 3 intraBC, 12 CCSO block-control,
one luma-mode-set, three luma-mode-index, and three luma-mode-offset initial
CDFs plus 24 FSC, six MRL, three CFL, and two UV-mode rows in 784 bytes of
caller storage. It continues from the
partition endpoint
on the same range decoder and fails closed outside the locked base-Q-128,
top-left, nonsegmented, no-delta-Q profile.

For the first top-left leaf, intraBC and CCSO use context zero because no
neighbors exist. Tiny omits intraBC and CCSO. Odd reads intraBC zero followed
by U and V CCSO controls of one. Ordinary and high-detail each read intraBC
zero followed by a Y CCSO control of one. GDF consumes no symbol because the
only enabled fixture uses mode 1; CDEF consumes no symbol because enabled
fixtures have one strength.

CCSO block control is read only at a 64x64 unit origin, matching the normative
MI-coordinate gate. Later leaves inside the same unit do not consume or adapt
a CCSO symbol. Luma mode-index mapping now builds the complete 61-entry list
for every block at least 8x8, with left/above directional candidates and the
additional wrapped angular neighbors for blocks larger than 64 samples. Small
4x4, 4x8, and 8x4 blocks retain the fixed normative ordering.

All four streams then decode luma mode set zero. Their mode indexes are 0, 0,
0, and 4, mapping to DC, DC, DC, and PAETH. Sanitizer-backed tests freeze the
conditional symbols, every touched observation count, and production integer
tells `(11,11,12)`, `(17,17,18)`, `(8,8,9)`, and `(18,18,22)` at the pre-mode,
post-set, and post-index boundaries.

All four first leaves then decode FSC mode zero with context zero and size
groups 1, 1, 5, and 2. Their integer tells remain 12, 18, 9, and 22. The
continuation rejects palette and intra-DIP paths that require an unimplemented
entropy symbol.

The tiny fixture then reaches its second 4x4 leaf at `(0,2)` on the same range
decoder. Neighbor-aware mode-set/index/offset mapping selects D203, FSC and MRL
both decode zero, and the mode endpoint has integer tell 59. The adjacent third
4x4 block selects the same mode and reaches integer tell 80 after MRL. Tests
freeze the touched CDF updates and mode results. Caller-owned mode maps now
derive every later tiny luma neighbor through the bounded tile orchestrator.

After the final tiny luma leaf at tell 415, partition CDF structure one decodes
the first chroma-tree 16x16 horizontal and 16x8 implied-vertical decisions,
reaching a forced 8x8 leaf at tell 420. CFL context zero decodes false and UV
mode context zero decodes index zero as DC, ending at tell 423. The 57 mode
rows, including all CFL and UV-mode defaults, match hash-locked normative
attachments and pinned AVM. U then decodes EOB 3, CCTX none, quantized
coefficients `-3` and `-1` at positions 1 and 4, and sample-exact 4x4 pixels
through tell 445. V uses its separate context-9 skip row, decodes EOB 3 and
quantized coefficients `11`, `-3`, and `-1` at positions 0, 1, and 4, and
reconstructs sample-exactly through tell 469. The continuation covers the
other three leaves as well: D203 through tell 571, inherited D67 angle delta
-1 through tell 643,
and D45 through tell 725. Their mode escape literals, separate U/V skip
contexts, implied primary transforms, and sparse coefficient contexts match
the pinned oracle.

## First coefficient prefix

Under frame `TX_MODE_LARGEST`, the first transforms are full-leaf `TX_8X4`,
`TX_8X4`, `TX_32X32`, and `TX_8X8`. A private 102-byte caller-owned coefficient
state decodes the normative luma `all_zero` symbol and complete EOB, then stops
before transform-type syntax. All four `all_zero` values are zero. Exact EOBs
are 10, 10, 29, and 63, with final integer tells 18, 24, 15, and 29.

The coefficient prefix state is 234 bytes after adding the tiny 4x4 luma and
all reached U/V skip and EOB-point rows. The second luma
block has EOB 2 and reaches integer tell 64; the third has EOB
16 and reaches integer tell 84.
Caller-owned packed coefficient maps replicate each block's capped magnitude
and DC-sign flags across its MI footprint. Prefix and level continuations use
that shared state to derive full-leaf luma context 0, FSC luma context 9,
chroma U contexts 6 through 8, the separate U-dependent V contexts 7 through
9, and luma DC-sign contexts 0 through 2. No later tiny-fixture coefficient
call receives a hardcoded neighbor context.
The checkpoint validates transform capacity, Q context 1, intra-luma plane
context zero, adaptive observation counts, and every point/extra/bypass tell.
It fails closed on unsupported transform geometry, all-zero transforms, and
the unimplemented 1024-coefficient point-extension path.

## First transform type

A separate 82-byte caller-owned transform state expands two 7-symbol primary
rows, the locked intra 32x32 4-symbol secondary row, and the 7-symbol CCTX row.
It continues on the
same decoder after EOB. Tiny and odd signal primary symbol zero and map to
`DCT_DCT`, advancing integer tells from 18 to 20 and 24 to 26. Ordinary has an
implied `DCT_DCT` primary and signals secondary flag zero, advancing from 15
to 19. High-detail signals primary symbol four, maps to `V_DCT`, and advances
from 29 to 34.

The tiny second block signals primary symbol three, maps to `DCT_ADST`, and
advances integer tell from 64 to 67. The third signals symbol six, maps to
`H_ADST`, and advances from 84 to 88. The fourth 8x4 block signals symbol five,
maps to `ADST_FLIPADST`, and advances from 142 to 145. The fifth signals symbol
zero and maps to `DCT_DCT`; the sixth uses FSC and therefore has implied
`IDTX`, with no transform symbol consumed at tell 185.
The seventh signals primary symbol three and maps to 8x8 `DCT_ADST`.

The checkpoint evaluates secondary-transform eligibility from transform
geometry, primary type, intra mode, EOB, and Sequence Header IST enablement.
Tiny and odd exceed the small-transform EOB limit; high-detail is ineligible
by mode and primary type. Nonzero secondary flags, reduced sets, and
unvalidated type mappings fail closed. All 14 normative transform/CCTX rows match
pinned AVM defaults in hosted evidence, while no AVM table enters production.

## First transform coefficients

A private 1,620-byte sparse CDF state decodes the validated luma and tiny
chroma transforms:
normative diagonal or vertical scan, base and low-frequency magnitudes,
base-range extensions, bypass and adaptive DC-axis signs, adaptive
truncated-Rice high range, and fixed-profile dequantization. The result uses
caller-owned arrays sized for the largest accepted 32x32 transform. TCQ,
parity hiding, quant matrices, and unvalidated CDF rows fail closed.

Tiny and odd each reconstruct four nonzero quantized coefficients and finish
at integer tells 51 and 57. Ordinary reconstructs eight and finishes at 96.
High-detail uses the vertical scan, reconstructs 54, and finishes at 314.
Tests compare deterministic hashes over every quantized and dequantized entry,
as well as nonzero count, DC value, magnitude checkpoint, and final tell. All
3,368 rows in the 23 coefficient attachments match pinned AVM defaults in the
hosted semantic checker; production retains only the fixture-reachable rows.

The same reader continues through the tiny second leaf, producing quantized
coefficients `4` at index 0 and `-1` at index 4, with all others zero, before
finishing at integer tell 74. Neighbor coefficient contexts include the
normative capped magnitude and DC-sign flags.

The third block uses the horizontal scan and `H_ADST` neighborhoods. Its eight
nonzero quantized coefficients are `2, 2, 3, -4` down column zero and `1, 6,
-4, 1` at positions 9, 13, 14, and 15. Its final DC sign uses neighboring
context 2; all other adaptive axis signs use context 0. It finishes at integer
tell 126 with magnitude context 23.

The fourth block uses the diagonal two-dimensional scan. Its only nonzero
quantized coefficients are `-2` at index 0 and `-1` at index 2. DC-sign
context 1 is derived from the reconstructed neighbors; the block finishes at
integer tell 152 with magnitude context 11.

The fifth block has quantized coefficients `2`, `-1`, and `-1` at positions 0,
1, and 4 and finishes at tell 172. The sixth uses the distinct FSC forward
path: signaled EOB 15 becomes BOB index 1 in the full 4x4 scan. Its nonzeros
are `-1`, `9`, `9`, `10`, and `1` at positions 4, 8, 5, 13, and 15. Sparse
adaptive BOB, IDTX base, base-range, and sign rows reproduce the hosted endpoint
at tell 232.

The seventh reuses all previously admitted base and sign rows and adds one
Q1/8x8/EOB-context-1 LF row. Its only nonzeros are `2` at position 0 and `-1`
at position 8, and it finishes at tell 262.

The eighth extends FSC to BOB index 8 and adds BOB context 2. Its nonzeros are
`-1`, `5`, and `-6` at positions 6, 7, and 14, finishing at tell 309. The ninth
uses BOB index 3 and adds BOB context 1 plus IDTX base context 2. Its nonzeros
are `-1`, `-1`, and `1` at positions 8, 5, and 15, finishing at tell 335.

The tenth uses FSC with BOB level 3, exercising the BOB base-range extension,
and implied `IDTX`. Its seven nonzeros are `-10`, `3`, `1`, `-5`, `1`, `1`,
and `-1` at positions 8, 2, 12, 9, 3, 10, and 14. The sparse state adds IDTX
base context 4, base-range context 5, and sign contexts 1 and 2; decoding ends
at integer tell 399.

The eleventh and final tiny luma leaf reuses existing low-frequency rows. Its
diagonal scan contains quantized coefficients `3` and `-1` at positions 0 and
4, and coefficient decoding ends at integer tell 415. No additional sparse
CDF storage is required at that point. The four chroma leaves add sparse UV
EOB, base, LF, and base-range rows for every reached 4x4 scan context,
increasing caller-owned coefficient level state to 1,620 bytes. Their U/V
coefficient endpoints are 469, 571, 643, and 725.

## First-block reconstruction

The private reconstruction continuation implements the normative integer
inverse-transform path for the three first-block cases: 8x4 `DCT_DCT`, 32x32
`DCT_DCT`, and 8x8 `V_DCT`. It applies rectangular inverse-square-root-two
scaling, the specified row and column shifts, 8-bit intermediate clipping,
and clipped residual addition. Scratch and output are caller-owned; the core
does not allocate or call libc. The sparse 32-point matrix fails closed if a
coefficient activates a frequency row outside the validated first transform.
The explicit transform scratch contains three 1024-entry 32-bit arrays, for a
fixed 12,288-byte caller-owned requirement.

Both unavailable top and left references produce the normative 128 predictor
for these top-left DC and Paeth blocks. Sanitizer-backed tests continue through
the live partition, mode, transform, and coefficient readers before comparing
every pixel of the 8x4 and 8x8 outputs. Complete block hashes are
`6adb5720dbef6cf3` for tiny and odd, `b666ec5be3701956` for ordinary, and
`62cd66e73a5893f4` for high-detail, matching the pinned hosted oracle.

The 4x4 `DCT_ADST` and `H_ADST` paths reconstruct the tiny fixture's second and
third luma blocks from production D203 predictors. The predictor reads live
reconstructed left samples, applies derivative 24 and the normative four-tap
interpolation phases, and extends the boundary sample. Their residuals and all
32 final samples match the oracle exactly at integer tells 74 and 126. The
`H_ADST` path uses the normative 4-point identity scale 128 rather than the
8-point scale 181.

The fourth 8x4 block uses a D45 predictor with angle delta +3. The production
path applies normative derivative 44 and four-tap interpolation phases 22, 12,
2, and 24 to the live reconstructed above row. Its `ADST_FLIPADST` inverse
transform applies the sparse 8-point horizontal FLIPADST rows, 4-point vertical
ADST, rectangular scaling, and 7/10 shifts. All 32 predictor, residual, and
final pixel samples match pinned AVM, ending at integer tell 152.

The fifth 4x4 block reads above, left, and top-left reconstructed samples for
Paeth and applies `DCT_DCT`; all predictor, residual, and final samples match at
tell 172. The sixth uses zero-delta D45 prediction and the 4-point identity
scale 128 in both transform dimensions. Its sparse residual positions and all
16 final pixels match pinned AVM at tell 232.

The seventh 8x8 block uses the neighbor-reordered mode list to map signaled
index 36 to D67 with angle delta -1. MRL index 1 adjusts the effective angle to
65, selecting normative derivative 27 and eight verified IDIF phases. Sparse
8-point horizontal ADST and vertical DCT reconstruction reproduces all 64
predictor and final samples at tell 262.

The eighth uses 4x4 D67, angle delta -2, and MRL index 1. The MRL adjustment
selects effective angle 62, derivative 31, and four verified IDIF phases; IDTX
reconstruction matches all samples at tell 309. The ninth reuses zero-delta
D45 and IDTX, matching all samples at tell 335.

The tenth uses 4x4 D67, angle delta -3, and MRL index 3. Effective angle 58
selects normative derivative 38 and verified IDIF phases 12, 31, 18, and 5.
The predictor reads the eight reconstructed samples above it from blocks eight
and nine; two-stage IDTX rounding, residual clipping, and all 16 final samples
match pinned AVM at tell 399.

The eleventh leaf maps mode index 1 to `SMOOTH`, signals primary transform
symbol 3 as `DCT_ADST`, and reads reconstructed samples from block nine above
and block ten to the left. Its AV2 smooth process uses the derived 4x4 blend
values 32, 8, 2, and 0 with replicated unavailable edge extensions. All 16
predictor, residual, and final samples match pinned AVM at tell 415. This
completes the tiny fixture's luma leaves.

The four tiny chroma leaves then reconstruct sample-exact 4x4 U and V blocks.
They exercise DC, D203, D67 angle delta -1 inherited from colocated luma, and
D45 prediction, plus implied `DCT_DCT`, `DCT_ADST`, and `ADST_DCT` transforms.
Each predictor uses reconstructed chroma edges, and all 128 predictor and final
samples match pinned AVM through tell 725.

## Filter pipeline

The private filter pipeline uses caller-owned reconstructed, deblocked, CDEF,
and final planes. A bounded per-4x4 metadata map supplies prediction and
transform bases, transform extents, final Q indexes, skip state, and lossless
state to the deblock scheduler. Vertical edges are processed before horizontal
edges, with normative threshold and asymmetric width derivation. CDEF runs
out of place, CCSO classifies against the immutable pre-CDEF luma plane, and
GDF writes final luma from an immutable post-CCSO source. Filter planes cover
the coded MI extent, so the 17x15 fixture uses 24x16 luma storage before final
cropping.

GDF's five trained tables and the deblock side-threshold table are generated
from hash-locked AV2 v1.0.0 normative attachments. Focused sanitizer vectors
cover deblock edge and frame scheduling, CDEF direction/block/frame filtering,
CCSO controls and classifiers, a complete independently computed 8x8 GDF
output, coded bounds, and preflight failures. Restoration and unsupported
per-block GDF or multi-strength CDEF modes reject before final-plane mutation.

These kernel and orchestration checks do not establish fixture stage parity.
The larger-fixture tile and reconstruction work must first produce complete
coded planes and populate every deblock and CCSO control cell.

## Successor milestones

The work formerly grouped into this milestone is split into bounded successors:

- [Milestone 6](milestone-6.md) connects frame-wide state and completes the odd
  fixture.
- [Milestone 7](milestone-7.md) completes the ordinary fixture from its current
  six-leaf checkpoint.
- [Milestone 8](milestone-8.md) completes high-detail, CFL, intraBC, and the
  remaining advanced transform paths.
- [Milestone 9](milestone-9.md) integrates mandatory filters and proves final
  filtered parity for all four fixtures.
- [Milestone 10](milestone-10.md) finalizes bounded workspace contracts and
  exposes the public decoder only after parity passes.

This split changes scheduling, not acceptance. Unsupported syntax still fails
closed, restoration remains outside the accepted profile, and no public decode
surface exists before Milestone 10.
