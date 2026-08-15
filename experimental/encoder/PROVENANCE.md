# Encoder-local provenance

All code in this experiment is distributed under the repository's CC0-1.0
license. The sibling `newos` project was reviewed as a CC0 source of
freestanding implementation concepts; it is not a source, build, link, or
runtime dependency.

## Checked substrate

- **Revision:** `57667e4e01f62bc03593e2f84129eca428afaa7b`
- **Reviewed paths:** `src/base.c`, `src/base.h`, `src/shared/limits.h`,
  `src/shared/stddef.h`, and `src/shared/stdint.h`
- **Concepts adapted:** fixed-width freestanding types, checked size
  arithmetic, small memory primitives, bounded caller-owned buffers, and
  sticky error propagation.
- **Local boundary:** `include/encoder/base.h`, `src/memory.c`, and
  `src/writer.c` use encoder-local names and interfaces. No newos file is
  compiled or linked.

The planar input implementation reuses only those already recorded
encoder-local checked-addition, checked-multiplication, bounded-span, and
explicit-ownership patterns. No additional newos source or interface was
copied for the I400/I420/I444 loader.

## Linux x86-64 process substrate

- **Revision:** `972ca695b3f4b401a00f596ed78172dd1ceb03cf`
- **Reviewed paths:** `src/platform/linux/common.h` and
  `src/arch/x86_64/linux/syscall_stubs.S`
- **Concepts adapted:** reading the kernel-provided initial process stack,
  entering without a C runtime, issuing raw Linux syscalls, and terminating
  through the kernel.
- **Local boundary:** `platform/x86_64/start.S` and `platform/linux.c` are
  experiment-specific implementations with only the syscalls required by the
  encoder scaffold.

The reviewed newos revisions declare CC0-1.0. This record covers copied or
adapted concepts and their local ownership boundary; no decoder source,
header, object, library, generated table, or build target is part of the
encoder implementation.

## BMP, PNG, CRC, and zlib/DEFLATE input research

- **Revision:** `972ca695b3f4b401a00f596ed78172dd1ceb03cf`
- **Reviewed paths:** `src/shared/image/bmp.c`,
  `src/shared/image/png.c`, `src/shared/compression/crc32.c`,
  `src/shared/compression/zlib.c`, and their local headers.
- **License checked:** `/home/mathias/newos/LICENSE`, CC0 1.0 Universal.
- **Concepts adapted:** bounded endian reads, BMP signed-height and
  compression classification, PNG chunk walking, nibble-table CRC-32,
  Adler-32, LSB-first DEFLATE reads, canonical Huffman construction,
  stored/fixed/dynamic blocks, and overlapping LZ77 copies.
- **Local boundary:** `include/encoder/image_input.h` and
  `src/image_input.c` use encoder-local interfaces, caller-owned storage,
  stricter subset validation, local unfiltering, and local integer YUV
  conversion. No newos file is compiled, linked, or required at runtime.
  The CLI now maps files through the existing Linux syscall substrate and
  passes typed planes to encoder-local workspace APIs; no external image or
  allocation library was introduced.

## AV2 v1.0.0 normative CDF attachments

`tools/generate_cdf_subset.py` reads only official local specification
attachments corresponding to
`https://av2.aomedia.org/v1.0.0/attachments/`. It verifies each SHA-256,
expected row/value count, ascending Q15 thresholds, parameter index, and
adaptation count before emitting tracked `src/normative_cdfs.inc`. The
generated file records every input hash next to its table.

The 2,256-row subset contains do-split, delta-Q, luma mode set/index, FSC,
transform partition, palette presence, luma txb-skip/all-zero, 8x8 EOB point,
normal and low-frequency base-at-EOB, low-frequency non-EOB base,
low-frequency BR, DC sign, and the three intra transform-selection tables.
It also hash-checks all 125 normative
parameter-adjustment rows.
`make check-generated` rejects changed attachments or stale generated data.

This is deliberately not an AVM table extraction: no AVM source is read, and
no implementation table is copied. EOB tables for transform sizes other than
8x8, high-frequency coefficient contexts, chroma, and transforms other than
the bounded 8x8 DCT_DCT path remain outside the subset.

The M4 scalar inverse uses `dct_kernel8.h`, SHA-256
`e89f5aac7056f3a8e55ee93a855520e3efb6392666a611563c17be3d9b86d560`,
from the same normative attachment set.

CDF inverse-threshold conversion, parameter-index expansion, adaptive update,
probability increments, and interval termination are implemented locally from
the normative decoding process. No AVM implementation source or table is
copied.
