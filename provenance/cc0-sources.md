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
in this repository are project-owned CC0 code written for this experiment.