# Milestone 9: mandatory filters and filtered parity

Milestone 9 is planned. Focused private deblock, CDEF, CCSO, and GDF kernels
already pass sanitizer vectors, but no fixture-level filtered parity is claimed.

## Completed isolated preparation

The private filter surface has received an isolated hardening pass that does
not depend on Milestone 7 traversal work:

- CDEF now applies the normative signed update
  `(8 + sum - (sum < 0)) >> 4` without relying on implementation-defined
  signed right shift. Positive and negative rounding vectors are frozen
  against the pinned AVM v1.0.0 `cdef_block.c` behavior.
- CDEF rejects out-of-profile primary, secondary, damping, and encoded frame
  strengths before destination mutation.
- CCSO validates the exact parser-produced parameter shape, including boolean
  fields, band-only versus edge-classifier fields, maximum band count,
  `offset_count`, and binary per-unit controls. Invalid state fails before
  plane mutation.
- The frame orchestrator rejects CDEF or CCSO frame state when the
  corresponding Sequence Header tool is disabled.
- GDF rejects overlapping destination, CDEF-source, or immutable
  deblocked-source storage while accepting non-overlapping padded strides.
- Closed-key Frame Headers derive the GDF block count from the Sequence Header
  unit-size flag and frame dimensions, read the mode-2 bit only for
  multi-block frames, and retain picture QP and scale indices. All 21 artifact
  streams match the pinned AVM `uncompressedHeaderBits` endpoint.
- GDF mode 2 now initializes a private adaptive entropy state from the pinned
  binary CDF and stores flags in caller-backed row-major memory. The symbol is
  scheduled after intraBC use/skip syntax and before CDEF/CCSO, including when
  intraBC is selected.
- All six mode-2 artifacts decode their first flag at the exact pinned AVM
  endpoint:

  | Stream | First block | Before | Flag | After |
  | --- | --- | ---: | ---: | ---: |
  | `emerald-480x240-qp128` | 64x64 | 2 | 1 | 3 |
  | `flycatcher-397x265-qp048` | 32x32 | 7 | 0 | 8 |
  | `laarim-256x171-qp096` | 32x32 | 7 | 1 | 8 |
  | `pastell-320x213-qp064` | 8x8 | 17 | 0 | 18 |
  | `schwaebisch-353x234-qp112` | 32x8 | 12 | 1 | 13 |
  | `silene-479x383-qp176` | 64x64 | 2 | 1 | 3 |

  The first five then reject at unsupported multi-strength CDEF state;
  `silene-479x383-qp176` rejects at screen-content handling.
- A checked private workspace query derives coded Y/U/V dimensions, two
  complete stage-plane sets, per-4x4 deblock-cell counts, per-64x64 CCSO
  control counts, total bytes, and required alignment. The matching initializer
  binds aligned caller-owned memory and returns writable map storage without
  exposing a public decode API.
- A synthetic full-pipeline vector compares the orchestrator with explicit
  deblock, CDEF, CCSO, and GDF calls using independent stage planes.
- Focused vectors now cover skipped non-block transform edges, all-lossless
  edges, one-sided lossless filtering, the asymmetric deblock length used at
  horizontal superblock boundaries, CCSO across multiple 64x64 units with
  chroma subsampling, and immutable GDF stripe-boundary sources.
- `tools/run_m9_filter_evidence.py` records externally produced I420 stage
  dumps with per-file and per-plane hashes, validates manifest shape and
  geometry fail-closed, and reports the first mismatching filter stage. It
  consumes dumps only and does not invoke or modify AVM.

`tests/filter_unit.c` covers the normative CDEF rounding boundary, invalid
CDEF strengths, malformed CCSO parameter and control state, Sequence
Header/frame inconsistencies, GDF aliasing, and output preservation on each
new failure path. It also covers the private workspace model, complete
synthetic pipeline order, deblock scheduling, multi-unit CCSO, and GDF stripe
boundaries. `tests/test_m9_filter_evidence.py` covers stage-local comparison,
first-mismatch ownership, source-stream binding, and malformed-manifest
rejection. The focused hosted sanitizer binary and strict freestanding
zero-undefined-symbol core pass.

The isolated filter-kernel preparation is limited to `src/decoder/filter.c`,
`src/decoder/filter.h`, `tests/filter_unit.c`,
`tools/run_m9_filter_evidence.py`, `tests/test_m9_filter_evidence.py`,
`plan.md`, and this document. The later GDF entropy checkpoint separately
extends frame, leaf, and tile state plus their focused and artifact tests; it
does not modify coefficient, transform, or reconstruction behavior.

## Hosted evidence interface

Record any subset of ordered stages with explicit coded or visible geometry:

```sh
python3 tools/run_m9_filter_evidence.py record \
  --fixture odd \
  --producer "pinned AVM v1.0.0" \
  --source-obu build/m0/matrix/odd/original.obu \
  --stage reconstructed build/m9/odd/reconstructed.i420 24 16 \
  --stage deblocked build/m9/odd/deblocked.i420 24 16 \
  --stage filtered build/m9/odd/filtered.i420 17 15 \
  --output build/m9/odd/reference.json
```

Compare a production manifest with the reference:

```sh
python3 tools/run_m9_filter_evidence.py compare \
  build/m9/odd/reference.json \
  build/m9/odd/production.json \
  --output build/m9/odd/comparison.json
```

Comparison requires the same fixture, the same source OBU when one is
recorded, the same ordered stage set, and valid I420 geometry. It exits
nonzero and names the first mismatching stage when file or Y/U/V plane hashes
differ.

## Scope

- populate complete deblock and CCSO metadata from committed leaves;
- run deblock, CDEF, CCSO, and GDF in normative order over coded planes;
- preserve immutable source planes where a later filter classifies against an
  earlier stage;
- compare stage-local hashes and samples against pinned AVM evidence; and
- crop coded planes to display dimensions only at the defined output boundary.

Restoration remains unsupported for the accepted profile and must reject before
final-plane mutation.

## Remaining integration work

The following work still depends on complete Milestones 7 and 8 syntax,
reconstruction, and traversal:

- extend tile traversal beyond its current first 64x64 root so every GDF
  mode-2 block flag is decoded at its row-major superblock entropy position;
- populate deblock cells and CCSO control maps from every committed luma and
  chroma leaf;
- connect the prepared workspace binding to the post-Milestone-8 frame
  orchestrator;
- freeze pinned AVM hashes or samples for deblock, CDEF, CCSO, GDF, and final
  output stages;
- run the complete filter order over tiny, odd, ordinary, and high-detail
  coded planes; and
- crop only after final filtered parity has been established.

The current focused vectors prove kernel behavior and fail-closed validation;
they do not claim fixture-level filter orchestration or filtered parity. A
decoded-count guard prevents the current partial traversal from entering
chroma while frame-wide GDF flags remain unread.

## Exit gate

Tiny, odd, ordinary, and high-detail fixtures must match pinned AVM
sample-exactly on final filtered Y, U, and V planes. Stage-local checks must
identify deblock, CDEF, CCSO, or GDF as the owning layer for any mismatch.

Milestone 9 does not expose a public decoder.