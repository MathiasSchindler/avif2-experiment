# CC0 source provenance

The initial generic substrate was adapted from the sibling `avif-decoder`
checkout at commit `57667e4e01f62bc03593e2f84129eca428afaa7b`, which is
published under CC0-1.0.

Reviewed source paths:

- `src/base.c`
- `src/base.h`
- `src/shared/limits.h`
- `src/shared/stddef.h`
- `src/shared/stdint.h`

The adapted code provides checked size arithmetic, byte and bit readers,
caller-backed arenas, memory primitives, and freestanding standard type
headers. It was renamed to the `av2h_` namespace. AVIF defaults, capability
state, AV1 framing options, file I/O, platform code, and decoder state were not
copied.

The byte and bit writers, AV2 LEB128 and OBU carriage parser, Sequence Header
and Content Interpretation parsers, draft-shaped `av2C`, and BMFF box framing
in this repository are project-owned CC0 code written for this experiment. The
typed AV2-HEIF parser and deterministic writer in `src/container.c` are also
project-owned CC0 code written for this experiment. The hosted artifact adapter
and Python laboratory orchestration under `tools/` are project-owned CC0 code;
they remain outside the freestanding product boundary. The bounded dialect,
validation, and query implementation in `src/query.c` is project-owned CC0
freestanding code written for this experiment.

The AV2 arithmetic-decoder ownership and checked-span structure in
`src/decoder/range.c` was adapted from the sibling CC0 implementation in
`src/codec/av1_symbol.c` at the same commit. AV2 inverse-Q15 probability
scaling, exact bypass decoding, adaptive CDF state, tell semantics, and
validation were independently implemented for this project. Deterministic
bytes and state transitions are compared with a separately built pinned AVM
oracle through `tools/m5_entropy_oracle.c`; no AVM source or object is included
in the CC0 production core.

The raw UVLC, signed-literal, quasi-uniform, and alignment behavior and the AV2
Sequence Header field order were checked against the pinned AVM source. The
freestanding implementations are project-owned CC0 code and are validated by
independent fixture and boundary vectors in `tests/substrate_unit.c`. The
one-tile framing implementation in `src/decoder/tile.c` is likewise
project-owned CC0 code and is checked against fixture-derived AVM offsets.
The Closed Loop Key field order and directly controlling helper behavior were
checked against pinned AVM while implementing `src/decoder/frame.c`; no AVM
code is copied or linked. The project-owned parser is independently gated by
four exact header-length vectors, truncation checks, strict alignment tests,
and the freestanding undefined-symbol inspection.

The compact CDF initializer in `src/decoder/cdf.c` is written from the
normative AV2 v1.0.0 symbol-decoding equation, `Para_Adjustment_List`, and
default CDF attachments for alphabets of 2 through 16 symbols. Attachment URLs
and hashes are locked in
`spec/cdf-attachments.lock`; the separately licensed AVM copy is used only by
the hosted semantic equivalence checker.

The block catalog, MI dimensions, partition size adjustments, ordinary child
geometry, and boundary-forcing rules in `src/decoder/partition.c` are written
from the AV2 v1.0.0 conversion tables and partition parsing process. A locally
instrumented, separately licensed AVM inspection build supplies only hosted
equivalence evidence for node geometry, forced/implied provenance, contexts,
symbols, and entropy positions; no AVM partition code or table is included or
linked into production.

The first-leaf pre-mode controls and luma mode implementation in
`src/decoder/leaf.c` are project-owned CC0 code written from the AV2 v1.0.0
parsing process and hash-locked intraBC, CCSO, and luma-mode CDF attachments.
A separately licensed AVM inspection build supplies hosted symbol-order and
fixture correspondence evidence only. No AVM mode code, table, header, or
object is included or linked into production.

The FSC and first-transform `all_zero`/EOB continuation in
`src/decoder/leaf.c` and `src/decoder/coeff.c` is project-owned CC0 code
written from the AV2 v1.0.0 parsing processes and hash-locked normative CDF
attachments. Hosted checkers compare the compact rows with pinned AVM's
separately licensed defaults; no AVM coefficient code, table, or object is
included or linked into production.