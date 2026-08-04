# Refactoring Phase 4: transform syntax and table ownership

**Status:** Complete as of 2026-08-04.

Phase 4 splits transform CDF and mapping ownership from entropy syntax without
changing public declarations, decoder state layout, adaptive-read order,
diagnostics, failure precedence, or decoded output. Inverse-transform kernels
remain with reconstruction as planned.

## Ownership

- `transform_cdf.c`: `Av2hFirstTransformCdfs` initialization and all transform
  CDF seed tables.
- `transform_map.c`: angle-adjusted mode mapping, luma/chroma primary transform
  maps, and ordinary/reduced secondary set maps.
- `transform.c`: transform signaling decisions, range-decoder reads, entropy
  endpoints, diagnostics, and public read entry points.

`transform.h` remains the stable declaration and state-layout owner. The two
new source files are listed explicitly in `AV2HEIF_CORE_SOURCES`.

## Internal boundary

`internal/transform_modules.h` declares only the mode/chroma and secondary-set
mapping interfaces needed by the syntax reader. The existing public
`av2h_primary_tx_type_map()` declaration remains in `transform.h`.

Small transform-type exception tables inside `read_primary()` remain beside
the adaptive reads that select them. Moving or consolidating those tables would
require restructuring a large ordered fallback chain and could change failure
precedence; that is algorithm cleanup rather than move-only extraction.

## Validation cadence

CDF initialization was extracted first, followed by the core luma/secondary
maps and then mode/chroma maps. After each extraction, `transform-unit`,
`coeff-level-unit`, `coeff-unit`, and `layout-unit` passed without expectation
changes. `transform-unit` remains enabled in the default hosted gate and pins
CDF state, transform maps, signaling, and entropy endpoints.

## Completion validation

- Focused transform, coefficient, and layout tests: 4/4 passed after every
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
