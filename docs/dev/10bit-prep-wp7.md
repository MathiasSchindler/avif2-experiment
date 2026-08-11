# 10-bit preparation WP7 typed sample operations

WP7 was implemented and validated on 2026-08-11. It establishes explicit
8-bit operation boundaries around shared sample-bearing work without enabling
16-bit storage, changing public APIs, or duplicating prediction and filter
kernels.

## Typed operation boundary

`sample_ops.h` owns the private 8-bit entry points:

| Operation | Entry point | Production owner |
| --- | --- | --- |
| Plane fill | `av2h_plane_fill_u8` | Frame workspace coded-plane initialization |
| Plane copy | `av2h_plane_copy_u8` | Filter stage copies and GDF input staging |
| Visible crop | `av2h_visible_crop_u8` | Coded-to-packed planar output copy |
| Reference load | `av2h_reference_load_u8` | Clamped chroma coded-plane references |
| Predictor commit | `av2h_predictor_commit_u8` | Luma, chroma, and predictor-observation plane commits |
| Reconstructed-sample load | `av2h_reconstructed_sample_load_u8` | Readiness-approved luma and chroma neighbor gathering |
| Sample clip/store | `av2h_sample_clip_store_u8` | CCSO, CDEF, and GDF output stores |

The boundary also provides direct typed `av2h_sample_load_u8` and
`av2h_sample_store_u8` primitives. The existing `av2h_sample_clip` remains the
8-bit value-only wrapper for algorithm loops and pointer-relative stores where
a plane coordinate API would be artificial.

The existing private `filter_copy_plane` entry point remains as a forwarding
wrapper because it owns filter-plane structure validation and call-site
readability. It now delegates its sample loop to `av2h_plane_copy_u8`.

## Contracts

All dimensions and strides are measured in typed samples. For `_u8` operations,
one sample is one `uint8_t`; calls to byte-oriented `av2h_memory_fill` and
`av2h_memory_copy` therefore receive the same numeric row extent. Visible crop
is explicitly tightly packed, while plane copy and predictor commit preserve
independent source and destination strides.

These are inner operations, not validation APIs. Their callers establish
non-null storage, nonzero reference dimensions, valid strides and coordinates,
and checked allocation arithmetic before entry. Bulk fill/copy operations are
no-ops for zero width or height. Reconstruction readiness remains a separate
policy check: code first verifies the readiness map and then uses the typed
reconstructed-sample load.

No sample operation accepts `void *`, dispatches through a callback, or uses a
per-sample function pointer.

## Depth specialization decision

Future 16-bit storage will use parallel, type-safe `_u16` entry points beside
the `_u8` functions. Separate functions were selected over translation-unit
templates or preprocessor-generated kernels because there are only two planned
storage widths, these boundary operations are small, and explicit types make
stride and storage review straightforward in freestanding C11.

WP7 does not add `_u16` implementations. They should be introduced with the
first 10-bit sample vectors so their byte order, clipping, and storage behavior
are test-driven. Prediction, transform, and filter algorithms remain direct
8-bit kernels for now; broad duplication before normative 10-bit evidence is
intentionally deferred.

## Focused evidence

`sample-ops-unit` exercises every entry point with padded source and destination
strides. Its vectors verify:

- plane fill changes only visible samples and preserves row padding;
- plane copy preserves independent source and destination padding;
- visible crop emits tightly packed rows and preserves surrounding guards;
- reference loads clamp right and bottom coordinates;
- reconstructed and direct loads use sample strides rather than byte casts;
- predictor commit copies only its requested rectangle;
- direct and clipped stores preserve row padding and saturate to `[0, 255]`;
  and
- zero-width and zero-height bulk operations are no-ops.

Existing reconstruction, tile, workspace, and filter suites were run after
each owning module migrated. The filter suite caught an initially block-relative
GDF store coordinate; the boundary call was corrected to use absolute plane
coordinates before full validation.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 21/21 passed;
- Python development suite: 21/21 passed;
- focused sample-operation, reconstruction, tile, workspace, and filter vectors
  passed after each owner migration;
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
