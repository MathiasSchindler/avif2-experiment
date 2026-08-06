# Refactoring Phase 9: diagnostics cleanup

**Status:** Complete as of 2026-08-05.

Phase 9 removes artifact-specific probes and release side effects while
retaining optional owner-scoped failure diagnostics. Decode decisions,
statuses, public layouts, workspaces, and output bytes remain unchanged.

## Release behavior

`AV2HEIF_ENABLE_DIAGNOSTICS` is a default-off CMake option. Release builds do
not allocate diagnostic records or update diagnostic fields. Successful
Windows decodes are silent apart from the requested output file and no longer
write Chimp filter-stage snapshots.

Normal usage, file, allocation, decode, and output errors remain visible. The
detailed private failure formatter is compiled only when diagnostics are
enabled and reads the structured `Av2hFrameDecodeResult` rather than decoder
globals.

## Private ownership

`internal/decode_diagnostics.h` groups retained observations by their codec
owner:

- tile traversal stages and range checkpoints;
- leaf mode stages;
- coefficient-prefix stages and range state;
- transform parsing stage;
- luma/chroma reconstruction and inverse-transform stages;
- MHCCP failure reason.

Each owner stores one private record only in diagnostics-enabled builds and
exposes private record accessors plus no-op setters. `frame_output.c` samples
those records only while constructing a failure result. Filter validation uses
the separate owner-scoped record in `internal/filter_diagnostics.h`.

No public header exposes diagnostic records, entropy state, or diagnostic
globals. Workspace and result layouts remain unchanged.

## Removed diagnostics

The cleanup deletes coefficient/CDF snapshots, partition and leaf circular
traces, predictor edge and hash captures, IntraBC geometry and candidate
arrays, Chimp-specific reconstruction and superblock captures, filter-stage
hashes and plane aliases, and CCSO entropy snapshots. Tests now validate
behavioral outputs and hashes instead of reading `av2h_debug_*` globals.

The Windows decoder no longer prints successful-decode trace dumps or creates
`chimp-local-*.i420` stage files. There are no remaining `av2h_debug_*`
symbols or references in production or tests.

## Completion validation

- Focused frame, tile, filter, and artifact-profile tests: passed.
- Hosted diagnostics-off CTest: 16/16 passed.
- Hosted diagnostics-on CTest: 16/16 passed.
- Windows diagnostics-off CTest: 51/51 passed.
- Windows diagnostics-on CTest: 51/51 passed.
- Complete artifact parity in both modes: 31/31 full-I420 hashes exact.
- Successful Chimp decode in both modes: empty stderr and no stage-file
  changes.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.