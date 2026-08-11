# 10-bit implementation WP5 chroma reconstruction

## Result

10B-WP5 is complete. The constrained 16x16 fixture now reaches tile-decode
completion with exact unfiltered U16 Y, U, and V planes. The private frame path
returns a tightly packed 768-byte `yuv420p10le` frame whose bytes match the
pinned AVM decode exactly.

This package deliberately stops before 10-bit deblock, CDEF, CCSO, and GDF.
Public 10-bit decode therefore remains unsupported and nonmutating at the
filter boundary. Broader chroma prediction, CfL, CCTX, and transform coverage
remain owned by 10B-WP6.

## Reached chroma slice

The locked input is
`tests/fixtures/10bit/avm-reference/tiny-constrained-16x16-yuv420p10le-qp208.obu`.
Its 4:2:0 chroma syntax contains two vertically stacked 8x4 leaves per plane.
Every reached leaf uses DC prediction, TX_8X4, DCT_DCT, nonzero coefficients,
no CfL, and no CCTX. Tile syntax ends at range tell 192 with two luma leaves and
two shared U/V chroma leaves.

The top U and V blocks have no reconstructed neighbors and use the normative
10-bit midpoint 512. The lower blocks consume the eight reconstructed samples
immediately above them. Their rounded DC averages are 442 for U and 451 for V.
No predictor or neighbor samples come from fixture-side test data.

## Exact evidence

The pinned little-endian plane hashes are:

| Plane | Bytes | SHA-256 |
| --- | ---: | --- |
| Y | 512 | `98d9e42de8a9aef3a649194227d876f1775c97a82272260c7e4cf051247ec219` |
| U | 128 | `410fe42ed07908d04009787be600b1fc30013afc32812225d4a95c26d4491840` |
| V | 128 | `8145a7b1d1b1b248005d5228c068a4b62c532e1bc0f7e920258d752e9a6290c8` |
| Y/U/V | 768 | `ca9ec6d9f6a901431de7364968b9b84ebf8ed34a832b167b66661add370f302b` |

The focused test compares every coded sample, every visible output byte, and
all readiness cells. It also records little-endian FNV-1a block checkpoints;
signed residual hashes use four-byte two's-complement values before final
sample clipping:

| Plane/block | DC | Predictor hash | Residual hash | Reconstruction hash |
| --- | ---: | --- | --- | --- |
| U top | 512 | `5ce0c12b6b7c2d83` | `df58948146da861a` | `b12e42a332bc880c` |
| U bottom | 442 | `6ff33cc138f318c3` | `78a01557af4ba533` | `3e8c15adaefe5dc3` |
| V top | 512 | `5ce0c12b6b7c2d83` | `040e27baa33e3047` | `53791fec93b3b8dc` |
| V bottom | 451 | `89ab94fc7cfd4183` | `d61bf628f17151db` | `3bfc5317a5ebf4fb` |

## Arithmetic and geometry

The reached TX_8X4 kernel uses rectangular scaling by 2896 with 12-bit
rounding, row and column shifts 7 and 10, a signed 18-bit first-pass clamp, a
10-bit second-pass clamp, and final clipping to 0 through 1023. It shares the
existing normative DCT kernels while retaining typed U16 predictor and sample
storage and signed 32-bit residuals.

For 4:2:0, luma leaf coordinates are converted to chroma sample coordinates by
a factor of two. Visible dimensions are bounded independently against the
chroma plane, preserving the existing ceil-divided frame geometry and coded
edge ownership. U and V reconstruct in syntax order from separate readiness
maps; the reached stream has no cross-plane sample dependency. Nonzero 10-bit
CCTX fails closed before the existing 8-bit cross-transform clamp can run.

## Private output and public boundary

After all three coded planes and deblock metadata are complete, the private
path crops each visible plane and writes explicit little-endian sample bytes in
Y, U, V order. For the selected fixture the offsets are 0, 512, and 640 and the
final size is 768 bytes.

The public API does not invoke that private output path yet. It returns
`AV2H_UNSUPPORTED` with `AV2H_CONTEXT_FILTER` before any caller output mutation,
because the normative 10-bit filter pipeline is not implemented. The original
WP0 decoder oracle continues to require an unchanged pre-existing output file.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp5
```

It runs frame, leaf, tile, reconstruction, workspace-accounting, and public
boundary tests under ASan/UBSan, followed by the WP0 real-decoder nonmutation
oracle. The ordinary `make test` suite remains the complete regression gate.
