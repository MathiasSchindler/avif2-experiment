# Refactoring Phase 8: frame output and workspace ownership

**Status:** Complete as of 2026-08-04.

Phase 8 gives frame geometry, decode/filter storage, output validation, and
visible-frame copying one owner without changing public decoder layouts,
entropy state, failure precedence, diagnostics, or decoded bytes.

## Ownership

- `frame_workspace.c`: filter workspace query/init, full frame decode arena
  query/init, coded and reference plane geometry, tile-context capacities,
  deblock and filter metadata storage, pre-decode validation and clearing,
  output overlap rejection, completion checks, and final visible I420 copy.
- `frame_output.c`: decode argument and restoration checks, tile traversal,
  reconstruction commits, diagnostics, filter-control handoff, filtering, and
  final output-call ordering.
- `platform/windows/decoder.c`: allocation of the byte count returned by the
  private frame workspace query and invocation of its initializer.

The former `filter_workspace.c` implementation moved into
`frame_workspace.c`. All production sources remain explicit entries in
`AV2HEIF_CORE_SOURCES`; no implementation file includes another `.c` file.

## Internal boundary

`internal/frame_workspace.h` exposes only private frame decode workspace
layout, initialization, preflight, and completion services. The layout record
contains total size and alignment; coded geometry, filter layout, tile
capacities, and arena binding remain private to the owner. No entropy-module
structure is exposed through a public API.

The query and initializer use the same arena allocation sequence. Initialization
rechecks the layout against the current sequence and frame, rejects null,
misaligned, stale, and undersized storage before publishing caller-visible
workspace records, and binds filter storage only after all arena allocations
succeed.

## Failure coverage

Focused tests pin exact-size initialization, one-byte-short rejection,
misalignment, stale frame geometry, and caller-record nonmutation. Frame
preflight tests cover output overlap with coded planes, filter planes,
tile-context storage, reconstruction scratch, and the result record, with the
entire arena unchanged on failure. Existing visible-I420 tests retain
undersized-output rejection, source overlap rejection, cropped odd-dimension
copying, and output nonmutation.

## Completion validation

- Focused filter, frame, tile, layout, and artifact-profile tests: 5/5 passed.
- Hosted CTest: 16/16 passed.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 full-I420 hashes exact.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.
- Schwaebisch output: 173,611 bytes, SHA-256
  `8906752a123863bdb65d1c2dad8edd9ae5ce0c8c0dfd21fce48f14a9d09faaa1`.
- Pinned AVM checkout: clean.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.