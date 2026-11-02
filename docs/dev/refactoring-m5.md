# Refactoring M5: codec syntax and container carriage

Completed 2 November 2026.

## Ownership

The former 1,121-line carriage and 1,201-line container owners are now:

| Owner | Lines | Responsibility |
| --- | ---: | --- |
| [`decoder/sequence.c`](../../src/decoder/sequence.c) | 450 | Sequence information/state parsing, tool flags, segmentation, dimensions, and sequence tile signaling |
| [`decoder/obu.c`](../../src/decoder/obu.c) | 183 | Minimal LEB128 reading/writing, OBU framing, layer/header validation, and ledger construction |
| [`decoder/content_interpretation.c`](../../src/decoder/content_interpretation.c) | 141 | Default color information, Rice-Golomb identifiers, color/chroma-position syntax, and trailing-bit validation |
| [`carriage.c`](../../src/carriage.c) | 295 | Configuration-box parsing/writing, sequence consistency, configuration/item OBU ordering, and metadata validation |
| [`container.c`](../../src/container.c) | 951 | Canonical HEIF reading, dialect probing, and shared container interpretation rules |
| [`container_write.c`](../../src/container_write.c) | 224 | Deterministic canonical HEIF sizing/writing and the existing post-write parse/self-check |

Sequence syntax now lives alongside
[`sequence.h`](../../src/decoder/sequence.h), not in carriage code.
The existing OBU error/trailing-bit helpers are shared in
[`decoder/internal/obu.h`](../../src/decoder/internal/obu.h), without duplicating
their bodies or changing their initialization policy.

The private [`container.h`](../../src/container.h) owns the unchanged probe
record/declaration and the constants, pixel metadata, and interpretation helpers
shared by reading and writing. Probe declarations no longer live in
[`base.h`](../../src/base.h).
The metadata-validation declaration also moved to its own
[`carriage.h`](../../src/carriage.h).

Reading and probing retain their existing shared BMFF reader, property
association parsing, and item/extent rules. The writer uses the reader's single
pixel-metadata and color-comparison implementations; it does not duplicate
canonical container policy or introduce another box parser.

Root [`query.c`](../../src/query.c) only gains the explicit container include.
Public query/decode coordination and all public parsing signatures are
unchanged. No workspace/value type was redesigned.

All four new translation units are registered in the authoritative
[`CORE_SOURCES`](../../Makefile). Both laboratory provenance lists now
fingerprint the moved owners and their private interfaces:
[`run_artifact_lab.py`](../../tools/run_artifact_lab.py) and
[`run_m3_lab.py`](../../tools/run_m3_lab.py).

## Preservation evidence

A pre-extraction source comparison verified all **57 original function bodies**
after normalizing the shared helper names. The only other function changes are
helper placement/linkage: OBU-local helpers are private inline definitions, and
three existing container helpers have private cross-translation-unit
declarations.

This preserves, in particular:

- Minimal LEB128 encoding, the eight-byte limit, writer overflow behavior,
  OBU length/header validation order, layer-ID policy, and partial ledger writes.
- Sequence bit consumption, tool/segment/tile arithmetic, supported tuples,
  trailing bits, diagnostic syntax/value selection, and partial state on errors.
- Content defaults, reserved identifiers, Rice-Golomb limits, chroma positions,
  unsupported aspect/timing behavior, and trailing-bit rejection.
- Configuration/header consistency, extra-OBU rejection, error offsets/indexes,
  and configuration-versus-item metadata precedence.
- Canonical box/property/association order, strict parsing versus permissive
  probing, dialect classification, and item extent offsets.
- Writer bytes, sizing, short-buffer nonmutation, output tails, and its existing
  post-write unsupported-format handling.
- Pixels, resource layouts, public interfaces, fixtures, reference locks, and
  both oracle revisions.

This is an ownership extraction, not additional HEIF/OBU admission or an
error-initialization cleanup. The existing difference between strict reading and
dialect probing remains deliberate.

## Regression coverage

[`carriage_refactor_unit.c`](../../tests/carriage_refactor_unit.c) freezes
**52,630 scenario groups**, measured and checked against the original owners
before extraction, in both diagnostic configurations:

| Group | Cases | Frozen hash |
| --- | ---: | --- |
| LEB128 | 6,144 | `1ab561e3a7fc4825` |
| OBU framing/ledger | 24,576 | `33c82387f94cd6c5` |
| Sequence state/public information | 591 | `11dd8e51712eb827` |
| Content interpretation | 1,024 | `1986dc98fde279c5` |
| Configuration | 1,221 | `930c0e080d97ed0c` |
| Container/probe/dialect/writer | 19,074 | `ee7fea65ce3bd619` |

The framing matrix covers all first-header bytes, selected extended-layer
bytes, truncations, and ledger capacities 0/1/2. Snapshots include all ledger
fields, including partially written and untouched entries.

Sequence snapshots serialize every state scalar and segment/tile array
explicitly, along with public sequence information and errors. Inputs reuse the
existing minimal I420 test stream and four retained tiny fixture streams
(8-bit I400/I444 and 10-bit I420/I444), plus a content-interpretation variant.
Each sequence payload is truncated at every byte and has every bit flipped
individually.

Configuration and container cases cover all byte-prefix truncations and
single-bit mutations, serialized writer output, span offsets, parsed metadata,
probe state, statuses, and error precedence. HEIF writer cases additionally
freeze short/exact/oversized output behavior and untouched tails.
Scalar hashing is explicit-endian and excludes pointers/padding.

Errors begin with nonzero sentinel fields to expose partial updates.
Dialect snapshots intentionally cover only the core error fields: forwarding a
local strict-parse error does not consistently initialize extended fields yet.
M7, rather than this extraction, owns error-record standardization.

These are fixed implementation characterizations, not new unsupported-feature
witnesses or AVM conformance claims.
Existing [`substrate_unit.c`](../../tests/substrate_unit.c),
[`container_unit.c`](../../tests/container_unit.c), and
[`public_decode_unit.c`](../../tests/public_decode_unit.c) remain unchanged,
including golden deterministic bytes, metadata consistency, legacy/conflicting
dialects, reordered media, limits, and decode checks.

All **97,908** prediction calls, **21,600** M3 plane characterizations, and M4
frame/storage/metadata snapshots retain their original expected values.

## Verified gates

Host: macOS ARM64, strict C11 warnings, Clang 22.1.8, ASan/UBSan.
Diagnostics-disabled means the macro is absent.

| Gate | Result |
| --- | --- |
| Full `test-c`, diagnostics absent and enabled | Both pass: 29 host units, runtime unit, 2,460 malformed cases, output-policy and six SHA-256 checks |
| New private interfaces | All three compile independently with freestanding headers |
| Frozen M0 inventory and resource/layout checks | Inventories byte-identical to M0 and between both configurations |
| Spatial admission, both configurations | 16 exact fixtures, eight source-bound witnesses, and 11,850 malformed cases pass per configuration |
| Native photographic parity | 88 legacy and 28 AVM-default references exact |
| Native filter parity | Tiny, odd, ordinary, and high-detail Y/U/V exact |
| Reference locks | 130 valid records; 116 local manifests present |
| Freestanding symbols and source-table evidence | Fresh core and complete evidence gate pass |
| WebAssembly | Fresh wasm32 module; all 16 spatial fixtures exact under Node |
| Windows x86-64 and ARM64 | Fresh cross-builds; decoder/smoke imports remain exactly 10/1, KERNEL32.dll only |
| Existing current-oracle Baya replay | Four cases pass in both existing-stream and fresh-encode modes; no errors, skips, or partial results |
| Laboratory provenance | Pylance syntax checks pass; all 14 recorded source paths resolve in each list and include every moved owner/interface |

The oracle was not fetched, rebuilt, or advanced. Current remains
`edaf3ebc7ce3666290003f70a66efe5a508982a1`; pinned remains
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.

### Reproduction

Use fresh build directories when changing flags or headers. Run spatial
configurations sequentially because generated paths are shared.

```sh
make -j4 HOST_BUILD_DIR=build/refactor-m5-diag-absent \
  NATIVE_BUILD_DIR=build/refactor-m5-native \
  test-c spatial-still-admission test-freestanding test-decoder-evidence
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m5-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission
make test-reference-locks
python3 experimental/wasm/build.py --output-directory build/refactor-m5-wasm
node tools/validate_spatial_still_wasm.js build/refactor-m5-wasm/decoder.wasm
python3 tools/check_avm_current.py \
  --decoder build/refactor-m5-diag-absent/av2heif-decoder \
  --suite photographic-8bit --source baya-weaver --preparation le896 \
  --mode both --output-dir build/refactor-m5-current-baya
```

The oracle output directory must be new. Photographic, filter, Windows, and
inventory commands follow [M2's reproduction](refactoring-m2.md#reproduction),
substituting the `refactor-m5-*` build directories.

## Incomplete gates and limits

The following gates were rerun and retain the same missing-media blockers:

- Aggregate `make test`: C checks pass, then corpus parity cannot read
  `images/artifacts/chimp-93x111-qp064.obu`; the 31 legacy corpus OBUs are absent.
- Python discovery: 75 of 76 tests pass; one errors because
  `images/artifacts/emerald-192x96-qp208-decoded.i420` is absent.
- Target-baseline parity: all 14 local manifests are absent; the first failure
  is Baya I420 QP152.
- Full 10-bit Wasm manifest validation: absent
  `images/artifacts/10bit/avm-reference/manifest.json`.

These are not passing aggregate gates. No fixtures were regenerated and no
checks were weakened or silently skipped.

Full photographic laboratory regeneration, Windows execution, Linux no-libc
execution, browser runtime, maximum-image decoding, peak-stack measurement, and
native 32-bit layout validation remain untested. Source identities, parser
characterizations, and cross-builds do not establish those claims.
