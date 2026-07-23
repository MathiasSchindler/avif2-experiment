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

Across all 21 artifact streams, six GDF-mode-2 streams still reject at tile
initialization, four now complete their first coefficient block, and eleven
remain fail-closed at an earlier first-luma syntax frontier. The QP-192
`chimp-240x287` stream remains intentionally gated at tell 15 because its
partition range state diverges before mode syntax; consuming CCSO there would
decode different symbols from AVM.

## Scope

- complete the high-detail fixture's luma and chroma traversal;
- implement every reached large or secondary transform and coefficient-range
  path;
- complete CFL and intraBC prediction, displacement, references, and sample
  reconstruction;
- replace sparse transform or CDF admission with fuller normative machinery
  where the reached frequency set makes sparse handling fragile; and
- retain deterministic failure for restoration and unreached profile features.

The next artifact work is to pin the remaining eleven first-luma frontiers,
then continue beyond the first block only where every newly reached CDF,
transform, coefficient, prediction, and reconstruction path has matching AVM
evidence. The ordinary Milestone 7 frontier at `(12,0)` remains independently
owned by Milestone 7.

## Exit gate

The high-detail fixture must match pinned AVM on complete unfiltered Y, U, and
V planes, including every reached CFL and intraBC block. The four-fixture set
must then have complete unfiltered parity under sanitizers and the strict
freestanding dependency gate.

Milestone 8 does not run mandatory frame filters and does not expose a public
decoder.