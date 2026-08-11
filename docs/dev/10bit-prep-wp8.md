# 10-bit preparation WP8 raw output and preview separation

WP8 was implemented and validated on 2026-08-11. It centralizes raw-output
naming and the existing planar-to-BGR8 conversion without changing decoded raw
bytes, BMP bytes, public APIs, or platform ownership of allocation and file
I/O.

## Raw-output naming policy

`preview.h` provides private helpers that derive names from the complete
`Av2hPixelFormat` descriptor rather than from chroma alone:

| Pixel format | Format name | Accepted raw suffix |
| --- | --- | --- |
| 8-bit monochrome U8 | `gray8` | `.i400` |
| 8-bit 4:2:0 planar U8 | `yuv420p` | `.i420` |
| 8-bit 4:4:4 planar U8 | `yuv444p` | `.i444` |
| 10-bit 4:2:0 planar U16 little-endian | `yuv420p10le` | None yet |
| 10-bit 4:4:4 planar U16 little-endian | `yuv444p10le` | None yet |

The legacy suffixes are strict 8-bit names. Generic `.yuv` is not accepted:
it cannot identify chroma layout, significant depth, storage width, byte order,
or plane order. The future 10-bit names are available for metadata and policy
work, but the CLI does not claim to write those formats before 10-bit decode
and output support exists.

Each platform CLI now constructs the current `Av2hPixelFormat` and asks the
shared helper for its permitted suffix. A suffix that does not match the
decoded format exits with status 2 before an output file is created.

## Preview contract

`Av2hPlanarPreview` describes the source independently of a file writer. It
carries the complete pixel format, color range, matrix coefficients, image
dimensions, three typed plane addresses, and per-plane strides measured in
samples. `av2h_planar_preview_init_tight_8bit` adapts the decoder's current
tightly packed I400, I420, or I444 buffer to that descriptor with checked plane
geometry.

`av2h_planar_to_bgr8_row` converts one caller-selected source row into exactly
`width * 3` BGR bytes. It allocates no memory and has no file, platform, or
whole-image dependency. BMP writers retain their local header, row allocation,
padding, bottom-up row order, and write calls. WP9's PNG writer consumes the
matching shared RGB row interface in top-down order.

The currently supported conversion domain is deliberately narrow:

- unsigned one-byte planar I400, I420, or I444 samples;
- limited color range; and
- matrix coefficients value 6, preserving the historical BT.601 conversion.

Initialization rejects unsupported range or matrix combinations before a
platform writer opens its output. Row conversion independently validates the
descriptor, storage, depth, strides, row index, and destination extent so a
manually constructed unsupported descriptor also fails.

The integer coefficients, bias, truncating division, and clipping are unchanged
from the three former platform copies. Monochrome uses neutral chroma. I420
uses `row / 2` and `column / 2`, while I444 uses full-resolution coordinates.
No 10-bit conversion is implemented by WP8. When added, BMP must be an explicit
8-bit preview downconversion; exact planar 10-bit output remains authoritative.

## Focused evidence

`preview-unit` verifies all current and reserved format names, strict raw
extensions, exact I400/I420/I444 BGR rows, odd-width I420 chroma addressing,
destination guards, and rejection of unsupported range, matrix, and depth.

`decoder-output-policy` decodes one fixture per current chroma layout and pins
the complete BMP SHA-256 values:

| Layout | SHA-256 |
| --- | --- |
| I400 | `2cf48f28100237d4ac9656e17f3cdf2c38c8bb5dd2b3c8af999a159cba513a8b` |
| I420 | `66ebb65fe21ec8ec8fbfe46fc009b0cf395dba2a22c8f6f91df45eaaa867c20f` |
| I444 | `782695dcf08bce63864ea9187d3e0fd472a9681547671805c299eb474b2f6469` |

Those files were captured before platform migration and compared byte for byte
after migration. The durable test also verifies that generic `.yuv` and an
I420 decode requested as `.i444` both exit with status 2 and create no file.
The Linux no-libc writer produced the same three complete BMP hashes.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan CTest suite: 23/23 passed;
- Python development suite: 21/21 passed;
- production raw parity: 34/34 I420, 12/12 I400, and 12/12 native I444 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- native and freestanding builds completed with no undefined core symbols;
- Linux static no-libc suite: 2/2 passed, with complete I400/I420/I444 BMP
  hashes also matching the hosted baselines;
- WebAssembly module rebuilt and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime;
- Windows x86-64 decoder and smoke PE32+ executables built without diagnostics;
  the decoder retains its established ten `KERNEL32.dll` imports and the smoke
  executable imports only `ExitProcess`.

Windows runtime BMP parity could not execute on this Linux host because no
Wine, Wibo, or PE binfmt handler is installed. The shared row conversion and
Windows wrapper compile are covered here; the existing native Windows runtime
gate remains required on Windows or a host with a configured PE emulator.