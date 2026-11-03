# 10-bit preparation WP3 workspace accounting

WP3 was implemented and validated on 2026-08-11. It makes workspace size
queries sample-width-aware without enabling 10-bit decode or widening syntax,
context, coefficient, readiness, or filter-control storage.

## Region classification

The frame decode arena contains these categories:

| Category | Regions | WP3 accounting |
| --- | --- | --- |
| Sample-backed | coded Y/U/V planes; deblocked Y/U/V stages; CDEF Y/U/V stages | Multiply stored sample count by bytes per sample |
| Sample-backed scratch | reconstruction predictor plus primary and secondary above/left references | Multiply 4,608 samples by bytes per sample |
| Coefficient/intermediate | three 1,024-entry `int32_t` reconstruction scratch arrays | Keep fixed at 12,288 bytes |
| Syntax/context | partition neighbors, luma modes, coefficient contexts, colocated luma mode state | Keep existing element types and counts |
| Reconstruction metadata | per-4x4 reconstructed-availability maps | Keep one byte per cell |
| Filter metadata | deblock cells, GDF flags, CDEF strengths, and CCSO controls | Keep existing element types and counts |
| Other fixed state | `Av2hReconstructionScratch` alignment and arena bookkeeping | Keep existing alignment contract |

There is no workspace-owned output staging plane. Decoded output is written to
the caller-owned output buffer, whose byte accounting was completed in WP2.
The optional diagnostic chroma predictor plane is not allocated by the normal
decode workspace and remains absent in the production path.

## Query contract

`av2h_filter_workspace_query` and
`av2h_frame_decode_workspace_query` accept only one- or two-byte sample
storage. C4 consolidated the byte-aware queries under these canonical names;
callers now supply bytes per sample explicitly instead of using one-byte
compatibility wrappers.
Decode preparation passes the WP1 descriptor's `bytes_per_sample` into the
byte-aware frame workspace query.

Sample plane reservations use the storage width as their alignment. The
aggregate frame workspace alignment remains four bytes because fixed `int32_t`
scratch is already the strongest requirement. Existing public 8-bit workspace
sizes and alignments therefore remain unchanged.

WP3 intentionally adds accounting, not a two-byte binder. The current
workspace structs and kernels contain typed `uint8_t` sample pointers, so
`av2h_frame_decode_workspace_init` continues to bind only one-byte layouts. It
rejects a two-byte accounting layout without modifying destination structs.
Typed two-byte binding belongs with the sample-operation boundary work rather
than being represented through misleading byte pointers.

## Focused evidence

`workspace-accounting-unit` covers I400, I420, and I444 at odd 17 x 15
dimensions for both storage widths. For each format it proves:

- both filter stage plane sets scale with sample width;
- the coded plane set and 4,608 predictor/reference scratch samples scale;
- deblock-cell and CCSO-control counts remain identical;
- total frame-workspace growth equals exactly the three sample plane sets plus
  scratch sample growth and conservative per-plane two-byte alignment padding;
- aggregate alignment remains unchanged;
- one-byte layouts initialize successfully;
- padded readiness maps cover odd dimensions; and
- two-byte layouts fail closed at the one-byte binding boundary without
  modifying output structs.

The unit also queries one- and two-byte workspaces for every chroma format at
the configured 16384 x 16384 maximum, confirms the two-byte requirement
exceeds the configured workspace limit, rejects zero- and three-byte storage,
and exercises overflow for both supported widths. Existing tests continue to
cover undersized and misaligned allocations, overlap rejection, and decode
failure atomicity.

No public struct or private decoder-state type changed size in WP3.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 20/20 passed;
- Python development suite: 21/21 passed;
- native decoder and strict freestanding core: built with no undefined core
  symbols;
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