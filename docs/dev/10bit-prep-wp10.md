# 10-bit preparation WP10 synthetic reference matrix

WP10 was implemented and validated on 2026-08-11. It adds a compact,
deterministic 10-bit evidence corpus without changing the decoder's accepted
profile.

## Source contract

`tools/generate_10bit_synthetic.py` uses only the Python standard library. It
generates `tiny-precision` 16x16, `odd-geometry` 17x15,
`boundary-extremes` 65x63, and `high-detail` 96x80 in both `yuv420p10le` and
`yuv444p10le`. Storage is headerless planar unsigned 16-bit little-endian,
with tightly packed Y, U, and V planes and ceil-divided 4:2:0 chroma.

Every plane independently contains 0, 1, 2, 3, 255, 256, 511, 512, 1020,
1021, 1022, and 1023. Distinct U and V functions cover flat fields, horizontal
and vertical ramps, checkerboards, impulses, hard 64-pixel edges, clipping
extremes, and high-frequency detail. `source-manifest.json` pins each file and
plane hash, offset, size, extrema, distinct-value count, sentinel set, and low
two-bit exposure.

## Pinned reference workflow

The positive workflow uses AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. `scripts/setup-avm.sh` builds the
reference encoder and decoder. `scripts/setup-avm-accounting.sh` creates a
separate decoder library with `CONFIG_INSPECTION=1` and
`CONFIG_ACCOUNTING=1`, then links `av2-census`; encoder-only ML experiments are
disabled in that build so census has no TensorFlow Lite link dependency.

`tools/generate_10bit_synthetic_references.py` encodes and decodes both tiny
sources at every integer QP from 90 through 160. In ascending order it retains
the first QP, every QP that adds a quantizer-band or syntax atom, and QP 160.
The selected sets are:

- 4:2:0: 90, 91, 92, 93, 94, 96, 99, 104, 105, 112, 121, 122, 124, 125,
  128, 131, 134, 139, 144, 148, and 160;
- 4:4:4: 90, 91, 96, 97, 105, 111, 112, 115, 119, 128, 133, 136, 137, 139,
  144, 146, and 160.

Each of the six larger source/format pairs is retained at QP 128. AVM QP 0
was tested but was not byte-exact. Explicit `--lossless=1` streams are stable
and exactly reconstruct both tiny sources, so both are retained. The permanent
matrix contains 46 OBU/decoded-output pairs. Its manifest pins source and tool
hashes, commands, OBU ledgers, decoded planar hashes, per-plane errors, and
census summaries.

## Measured coverage

The accounting census reports 13 luma prediction modes and 10 chroma modes,
including CfL. It records 16 primary transform types across 16 transform sizes,
30 distinct Q-indices from 0 through 208, and nine CfL parameter tuples. The
filter-accounting search finds the CCSO `blk_idc` control with value 1. These
are observed properties of this corpus, not claims of complete syntax
coverage. `coverage-report.json` retains all 142 sweep summaries and the exact
selection inputs.

## Negative fixtures

`scripts/setup-avm-negatives.sh` makes an isolated build of the same pinned
revision with `CONFIG_TESTONLY_12BIT_SUPPORT=1`. The negative generator uses it
to create independently `avmdec`-valid 12-bit 4:2:0 and 10-bit 4:2:2 streams.
It also changes the 12-bit stream's sequence profile to an inconsistent
profile/chroma combination and removes one byte for a truncated frame case.

The ASan/UBSan decoder returns exit code 3 with `unsupported feature` for the
12-bit, 4:2:2, and inconsistent-profile files. It returns exit code 1 with
`truncated input` for the truncated file. No AddressSanitizer or Undefined
BehaviorSanitizer diagnostic is emitted.

## Reproduction and validation

Run the complete generation and focused checks with:

```sh
make 10bit-wp10
```

The source directory, selected reference directory, and negative directory are
byte-identical after a second complete generation. Focused Python tests verify
source geometry and samples, selection semantics, explicit coding profiles,
fixture hashes, expected decoder statuses, and sanitizer cleanliness. Every
permanent positive decodes with the pinned `avmdec`, has the expected plane
size, and contains only samples in the range 0 through 1023.