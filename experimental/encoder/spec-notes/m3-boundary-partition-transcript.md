# M3 AVM boundary partition transcript

Status: exact mismatch derived, independently modeled, corrected in the
public 17..64 syntax path, and accepted by both decoders.

## Root cause

Partition geometry in `EncM3TileState` is measured in 4x4 MI units. The
private context helper mapped square widths 16, 8, and everything smaller to
split-size adjustments 3, 2, and 1. That is correct for 64x64, 32x32, and
16x16, but wrong for an 8x8 node: width 2 MI must use adjustment 0.

The final boundary 8x8 `PARTITION_NONE` therefore selected
`do_split[structure=0][context=4]` instead of context 0. The symbol value was
still zero. The wrong adaptive row and range subdivision, not a different
partition choice, caused the eventual AVM rejection.

The corrected square mapping is:

```text
MI width 16 -> split adjustment 3
MI width  8 -> split adjustment 2
MI width  4 -> split adjustment 1
MI width  2 -> split adjustment 0
```

This agrees with AVM `bsize_map`, the AV2 block geometry table, the project
decoder, and the independent model.

## Exact first differences

All values below are before coding `do_split=0`.

| class | event | node | position | tell | low | range | count |
|---|---:|---|---|---:|---:|---:|---:|
| 24x24 | 34 | 8x8 | (16,16) | 21 | 196896 | 43952 | -5 |
| 40x40 | 91 | 8x8 | (32,32) | 47 | 1052248 | 48008 | -3 |
| 56x56 | 181 | 8x8 | (48,48) | 78 | 1369624 | 54888 | -4 |

The correct initial row is
`do_split[0] = [4684, 0, count=0, rates=2,3,5]`.

* 24x24: correct range becomes 37712; row 4
  `[6435,0,count=3,rates=2,3,4]` produced 35320.
* 56x56: correct range becomes 47080; adapted row 4
  `[2969,0,count=15,rates=2,3,4]` produced 49864.
* 40x40 also selected the wrong row, but Q15 range quantization produced the
  same post-symbol state, 41184. Its bytes and decoder result were therefore
  unchanged.
* 32x32, 48x48, and 64x64 have no terminal boundary 8x8 `NONE` symbol and no
  transcript difference.

## Ruled-out partition causes

Pinned AVM source and instrumented minimal probes confirmed:

* square split is unavailable at these nodes;
* fully boundary-implied HORZ/VERT decisions consume no symbol;
* partial rectangular boundaries imply `do_split=1` but still code
  `rect_type` when both axes are legal;
* the project sequence enables extended and uneven partitions;
* ordinary partitions code `do_ext_partition=0` only when ordinary and
  extended choices are both legal, otherwise it is implied;
* rectangle and extension contexts use the selected axis and the correct
  first/second-half context entries; and
* every rectangle/extension CDF observed in pinned AVM matched the generated
  normative table before and after adaptation.

Thus rectangle selection, extension tool signaling, boundary implication, and
their CDFs were already correct. The first mismatch was solely the 8x8 split
context index.

## Legacy rejection offsets

The frozen legacy streams reproduce the original disagreement:

* 24x24 flat: AVM range overflow, tell 158 with a 4-byte tile; the project
  decoder reports truncated input at tell 33.
* 24x24 non-flat: AVM reaches tell 204, then rejects nonzero trailing padding
  byte 192 at tile offset 26; the project decoder accepts.
* 56x56 flat: AVM reaches tell 83, then rejects last byte 208 against trailing
  marker pattern 32; the project decoder accepts.
* 56x56 non-flat: AVM reaches tell 830, then rejects nonzero trailing padding
  byte 53 at tile offset 104; the project decoder accepts.

The different rejection forms are downstream effects of the same earlier
range subdivision. They are not range-finalization defects.

## Fixed parity

`tests/m3_boundary_partition_probes.json` freezes flat and deterministic
non-flat 24, 32, 40, 48, 56, and 64 square classes. Pinned `avmdec` and the
project decoder accept all 12 streams and emit byte-identical planes.
`tests/m3_boundary_partition_legacy_probes.json` freezes the pre-fix hashes and
rejections. `tests/m3_boundary_partition_vectors.json` freezes every relevant
CDF and range state.

The independent Python range coder is byte-exact with the C writer for every
flat class. The correction is confined to the >16 traversal, which is now
public through 64; the legacy 1..16 path does not call this helper.

Run:

```sh
make -C experimental/encoder build/m3-boundary-partition-driver
python3 -m unittest \
  experimental/encoder/tests/test_m3_boundary_partition_transcript.py
python3 experimental/encoder/tools/m3_boundary_partition_transcript.py \
  --probe experimental/encoder/build/m3-boundary-partition-probes
```
