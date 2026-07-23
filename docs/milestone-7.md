# Milestone 7: ordinary fixture coverage

Milestone 7 is paused at a verified fail-closed checkpoint. It is not complete.
The private decoder commits five ordinary luma leaves sample-exactly through
tell 241. It then decodes the next 16x4 leaf at `(10,0)` through the pinned AVM
entropy endpoint at tell 276, but does not commit it because its D45
angle-minus-one predictor and 16x4 DCT_DCT reconstruction remain unsupported.

The locked input is `tests/fixtures/ordinary-160x120.obu`, 843 bytes, SHA-256
`adbb88447dacbdf6895964df6a6fc1a1506cdf007d27044ea42171ccbb12810f`.

## Verified checkpoint

The five committed luma leaves end at these exact tells:

| Leaf | Position | Block | Transform | Endpoint |
| --- | --- | --- | --- | --- |
| 0 | `(0,0)` | 32x32 | DCT_DCT | 96 |
| 1 | `(0,8)` | 8x32 | DCT_DCT | 135 |
| 2 | `(0,10)` | 8x32 | DCT_DCT | 174 |
| 3 | `(0,12)` | 16x32 | DCT_ADST | 209 |
| 4 | `(8,0)` | 32x8 | DCT_DCT | 241 |

The reached 16x32 path includes smooth-vertical prediction and the pinned
AVM tuned 16-point DDT matrix. The 32x8 path includes the exact edge-filtered
D45 angle-minus-one predictor, square-mapped secondary-transform signaling,
and nonzero coefficients at positions 32 and 0.

The uncommitted `(10,0)` leaf is 16x4 D45 angle-minus-one, TX_16X4,
DCT_DCT, and EOB 1. Its prefix ends at tell 256, level decoding ends at tell
265, and coefficient/sign decoding ends at tell 276 with only position 0
nonzero. The focused test requires those values and requires reconstruction to
return `AV2H_UNSUPPORTED` before plane mutation.

Production support added at this checkpoint includes TX_16X32, TX_32X8, and
TX_16X4 geometry; EOB capacities through 512; the reached sparse EOB, LF-base,
and BR-LF rows; and the reached 16x32 and 32x8 prediction/transform paths.
Temporary private arithmetic-state and CDF dumps used to diagnose the 32x8
secondary-transform mismatch have been removed.

## Scope

- complete the remaining ordinary luma partition and leaf traversal;
- add only the normative CDF rows, scan rules, prediction modes, and inverse
  transforms reached by that traversal;
- complete ordinary chroma mode, coefficient, prediction, and reconstruction;
- preserve live-reference and commit-before-context-update invariants; and
- freeze each new frontier with exact syntax, tell, coefficient, and sample
  evidence.

## Handoff

Resume with the 16x4 D45 angle-minus-one predictor and DCT_DCT inverse
transform. Require sample parity for `(10,0)` before committing it or advancing
to the next leaf. Then continue the remaining ordinary luma traversal and all
ordinary chroma paths. Do not add later syntax from a divergent entropy or
sample frontier.

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