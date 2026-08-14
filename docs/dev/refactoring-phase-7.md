# Refactoring Phase 7: partition and tile orchestration

**Status:** Complete as of 2026-08-04.

Phase 7 separates partition context derivation, luma/chroma tile stepping, and
filter-control handoff without changing public decoder structures, entropy
ordering, state transitions, failure precedence, output mutation, or decoded
bytes.

## Ownership

- `partition_context.c`: immutable block geometry, partition context
  derivation, and partition context-state initialization and updates.
- `partition.c`: forced paths, decision reading, child construction, and the
  explicit partition traversal stack.
- `tile_luma.c`: luma partition stepping, mode and coefficient syntax,
  all-zero/IntraBC initialization, luma reconstruction wrappers, and luma
  leaf diagnostics.
- `tile_chroma.c`: chroma partition stepping, colocated-mode/CfL context,
  ordered U/V coefficient syntax, cross-component transform, chroma
  reconstruction wrappers, and chroma leaf diagnostics.
- `tile_filter_handoff.c`: final GDF/CCSO completeness checks and a private
  zero-copy CDEF/GDF/CCSO control-map view for frame filtering.
- `tile.c`: tile-group parsing, decoder initialization, root reset and
  luma/chroma handoff transitions, final DONE transition, and leaf commit.

`tile.c` is reduced from 1,041 to 373 lines. `partition.c` is reduced from
1,226 to 1,036 lines. All new sources are listed explicitly in
`AV2HEIF_CORE_SOURCES`.

## Internal boundaries

`internal/partition_modules.h` shares the immutable block-geometry table
between context and traversal owners. `internal/tile_modules.h` contains only
sibling state-machine services and the private filter-control handoff record.
`Av2hTileDecoder` groups its unchanged storage into entropy, traversal,
superblock, root, luma, chroma, shared-chroma, and control ownership records.
The aggregate remains 69,792 bytes, preserving the validated workspace
contract. `partition.h` and `Av2hPartitionTraversal` remain unchanged.

Frame output now receives completed filter controls through
`av2h_tile_filter_controls_handoff()` instead of rebinding them directly from
tile workspace storage. Filter workspace still owns the copied CCSO control
buffers.

## Validation cadence

Partition context, luma stepping, chroma stepping, and filter handoff were
extracted separately. Focused partition, tile, frame, filter, layout, and
artifact-profile tests passed after their owning slices.

The first Windows gate caught inactive CCSO maps reporting zero geometry. The
handoff now derives CCSO columns, rows, and strides from the same coded-frame
geometry used by filter validation. The focused Emerald filter-path test and
the complete Windows suite then passed.

## Completion validation

- Hosted CTest: 16/16 passed.
- Windows release CTest: 51/51 passed.
- Complete artifact parity: 31/31 full-I420 hashes exact.
- Partition/tile transition, truncation, and incomplete-filter-state checks:
  passed in `partition-unit`, `tile-unit`, and `frame-unit`.
- Freestanding object closure: no undefined core symbols.
- Decoder ABI layout: unchanged from the Phase 0 baseline.
- Pinned AVM checkout: clean.

The follow-up tile-state decomposition and prediction-owner split passed the
36-test hosted suite, the 30-test ASan/UBSan suite, diagnostics-enabled parity,
freestanding closure, and exact native and WebAssembly parity for all 102 local
reference records.

LLVM-MinGW on Windows still lacks a usable AddressSanitizer runtime. The
`host-sanitized` preset remains the Linux/macOS sanitizer gate.