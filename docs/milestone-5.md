# Milestone 5: first complete decode slice

Milestone 5 is in progress. The freestanding AV2 entropy decoder is implemented
and independently checked, but frame syntax, reconstruction, filters, workspace
sizing, and the public decode API are not yet implemented. The milestone exit
gate has therefore not been reached.

## Fixture inventory

The pinned AVM v1.0.0 inspection build was run over IVF equivalents generated
from the four locked I420 sources with the exact laboratory encoder options.
The traces are local artifacts under `build/m5/inventory/`.

The union is substantially broader than a minimal DC-only profile:

- 4x4 through large rectangular block and transform shapes;
- every one of AVM's 16 reported primary transform types;
- primary and secondary transform signaling, CCTX, forward and reverse
  coefficient paths, EOB classes, Rice and Exp-Golomb values;
- directional, smooth, Paeth, MRL, CFL, and intraBC prediction;
- skipped and non-skipped transform blocks;
- active CDEF plus CCSO syntax; and
- odd dimensions and intraBC displacement vectors.

Palette is not used, segmentation remains zero, delta Q is absent, and every
fixture is one shown closed-random-access 8-bit 4:2:0 frame at base Q index
128. These observations narrow implementation, but no syntax path may be
accepted until every reachable reconstruction and filter operation is present.

## Entropy decoder

`src/decoder/range.c` implements the normalized AV2 arithmetic decoder with:

- inverse-Q15 CDF symbol decoding for alphabets of 2 through 16 symbols;
- AV2 probability scaling, including per-symbol minimum-range increments;
- exact bypass literals up to 32 bits;
- adaptive inverse-CDF updates with saturating observation count and three
  time-interval rate offsets;
- input supplied as one or more immutable spans;
- checked tell and truncation state; and
- no allocation, libc, system headers, global mutable state, or runtime helper
  dependency.

The module is internal until frame and tile ownership is defined. It does not
weaken carriage acceptance or expose a partial public decoder.

## Independent oracle

`tools/m5_entropy_oracle.c` is hosted-only and links the pinned AVM build to
generate deterministic evidence. The production module does not include or
link AVM code.

The frozen mixed transcript is:

```text
bytes: 2b 79 82 40
tell:  1, 4, 9, 10, 12, 13, 15, 25, 26
```

It combines 2-, 3-, and 5-symbol inverse CDFs with 1-, 5-, and 10-bit bypass
literals. The adaptive transcript uses bytes `38 79 80` and checks every
decoded symbol, updated CDF entry, observation count, and tell position across
eight updates. The frozen transcript is also decoded across every byte split,
and every shorter prefix must report truncation.

`tests/range_unit.c` runs under AddressSanitizer and UndefinedBehaviorSanitizer.
The normal strict gate compiles the module with `-ffreestanding -nostdinc` and
includes it in the zero-undefined-symbol relocatable core.

## Remaining work

The next required evidence is a hosted frame/tile transcript containing frame
flags, tile byte ranges, partition and mode maps, coefficient checksums, and
plane hashes after reconstruction, deblock, CDEF, CCSO, and restoration. The
implementation then proceeds through frame/tile syntax, partition state,
coefficient decoding, inverse transforms, prediction, reconstruction, filters,
workspace planning, and finally the public decode API.

Until those stages reconstruct all four fixtures sample-exactly, Milestone 5
must remain marked in progress.