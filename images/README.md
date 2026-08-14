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
  contracts plus the tracked prepared-output and reference locks.
- `prepared/` contains generated RGB masters, planar encoder inputs, and
  per-preparation manifests. It is ignored by Git and can be regenerated.
- `reference/` contains pinned-AVM encoded OBUs, decoded planes, and case
  manifests. Its bulk files remain ignored. `reports/` is reserved for later
  analysis.
- `old/` preserves the previous layout during migration and must not be used as
  a provenance source for new releases.
- [plan.md](plan.md) defines the full preparation, AVM reference, validation, and
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

## AVM references

Both reference suites are tied to AVM revision
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. The original
`photographic-8bit` suite contains 88 one-frame cases:

- I420 at QP 96 and 152 for all 28 prepared images (56 cases);
- I400 and I444 at QP 152 for all 14 `le896` preparations (28 cases);
- I420 at QP 0, 48, 208, and 255 for `pastel-chalks/le896` (4 cases).

QP 96 provides the higher-quality baseline requested around 90. QP 152 is the
main compressed reference around 140-160 and also anchors format comparisons.
The Pastel spectrum exercises the lossless endpoint, an intermediate point,
strong compression, and the maximum QP without multiplying every image by six
settings.

Each case lives under
`reference/avm-<revision>/photographic-8bit/<case-id>/` and contains
`encoded.obu`, a normative `decoded.i400`, `decoded.i420`, or `decoded.i444`,
plus a byte-identical `decoded.yuv` alias, an 8-bit RGB `decoded.png` preview,
and `manifest.json`. The extension-specific planar file remains normative;
`.yuv` is a convenience alias because that extension does not identify a pixel
layout. Both supplemental files are generated and locked for every case.

Generate the complete matrix and refresh `recipes/reference-lock.json` with:

```sh
python3 tools/generate_image_references.py --threads 24 --jobs 8
```

Every `avmenc` invocation receives all 24 detected logical cores and row
multithreading. Eight independent still-image cases run concurrently because a
single encode does not saturate this host. Filters `--source`, `--preparation`,
`--pixel-format`, and `--qp` are available for probes; a filtered run does not
rewrite the complete lock. `--resume` reuses existing case manifests during a
full interrupted run.

Verify the legacy suite's tracked lock alone, or require and hash every ignored
local file:

```sh
python3 tools/verify_image_references.py
python3 tools/verify_image_references.py --require-files
```

The separate `target-baseline-8bit` suite contains one `le896` I420 case at QP
152 for each of the 14 photographs. It uses the high-effort target profile
proven by `pastel-chalks`: `cpu-used=1`, 16 encoder threads, Intra-DIP,
IntraBC, TCQ, trellis quantization, full restoration search, and perceptual
delta-Q mode 2. Parity hiding, quantization matrices, variance AQ, and extended
segmentation are explicitly disabled. AQ, delta-Q, and quantization matrices
remain competing profile choices rather than being stacked.

Each target case lives under
`reference/avm-<revision>/target-baseline-8bit/<case-id>/` and contains
`encoded.obu`, the pinned-AVM `decoded.i420`, and `manifest.json`. It does not
duplicate the `.yuv` alias or PNG preview from the broad legacy suite. Generate
or resume the target matrix and refresh its independent lock with:

```sh
python3 tools/generate_image_references.py --suite target-baseline-8bit --resume
```

The target suite defaults to two concurrent cases and 16 threads per encoder;
these values can still be overridden for an explicit experiment. Verify its
tracked lock alone, or require every local payload, with:

```sh
python3 tools/verify_image_references.py --suite target-baseline-8bit
python3 tools/verify_image_references.py --suite target-baseline-8bit --require-files
```

The two suites have separate lock files, so generating or filtering the target
suite cannot overwrite the original `reference-lock.json`. `--suite` is also
accepted together with the existing source, preparation, pixel-format, and QP
filters; filtered runs do not rewrite either complete lock.

The permanent repository gates cover both suites. Lock validation does not
require ignored payloads; native and WebAssembly parity do:

```sh
make test-reference-locks
make test-reference-images
make test-reference-images-wasm
```

The equivalent direct native command accepts an alternate decoder and job
count:

```sh
python3 tools/verify_image_references.py --all-suites --require-files \
  --decoder build/host-sanitized/av2heif-decoder --jobs 8
```

The lock pins the AVM source revision plus SHA-256 identities of `avmenc` and
`avmdec`. Each manifest records normalized commands, the exact prepared input,
the parsed OBU ledger, and all output hashes. The current payload is
318,088,662 bytes (313 MiB on disk). Regenerating the representative Pitti case
was byte-identical for its OBU, decoded files, PNG, and manifest on this Linux
host. This is measured local reproducibility, not yet a cross-platform claim.

### Browser gallery

Publish the reference gallery bundle after generating or updating the local
reference corpus with:

```sh
make reference-gallery
```

This verifies and copies all 88 encoded OBUs, the 14 reviewed `source.json`
records, and their three legal-code files into `docs/reference/`, then writes
the aggregate manifest consumed by `docs/gallery.html`. The published OBU
payload is 4.91 MiB; decoded planes and PNG previews are not duplicated because
the gallery renders the streams through `docs/decoder.wasm`.

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