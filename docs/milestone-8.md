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
Five streams have a pre-prefix range state that does not yet reproduce AVM and
therefore reject before the all-zero symbol. Flycatcher and louis now consume
their exact large-transform primary/secondary syntax and stop at coefficient
levels. Silene reaches its AVM-exact EOB but rejects before adjusted-size IST
entropy because a direct read would produce a different symbol and endpoint.
The exact new frontiers are:

| Stream | New large-transform frontier |
| --- | --- |
| `chinnathurai-128x128-qp112.obu` | 32x64 derived, reject before prefix at tell 9 |
| `chinnathurai-333x333-qp224.obu` | 64x64 EOB 1, implicit DCT_DCT, tell 9 |
| `cuckoo-431x287-qp080.obu` | 64x64 derived, reject before prefix at tell 9 |
| `emerald-480x240-qp128.obu` | 64x64 derived, reject before prefix at tell 17 |
| `flycatcher-223x149-qp176.obu` | 32x64 EOB 8 and IST 0 complete at tell 20 |
| `laarim-448x299-qp192.obu` | 64x64 derived, reject before prefix at tell 5 |
| `louis-301x222-qp144.obu` | 64x16 short-side 0 and IST 0 complete at tell 26 |
| `schwaebisch-417x277-qp208.obu` | 64x16 all-zero block complete at tell 17 |
| `silene-384x307-qp080.obu` | 64x64 derived, reject before prefix at tell 6 |
| `silene-479x383-qp176.obu` | 64x64 EOB 3, reject before IST at tell 13 |

After committing the all-zero block in the entropy-only traversal,
`schwaebisch-417x277-qp208` reaches a 32x16 leaf at `(4,0)`, tell 19. Full
decoding first needs 64x16 all-zero DC reconstruction, then the Q208 32x16
luma profile. `chinnathurai-333x333-qp224` next needs the Q224 64x64
coefficient-level profile.

The ordinary coefficient path now applies the normative 8-bit QTX lookup,
luma DC delta, large-transform post-dequant shift, and asymmetric
`[-32768,32767]` saturation. The same saturation applies to FSC/IDTX
dequantization.

Four artifact first blocks now complete coefficient parsing exactly:

| Stream | Transform | Levels | Coefficients | Nonzero / DC |
| --- | --- | ---: | ---: | --- |
| `cuckoo-157x105-qp160.obu` | 16x32 DCT_DCT | 31 | 35 | 4 / -6 |
| `emerald-192x96-qp208.obu` | 32x32 DCT_DCT | 24 | 27 | 3 / -2 |
| `louis-463x342-qp224.obu` | 32x32 DCT_DCT | 26 | 27 | 1 / +6 |
| `pastell-512x341-qp160.obu` | 16x32 DCT_FLIPADST | 53 | 65 | 10 / -8 |

`tests/artifact_profile_unit.c` freezes each mode, prefix, transform, level,
coefficient, and final tell endpoint. It also compares every quantized and
dequantized coefficient against the pinned AVM vectors. Representative DC
values are `-6/-1488`, `-2/-1984`, `+6/+9408`, and `-8/-1984`.

Across all 21 artifact streams, six GDF-mode-2 streams decode their first GDF
flag, CDEF strength index, and enabled first-unit CCSO controls at the exact
pinned-AVM endpoints. They now continue through mode, implicit large-block
FSC, and reached Palette=0 syntax to tells 17, 25, 20, 25, 32, and 9.
Four other streams complete their first coefficient block, and eleven remain
fail-closed at an earlier first-luma syntax frontier. The QP-192
`chimp-240x287` stream remains intentionally gated at tell 15 because its
partition range state diverges before mode syntax; consuming CCSO there would
decode different symbols from AVM.

The next exact blockers are large-transform prefix range-state parity,
silene's adjusted-size IST entropy, Q176 32x64, Q144 64x16, and Q224 64x64
coefficient levels, 64x16 all-zero reconstruction, the Q48 coefficient-level
profile for flycatcher, extended
32x32 EOB point 7 for laarim, Q-context-0 8x8 coefficient-prefix geometry for
pastell, and the Q112 coefficient-level profile for schwaebisch.

## Scope

- complete the high-detail fixture's luma and chroma traversal;
- implement every reached large or secondary transform and coefficient-range
  path;
- complete CFL and intraBC prediction, displacement, references, and sample
  reconstruction;
- replace sparse transform or CDF admission with fuller normative machinery
  where the reached frequency set makes sparse handling fragile; and
- retain deterministic failure for restoration and unreached profile features.

The next artifact work is to repair large-prefix and silene IST range-state
parity, then add the reached large-transform coefficient and 64x16 all-zero
reconstruction paths.
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