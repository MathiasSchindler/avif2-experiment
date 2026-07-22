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
- Milestone 5 is in progress: its freestanding entropy decoder and private
  Closed Loop Key frame parser pass pinned-AVM oracle checks and derive all four
  tile entropy boundaries from syntax. First blocks for all fixtures and the
  tiny fixture's remaining ten luma blocks reconstruct sample-exactly in
  private tests. Production derives the 4x4 D203 predictors from reconstructed left
  samples, the 8x4 and 4x4 D45 predictors from reconstructed above samples,
  the 4x4 Paeth predictor from live above, left, and top-left samples, and the
  8x8 and 4x4 D67 predictors from live MRL edges, including MRL index 3, and
  the final 4x4 smooth predictor from live top and left edges. All four tiny
  chroma leaves decode their UV modes, transforms, and 4x4 U and V blocks
  sample-exactly through tell 725. Bounded caller-owned partition, luma-mode,
  and packed per-plane coefficient maps now derive every later tiny-fixture
  neighbor context. A bounded depth-first partition traversal yields and
  commits all tiny luma and chroma leaves in entropy order. A private bounded
  tile orchestrator now owns the range decoder, adaptive CDFs, both partition
  traversals, neighbor maps, and colocated luma modes; it decodes all 15 tiny
  leaves through tell 725 without fixture-supplied contexts. A private bounded
  pipeline executes deblock, CDEF, CCSO, and accepted-profile GDF from
  caller-owned planes and metadata; restoration remains unsupported and
  fail-closed. General reconstruction wiring, larger-fixture reconstruction,
  filtered fixture parity, final workspace sizing, and the public decode API
  are not implemented yet.

The public API is declared in `include/av2heif.h`. Milestone details and
limitations are recorded in `docs/milestone-0.md`, `docs/milestone-1.md`, and
`docs/milestone-2.md`, `docs/milestone-3.md`, `docs/milestone-4.md`, and
`docs/milestone-5.md`; the longer roadmap is in `plan.md`.

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