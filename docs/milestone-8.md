# Milestone 8: high-detail and advanced prediction

Milestone 8 is in progress. The first QP-dependent artifact coefficient paths
are pinned to AVM, while complete high-detail traversal and reconstruction
remain open.

## Verified QP-dependent checkpoint

Coefficient entropy now derives the four normative Q contexts from
`base_q_index`: 0 through 90, 91 through 140, 141 through 190, and 191 through
255. The reached Q2 and Q3 paths initialize their exact `txb_skip`, EOB,
EOB-extra, base, LF-base, BR-LF, and DC-sign CDF rows. Unpinned Q2/Q3 rows
reject before entropy consumption rather than falling back to Q1 tables.
Direct coefficient-prefix callers also reject a frame/CDF Q-context mismatch
before reading the range decoder.

Luma mode and transform-type parsing no longer carry the old QP-128
restriction. Large rectangular transform mapping follows AVM's orientation:
the vertical 16x32 short-side symbol 2 maps to `DCT_FLIPADST`. The reached
16x32 secondary-transform CDF is also initialized.

The normative large TX IDs `TX_32X64`, `TX_64X64`, and `TX_64X16` are now
derived for the reached artifact blocks. Their coded coefficient geometries
follow AVM's adjusted-size rules: 32x64 and 64x64 use 32x32 coefficient
storage with a 1024-entry EOB capacity and coefficient entropy context 4,
while 64x16 uses 32x16 storage with a 512-entry capacity and context 3.
32x64 and 64x64 use implicit DCT_DCT primary transforms. For horizontal
64x16, the 64-pixel side is fixed to DCT while the 16-pixel short-side symbol
is still decoded. All-zero initialization uses the same adjusted geometry.

The implementation remains fail-closed at the first unconfirmed entropy row.
Using the normative context 4 fixes four earlier pre-prefix divergences. The
remaining cuckoo path requires the normative two-bit extension of EOB raw
symbol 7 for the 512/1024 classes; with that extension it also reaches AVM's
EOB 87 at tell 23. Seven streams now consume exact large-transform EOB and
transform syntax before stopping at coefficient levels. Emerald and the
larger silene reach AVM-exact EOB endpoints but reject before adjusted-size
IST entropy because a direct read would produce a different symbol and
endpoint. The exact new frontiers are:

| Stream | New large-transform frontier |
| --- | --- |
| `chinnathurai-128x128-qp112.obu` | 32x64 EOB 22 and IST 0 complete at tell 19 |
| `chinnathurai-333x333-qp224.obu` | 64x64 EOB 1, implicit DCT_DCT, tell 9 |
| `cuckoo-431x287-qp080.obu` | 64x64 EOB 87 and transform complete at tell 23 |
| `emerald-480x240-qp128.obu` | 64x64 EOB 27, reject before IST at tell 23 |
| `flycatcher-223x149-qp176.obu` | 32x64 EOB 8 and IST 0 complete at tell 20 |
| `laarim-448x299-qp192.obu` | 64x64 EOB 49 and transform complete at tell 18 |
| `louis-301x222-qp144.obu` | 64x16 short-side 0 and IST 0 complete at tell 26 |
| `schwaebisch-417x277-qp208.obu` | 64x16 all-zero block complete at tell 17 |
| `silene-384x307-qp080.obu` | 64x64 EOB 46 and transform complete at tell 20 |
| `silene-479x383-qp176.obu` | 64x64 EOB 3, reject before IST at tell 13 |

The same raw-symbol-7 extension now covers TX_32X32. For
`laarim-256x171-qp096.obu`, two bypass bits extend the raw point to EOB point
8; EOB 126 and implicit DCT_DCT complete at tell 28. The reached Q-context-1
TX-context-3 base, LF-base, BR, BR-LF, EOB, and DC-sign rows then decode all
126 levels through tell 314 and all 82 signed/high-range coefficients through
tell 471. The pinned coefficient hash is `ec5538b90b63a6e7`.

`schwaebisch-417x277-qp208` now reconstructs its first 64x16 all-zero DC
block using adjusted 32x16 coefficient geometry. Its predictor and prefilter
are constant 128 with hash `d495a039fba13383`; the block ends at tell 17 and
the next `(4,0)` 32x16 leaf remains fail-closed at tell 19.
`chinnathurai-333x333-qp224` next needs the Q224 64x64 coefficient-level
profile.

The ordinary coefficient path now applies the normative 8-bit QTX lookup,
luma DC delta, large-transform post-dequant shift, and asymmetric
`[-32768,32767]` saturation. The same saturation applies to FSC/IDTX
dequantization.

Five artifact first blocks now complete coefficient parsing exactly:

| Stream | Transform | Levels | Coefficients | Nonzero / DC |
| --- | --- | ---: | ---: | --- |
| `cuckoo-157x105-qp160.obu` | 16x32 DCT_DCT | 31 | 35 | 4 / -6 |
| `emerald-192x96-qp208.obu` | 32x32 DCT_DCT | 24 | 27 | 3 / -2 |
| `laarim-256x171-qp096.obu` | 32x32 DCT_DCT | 314 | 471 | 82 / +67 |
| `louis-463x342-qp224.obu` | 32x32 DCT_DCT | 26 | 27 | 1 / +6 |
| `pastell-512x341-qp160.obu` | 16x32 DCT_FLIPADST | 53 | 65 | 10 / -8 |

`tests/artifact_profile_unit.c` freezes each mode, prefix, transform, level,
coefficient, and final tell endpoint. It also compares every quantized and
dequantized coefficient against the pinned AVM vectors. Representative DC
values include `-6/-1488`, `-2/-1984`, `+67/+2613`, `+6/+9408`, and
`-8/-1984`.

The Laarim block also reconstructs sample-exactly. Its D45 predictor hash is
`75290844cee19be7`, its prefilter hash is `51b573f06c63948a`, and the AVM
postfilter reconstruction evidence hash is `4ae64e670eb54810`. After the
block is committed, partition traversal reaches `(0,8)` 16x16 at tell 476
and rejects before unpinned mode/profile syntax.

The Q160 cuckoo and pastell first blocks now also reconstruct sample-exactly
and are committed in the artifact traversal. Their complete 16x32 sample
hashes are `fb97d9776de897f5` and `73f9aa733e29f1c7`, with first and last row
locks matching the pinned AVM samples. Cuckoo next reaches `(0,4)` 8x16 at
tell 41 and rejects before mode syntax at the unpinned Q160 8x16 profile.
Pastell next reaches `(0,4)` 16x32 Smooth, EOB 3, DCT_DCT, and secondary
symbol 0 through tell 78, then rejects before coefficient-level entropy.

`emerald-192x96-qp208.obu` now reconstructs its first five luma leaves
sample-exactly. The second 32x32 leaf has no above references and uses the
rounded mean of the first leaf's 32 right-edge samples as its DC value.
Because the sequence enables IBP, the first eight columns are refined with
the normative 32-wide `Ibp_Weights` row before the constant -8 inverse-DC
residual is added.

The following `(8,0)` 32x8 leaf uses 32 above references and no left
references. Its Q-context-3 EOB-256 prefix selects EOB 1, implicit DCT_DCT,
and the sole coefficient `q=-1`, `dq=-1984` through tell 46. The height-8
IBP weights refine its first two predictor rows, and the inverse transform
adds a constant -15 residual. The artifact test pins all 256 predictor and
reconstructed samples.

The reached TX_16X16 profile now derives normative ID 2, size context 2, and
the 256-coefficient EOB class. The `(10,0)` DC block completes its all-zero
symbol at tell 52, uses implicit DCT_DCT, initializes 256 zero coefficients,
and reconstructs a constant-98 predictor and prefilter surface exactly.

After that leaf is committed, `(10,4)` is a 16x16 Smooth block. Its mode ends
at tell 55 and EOB point 3 produces EOB 4 at tell 59. The primary CDF is
`intra_ext_tx_cdf[1][2]`, with default thresholds
`13759,26108,27688,29793,30265,31576`; primary symbol 1 maps to ADST_ADST and
ends at tell 61. STX symbol 0 uses `stx_cdf[0][2]`, whose AVM trace shows the
adapted cumulative row `30423,23962,17764`, and ends at tell 65 with no
secondary transform.

The LF 2D scan is `[0,16,1,32]`. Levels end at tell 69 and signs end at tell
72, producing `q/dq` values `+2/+3968` at 0, `+1/+1984` at 16, zero at 1,
and `-1/-1984` at 32. There are no BR reads, parity hiding, or further
high-range expansion; `cul_level=20` and DC is +2. The predictor is constant
98. The complete ADST_ADST prefilter surface matches pinned AVM with trace
hash `7959e335`, including the locked representative rows in the artifact
test. The next `(14,0)` 32x8 leaf is reached fail-closed at tell 74.

Across all 21 artifact streams, six GDF-mode-2 streams decode their first GDF
flag, CDEF strength index, and enabled first-unit CCSO controls at the exact
pinned-AVM endpoints. They now continue through mode, implicit large-block
FSC, and reached Palette=0 syntax to tells 17, 25, 20, 25, 32, and 9.
Five streams complete their first coefficient block, and the newly admitted
Emerald, Laarim, and Schwaebisch surfaces are sample-locked. The remaining
streams stay fail-closed at their earlier first-luma syntax frontiers. The QP-192
`chimp-240x287` stream remains intentionally gated at tell 15 because its
partition range state diverges before mode syntax; consuming CCSO there would
decode different symbols from AVM.

The next exact blockers include Emerald Q208 32x8 and Laarim Q96 16x16
luma-profile admission, emerald/silene adjusted-size IST entropy,
Q80/Q112/Q144/Q176/Q192/Q224 large-transform coefficient levels, the Q48
coefficient-level profile for flycatcher, Q-context-0 8x8 coefficient-prefix
geometry for pastell, the Q160 cuckoo 8x16 profile, the Q160 pastell 16x32
EOB-3 coefficient profile, and the Q208 Schwaebisch 32x16 profile.

## Scope

- complete the high-detail fixture's luma and chroma traversal;
- implement every reached large or secondary transform and coefficient-range
  path;
- complete CFL and intraBC prediction, displacement, references, and sample
  reconstruction;
- replace sparse transform or CDF admission with fuller normative machinery
  where the reached frequency set makes sparse handling fragile; and
- retain deterministic failure for restoration and unreached profile features.

The next artifact work is to admit only the newly reached Emerald 32x8,
Laarim 16x16, and Schwaebisch 32x16 profiles with matching AVM evidence, or
repair emerald/silene IST range-state parity, then continue the remaining
large-transform coefficient paths.
Traversal should continue beyond a block only where every newly reached CDF,
transform, coefficient, prediction, and reconstruction path has matching AVM
evidence. The ordinary Milestone 7 frontier remains ordinary chroma after the
completed 22-leaf luma path through tell 762.

## Exit gate

The high-detail fixture must match pinned AVM on complete unfiltered Y, U, and
V planes, including every reached CFL and intraBC block. The four-fixture set
must then have complete unfiltered parity under sanitizers and the strict
freestanding dependency gate.

Milestone 8 does not run mandatory frame filters and does not expose a public
decoder.