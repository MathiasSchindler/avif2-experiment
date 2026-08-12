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
  contracts. They are intentionally empty until those pipelines are fixed.
- `prepared/`, `reference/`, and `reports/` are working/output areas for later
  stages. Bulk files in those directories remain ignored.
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