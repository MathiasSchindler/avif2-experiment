# Independent scalar 8x8 DCT_DCT AC model

Status: independent model complete; explicit I400 production candidate wired,
but the locked production admission gate is blocked.

`tools/ac8x8_model.py` models the complete non-TCQ, non-parity-hidden luma
coefficient surface for an intra 8x8 `DCT_DCT` transform:

- all 64 raster positions and the normative 2-D scan
  `0,8,1,16,9,2,...,55,63`;
- signed magnitudes 1 through 32767 (the bounded RD search uses 1 through 63);
- EOB classes 1 through 7 for capacities through 64, including adaptive
  `eob_extra` and literal offset bits;
- low- and high-frequency EOB base, regular base, BR, high-range Rice/Golomb
  remainder, DC-adaptive sign, and equiprobable AC sign syntax;
- decoder-equivalent LF/base/BR/EOB contexts and adaptive CDF updates; and
- the exact project range-writer state machine, `tell()` cost, and final byte
  flush.

The forward analysis is

```text
F[v,u] = Round2(sum((source[y,x]-prediction)*K[v,y]*K[u,x]), 12)
```

and inverse reconstruction is the normative two-pass kernel with shifts 7 and
11, signed rounding, intermediate `[-32768,32767]` clipping, residual
`[-256,255]` clipping, dequant `Round2(abs(level)*qlookup,3)`, and final
8-bit clipping. Every attachment used by the model is SHA-256 locked.

The deterministic search ranks analysis coefficients, tests the ideal scalar
level and its ±1/±2 neighbors, caps work at 16 positions, 8 nonzero
coefficients, and magnitude 63, and minimizes exact
`SSE + lambda * range_writer_bits`. Frozen evidence covers horizontal and
vertical edges, checkerboard, synthetic detail, deterministic noise, and a
photographic 8x8 block at qindex 50, 100, 150, and 210. Baselines are
strictly DC-only and the current M4 `{DC,(1,0)}` surface.

Run:

```sh
python3 experimental/encoder/tools/ac8x8_model.py --check
python3 -m unittest experimental/encoder/tests/test_ac8x8_model.py
```

`tests/ac8x8_vectors.json` contains 20 coefficient/range/reconstruction
vectors. `metrics/ac8x8-rd.json` contains 24 bounded searches.

The production implementation is in `src/ac8x8.c` and the generalized
coefficient writer is in `src/syntax.c`. `--search off` is the default and is
legacy compatible. `--search bounded` is admitted only for I400 and uses
caller-owned workspace, exact range-writer `tell()`, complete-stream staging,
and atomic output commit. Full-stream local/pinned-AVM/project parity covers
all four q contexts, crops, multileaf images, and multiple superblocks.

Default admission is blocked by the equal-rate gate.
`metrics/ac8x8-production.json` records 66.06% targeted AC-subset Y-SSE gain,
but only 7/9 required wins and 4,459 candidate bytes versus 786 baseline bytes
over the 18 targeted I400 points. A deterministic cross-case q/rate-budget
selector or materially sparser exact-rate trellis is required before
default-on or chroma admission.
