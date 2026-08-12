# Image corpus plan

## Status and purpose

This document describes the replacement for the organically grown image and
artifact layout now preserved under `images/old/`. The source layer is
implemented: the initial Commons originals, reviewed source records, exact
license texts, attribution, directory skeleton, and empty recipe contracts are
in place. Preparation and reference generation remain planned work. The old
files should remain available until every active test, generator, report, and
viewer asset has been migrated.

The replacement corpus has four goals:

1. Every photographic source has reviewable authorship, license, source, and
   integrity information.
2. Every encoder input is reproducibly derived from an exact source file.
3. Every AV2 reference stream and decoded plane is tied to a pinned AVM build
   and a canonical prepared input.
4. A normal build and test run remains offline and does not require the large
   photographic corpus.

## Source strategy

### Wikimedia Commons for 8-bit photography

[Wikimedia Commons Picture of the day](https://commons.wikimedia.org/wiki/Commons:Picture_of_the_day)
is the primary discovery route for high-quality, freely licensed 8-bit
photographs. Picture-of-the-day selection is evidence of photographic quality,
not a license grant. Every selected file must still be reviewed on its own
Commons file page.

Commons photographic originals are almost always 8-bit JPEGs. They are
therefore appropriate for the 8-bit I400, I420, and I444 photographic corpus.
Converting one to a 10-bit plane can exercise 10-bit packing and decoder paths,
but it does not provide genuine additional sample precision and must not be
presented as a true 10-bit photographic source.

Prefer, in order:

- CC0 or unambiguous public-domain works;
- CC BY 4.0 works with straightforward creator attribution;
- CC BY-SA 4.0 works when distributing all image derivatives under the same
  license is acceptable.

Avoid sources with unclear authorship, unclear licensing, GFDL-only terms,
noncommercial or no-derivatives restrictions, or unresolved personality,
privacy, trademark, artwork, or freedom-of-panorama concerns. Commons normally
requires freely reusable media, but its metadata is not a legal warranty.

The creator must be credited rather than merely the Commons uploader. When a
file offers multiple licenses, the source record must identify the one chosen
for this project. Public-domain works should still receive provenance credit.

### Project-owned Pixel sources for 10-bit photography

Genuine 10-bit photographic material will come from project-author-owned
Google Pixel 8 DNG captures. The existing main-camera and ultrawide pipeline
has already demonstrated that these sources can produce useful
`yuv420p10le` and `yuv444p10le` inputs. Future captures should broaden scene,
lighting, texture, noise, color, and camera-module coverage rather than merely
increase the number of similar images.

Each owned source needs the same integrity and transformation records as a
Commons source, plus its camera module, raw dimensions, active crop,
orientation, black and white levels, and raw-development settings. Before any
source or derivative is published, metadata must be checked for GPS or other
private information. The source record must state who owns the photograph and
the license under which any distributed derivative is released.

## Intended directory layout

```text
images/
  plan.md
  README.md
  ATTRIBUTION.md

  licenses/
    CC-BY-4.0.txt
    CC-BY-SA-4.0.txt
    CC0-1.0.txt

  sources/
    commons/
      <source-id>/
        source.json
        <downloaded-media>
    owned/
      pixel8/
        <source-id>/
          source.json
          <original-dng>

  recipes/
    preparations.json
    encodings.json
    suites.json

  prepared/
    <preparation-id>/
      <source-id>/
        master.png
        source-<width>x<height>-gray8.i400
        source-<width>x<height>-yuv420p8.i420
        source-<width>x<height>-yuv444p8.i444
        source-<width>x<height>-yuv420p10le.yuv
        source-<width>x<height>-yuv444p10le.yuv
        manifest.json

  reference/
    avm-<revision>/
      <suite>/
        <case-id>/
          encoded.obu
          decoded.<pixel-format>
          manifest.json

  reports/
    status.md
    blocker-map.md

  old/
    <temporary preserved old layout>
```

The exact downloaded and generated media can remain ignored or be distributed
as release artifacts. Source records, recipes, attribution, license texts, and
lock manifests should be tracked. Small fixtures needed by ordinary tests
belong under `tests/fixtures/`, not in the large photographic workspace.

## Source metadata contract

Every `source.json` should be a manually reviewed record containing at least:

- a stable project source ID and human-readable title;
- source class (`commons` or `project-owned`);
- creator name and creator or requested-attribution URL;
- canonical Commons file page, when applicable;
- a permalink or revision identifier for the reviewed Commons metadata;
- the exact chosen license name, SPDX expression, and license URL;
- the required credit line and any copyright notice;
- original media URL and exact download URL;
- original filename, MIME type, byte size, pixel dimensions, and SHA-256;
- Wikimedia media revision timestamp and retrieval date, when applicable;
- review status, review date, and any non-copyright concerns;
- ownership and privacy review for project-owned captures.

The source hash identifies the acquisition bytes. It must not be replaced by a
hash of a thumbnail, decoded bitmap, or later derivative. If a deliberately
selected Wikimedia-generated derivative is used to limit bandwidth, both the
original work and exact derivative must be recorded and hashed.

`images/ATTRIBUTION.md` should be generated from these records. Generated
manifests, retained fixtures, and browser assets should refer to the same
source ID rather than carry separately maintained attribution text. The root
repository license must clearly exclude separately licensed image sources and
derivatives from any general CC0 grant.

## Wikimedia acquisition

Downloading is an explicit bootstrap operation, never an implicit part of a
build or test. The downloader should:

- consume only reviewed source records and never auto-approve a live license;
- use the Commons Action API to compare current metadata with the reviewed
  record and report drift;
- send a descriptive User-Agent containing project and contact information;
- batch metadata queries where possible but download media serially;
- use GET, compression, caching, and `maxlag` for noninteractive API work;
- honor `Retry-After` and use bounded exponential backoff for throttling and
  temporary server errors;
- skip a local file when its size and SHA-256 already match;
- stream into a temporary file, verify size and hash, and rename atomically;
- refuse to overwrite mismatching data without an explicit repair operation;
- restrict redirects to expected HTTPS Wikimedia hosts;
- offer source and suite selection plus a maximum-byte budget;
- never hotlink Commons files from tests, documentation, or the viewer.

These rules follow Wikimedia's current
[User-Agent policy](https://foundation.wikimedia.org/wiki/Policy:Wikimedia_Foundation_User-Agent_Policy)
and [API etiquette](https://www.mediawiki.org/wiki/API:Etiquette), which favor
identifiable clients, serial requests, caching, and considerate retries.

## Canonical prepared inputs

The generated raw planes should be called canonical prepared inputs rather
than lossless source images. Their stored bytes are lossless, but JPEG decode,
crop, color conversion, chroma subsampling, and resizing are transformations
of an already lossy source.

Preparation should have two explicit steps:

```text
exact downloaded JPEG or owned DNG
    -> oriented, cropped, color-normalized, scaled RGB master
    -> canonical I400, I420, I444, or 10-bit YUV encoder inputs
```

All pixel formats for one preparation should derive from the same RGB master.
This keeps geometry and luma behavior comparable across chroma formats.

Each preparation recipe must state:

- source ID and source SHA-256;
- EXIF orientation handling and exact crop rectangle;
- embedded-profile policy and target RGB color space;
- exact output width and height, not just a long-edge request;
- scaling kernel, support, edge behavior, arithmetic, and rounding;
- RGB-to-YUV matrix and integer rounding rules;
- full or limited range;
- chroma siting and chroma downsampling filter;
- bit depth, packing, plane order, and byte order;
- converter version or binary hash;
- expected output size and SHA-256 for every generated file.

Raw plane files have no self-description, so dimensions and the full pixel
contract must be present in their sidecar manifest and encoded in their
filename. Main photographic suites should use conventional even dimensions;
odd dimensions and boundary geometry belong in a deliberate geometry suite.

### Reproducible conversion

Merely documenting an FFmpeg or ImageMagick command does not establish
byte-identical reproduction across tool versions and platforms. The first
implementation step should compare a pinned conversion toolchain on Linux and
Windows using representative JPEG and DNG inputs.

If those hashes differ, use a narrow hermetic preparation tool. It should use
an established, pinned JPEG or raw decoder rather than introduce a new image
decoder, followed by project-controlled fixed-point scaling, color conversion,
and chroma subsampling. Host floating-point behavior, unspecified library
defaults, and platform-dependent output must not be part of the canonical
contract.

The prepared RGB master may be retained or distributed to make later YUV
conversion independent of JPEG or DNG development. The full path from the
original source must still remain documented and reproducible.

## AVM reference generation

The AVM stage consumes only canonical prepared inputs. It produces a concrete
test stream and the exact output expected from that stream.

Each reference manifest should record:

- source ID, preparation ID, and prepared-input SHA-256;
- dimensions, pixel format, bit depth, range, matrix, and chroma siting;
- pinned AVM source revision;
- `avmenc` and `avmdec` binary hashes and version banners;
- complete normalized encoder and decoder arguments;
- encoded OBU size, SHA-256, and parsed OBU ledger;
- decoded plane size and SHA-256;
- source license inherited by distributed derivatives;
- whether the local decoder matches AVM byte for byte.

Commands in manifests must use normalized repository-relative paths rather
than host-specific absolute paths. Generation should occur in a temporary
directory, validate all outputs, and publish only a complete successful run.
It must not delete supplemental evidence owned by another stage.

Encoding should use one frame and one thread and should run twice to detect
nondeterminism. OBU byte reproducibility and decoded-pixel reproducibility are
separate claims: both should be measured, and neither should be assumed merely
because the AVM revision is pinned.

## Corpus composition

The photographic corpus should be deliberately small and varied rather than a
Cartesian product of every image, size, format, and QP. Candidate content
classes include:

- foliage and other high-frequency natural texture;
- feathers, hair-like detail, fabric, or fine repeated patterns;
- architecture and strong straight edges;
- smooth sky or wall gradients;
- low-light sensor noise;
- saturated colors and sharp chroma transitions;
- near-monochrome scenes;
- high-contrast fine detail;
- wide scenes and close macro detail;
- multiple Pixel camera modules and lighting conditions for true 10-bit data.

Photographs provide realistic entropy and reconstruction workloads. Synthetic
fixtures should continue to cover exact corner cases, tiny dimensions, odd
chroma geometry, extreme sample values, and isolated coding tools.

### Initial Commons source batch

The following source audit and acquisition were performed on 2026-08-12. Links in the
`Source` column are normalized canonical file pages; `revision` links pin the
reviewed description-page metadata. All 14 current originals report MIME type
`image/jpeg`. Dimensions, byte sizes, media timestamps, and Commons SHA-1
values were checked through the Commons Action API. The downloaded originals
were then verified and their SHA-256 identities recorded in `source.json`.

`Core` means included in the initial photographic source batch.

| Decision | Proposed source ID | Source and reviewed revision | Creator and chosen license | Current original | Intended coverage and caveats |
| --- | --- | --- | --- | --- | --- |
| Core | `pitti-ceiling` | [Palazzo Pitti ceiling](https://commons.wikimedia.org/wiki/File:Palacio_Pitti,_Florencia,_Italia,_2022-09-18,_DD_163-165_HDR.jpg), [revision 1227113384](https://commons.wikimedia.org/w/index.php?oldid=1227113384) | Diego Delso, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 5,515 x 5,515; 10,837,769 bytes; media revision 2023-04-04T06:02:09Z | Dense fresco and stucco detail, symmetry, broad colors, and a square frame. Tone-mapped HDR; preserve the requested `Diego Delso, delso.photo` credit form and mark all preparations as modified. |
| Core | `rome-ponte-blue-hour` | [Ponte Vittorio Emanuele II](https://commons.wikimedia.org/wiki/File:Rom_(IT),_Br%C3%BCcke_%E2%80%9EPonte_Vittorio_Emanuele_II%E2%80%9C_--_2024_--_0732.jpg), [revision 1256318675](https://commons.wikimedia.org/w/index.php?oldid=1256318675) | Anil Öztas, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 5,888 x 3,926; 19,872,949 bytes; media revision 2024-10-18T16:08:58Z | Blue-hour gradients, illuminated stone, water reflections, fine railings, and mixed color temperatures. Required attribution is `© Anil Öztas`; VRT permission is recorded on Commons. |
| Core | `elbrus-milky-way` | [Milky Way over Elbrus](https://commons.wikimedia.org/wiki/File:Milky_way_in_Elbrus.jpg), [revision 1259339443](https://commons.wikimedia.org/w/index.php?oldid=1259339443) | oliwok, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | 3,648 x 5,472; 27,322,996 bytes; media revision 2023-05-21T06:33:18Z | Sparse stars, dark cloud structure, low-light noise, near-black gradients, mountain edges, and small bright tents. Long exposure at ISO 5000. |
| Core | `finland-warbirds` | [Hurricane and T-6 over Finland](https://commons.wikimedia.org/wiki/File:Finnish_Air_Force_Hawker_Hurricane_warbird_and_US_Navy_T-6_Texan_warbird_in_flight_over_Finland.jpg), [revision 1259495366](https://commons.wikimedia.org/w/index.php?oldid=1259495366) | Peng Chen, [CC BY-SA 2.0](https://creativecommons.org/licenses/by-sa/2.0/) | 2,560 x 1,440; 1,328,325 bytes; media revision 2015-06-09T05:52:01Z | Large smooth atmospheric regions against compact mechanical detail, camouflage, trees, water, and reflections. The Flickr license later changed; Commons' 2015 review preserves the irrevocable CC BY-SA 2.0 grant. Keep that review evidence in the source record. |
| Core | `baya-weaver` | [Baya Weaver](https://commons.wikimedia.org/wiki/File:Baya_Weaver_in_Bhigwan_August_2025_by_Tisha_Mukherjee_04.jpg), [revision 1260205636](https://commons.wikimedia.org/w/index.php?oldid=1260205636) | Tisha Mukherjee, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 3,162 x 2,108; 2,909,038 bytes; media revision 2025-08-19T17:15:10Z | Fine feathers, grasses and twigs, yellow and brown transitions, shallow background, and visible high-ISO texture. |
| Core | `succisa-flower-bud` | [Succisa pratensis flower bud](https://commons.wikimedia.org/wiki/File:Bloemknop_van_een_blauwe_knoop_(Succisa_pratensis)._29-08-2023._(d.j.b).jpg), [revision 1252796453](https://commons.wikimedia.org/w/index.php?oldid=1252796453) | Dominicus Johannes Bergsma, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 3,456 x 3,456; 8,129,395 bytes; media revision 2023-12-27T16:31:43Z | Hair-like bristles, repeated florets, purple-on-green chroma detail, and smooth defocus. A 36-frame focus stack; record it as manipulated photographic content. |
| Core | `toco-toucan` | [Toco toucan](https://commons.wikimedia.org/wiki/File:006_Toco_toucan_in_Encontro_das_%C3%81guas_State_Park_Photo_by_Giles_Laurent.jpg), [revision 1255977494](https://commons.wikimedia.org/w/index.php?oldid=1255977494) | Giles Laurent, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 6,914 x 4,609; 15,023,557 bytes; media revision 2025-07-13T21:37:21Z | Feather detail, dark plumage, bright orange beak, hard chroma boundaries, branches, and subdued foliage. Use the requested `© Giles Laurent, gileslaurent.com, License CC BY-SA` attribution. |
| Core | `chinnathurai-church` | [St. Jude's Church](https://commons.wikimedia.org/wiki/File:Front_Judes_Church_Chinnathurai_Tamil_Nadu_crop_Mar24_A7C_10007.jpg), [revision 1246934269](https://commons.wikimedia.org/w/index.php?oldid=1246934269) | Timothy A. Gonsalves, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 3,629 x 3,629; 8,666,685 bytes; media revision 2025-09-17T03:27:53Z | Pale uniform sky and walls against sharp architectural ornament, railings, sand, gravestones, and foliage. The current file includes AI-assisted distraction removal and a cloned-out bird; record those edits. The request to contact the creator before commercial use is not part of the selected CC license. |
| Core | `scarlet-flycatcher` | [Scarlet Flycatcher](https://commons.wikimedia.org/wiki/File:095_Scarlet_Flycatcher_in_Encontro_das_%C3%81guas_State_Park_Photo_by_Giles_Laurent.jpg), [revision 1246417320](https://commons.wikimedia.org/w/index.php?oldid=1246417320) | Giles Laurent, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 2,848 x 1,899; 3,289,321 bytes; media revision 2025-09-25T20:55:07Z | Saturated red plumage, fine feather and branch detail, and a smooth green background. Some content overlap with the other birds, but it adds a difficult red transition and remains small enough for a viewer candidate. Use the creator's requested credit line. |
| Core | `emerald-swift` | [Emerald swift](https://commons.wikimedia.org/wiki/File:Emerald_swift_(Sceloporus_malachiticus)_Finca_El_Pilar.jpg), [revision 1248410468](https://commons.wikimedia.org/w/index.php?oldid=1248410468) | Charles J. Sharp, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 4,095 x 2,048; 7,210,585 bytes; media revision 2023-05-04T15:49:17Z | Dense scales, spines, bark, green-blue variation, shallow depth of field, and an unusual 2:1 frame. In-camera focus bracketing merged with Helicon Focus; record it as a focus stack. |
| Core | `jinan-grand-theater` | [Shandong Grand Theater interior](https://commons.wikimedia.org/wiki/File:Jinan_Grand_Theater-20150520-RM-170343.jpg), [revision 1255022112](https://commons.wikimedia.org/w/index.php?oldid=1255022112) | Reinhold Möller, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 4,418 x 3,314; 7,438,268 bytes; media revision 2021-05-28T08:47:11Z | Repeated seating, long straight and curved edges, ceiling texture, warm wood, and deep interior gradients. Retain the page's social-media compatibility warning and independently review interior-design rights before public redistribution. |
| Core | `hong-kong-harbour-night` | [Hong Kong Harbour at night](https://commons.wikimedia.org/wiki/File:Hong_Kong_Harbour_Night_2019-06-11.jpg), [revision 1234814215](https://commons.wikimedia.org/w/index.php?oldid=1234814215) | Benh LIEU SONG, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 10,439 x 4,936; 12,162,479 bytes; media revision 2020-03-22T18:14:56Z | Very wide panorama, dense windows and signage, rain haze, dark gradients, water, and colored reflections. The source was Flickr-reviewed under CC BY-SA 2.0 and the current Commons page offers 4.0 after creator edits; preserve both facts and select 4.0 explicitly. |
| Core | `pastel-chalks` | [Pastel chalk art](https://commons.wikimedia.org/wiki/File:Pastellkreide_--_2025_--_8665.jpg), [revision 1249763715](https://commons.wikimedia.org/w/index.php?oldid=1249763715) | Dietmar Rabich, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 6,720 x 4,480; 25,106,351 bytes; media revision 2025-08-04T11:23:56Z | Excellent varied-color texture and intentional blur, made with camera movement and multiple exposure. The request not to change the file, license, filename, or embedded EXIF/IPTC/XMP is non-binding and does not narrow the selected license. Preserve its spirit by retaining the original unchanged, carrying the exact title, author, source, license, and relevant metadata in the source record and dataset manifest, and marking derived PNG/YUV files as modifications. |
| Core | `freiburg-minster-columns` | [Freiburg Minster column art](https://commons.wikimedia.org/wiki/File:Freiburg_im_Breisgau,_M%C3%BCnster,_Vorhalle_--_2025_--_7903_(kreativ_1).jpg), [revision 1250960945](https://commons.wikimedia.org/w/index.php?oldid=1250960945) | Dietmar Rabich, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) | 4,480 x 6,720; 14,864,515 bytes; media revision 2025-06-26T19:20:45Z | Strong vertical color bands, symmetry, repeated geometry, and smooth intentional blur. It is an ICM photograph mirrored, combined, and rotated in post-processing. As with `pastel-chalks`, retain the unchanged original and preserve the requested attribution and metadata in the source record and dataset manifest when derivative formats cannot carry the same embedded metadata structure. It is partly redundant with the architecture and abstract-color coverage above. |

On 2026-08-12, Mathias Schindler notified Dietmar Rabich of the intended use of
these two images in the demo in the Commons discussion
[Bildkompressionsverfahren](https://commons.wikimedia.org/wiki/User_talk:XRay#c-Mathias_Schindler-20260812144900-Bildkompressionsverfahren).
This was a courtesy notice, not a request for additional permission, and no
response or silence should be represented as approval. Independently of the
rights granted by CC BY-SA 4.0, this project will voluntarily remove and replace
either image if the author objects to its use; it will not contest such an
objection by asserting its license or copyright position. Preserve the notice
and any response in the corresponding source records.

This gives a 14-image initial core. It deliberately
retains three birds because their coding stresses differ: fine yellow/brown
feathers, black/orange hard transitions, and saturated red against defocus.
If a smaller first download is needed, defer `scarlet-flycatcher` first because
it shares creator, habitat, and photographic technique with `toco-toucan`.

No candidate is a genuine 10-bit source. Several are processed photographs
(HDR, focus stacks, a panorama, retouched architecture, or artistic
composites); that is useful codec content but must be disclosed and must not be
used as evidence about unprocessed camera fidelity. The corresponding
`source.json` records preserve exact requested attribution wording, the media
and reviewed description revisions, acquisition hashes, and non-copyright
review notes. The Baya Weaver record deliberately retains reviewed revision
1260205636 after a later page-only edit introduced unrelated text; its media
bytes did not change.

Suggested suites are:

- `smoke`: small inputs for quick local validation;
- `photographic`: representative realistic content;
- `large`: memory, traversal, and browser stress;
- `formats`: I400, I420, I444, and supported 8/10-bit combinations;
- `geometry`: intentional odd and boundary dimensions;
- `viewer`: modest assets intended for browser distribution.

Do not retain BMP previews unless BMP itself is under test. PNG previews can be
generated on demand and avoid much of the current duplication.

## Tracking and validation policy

The repository should track:

- this plan and the eventual corpus README;
- reviewed source metadata;
- preparation, encoding, and suite recipes;
- applicable license texts and generated attribution;
- generated lock manifests and compact reports;
- small, explicitly selected conformance fixtures.

The repository should normally ignore:

- downloaded full-resolution Commons media;
- private original DNGs;
- prepared raw planes and previews;
- bulk AVM streams and decoded outputs;
- temporary generation directories;
- the preserved `images/old/` tree.

An offline verifier should check schemas, all locally present hashes, raw plane
size formulas, manifest references, attribution completeness, and stale or
orphaned outputs. A stronger reproduction check should regenerate into an
isolated build directory and byte-compare against the lock manifest.

Large downloadable corpus bundles should contain the relevant attribution and
license information alongside the media. Tests and CI may use a cache, but
must never silently fetch data from Wikimedia.

## Migration sequence

1. Freeze and inventory `images/old/`: file hashes, dimensions, current
   consumers, likely origin, and known provenance.
2. Classify every old source as verified Commons, project-owned, or unknown.
   Unknown sources must not enter new releases or regenerated documentation.
3. Finalize the source, recipe, and lock-manifest schemas and the repository
   licensing boundary.
4. Select an initial 8-bit Commons corpus of approximately 8 to 12 deliberately
   varied Picture-of-the-day candidates and review each file individually.
5. Import the existing project-owned Pixel DNG records as the initial genuine
   10-bit corpus, preserving their current hashes and development contract.
6. Implement source fetching and offline source verification.
7. Establish and cross-platform-test the deterministic preparation pipeline.
8. Implement isolated pinned-AVM encode/decode reference generation.
9. Migrate generators, tests, census reports, documentation, and viewer assets
   one suite at a time.
10. Generate all attribution surfaces from the source records and verify the
    license of every distributed derivative.
11. Remove the old layout only after no active consumer refers to it and all
    replacement suites pass exact parity validation.

The initial migration should prioritize clear ownership and reproducibility,
not preserving every old photographic case. A smaller defensible corpus is
more valuable than a larger collection whose origin or generation contract is
uncertain.