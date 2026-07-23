# Milestone 7: ordinary fixture coverage

Milestone 7 is paused at a verified fail-closed checkpoint. It is not complete.
The private decoder commits eight ordinary luma leaves sample-exactly through
tell 414. The next 4x4 leaf at `(10,5)` reaches FSC/IDTX coefficient decoding,
then fails closed at tell 443 because TX-size-context-0 FSC base context 5 is
not yet present in the sparse table.

The locked input is `tests/fixtures/ordinary-160x120.obu`, 843 bytes, SHA-256
`adbb88447dacbdf6895964df6a6fc1a1506cdf007d27044ea42171ccbb12810f`.

## Verified checkpoint

The eight committed luma leaves end at these exact tells:

| Leaf | Position | Block | Transform | Endpoint |
| --- | --- | --- | --- | --- |
| 0 | `(0,0)` | 32x32 | DCT_DCT | 96 |
| 1 | `(0,8)` | 8x32 | DCT_DCT | 135 |
| 2 | `(0,10)` | 8x32 | DCT_DCT | 174 |
| 3 | `(0,12)` | 16x32 | DCT_ADST | 209 |
| 4 | `(8,0)` | 32x8 | DCT_DCT | 241 |
| 5 | `(10,0)` | 16x4 | DCT_DCT | 276 |
| 6 | `(11,0)` | 16x4 | IDTX/FSC | 365 |
| 7 | `(10,4)` | 4x4 | IDTX/intraBC | 414 |

The reached 16x32 path includes smooth-vertical prediction and the pinned
AVM tuned 16-point DDT matrix. The 32x8 path includes the exact edge-filtered
D45 angle-minus-one predictor, square-mapped secondary-transform signaling,
and nonzero coefficients at positions 32 and 0.

The committed `(10,0)` leaf is 16x4 D45 angle-minus-one, TX_16X4, DCT_DCT,
and EOB 1. Its prefix ends at tell 256, level decoding ends at tell 265, and
coefficient/sign decoding ends at tell 276 with only position 0 nonzero. The
dequantized DC coefficient is 5096. Its predictor uses the normative angle-42
derivative 73, strength-3 intra-edge filtering, and zone-1 IDIF interpolation.
The DCT16/DCT4 inverse transform uses shifts 6 and 12 without rectangular
square-root-of-two scaling, producing a constant residual of +80. All 64
reconstructed samples match AVM's prefiltered frame buffer exactly.

Production support added at this checkpoint includes TX_16X32, TX_32X8, and
TX_16X4 geometry; EOB capacities through 512; the reached sparse EOB, LF-base,
and BR-LF rows; the reached 16x32 and 32x8 prediction/transform paths; and
sample-exact 16x4 D45 angle-minus-one prediction and DCT_DCT reconstruction.
Temporary private arithmetic-state and CDF dumps used to diagnose the 32x8
secondary-transform mismatch have been removed.

The committed `(11,0)` leaf is another 16x4 D45 angle-minus-one block. FSC mode
1 ends at tell 285. Its EOB syntax ends at tell 291 with value 14, which is the
FSC BOB escape: the active scan starts at index 50 and continues through 63.
The TX_16X4 transform is forced to IDTX. Level decoding ends at tell 322 and
sign/high-range decoding ends at tell 365. Ten coefficients at positions
30, 31, 45, 46, 47, and 59 through 63 are all quantized to -10 and dequantized
to -1960. The identity reconstruction applies a residual of -245 at exactly
those positions. All 64 predictor and reconstructed samples match AVM's
predicted and prefiltered frame buffers.

This path adds the exact TX-size-context-1 FSC skip, BOB, base, BR, and sign
CDF rows reached by the leaf. Other TX-size-context-1 FSC rows remain omitted
and fail closed.

The committed `(10,4)` leaf is a 4x4 intraBC block. Its displacement is
`(-32,24)` at precision 3 with DRL index 3. The 4x4 prefix has EOB 5 and ends
at tell 396. Inter transform context 1 uses the reached set/index CDF rows;
set/index 0/0 maps to IDTX and ends at tell 402. The inter-FSC coefficient
path ends at tell 414 with one positive coefficient at IDTX position 10:
quantized +10 and dequantized +1960.

The displacement references the already reconstructed 4x4 region at `(12,43)`,
which is below and left of the destination at `(16,40)`. intraBC validation now
uses the per-4x4 readiness map for every source sample instead of rejecting all
references extending below the destination row. Reconstruction adds +245 at
local sample `(2,2)`, changing 6 to 251, and leaves the other 15 predictor
samples unchanged. All predictor and reconstructed samples match pinned AVM.

The current frontier is `(10,5)`, a 4x4 D203 angle-plus-three block with FSC
mode 1 and IDTX. Mode parsing ends at tell 428, its EOB 15 prefix and forced
IDTX selection end at tell 432, and level decoding reaches position 5 with
TX-size-context-0 FSC base context 5. That sparse row is omitted, so decoding
fails closed before coefficient completion or reconstruction at tell 443.

## JPEG artifact OBU audit

The 20 generated streams listed by `images/artifacts/manifest.json` and the
legacy `chalk-320x240.obu` have been checked against the current private
decoder. All 21 Sequence Headers parse, but none of the streams reaches the
ordinary-fixture Milestone 7 frontier or another missing Milestone 7 path.

- 17 streams stop in the frame header because GDF is enabled on a frame larger
  than one superblock. Carrying that frame metadata and later applying GDF are
  Milestone 9 responsibilities.
- The remaining four streams parse their frame and single tile, then stop at
  the first luma mode. Their QP-dependent mode and coefficient profile is
  broader Milestone 8 work.
- The only generated QP-128 stream,
  `emerald-480x240-qp128.obu`, belongs to the first group and is blocked by
  large-frame GDF header handling before tile syntax.

Consequently, the newly committed ordinary 4x4 intraBC leaf does not make any
artifact stream decode farther. No base-Q guard may be removed merely to admit
the artifact matrix: non-128 streams require matching Q-context CDF
initialization and dequantization rather than permissive parsing.

`tests/artifact_profile_unit.c` locks the per-stream dimensions, base Q,
Sequence Header filter flags, frame-parser status, and the first block/tell for
the four streams that reach tile decoding. It is part of `make test-c`.

## Scope

- complete the remaining ordinary luma partition and leaf traversal;
- add only the normative CDF rows, scan rules, prediction modes, and inverse
  transforms reached by that traversal;
- complete ordinary chroma mode, coefficient, prediction, and reconstruction;
- preserve live-reference and commit-before-context-update invariants; and
- freeze each new frontier with exact syntax, tell, coefficient, and sample
  evidence.

## Handoff

Resume with TX-size-context-0 FSC base context 5 at `(10,5)`, then add only the
remaining FSC rows reached by that leaf until its coefficient and sample
endpoint match pinned AVM. Continue the remaining ordinary luma traversal and
all ordinary chroma paths without adding later syntax from a divergent entropy
or sample frontier. Broad intraBC coverage remains Milestone 8 work.

Run the current focused checkpoint with:

```sh
make build/host/tile-unit
build/host/tile-unit
```

## Exit gate

The complete ordinary fixture must match pinned AVM on unfiltered Y, U, and V
planes, terminate at the authoritative entropy endpoint, and pass sanitizers
and the strict freestanding gate. Sparse normative tables remain acceptable
only while every omitted row fails closed before reconstruction.

Milestone 7 does not integrate frame filters or expose a public decoder.