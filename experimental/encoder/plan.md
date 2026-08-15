# Isolated freestanding AV2 still encoder plan

## Contract

This experiment is a new encoder, not an extension of the decoder.

- **Isolation:** every source, header, test, script, fixture manifest, generated
  artifact, and build product lives below `experimental/encoder/`. Encoder code
  must not include, compile, link, copy, generate from, or depend on decoder
  sources, headers, objects, make targets, or build directories. The repository
  decoder may only be invoked as an already-built black-box validation command.
- **License:** new work is CC0-1.0 under the repository license. Implement from
  the AV2 specification and independently recorded observations; do not copy
  third-party codec implementation code or tables unless their provenance is
  compatible with CC0 and recorded.
- **Language:** production code is freestanding C11. It has no libc, hosted
  headers, compiler runtime calls, threads, floating point, global mutable
  state, undefined behavior, or hidden allocation.
- **First target:** static, dependency-free, nolibc Linux x86-64 executable.
  Linux process entry, file I/O, timing, and exit are a thin backend using raw
  syscalls. The codec core contains no OS knowledge.
- **Future targets:** portable core plus replaceable backends for hosted C,
  other operating systems/architectures, and WebAssembly. Backend interfaces
  use fixed-width project types and caller-provided callbacks/buffers.
- **Input:** a prepared planar still image, initially 8-bit I400 and then I420
  and I444. Dimensions, bit depth, chroma format, plane paths/offsets, and
  strides are explicit CLI arguments; image-file parsing and color conversion
  are out of scope.
- **Output:** exactly one deterministic AV2 still-picture `.obu` stream. No
  AVIF, BMFF, IVF, metadata container, preview, sidecar, or stdout diagnostic is
  mixed into the output.
- **Scope:** one independently decodable closed-loop-key still image. Animation,
  inter prediction, multiple frames, and container muxing are out of scope.

No milestone may weaken these rules. A feature that cannot fit them is deferred
behind an interface rather than added as an exception.

## Allocation and ownership

The core never allocates. `enc_query_memory(config, image)` returns checked
sizes and alignments for these caller-owned regions:

1. immutable input planes;
2. output byte buffer;
3. persistent encoder state and initialized CDFs;
4. frame reconstruction planes, including borders;
5. transform/coefficient scratch;
6. partition and mode-search scratch;
7. filter/restoration trial scratch; and
8. optional metrics scratch.

`enc_init` partitions one supplied workspace arena once. Encoding performs no
subsequent allocation. All size additions, products, strides, alignments, and
bit counts are overflow-checked. The Linux backend obtains input/output and one
workspace mapping, passes bounded spans to the core, and releases them at exit.
Tests also exercise deliberately undersized and maximally misaligned arenas.
Memory use in bytes and peak committed pages are validation metrics.

## Proposed layout

```text
experimental/encoder/
  plan.md                 this architecture and milestone contract
  log.md                  append-only experiment decisions and gate results
  LICENSE                 optional local CC0 notice before code distribution
  Makefile                encoder-only targets; never invokes repository builds
  include/
    av2enc.h              public configuration, image, workspace, result API
  src/
    base.h                local integer types, checked arithmetic, spans
    arena.c               caller-owned aligned workspace partitioning
    bit_writer.c          bounded raw-bit and LEB128 writer
    obu.c                 temporal delimiter/sequence/frame OBU framing
    range.c               forward arithmetic/range encoder
    cdf.c                 independent default CDF materialization and updates
    syntax.c              sequence/frame/tile syntax emission
    image.c               prepared-plane validation and edge extension
    predict.c             intra predictors and reconstructed references
    transform.c           forward transforms
    quant.c               quantize/dequantize and coefficient costing
    coeff.c               coefficient scan and entropy syntax
    partition.c           block tree representation and legal candidates
    search.c              mode/transform/partition rate-distortion search
    filter.c              deblock/CDEF/CCSO candidate application
    restoration.c         restoration candidate interface and later kernels
    rate.c                Q selection, lambda, trial encode, byte budget
    metrics.c             local integer/fixed-point PSNR and optional SSIM
    encoder.c             frame orchestration only
  platform/
    linux_x86_64_start.S  process entry and syscall veneer
    linux_x86_64.c        CLI, mappings, files, monotonic timing
    hosted.c              future optional hosted backend, not in strict gate
  tests/
    *_unit.c              freestanding-capable module vectors
    encode_cases.c        deterministic end-to-end fixtures
    malformed.c           truncation/mutation rejection checks
  fixtures/
    manifest.txt          exact source/result hashes and image descriptions
    prepared/             small CC0 raw planar inputs only
    expected/             locked `.obu` and decoded-plane results
  tools/
    validate.py           orchestration only; no production dependency
    summarize.py          hashes, rates, metrics, speed, and size report
  pins/
    validators.txt        exact avm commit/build/options and decoder identity
  build/                  ignored, disposable, encoder-local products
```

Module dependencies point downward: `base/arena` → `bit/obu/range/cdf` →
`syntax/image/predict/transform/quant/coeff` → `partition/search/filter/rate` →
`encoder` → platform. Syntax writing never calls search, and search never
knows OBU framing. Metrics are validation helpers and cannot affect a
deterministic encode unless explicitly selected as a search distortion metric.

## Reproducibility record

Before the first conformance claim, `pins/validators.txt` and
`fixtures/manifest.txt` must lock:

- the AVM repository URL, exact commit hash, `avmdec` executable SHA-256,
  compiler identity, build flags, and full decode command;
- the repository decoder executable SHA-256, source commit, build identity,
  and full command, without making its build an encoder prerequisite;
- encoder source commit, compiler SHA-256/version, flags, target, and command;
- every prepared plane's dimensions, format, stride, byte count, and SHA-256;
- emitted `.obu` SHA-256 and byte count;
- each decoder's output format, plane SHA-256 values, and combined SHA-256; and
- measured machine/CPU, warm-up count, repetitions, timing method, peak memory,
  and executable SHA-256/size.

Validation refuses an unpinned or hash-mismatched executable. Hashes are exact,
not “latest” labels.

## Common gate for every milestone

Each completed milestone must:

1. compile its production objects with C11, `-ffreestanding`, `-nostdinc`,
   disabled builtins/stack protector/unwind tables, warnings as errors, and no
   decoder include or library path;
2. link the strict Linux x86-64 executable statically with no dynamic
   interpreter, `DT_NEEDED`, libc, compiler runtime, or undefined symbol;
3. pass focused unit tests plus ASan/UBSan hosted test builds where available;
4. encode twice from identical bytes and produce byte-identical `.obu` hashes;
5. have pinned `avmdec` accept the stream and produce the expected exact plane
   hashes;
6. have our decoder either produce the same exact plane hashes or record a
   specific unsupported-feature result; after a feature is supported by both,
   disagreement blocks the milestone;
7. report source bytes, `.obu` bytes, bits/pixel and, for a declared still-image
   presentation duration, bitrate; no ambiguous “quality” number substitutes
   for byte count;
8. report per-plane and weighted PSNR from a local deterministic implementation;
   report SSIM only after a local, versioned integer/fixed-point implementation
   is complete, otherwise state `SSIM: not implemented`;
9. report median and worst encode time, pixels/second, peak workspace, and
   stripped static executable size; and
10. run malformed checks: every truncation of each small golden stream, bad
    LEB128 sizes, forbidden/reserved OBU bits, inconsistent payload lengths,
    nonzero alignment padding, and selected entropy-bit mutations must be
    rejected by at least pinned `avmdec` or our decoder as applicable. The
    encoder must also reject short input, bad strides, unsupported formats,
    output exhaustion, and workspace exhaustion without partial success.

For syntactically valid entropy mutations that decode to a different image,
the expected result is a changed decoded hash, not mandatory rejection. Crashes,
hangs, out-of-bounds access, acceptance beyond declared input, or decoder
disagreement are always failures.

## Milestones

Status key: `[x]` completed gate, `[~]` active work whose exit gate has not
passed, `[ ]` not yet completed.

### [x] M0 — Isolation, substrate, and empty framing tests

Create the local build, types, checked arithmetic, spans, arena, bit writer,
canonical LEB128, OBU header/size writer, Linux syscall backend, and strict
link audit. Unit-test every bit boundary, output exhaustion, LEB128 width, and
arena alignment. The executable may only emit test vectors, not claim an image.

**Exit:** common compile/link gates pass; OBU framing vectors match independent
hand-calculated bytes; a repository-wide dependency audit shows no encoder
reference to decoder sources/builds.

### [x] M1 — Trivial decodable still

Write the minimal sequence header, closed-loop-key frame header, one-tile
layout, and fixed syntax necessary for a tiny 8-bit I400 image composed solely
of constant/DC blocks. Implement the smallest forward range encoder and frozen
CDF subset needed by that stream. Start with one fixed dimension and constant
sample value, but derive all lengths rather than embedding a complete golden
stream.

**Exit:** a generated `.obu`, not a copied fixture, is accepted by pinned
`avmdec`; decoded dimensions and plane hash equal the declared constant image;
our decoder agrees if its supported profile covers the stream.

**Completed result:** the fixed M1 CLI generates a 21-byte 8×8 constant-128
I400 stream with SHA-256
`4c13e82fce9975d573b92b8ca2da25e1fed90308ac6857ec4d2d92dba0268b11`.
Pinned `avmdec` and the project decoder both accept it and produce the same
64-byte plane hash
`1df1b7ce1fd8fcbe20cde61646875e54fe38d8945ea7911afd59e025cc520a68`.
This is only a fixed constant-image milestone, not arbitrary-image support.
Exact commands, tool hashes, measurements, and limitations are in `log.md`.

### [x] M2 — Complete entropy-writing foundation

Implement normalized range encoding, bypass literals, inverse-CDF symbol
mapping, adaptive CDF updates, tell/cost accounting, flush behavior, default
CDF expansion, and syntax helpers for signed, quasi-uniform, UVLC, alignment,
and coefficient values. Build round-trip transcripts against independently
specified decoder vectors and freeze exact encoded bytes.

**Exit:** independent C and Python transcript decoders agree with the C writer
for every alphabet size 2 through 16 and update boundary; split-buffer and
capacity tests pass. Separately, generated complete M1/M3/M4 streams decode
byte-identically in the pinned `avmdec` and project decoder. Raw range
transcripts are not image streams and are never presented to those decoders.

**Completed result:** the normalized writer and independent decoder models
cover every supported alphabet size (2 through 16), every symbol, exact
tell/cost changes, bypass literals through 32 bits, deterministic finish,
split-buffer decoding, long carry/`0xff` paths, and exact/short/zero
capacities. CDF materialization and adaptation cover all 125 normative
parameter combinations and the count transitions around 16 and 32. Raw and
range syntax helpers cover signed literals, UVLC/SVLC, `ns`, subexponential
coding with reference recentering, alignment/trailing bits, and general
base/BR/remainder coefficient values. The C and Python models independently
decode the frozen writer transcript matrix; six generated complete M1/M3/M4
streams, rather than raw transcripts, decode exactly in the hash-pinned AVM
v1.0.0 and project validators. Exact commands, hashes, capacities, and post-M2
limits are recorded in `log.md` and `spec-notes/m2-validator-matrix.md`.

### [~] M3 — Real prepared-image DC encoder

Accept arbitrary bounded prepared I400 dimensions and strides. Partition into
a fixed legal block grid, form a DC prediction from reconstructed neighbors,
compute residual means, quantize DC, entropy-code coefficients, inverse
quantize/reconstruct locally, and extend edges. No input pixels may be ignored.

**Exit:** flat, ramp, impulse, odd-size, and photographic luma fixtures encode;
the encoder's reconstruction hash exactly equals each validator's decoded
hash; quality changes monotonically in aggregate over a locked Q sweep.

**Completed bounded submilestone (not full M3):** arbitrary-pixel dimensions
1 through 16 use legal edge-extended 8×8 transform grids. Qindex 1 through 255
is configurable, with default 100 compatibility. A locked generated/prepared
crop sweep proves exact local/AVM/project parity and aggregate SSE
non-increasing as qindex decreases (quality increases).
The complete 256-pair dimension matrix is exercised with an odd-sensitive
pattern through local reconstruction and both decoders.
Caller-owned stream and compact active reconstruction spans are capacity
checked; the core stages both and commits neither on output or reconstruction
exhaustion. The CLI retains atomic `.obu` replacement and reports exact bytes,
active-pixel SSE/PSNR, reconstruction bytes, and qindex.
Boundary-implied
rectangular prediction blocks split horizontally or vertically into two 8×8
transforms; 16×16 splits four ways. Raster reconstruction, coefficient
contexts, base/BR/remainder syntax, exact dequantization, inverse-DC
reconstruction, and clipping are decoder-exact. The gate remains active
because broader dimensions, general partition/mode search, filters, and rate
control are not implemented. A shared-tree 32×32
four-16×16-prediction-block attempt exists, but its current stream is rejected
at tile data by pinned `avmdec`; it is not supported or part of this completed
submilestone.

### [~] M4 — Forward transforms, quantization, and AC coefficients

Add normative forward transforms in small-to-large order, scans, EOB/base/rice
coefficient syntax, quant/dequant parity, clipping, and transform selection.
Begin with DCT-only square blocks, then legal rectangles and selected intra
transform types. Keep scalar reference kernels as the portability oracle.

**Exit:** transform/quant unit vectors are exact; nonzero AC survives decode;
at equal or lower bytes than M3, the locked corpus has no aggregate PSNR
regression and improves on textured cases.

**Completed bounded submilestone (not full M4):** an explicit 8×8 I400 mode
searches DCT_DCT DC and vertical `(1,0)` AC levels at qindex 50, emits
scan/EOB/base/sign syntax, and reconstructs with the scalar normative kernel.
The canonical vertical gradient is exact in local reconstruction, pinned AVM,
and the project decoder. Broader AC positions and magnitudes, transforms,
dimensions, coefficient contexts, and corpus RD gates remain open.

### [ ] M5 — Chroma and bit-depth surface

Add prepared 8-bit I420, then I444, with explicit chroma siting assumptions and
independent plane strides. Implement chroma prediction, transform/quantization,
coefficient contexts, and optional CFL only after basic chroma is stable.
Reserve API representation for 10/12-bit samples; implement higher bit depth
only after 8-bit gates are stable.

**Exit:** odd and even dimensions in I400/I420/I444 pass exact reconstruction
hashes in both validators; no plane overread is possible; per-plane metrics and
weighted metrics are reported.

### [ ] M6 — Partition and intra-mode search

Represent legal partition trees and neighbor context independently of syntax
emission. Add deterministic candidate enumeration: block sizes/partitions,
DC/directional/smooth/Paeth predictors, transform choices, and skip decisions.
Use bounded rate-distortion cost from exact range-writer bit cost plus local
reconstruction distortion. Tie-break by a documented stable ordering.

**Exit:** search-off reproduces prior hashes; search-on is deterministic,
decoder-exact, and Pareto-improves at least one locked corpus point without
worsening aggregate distortion at equal-or-lower bytes.

### [ ] M7 — In-loop filters and restoration choices

Add deblock, then CDEF, then CCSO as independent trial choices. Encode a choice
only after the encoder's filtered reconstruction matches both decoders exactly.
Keep “off” as a candidate. Introduce restoration through a disabled-by-default
interface; implement each restoration type only with exact local kernels and
bounded trial scratch.

**Exit:** for every enabled filter, local post-filter plane hashes equal both
validators. Search chooses filters by deterministic RD cost and can retain
off. No filter is enabled merely because it improves an unverified metric.

### [ ] M8 — Rate control and quality ladder

Map a stable quality setting to base Q, plane deltas, lambda, search effort, and
filter trials. Add constant-Q first, then bounded trial encoding for target
bytes/bits-per-pixel. Trial output uses caller-provided scratch and commits only
the selected stream. Define deterministic behavior when a target is impossible.

**Exit:** the locked quality ladder records hashes and metrics; size is
nonincreasing as requested quality decreases except documented syntax
granularity; target-size mode meets `max(32 bytes, 2%)` when a feasible point
exists; reconstruction remains exact across validators.

### [ ] M9 — Validation hardening and useful baseline

Expand the CC0 prepared corpus across flat areas, edges, noise, text, natural
detail, odd dimensions, and all supported chroma formats. Run exhaustive small
input/capacity cases, mutation/truncation tests, sanitizers, strict-link audits,
and repeated deterministic benchmarks. Establish local PSNR and, only if
implemented without dependencies, SSIM. Compare rate-distortion and speed
against the previous milestone and a separately pinned AVM reference encode,
without linking or importing AVM.

**Exit:** all common gates pass; no malformed stream crashes either validator;
the encoder offers at least three useful deterministic quality points; reports
contain exact hashes, bitrate/size, PSNR, SSIM status, speed, memory, and binary
size; known unsupported tools fail before output commit.

### [ ] M10 — Portable backends without weakening the first target

Freeze the core/backend contract, add a hosted test backend and one second
architecture or WebAssembly backend, and verify endian/alignment assumptions.
Optimized kernels are optional and selected explicitly; scalar C remains the
bit-exact oracle.

**Exit:** identical configuration and input produce identical `.obu` and
decoded hashes on every backend; Linux x86-64 remains static/nolibc and does
not gain a hosted dependency.

## Stop/restart boundaries

Work stops only after a small golden set, its manifest hashes, gate commands,
and results have been appended to `log.md`. A boundary is restartable when:

- production and tests build from a clean `experimental/encoder/build/`;
- the last passing milestone can be selected with one documented command;
- generated files are either reproducible or listed with exact hashes;
- unfinished code is unreachable behind a default-off feature flag;
- the previous milestone's golden `.obu` hashes still reproduce; and
- the next unimplemented syntax/tool is named with its required oracle.

If a gate fails, revert the active feature to default-off, retain the smallest
reproducer under `fixtures/`, and record `BLOCKED` with expected/actual hashes
and commands. Restart from the last passing milestone, not from an unverified
bitstream. Changes to CDF initialization, range flush, syntax order, transform
rounding, or filtering require rerunning all earlier golden hashes because they
cross subsystem boundaries.

## Explicit non-goals

No decoder refactor, shared codec library, repository build integration,
container output, image loader, network access, runtime CPU dispatcher,
assembly-only algorithm, hidden heap, external metric library, or unpinned
oracle is part of this experiment. Promotion outside this directory requires a
separate reviewed project after the experimental gates pass.
