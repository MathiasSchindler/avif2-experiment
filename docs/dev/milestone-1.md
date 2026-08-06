# Milestone 1: freestanding substrate and carriage core

Milestone 1 implements the vendor-free byte-carriage layer for the
AV2-HEIF experimental v0 profile. It does not parse HEIF item structures or
decode AV2 picture syntax.

## Implemented core

The Milestone 1 core contains:

- checked size arithmetic, byte and bit readers and writers, memory primitives,
  and a caller-backed arena;
- canonical one-to-eight-byte AV2 LEB128 parsing and writing;
- ordered length-delimited OBU framing with extension, type, temporal,
  media-layer, and extended-layer metadata;
- AV2 type-specific layer-ID checks, including global Temporal Delimiter and
  Multi-Stream Decoder Operation semantics;
- the supported single-picture Sequence Header subset needed to derive v0
  profile, level, chroma, bit depth, dimensions, and feature flags;
- Content Interpretation color metadata with CICP defaults and preset IDs;
- draft-shaped nine-byte `av2C` parsing and deterministic writing;
- fixed-field cross-checks between `av2C` and its Sequence Header OBU;
- strict v0 carriage validation; and
- checked 32-bit, extended-size, and end-of-input BMFF box headers.

The carriage API and data model are declared in `include/av2heif.h`. Generic
substrate helpers remain internal in `src/base.h`.

The strict carriage profile requires exactly one unextended temporal-layer-zero
Sequence Header OBU in configuration and exactly one unextended
temporal-layer-zero Closed Loop Key OBU with a nonempty payload in item data.
Film grain and unsupported Sequence Header branches are rejected rather than
silently normalized.

The core uses caller-owned spans, output buffers, ledgers, and arenas. It does
not allocate memory, perform file I/O, use global mutable state, or include
vendored codec headers.

## Diagnostics and bounds

Status values distinguish invalid arguments, truncated input, malformed data,
integer overflow, caller capacity limits, output exhaustion, and unsupported
features. Parser diagnostics include the byte offset, parser context, and OBU
index and type when an OBU can be identified.

LEB128 inputs are canonical and limited to eight bytes. All offset, extent,
writer, and arena calculations use checked arithmetic. `av2C` validity is
independent of the caller's ledger capacity: malformed extra configuration
OBUs are rejected as data errors before the optional output-capacity result.

## Validation

`make test` runs without AVM or any other vendored software. It covers:

- sanitizer builds with AddressSanitizer and UndefinedBehaviorSanitizer;
- all legal LEB128 widths plus noncanonical, truncated, overwide, and writer
  overflow cases;
- OBU truncation, extension headers, zero sizes, layer rules, global-layer
  semantics, empty coded payloads, forbidden order, duplicates, and precise
  extra-OBU diagnostics;
- all four corrected Milestone 0 Sequence Headers, Sequence Header truncation,
  and film-grain rejection;
- Content Interpretation parsing, reserved color-description defaults, and
  truncation;
- `av2C` sizing, output exhaustion, reserved bits, field mismatches, film
  grain, extra OBUs, caller-capacity independence, and round trips;
- caller-arena sizing over every base alignment modulo 16; and
- normal, truncated, malformed, extended-size, and overflow BMFF box headers.

The same target compiles each core source with C11, `-ffreestanding`,
`-nostdinc`, no builtins, no stack protector, and no unwind tables. It links a
relocatable aggregate and requires `nm -u` to produce an empty result.

## Limitations

The Sequence Header parser intentionally supports only the initial
single-picture 8-bit 4:2:0 v0 subset and rejects unsupported branches. A
nonempty Closed Loop Key payload is a carriage check, not a proof that its full
picture syntax is decodable or self-sufficient; that proof remains part of
later decoder work. Full HEIF boxes, item/property associations, deterministic
muxing, fuzzing, and cross-compiler build coverage are also future milestones.