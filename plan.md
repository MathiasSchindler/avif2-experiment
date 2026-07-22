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

## Milestone 5: first complete decode slice

**Status:** In progress. The independent freestanding entropy decoder, private
Closed Loop Key frame parser, complete partition CDF initialization, and
bounded first-leaf traversal through luma mode, FSC, transform `all_zero`, and
complete EOB pass pinned-AVM oracle checks under sanitizers and the strict
dependency gate. The parser derives all four tile entropy boundaries from
syntax. Full frame traversal, remaining coefficient syntax,
reconstruction, filter execution, workspace sizing, and the public decode API
remain; see `docs/milestone-5.md`.

Implement only the smallest profile that pinned AVM can reliably emit and the
project can validate completely. The initial target is one 8-bit 4:2:0
single-picture, single-layer, closed-random-access stream.

Add entropy decoding, prediction, inverse quantization, transforms,
reconstruction, and every mandatory filter for accepted syntax. Do not accept
a syntax path that can reach an unimplemented reconstruction operation.

Exit gate: every supported fixture reconstructs sample-exactly with pinned AVM
on every plane under hosted sanitizers and freestanding builds, with bounded
workspace behavior across alignments and truncations.

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

## Immediate work order

1. Correct and pin the evidence record in the design report.
2. Choose the permanent repository name, API prefix, and CC0 license files.
3. Create the repository skeleton and dependency/provenance policy.
4. Integrate pinned AVM as the first hosted reference dependency.
5. Complete the Milestone 0 carriage-equivalence experiment.
6. Proceed to the freestanding substrate only after the experiment passes.

The exact AVM one-frame invocation remains deliberately unrecorded until it is
verified against the pinned build and its produced OBU syntax is inspected.