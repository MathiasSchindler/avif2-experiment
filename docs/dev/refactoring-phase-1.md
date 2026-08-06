# Refactoring Phase 1: private module conventions

**Status:** Complete as of 2026-08-04.

Phase 1 establishes the first interface used only between decoder
implementation files. It does not move algorithms, change public declarations,
or alter decoder state.

## Internal header convention

Decoder-only sibling interfaces live under `src/decoder/internal/`.

- Headers are named for the lowest coherent owner, not a generic utility
  bucket.
- Declarations and definitions use the `av2h_` prefix.
- Every header is self-contained and has its own include guard.
- Internal headers may include standard freestanding headers and narrower
  internal owners, but must not be included by public headers.
- Stateless helpers may be `static inline` when that keeps ownership local and
  introduces no link-time dependency.
- Publicly included decoder headers retain their existing declarations until
  the phase that owns their API migration.

`sample_math.h` is the first internal owner. It contains only sample-domain
clamping and 8-bit clipping. `filter.c` uses it directly. `reconstruct.c` keeps
its established local call surface as a forwarding function so this phase does
not combine interface setup with a large mechanical rewrite.

The filter and reconstruction signed-rounding helpers remain local. Their
signatures and negative-value semantics differ, so combining them would be an
algorithm change rather than deduplication.

## Focused coverage

`sample-math-unit` includes only `decoder/internal/sample_math.h`, which proves
header self-containment while pinning lower bound, interior, upper bound, and
8-bit clipping behavior. Existing `filter-unit` and `reconstruct-unit` cover
both production consumers.

## Completion validation

- Hosted CTest: 16/16 passed, including `layout-unit` and
  `sample-math-unit`.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 I420 hashes exact.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.
- Schwaebisch Q208 output: 173,611 bytes with SHA-256
  `8906752a123863bdb65d1c2dad8edd9ae5ce0c8c0dfd21fce48f14a9d09faaa1`.
- Pinned AVM checkout: clean.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.
