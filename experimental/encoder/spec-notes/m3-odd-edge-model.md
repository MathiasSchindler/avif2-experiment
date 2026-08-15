# M3 odd-edge model for dimensions 1..16

Status: exact decoder syntax/reconstruction recipe for monochrome DC-only
coding, plus a canonical forward encoder choice. This note does not change
production C.

## Normative coded geometry and partition path

AV2 v1.0.0 defines:

```text
MiCols = 2 * ((FrameWidth  + 7) >> 3)
MiRows = 2 * ((FrameHeight + 7) >> 3)
```

Therefore the reconstructed luma canvas is `8*ceil(width/8)` by
`8*ceil(height/8)`, even though the displayed frame remains `FrameWidth` by
`FrameHeight`. For dimensions 1..16, the prediction-block shape is exactly:

| Active dimensions | Coded prediction block | 8x8 transforms |
|---|---:|---:|
| `w<=8, h<=8` | 8x8 | 1 |
| `w<=8, h>8` | 8x16 | 2, raster vertical |
| `w>8, h<=8` | 16x8 | 2, raster horizontal |
| `w>8, h>8` | 16x16 | 4, raster 2x2 |

`partition_implied_at_boundary()` gives this exact top-left path:

```text
64x64 --HORZ--> 64x32 --VERT--> 32x32
32x32 --HORZ--> 32x16 --VERT--> 16x16
```

Those four boundary partitions consume no entropy symbols. From 16x16:

* 8x8 canvas: implied HORZ to 16x8, then implied VERT to 8x8.
* 8x16 canvas: implied VERT to 8x16.
* 16x8 canvas: implied HORZ to 16x8.
* 16x16 canvas: no implied partition.

The resulting 8x8, 8x16, 16x8, or 16x16 block then codes `do_split=0`
(`PARTITION_NONE`). This distinction matters: the boundary direction is
implied, while the final NONE decision is coded. The DC grid recipe then uses
8x8 transforms: an 8x8 block codes no transform split; the other shapes code
`do_split=1`, followed by the square four-way, vertical, or horizontal
transform partition respectively.

## Out-of-frame samples, prediction, and residual input

Samples outside the displayed width/height but inside the coded canvas are
not unavailable decoder samples. They are ordinary coded/reconstructed
samples. Only a coded-canvas top or left boundary makes an intra edge
unavailable.

The canonical forward model extends the active source to the coded canvas by
replicating the final active column and final active row:

```text
padded[y][x] = source[min(y,height-1)][min(x,width-1)]
```

AV2 does not normatively specify encoder source padding; this is the bounded
encoder recipe. It matches the pinned AVM's accepted odd-size, block-flat
probes and avoids assigning zero weight to coded samples that affect the
normative transform reconstruction.

Every transform residual input is a full 8x8:

```text
residual[y][x] = padded[blockY+y][blockX+x] - prediction
forwardDC = sum(residual[0..7][0..7])
```

Do not divide or weight by the active crop size. The first transform predicts
128. Later transforms use already reconstructed 8-sample top/left edges in
raster order. One-edge DC uses `Round2(sum,3)`. Both-edge DC uses normative
`approx_divide(sum,16)`; 16 is the transform-edge count, not `activeW+activeH`.
For this power-of-two divisor it is the rounded 16-sample average.

The intra edge preparation process extends a valid edge to `aboveLimit` or
`leftLimit` by repeating its last valid reconstructed sample. Its `maxX` and
`maxY` are based on `MiCols*4-1` and `MiRows*4-1`, so the coded padding is
included.

## Reconstruction, crop, and coefficient contexts

Dequantization and inverse 8x8 DC reconstruction are unchanged from
`m3-dc-transform-model.md`. The constant reconstructed value fills all 64
samples of each transform. Neighboring transforms predict from that complete
reconstruction. Finally copy only rows `0..FrameHeight-1` and columns
`0..FrameWidth-1`; the validated bounded encoder streams make both decoders
emit exactly `width*height` bytes.

For a lone 8x8 block, `all_zero` context is 0. For transforms inside a larger
prediction block:

```text
top  = Min(4, abs(above quantized DC)) or 0
left = Min(4, abs(left  quantized DC)) or 0
all_zero_context = (top + left + 3) >> 1
```

Thus the first split transform has context 1, not 0. DC-sign context sums
neighbor categories (`negative=-1`, `zero=0`, `positive=+1`) and selects 1
for a negative sum, 2 for a positive sum, otherwise 0. Above/left refer to
the raster transform grid, including transforms in the same prediction
block. EOB, base, BR, sign, and remainder syntax otherwise use the existing
8x8 DC transcript.

## Black-box probes and executable vectors

Pinned hashes:

```text
AV2 v1.0.0 HTML  b267c96181f40799ec1c866ca8de3e936804617b71ac19437430d516aa73f1e3
avmenc            830c5f7a82e071560501183fb85ea9e2632ce7665c9033232ac64b546efb7e64
avmdec            c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0
project decoder    c0c6329cc85de13c92d04a00deff8124c9b45dbe74da8a86ddc60d9d7b4de9df
```

AVM accepted 1x1, 7x9, 9x7, 15x15, 1x16, and 16x1 block-flat probes at
QP 100 with the documented tools disabled. `avmdec` reconstructed every
visible sample exactly and emitted 1, 63, 63, 225, 16, and 16 bytes. Locked
OBU/output hashes are in `tests/m3_odd_avm_probes.json`. The project decoder
was also exercised as a second black box and rejected these general
AVM-encoder streams at tile syntax; the report records that limitation. Its
acceptance of the bounded experimental streams is covered by the M3 encoder
and prepared-image gates.

Run:

```sh
python3 experimental/encoder/tools/m3_odd_model.py
python3 experimental/encoder/tools/m3_odd_model.py \
  --generate-avm experimental/encoder/build/m3-odd-avm-probes
python3 -m unittest experimental/encoder/tests/test_m3_odd_model.py
```

The pure model's locked vectors are
`tests/m3_odd_model_vectors.json`. Normative source: AV2 v1.0.0 sections
5.18.4.4 (image size), 5.20.3.1 (`read_partition` and boundary implication),
7.13.2.1 (intra edge preparation), and 7.13.2.10 (DC prediction).
