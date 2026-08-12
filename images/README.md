# Image corpus

This directory contains the provenance and working layout for the project's
photographic corpus. The initial source layer was acquired and reviewed on
2026-08-12: 14 Wikimedia Commons JPEG originals totaling 164,162,233 bytes
(156.56 MiB).

The downloaded originals are deliberately ignored by Git. Their
`source.json` records, license texts, attribution, and recipes are tracked.
A fresh clone therefore has a complete provenance catalog but does not fetch
large media during configuration, build, or test.

## Layout

- `sources/commons/<source-id>/source.json` is the reviewed source of truth.
  A locally acquired original sits beside that record under its exact Commons
  filename.
- `sources/owned/pixel8/` is reserved for project-owned DNG records and private
  source files. DNG bytes are ignored by Git.
- `licenses/` contains exact copies of the three Creative Commons legal-code
  editions selected for this batch.
- `recipes/` contains the versioned preparation, encoding, and suite
  contracts plus the tracked prepared-output lock.
- `prepared/` contains generated RGB masters, planar encoder inputs, and
  per-preparation manifests. It is ignored by Git and can be regenerated.
- `reference/` and `reports/` are working/output areas for later stages. Bulk
  files in those directories remain ignored.
- `old/` preserves the previous layout during migration and must not be used as
  a provenance source for new releases.
- `plan.md` defines the full preparation, AVM reference, validation, and
  migration contracts.

## Integrity

Each Commons source record pins the reviewed description revision and media
revision, original URL and filename, dimensions, MIME type, byte size,
Commons SHA-1, and locally verified SHA-256. Verify the tracked catalog and any
locally present originals offline with:

```sh
python3 tools/verify_image_sources.py
```

Require every downloaded original to be present with:

```sh
python3 tools/verify_image_sources.py --require-media
```

Regenerate the human-readable attribution file after changing a source record:

```sh
python3 tools/generate_image_attribution.py
```

No normal build or test target downloads these files. Network acquisition must
remain an explicit operation that checks Commons metadata drift before
accepting bytes.

## Prepared inputs

The initial 8-bit preparation profile produces two variants per Commons source:

- `le896`: exact 896-pixel long edge;
- `le1440`: exact 1440-pixel long edge.

Both values are multiples of 64, above 800 and below 1500. The short edge is
rounded to the nearest even integer so I420 geometry remains conventional.
No image is upscaled. Every prepared image uses less than 50% of its source's
pixel area. Measured linearly, two 1440 variants cannot stay below 50%:
`finland-warbirds` is 56.25% and `scarlet-flycatcher` is 50.56%. All other
variants, including every `le896` variant, are below 50% linearly.

| Source ID | `le896` | `le1440` |
| --- | ---: | ---: |
| `baya-weaver` | 896x598 | 1440x960 |
| `chinnathurai-church` | 896x896 | 1440x1440 |
| `elbrus-milky-way` | 598x896 | 960x1440 |
| `emerald-swift` | 896x448 | 1440x720 |
| `finland-warbirds` | 896x504 | 1440x810 |
| `freiburg-minster-columns` | 598x896 | 960x1440 |
| `hong-kong-harbour-night` | 896x424 | 1440x680 |
| `jinan-grand-theater` | 896x672 | 1440x1080 |
| `pastel-chalks` | 896x598 | 1440x960 |
| `pitti-ceiling` | 896x896 | 1440x1440 |
| `rome-ponte-blue-hour` | 896x598 | 1440x960 |
| `scarlet-flycatcher` | 896x598 | 1440x960 |
| `succisa-flower-bud` | 896x896 | 1440x1440 |
| `toco-toucan` | 896x598 | 1440x960 |

Each preparation directory contains:

- an oriented, resized, metadata-stripped 8-bit sRGB `master.png` without
  alpha;
- full-range BT.709 `gray8` I400, `yuv420p8` I420, and `yuv444p8` I444 raw
  planes;
- `manifest.json`, which records the source hash, dimensions, scale, tool
  versions, normalized commands, and output hashes.

Generate all 28 preparations and refresh `recipes/prepared-lock.json` with:

```sh
python3 tools/prepare_image_corpus.py
```

Selective generation is available with `--source SOURCE_ID` and/or repeated
`--long-edge 896` or `--long-edge 1440` options. Selective generation does not
rewrite the complete lock.

Verify the lock and any locally present outputs using only the Python standard
library:

```sh
python3 tools/verify_prepared_images.py
python3 tools/verify_prepared_images.py --require-files
```

The current files were generated with ImageMagick 7.1.2-18 Q16 and FFmpeg
8.0.1-3ubuntu2. Selective regeneration has been byte-stable in this Linux
environment. Cross-platform byte identity has not yet been established, so
these outputs are the current locked preparation set rather than proof that
arbitrary ImageMagick/FFmpeg builds reproduce it exactly.

## Licensing

The repository root CC0 dedication does not apply to these photographs or to
their derivatives. The selected licenses for this batch are:

- Creative Commons Attribution 4.0 International (`CC-BY-4.0`): 2 sources;
- Creative Commons Attribution-ShareAlike 4.0 International
  (`CC-BY-SA-4.0`): 11 sources;
- Creative Commons Attribution-ShareAlike 2.0 Generic (`CC-BY-SA-2.0`):
  1 source.

These names matter. The 2.0 aircraft license is the unported **Generic**
license, not a jurisdiction-specific 2.0 port. The 4.0 licenses are
**International** editions. See `licenses/README.md` for canonical URLs and
hashes of the local copies, and `ATTRIBUTION.md` for per-work credit.

Prepared images, raw planes, encoded files, previews, and decoded references
are modifications or reproductions of the source works. Distributions must
include the applicable credit, source and license links, local legal code, and
a description of changes. ShareAlike derivatives must use the applicable same
or compatible license as required by that license edition.

Commons JPEGs remain 8-bit sources. Converting them to a 10-bit pixel format
can test 10-bit storage and decoding, but does not create genuine 10-bit source
precision.