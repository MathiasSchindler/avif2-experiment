# 10-bit preparation WP2 byte-correct output layouts

WP2 was implemented and validated on 2026-08-11. It makes public plane layout
arithmetic sample-width-aware without admitting a 10-bit stream or changing
any existing 8-bit decoded bytes.

## Layout contract

`src/output_layout.c` is the authoritative owner of tightly packed decoded
plane geometry. Its inputs are luma width and height, chroma format, and bytes
per sample. It computes:

- `row_bytes` as checked `width * bytes_per_sample`;
- plane `size` as checked `row_bytes * height`;
- U and V offsets with checked byte additions; and
- `Av2hInfo.output_size` as the total byte count of all present planes.

Width and height remain sample counts. The public header and README now state
explicitly that `Av2hPlaneLayout.row_bytes`, `offset`, and `size`, and
`Av2hInfo.output_size` are byte counts.

The calculator derives 4:2:0 chroma dimensions with ceil division, preserves
full dimensions for 4:4:4, and zeroes absent U/V layouts for monochrome. It
rejects zero dimensions, zero bytes per sample, unsupported chroma, and null
outputs. Calculations use local layouts and publish them only on success, so an
invalid input or overflow does not partially modify caller results.

Public query passes the WP1 descriptor's `bytes_per_sample` to this calculator.
Decode preparation uses a byte-width-aware planar-size entry point, while the
existing `av2h_planar_output_size` remains a one-byte compatibility wrapper for
current frame decode and platform callers.

WP2 does not construct or accept a 10-bit descriptor, widen decoded sample
storage, or change workspace accounting. Those remain separate later work
packages.

## Test matrix

`output-layout-unit` covers one- and two-byte storage for odd 17 x 15 I400,
I420, and I444 layouts. The two-byte expected totals are:

| Format | Y bytes | U bytes | V bytes | Total bytes |
| --- | ---: | ---: | ---: | ---: |
| I400 | 510 | 0 | 0 | 510 |
| I420 | 510 | 144 | 144 | 798 |
| I444 | 510 | 510 | 510 | 1,530 |

The unit also covers all three formats at the configured maximum 16384 x 16384
dimensions with two-byte samples, every invalid argument category, row-byte
and plane-size multiplication overflow, first and second plane-total addition
overflow, and failure atomicity. Existing container tests retain output-limit
ownership and precedence through `AV2H_CONTEXT_LIMITS`.

No public struct field or function signature changed in WP2, so LP64 and
wasm32 public aggregate layouts remain at their WP1 values.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan C suite: 19/19 passed;
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