# 10-bit preparation WP9 dependency-free PNG preview

WP9 was implemented and validated on 2026-08-11. It adds deterministic PNG
preview output to the hosted POSIX, Linux no-libc, and freestanding Windows
CLIs while preserving raw planar output as the exact decoder result.

## Shared writer contract

`src/shared/png_writer.h` exposes a private callback-based RGB writer. The
caller supplies image dimensions, a bit depth of 8 or 16, one reusable row
buffer, a top-down row producer, and an output callback. The writer performs no
allocation and has no libc, file, or platform dependency.

The emitted file has exactly four parts:

1. the standard PNG signature;
2. a 13-byte truecolor IHDR with no interlace;
3. one IDAT containing a zlib stream of filter-0 scanlines; and
4. an empty IEND.

The IDAT length is computed with checked arithmetic before output begins. Its
payload is streamed directly to the output callback. Stored DEFLATE blocks are
limited to 65,535 bytes and may cross row boundaries, so memory use remains one
RGB row plus small fixed headers even for a large image. CRC-32 is updated over
each chunk type and payload as bytes are written. Adler-32 is updated over the
uncompressed filter bytes and RGB rows.

No timestamps or ancillary chunks are emitted. Raw OBU input does not establish
enough color metadata to label the output as sRGB, so the current writer emits
no `sRGB`, `gAMA`, `cHRM`, or ICC chunk. The CLI's documented preview policy
continues to use limited-range matrix coefficients value 6.

## Sample depths

Current CLIs request truecolor RGB8. Their row callbacks use WP8's shared
planar conversion and therefore do not duplicate YUV plane geometry or color
arithmetic. BMP requests BGR order and PNG requests RGB order from one common
conversion kernel.

The writer also accepts truecolor depth 16. A 16-bit row callback supplies each
RGB sample in PNG wire order: unsigned big-endian bytes. This boundary is
covered with synthetic values including `0x0001`, `0x1234`, `0xabcd`, and
`0xffff`. WP9 does not add a 10-bit YUV-to-RGB conversion; that future caller
will choose whether to preserve precision in RGB16 or explicitly request an
8-bit compatibility preview.

## Platform ownership

Each CLI initializes and validates `Av2hPlanarPreview` before opening the output
file, allocates only `width * 3` bytes for the current RGB8 row, and adapts its
existing reliable write loop:

- POSIX uses `malloc`, `fopen`, and `fwrite`;
- Linux no-libc uses anonymous `mmap`, `openat`, and `write`; and
- Windows uses `VirtualAlloc`, `CreateFileW`, and `WriteFile`.

The PNG writer itself is shared core code. `.png` is a preview extension and is
accepted independently of I400/I420/I444 raw suffix matching. Generic `.yuv`
remains rejected before input is read.

## CC0 audit

The sibling `newos` checkout was clean at the planned revision
`f4391afd003cd629166d3626a9261360b85115fb`. Its CRC-32, zlib, and PNG probe
sources were audited before implementation. Only the nibble-table CRC update,
Adler arithmetic, and stored-DEFLATE framing were selectively adapted. The
inflate, Huffman, LZ77, runtime, and PNG parsing code was not imported. Exact
paths and adaptations are recorded in `provenance/cc0-sources.md`; the sibling
checkout is not a build dependency.

## Focused evidence

`png-writer-unit` independently parses generated files and verifies signature,
chunk order and lengths, bitwise CRC-32, zlib header, stored-block headers and
complements, independently computed Adler-32, filter bytes, dimensions,
truecolor type, depth, and exact reconstructed scanlines. It covers:

- deterministic odd-sized RGB8 output;
- synthetic RGB16 big-endian rows;
- invalid depth, insufficient row storage, and output callback failure; and
- a 65,539-byte scanline split into stored blocks of 65,535 and 4 bytes.

`decoder-output-policy` pins complete deterministic PNG files for all current
layouts:

| Layout | Dimensions | SHA-256 |
| --- | --- | --- |
| I400 | 17 x 15 | `2faa3ad4be68c069360745d3b402c359d0224e8c6ccd20cf15341099ddd92f7b` |
| I420 | 160 x 120 | `53350b01df7bd559645fb3edd0d36727b47bbc3f55095108591478343b94bacd` |
| I444 | 17 x 15 | `2064d1563ff1e65ea31ee631a3608a9e72606573e6ca4cca2cf42e1c3c32276b` |

Second hosted runs and Linux no-libc outputs matched those files byte for byte.
ImageMagick 7.1.2-18 and FFmpeg 8.0.1 independently decoded all three files to
identical RGB bytes. Those bytes also matched the shared conversion output as
independently reconstructed from the unchanged bottom-up BGR BMP files.

## Validation gates

The completion run produced these results:

- hosted ASan/UBSan CTest suite: 24/24 passed;
- Python development suite: 21/21 passed;
- production raw parity: 34/34 I420, 12/12 I400, and 12/12 native I444 exact;
- canonical HEIF public I444 parity: 12/12 exact;
- existing complete I400/I420/I444 BMP hashes remained unchanged;
- Linux static no-libc suite: 2/2 passed, with all three PNG files matching
  hosted output byte for byte and no undefined core symbols;
- WebAssembly module rebuilt and produced 12/12 exact I444 outputs through
  Node's WebAssembly runtime; and
- Windows x86-64 decoder and smoke PE32+ executables built without diagnostics;
  the decoder retains its ten `KERNEL32.dll` imports and the smoke executable
  imports only `ExitProcess`.

Windows runtime PNG parity could not execute on this Linux host because no
Wine, Wibo, or PE binfmt handler is installed. The shared writer and Windows
wrapper compile are covered here; byte parity remains required on a native
Windows host or a host with a configured PE emulator.