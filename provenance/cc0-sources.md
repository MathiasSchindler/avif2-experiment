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