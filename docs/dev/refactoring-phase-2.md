# Refactoring Phase 2: frame-filter pilot split

**Status:** Complete as of 2026-08-04.

Phase 2 proves the move-only extraction workflow on frame filtering. Public
filter declarations, decoder state, filter order, generated values, and failure
behavior remain unchanged.

## Ownership

- `filter.c`: stage diagnostics, pipeline ordering, and `av2h_filter_frame()`.
- `filter_workspace.c`: workspace query, sizing, arena binding, and storage
  initialization.
- `deblock.c`: thresholds, edge decisions, lane kernels, map validation, and
  frame traversal.
- `cdef.c`: direction search, constrain/tap kernels, block filtering, and frame
  traversal.
- `gdf.c`: generated GDF data, stripe sampling, classification, block kernels,
  control validation, and frame traversal.
- `ccso.c`: parameter/control validation, classification, offsets, and frame
  traversal.

`filter.h` remains the stable declaration owner. The new source files are
listed explicitly in `AV2HEIF_CORE_SOURCES`.

## Internal boundaries

- `filter_geometry.h` owns coded-plane dimensions and legacy CCSO unit-size
  normalization shared by workspace, CCSO, and pipeline validation.
- `filter_common.h` owns identical static-inline fixed-point helpers, coded
  plane validation, and plane copying. Static inline ownership avoids sibling
  link cycles.
- `filter_modules.h` contains only the narrow sibling validation interfaces
  needed by the pipeline.
- `sample_math.h` remains the sample clamp/clip owner established in Phase 1.

`gdf_data.inc` is included only by `gdf.c`; its generated values were not
edited. The historical deblock side-threshold table remains in that generated
file, so `gdf.c` exposes one bounded internal lookup to `deblock.c` rather than
including the full generated bank twice.

## Validation cadence

The workspace extraction was validated first with `filter-unit` and
`layout-unit`. The remaining family extraction was then compiled and repaired
only for explicit sibling dependencies before rerunning the same focused gate.
No test expectations changed.

## Completion validation

- Focused `filter-unit` and `layout-unit`: passed.
- Hosted CTest: 16/16 passed.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 I420 hashes exact.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.
- Schwaebisch Q208 output: 173,611 bytes with SHA-256
  `8906752a123863bdb65d1c2dad8edd9ae5ce0c8c0dfd21fce48f14a9d09faaa1`.
- Pinned AVM checkout: clean.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.
