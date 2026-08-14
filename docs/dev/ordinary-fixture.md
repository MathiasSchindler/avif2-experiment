# Ordinary fixture coverage

Ordinary-fixture coverage is complete. The private decoder traverses the full coded-size
ordinary fixture through `AV2H_TILE_DECODE_DONE` at tell 6498. Its unfiltered
prefilter planes and final cropped output are sample-exact with pinned AVM.
The prefilter crop FNV-1a hashes are Y `7945904097577017502`, U
`4124092689509881817`, and V `6418349723304023184`. The final 28,800-byte
I420 SHA-256 is
`def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5`.

The locked input is `tests/fixtures/ordinary-160x120.obu`, 843 bytes, SHA-256
`adbb88447dacbdf6895964df6a6fc1a1506cdf007d27044ea42171ccbb12810f`.

## Verified checkpoint

The 22 committed luma leaves end at these exact tells:

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
| 8 | `(10,5)` | 4x4 | IDTX/FSC | 457 |
| 9 | `(11,4)` | 8x4 | DCT_DCT | 482 |
| 10 | `(10,6)` | 8x8 | ADST_ADST | 505 |
| 11 | `(12,0)` | 16x8 | skipped intraBC | 529 |
| 12 | `(12,4)` | 16x8 | ADST_ADST | 551 |
| 13 | `(14,0)` | 32x8 | ADST_DCT | 576 |
| 14 | `(8,8)` | 8x32 | DCT_DCT | 616 |
| 15 | `(8,10)` | 8x32 | DCT_DCT | 647 |
| 16 | `(8,12)` | 8x4 | all-zero D45 | 671 |
| 17 | `(9,12)` | 8x4 | skipped intraBC | 696 |
| 18 | `(8,14)` | 8x8 | skipped intraBC | 703 |
| 19 | `(10,12)` | 8x16 | all-zero D203/MRL | 723 |
| 20 | `(10,14)` | 8x16 | skipped intraBC | 744 |
| 21 | `(14,12)` | 16x8 | skipped intraBC | 762 |

The reached 16x32 path includes smooth-vertical prediction and the pinned
AVM tuned 16-point ADST matrix. The 32x8 path includes the exact edge-filtered
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

The committed `(10,5)` leaf is a 4x4 D203 angle-plus-three block with FSC mode
1 and forced IDTX. Mode parsing ends at tell 428, its EOB 15 prefix and IDTX
selection end at tell 432, level decoding ends at tell 453, and coefficient
sign decoding ends at tell 457. The reached TX-size-context-0 FSC base context
5 row uses thresholds `3855/7854/12411` and parameter index 1.

Three coefficients are nonzero: position 1 is quantized +2/dequantized +392,
position 2 is -2/-392, and position 4 is -4/-784. The D203 predictor uses the
reached derivative 38 and reconstructs with identity residuals +49, -49, and
-98 at those positions. The exact 4x4 prefiltered output is:

```text
255 255 5 0
4   0   0 6
0   6   6 6
6   6   6 6
```

The committed `(11,4)` leaf is an 8x4 D203 angle-plus-three block with FSC mode
0. Mode parsing ends at tell 467 and its EOB 3 prefix ends at tell 471.
Primary-transform symbol 0 maps to DCT_DCT and ends at tell 474. The reached
secondary 4x4 symbol 0 ends at tell 477. Three coefficient levels end at tell
479, and their signs end at tells 480, 481, and 482. Positions 1 and 8 are
quantized -1/dequantized -196; position 0 is +1/+196.

The rectangular wide-angle mapping uses angle 32, derivative 107, and the
filtered above edge. The predictor's first row is `16 1 6 6 6 6 6 6`; the
other three rows contain only 6. DCT_DCT reconstruction produces the exact
prefiltered output:

```text
9  0  1  4  6  8 10 11
2  3  5  7  9 11 13 14
7  8  9 11 14 16 18 19
10 11 13 15 17 19 21 22
```

The committed `(10,6)` leaf is an 8x8 D203 angle-plus-three block with FSC mode
0 and MRL index 1 selecting multi-line MRL 0. Mode parsing ends at tell 491 and
its EOB 2 prefix ends at tell 497. Primary-transform symbol 1 maps to
ADST_ADST and ends at tell 499. Secondary-transform symbol 0 ends at tell 502.
Level decoding ends at tell 504 and both coefficients complete at tell 505:
position 8 is quantized -1/dequantized -196 and position 0 is +2/+392.

The MRL predictor uses angle 213, derivative 41, and the second reference line
without edge filtering. Its first two rows are `1 5 6 7 9 11 14 17` and
`6 6 8 11 12 15 19 21`. ADST_ADST reconstruction produces the exact
prefiltered output:

```text
1  5  6  6  8 10 13 16
6  6  7 10 11 14 18 20
7 10 12 14 17 20 21 21
11 14 17 21 23 23 24 24
15 20 23 25 26 27 27 28
20 25 27 28 30 31 32 32
23 26 29 31 33 35 36 36
23 27 30 32 35 37 38 39
```

The committed `(12,0)` leaf is a 16x8 intraBC block. The normative
skip-transform context is 0; AVM reads symbol 1 from inverse CDF `6041,0,1`
and reaches tell 516. The remaining intraBC syntax ends at tell 529:
mode 0, DRL index 3, integer-pixel precision, and motion vector `(96,-64)` in
eighth-pixel units. Shell class 3 uses the three reached LSB-first offset CDF
rows. The selected BVP is `(0,-64)` and the decoded difference is `(96,0)`.

The motion vector copies the already reconstructed 16x8 source at pixel
`(12,40)` to destination `(0,48)`. `skip_transform=1` consumes no transform,
EOB, level, or coefficient bits. The tile path records TX_16X8, EOB 0, and
128 zero coefficients at tell 529, updates coefficient contexts with
`cul_level=0`, and reconstructs the destination as an unchanged predictor
copy. All 128 samples match the pinned AVM prefiltered frame buffer.

The final ten leaves add the reached 16x8 and 32x8 smooth predictors and
transforms, two 8x32 DCT_DCT paths, the 8x4 D45 all-zero path with implicit
intra-bi-prediction, the 8x16 D203 MRL all-zero path with extended below-left
references, and four additional skipped intraBC copies. The AV2 smooth
predictors use the current geometric blend rather than the legacy AV1 weight
tables. All 22 leaf surfaces are locked by complete AVM-prefilter FNV hashes,
including the earlier 8x32, 16x32, and 32x8 surfaces that previously lacked
full-surface sample oracles.

The ordinary fixture has no remaining syntax, traversal, prediction,
reconstruction, or crop frontier in ordinary-fixture coverage.

## Historical JPEG artifact OBU audit

This section records the corpus frontier when ordinary-fixture coverage closed. Current
end-to-end artifact results are maintained in
[`images/artifacts/status.md`](../../images/artifacts/status.md).

The 20 generated streams listed by `images/artifacts/manifest.json` and the
legacy `chalk-320x240-qp128.obu` have been checked against the current private
decoder. All 21 Sequence Headers and Frame Headers parse, but none of the
streams reaches the ordinary-fixture frontier or another missing
ordinary-fixture coverage path.

- Every `uncompressed_header_bits` endpoint matches the pinned AVM inspector.
  GDF mode, picture QP index, and scale index are retained for all streams.
- Six streams select GDF mode 2. They now initialize the tile decoder and
  consume their first per-block on/off symbol at the exact pinned-AVM tell.
  Five then consume and store their exact first multi-strength CDEF index; the
  sixth has one implicit CDEF strength. All six subsequently consume their
  first enabled CCSO controls on the 256x256 unit grid and continue through
  exact mode syntax. Emerald, flycatcher, laarim, pastell, schwaebisch, and
  silene now reach tells 17, 25, 20, 25, 32, and 9. Large-block FSC is
  correctly implicit zero, and silene consumes Palette=0.
- Non-mode-2 multi-strength CDEF and CCSO now run in the same normative order.
  Chalk reaches tell 32; chinnathurai-128 reaches 9, chinnathurai-333 reaches
  7, cuckoo-431 reaches 9, louis-301 reaches 16, and silene-384 reaches 6.
- The other 15 streams parse their frame and single tile. Four now complete
  their first luma coefficient block through exact pinned-AVM endpoints:
  `cuckoo-157x105-qp160` at tell 35, `emerald-192x96-qp208` at tell 27,
  `louis-463x342-qp224` at tell 27, and `pastell-512x341-qp160` at tell 65.
  The remaining eleven stop fail-closed at an earlier first-luma syntax
  frontier. This remains part of advanced-prediction coverage.
- The generated QP-128 stream `emerald-480x240-qp128.obu` is one of the six
  GDF-mode-2 streams. The legacy QP-128 `chalk-320x240-qp128.obu` reaches its first
  8x16 luma leaf.

Consequently, the newly committed ordinary 4x4 intraBC leaf does not make any
artifact stream decode farther. No base-Q guard may be removed merely to admit
the artifact matrix: non-128 streams require matching Q-context CDF
initialization and dequantization rather than permissive parsing.

`tests/artifact_profile_unit.c` locks the per-stream dimensions, base Q,
Sequence Header filter flags, exact Frame Header endpoint, GDF parameters,
the first mode-2 GDF flag, the first CDEF strength index, the first enabled
Y/U/V CCSO controls and their caller-backed map values, and the first
block/tell for all streams. For the four completed advanced-prediction coverage first blocks it
additionally locks mode, prefix, transform, level, coefficient, and complete
quantized/dequantized arrays. It is part of `make test-c`.

## Scope

- complete the remaining ordinary luma partition and leaf traversal;
- add only the normative CDF rows, scan rules, prediction modes, and inverse
  transforms reached by that traversal;
- complete ordinary chroma mode, coefficient, prediction, and reconstruction;
- preserve live-reference and commit-before-context-update invariants; and
- freeze each new frontier with exact syntax, tell, coefficient, and sample
  evidence.

## Handoff

At this handoff, advanced-prediction coverage owned broader artifact-profile coverage. The
ordinary fixture is a closed regression oracle: retain coded-size planes, full
traversal to DONE at tell 6498, and the exact Y/U/V crop hashes above.

Run the current focused checkpoint with:

```sh
make build/host/tile-unit
build/host/tile-unit
```

## Exit gate

Complete: the ordinary fixture matches pinned AVM on unfiltered prefilter and
final cropped Y, U, and V planes and terminates at authoritative tell 6498.
Sparse normative tables remain acceptable only while every omitted row fails
closed before reconstruction.

ordinary-fixture coverage does not integrate frame filters or expose a public decoder.