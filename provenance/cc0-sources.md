# CC0 source provenance

This project uses the sibling
[`newos`](https://github.com/MathiasSchindler/newos) project as a source
substrate where its freestanding, dependency-free code fits the decoder's
needs. Both projects are released under CC0-1.0. Selected code is copied,
adapted, and maintained in this repository; newos is not a build or runtime
dependency. Each import is recorded below with its reviewed revision, source
paths, and adaptation boundary.

The initial generic substrate was adapted from the sibling `avif-decoder`
checkout at commit `57667e4e01f62bc03593e2f84129eca428afaa7b`, which is
published under CC0-1.0.

Reviewed source paths:

- `src/base.c`
- `src/base.h`
- `src/shared/limits.h`
- `src/shared/stddef.h`
- `src/shared/stdint.h`

The adapted code provides checked size arithmetic, byte and bit readers,
caller-backed arenas, memory primitives, and freestanding standard type
headers. It was renamed to the `av2h_` namespace. AVIF defaults, capability
state, AV1 framing options, file I/O, platform code, and decoder state were not
copied.

The Linux x86-64 and macOS ARM64 no-libc builds use the linker adapted from the
sibling `newos` checkout at
commit `c9383f9ee97480c1c3158f1757709a4f30584f3d`, whose root `LICENSE`
declares CC0-1.0. The hosted ELF/Mach-O linker closure is copied without source
changes under `tools/macho_linker/newos/`. Reviewed source paths were:

- `src/tools/linker.c`;
- `src/compiler/linker.c`, `linker_util.c`, `linker_elf.c`,
  `linker_object.c`, `linker_symbols.c`, `linker_gc.c`, `linker_merge.c`,
  `linker_icf.c`, `linker_reloc.c`, `linker_layout.c`, `linker_report.c`,
  `linker_lto.c`, and `linker_macho.c`;
- `src/shared/crypto/sha256.c`;
- `src/shared/runtime/memory.c`, `string.c`, `parse.c`, and `io.c`;
- `src/shared/tool_cli.c`, `tool_io.c`, `tool_json.c`, and `tool_path.c`; and
- `src/platform/posix/fs.c`, `process.c`, and `identity.c`.

The copied header closure comprises `src/compiler/compiler.h`, `linker.h`,
`linker_internal.h`, and `source.h`; `src/platform/common/signal_util.h`;
`src/platform/posix/common.h`; `src/shared/concurrency.h`, `limits.h`,
`platform.h`, `runtime.h`, `stddef.h`, `stdint.h`, `tool_util.h`, and `xml.h`;
and `src/shared/crypto/sha256.h`.

The direct Darwin syscall wrappers in `src/platform/macos/macos_abi.h` were
reduced and renamed from `src/arch/aarch64/macos/syscall.h`. The entry code in
`src/arch/aarch64/macos/runtime.S` was reduced from
`src/platform/macos/newlinker_start.S`; environment setup was omitted because
the decoder does not consume the environment. The decoder-specific allocation,
file I/O, and output policy remain project-owned code shared with the Linux
no-libc CLI. No newos checkout, library, binary, or generated file is a build
or runtime dependency.

The byte and bit writers, AV2 LEB128 and OBU carriage parser, Sequence Header
and Content Interpretation parsers, draft-shaped `av2C`, and BMFF box framing
in this repository are project-owned CC0 code written for this experiment. The
typed AV2-HEIF parser and deterministic writer in `src/container.c` are also
project-owned CC0 code written for this experiment. The hosted artifact adapter
and Python laboratory orchestration under `tools/` are project-owned CC0 code;
they remain outside the freestanding product boundary. The bounded dialect,
validation, and query implementation in `src/query.c` is project-owned CC0
freestanding code written for this experiment.

The AV2 arithmetic-decoder ownership and checked-span structure in
`src/decoder/range.c` was adapted from the sibling CC0 implementation in
`src/codec/av1_symbol.c` at the same commit. AV2 inverse-Q15 probability
scaling, exact bypass decoding, adaptive CDF state, tell semantics, and
validation were independently implemented for this project. Deterministic
bytes and state transitions are compared with a separately built pinned AVM
oracle through `tools/entropy_oracle.c`; no AVM source or object is included
in the CC0 production core.

The raw UVLC, signed-literal, quasi-uniform, and alignment behavior and the AV2
Sequence Header field order were checked against the pinned AVM source. The
freestanding implementations are project-owned CC0 code and are validated by
independent fixture and boundary vectors in `tests/substrate_unit.c`. The
one-tile framing implementation in `src/decoder/tile.c` is likewise
project-owned CC0 code and is checked against fixture-derived AVM offsets.
The Closed Loop Key field order and directly controlling helper behavior were
checked against pinned AVM while implementing `src/decoder/frame.c`; no AVM
code is copied or linked. The project-owned parser is independently gated by
four exact header-length vectors, truncation checks, strict alignment tests,
and the freestanding undefined-symbol inspection.

The compact CDF initializer in `src/decoder/cdf.c` is written from the
normative AV2 v1.0.0 symbol-decoding equation, `Para_Adjustment_List`, and
default CDF attachments for alphabets of 2 through 16 symbols. Attachment URLs
and hashes are locked in
`spec/cdf-attachments.lock`; the separately licensed AVM copy is used only by
the hosted semantic equivalence checker.

The block catalog, MI dimensions, partition size adjustments, ordinary child
geometry, and boundary-forcing rules in `src/decoder/partition.c` are written
from the AV2 v1.0.0 conversion tables and partition parsing process. A locally
instrumented, separately licensed AVM inspection build supplies only hosted
equivalence evidence for node geometry, forced/implied provenance, contexts,
symbols, and entropy positions; no AVM partition code or table is included or
linked into production.

The first-leaf pre-mode controls, luma mode, and bounded first-chroma mode implementation in
`src/decoder/leaf.c` are project-owned CC0 code written from the AV2 v1.0.0
parsing process and hash-locked intraBC, CCSO, luma-mode, CFL, and UV-mode CDF attachments.
A separately licensed AVM inspection build supplies hosted symbol-order and
fixture correspondence evidence only. No AVM mode code, table, header, or
object is included or linked into production.

The FSC, first-transform `all_zero`/EOB, transform type, first-chroma separate
U/V skip and CCTX-none syntax, reverse luma/UV coefficient, and bounded
forward-IDTX coefficient continuation in
`src/decoder/leaf.c`, `src/decoder/coeff.c`, and
`src/decoder/transform.c` plus `src/decoder/coeff_level.c` is project-owned CC0 code
written from the AV2 v1.0.0 parsing processes and hash-locked normative CDF
attachments. Hosted checkers compare the compact rows with pinned AVM's
separately licensed defaults; no AVM coefficient code, table, or object is
included or linked into production.

The first-block prediction and inverse-transform implementation, plus the tiny
fixture's 4x4 `DCT_ADST`, `H_ADST`, `DCT_DCT`, and `IDTX` continuations,
8x8 `DCT_ADST`, generated D203, D45, MRL-1 and MRL-3 D67 prediction, live-edge Paeth, and 8x4
D45/`ADST_FLIPADST` continuation, plus AV2 4x4 smooth prediction, in
`src/decoder/reconstruct.c` is project-owned CC0 code written from the AV2
v1.0.0 integer transform matrices, rectangular scaling rule, transform-type
mapping, shift table, directional derivative attachment, interpolation phases,
and clipping process. This includes the first tiny chroma leaf's DC-128 U/V
prediction and 4x4 `DCT_DCT` reconstruction. Attachment hashes are locked in
`spec/cdf-attachments.lock`, and `tools/check_prediction.py` verifies the
selected production constants against the normative derivative and pinned AVM
tables. A separately licensed AVM
inspection build supplies only hosted reconstructed-pixel correspondence
evidence; no AVM transform source, table, header, or object is included or
linked into production.

The bounded deblock, CDEF, CCSO, and GDF implementation in
`src/decoder/filter.c` is project-owned CC0 code written from the AV2 v1.0.0
decoding processes. GDF's trained alpha, weight, bias, intra-error, and
inter-error tables and the deblock side-threshold table are generated from
hash-locked normative specification attachments. No AVM filter source, table,
header, or object is included or linked into production.

The streaming PNG writer in `src/shared/png_writer.c` was implemented after
auditing the sibling `newos` checkout at commit
`f4391afd003cd629166d3626a9261360b85115fb`, whose root `LICENSE` declares
CC0-1.0. The reviewed source paths were:

- `src/shared/compression/crc32.h` and `crc32.c`;
- `src/shared/compression/zlib.h` and `zlib.c`; and
- `src/shared/image/png.c`.

The 16-entry CRC-32 update table and nibble-at-a-time update structure were
adapted from `crc32.c`. The incremental Adler-32 arithmetic and stored-DEFLATE
header rules were adapted from the corresponding small portions of `zlib.c`.
They were renamed to the local `av2h` ownership boundary, made incremental,
and integrated into a checked callback writer that emits PNG signature, IHDR,
one streamed IDAT, and IEND without complete-image allocation. The local code
adds deterministic filter-0 scanlines, row callbacks, 8/16-bit truecolor
support, checked PNG chunk sizing, and platform-independent output callbacks.

The newos PNG parser, zlib inflate paths, Huffman encoders, LZ77 tables, runtime
layer, and public compression interfaces were not copied. The sibling checkout
is audit evidence only and is not a build input.

The reusable concurrency substrate in `src/runtime/concurrency.c`,
`src/runtime/concurrency_linux_x86_64.c`, and
`src/arch/x86_64/linux/concurrency.S` was adapted from the sibling `newos`
checkout at commit `972ca695b3f4b401a00f596ed78172dd1ceb03cf`, whose root
`LICENSE` declares CC0-1.0. The reviewed source paths were:

- `src/shared/concurrency.h`;
- `src/shared/runtime/concurrency.c`;
- `src/platform/linux/thread.c`;
- `src/platform/linux/common.h`; and
- `src/arch/x86_64/linux/syscall_stubs.S`.

The adaptation keeps the raw x86-64 `clone` trampoline, page-backed worker
stacks, kernel clear-tid join, futex generation waits, fixed reusable workers,
dynamic parallel-for chunks, explicit worker indices, and serial fallback.
It is narrowed to the `av2h_` namespace and removes task groups, statistics,
TLS, pthreads, libc, and general heap allocation. Unsupported targets compile
the serial backend; Linux x86-64 uses direct syscalls. The Linux no-libc CLI is
wired to the task substrate, while the public `av2h_decode` path remains
serial.
