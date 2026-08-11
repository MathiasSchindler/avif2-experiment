# 10-bit preparation WP4 explicit stride units

WP4 was implemented and validated on 2026-08-11. It makes private plane,
reference, map, allocation, and range units explicit without changing decoded
arithmetic, storage layout, public ABI, or existing 8-bit output.

## Unit-bearing contracts

Every production `stride` field and parameter was classified by the element
used for pointer arithmetic:

| Storage | Unit-bearing name | Meaning |
| --- | --- | --- |
| Reconstruction and filter sample planes | `stride_samples` | Typed sample elements per row |
| Predictor buffers | `predictor_stride_samples` | Typed predictor samples per row |
| Pre-CDEF luma | `pre_cdef_luma_stride_samples` | Typed luma samples per row |
| Reconstructed-availability maps | `reconstructed_stride_cells` | One readiness cell per four-sample block |
| Deblock maps | `stride_cells` | `Av2hDeblockCell` elements per row |
| CCSO control maps | `stride_units` | CCSO control units per row |
| GDF control maps | `stride_blocks` | GDF blocks per row |
| Leaf spatial context | `spatial_stride_bytes` | Byte offset between spatial-map rows |

The source-tree census contains no remaining bare identifier named `stride`.
Public `Av2hPlaneLayout.row_bytes` was already byte-explicit after WP2 and did
not change.

Private allocation, capacity, overlap, and output-range names now carry a
`_bytes` suffix, including frame/filter workspace `size_bytes`, coded plane
capacities, source plane sizes, output sizes, and memory sizes. The
`sized_u8_plane_bytes` helper names its current one-byte typed-plane boundary
directly; future typed 16-bit kernels must use a corresponding typed boundary
rather than reinterpret this helper's arithmetic.

## Compatibility and layout review

WP4 is a mechanical naming refactor. It changes no expression, enum, field
order, field type, function linkage, or public declaration. Existing one-byte
sample arithmetic remains identical.

`layout-unit` adds explicit LP64 size and alignment locks for the renamed
private contracts:

| Type | Size | Alignment |
| --- | ---: | ---: |
| `Av2hReconstructionPlane` | 48 | 8 |
| `Av2hFilterPlane` | 24 | 8 |
| `Av2hCcsoControlMap` | 64 | 8 |
| `Av2hGdfControlMap` | 40 | 8 |
| `Av2hDeblockMap` | 96 | 8 |
| `Av2hFilterWorkspace` | 368 | 8 |
| `Av2hFilterWorkspaceLayout` | 152 | 8 |
| `Av2hFrameDecodeWorkspaceLayout` | 16 | 8 |

The existing locks for public aggregates, frame state, CDF state, tile state,
and reconstruction scratch remain unchanged.

## Build-tree cleanup

An attempted language-server rename of the generic member `stride` reached
ignored fetched dependency sources under three `build/reference` trees. No
such edits were retained as implementation work. The pinned `vendor/avm`
checkout and the regression source copy were restored and verified clean; the
three tainted ignored reference build directories were removed. A subsequent
`make clean && make` regenerated the native build successfully with no
undefined core symbols.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 20/20 passed;
- Python development suite: 21/21 passed;
- exact source census: no bare `stride` remains under `src/`;
- clean native decoder and strict freestanding core: built with no undefined
  core symbols;
- production I420 parity: 31/31 exact;
- production I400 parity: 12/12 exact;
- production I444 native parity: 12/12 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- Linux static no-libc suite: 2/2 passed;
- WebAssembly module: built and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime; and
- Windows x86-64: decoder and smoke PE32+ binaries built with no diagnostics;
  the decoder retains its established ten `KERNEL32.dll` imports and the smoke
  binary imports only `ExitProcess`.

Windows runtime parity could not execute on this Linux validation host because
no Wine, Wibo, or PE binfmt handler is installed. The native Windows CTest gate
remains required on Windows or a host with a configured PE emulator.