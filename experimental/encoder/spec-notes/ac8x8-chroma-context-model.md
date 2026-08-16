# General chroma AC context model

Status: independent model complete; production admission is **not** approved.

`tools/ac8x8_chroma_context_model.py` extends the completed scalar 8x8 AC
work to the DCT_DCT chroma entropy surface used by I420 and I444. It covers
4x4, 4x8, 8x4, 8x8, 8x16, and 16x8 transforms, deriving every diagonal scan
and checking it against the pinned AVM scan tables.

The important chroma specialization is that the 2-D LF condition is
`row + column < 1`: only DC uses the LF UV tables. Every AC coefficient,
including scan position 1, uses the regular UV EOB/base/BR contexts. U
contexts are 0..3 and V contexts are 4..7; BR contexts are shared 0..3.
EOB position contexts are 0 at DC, 1 through one eighth capacity, 2 through
one quarter, and 3 afterward. Chroma has no parity hiding and no TCQ.
U and V signs are literal bits. High-range remainders retain the adaptive
Rice/Golomb average from the completed 8x8 model.

Neighbor state stores saturated cumulative magnitude plus DC sign over every
covered 4x4 unit. It supplies skip and DC-sign contexts, is updated after a
chosen transform block, and is cleared at the normative superblock/tile
boundaries. V's skip context additionally adds 6 when the corresponding U
block has nonzero EOB, so U must be coded and committed before V.

Frozen evidence contains 12 full-surface entropy vectors and 12 scalar
low-frequency AC rate/distortion probes. The probes show the distortion
headroom of one horizontal or vertical DCT AC, but deliberately do not claim
bit-exact rectangular reconstruction.

Low-frequency scalar chroma DCT AC is safe to admit only as an estimator and
candidate-ranking model. Production writing remains blocked until all of
these boundaries are implemented and proved together:

1. bit-exact 4/8/16 rectangular forward/inverse transform and dequant parity;
2. caller-owned tile CDF state for skip, EOB, base, BR, remainder, and signs;
3. U-before-V EOB coupling and transactional neighbor-state commit;
4. superblock/tile reset and cropped-edge entropy propagation;
5. CCTX, qmatrix, delta-q, prediction, and full-stream local/AVM/project gates;
6. an equal-rate gate, because the completed luma 8x8 production candidate
   already showed large SSE gains but unacceptable aggregate byte growth.

No production `m3`, `m4`, or syntax path is changed by this work.

Run:

```sh
python3 experimental/encoder/tools/ac8x8_chroma_context_model.py --check
python3 -m unittest experimental/encoder/tests/test_ac8x8_chroma_context_model.py
```
