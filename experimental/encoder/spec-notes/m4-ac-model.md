# M4 8x8 grayscale DC plus first-AC model (AV2 v1.0.0)

Status: PASS for the bounded C stream and both decoder gates. The forward
choice, normative inverse, 2-D scan, quantization, coefficient transcript,
and freestanding C implementation are executable. No AVM source or C
implementation was read or copied.

The selected input is an 8x8 vertical gradient, with rows of constant values
`128 128 128 129 129 129 130 130`, predicted by DC value 128. DCT_DCT uses
the normative `Dct_Kernel8`. A canonical encoder analysis coefficient is:

```text
F[v,u] = Round2(sum_y,x((source[y,x]-128) * K[v,y] * K[u,x]), 12)
```

The `/4096` scale follows from the two inverse kernel norms and gives
`F[0,0] = sum residual`. Only raster coefficients 0 `(0,0)` and 8 `(1,0)`
are retained; these are scan positions 0 and 1. Candidate quantized levels use the bounded base-only syntax's complete
encodable nonzero range, `[-4,-1] U [1,4]`. Every pair is decoded and the pair
with minimum pixel SSE is selected, with deterministic lower-magnitude tie
breaks. BR/remainder syntax is not emitted by this M4 subset.

For each signed level `L`:

```text
dq = sign(L) * Round2(abs(L) * qlookup(qindex), 3)
row[y,x] = Clip3(-32768,32767,Round2(sum_j K[j,x]*C[y,j],7))
res[y,x] = Clip3(-256,255,Round2(sum_i K[i,y]*row[i,x],11))
recon = Clip3(0,255,128+res)
```

The normative 2-D scan starts `0,8,1,16,9,2,...`; therefore the retained
vertical AC `(1,0)` is scan position 1 and the model has `eob=2`, encoded as
`eob_pt_64=1`. Coefficients are decoded backwards: AC position 1 uses
low-frequency EOB-base context 1, then DC position 0 uses
low-frequency base context `(min(abs(AC),5)+1)>>1`. AC BR context is 7
(all forward reference levels zero); DC BR context is
`min((min(abs(AC),7)+1)>>1,6)`. The AC sign is an equiprobable literal and
the boundary DC sign context is 0. The JSON transcript includes emitted BR
symbols and explicit non-emitted BR summaries.

At `qindex=50`, the selected exact-reconstruction pair is DC `+3` and AC
`-2`, dequantized to `+62` and `-41`, with SSE 0. The coefficient transcript
is:

```text
all_zero=0                         ctx=0
eob_pt_64=1                        ctx=0 -> eob=2
coeff_base_lf_eob=1 (AC level 2)  ctx=1
coeff_base_lf=3 (DC level 3)      ctx=1
AC sign_bit=1                      equiprobable
dc_sign=0                          ctx=0
AC coeff_br_lf                     ctx=7, not emitted
DC coeff_br_lf                     ctx=1, not emitted
```

The script hash-checks every normative attachment it uses. Run:

```sh
python3 experimental/encoder/tools/m4_ac_model.py --transcript
python3 -m unittest experimental/encoder/tests/test_m4_ac_model.py
```

The experimental CLI emits a 21-byte stream with SHA-256
`9d84278811a478aaece5b87bf9340e47d1dfd20badc6af0765776840f8d98c15`.
Local reconstruction, pinned `avmdec`, and the project decoder all emit the
exact source plane, SHA-256
`fa5e2dc3cd94a1e52711093de4db4bc9d7d1124d86c2c4389534521761edbf93`.
SSE is 0 and luma PSNR is infinite. Focused native/model/decoder tests and the
existing static no-libc audits cover the path.
