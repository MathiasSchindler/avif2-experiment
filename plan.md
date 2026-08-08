# AV2-HEIF experimental implementation plan

## Objective

Build a sibling project for experimenting with AV2 still images carried in
HEIF without changing `avif-decoder` or implying that the result is a
standardized AVIF successor.

The production decoder and muxer will be CC0, freestanding C11, allocation-free
at their core, and usable without libc. Hosted tools may use separately
licensed vendored software to create, inspect, decode, and compare artifacts.
Those dependencies are test and research infrastructure, not part of the
freestanding product.

The working format name is **AV2-HEIF experimental v0**. A permanent repository
and API name must be chosen before public distribution; it should not imply an
approved "AVIF 2" standard.

## Decisions

1. Keep `avif-decoder` AVIF/AV1-only and unchanged.
2. Create a new sibling repository rather than a fork.
3. Implement carriage and deterministic muxing before pixel reconstruction.
4. Treat use of `av02` and `av2C` for an image item as project-defined until a
   normative AV2 image-item binding exists.
5. Do not use AVIF brands, media types, extensions, or conformance claims for
   canonical v0 files.
6. Make the canonical format use the draft-shaped nine-byte `av2C`, with the
   Sequence Header OBU in `configOBUs` and coded picture data in the item
   payload.
7. Keep the four-byte AV1-shaped `av2C` used by experimental libavif/AVM code as
   a separate compatibility dialect. Support it through an importer, not by
   weakening the canonical parser.
8. Require strict parsing, bounded resource use, deterministic output, and
   sample-exact differential decoding for every supported profile.

## Licensing and dependency boundary

The repository has two deliberately separate software domains.

### CC0 freestanding domain

The public library, canonical parser, muxer, decoder, and self-contained tests
must be project-owned CC0 code or recorded CC0 code extracted from
`avif-decoder`.

This domain must:

- compile as freestanding C11;
- support `-ffreestanding` and the selected `-nostdlib`/`-nostdinc` policy;
- perform no file I/O in the core;
- use caller-owned input, output, and workspace memory;
- avoid hidden allocation, global mutable state, and compiler runtime helpers;
- build and run its ordinary tests without network access or vendored codecs;
- expose no types or headers from hosted or third-party dependencies;
- never link vendored reference code into release libraries or binaries.

Any code taken from `avif-decoder` must be reviewed for AV1 assumptions,
renamed into the new project namespace, and recorded with its source commit and
paths in a provenance document.

### Hosted reference domain

Separately licensed software may be vendored in dedicated subdirectories for
research, fixture generation, inspection, interoperability testing, and
differential comparison. Candidate dependencies include AVM, rav1e, dav1d,
libaom, libavif, AV2 Tools, and other independent container or codec tools.

Vendored software may use libc, C++, CMake, Meson, Python, shell scripts, and
its normal hosted dependencies. Each dependency must:

- live under `vendor/<name>/` or be obtained into an ignored local reference
  cache by a lock-aware setup script;
- retain its original license, notices, and patent terms;
- be pinned to an immutable revision and, where practical, an archive hash;
- have its purpose and build recipe recorded in `vendor/README.md`;
- be built only by explicit hosted/reference targets;
- write generated artifacts outside the production source tree;
- be absent from freestanding include paths and link graphs;
- not determine production behavior merely because one implementation behaves
  a certain way.

Thin project-owned adapters should translate between a common artifact model
and each reference implementation. This allows several tools to inspect the
same bytes independently without introducing their data structures into the
core.

The repository must not claim that third-party code is CC0. Distribution of a
source bundle containing `vendor/` is distinct from distribution of the CC0
core and needs a complete license review.

## Proposed repository layout

```text
av2-heif-experiment/
  LICENSE
  README.md
  Makefile
  docs/
    architecture.md
    compatibility.md
    evidence.md
    format-v0.md
    licensing.md
    testing.md
  spec/
    README.md
    revisions.lock
  provenance/
    cc0-sources.md
    generated-artifacts.md
  include/
    av2heif.h
  src/
    base.c
    base.h
    container/
    carriage/
    decoder/
    output/
  tools/
    av2heif-inspect.c
    av2heif-pack.c
    av2heif-import-libavif.c
  tests/
    unit/
    strict/
    fixtures/
    manifests/
    differential/
    fuzz/
  adapters/
    avm/
    rav1e/
    dav1d/
    libaom/
    libavif/
    av2-tools/
  vendor/
    README.md
    avm/
    rav1e/
    dav1d/
    libaom/
    libavif/
    av2-tools/
  scripts/
    setup-references.sh
    generate-fixtures.sh
    compare-artifacts.sh
```

The exact source decomposition should follow implemented state and test
boundaries. Empty codec-specific directories should not be created until a
tool is actually integrated.

## Initial source locks

Record these immutable inputs in `spec/revisions.lock`:

| Source | Revision |
| --- | --- |
| AVM v1.0.0 | `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` |
| AV2 specification v1.0.0 | `016ffa9cbb0d5538f92736100f82f78733581149` |
| AV2 specification HTML blob | `75b46fae50d6e8aaf38fe99a6f2a7b7e38b88abe` |
| AV2-ISOBMFF reviewed draft | `1b5fed56bf183a0d2790b30a57bb0605e474b1dc` |
| Local `avif-decoder` reuse source | `57667e4e01f62bc03593e2f84129eca428afaa7b` |

Pins for libavif, AV2 Tools, rav1e, dav1d, and libaom should be added only when
their adapters are introduced. A lock entry must include the revision, source
URL, license identifier, retrieval method, and artifact or checkout hash.

## Milestone 0: prove the carriage assumption

**Status:** Implemented. The four-fixture differential gate passes; see
`docs/milestone-0.md` for reproducible commands, hashes, scope, and limitations.

The first executable result must test assumption A2: moving the Sequence Header
OBU from AVM's elementary stream into `av2C.configOBUs` must preserve decode
semantics when the logical AV2 stream is reconstructed.

Work:

1. Build pinned AVM v1.0.0 in the hosted reference domain.
2. Verify and record the exact command for encoding one 8-bit 4:2:0 frame as
   length-delimited OBU output.
3. Parse the output into an ordered OBU ledger.
4. Decode the original stream with AVM and record dimensions, format, and raw
   plane hashes.
5. Extract the Sequence Header OBU into draft-shaped `configOBUs`.
6. Reconstruct the logical stream from the configuration and coded-data spans.
7. Decode the reconstructed stream with the same AVM build.
8. Require byte-identical reconstructed planes and matching metadata.

Run this for at least a tiny image, an odd-dimension image, an ordinary image,
and a high-detail image. One passing fixture is evidence, not confirmation of
the general assumption.

Exit gate: all selected streams decode identically before and after
normalization. If they do not, stop canonical HEIF implementation and either
define a clearly project-private payload revision or wait for clarified
image-item rules.

## Milestone 1: freestanding substrate and carriage core

**Status:** Implemented. The vendor-free sanitizer, strict freestanding, and
undefined-symbol gates pass; see `docs/milestone-1.md` for the supported
profile, API surface, negative coverage, and limitations.

Extract only reusable generic pieces from `avif-decoder`:

- checked size arithmetic;
- byte and bit readers;
- caller-backed arena;
- deterministic byte and bit writers;
- LEB128 support;
- generic checked BMFF box framing.

Replace AVIF-prefixed APIs and remove AVIF defaults, brands, capability state,
and assumptions. Build a new focused test harness and port only relevant test
vectors.

Then implement:

- strict AV2 Annex B length parsing and writing;
- AV2 OBU header parsing, including extension and reserved-bit checks;
- an ordered OBU ledger;
- enough Sequence Header and content-interpretation parsing to derive v0
  metadata;
- draft-shaped nine-byte `av2C` parsing and writing;
- cross-checks between `av2C` fixed fields and the effective Sequence Header.

Exit gate: strict and hosted sanitizer tests pass without any vendored software
present, and malformed lengths, headers, ordering, and field mismatches are
rejected deterministically.

## Milestone 2: deterministic AV2-HEIF v0

**Status:** Implemented. The byte-exact writer, strict typed parser, sanitizer,
freestanding, and undefined-symbol gates pass; see `docs/milestone-2.md` for
the canonical graph, API contract, mutation coverage, and limitations.

Implement a strict typed parser and deterministic writer for one primary image
item with:

- one `av02` item;
- an essential draft-shaped `av2C` property;
- `ispe`, `pixi`, and `colr` properties;
- construction method 0;
- one `mdat` extent;
- stable box and property ordering;
- generic HEIF identification only where valid;
- no AVIF brands or standardized AV2 image-format claims.

Add byte-exact goldens, parser/writer round trips, checked offset and extent
arithmetic, required-property validation, unknown-essential-property failure,
and negative tests for truncated, duplicate, dangling, conflicting, or
ambiguous structures.

Exit gate: canonical output reparses into the expected typed model, remuxing an
already normalized model is byte-identical, and all named mutations fail at
the expected layer without AVM or other hosted dependencies.

## Milestone 3: artifact laboratory

**Status:** Implemented. The four-fixture offline generate, normalize, mux,
inspect, extract, decode, and compare gate passes; see `docs/milestone-3.md` for
the artifact schemas, comparison semantics, reproducible command, and limits.

Create a common hosted artifact description containing:

- source and generated-file hashes;
- container box tree and item/property associations;
- ordered OBU ledger and byte spans;
- effective codec metadata;
- decoded dimensions, format, range, and CICP;
- normalized raw-plane hashes;
- tool name, exact revision, build options, and command line.

Adapters may then use AVM, rav1e, dav1d, libaom, libavif, AV2 Tools, or other
software where each tool is applicable. AV1-only software remains useful for
container precedent, framing comparisons, test corpus creation, and controlled
AV1-versus-AV2 experiments; it is not treated as an AV2 oracle.

Comparisons should distinguish:

- identical encoded bytes;
- equivalent normalized OBU structure;
- matching interpreted metadata;
- sample-exact decoded planes;
- lossy quality or compression measurements.

Exit gate: one command can generate, normalize, mux, inspect, extract, decode,
and compare the initial fixture matrix using pinned local reference builds.
Normal comparison runs must not require network access.

## Milestone 4: freestanding query surface

**Status:** Implemented. Bounded validation, structural dialect detection,
metadata query, exact I420 plane sizing, input binding, and the strict
freestanding dependency gate pass; see `docs/milestone-4.md` for the public
contract, defaults, diagnostics, fixture parity, and limitations.

Add the public API for caller limits, structured errors, container validation,
dialect detection, metadata query, output sizing, and conservative workspace
sizing. Consume `configOBUs` and item payload as separate spans rather than
constructing a temporary concatenated stream.

Exit gate: query results match fixture manifests, failures identify the owning
layer, and dependency inspection finds no unintended libc, allocator, C++
runtime, or compiler helper references in freestanding artifacts.

The original complete-decode slice is divided below into six approximately
equal engineering-risk packets. They are balanced by ownership boundary and
proof burden rather than projected line count: private foundation, frame-wide
wiring, ordinary coverage, advanced coverage, filter parity, and public API
hardening each receive an independent exit gate.

## Milestone 5: private decoder foundation and tiny fixture

**Status:** Implemented. The private, allocation-free decoder foundation owns
frame and tile syntax, adaptive entropy state, bounded partition traversal,
neighbor contexts, luma/chroma mode syntax, sparse coefficient decoding,
prediction, inverse transforms, and live-plane writes. All eleven tiny luma
leaves and four tiny chroma leaves reconstruct sample-exactly through tell 725.
Focused private kernels cover deblock, CDEF, CCSO, and accepted-profile GDF;
restoration remains unsupported and fail-closed. See `docs/milestone-5.md`.

Exit gate: the tiny fixture reconstructs sample-exactly on every unfiltered
plane through a caller-owned tile orchestrator under hosted sanitizers and the
strict freestanding dependency gate. Every unimplemented syntax or
reconstruction path fails before leaf commit or final-plane mutation.

## Milestone 6: frame-wide wiring and odd fixture

**Status:** Implemented. Caller-owned coded Y, U, and V planes now retain
visible reference bounds separately from padded storage and use per-4x4
readiness maps to prevent references to future leaves. All 12 luma and five
chroma leaves of the 17x15 fixture reconstruct sample-exactly through tell 696.

Connect tile traversal, live references, reconstruction planes, and per-4x4
reconstruction-readiness metadata for a complete coded frame. Complete every luma and chroma path
reached by the 17x15 odd fixture, including coded-edge extension and cropping.
Keep reconstruction private and restoration fail-closed.

Exit gate: the odd fixture reconstructs sample-exact unfiltered Y, U, and V
planes with exact entropy endpoints, coded-plane bounds, and no
fixture-supplied contexts or references.

## Milestone 7: ordinary fixture coverage

**Status:** Complete. The ordinary 160x120 fixture traverses coded-size Y/U/V
planes to `AV2H_TILE_DECODE_DONE` at tell 6498 with exact pinned-AVM prefilter
and final crop parity. Prefilter FNV-1a hashes are Y `7945904097577017502`, U
`4124092689509881817`, and V `6418349723304023184`; the final I420 SHA-256 is
`def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5`.
See `docs/milestone-7.md` for the locked checkpoints.

The following artifact-reach audit records the Milestone 7 completion
checkpoint. Its per-stream frontiers are historical; current end-to-end corpus
results live in `images/artifacts/status.md`.

An audit of all 21 `images/artifacts/*.obu` streams found no artifact-relevant
Milestone 7 gap. Every Frame Header now parses to the exact pinned-AVM bit
endpoint, including multi-block GDF mode and picture parameters. Six streams
select GDF mode 2; they now initialize the tile decoder and decode their first
block flag at the exact pinned-AVM endpoint. The first five now also decode and
store their first multi-strength CDEF index exactly. All six then decode their
first enabled Y/U/V CCSO controls on the normative 256x256 unit grid. The
decoder now continues through their exact mode syntax: emerald reaches tell
17, flycatcher 25, laarim 20, pastell 25, schwaebisch 32, and silene 9.
Large-block FSC context 6 is treated normatively as implicit zero, and
silene's reached palette flag is decoded as zero. Non-mode-2 multi-strength
CDEF and CCSO are also decoded for chalk, chinnathurai, cuckoo, louis, and the
smaller silene stream. Of the other streams, four complete their exact first
coefficient block through Milestone 8 and the remaining frontiers are
fail-closed at transform-size, EOB, coefficient, prediction, or reconstruction
paths. The ordinary 4x4 intraBC leaf is implemented, but none of the artifact
streams reaches it.

Exit gate complete: the ordinary fixture reconstructs sample-exact unfiltered
prefilter and final cropped Y, U, and V planes, and full traversal terminates
at the pinned AVM entropy endpoint without unsupported paths.

## Milestone 8: high-detail and advanced prediction

**Status:** Complete. The locked 96x80 high-detail fixture has complete
frame-wide traversal and unfiltered-plane parity coverage under the hosted
sanitizer and strict freestanding dependency gates.

### Implemented coverage

- all four coefficient Q contexts, complete ordinary luma and chroma
  prefix/level/DC-sign banks reached by the corpus, normative EOB extensions,
  dequantization, saturation, and descriptor-based coefficient geometry;
- adjusted TX_32X64, TX_64X64, and TX_64X16 coefficient geometry, transform
  syntax, large all-zero blocks, and the reached large-transform inverse paths;
- ordinary and FSC transform selection, broad secondary-transform syntax and
  a growing set of sample-locked inverse-secondary kernels;
- directional, cardinal, Smooth, Paeth, IBP, MRL, multi-line MRL, palette,
  CFL, CCTX, MHCCP, and coded-edge prediction paths reached by the corpus;
- IntraBC shell/DRL/precision syntax, persistent reference-vector bank,
  morph prediction, skipped copies, bilinear reconstruction, and readiness
  checks over the reached legal geometries; and
- complete chroma prefix, coefficient-level, primary-transform, CCTX, and
  cross-chroma-transform paths reached by current production traversal; and
- complete locked high-detail traversal with 347 luma and 9 chroma handoffs,
  final entropy tell 36,736, full readiness maps, and AVM-matched unfiltered
  Y/U/V hashes. Production I420 SHA-256 is
  `4120930c9c9c49f73d4aa3e1d4c56cbb4b557679b42fe3b9de984bca1431745a`.

At Milestone 8 closure, the production decoder emitted 7 of the 21 research
artifacts. This historical checkpoint is retained to explain the closure work;
the current 21-artifact production snapshot is maintained in
`images/artifacts/status.md`.

### Completed Milestone 8 closure

1. Chimp Q64 and Cuckoo Q160 prefilter/final reconstruction are byte-identical
  to pinned AVM. Chimp Q64 uses the normative zero-direction rule for chroma
  CDEF with zero primary strength; Cuckoo Q160 extends filter-2 CfL references
  over coded-plane padding.
2. Ordinary 8x8-class inverse-secondary transforms use bounded set/type kernel
  dispatch rather than mode/geometry tuple admission. Only transforms with
  geometry-specific output-position maps retain explicit routing; absent
  kernels remain fail-closed.

The former nine-stream inverse-transform item was completed. Hash-locked
normative small and large IST kernels now advance every named stream beyond
its previous transform endpoint. At that checkpoint Chimp Q192 completed with
an upstream entropy parity gap, while Laarim Q96 failed closed later.

The former five-stream upstream-divergence item is also complete. Resetting
the IntraBC reference bank before each superblock row, while preserving it
across superblocks within that row, matches AVM's lifecycle and advances
Emerald Q128, Pastell Q64, Silene Q176, Louis Q144, and Silene Q80 beyond their
mode, skip, EOB, palette, and coefficient-level checkpoints without weakening
fail-closed syntax checks.

### Exit gate

Milestone 8 is complete: the locked 96x80 high-detail fixture traverses to
`AV2H_TILE_DECODE_DONE` and its complete unfiltered Y, U, and V planes are
sample-exact against pinned AVM under hosted sanitizers and the strict
freestanding dependency gate. Every reached CFL and IntraBC block must be
covered without fixture-supplied contexts or references. Mandatory frame
filters are completed in Milestone 9; the public decode API remains Milestone
10 work.

## Milestone 9: mandatory filters and filtered parity

**Status:** Complete.

The implementation chronology below is retained as Milestone 9 evidence.
Intermediate statements about blocked traversal describe the state before the
completed integration and exit gate later in this section.

Completed preparation corrects normative CDEF signed rounding, validates
CDEF/CCSO parser-produced state and Sequence Header consistency before
mutation, rejects overlap among all GDF stage storage, and freezes
focused failure and nonmutation vectors. A private checked workspace model,
synthetic complete filter-pipeline vector, deblock/CCSO/GDF boundary vectors,
and hosted stage-evidence recorder/comparator are also implemented. That
isolated filter preparation is confined to the filter implementation/header,
focused C/Python tests,
`tools/run_m9_filter_evidence.py`, and Milestone 9 documentation; see the
milestone document for the exact handoff and remaining dependencies.
Frame-header GDF signaling is also complete for the artifact profile: the
parser derives the normative 64/128-pixel GDF block count, conditionally reads
the mode-2 bit, and matches all 21 AVM header endpoints. Mode-2 tile entropy
now uses the pinned adaptive binary CDF and caller-backed row-major flag
storage. All six mode-2 artifacts decode their first GDF flag at the exact AVM
tell. Complete frame-wide flag decoding remains blocked by the tile
traversal's single 64x64 root; a decoded-count guard prevents transition to
chroma while expected flags remain unread. A separate caller-backed CDEF
entropy state now implements all 2-through-8-strength CDFs, the index-zero
split, nonzero remainder symbols, skip gating, and 64x64 unit storage. The five
multi-strength mode-2 artifacts match their first AVM CDEF index and endpoint;
the following caller-backed CCSO entropy state uses the pinned four-context
CDFs and the Sequence Header's 64/128/256-pixel unit geometry. All six streams
match their first enabled plane controls and exact post-CCSO endpoint. A
read-once sentinel protects each plane/unit entry, and an all-units guard
prevents chroma transition while controls remain unread. The filter workspace
now derives the same parsed unit size instead of assuming 64x64 controls.
The same filter entropy path now runs for non-mode-2 frames. Large FSC block
context 6 is an implicit-zero sentinel rather than a CDF row, and the reached
screen-content path decodes Palette=0 with the pinned adaptive CDF while
retaining fail-closed behavior for real palettes.

The private frame-output owner populates complete deblock metadata from every
committed transform, binds parsed CDEF and CCSO unit controls, executes
deblock, CDEF, CCSO, and GDF in normative order over coded planes, and crops
only after filtering. Restoration remains outside the accepted profile and
fails before output mutation.

Exit gate complete: all four locked fixtures match pinned AVM sample-exactly
on final filtered Y, U, and V planes. `tools/run_m9_final_parity.py` runs each
locked OBU through the production decoder and pins final file and plane hashes;
`tools/run_m9_filter_evidence.py` retains stage-local ownership for any future
deblock, CDEF, CCSO, or GDF mismatch. `make milestone-9` runs fixture parity,
focused filter and evidence tests, and the strict freestanding dependency
gate.

## Milestone 10: bounded public decode surface

**Status:** Complete. The public query reports exact workspace size/alignment
and contiguous I420 layouts; decode verifies immutable query binding, caller
bounds, alignment, and disjoint ranges before entering the private pipeline.
The four canonical fixtures pass complete output hashes through canonical HEIF,
and the ordinary fixture covers stale state, overlap, undersized buffers, and
every shorter input prefix without preflight output mutation. See
`docs/dev/milestone-10.md`.

Structural preparation for this milestone is tracked in
`docs/decoder-refactoring-plan.md`.

Finalize workspace size and alignment calculations, query-to-decode input
binding, caller-owned output layouts, truncation behavior, error ownership,
fuzz limits, release documentation, and the public decode API. Do not expose
the API before Milestone 9 parity is complete.

Exit gate: public decoding of all four fixtures is sample-exact across allowed
workspace alignments; undersized workspace, stale input, malformed syntax,
and truncation fail deterministically; release artifacts remain CC0,
freestanding, allocator-free, nolibc, and free of vendored dependencies.

## Milestone 11: 8-bit monochrome

**Status:** Complete.

Add the AV2 4:0:0 pixel model while retaining the current 8-bit sample type.
This is the smallest pixel-model expansion, but it is not only an output-loop
change. The Sequence Header's monochrome value suppresses chroma-dependent
syntax, including SDP, CfL downsample selection, chroma transform tools, and
UV quantization fields. Parsing must therefore derive monochrome from the
normative chroma-format value before reading those fields.

Introduce explicit plane presence independently of the maximum three-plane
storage used by internal arrays. Generalize canonical HEIF `pixi` validation,
`Av2hInfo` plane layouts, output sizing, workspace allocation, tile traversal,
reconstruction handoff, and filter iteration so an absent U or V plane is
neither parsed nor allocated nor exposed as initialized output. Keep byte
strides and all luma arithmetic unchanged. The public API must define unused
plane layouts canonically rather than leaving caller-visible values
indeterminate.

Expected decoder impact: low to medium. The main owners are Sequence Header
and frame parsing, container `pixi` validation/writing, query and workspace
layout, fixed three-plane loops in tile/output/filter orchestration, and the
Windows output adapter. Prediction, inverse transforms, and luma filter
kernels should require little or no arithmetic change. This milestone should
establish reusable plane-presence helpers, not replace every fixed-size
three-element internal array.

Non-goals: 4:2:2, 4:4:4, high-bit-depth samples, RGB conversion, and changes to
the accepted still-picture or coding-tool profile.

Exit gate: canonical monochrome HEIF items with tiny, odd, ordinary, and
high-detail pictures decode sample-exactly against pinned AVM luma; `pixi`,
query layouts, exact workspace bounds, truncation, and absent-plane behavior
have focused tests; malformed chroma syntax fails before output mutation; and
all existing 8-bit 4:2:0 artifacts retain byte-identical output.

Implemented in the public decoder with one active Y plane, canonical zero U/V
layouts, one-channel 8-bit `pixi`, active-plane workspace/filter ownership,
and luma-only tile traversal across multiple superblocks. The four generated
fixtures and their pinned AVM hashes are recorded in
`tests/fixtures/monochrome-manifest.json`; see `docs/dev/milestone-11.md`.

## Milestone 12: 8-bit 4:4:4

**Status:** Complete.

Add three-plane 8-bit 4:4:4 after plane presence is explicit. Accept only the
normative AV2 profile and chroma-format combinations required for this slice.
Replace implicit half-width/half-height chroma calculations with one shared,
checked plane-geometry model used by parsing, block traversal, reconstruction,
filters, query, workspace sizing, and output layout.

Audit every luma-to-chroma coordinate conversion rather than treating larger
output planes as sufficient. The affected normative paths include chroma
partition and transform geometry, neighbor availability, directional
prediction, CfL's full-resolution luma mapping, MHCCP, CCTX, deblock metadata,
and CDEF, CCSO, and GDF plane traversal. Rename private I420-specific entry
points where their contract becomes format-neutral. Canonical HEIF `pixi` and
the public plane layouts must report three full-resolution byte planes.

Expected decoder impact: medium to high. Storage remains `uint8_t`, so core
sample arithmetic and most transform kernels can remain intact, but geometry
is pervasive across frame workspace, tile/leaf/transform ownership, chroma
prediction, reconstruction, and all filter stages. The platform viewer and
development parity tools also need format-aware output handling. This
milestone should centralize subsampling and coordinate helpers so no decoder
owner independently re-derives 4:2:0 or 4:4:4 dimensions.

Non-goals: 4:2:2, monochrome changes beyond Milestone 11, 10-bit storage,
color conversion in the production core, and newly enabled coding tools.

Exit gate: canonical 8-bit 4:4:4 fixtures covering odd dimensions, large
chroma blocks, CfL/MHCCP/CCTX, and each mandatory filter match pinned AVM on
all three full-resolution planes; checked output/workspace calculations and
geometry boundary tests pass; and both monochrome and 8-bit 4:2:0 corpora
remain sample-exact.

Implemented with shared checked plane geometry, exact profile-4/I444
admission, full-resolution transform/reconstruction/filter traversal, and
format-aware Windows and WebAssembly presentation. Tiny, odd, ordinary, and
high-detail fixtures match pinned AVM on all three planes; see
`docs/dev/milestone-12.md`.

### Production-scale 4:4:4 artifact coverage

Large shared coding nodes use transform-major Y/U/V syntax order with bounded
per-transform chroma ownership. V skip contexts use nominal coding geometry
and current-U EOB state for I444 while preserving the sample-exact I420
contract. Directional chroma prediction observes AVM's 64-sample top-right and
bottom-left limits, including partial frame edges. Full-resolution deblock,
CDEF, CCSO, and GDF traversal remains enabled.

Every OBU generated by `tools/generate_8bit_444_artifacts.py` at its original
maximum dimension of 800 pixels, including Palace 800x800, decodes through the
public canonical-HEIF, freestanding Windows, and WebAssembly paths and matches
pinned AVM byte-for-byte on Y, U, and V. The maximum-size monochrome corpus and
Milestone 9 I420 matrix remain exact. Reproducible commands and focused edge
regressions are documented in `docs/dev/milestone-12.md`.

## Milestone 13: 10-bit 4:2:0

**Status:** Planned.

Add 10-bit 4:2:0 as the sample-width milestone while retaining the established
4:2:0 geometry. Use 16-bit containers for public and internal 10-bit samples,
with byte-based strides, offsets, alignments, and sizes stated explicitly.
Define the public byte order and packing contract; do not describe a 16-bit
planar result as ordinary I420.

Make bit depth an explicit input to prediction, inverse-transform,
reconstruction, and filter arithmetic. Audit coefficient dequantization and
clamps, intermediate widths, rounding, sample clipping, default predictors,
CfL/MHCCP domains, deblock thresholds, and CDEF/CCSO/GDF calculations against
the pinned AVM implementation. Widen predictor, reference, reconstruction,
filter-plane, and frame-output storage deliberately; keep entropy and syntax
state narrow where bit depth does not affect their numeric domain. Query and
workspace calculations must multiply sample-backed regions by the checked
bytes-per-sample value without inflating unrelated maps and coefficient state.

Expected decoder impact: high and architectural. The principal owners are the
public plane contract, query/workspace allocation, reconstruction structures,
luma and chroma prediction, dequantization and inverse transforms, and every
in-loop filter. This reaches more arithmetic code than 4:4:4 even though the
existing chroma geometry is retained. Prefer typed 8-bit and 16-bit accessors
or depth-specialized kernels over untyped `void *` sample access in normative
inner loops.

Non-goals: 12-bit, 4:2:2, 4:4:4, production color conversion, lossy
downconversion in the decoder core, and changes to entropy coding unrelated
to bit depth.

Exit gate: canonical 10-bit 4:2:0 fixtures spanning quantizer and filter
boundaries match pinned AVM sample-for-sample in 16-bit planar output; public
layout, byte order, alignment, overflow, and exact workspace tests pass;
sanitizer and fuzz coverage exercise both sample widths; and every 8-bit
profile remains byte-identical.

## Milestone 14: 10-bit 4:4:4

**Status:** Planned.

Combine the generalized 4:4:4 geometry from Milestone 12 with the 10-bit
sample domain from Milestone 13. This is an independent integration milestone,
not an inference that two separately tested axes compose correctly. Accept
only the normative profile/signaling combination and preserve fail-closed
behavior for 4:2:2, 12-bit, and unsupported tools.

Expected decoder impact after Milestones 12 and 13: low to medium production
code, concentrated where geometry and sample width meet. Re-audit chroma
prediction references, CfL/MHCCP/CCTX intermediates, full-resolution filter
grids, plane byte strides, maximum workspace calculations, and platform/tool
output. Any substantial format-specific branch found here should be folded
back into the shared geometry or sample-width abstraction rather than forming
a fourth decoder path.

Non-goals: new coding tools, 4:2:2, 12-bit, RGB output, metadata, alpha, and
grid items.

Exit gate: independent tiny, odd, ordinary, and high-detail 10-bit 4:4:4
fixtures, including chroma-prediction and filter stress cases, match pinned
AVM on all planes; maximum-size output/workspace arithmetic remains bounded;
and the complete monochrome, 4:2:0, 4:4:4, 8-bit, and 10-bit regression matrix
passes through both private fixture and public canonical-HEIF paths.

## Later work

After Milestone 14, add one independently testable slice at a time: 4:2:2,
color conversion, streaming PNG, transformations, metadata, alpha auxiliary
items, grids, and bounded parallelism. Each slice needs strict syntax tests,
reference plane equality, workspace bounds, fuzz coverage, and updated support
documentation.

Compression studies begin only after decoder correctness and fixture
reproducibility are stable. They must pin encoder configurations, distinguish
codec payload from total container size, publish machine-readable results, and
state which AV2 video features are irrelevant to still images.

## Required validation

Every production change should run the narrowest relevant self-contained test.
Release validation additionally includes:

- strict freestanding compilation and final ELF/Mach-O dependency inspection;
- hosted ASan and UBSan runs over the same core;
- deterministic golden and round-trip tests;
- malformed container, OBU, and sequence-header suites;
- fuzzing with low resource limits and minimized regression inputs;
- differential plane comparison with pinned AVM;
- comparison through other independent tools when they support the artifact;
- Linux/x86-64 and macOS/arm64 coverage;
- verification that vendored source and build products are absent from CC0
  release archives and link graphs.

## Current work order

1. Milestone 11: add 8-bit monochrome and explicit plane presence.
2. Milestone 12: generalize chroma geometry for 8-bit 4:4:4.
3. Milestone 13: widen sample storage and arithmetic for 10-bit 4:2:0.
4. Milestone 14: validate the combined 10-bit 4:4:4 model independently.

Early work in a later milestone may be retained when it is already validated,
but it does not waive an earlier exit gate. Each milestone must leave the
decoder fail-closed at the first unsupported owner.

Milestones 7 through 10 are complete. Milestones 11 through 14 are planned as
independent pixel-model expansions. The private Windows decoder and public
canonical-HEIF path retain byte-identical pinned-AVM output for their locked
corpora. Restoration remains outside the accepted profile and must continue to
fail closed.