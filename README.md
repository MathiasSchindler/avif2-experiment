# AV2-HEIF experiment

This repository explores a project-defined AV2 still-image carriage in HEIF.
It does not define or claim a standardized AVIF successor.

The implemented core is CC0, freestanding C11, allocation-free, and does not
depend on libc or vendored codecs. Hosted AVM tooling is used only for the
separate reference experiment and is fetched into ignored `vendor/` and
`build/` paths.

## Current status

- Milestone 0 proves sample-exact Sequence Header normalization with four AVM
  v1.0.0 fixtures.
- Milestone 1 provides checked readers, writers, arenas, AV2 OBU framing,
  Sequence Header and Content Interpretation parsing, nine-byte `av2C`, strict
  v0 carriage validation, and checked BMFF box headers.
- Milestone 2 provides a strict zero-copy typed parser and deterministic writer
  for one primary `av02` item, with byte-exact output and mutation coverage.
- Milestone 3 provides an offline hosted artifact laboratory that generates,
  muxes, inspects, extracts, decodes, and compares the four-fixture matrix.
- Milestone 4 provides bounded freestanding validation, structural dialect
  detection, metadata query, exact I420 output sizing, and input binding.
- Milestone 5 is implemented: the private freestanding decoder reconstructs all
  eleven tiny luma and four tiny chroma leaves sample-exactly through tell 725
  with caller-owned traversal, context, scratch, and live-plane state. Focused
  deblock, CDEF, CCSO, and GDF kernels pass; restoration remains unsupported
  and fail-closed.
- Milestone 6 is implemented: caller-owned coded Y/U/V planes distinguish
  visible reference bounds from padded storage, retain per-4x4 reconstruction
  readiness, and reconstruct all 12 odd luma and five odd chroma leaves
  sample-exactly through tell 696 before filtering.
- Milestone 7 is in progress through six committed ordinary luma leaves and
  currently stops at `(4,14)`, 4x8 DC, TX_4X8, EOB 29, primary
  `FLIPADST_ADST`, before coefficient scanning.
- Milestone 8 owns high-detail, CFL, intraBC, and remaining advanced transform
  coverage. Milestone 9 owns mandatory-filter integration and filtered parity.
  Milestone 10 alone owns workspace finalization and public decoder exposure.

The public API is declared in `include/av2heif.h`. Milestone details and
limitations are recorded in `docs/milestone-0.md` through
`docs/milestone-10.md`; the longer roadmap is in `plan.md`.

## Build and test

Run the vendor-free gate with:

```sh
make test
```

This runs the C harnesses under AddressSanitizer and UndefinedBehaviorSanitizer,
builds the core with `-ffreestanding -nostdinc`, verifies that the linked
relocatable core has no undefined symbols, and runs the Python carriage tests.
The C gate has separate substrate/carriage, container, AV2 range-decoder,
CDF-initialization, partition-prefix, frame-header, and tile-framing test
binaries.

Run the hosted Milestone 5 normative and pinned-AVM correspondence checks with:

```sh
make milestone-5-evidence
```

Run the hosted AVM differential experiment with:

```sh
make milestone-0
```

That target downloads or verifies the pinned AVM checkout and is intentionally
outside the production dependency boundary.

After the pinned AVM build is available, run the complete artifact laboratory
without network access with:

```sh
make milestone-3
```

The machine-readable result is `build/m3/laboratory/manifest.json`.

Run the Milestone 4 query and fixture-parity gate with:

```sh
make milestone-4
```