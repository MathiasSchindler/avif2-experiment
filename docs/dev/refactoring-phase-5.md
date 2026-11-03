# Refactoring Phase 5: leaf syntax and context state

**Status:** Complete as of 2026-08-04.

Phase 5 splits leaf syntax by responsibility without changing public
structures, adaptive-read order, failure precedence, diagnostics, state layout,
or decoded output.

## Test prerequisite

The disabled 1,619-line whole-stream fixture in `tests/leaf_unit.c` was removed.
The active suite already provides focused vectors for mode ordering, argument
and CDF initialization, filter controls, CfL branches, IntraBC bank lifetime,
morph defaults, and frame-edge neighbor state. No active expectation changed.

## Ownership

- `leaf_cdf.c`: `Av2hLeafCdfs` defaults and initialization.
- `filter_entropy.c`: GDF, CDEF, and CCSO entropy-state initialization,
  context derivation, and control reads.
- `palette.c`: block admission, color-cache syntax, packed palette maps,
  color contexts, and map reads.
- `intrabc.c`: candidate construction, displacement syntax, precision/shell
  decoding, morph context, and IntraBC diagnostics.
- `leaf_context.c`: luma neighbor derivation and context-state lifecycle.
- `leaf.c`: luma/chroma mode syntax, joint-mode mapping, FSC/MRL signaling,
  filter/IntraBC/palette ordering, diagnostics, and compatibility wrappers.

`leaf.h`, `Av2hLeafCdfs`, and `Av2hLeafMode` remain stable. All new source
files are listed explicitly in `AV2HEIF_CORE_SOURCES`.

## Internal boundary

`internal/leaf_modules.h` declares only helpers called by mode orchestration:
filter-control readers, the IntraBC reader, and palette admission/color reads.
Public state initializers and packed-map APIs remain declared in `leaf.h`.

The ordered body of `luma_mode_read_internal()` remains in `leaf.c`. It must
consume GDF, CDEF, and CCSO syntax before the possible IntraBC early return,
then preserve mode, FSC/MRL, and palette failure precedence. Splitting that
orchestration would be algorithm cleanup rather than move-only extraction.

## Validation cadence

The stale fixture was removed first, then CDF, context, palette, filter entropy,
and IntraBC owners were extracted separately. After each step, `leaf-unit`,
`partition-unit`, `tile-unit`, and `layout-unit` passed without expectation
changes.

## Completion validation

- Focused leaf, partition, tile, and layout tests: 4/4 passed after every
  extraction.
- Hosted CTest: 16/16 passed.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 full-I420 hashes exact.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.
- Schwaebisch Q208 output: 173,611 bytes with SHA-256
  `8906752a123863bdb65d1c2dad8edd9ae5ce0c8c0dfd21fce48f14a9d09faaa1`.
- Pinned AVM checkout: clean.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.
