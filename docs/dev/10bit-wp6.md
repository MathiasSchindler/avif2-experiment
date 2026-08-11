# 10-bit implementation WP6 prediction and transform matrix

## Result

10B-WP6 is complete. Twenty-two retained 16x16 4:2:0 fixtures now decode to
complete unfiltered U16 Y, U, and V planes that match pinned AVM exactly. The
matrix includes 21 lossy quantizer variants and one coded-lossless stream.

This package does not implement filters. Public 10-bit decode still returns
`AV2H_UNSUPPORTED` with `AV2H_CONTEXT_FILTER` before mutating caller output.
Deblock is implemented by 10B-WP7 without changing this public boundary.

## Retained matrix

The lossy fixtures span Q indices 90 through 160 selected to expose distinct
prediction, transform, CfL/CCTX, FSC, palette, IntraBC, IBP, MRL, and secondary
transform paths. The generated AVM census for the retained set reaches:

| Surface | Retained values |
| --- | --- |
| Luma modes | D67, DC, Paeth, Smooth, Smooth Horizontal, Vertical |
| Chroma modes | D113, DC, Horizontal, Paeth, Smooth Vertical, Vertical |
| Transform sizes | 4x4, 4x8, 4x16, 8x4, 8x8, 8x16, 16x4 |
| Primary transforms | ADST_ADST, ADST_FLIPADST, DCT_ADST, DCT_DCT, H_ADST, H_DCT, H_FLIPADST, IDTX, V_DCT |

Focused reconstruction tests retain additional exact vectors for specialized
prediction, primary and secondary transform, CfL, CCTX, FSC, palette, and
IntraBC arithmetic. Dispatch remains bounded: an unrepresented 10-bit kernel
returns unsupported rather than reusing an unproved 8-bit path.

## Coded lossless

The Q0 fixture exercises the syntax and geometry that differ from ordinary
lossy reconstruction. DPCM is parsed before ordinary intra mode syntax. FSC
blocks consume the adaptive lossless transform-size decision and retain their
large transform geometry; non-FSC lossless luma and 4:2:0 chroma are traversed
as 4x4 transform grids.

Lossless FSC/DPCM uses scaled spatial IDTX residuals with cumulative horizontal
or vertical reconstruction. Ordinary non-FSC 4x4 blocks use AVM's reversible
inverse Walsh-Hadamard transform. Lossless DPCM 4x4 blocks use scaled IDTX and
axis accumulation without the Walsh-Hadamard stage. Ordinary lossless blocks
force DCT_DCT without consuming transform-type entropy, while FSC retains its
IDTX selection path.

The fixture also reaches bounded 10-bit directional D157 prediction and 4x4
zone-1 single-line MRL up to index 2. All 256 luma samples and both 64-sample
chroma planes match AVM exactly.

## Oracle provenance

Each `*-unfiltered.yuv` file is a 768-byte little-endian capture of AVM's
`prefiltered_frame_buffer` at revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. It contains 256 Y, 64 U, and 64 V
10-bit samples before deblock or later filtering. The complete SHA-256 list is
retained in `tests/fixtures/10bit/wp6-unfiltered.sha256`; the lossless oracle
hash is `1812389b0f37c974352265ffbfb7ac679a0bcfed56d0a5ff24ab890451dca487`.

## Validation

Run the focused package gate with:

```sh
make 10bit-wp6
```

It verifies coefficient, frame, leaf, tile, reconstruction, workspace, and
public boundary tests under the selected host preset, checks all retained
oracle hashes, and reruns the WP0 decoder nonmutation oracle. The complete
regression gate remains `make test`.