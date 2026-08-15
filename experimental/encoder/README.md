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

The active M3 subset accepts arbitrary-pixel raw 8-bit I400 images whose
dimensions are each in 1 through 16:

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
declared frame. AV2's four-pixel boundary grid implies a legal 8×8, 8×16,
16×8, or 16×16 prediction block containing one, two, or four 8×8 transforms.
Transforms reconstruct in raster order and use already reconstructed top/left
edges. Local dequantization, inverse-DC reconstruction, and clipping exactly
match both validators.

Input metadata validation still accepts dimensions through 16384 for a precise
unsupported result, but validated encoding is intentionally limited to
dimensions whose width and height are each 1 through 16.
Stride must be at least width and the file must contain exactly
`stride * height` bytes. Larger dimensions exit 5 without creating output.
The current shared-tree 32×32 attempt can emit a stream, but pinned
`avmdec` rejects it at tile data; 32×32 must therefore be treated as
unsupported until it passes both decoder gates.
This is a completed bounded submilestone, not the full M3 gate. An exhaustive
257-case check (all 256 constant 8×8 sample values plus one four-quadrant
16×16 grid) was deterministic and byte-exact between local reconstruction,
pinned `avmdec`, and the project decoder. Representative results are flat-128:
19 bytes, infinite PSNR; four-quadrant 16×16: 30 bytes, infinite PSNR.

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
validators rather than silently skipping them. The 32×32 failure was traced
to omitted transform-partition-type
syntax and an illegal sixteen-8×8 interpretation of a 32×32 prediction block;
the current four-16×16-prediction-block attempt is still rejected at tile data
and remains unsupported.

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

## CC0 newos provenance

`PROVENANCE.md` records the exact reviewed newos revisions and paths, the CC0
concepts copied or adapted, and the experiment-local implementation boundary.
The repository-wide source record remains in `provenance/cc0-sources.md`.
