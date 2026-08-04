# Refactoring Phase 3: coefficient-level ownership

**Status:** Complete as of 2026-08-04.

Phase 3 splits coefficient-level decoding by responsibility without changing
public declarations, decoder state layout, generated CDF values, entropy-read
order, diagnostics, failure precedence, or decoded output.

## Ownership

- `coeff_level_cdf.c`: coefficient/FSC CDF data, bank initialization, and
  mutable base/BR/EOB row selection.
- `coeff_dequant.c`: coefficient Q context, transform geometry, dequant steps,
  signed clamping, dequantization, and CUL finalization.
- `coeff_context.c`: scan positions and LF/base/BR/EOB/UV/FSC context
  derivation.
- `coeff_level.c`: diagnostics, luma/chroma/FSC entropy reads, level
  orchestration, and public forwarding entry points.

`coeff_level.h` remains the stable declaration and state-layout owner. All new
source files are listed explicitly in `AV2HEIF_CORE_SOURCES`.

## Internal boundaries

- `internal/coeff_common.h` owns the shared static-inline unsigned minimum
  primitive used by CDF initialization, context derivation, and entropy reads.
- `internal/coeff_modules.h` declares only sibling Q/context derivation and CDF
  row-selection interfaces.
- The duplicate coefficient Q-context helper formerly local to `coeff.c` was
  removed; coefficient prefix and level decoding now consume the single owner
  in `coeff_dequant.c`.
- Mutable CDF row selection stays with `coeff_level_cdf.c`, keeping table-layout
  details out of `coeff_level.c`.

The generated `coeff_cdf_all.inc`, `fsc_cdf_data.inc`,
`coeff_cdf_q0_txs4.inc`, `coeff_cdf_q0_txs3.inc`,
`coeff_cdf_q0_txs012.inc`, and `coeff_cdf_q0_uv.inc` files are included only by
`coeff_level_cdf.c`. Their generated values were not edited.

## Validation cadence

The dequant/geometry block was extracted first and checked with
`coeff-level-unit`, `coeff-unit`, and `layout-unit`. Context derivation and CDF
ownership were then moved separately, with the same focused gate run after each
step. No test expectations changed.

## Completion validation

- Focused coefficient and layout tests: 3/3 passed after every extraction.
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
