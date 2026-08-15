# Experimental freestanding AV2 encoder

This directory is an isolated Linux x86-64 C11/nolibc encoder experiment. Run
`make -C experimental/encoder` to build it and
`make -C experimental/encoder test` to run field, range-writer, exhaustion,
decoder, static-ELF, and dependency-isolation checks. It is intentionally
absent from the repository's top-level build.

The full `test` gate requires executable pinned reference and project
decoders. Their paths remain configurable:

```sh
make -C experimental/encoder test \
  AVMDEC="$PWD/build/reference/avm/avmdec" \
  PROJECT_DECODER="$PWD/build/host-cmake/av2heif-decoder"
```

Missing or non-executable validators fail the full gate; decoder tests are
never silently skipped. `make -C experimental/encoder test-unit` runs the
local, dependency-independent checks and explicitly skips tests needing those
external validators. It is not a substitute for the full decode-validation
gate.

## Input CLI contract

The ordinary invocation is:

```sh
experimental/encoder/build/av2-encoder-experiment INPUT
```

The input suffix is matched case-insensitively. Recognized suffixes are
`.i400`, `.i420`, `.i444`, `.png`, and `.bmp`; PNG and BMP can also be
recognized from their signatures when their suffix is not descriptive. The default output is the
sibling path formed by replacing that suffix with `.obu`; for example,
`scene.v1-896x598.i400` writes `scene.v1-896x598.obu`. A content-recognized
image with no recognized suffix has `.obu` appended instead. `-o PATH` and
`--output PATH` override it. `--qindex 1..255` is accepted in the ordinary
form and defaults to 100, exactly like the legacy I400 form. Options may
appear before or after the single input.

Raw planar bytes do not identify their dimensions. With no explicit
`--width W --height H`, geometry is accepted only from:

* exactly one `WIDTHxHEIGHT` token in the basename before the raw suffix; or
* a sibling `manifest.json` whose single top-level `"dimensions": [W, H]`
  applies to an output record whose `"filename"` exactly matches the input
  basename.

If both sources exist, they must agree. Missing, repeated, malformed, or
conflicting metadata is rejected rather than guessed. `--stride S` is
available for explicit or discovered I400 input; otherwise the I400 stride is
the width. The older
`--i400-8 INPUT --width W --height H [--stride S] [--qindex Q] OUTPUT`,
`--constant-128-8x8`, and `--experimental-m4-ac` forms remain supported.

PNG and BMP are recognized by suffix and checked against their file
signatures. Their reusable color-input contract is 8-bit I444: color is not
silently subsampled. Alpha does not become an AV2 plane. The current DC
encoder subset reports these color inputs, and raw I420/I444, as unsupported
without creating output.

The encoder refuses an output path that names the input, including an
existing hard-link or symlink alias. Existing non-input outputs are replaced
using an exclusive sibling temporary file and atomic rename, so a failed
encode/write preserves the previous output and cleans its temporary file.
Exit statuses are 0 success, 2 command usage, 3 invalid/ambiguous input
metadata or format, 4 input I/O/size failure, 5 recognized but unsupported
input, 6 encoder failure, and 7 output I/O failure.

The M2 validation matrix is frozen in `fixtures/m2-transcript-matrix.json`
and `fixtures/m2-full-streams/manifest.json`. Independent C and Python range
decoder models both check C-writer transcripts for every 2-through-16-symbol
alphabet at update counts 0/15/16/31/32; the native suite additionally covers
all 125 CDF parameter combinations, bypass/tell/finish, split buffers,
carry/`0xff` stress, and capacity failures. Raw range transcripts are not
valid image streams. Separately, six generated complete M1/M3/M4 streams cover
coefficient-free, base/BR/remainder, multi-block adaptation, and AC/sign
syntax; these must decode byte-identically in hash-pinned AVM v1.0.0 and the
project decoder.

The M1 CLI is:

```sh
experimental/encoder/build/av2-encoder-experiment \
  --constant-128-8x8 output.obu
```

It generates one deterministic 8×8, 8-bit, I400 Closed Loop Key still whose
64 decoded samples are all 128. Sequence/frame fields, OBU headers and
canonical LEB128 lengths are written from named syntax. Every tile decision is
sent through the forward range writer using CDFs materialized from the
normative AV2 v1.0.0 attachment values listed in `PROVENANCE.md`; no fixture or
complete golden stream is embedded in production code.

The active M3 subset accepts arbitrary-pixel raw 8-bit I400 images with each
axis in 1 through 896:

```sh
experimental/encoder/build/av2-encoder-experiment \
  --i400-8 input.i400 --width 8 --height 8 \
  [--stride 8] [--qindex 1..255] output.obu
```

Every input pixel participates in deterministic SSE minimization. Qindex 100
is the default and preserves the pre-option stream; 1 through 255 is the
validated non-lossless range. Qindex 0 is rejected because it changes the
frame to lossless transform semantics that this M3 path does not emit.
Active samples are edge-extended to complete 8×8 transform blocks for DC
selection, while reported SSE and returned reconstruction are cropped to the
declared frame. AV2's four-pixel boundary grid implies legal rectangle-only prediction leaves.
The scalable path visits 64×64 superblocks row-major and traverses each root
depth first. It reconstructs each leaf's 8×8
transforms in local raster order, persists adaptive CDFs across the tile, and
updates coefficient contexts after every transform. Local dequantization,
inverse-DC reconstruction, clipping, and the compact cropped reconstruction
exactly match both validators.

Input metadata validation still accepts dimensions through 16384 for a precise
unsupported result, but validated encoding is limited to 896 on either axis.
Stride must be at least width and the file must contain exactly
`stride * height` bytes. Larger dimensions exit 5 without creating output.

The input module also provides codec-independent 8-bit I400, I420, and I444
views and exact-size loaders. Each plane has a typed byte span plus
width/height/stride; I420 chroma uses ceil-divided geometry, so odd luma sizes
retain their final chroma row and column. Plane sizes and offsets are checked,
the packed file may contain neither truncation nor trailing bytes, overlapping
caller-provided planes are rejected, and close releases the platform mapping
and clears every borrowed pointer.

Dimensions may be discovered only from an existing prepared-image
`manifest.json` whose output record agrees with the file, or from the exact
prepared names `source-WxH-gray8.i400`, `source-WxH-yuv420p8.i420`, and
`source-WxH-yuv444p8.i444`. `tools/raw_planar_input.py` exposes the manifest
helper. Generic raw names do not imply dimensions and are intentionally
rejected. Chroma loading is an input-boundary capability only; codec syntax
and the current CLI encoding mode remain I400.
The complete multi-leaf transform/coefficient CDF transition model is frozen
in `tests/m3_multileaf_cdf_vectors.json` and documented in
`spec-notes/m3-multileaf-cdf-state.md`. The production matrix transcript in
`tests/m3_production_matrix.json` freezes all 2,304 17..64 dimension pairs;
local reconstruction, pinned `avmdec`, and the project decoder agree exactly.
The one-superblock transcript remains frozen. The 14 selected complete
prepared planes are 896×424 (1), 896×448 (1), 896×504 (1), 896×598 (5),
598×896 (2), 896×672 (1), and 896×896 (3); every one has deterministic OBU
and exact local/pinned-AVM/project reconstruction parity. This is a
896-per-axis I400 bound, not arbitrary-size or chroma support. An exhaustive
257-case check (all 256 constant 8×8 sample values plus one four-quadrant
16×16 grid) was deterministic and byte-exact between local reconstruction,
pinned `avmdec`, and the project decoder. Representative results are flat-128:
19 bytes, infinite PSNR; four-quadrant 16×16: 30 bytes, infinite PSNR.

The scalable C entry point is `enc_m3_dc_i400_q_workspace()`.
`enc_m3_query_workspace()` returns overflow-checked padded geometry, block and
pixel counts, alignment, and the exact caller-owned workspace size. Short or
misaligned workspaces, reconstruction spans, and stream spans fail without
modifying caller outputs. The older `enc_m3_dc_i400[_q]()` wrappers remain source-compatible with their
64×64 bounded internal arena; larger encoding uses the workspace entry point.
The Linux CLI obtains
workspace, stream, and reconstruction storage from anonymous page mappings;
allocation stays outside the freestanding codec core.

The bounded M4 experiment is an explicit 8×8 DC-plus-vertical-AC mode:

```sh
experimental/encoder/build/av2-encoder-experiment \
  --experimental-m4-ac input.i400 output.obu
```

It searches signed DC and `(1,0)` DCT_DCT levels at qindex 50, emits EOB=2
with normative transform/base/sign syntax, and reconstructs locally. Because
this bounded syntax does not yet emit BR/remainder symbols, both searches are
restricted to the fully encodable nonzero magnitude range 1 through 4. Every
64-byte input therefore produces a valid, possibly lossy stream; no output is
created for input-size or encoder failures. The
canonical rows `128 128 128 129 129 129 130 130` select levels `+3,-2`,
reconstruct exactly, and match both decoders. The default `--i400-8` M3 path
and its qindex-100 DC behavior are unchanged.

That command emits 21 bytes with SHA-256
`9d84278811a478aaece5b87bf9340e47d1dfd20badc6af0765776840f8d98c15`.
Local C, pinned `avmdec`, and the project decoder reconstruct the exact source,
SHA-256
`fa5e2dc3cd94a1e52711093de4db4bc9d7d1124d86c2c4389534521761edbf93`
(SSE 0, infinite PSNR). This is the first completed AC-stream submilestone,
not full M4.

Exit statuses are 0 success, 2 CLI usage, 3 invalid input metadata, 4 input
I/O or exact-size failure, 5 accepted-but-unsupported input, 6 encoder failure,
and 7 output I/O failure. Diagnostics are written to standard error.

## M1 status

M0 and the fixed constant-image M1 gate are complete. From the repository root:

```sh
experimental/encoder/build/av2-encoder-experiment \
  --constant-128-8x8 experimental/encoder/build/m1-final.obu
```

The result is 21 bytes with SHA-256
`4c13e82fce9975d573b92b8ca2da25e1fed90308ac6857ec4d2d92dba0268b11`.
Pinned `avmdec` and `build/host-cmake/av2heif-decoder` both accept it and emit
the same 64-byte I400 plane, SHA-256
`1df1b7ce1fd8fcbe20cde61646875e54fe38d8945ea7911afd59e025cc520a68`.
The executable remains static with no interpreter, dynamic dependencies, or
undefined symbols. `make -C experimental/encoder test` covers M1 and the
bounded M3 subset with native/Python tests, decoder checks, smoke/input checks,
and dependency/ELF audits.

The CLI encodes generated flat, ramp, impulse and multi-block edge images,
padded strides, all 256 dimension pairs in 1..16, and even/odd prepared
photographic-luma crops. Stride products and the final active input offset are
overflow-checked; output is committed by an exclusive sibling temporary file
plus atomic rename, with cleanup on failure; the full gate requires both
validators rather than silently skipping them. The 32×32 investigations found two separate errors: omitted transform-
partition-type syntax in the one-block path, and a coded 32×32 square split
where AV2 v1.0 requires rectangular partitioning. The verified replacement is
documented in `spec-notes/m3-large-partitions.md`; it is not yet production
syntax.

Remaining work is deliberately broad: larger dimensions, general AC
coefficient streams and transforms, chroma, partition/intra search, in-loop
filters/restoration, and quality/rate control.

The retained AVM 8×8 fixture is a different valid 19-byte stream
(`3fd4a46a30554e31d4fd1da790782ac44008973a809a28d1b52549df191861bc`)
with the same decoded hash. Exact AVM byte reproduction is not required.
The independent Python entropy model now uses normative `EC_PROB_SHIFT=7`,
distinguishes official cumulative from writer-side inverse CDFs, and decodes
the generated `8d80` transcript as `delta_q_abs=1`. Its tests and both compiled
decoder gates pass.

## Isolation

Only sources below `experimental/encoder` are compiled. No decoder production
header, source, object, library, or build target is included or linked.

## Standalone BMP and PNG input module

`include/encoder/image_input.h` exposes a caller-owned, memory-to-memory input
module; it is intentionally not wired into the encoder CLI or codec.
`enc_image_query()` reports exact output and scratch sizes, and
`enc_image_decode()` produces full-resolution planar 8-bit I400 or I444.

BMP support covers Windows DIB size 40 or larger, BI_RGB, top-down or
bottom-up rows, 8-bit indexed input whose used entries are grayscale, and
24-bit BGR input, including four-byte row padding. PNG support validates the
signature, chunks, CRCs, ordering, zlib Adler-32, and accepts non-interlaced
8-bit grayscale, RGB, RGBA, and palette images, all five filters, split IDAT,
and stored/fixed/dynamic DEFLATE. Other depths, interlace, grayscale-alpha,
BMP compression/bitfields, preset dictionaries, and unknown critical PNG
chunks are explicitly unsupported.

Alpha is ignored: RGBA uses RGB and palette transparency does not alter RGB.
The deterministic full-range conversion is
`Y=(54R+183G+19B+128)/256`, `U=128+(-29R-99G+128B)/256`, and
`V=128+(128R-116G-12B)/256`, using C11 truncation toward zero and clipping.
Generated tests cover the supported variants plus truncation, CRC, filter,
DEFLATE, capacity, header, overflow, and unsupported-feature failures. A
prepared 896×896 RGB PNG is decoded when the corpus is present.

## CC0 newos provenance

`PROVENANCE.md` records the exact reviewed newos revisions and paths, the CC0
concepts copied or adapted, and the experiment-local implementation boundary.
The repository-wide source record remains in `provenance/cc0-sources.md`.
