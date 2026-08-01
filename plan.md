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

**Status:** In progress. Q-aware first-block coefficient parsing is pinned for
four artifact streams.

The decoder now derives the four coefficient Q contexts, initializes the exact
Q2/Q3 prefix, level, and DC-sign CDF rows reached by the artifact matrix, and
rejects every unpinned Q2/Q3 row before entropy consumption. Normative 8-bit
QTX lookup, luma DC delta, large-transform scaling, and coefficient saturation
are implemented. The reached 16x32 large-transform orientation and secondary
CDF are also pinned. TX_32X64, TX_64X64, and TX_64X16 now derive their
normative IDs, adjusted 32x32/32x16 coefficient geometries, EOB capacities,
and entropy contexts. 32x64/64x64 use implicit DCT_DCT; 64x16 reads the
normative short-side transform symbol with its 64-pixel side fixed to DCT.
The normative two-bit extension for 512/1024-class EOB raw symbol 7 also
advances cuckoo Q80 to exact EOB 87. Emerald Q128 and silene Q176 reach exact
EOB endpoints before adjusted-size IST. Seven streams reach large-transform
coefficient levels after exact transform syntax, and schwaebisch Q208
completes a 64x16 all-zero block before its reconstruction and following
32x16 profile frontier.

First-block coefficient endpoints now match AVM for
`cuckoo-157x105-qp160` (tell 35, DC -6),
`emerald-192x96-qp208` (tell 27, DC -2),
`louis-463x342-qp224` (tell 27, DC +6), and
`pastell-512x341-qp160` (tell 65, DC -8). The artifact test compares the full
quantized and dequantized arrays, not only endpoints. Six GDF-mode-2 streams
now consume exact first-unit GDF, CDEF, and CCSO syntax before their next
unsupported profile path; eleven other streams still stop at their first-luma
Milestone 8 frontier.

Cuckoo Q160 and pastell Q160 now reconstruct and commit those first 16x32
blocks sample-exactly, with pinned full-block hashes `fb97d9776de897f5` and
`73f9aa733e29f1c7` plus first/last-row locks. Cuckoo next reaches `(0,4)`
8x16 at tell 41 and fails before mode syntax on Q160 profile admission.
Pastell next reaches `(0,4)` 16x32 Smooth, EOB 3, DCT_DCT, and secondary
symbol 0 at tell 78, then fails before coefficient-level entropy.

Laarim Q96 now applies the normative two-bypass-bit extension to TX_32X32 raw
EOB point 7, reaching EOB point 8, EOB 126, and implicit DCT_DCT at tell 28.
The sparse normative Q-context-1 TX-context-3 rows decode 126 levels through
tell 314 and 82 coefficients through tell 471. Its D45 predictor and
prefilter hashes are `75290844cee19be7` and `51b573f06c63948a`; traversal
then reaches `(0,8)` 16x16 fail-closed at tell 476.

Emerald Q208 now continues through two adjacent 32x32 luma leaves, the
following `(8,0)` 32x8 DC leaf, and both `(10,*)` 16x16 leaves. `(10,0)`
completes all-zero at tell 52. `(10,4)` uses the normative
`intra_ext_tx_cdf[1][2]` primary row, maps symbol 1 to ADST_ADST at tell 61,
reads STX 0 through tell 65, and decodes coefficients
`+2@0,+1@16,-1@32` through tell 72. Its constant-98 predictor plus real
ADST_ADST inverse transform matches all pinned samples and trace hash
`7959e335`; `(14,0)` 32x8 is the new tell-74 fail-closed frontier.

Schwaebisch Q208 reconstructs its first 64x16 all-zero DC block with adjusted
32x16 coefficient geometry. Predictor and prefilter are constant 128 with
hash `d495a039fba13383`; the block ends at tell 17 and `(4,0)` 32x16 remains
the tell-19 frontier.

Complete the high-detail fixture and the remaining reached high-energy,
secondary-transform, CFL, intraBC, coefficient-range, and large-transform
paths. Preserve bounded sparse tables where they remain sufficient; complete
normative implementations where fixture reach makes sparse admission fragile.

Exit gate: the high-detail fixture reconstructs sample-exact unfiltered Y, U,
and V planes, including every reached CFL and intraBC block, under sanitizers
and the freestanding gate.

## Milestone 9: mandatory filters and filtered parity

**Status:** Planned; isolated filter hardening is implemented, while
fixture-level metadata wiring and filtered parity remain open.

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

Populate complete deblock and CCSO metadata from reconstructed leaves, execute
deblock, CDEF, CCSO, and GDF in normative order over coded planes, crop to
display dimensions, and compare each intermediate stage where the hosted
oracle exposes it. Restoration remains outside the accepted profile and must
fail before output mutation.

Exit gate: all four locked fixtures match pinned AVM sample-exactly on final
filtered Y, U, and V planes, with stage-local hashes identifying any failure.

## Milestone 10: bounded public decode surface

**Status:** Planned.

Finalize workspace size and alignment calculations, query-to-decode input
binding, caller-owned output layouts, truncation behavior, error ownership,
fuzz limits, release documentation, and the public decode API. Do not expose
the API before Milestone 9 parity is complete.

Exit gate: public decoding of all four fixtures is sample-exact across allowed
workspace alignments; undersized workspace, stale input, malformed syntax,
and truncation fail deterministically; release artifacts remain CC0,
freestanding, allocator-free, nolibc, and free of vendored dependencies.

## Later work

Add one independently testable slice at a time: monochrome, 4:4:4, 10-bit,
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

1. Complete Milestone 8 high-detail, CFL, intraBC, coefficient-range, and
  advanced-transform coverage.
2. Integrate and prove the mandatory filter pipeline in Milestone 9 after the
  unfiltered fixture dependencies are complete.
3. Finalize workspace contracts and expose decoding only in Milestone 10.

Early work in a later milestone may be retained when it is already validated,
but it does not waive an earlier exit gate. Each milestone must leave the
decoder fail-closed at the first unsupported owner.

The remaining-work audit is complete: Milestone 7 ordinary unfiltered
luma/chroma work is closed; Milestone 8 owns the high-detail and advanced
prediction/transform profile; Milestone 9 owns metadata wiring and normative
filtered parity; and Milestone 10 owns bounded workspace, public API, fuzz,
and release finalization. Restoration remains outside the accepted profile
and must continue to fail closed.