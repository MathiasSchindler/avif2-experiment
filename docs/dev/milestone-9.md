# Milestone 9: mandatory filters and filtered parity

Milestone 9 is complete. The private frame path and focused deblock, CDEF,
CCSO, and GDF kernels pass the milestone gate, including fixture-level final
filtered parity.

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

  CDEF multiple-strength entropy is now implemented immediately after GDF.
  It uses the four pinned index-zero contexts, all six nonzero-remainder CDFs
  for frame strength counts 3 through 8, caller-backed 64x64 unit storage,
  first-eligible-block transmission, and the frame's skip-transform policy:

  | Stream | Strengths | Before | Index | After |
  | --- | ---: | ---: | ---: | ---: |
  | `emerald-480x240-qp128` | 3 | 3 | 1 | 4 |
  | `flycatcher-397x265-qp048` | 8 | 8 | 6 | 11 |
  | `laarim-256x171-qp096` | 3 | 8 | 0 | 11 |
  | `pastell-320x213-qp064` | 8 | 18 | 3 | 21 |
  | `schwaebisch-353x234-qp112` | 6 | 13 | 5 | 16 |
  | `silene-479x383-qp176` | 1 | 3 | 0 (implicit) | 3 |

  CCSO control entropy now follows CDEF in pinned AVM order, including the
  intraBC/screen-content path. It uses caller-backed per-plane maps, the four
  neighbor contexts, read-once sentinels, and the Sequence Header-selected
  64/128/256-pixel unit size. These artifact streams use 256x256 units:

  | Stream | Enabled | Controls Y/U/V | Before | After |
  | --- | --- | --- | ---: | ---: |
  | `emerald-480x240-qp128` | Y/U/V | 1/1/1 | 4 | 9 |
  | `flycatcher-397x265-qp048` | U/V | -/0/0 | 11 | 12 |
  | `laarim-256x171-qp096` | U | -/1/- | 11 | 13 |
  | `pastell-320x213-qp064` | U/V | -/1/1 | 21 | 24 |
  | `schwaebisch-353x234-qp112` | Y/U/V | 1/1/0 | 16 | 20 |
  | `silene-479x383-qp176` | Y/U/V | 1/1/1 | 3 | 8 |

  The decoder now continues beyond this table through exact mode syntax.
  Blocks larger than 32 pixels use implicit FSC=0 rather than indexing a
  nonexistent context-6 CDF row. The reached screen-content streams decode the
  pinned Palette=0 symbol; nonzero palettes remain unsupported. An all-units
  guard prevents transition to chroma while any enabled CCSO unit remains
  untransmitted.
- A checked private workspace query derives coded Y/U/V dimensions, two
  complete stage-plane sets, per-4x4 deblock-cell counts, CCSO control counts
  from the parsed 64/128/256-pixel unit geometry (with a 64-pixel legacy
  default for synthetic states), total bytes, and required alignment. The matching
  initializer binds aligned caller-owned memory and returns writable map
  storage without exposing a public decode API.
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
`docs/history/plan.md`, and this document. The later GDF entropy checkpoint separately
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

## Completed integration

`av2h_decode_frame_i420_private()` now owns the complete bounded handoff. It
populates every luma and chroma deblock cell while committing leaves, uses the
decoded frame-wide CDEF strength-index and CCSO unit-control maps, runs the
four mandatory filters in normative order, and copies only the visible crop
after final filtering. Multi-strength CDEF and frame-wide GDF/CCSO traversal
are accepted by this path. Restoration rejects before filter or output
mutation.

The locked fixture final I420 hashes from pinned AVM v1.0.0 are:

| Fixture | Dimensions | SHA-256 |
| --- | --- | --- |
| tiny | 16x16 | `5c18646460bb9f13c53baa6e65861a5cc746706a173e9c4f9b1cdeda86fd7ab5` |
| odd | 17x15 | `74b176943ef8760c8971600c4e6ec25d6e3b6a4b21ffc3a55df845a41bdef9af` |
| ordinary | 160x120 | `def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5` |
| high-detail | 96x80 | `4120930c9c9c49f73d4aa3e1d4c56cbb4b557679b42fe3b9de984bca1431745a` |

`tools/run_m9_final_parity.py` reruns all four locked OBUs through the
production decoder and pins the final I420 plus each Y, U, and V plane
independently. The hosted evidence tool and its Python tests retain ordered
stage-local diagnosis. `make test-filter-parity` runs the fixture, filter,
evidence, and strict freestanding gates; Windows CTest includes the same final
parity runner when Python is available.

## Exit gate

Tiny, odd, ordinary, and high-detail fixtures match pinned AVM sample-exactly
on final filtered Y, U, and V planes. Stage-local checks identify deblock,
CDEF, CCSO, or GDF as the owning layer for any mismatch.

Milestone 9 does not expose a public decoder.