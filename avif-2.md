# AV2 still images in HEIF: experimental project report

**Status:** design and research record, not an implementation specification

**Research snapshot:** 21 July 2026

**Intended destination:** a new sibling repository, provisionally named
`avif-2`, next to `avif-decoder`

For current implementation status, supported formats, and build instructions,
see the [README](README.md). The [active roadmap](plan.md) tracks
current and planned work. The text below preserves the dated design and
research record.

**Scope of this file:** preserve the evidence, assumptions, decisions, wire
format, architecture, and validation plan needed to start that repository
without silently turning guesses into format requirements

## Executive decision

The AV2 experiment should not be implemented in `avif-decoder`.

`avif-decoder` implements the approved AV1 Image File Format and has a clear
contract: dependency-free, freestanding C, no libc, caller-owned memory, and
conformance to the existing AVIF/AV1 specifications. AV2 is a different codec,
and there is no approved AV2 image-item specification or AVIF version that
replaces AV1 with AV2 as of the research snapshot. Adding experimental AV2
syntax to this repository would make its format boundary and conformance claims
ambiguous.

The proposed sibling project has two independent deliverables:

1. A reproducible hosted toolchain that creates, inspects, normalizes, and
   validates experimental AV2 still-image files in HEIF.
2. A freestanding C, `nolibc`, allocation-free decoder in the style of
   `avif-decoder`, but written for AV2 and the explicitly versioned experimental
   container dialect defined by the new project.

The working names **AVIF-AV2**, **AVIF 2**, and **avif-2** are project names,
not standards claims. Files produced by the experiment must not be presented as
conforming AVIF files, served as `image/avif`, or given an unqualified `.avif`
extension unless a future approved specification explicitly permits that.

## How to read this report

The words below have deliberately different meanings:

- **Verified fact** means the cited specification or source code said this on
  21 July 2026.
- **Observed implementation behavior** means current source code implements
  this behavior. It is not automatically a format requirement.
- **Decision** means the proposed sibling project will follow this rule until a
  recorded design change supersedes it.
- **Assumption** means work may proceed on this basis, but a test or normative
  reading must confirm it before the affected feature is complete.
- **Open question** means implementation must not invent an answer. The project
  must resolve it, constrain it out of scope, or preserve it as unsupported.

Normative words such as “shall” and “must” are used in this report only for the
project's own experimental contract. Quotations or summaries of external
normative requirements are identified as such.

## Nomenclature

This report uses the following names to prevent several similar formats from
being conflated:

- **AVIF 1.2**: the approved AV1 Image File Format v1.2.0, published as an AOM
  Final Deliverable on 16 October 2025.
- **AV2**: the AV2 Bitstream & Decoding Process Specification v1.0.0, released
  on 28 May 2026.
- **AV2-ISOBMFF draft**: the AV2 Codec ISO Media File Format Binding Working
  Group Draft dated 15 July 2026. Its current scope is tracks and CMAF, not
  HEIF image items.
- **libavif AVM dialect**: the experimental AV2 item files written and read by
  current libavif when compiled with `AVIF_CODEC_AVM`. This report records the
  behavior observed on libavif `main` on 21 July 2026. It is not an approved
  specification.
- **AV2-HEIF experimental v0**: the project-owned, draft-derived still-image
  item dialect proposed in this report. It is intentionally not called AVIF.
- **elementary AV2 stream**: an AV2 OBU bitstream outside a container.
- **configuration OBUs**: the sequence header and other allowed initialization
  OBUs stored in `av2C` by the AV2-ISOBMFF draft.
- **item payload**: the bytes selected by the HEIF item's `iloc` extents.

## Evidence ledger

The following table is the minimum factual basis for this project. A future
implementation should update the snapshot and pin exact source commits before
changing any conclusion derived from it.

| ID | Status | Statement | Consequence |
| --- | --- | --- | --- |
| F1 | Verified fact | AV2 v1.0.0 is the current release, dated 28 May 2026, and AVM v1.0.0 is its corresponding reference software. | Pin AV2 v1.0.0 and AVM v1.0.0 for the first reproducible baseline. Do not implement against an older AV2 working draft. |
| F2 | Verified fact | AVIF v1.2.0 stores a subset of AV1 syntax in HEIF. Its coded image item type is `av01`, its mandatory codec property is `av1C`, and its `avif`, `avis`, `avio`, `MA1A`, and `MA1B` brands carry AV1-specific promises. | Replacing AV1 bytes with AV2 bytes does not create a conforming AVIF file. Do not reuse AV1-specific brands for new experimental files. |
| F3 | Verified fact | The AV2-ISOBMFF document available on 21 July is a Working Group Draft dated 15 July 2026 and warns that it should not be referenced as anything other than a working draft. | Treat it as volatile input. Record its revision in every generated fixture manifest. |
| F4 | Verified fact | The current AV2-ISOBMFF draft specifies AV2 tracks and CMAF files. It does not define an AV2 HEIF image item. | Any `av02` image item is an experimental extension, even if it borrows track syntax exactly. |
| F5 | Verified fact | The draft defines an `av02` compatible brand, an `av02` `VisualSampleEntry`, and an `av2C` `AV2CodecConfigurationBox` for tracks. | These names provide strong prior art, but their use for an image item must be labeled project-defined. |
| F6 | Verified fact | The draft `av2C` begins with a nine-byte fixed prefix and then contains length-delimited `configOBUs`. The first configuration OBU must be a Sequence Header OBU. | The new project should implement this shape rather than the four-byte AV1 configuration record. |
| F7 | Verified fact | Draft track samples contain exactly one temporal unit. OBU boundaries use Annex B length-delimited framing. Samples exclude Temporal Delimiter, Padding, Sequence Header, and Layer Configuration Record OBUs. | The experimental item payload can deliberately adopt the same self-delimiting sample format. |
| F8 | Verified fact | A draft AV2 sync sample begins each coded extended-layer unit with `OBU_CLOSED_LOOP_KEY` and must be self-sufficient when combined with the active `av2C`. | The first experimental still-image profile should require the corresponding closed-random-access property. |
| F9 | Observed implementation behavior | Current libavif exposes `AVIF_CODEC_CHOICE_AVM` as experimental AV2 support, requires explicit AVM selection for encoding, and automatically selects AVM when decoding an `av02` item. | libavif plus AVM is a useful independent oracle and fixture source. It is not the normative definition of the new format. |
| F10 | Observed implementation behavior | Current libavif writes image item type `av02` and property `av2C`, but serializes `av2C` with the four-byte AV1 codec-configuration layout. It harvests those fields from an AV2 sequence header in the encoded item payload. | The libavif AVM dialect and the draft `av2C` dialect are wire-incompatible and must have separate parsers and tests. |
| F11 | Observed implementation behavior | Current libavif writes the `avif` major brand, minor version 2 for AV2, and can also write AV1-specific MIAF profile brands based on image properties. | Such files are valuable test artifacts but should be classified as nonconforming experimental AVIF, not copied as the new project's canonical format. |
| F12 | Observed implementation behavior | libavif's local AVM integration pins AVM v1.0.0, uses `g_limit = 1` for a single image, and states that this causes AV2 still/single-picture header signaling. Its encoder currently rejects depths other than 8 and 10. | Use the same pinned reference version and begin with 8-bit input. Treat 10-bit as the next profile and 12-bit as unsupported until both encoder and decoder evidence exist. |
| F13 | Observed implementation behavior | `avmenc` can write raw `.obu` output, while AVM's library encoder returns length-delimited coded frame packets. | The fixture builder can begin from `.obu` or directly from the AVM API, then normalize OBUs before muxing. |
| F14 | Observed implementation behavior | AOMedia's `av2-tools` project parses AV2 OBUs and optionally builds `av2_mux`/`av2_demux` for track-based ISOBMFF. It is C++17 and has substantial hosted dependencies. | Use it as a reference and differential tool only. It is not suitable as production code for the freestanding decoder. |

## Primary sources

The initial project must pin immutable copies or commit identifiers for every
source used to define a fixture. These URLs identify the sources reviewed for
this report:

- [AV2 v1.0.0 specification landing page](https://av2.aomedia.org/)
- [AV2 v1.0.0 online specification](https://av2.aomedia.org/v1.0.0/index.html)
- [AVM v1.0.0 reference software](https://github.com/AOMediaCodec/avm/tree/v1.0.0)
- [AV2-ISOBMFF Working Group Draft](https://aomediacodec.github.io/av2-isobmff/main/index.html)
- [AV2-ISOBMFF v1.0.0 draft snapshot](https://aomediacodec.github.io/av2-isobmff/v1.0.0.html)
- [AVIF v1.2.0](https://aomediacodec.github.io/av1-avif/v1.2.0.html)
- [libavif](https://github.com/AOMediaCodec/libavif)
- [AV2 Tools](https://github.com/AOMediaCodec/av2-tools)

The words “v1.0.0” have different maturity in those sources: AV2 v1.0.0 is a
released codec specification, while the AV2-ISOBMFF page carrying a v1.0.0 URL
still identifies itself as a Working Group Draft. Version numbers alone must
not be used as evidence of approval status.

## Why this does not belong in `avif-decoder`

The existing repository's architecture and documentation establish all of the
following:

- it is an AVIF/AV1 implementation;
- it aims to accept approved AVIF still-image and sequence syntax;
- it writes a deliberately narrow but interoperable AVIF/AV1 profile;
- its production core is freestanding C11, allocation-free, and I/O-free;
- its CLI is `nolibc` and uses direct platform services;
- its source is dedicated under CC0;
- external codec implementations are optional reference-test dependencies, not
  production dependencies.

The AV2 experiment conflicts with the first three points until there is an
approved AV2 image format. Keeping it in a sibling repository has practical
benefits beyond naming:

- AVIF conformance tests cannot accidentally accept project-private syntax.
- AV2 tables, syntax, profiles, and decoder state do not complicate AV1 code.
- The experimental container can change incompatibly while its draft evolves.
- The new project can make an explicit licensing decision for code informed by
  AVM without changing this repository's CC0 claim.
- Release notes for `avif-decoder` can continue to mean AVIF/AV1 support.
- Security limits, fuzz corpora, and compatibility policy can evolve
  independently.

The sibling project may copy suitable CC0 platform, buffer, PNG, color, test,
and build substrate from `avif-decoder`, with provenance recorded at the time of
the copy. It should not share source files by relative paths, a submodule, or a
build-time dependency. Independence is part of the boundary.

## External format state on 21 July 2026

### AVIF 1.2 is explicitly AV1

AVIF v1.2.0 defines storage of AV1 images in HEIF. A coded image item has type
`av01`; it has exactly one associated `av1C`; and its item data is an AV1 sync
sample containing exactly one Sequence Header OBU. AVIF additionally defines
AV1-specific brands and profiles.

This matters because container similarity is not codec substitution. HEIF is
generic enough to carry newly defined coded-image item formats, but AVIF's
brands and media type are not generic HEIF declarations. They promise the
AV1-specific rules of the AVIF specification.

**Decision D1:** New project-owned fixtures shall not contain `avif`, `avis`,
`avio`, `MA1A`, or `MA1B` in `ftyp`.

**Decision D2:** New project-owned fixtures shall not be described as
`image/avif` and shall use `.av2.heif` in the repository. The compound suffix
is a test-project convention, not a registered extension.

### The current AV2 binding is track-only

The 15 July AV2-ISOBMFF draft defines a track sample entry and sample format.
It says a conforming file contains at least one track using an
`AV2SampleEntry`. It does not define an `infe` item type, item property
semantics, a still-image brand, an image media type, or an AV2 equivalent of the
AVIF image-item clauses.

Therefore the following inference would be invalid:

> The track binding defines `av02`, so an `av02` HEIF image item is already
> standardized.

It is not standardized by the reviewed draft. The proposed item syntax is a
controlled experiment that borrows the track initialization and sample format.

**Decision D3:** Every project document and diagnostic shall call the canonical
format “AV2-HEIF experimental v0,” not “AVIF v2.” User-facing tools may mention
“AVIF-AV2 experiment” as a project description only.

### The current AV2 binding is incomplete

The draft contains `TBD` text for `av2C` semantics, CMAF, Common Encryption,
and the codecs parameter string. The shape of `av2C` and the OBU placement
rules are concrete enough for an experiment, but the project must expect
changes.

**Decision D4:** Generated fixtures shall carry a sidecar manifest containing
the exact AV2-ISOBMFF source revision. A fixture is never identified only as
“v1.0.0.”

**Decision D5:** No encrypted, fragmented, CMAF, multi-sample, or codecs-string
behavior is in scope for experimental v0.

## Existing libavif/AVM experiment

Current libavif has already demonstrated that its HEIF item graph and AVM can
be joined:

- encoder choice `AVIF_CODEC_CHOICE_AVM` produces AV2;
- the writer changes coded item type from `av01` to `av02`;
- the writer changes configuration property name from `av1C` to `av2C`;
- the reader recognizes `av02`, locates `av2C`, and selects AVM;
- ordinary HEIF machinery continues to carry `ispe`, `pixi`, `colr`, alpha,
  grids, metadata, and transformations;
- AVM receives a single-image configuration and returns a coded packet;
- libavif's AVM tests encode, decode, and compare reconstruction quality.

This is strong evidence that the experiment is technically viable. It is not
evidence that the resulting files conform to AVIF or to the new AV2-ISOBMFF
draft.

The most important incompatibility is `av2C`:

| Property | libavif AVM dialect | AV2-ISOBMFF draft |
| --- | --- | --- |
| Fixed header | Four-byte AV1-shaped configuration record | Nine-byte AV2-specific prefix |
| First byte | AV1 marker/version bit packing, normally `0x81` | `configurationVersion`, currently `0x01` |
| Sequence Header OBU | Remains in coded item payload | First OBU in `configOBUs` |
| Coded payload | AVM packet including initialization OBUs | Sample excludes Sequence Header and Layer Configuration Record OBUs |
| Brands in observed writer | `avif` major brand, minor version 2, plus generic and sometimes AV1 profile brands | Draft requires `av02` compatible brand for track files; no image-item brands are defined |

**Decision D6:** The new project shall preserve libavif AVM files as a named
compatibility dialect, `libavif-avm-2026-07`, and shall never parse their
four-byte `av2C` as if it were the draft box.

**Decision D7:** The canonical writer shall not copy libavif's branding or
four-byte `av2C`. A separate importer may convert a libavif AVM file into the
canonical experimental dialect by parsing and normalizing its OBUs.

**Decision D8:** Strict decoding shall reject the libavif dialect by default.
An explicit compatibility flag may enable it. Inspection shall always identify
which dialect was detected.

## Proposed canonical format: AV2-HEIF experimental v0

### Purpose

Experimental v0 is designed to answer three questions, not to preempt a future
standard:

1. Can an independently decodable AV2 still picture be represented cleanly as
   a HEIF coded image item?
2. Can a small deterministic muxer create such files from AVM output?
3. Can a freestanding decoder parse the container and decode the AV2 picture
   without libc or codec dependencies?

It intentionally adopts as much of the current AV2 track binding as can be
applied unambiguously: `av2C` field syntax, `configOBUs`, length-delimited OBU
framing, one temporal unit, forbidden sample OBUs, and closed-random-access
requirements. Everything specific to image items is project-defined.

### File identification

The initial canonical file has:

- major brand `mif1`;
- minor version `0`;
- compatible brand `mif1`;
- no AVIF, AV1 profile, AV2 track, MIAF, or invented codec brand.

`mif1` is used only as a generic HEIF structural declaration. The project does
not add `miaf` until the complete file has been checked against the applicable
MIAF requirements. It does not add the draft's `av02` compatible brand because
the reviewed draft says that brand identifies a file containing an AV2 track;
experimental v0 contains no track.

This conservative branding means generic HEIF tools may inspect the container
but will not be told that they can decode the unknown coded item. The coded item
and its essential `av2C` property provide local format identification.

**Open question Q1:** A future AV2 image-item specification may assign a brand,
media type, item property, or item type different from this experiment. The
project shall add a new dialect rather than silently changing v0 fixtures.

### Required top-level structure

The canonical single-image file is:

```text
ftyp
meta (FullBox version 0)
  hdlr (handler_type = 'pict')
  pitm (primary_item_ID = 1)
  iloc
  iinf
    infe (item_ID = 1, item_type = 'av02')
  iprp
    ipco
      av2C
      ispe
      pixi
      colr ('nclx')
    ipma
mdat
  AV2 item payload
```

The order above is canonical for deterministic writing. A decoder may accept
other orders permitted by the underlying ISOBMFF and HEIF rules, but must still
enforce uniqueness, bounds, required associations, and descriptive-property
ordering where applicable.

### Exact v0 container choices

The deterministic writer uses these choices:

| Structure | v0 choice |
| --- | --- |
| `meta` | FullBox version 0, flags 0 |
| `hdlr` | First child of `meta`; version 0, flags 0, handler `pict` |
| Item ID | 1 |
| `pitm` | Version 0 with 16-bit item ID |
| `iloc` | Version 0; 32-bit extent offset and length; zero base offset; one extent; data reference index 0 |
| `iinf` | Version 0; one entry |
| `infe` | Version 2; protection index 0; item type `av02`; NUL-terminated item name `AV2` |
| `ipco` property order | `av2C`, `ispe`, `pixi`, `colr` |
| `ipma` | Version 0, flags 0, one item entry, 7-bit property indices |
| Essential association | `av2C` is essential; the other three v0 properties are not marked essential |
| Payload storage | One extent pointing to bytes immediately after the `mdat` box header |
| Large sizes | Not written by v0; the writer rejects files whose chosen 32-bit fields cannot represent offsets or lengths |

The decoder should eventually parse wider legal ISOBMFF forms, but the first
writer must stay deliberately narrow so every byte is covered by a test.

### Project-defined `av02` image item semantics

For experimental v0, an item with `item_type == 'av02'` is processable only if
all of these conditions hold:

- it has exactly one associated `av2C` property;
- `av2C` is marked essential;
- it has exactly one associated `ispe` property;
- it has exactly one associated `pixi` property;
- it has exactly one associated `colr` property with `colour_type == 'nclx'`;
- its payload is in the AV2 sample format defined below;
- the effective configuration and payload produce exactly one output picture;
- decoding is self-contained and requires no prior item, sample, or decoder
  state;
- the decoded picture dimensions, channel count, bit depth, chroma format,
  range, and color description agree with the associated properties.

These are project requirements. They are not attributed to the current
track-only draft as image-item requirements.

### `av2C` syntax adopted by v0

Experimental v0 copies the fixed field order from the 15 July 2026
AV2-ISOBMFF draft:

```text
configurationVersion                         8 bits, value 1
seq_profile_idc                              5 bits
seq_level_idx                                5 bits
seq_tier                                     1 bit
chroma_format_idc                            3 bits
bit_depth_idc                                3 bits
monotonic_output_order_flag                  1 bit
still_picture                                1 bit
film_grain_params_present                    1 bit
seq_initial_display_delay_present_flag       1 bit
seq_initial_display_delay_minus_1             4 bits
reserved1                                     7 bits, value 0
max_frame_width_minus_1                     16 bits
max_frame_height_minus_1                    16 bits
configOBUs                                  remaining bytes
```

The fixed prefix is nine bytes: one version byte, four packed bytes, two width
bytes, and two height bytes. Integer fields are written in network byte order
where they occupy complete bytes. The bit-level packing follows the listed
order from most significant bit to least significant bit.

Every `configOBUs` entry uses AV2 Annex B length-delimited framing:

```text
leb128(num_bytes_in_obu)
obu_header_and_payload[num_bytes_in_obu]
```

For v0:

- the first configuration OBU is exactly one Sequence Header OBU;
- duplicate Sequence Header OBUs are rejected;
- Temporal Delimiter, Padding, and coded-frame OBUs are forbidden in
  `configOBUs`;
- static initialization OBUs may be added only after the decoder implements
  and validates their ordering and persistence semantics;
- every fixed `av2C` field must equal the effective value derived from its
  configuration OBUs;
- nonzero reserved bits are rejected;
- trailing malformed or unparsed configuration bytes are rejected.

**Decision D9:** The v0 writer emits only the Sequence Header OBU in
`configOBUs`. Layer configuration, static metadata, content interpretation,
film grain, and other high-level syntax remain unsupported until added as
separately tested features.

### Item payload format

The v0 item payload copies the draft AV2 sample format even though the payload
is an item, not a track sample:

- it contains exactly one temporal unit;
- the item boundary delimits that temporal unit;
- every OBU is Annex B length-delimited;
- OBUs retain AV2-specified order;
- no Temporal Delimiter OBU is stored;
- no Padding OBU is stored;
- no Sequence Header OBU is stored;
- no Layer Configuration Record OBU is stored;
- only byte-alignment trailing bits are permitted;
- constant metadata should be moved to `configOBUs` or a container property,
  not duplicated in the item payload.

For the initial single-layer profile, the first and only coded extended-layer
unit must begin with `OBU_CLOSED_LOOP_KEY`. The combined `av2C` configuration
and item payload must be sufficient to initialize a fresh decoder and produce
one output picture.

**Decision D10:** The v0 profile requires effective still-picture signaling and
the AV2 single-picture header form emitted by the pinned AVM single-image path.
Before the writer accepts a fixture, the project's independent sequence-header
parser must verify the exact v1.0.0 syntax values. The project shall not infer
those values merely from the AVM command line.

### `ispe`, `pixi`, and `colr`

The project adopts these cross-checks:

- `ispe.image_width` and `ispe.image_height` equal the rendered dimensions of
  the one output picture in v0;
- `pixi.num_channels` is 1 for monochrome and 3 for YUV color;
- every `pixi.bits_per_channel` equals the effective AV2 bit depth;
- `colr` uses `nclx` and stores color primaries, transfer characteristics,
  matrix coefficients, and full-range flag;
- the `nclx` values equal the effective AV2 content interpretation;
- absence of explicit AV2 color description is represented by the applicable
  AV2 unspecified/default values, not values guessed from filename or source
  image type.

The first writer accepts only values it can derive from parsed AV2 syntax and
explicit source metadata. It never “repairs” a mismatch by changing container
metadata without reporting the mismatch.

### Initial supported profile

The first complete encode/mux/decode fixture profile is intentionally small:

- one primary coded image item;
- one extent;
- one AV2 singlestream;
- one extended layer, one embedded layer, and one temporal layer;
- one closed-loop key picture and exactly one output picture;
- 8-bit YUV 4:2:0;
- no alpha;
- no grid;
- no crop, rotation, mirroring, or pixel-aspect transform;
- no Exif, XMP, ICC, gain map, HDR mastering metadata, or thumbnails;
- no film grain;
- no encryption;
- no tracks, fragments, timing, or animation.

This profile is a project implementation boundary, not an assertion that AV2
or HEIF cannot represent the omitted features.

The first profile extensions, 8-bit monochrome and 8-bit 4:4:4, are
implemented and validated. The remaining planned profiles are, in order:

1. 10-bit 4:2:0 and 4:4:4.
2. Container transforms and metadata preservation.
3. Auxiliary alpha as a separate monochrome coded item.
4. HEIF grid derived images.

12-bit, multiple AV2 layers, progressive decoding, film grain, image
sequences, and gain maps are deferred until reference encoders, normative
rules, and independent tests are adequate.

## Dialect detection and compatibility

The canonical and libavif dialects both use `av02` and `av2C`, so detection
must be structural and fail closed.

### Canonical v0 detection

All of these are required:

- the file is a generic HEIF item file and does not claim an AVIF brand;
- the primary coded item type is `av02`;
- its essential `av2C` payload is at least nine bytes plus a nonempty
  `configOBUs` sequence;
- byte zero of the `av2C` payload is `configurationVersion == 1`;
- the first parsed configuration OBU is a Sequence Header OBU;
- the item payload does not contain a Sequence Header OBU.

### libavif AVM dialect detection

All of these are required before compatibility decoding:

- the file claims the `avif` brand;
- the primary coded item type is `av02`;
- `av2C` has the four-byte AV1-shaped form with marker/version bits;
- the item payload contains the AV2 Sequence Header OBU expected by libavif's
  experimental parser;
- compatibility mode is explicitly enabled.

The inspector reports this dialect as nonconforming experimental AVIF. It must
not normalize the label to AV2-HEIF v0 without actually rewriting the property
and payload.

### Ambiguous or future inputs

If a file mixes indicators, has an unknown `configurationVersion`, claims AVIF
while carrying draft-shaped `av2C`, or places a sequence header in both
`av2C` and the item payload, strict decoding fails with a diagnostic naming the
conflict. There is no heuristic “best effort” mode in the core decoder.

**Decision D11:** New format revisions are additive dialects with independent
golden fixtures. Existing fixture interpretation never changes in place.

## Reproducible test-file creation

### Separation of responsibilities

The test-file pipeline has four stages:

```text
source pixels
  -> pinned AVM encoder
  -> AV2 elementary OBU stream
  -> project OBU normalizer
  -> deterministic HEIF item muxer
  -> AV2-HEIF experimental v0 file + manifest
```

AVM decides AV2 coding syntax. The project normalizer parses and classifies
OBUs. The project muxer writes only container syntax. Keeping these stages
separate allows the container and normalizer to be tested with synthetic OBUs
without invoking the slow reference encoder.

### Reference encoder baseline

The initial baseline is AVM tag `v1.0.0`. The exact Git commit resolved by that
tag, CMake options, compiler identity, target architecture, and encoder options
must be recorded in the fixture manifest.

The planned encoder invocation will:

- read deterministic Y4M or raw planar input;
- encode exactly one frame;
- select AV2;
- write raw length-delimited OBU output rather than IVF or WebM;
- request the single/still-picture path;
- disable film grain and unsupported metadata;
- set explicit profile, bit depth, chroma format, range, quality, speed, and
  thread count;
- run AVM's own encode/decode mismatch check when available.

The exact command line is intentionally not frozen in this report because AVM
option names and defaults must be verified against the pinned binary's
`--help` and an inspected output stream. The future script, not a prose example,
will be the authoritative command. It must reject an AVM version or help output
that differs from the pinned expectation.

### OBU normalizer algorithm

The normalizer is a parser, not a byte-pattern search. It performs these steps:

1. Parse each Annex B `leb128` length with overflow and minimal-input checks.
2. Parse the complete OBU header and extension, and prove the OBU fits its
   declared span.
3. Classify the OBU by AV2 v1.0.0 type.
4. Require one Sequence Header OBU before coded picture data.
5. Parse the sequence header and derive all fixed `av2C` fields.
6. Move the Sequence Header OBU to `configOBUs`.
7. Move an allowed Layer Configuration Record or static metadata OBU only when
   the selected experimental profile supports it; v0 otherwise rejects it.
8. Drop Temporal Delimiter OBUs because the item boundary delimits the temporal
   unit.
9. Reject Padding OBUs rather than silently dropping bytes in v0.
10. Preserve coded OBUs byte-for-byte and in order in the item payload.
11. Verify that the first coded frame is a Closed Loop Key for every carried
    extended layer.
12. Verify that the stream produces the one picture required by the profile.
13. Serialize `configOBUs` and payload independently, both with length-delimited
    OBU framing.
14. Reparse the generated outputs and compare the parsed OBU ledger with the
    pre-serialization ledger.

Every rejection names the OBU index, byte offset, type, and violated rule.

### Deterministic muxer algorithm

The muxer takes a typed description, not arbitrary box bytes:

```text
Av2HeifInput
  width, height
  bit depth and chroma format
  CICP and range
  parsed AV2 configuration
  configOBUs byte span
  item payload byte span
```

It performs a sizing pass followed by a write pass:

1. Validate all semantic agreement before computing output.
2. Compute every box size and the final payload offset with checked arithmetic.
3. Reject any value that does not fit the selected v0 field widths.
4. Write boxes in canonical order.
5. Patch no unchecked pointer and perform no backward search for fields.
6. Reparse the complete output with the project inspector.
7. Byte-compare reparsed fields and extents with the input description.
8. Emit the file only after validation succeeds.

The same input bytes and manifest parameters must produce byte-identical output
across runs. If AVM output itself differs by platform, the manifest records that
fact and the checked-in canonical OBU input remains the source of deterministic
container tests.

### Fixture manifest

Each non-synthetic fixture has a machine-readable sidecar, preferably JSON,
with at least:

- dialect name and dialect revision;
- source image path, license/provenance, dimensions, and SHA-256;
- source pixel format, bit depth, range, and CICP values;
- AV2 specification version;
- AV2-ISOBMFF draft URL and immutable source revision;
- AVM repository URL, tag, resolved commit, build options, and binary SHA-256;
- full encoder argument vector;
- host architecture, operating system, compiler, and thread count;
- original elementary stream SHA-256;
- ordered OBU ledger with type, layer IDs, source offset, and size;
- normalized `configOBUs` SHA-256;
- normalized item payload SHA-256;
- final file SHA-256;
- expected `av2C`, `ispe`, `pixi`, and `colr` values;
- expected decoded plane hashes;
- expected rejection or success status for each reference implementation.

Do not use a prose README as the only record of fixture generation.

### Legacy fixture generation

A separate optional pipeline builds current libavif with
`AVIF_CODEC_AVM=LOCAL`, encodes a small image with explicit AVM codec choice,
and records the result under the `libavif-avm-2026-07` dialect. It must pin the
libavif commit because `main` is mutable and its `av2C` implementation is likely
to change.

Legacy fixtures exist to test detection, diagnostics, import, and differential
decoding. They are not canonical output examples.

## Proposed sibling repository

### Repository boundary

The proposed location is:

```text
<parent>/
  avif-decoder/
  avif-2/
```

`avif-2` is a separate Git repository. It does not modify the parent
`avif-decoder` build, tests, public API, supported-format matrix, or release
artifacts.

Before creating it, decide its permanent public name. The provisional name is
acceptable for local work, but a public repository should prefer a name that
does not imply an approved “AVIF 2” standard, for example `av2-heif-experiment`.

### Proposed source tree

```text
avif-2/
  LICENSE
  README.md
  Makefile
  docs/
    format-v0.md
    evidence.md
    architecture.md
    testing.md
    compatibility.md
  spec/
    README.md
    revisions.lock
  include/
    av2heif.h
  src/
    base.c, base.h
    container/
      bmff.c
      heif.c
      av2_item.c
    av2/
      bits.c
      leb128.c
      obu.c
      sequence_header.c
      content_interpretation.c
      frame_header.c
      entropy.c
      partition.c
      prediction.c
      transform.c
      reconstruction.c
      filters.c
      decoder.c
    output/
      color.c
      png.c
    platform/
      platform.h
      linux/
      macos/
    arch/
      x86_64/linux/
      arm64/macos/
  tools/
    av2heif-pack.c
    av2heif-inspect.c
    av2heif-import-libavif.c
    generate-fixtures.sh
  tests/
    unit/
    strict/
    fixtures/
    fuzz/
    differential/
    manifests/
  reference/
    README.md
    lock/
```

The final decomposition should follow the implemented control flow, not this
illustrative list mechanically. In particular, AV2 syntax modules should be
split only after the owning state and test boundaries are understood.

### Licensing boundary

`avif-decoder` is CC0. AVM v1.0.0 uses the BSD 3-Clause Clear License and the
AOMedia patent license. AV2 Tools and other references have their own terms.

**Decision D12:** Production decoder and muxer source in the sibling project
shall be written from the published specifications and project-owned design,
not copied from AVM or AV2 Tools. Optional reference builds remain separate and
retain their licenses.

**Decision D13:** CC0 code copied from `avif-decoder` shall be recorded in a
provenance document with source commit and paths. The new repository's overall
license must be chosen explicitly before public distribution; it must not claim
that third-party reference code is CC0.

This report is not legal advice. The project owner must review the AV2
specification terms, software licenses, and patent license before distribution.

## Freestanding decoder contract

The proposed decoder should preserve the useful engineering constraints of
`avif-decoder` without pretending that AV2 is a small patch to AV1.

### Non-negotiable core properties

- Freestanding C11 unless a later language-version decision is justified.
- No libc or external codec dependency in production targets.
- No allocation and no file I/O in the core API.
- Caller-owned immutable input.
- Caller-owned workspace and output planes.
- A query call computes all required workspace and output sizes before decode.
- Checked arithmetic for every offset, size, stride, count, and allocation.
- Reentrant calls with no mutable global decoder state.
- Structured error codes plus bounded diagnostics.
- Deterministic output across supported worker counts and platforms.
- Explicit image-size, dimension, OBU-count, tile-count, and recursion limits.
- Strict validation is the default; compatibility behavior is opt-in.

### Proposed public API shape

Names are provisional, but the two-pass contract is intentional:

```c
typedef struct Av2HeifLimits Av2HeifLimits;
typedef struct Av2HeifInfo Av2HeifInfo;
typedef struct Av2HeifImage Av2HeifImage;
typedef struct Av2HeifError Av2HeifError;

Av2HeifResult av2heif_query(
    const uint8_t *input,
    size_t input_size,
    const Av2HeifLimits *limits,
    Av2HeifInfo *info,
    Av2HeifError *error);

Av2HeifResult av2heif_decode(
    const uint8_t *input,
    size_t input_size,
    const Av2HeifInfo *info,
    void *workspace,
    size_t workspace_size,
    Av2HeifImage *image,
    Av2HeifError *error);
```

`Av2HeifInfo` must bind the query result to the input and selected item in a way
that prevents a caller from safely using stale sizing information for different
bytes. The implementation can use an input size plus internal structural digest
or require query and decode to repeat enough validation; this is an API design
question to resolve before freezing version 1.

The image API should expose planar decoded samples first. RGB conversion and
PNG writing are separate layers and must not obscure codec conformance.

### Layering

The decoder has four boundaries:

1. **Container parser:** proves box structure, identifies a supported dialect,
   resolves the primary item and extents, and validates properties.
2. **AV2 carriage parser:** parses `av2C`, configuration OBUs, and item payload;
   constructs one logical decoder input without mutating source bytes.
3. **AV2 decoder:** implements AV2 v1.0.0 syntax, semantics, prediction,
   transforms, reconstruction, and filters for the supported profile.
4. **Presentation/output:** applies supported HEIF properties and converts or
   writes output formats.

No layer may trust dimensions or format values from the preceding layer without
cross-checking them against the syntax it owns.

### Input spans rather than reconstructed temporary streams

The canonical file splits initialization OBUs and coded OBUs. A hosted AVM
adapter may concatenate them for a reference API, but the freestanding decoder
should consume an ordered list of immutable spans:

```text
span 0: av2C.configOBUs
span 1: item payload
```

The OBU reader advances across spans and presents one logical sequence. This
avoids allocating a duplicate elementary stream. It also makes the distinction
between configuration and picture data available to strict checks.

### Container parser requirements

At minimum, strict v0 parsing validates:

- `ftyp` appears before variable-length top-level boxes;
- box sizes, large sizes, and size-zero boxes cannot overflow input bounds;
- exactly one supported top-level `meta` is selected;
- `hdlr` is the first `meta` child and uses `pict`;
- `pitm`, `iloc`, `iinf`, and `iprp` uniqueness;
- nonzero and nonduplicated item IDs;
- `iloc` field widths and construction methods;
- every extent is within input and total item size cannot overflow;
- `infe` version and item type;
- `ipco` property indexing and `ipma` ordering;
- association count and 7/15-bit property-index rules;
- unknown essential properties cause the item to be unprocessable;
- exact required-property counts;
- `av2C` essential status;
- no contradictory duplicate `nclx`, `ispe`, or `pixi`;
- the primary item is the decoded item;
- unused bytes are either legal container padding or rejected according to the
  owning box syntax.

Parsing should index spans first and decode payloads later. It should never
follow an item reference recursively without a depth limit and cycle detection,
although v0 itself has no item references.

### AV2 parser requirements

The AV2 front end validates before reconstruction:

- minimal and bounded `leb128` lengths;
- complete OBU headers and legal reserved bits;
- legal OBU ordering and layer identifiers;
- exactly one supported sequence header;
- supported profile, level, tier, chroma format, and bit depth;
- single-picture and still-picture restrictions;
- maximum frame dimensions and caller limits;
- closed-loop-key entry and self-contained references;
- one output picture;
- no unsupported metadata, film grain, layer, or frame mechanism hidden in an
  otherwise supported stream;
- complete consumption or explicitly permitted alignment bits for each syntax
  structure.

Unsupported legal AV2 syntax returns `UNSUPPORTED`, while malformed syntax
returns `INVALID_BITSTREAM`. Tests must keep that distinction stable.

### AV2 implementation strategy

AV2 is not implemented by renaming AV1 symbols. The project may reuse generic
checked readers, arenas, task execution, pixel planes, PNG output, CICP color
conversion, and mathematically identical codec primitives from the CC0 codebase.
Every reused AV1 codec primitive must have a test proving that AV2 v1.0.0 uses
the same operation, constants, precision, rounding, clipping, and edge behavior.

The decoder should grow from complete, decodable syntax slices rather than from
a broad parser that accepts pictures it cannot reconstruct. A useful order is:

1. OBU framing and sequence-header parser.
2. Closed-loop-key picture headers and tile layout.
3. Entropy decoder and initial CDF state.
4. The smallest AVM-emittable intra profile with transforms and
   reconstruction.
5. Deblocking and every mandatory in-loop stage used by that profile.
6. Additional intra tools and transform sizes.
7. Chroma formats and high bit depth.
8. Parallel tiles and broader profile coverage.

The exact smallest AVM-emittable tool set must be discovered by encoding
controlled fixtures and inspecting syntax. The report does not assume that AVM
can disable every tool needed for a convenient bootstrap profile.

## Hosted tools versus production code

The sibling project intentionally has two dependency policies.

### Production and self-contained tests

- C compiler and ordinary build tools only.
- No AVM, libavif, AV2 Tools, FFmpeg, ImageMagick, OpenSSL, C++, or network.
- Checked-in synthetic and redistributable fixtures.
- Freestanding library and CLI targets.

### Reference and fixture-generation tools

- May build pinned AVM, libavif, AV2 Tools, and other independent parsers.
- May use hosted C/C++, CMake, Python, or shell orchestration.
- Must never be linked into the production decoder.
- Must record exact versions and hashes.
- Network access is setup-only; normal differential runs use a populated local
  reference cache.

This split permits rigorous interoperability testing without weakening the
`nolibc` product claim.

## Validation plan

### Container golden tests

The muxer needs byte-exact goldens for:

- smallest valid v0 file;
- odd image dimensions;
- largest v0 32-bit offset and length boundaries using synthetic sizing tests;
- each `pixi` channel count and bit depth as support is added;
- full- and limited-range `nclx`;
- property index and essential-bit encoding;
- `mdat` before and after metadata if the decoder supports both;
- multiple extents when decoder support is added;
- every supported `iloc`, `pitm`, `infe`, `iinf`, and `ipma` version.

Each golden has a box tree, byte offsets, field values, and a complete hex-level
expectation for the minimal writer.

### Negative container tests

At least these mutations must fail:

- truncated box header or body;
- size overflow, zero-sized forbidden child, and invalid large-size box;
- duplicate required unique box;
- missing or invalid `hdlr`;
- zero, duplicated, or dangling item ID;
- `pitm` pointing to a missing or unsupported item;
- unsupported `iloc` construction method;
- extent outside input, overlapping arithmetic overflow, or item-size overflow;
- invalid `infe` version or item type;
- missing `av2C`, `ispe`, `pixi`, or `colr`;
- duplicate required property;
- `av2C` not marked essential;
- property index zero marked essential or index outside `ipco`;
- unknown essential property;
- AVIF brands on a canonical file;
- draft `av02` track brand without the track required by that draft.

### OBU and `av2C` tests

Tests cover:

- every legal `leb128` width accepted by the implementation;
- nonterminating, overflowing, overlong, and truncated lengths;
- OBU header with and without extension;
- forbidden or reserved bits;
- missing, duplicate, late, and malformed Sequence Header OBU;
- Sequence Header in the item payload;
- coded frame in `configOBUs`;
- Temporal Delimiter or Padding in the item payload;
- every fixed `av2C` field mismatched against the sequence header;
- width or height mismatch against `ispe`;
- depth or channel mismatch against `pixi`;
- range or CICP mismatch against `colr`;
- non-closed random access picture;
- hidden or additional output picture;
- unsupported layers and metadata.

### Differential decoding

For every positive fixture:

1. Decode the normalized logical AV2 stream with pinned AVM.
2. Decode the container through the project's freestanding decoder.
3. Compare dimensions, format, bit depth, range, and CICP.
4. Compare every reconstructed plane sample exactly.
5. Hash the planes with dimensions and strides normalized.

Lossy source-to-reconstruction quality is not the decoder conformance metric.
The project decoder and AVM must reconstruct the same samples exactly.

### Round trips

The canonical round trip is:

```text
elementary AV2
  -> normalize and mux
  -> inspect and extract logical AV2
  -> AVM decode
  -> freestanding decode
```

Byte identity of the extracted elementary stream is not always expected because
the container deliberately moves configuration OBUs and removes the Temporal
Delimiter. Instead, compare the ordered normalized OBU ledger and decoded
pixels. Muxing an already normalized canonical representation must be byte
identical.

### Fuzzing

Separate fuzz targets should cover:

- top-level box framing;
- `meta` item indexing and associations;
- extent resolution;
- `av2C` parsing;
- Annex B OBU framing;
- sequence headers;
- supported picture headers;
- full decode with bounded tiny images;
- legacy-dialect detection and importer conversion.

Every fuzzer uses strict, low limits and treats timeout or excessive workspace
requests as failures. Regression inputs are minimized and checked into the
self-contained suite.

### Freestanding verification

Release validation includes:

- compile with `-ffreestanding`, `-nostdlib`, and the project's selected
  `-nostdinc` policy;
- inspect final ELF and Mach-O dependencies;
- reject unexpected undefined symbols and compiler-emitted runtime helpers;
- run strict and corpus tests on Linux/x86-64 and macOS/arm64;
- run hosted ASan and UBSan builds of the same core;
- compare hosted and freestanding decoded plane hashes;
- verify deterministic behavior across workspace alignments and worker counts.

## Compression experiment design

The project's first goal is interoperability, not a claim that AV2 is a
better still-image codec. Once decoding is correct, compression evaluation can
ask whether AV2's intra tools improve still images.

### Fair comparison rules

- Compare pinned AVM v1.0.0 with a pinned AV1 encoder configuration.
- Use the same source pixels, dimensions, chroma format, bit depth, range, and
  color conversion.
- Do not compare equal numeric quality or quantizer values across codecs.
- Sweep multiple quality points and compare rate-distortion curves.
- Report coded payload bytes and complete container bytes separately.
- Decode both codecs with independent reference decoders before measuring.
- Record encoder speed, threads, CPU, wall time, and peak memory, but do not
  imply reference-encoder speed predicts production performance.
- Separate natural images, screen content, graphics, noise, gradients, and HDR.
- Use exact lossless comparisons only when both configurations are genuinely
  lossless.

Useful outputs include per-plane PSNR/SSE, a versioned perceptual still-image
metric, complete-file bits per pixel, and BD-rate over a documented interval.
No single metric or cherry-picked quality point is sufficient.

### What a still-image result can and cannot show

A one-picture experiment can measure gains from AV2 intra prediction,
partitioning, transforms, entropy coding, filtering, restoration, screen
content tools, and still-picture syntax. It cannot measure gains that require
temporal prediction, random-access structures over time, stream switching,
multistream delivery, or video rate control.

## Security and robustness policy

The parser and decoder process untrusted files. The initial design therefore
requires:

- no unchecked addition, multiplication, shift, narrowing conversion, or
  pointer offset;
- no recursion controlled solely by input without an explicit depth limit;
- no loop whose progress depends on a malformed zero-length syntax element;
- bounded OBU count, property count, extent count, item count, tile count, and
  metadata size;
- dimensions checked independently and as a pixel product;
- workspace query failure before allocation when limits are exceeded;
- immutable source spans and no pointers retained beyond their documented
  lifetime;
- separate malformed and unsupported errors;
- no decoder fallback based on brands alone;
- no acceptance of unknown essential properties;
- complete initialization of every public result and error structure;
- exact tests for truncated input at every byte boundary of minimal fixtures.

Compatibility mode may relax only named dialect rules. It does not disable
bounds checks or accept structurally ambiguous input.

## Assumption register

These assumptions permit planning but require confirmation:

| ID | Assumption | Required confirmation | Failure response |
| --- | --- | --- | --- |
| A1 | AVM v1.0.0 can reliably emit the restricted 8-bit 4:2:0 one-picture profile. | Build pinned AVM, encode a matrix of tiny/odd/ordinary images, parse every OBU, and decode with AVM. | Narrow or change the initial profile; do not patch bytes after encoding. |
| A2 | Moving the Sequence Header OBU from AVM's elementary stream into draft-shaped `av2C` and presenting configuration before coded data preserves decode semantics. | Reconstruct a logical Annex B stream from the two spans and compare AVM output before and after normalization. | Keep a sequence header in a project-private payload revision or wait for clarified image-item rules; do not claim draft-derived v0 compatibility. |
| A3 | `OBU_CLOSED_LOOP_KEY` is sufficient for the single-layer self-contained item profile when all referenced HLS OBUs are in `av2C` or the payload. | Implement availability checks from AV2 v1.0.0 and test decoder initialization from empty state. | Add missing required initialization syntax or reject the stream. |
| A4 | Generic HEIF `mif1` branding without `miaf` or codec brands is an honest container declaration for v0. | Validate against the applicable HEIF brand requirements and at least two independent box inspectors. | Use no public conformance brand until a correct structural brand is identified. |
| A5 | Useful generic infrastructure can be copied from `avif-decoder` without importing AV1 assumptions. | Review each copied module and run independent unit tests before AV2 integration. | Rewrite the module or make assumptions explicit in its API. |
| A6 | A complete AV2 decoder can remain allocation-free using a caller-sized arena. | Produce a conservative memory model from AV2 limits and the first supported profile before pixel reconstruction lands. | Reduce supported profile limits or revise the API before release; do not allocate secretly. |
| A7 | Exact plane comparison with AVM is available for every supported profile. | Add a reference harness that exports raw reconstructed planes and metadata. | Keep the profile experimental and do not mark decoder support complete. |

Assumptions are not converted to facts merely because one fixture works.

## Open questions

The following questions are intentionally unresolved:

1. **Future standard name and brand:** Will AOMedia define an AV2 image format,
   an AVIF successor, an AV2 HEIF item, or a different container profile?
2. **Image-item configuration placement:** Will a future item binding copy the
   track draft by moving the sequence header into `av2C`, or follow AVIF's AV1
   item pattern by retaining it in item data?
3. **Item type and property FourCCs:** Will `av02` and `av2C` be assigned to
   image items as well as tracks?
4. **Still-image brand and media type:** No reviewed source defines them.
5. **MIAF conformance:** Which additional constraints and profile brands will
   apply to AV2 coded items?
6. **Alpha semantics:** HEIF auxiliary-image relationships are reusable, but an
   AV2 image specification may add codec-specific restrictions.
7. **Layer selectors and progressive images:** AVIF's `a1op` and `a1lx` are
   AV1-specific. They cannot be assumed to describe AV2's three scalability
   dimensions.
8. **Content interpretation placement:** The AV2 draft permits static metadata
   in `configOBUs`; exact image-property precedence and duplication rules need
   normative clarification.
9. **Film grain:** A still-image renderer needs explicit rules for synthesized
   output, metadata agreement, and comparison.
10. **12-bit reference support:** libavif's current AVM encoder rejects 12-bit,
    and AVM support must be demonstrated independently.
11. **Compliance tooling:** No reviewed image-item compliance checker recognizes
    the proposed project dialect.
12. **Permanent repository and API name:** `avif-2` is concise but risks being
    interpreted as an approved AVIF version.

An open question blocks only the feature that depends on it. The initial v0
profile is designed to avoid most of them.

## Phased implementation plan

### Phase 0: lock evidence and bootstrap the repository

Deliverables:

- create the sibling Git repository;
- choose its public name and license;
- copy this report into its docs and retain a pointer here;
- pin AV2 v1.0.0, AVM v1.0.0, the exact AV2-ISOBMFF draft revision, a libavif
  commit, and an AV2 Tools commit;
- record source licenses and provenance;
- establish hosted and freestanding build profiles;
- add checked arithmetic, reader, arena, and test substrate.

Exit gate: a clean build and self-contained unit test run on macOS/arm64 and
Linux/x86-64, with no codec implementation yet.

### Phase 1: OBU inspector and canonical muxer

Deliverables:

- Annex B `leb128` reader/writer;
- AV2 OBU header parser and ordered ledger;
- enough AV2 sequence-header and content-interpretation parsing to derive v0
  metadata;
- `av2C` parser/writer with field cross-checks;
- strict minimal HEIF parser and deterministic v0 writer;
- box-tree and JSON inspection output;
- synthetic positive and negative fixtures.

Exit gate: the muxer reparses byte-identical goldens, rejects all named
mutations, and requires no AVM at test time.

### Phase 2: pinned AVM fixture pipeline

Deliverables:

- build script for pinned AVM;
- deterministic source image/Y4M generation;
- one-frame AVM encode recipe verified by parsed syntax;
- OBU normalizer and canonical muxing;
- AVM logical-stream reconstruction adapter;
- fixture manifests and decoded plane hashes;
- optional libavif legacy fixture generator and importer.

Exit gate: at least one ordinary, one odd-dimension, one tiny, and one
high-detail v0 file decode through AVM after normalization, with reproducible
manifests.

### Phase 3: freestanding container and AV2 header query

Deliverables:

- public query API;
- caller limits and structured errors;
- complete strict v0 container validation;
- dialect detection;
- AV2 configuration and picture-header query sufficient to report dimensions,
  format, bit depth, required output, and conservative workspace;
- `nolibc` inspection CLI.

Exit gate: query results match manifests, malformed fixtures fail at the
expected layer, and the final binaries have no unintended runtime dependencies.

### Phase 4: first complete AV2 decode slice

Deliverables:

- entropy state and coded syntax for the smallest profile AVM can emit;
- intra prediction, inverse quantization, transforms, reconstruction, and every
  mandatory filter for those streams;
- planar 8-bit 4:2:0 output;
- exact differential comparison with AVM;
- sanitizer, fuzz, alignment, truncation, and freestanding coverage.

Exit gate: every supported fixture is sample-exact with AVM on all planes and
no accepted syntax reaches an unimplemented reconstruction path.

### Phase 5: practical still-image surface

Add formats and container features one independently testable slice at a time:

- monochrome and 4:4:4;
- 10-bit samples;
- RGB conversion and streaming PNG output;
- transformations and metadata;
- alpha auxiliary item;
- grids and bounded parallelism.

Each slice requires strict syntax tests, AVM plane equality, workspace bounds,
fuzz coverage, and updated support documentation.

### Phase 6: compression study

Only after decoder correctness and fixture reproducibility are stable:

- define the image corpus and provenance;
- pin AV1 and AV2 encoder configurations;
- generate rate-distortion curves;
- report payload and total-container results;
- publish raw machine-readable measurements and commands;
- state clearly which AV2 video features are irrelevant to the still-image
  result.

## Definition of done for the initial experiment

The initial experiment is complete when all of the following are true:

- The sibling project exists and `avif-decoder` remains AVIF/AV1-only.
- The project's format document distinguishes verified external rules from its
  own item semantics.
- Exact source revisions and licenses are pinned.
- A deterministic tool creates AV2-HEIF experimental v0 files from normalized
  AVM output.
- An inspector prints the container tree, dialect, OBU ledger, and all metadata
  cross-checks.
- A converter can import the named libavif AVM legacy dialect without treating
  it as canonical.
- The freestanding decoder queries and decodes the initial 8-bit 4:2:0 profile.
- Decoded planes are bit-exact with pinned AVM.
- Production binaries use no libc or external codec library.
- Core APIs allocate nothing and perform no I/O.
- Positive, negative, truncation, sanitizer, fuzz, and cross-platform tests
  pass.
- Fixtures have complete manifests and redistributable provenance.
- Documentation never labels the files as standardized AVIF or serves them as
  `image/avif`.

## Decision summary

The controlling decisions are:

- **D1-D3:** do not claim AVIF brands, media type, extension, or standard name;
  use a sibling project and call the format AV2-HEIF experimental v0.
- **D4-D5:** pin the draft and keep v0 to one unencrypted, untimed image item.
- **D6-D8:** preserve current libavif output as a separate legacy dialect and
  oracle, not the canonical format.
- **D9-D10:** use draft-shaped `av2C`, put the Sequence Header OBU in
  `configOBUs`, use draft sample framing for the item payload, and require a
  verified self-contained single picture.
- **D11:** version dialects additively and fail on ambiguity.
- **D12-D13:** keep third-party reference code out of production and make the
  licensing/provenance boundary explicit.

The resulting plan is intentionally conservative. It enables real AV2
still-image files and a genuinely independent decoder without asserting that a
track-only working draft has already standardized AV2 image items. That
distinction should remain visible in code, fixtures, diagnostics,
documentation, and benchmark claims until an approved external specification
resolves it.