# Milestone 5: first complete decode slice

Milestone 5 is in progress. The freestanding AV2 entropy decoder, Sequence
Header state, locked Closed Loop Key frame-header parser, and one-tile boundary
derivation are implemented and independently checked. Tile entropy syntax,
reconstruction, filter execution, workspace sizing, and the public decode API
are not yet implemented. The milestone exit gate has therefore not been
reached.

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

## Raw syntax and tile framing

The checked bit reader now implements UVLC and signed VLC values, inverse
signed literals, quasi-uniform symbols, and strict zero-padding alignment.
Focused sanitizer tests cover maximum-length and truncated UVLC prefixes,
signed boundaries, complete non-power-of-two quasi-uniform alphabets, and
valid and invalid alignment padding. Carriage parsing uses these shared
primitives rather than maintaining private copies.

`src/decoder/tile.c` implements the locked one-tile tile-group boundary. Given
the parsed uncompressed-header length, it validates the first tile-group flag,
skips the parsed syntax, consumes only zero alignment padding, and returns the
exact entropy span. Fixture-derived tests cover all four payload offsets and
reject bad padding, a non-first group, empty entropy data, and truncation.

## Sequence Header state

The existing public Sequence Header summary remains unchanged. A private
decoder state now retains the dimensions and tool flags needed to parse frame
syntax and initialize reconstruction. Its values are checked for all four
locked Sequence Headers against the hosted AVM inspection build.

The common profile enables SDP, MRL, FSC/IDTX, IST, CFL, MHCCP, and CCTX, while
TCQ, parity hiding, restoration, extended segmentation, sequence segmentation
data, and sequence tile data are disabled. Filter availability varies by
fixture: CDEF, GDF, and CCSO therefore remain real parser branches rather than
profile constants. The frame traces likewise show both disabled and enabled
intraBC.

## Closed Loop Key frame header

`src/decoder/frame.c` parses the complete uncompressed header reachable by the
locked still-picture profile. It cross-checks the multi-frame and Sequence
Header IDs, derives dimensions, consumes screen-content and intraBC controls,
derives one-tile signaling from dimensions and superblock size, and parses
quantization, deblock, GDF, CDEF, CCSO, transform mode, and reduced-transform
set syntax. Unsupported segmentation, delta-Q, custom quant matrices,
restoration, lossless coding, and multi-tile paths fail closed.

The parser independently derives uncompressed-header lengths of 49, 148, 101,
and 65 bits for the tiny, odd, ordinary, and high-detail fixtures. Those values
match the pinned AVM oracle and are used only as test expectations. Production
code contains no fixture-specific offsets. Every shorter byte prefix is
rejected, and the parsed length feeds the strict tile-boundary parser directly.

`tests/frame_unit.c` and `tests/tile_unit.c` run under AddressSanitizer and
UndefinedBehaviorSanitizer. The parser is also part of the strict
`-ffreestanding -nostdinc` zero-undefined-symbol core.

## Initial CDF state

The first production CDF primitive expands the normative binary table row
`{threshold, parameter_index, count}` into the inverse-CDF and three adaptive
rate offsets used by the range decoder. The parameter-index expansion follows
the pinned specification's `Para_Adjustment_List`, whose 125 entries are the
base-5 product of `{0, -1, -2, 1, 1}`.

The six normative attachments used for partition initialization are locked by
URL and SHA-256 in `spec/cdf-attachments.lock`. They remain ignored reference
inputs under `build/reference/`; no downloaded header is included by the core.
`tools/check_m5_partition_cdfs.py` semantically compares every locked
partition row with pinned AVM's hosted defaults. `tests/cdf_unit.c` independently
checks representative rows from each table and malformed specifications under
sanitizers and the freestanding dependency gate.

The complete partition-only CDF state occupies 6,336 caller-owned bytes. Its
tracked production constants are generated by
`tools/generate_m5_partition_cdfs.py`; regeneration from the hash-locked
attachments is byte-identical.

## First partition syntax

The private ordinary-partition prefix reader now consumes `do_split`,
`rect_type`, and `do_ext_partition` with caller-owned range and CDF state. It
accepts only the validated `do_split = 1`, `do_ext_partition = 0` path and
fails closed on a leaf or extended partition until those paths are complete.

Hosted AVM inspection established an important edge distinction. The 160x120
and 96x80 fixtures signal at their 64x64 root with contexts `(12, 24, 12)`.
The 16x16 and 17x15 roots instead pass through four and three normative
edge-forced ancestors, respectively; their first signaled nodes use contexts
`(4, 0, 4)` and `(8, 8, 8)`. All four fixture-derived entropy prefixes decode
to the same values and CDF mutations as the pinned oracle under sanitizers.

This remains a prefix checkpoint rather than a partition-tree claim. Mode and
coefficient syntax is interleaved at each leaf, so later partition decisions
cannot be reached honestly until the corresponding leaf parser advances the
same entropy decoder.

## Remaining work

The next required implementation begins with normative block geometry,
edge-forced ancestors, neighbor partition contexts, and the first leaf syntax.
Hosted
evidence must next expand to partition and mode maps, coefficient checksums,
and plane hashes after reconstruction, deblock, CDEF, CCSO, and restoration.
The implementation then proceeds through coefficient decoding, inverse
transforms, prediction, reconstruction, filter execution, workspace planning,
and finally the public decode API.

Until those stages reconstruct all four fixtures sample-exactly, Milestone 5
must remain marked in progress.