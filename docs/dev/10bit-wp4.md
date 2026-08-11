# 10-bit implementation WP4 luma reconstruction

## Result

10B-WP4 is complete. The decoder now reconstructs the constrained fixture's
unfiltered 10-bit Y plane in production U16 storage, consumes the complete tile
syntax, and stops explicitly before chroma sample reconstruction. The caller's
public output remains untouched on that unsupported boundary.

This package deliberately covers only the reached two-block luma slice. It does
not implement U16 chroma prediction or reconstruction, broaden prediction and
transform coverage, run filters, or expose successful public 10-bit output.

## Reached fixture

The locked input is
`tests/fixtures/10bit/avm-reference/tiny-constrained-16x16-yuv420p10le-qp208.obu`.
It contains two 8x16 luma leaves using DC prediction and DCT_DCT. Tile syntax
ends at range tell 192. The complete 16x16 little-endian AVM Y plane has SHA-256
`98d9e42de8a9aef3a649194227d876f1775c97a82272260c7e4cf051247ec219`.

No predictor or reference samples are loaded from the fixture. The first block
uses the unavailable-neighbor 10-bit midpoint of 512. The second block derives
DC 495 as the rounded mean of the first block's 16 reconstructed right-edge
samples.

## Block checkpoints

The focused test hashes little-endian U16 predictor/reconstruction samples and
little-endian two's-complement I32 residuals with FNV-1a:

| Block | Predictor | Predictor hash | Residual hash | Reconstruction hash |
| --- | ---: | --- | --- | --- |
| left 8x16 | 512 | `991b202dc315ab83` | `6cad529a8d799783` | `71b06bc83fc42a83` |
| right 8x16 | 495 | `580cec945426b383` | `ef5edb1a2c5b3783` | `636ef00a26871383` |

The test also compares every one of the 256 reconstructed Y samples directly
against the pinned AVM plane and verifies all 16 luma readiness bytes.

## Arithmetic contract

The reached TX_8X16 path uses the existing transform kernels with bit-depth
parameters instead of hard-coded 8-bit clamps. Its pass shifts are 7 and 11;
rectangular scaling uses 2896 with 12-bit rounding. The first pass clamps to
signed `bit_depth + 8` bits, the second pass clamps to
`[-2^bit_depth, 2^bit_depth - 1]`, and reconstruction clips to 0 through 1023.

U8 callers retain bit depth 8 and their existing arithmetic. The U16 owner is
allocation-free and commits samples through typed U16 plane operations while
sharing the existing byte readiness map.

## Integrated boundary

The private frame path prepares a two-byte-per-sample workspace and routes luma
leaves to the U16 reconstruction owner. It parses and commits all chroma syntax
without writing U or V samples, then returns `AV2H_UNSUPPORTED` at
`AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT`. The result records entropy tell
192 and two luma leaves, while the coded U16 Y plane remains available for exact
comparison.

The public decode diagnostic consequently reports
`AV2H_CONTEXT_RECONSTRUCTION`, and all 768 caller output bytes retain their
sentinel value. Successful chroma reconstruction and output publication remain
owned by 10B-WP5.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp4
```

It runs the leaf, tile, reconstruction, workspace-accounting, and public decode
tests under ASan/UBSan, followed by the WP0 real-decoder nonmutation oracle.
The ordinary `make test` suite remains the complete regression gate.
