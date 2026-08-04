# Refactoring Phase 6: reconstruction

**Status:** Complete as of 2026-08-04.

Phase 6 splits reconstruction by codec responsibility without changing public
entry points, scratch layout, diagnostics, failure precedence, output mutation,
or decoded bytes. `reconstruct.h` and caller-owned
`Av2hReconstructionScratch` remain unchanged.

## Ownership

- `secondary_transform.c`: IST scans, generated IST data, set/type selection,
  and inverse secondary-transform kernels.
- `inverse_transform.c`: primary transform tables, geometry and kind
  selection, one-dimensional kernels, and generic row/column reconstruction.
- `intra_prediction.c`: luma DC, smooth, Paeth, directional, IDIF, MRL, IBP,
  edge preparation, and prediction diagnostics.
- `intrabc_prediction.c`: displacement validation, reconstructed-source
  checks, fractional copy prediction, and morph fitting.
- `chroma_prediction.c`: basic chroma prediction, explicit and derived CfL,
  MHCCP sampling and fitting, fixed-point division tables, and MHCCP
  diagnostics.
- `reconstruct.c`: residual composition, cross-component transform,
  reconstructed-plane readiness, public wrappers, and plane commit.

All eight generated `ist_*.inc` files have one owner in
`secondary_transform.c`. All new source files are listed explicitly in
`AV2HEIF_CORE_SOURCES`. No decoder source file includes another `.c` file.

## Internal boundary

`internal/reconstruct_modules.h` contains sibling-only transform, prediction,
readiness-query, and fixed-point helper declarations. The public/private
declarations and layouts in `reconstruct.h` remain stable.

Prediction branches were moved without deduplicating their similar DC and IBP
implementations. Chroma directional prediction retains its direct call to the
mapped directional kernel; routing it through the broader luma dispatcher was
caught by `tile-unit` as a one-level ordinary-fixture regression and reverted
before the full gates.

## Validation cadence

After each extraction, `reconstruct-unit`, `coeff-level-unit`,
`transform-unit`, and `layout-unit` passed. The complete hosted suite then
exposed and verified the chroma dispatch correction before Windows and artifact
validation.

## Completion validation

- Focused reconstruction, coefficient-level, transform, and layout tests: 4/4
  passed after each extraction.
- Hosted CTest: 16/16 passed.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 full-I420 hashes exact.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.
- `reconstruct.c`: reduced from 9,294 to 1,719 lines.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.