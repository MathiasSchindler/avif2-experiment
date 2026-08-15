# M3 grayscale DC transform model (AV2 v1.0.0)

Status: the decoder-side DC path, coefficient transcript, contexts, selected
CDF rows, and a canonical forward encoder model are executable. No AVM source
or implementation table was read or copied.

## Normative boundary

AV2 v1.0.0 specifies decoding, not a unique forward transform or quantizer.
Consequently “the exact forward transform” means an encoder choice that is
exactly inverse-compatible with the normative reconstruction. For a residual
block `r[i] = source[i] - prediction`, this model uses:

```text
4x4 forward DC = 2 * sum(r[i])
8x8 forward DC =     sum(r[i])
```

For a constant residual `r`, these are `32*r` and `64*r`. The normative
inverse DCT has DC kernel value 64 in both dimensions:

```text
row = Clip3(-32768,32767,Round2(64*DequantDC,7))
residual4 = Clip3(-256,255,Round2(64*row,10))
residual8 = Clip3(-256,255,Round2(64*row,11))
recon = Clip3(0,255,prediction + residual)
```

Thus `DequantDC=32*r` reconstructs `r` in 4x4 and `DequantDC=64*r`
reconstructs `r` in 8x8 over the useful 8-bit range. Rounding plateaus mean
there is not a unique coefficient. `choose_dc_level()` canonically minimizes
block SSE after the complete local inverse and chooses the smaller magnitude
on a tie; this is safer than assuming one forward reciprocal.

## Quantization and dequantization

With equal luma AC/DC quantizers, no matrix, no TCQ, and a 4x4 or 8x8 square:

```text
qlookup(q<25) = {64,40,41,43,44,45,47,48,49,51,52,54,55,
                 57,59,60,62,64,66,68,70,72,74,76,78}[q]
qlookup(q>=25) = table[((q-1)%24)+1] << ((q-1)/24)
q = qlookup(Clip3(1,255,qindex+delta))
special case: qindex==0 && delta<=0 gives q=64
dq = Round2(abs(level)*q,3)
DequantDC = Clip3(-32768,32767,(level<0 ? -dq : dq))
```

The specification defines no forward rounding rule. The executable model
searches all distinct useful signed levels through the normative dequant,
inverse transform, prediction addition, and clipping. `base_q_idx`, not the
per-block delta-Q result, selects coefficient CDF group 0/1/2/3 at boundaries
90, 140, and 190.

The configurable M3 encoder accepts `base_q_idx` 1 through 255. The sequence
configuration is invariant across this range; the frame header writes the
selected base index, `delta_q_present=0`, so every block uses that same index.
The dequantizer, exhaustive useful-level search, and coefficient CDF group all
derive from it. Index 0 is deliberately excluded because AV2 lossless mode
changes transform semantics. The compatibility wrapper and CLI default remain
100.

## DC-only coefficient transcript

For intra luma with `fsc_mode=0`, `eob=1` implies `DCT_DCT`; no transform-type
symbol or secondary transform symbol is read. Every normative scan begins
with raster position zero, so only `scan[0]=0` matters.

For zero DC:

```text
all_zero=1
```

For magnitude `m>0`:

```text
all_zero=0
eob_pt_16=0 (4x4) or eob_pt_64=0 (8x8)  -> eobPt=1, eob=1
coeff_base_lf_eob=min(m-1,4)             -> initial level 1..5
if m>=5: coeff_br_lf=min(m-5,3)          -> level through 8
dc_sign=(level<0)
if m>=8: read_quant literal remainder x=m-8
```

For the first high-range coefficient, `m=1`, `k=2`, and `cMax=5` in
`read_quant`. `m3_dc_model.py` emits the exact `q_length_bit`,
`golomb_length_bit`, and `coeff_rem` literals.

Context selection for a transform block equal to its prediction block is:

```text
all_zero: TileTxbSkipCdf[0][txSzCtx][0], txSzCtx=0 for 4x4, 1 for 8x8
eob:      TileEobPt16Cdf[0] or TileEobPt64Cdf[0]
base-EOB: c=0 -> ctx=SIG_COEF_CONTEXTS_EOB-4=0, low-frequency luma row
BR:       pos=0, all referenced levels zero -> mag=0, ctx=0, LF row
DC sign:  ctx=1 if neighboring DC categories sum negative,
          ctx=2 if positive, otherwise 0
```

`CDF_ROWS` in the tool contains every selected row for all four q groups.
Rows retain the normative `(Q15 thresholds..., parameter_index, count)`
representation. The source attachment hashes are:

```text
dct_kernel4.h                  d300b3952e3d8f51e4347c554a3a818c1c251bfe916e6c37d52d3b769e1553cb
dct_kernel8.h                  e89f5aac7056f3a8e55ee93a855520e3efb6392666a611563c17be3d9b86d560
default_txb_skip_cdf.h         0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0
default_eob_pt_16_cdf.h        a06828f9166ee168f5e40fbe5025da2bc169d489648cbdc8ffe54277e853f883
default_eob_pt_64_cdf.h        cfaf9793bdae974a2233dd36d5c233a8a56dea4dc06ce24d5dc2725d3d7b8a4d
default_coeff_base_lf_eob_cdf.h 36dcaa7a6e355098c737a3187ec0038c54e0c609e080912cb620c3eda88d0e26
default_coeff_br_lf_cdf.h      fee755c055bd50681616f80618fb39ae47468f99a284259aac118b938d56943a
default_dc_sign_cdf.h          17d4545c3d60dbab3e10a2ca3a26d852c224a1ccd8170ce3daf089a2eb6161fa
```

## Prediction and boundaries

DC prediction is 128 with no available edge. With one edge it is that edge's
rounded average; with both it is the rounded average of all `w+h` samples.
For 4x4 and 8x8 the divisor is a power of two, so the normative
`approx_divide` reduces to `Round2(sum,log2(count))`. Reconstruction is clipped
after adding the residual. The vectors cover no-edge, top-only, left-only,
both-edge, positive/negative DC, non-integral ramp means, and 0/255 clipping.

For 8×16 and 16×8 frames, the 16×16 partition node is boundary-implied to
`PARTITION_VERT` or `PARTITION_HORZ`. The resulting rectangular prediction
block signals `do_split=0` and uses `TX_PARTITION_HORZ` or
`TX_PARTITION_VERT` to form two square 8×8 transforms. This avoids introducing
rectangular transform coefficient models.

For active dimensions in 1..16, boundary tests use the frame dimensions
rounded up to the AV2 four-pixel grid. An axis with at most eight coded pixels
is boundary-implied at the 16×16 node; nine through sixteen coded pixels remain
in the 16-wide leaf. Input samples outside the active plane replicate the last
active row or column to complete each 8×8 transform. Only active pixels
contribute to reported SSE, and reconstruction is cropped to the declared
width and height.

## Lossless alternatives

Lossless forces 4x4 transforms. With `fsc_mode=0`, the inverse WHT and `q=64`
reconstruct a constant residual exactly from `QuantDC=4*r`; it is not lossless
for arbitrary nonconstant pixels.

Lossless `IDTX` with FSC is mathematically simpler:

```text
Dequant = Round2(Quant*64,3) = 8*Quant
Residual[i] = Dequant[i] >> 3 = Quant[i]
```

It can therefore carry each pixel residual exactly. It is **not the simpler
first DC encoder path**: it stops being DC-only and requires FSC bob/base/BR,
per-position sign contexts, remainders, and up to 64 coded values per 8x8
block. The lossy `eob=1` DCT path has fewer symbols and is the smaller M3 step.

## Black-box AVM evidence

Pinned AVM v1.0.0 (`avmenc` SHA-256
`830c5f7a82e071560501183fb85ea9e2632ce7665c9033232ac64b546efb7e64`)
was run on 8x8 constant I400-as-I420 probes at QP
0, 1, 50, 100, 150, 200, 251, and 255 with TCQ, trellis, QM, FSC, IDTX,
palette, IntraBC, IST, and loop filters disabled. Reconstructions were always
constant; low-Q probes were exact and high-Q probes exhibited the expected
quantization plateaus and clipping. Separate lossless constant probes at
8x8/16x16/64x64 reconstruct exactly. These observations validate the
decoder-domain model but do not turn AVM's non-normative forward decisions
into requirements.

Run:

```sh
python3 experimental/encoder/tools/m3_dc_model.py --vectors
python3 experimental/encoder/tools/m3_dc_model.py --transcript -21 8 255
python3 -m unittest experimental/encoder/tests/test_m3_dc_model.py
```
