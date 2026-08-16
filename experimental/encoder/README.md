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

Quality search defaults to `--search bounded`. It applies scalar DCT_DCT AC
search to luma and chroma transforms in I400, I420, and I444 through 1440
pixels per axis. The production transform set is 4x4, 4x8, 8x4, 8x8, 8x16,
16x8, and 16x16; each chroma transform searches eight low-frequency
positions. `--search off` retains the frozen legacy DC-only bitstream.
`--search directional` enables the production predictor-SSE shortlist followed
by deterministic exact DC/vertical/horizontal complete-stream trials for I400,
I420, and I444. `--search directional-exact` bypasses only the prefilter and is
the validator/oracle path. `--search directional-full` adds the reconstruction-
safe 90°..180° classic angular surface: V through D113/D135/D157 to H, including
all legal deltas in that range, with four-tap luma IDIF, linear chroma
interpolation, normative edge filtering, wide-angle mapping, and MRLS=0.
`--search directional-full-exact` exercises all 56 classic joint directional
tokens as a syntax/kernel oracle; outward D45/D67/D203 production admission is
held back until transform-order reference availability is modeled exactly.
`--search intra` extends the cardinal surface with luma
Smooth, Smooth-V, Smooth-H, and Paeth throughout the complete 1..1440
production dimension range, including the legacy 8x8 and small-I420 syntax
classes. Complete-stream trials track reconstructed reference availability at
4x4 granularity so optional Smooth endpoints match decoder traversal exactly.
Full-directional color syntax supports all eight base modes and inherited luma
deltas; the safe production search currently trials V/D113/D135/D157/H chroma
bases. Mode syntax emits the complete joint mode-set/index/offset grammar,
FSC=0, MRLS=0, and CfL=0.
Search-off hashes remain locked. Directional and intra prediction searches
remain explicit modes and are not combined with the default AC search.

`make chroma-ac-gate` reproduces the frozen production report in
`metrics/chroma-ac-gate.json`. It requires deterministic output, no per-plane
SSE regression, strict chroma wins on comparable I420/I444 fixtures, and
exact local/pinned-AVM/project reconstruction parity.

`make full-directional-gate` reproduces
`metrics/full-directional-gate.json`: 28 I400/I420/I444 generated and prepared
cases, 11 strict wins, five prepared-image wins, 5,728,016 total SSE reduction,
48 fewer bytes, deterministic output, and exact local/AVM/project parity.

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
signatures. Grayscale decodes to I400. RGB, RGBA, and palette color decode to
full-resolution I444; color is never silently subsampled. Alpha is ignored
rather than composited and does not become an AV2 plane. All three raw planar
formats and both image formats feed the admitted 1..1440 DC workspace encoder.

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
axis in 1 through 1440:

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

The color M3 workspace API additionally supports even multileaf I420 and I444
through multiple 64×64 superblocks. Luma and chroma trees are searched and
written in decoder order, with reconstructed-neighbor DC prediction, separate
U/V coefficient and sign contexts, normative U-to-V coupling, tile-persistent
adaptive CDFs and above state, and superblock-row left-state resets. The
validator matrix covers flat and generated non-flat 64×64, 128×64, and
128×128 frames at every q-context boundary.

Input metadata validation still accepts dimensions through 16384 for a precise
unsupported result, but validated encoding is limited to 1440 on either axis.
Stride must be at least width and the file must contain exactly
`stride * height` bytes. Larger dimensions exit 5 without creating output.

The input module provides codec-independent 8-bit I400, I420, and I444
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
helper. Generic raw names with missing or ambiguous geometry are rejected;
explicit `--width` and `--height` remain available. The CLI wires these plane
descriptors directly to the I400 or typed chroma workspace API.
The complete multi-leaf transform/coefficient CDF transition model is frozen
in `tests/m3_multileaf_cdf_vectors.json` and documented in
`spec-notes/m3-multileaf-cdf-state.md`. The production matrix transcript in
`tests/m3_production_matrix.json` freezes all 2,304 17..64 dimension pairs;
local reconstruction, pinned `avmdec`, and the project decoder agree exactly.
The one-superblock transcript remains frozen. The 14 historically selected complete
prepared planes are 896×424 (1), 896×448 (1), 896×504 (1), 896×598 (5),
598×896 (2), 896×672 (1), and 896×896 (3); every one has deterministic OBU
and exact local/pinned-AVM/project reconstruction parity. The explicit
`le1440` gate additionally passes all 14 prepared sources, including three
1440×1440 images and both axis orientations, with deterministic OBU and exact
three-way reconstruction parity. This is a 1440-per-axis production bound,
not arbitrary-size support. An exhaustive
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

The typed experimental multi-plane entry point
`enc_m3_dc_yuv8_q_workspace()` admits decoder-exact 8-bit DC-only I420 and
I444 at qindex 1 through 255 for every width and height from 1 through 1440.
This is 2,073,600 dimension pairs per format. Existing I400, 8×8 chroma, odd
I420, and previously admitted half-grid streams through 896 retain their
frozen level selection; only an axis above 1440 is closed. The public bound is
`ENC_M3_CHROMA_MAX_DIMENSION`.

I420 active U/V dimensions are `ceil(width/2) × ceil(height/2)`.
General coded luma axes are `8*ceil(axis/8)`; I420 coded chroma axes are half
those values and I444 coded chroma axes are equal to them. Separate chroma
partition traversal uses that 8-pixel-aligned MI boundary, producing 16×16,
16×8, 8×16, and 8×8 luma-reference leaves. The 8×8 chroma node is
normatively unsignaled. Rectangular transforms use the 32/128 EOB CDF classes,
the max-axis skip context, rectangular inverse scaling, and exact width/height
neighbor spans. All selected prepared-image dimensions, including 896×424,
896×504, 896×598, and 598×896, are admitted.

Workspace queries expose active and coded plane geometry, transform counts,
alignment, staged reconstruction, and stream capacity. Strides may be padded;
capacity, arithmetic, alignment, all output/input alias classes, workspace,
stream, and atomic commit failures are checked. The existing I400 functions and frozen 8×8 and odd-I420 chroma streams are
unchanged. Twenty frozen vectors plus dual-decoder probes cover odd geometry,
mixed-axis boundaries, non-flat color, U/V asymmetry, a prepared 896×672
I420 frame, all four required real-image boundary sizes, and the 896×896 I444
bound.

Production luma AC is available through `enc_ac8x8_yuv8_q_workspace()` and is
the CLI default for I400, I420, and I444 in the validated 1..1440 range. The
typed query reports active/coded plane
geometry, transform counts, stream capacity, and the complete caller-owned
workspace layout. Only Y changes to general 8x8 DCT_DCT coefficients; U/V keep
the existing DC transforms, reconstruction, plane isolation, and U-to-V EOB
coupling. Multi-superblock I400/I420/I444 tests and real 1440x960 preparations
are deterministic and match local, pinned-AVM, and project reconstruction.
On the prepared Baya Weaver I400 image at qindex 100, AC lowers luma SSE from
124,853,206 to 55,002,899 and raises PSNR from 28.57 dB to 32.13 dB. The
default stream is 15,234 bytes and encodes in 28.58 seconds on the recorded
validation host; `--search off` remains available for smaller DC-only output.

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
module used by the encoder CLI.
`enc_image_query()` reports exact output and scratch sizes, and
`enc_image_decode()` produces full-resolution planar 8-bit I400 or I444.

BMP support covers Windows DIB size 40 or larger, BI_RGB, top-down or
bottom-up rows, grayscale or color 8-bit indexed input, and 24-bit BGR input,
including four-byte row padding. PNG support validates the
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
