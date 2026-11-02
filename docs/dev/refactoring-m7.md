# Refactoring M7: standardize error construction

Completed 2 November 2026.

## Ownership and preserved policies

[`error.h`](../../src/error.h) provides two private, null-safe inline helpers:

| Helper | Writes | Preserves |
| --- | --- | --- |
| `av2h_error_set_core` | Status, offset, context, unknown OBU index, zero OBU type | Extended diagnostics |
| `av2h_error_reset_diagnostics` | Caller-supplied diagnostic stage/reason and zeroed syntax, plane, row, column, entropy tell, parsed value | Core fields |

The core helper returns the supplied status even without an error record.
Neither helper clears the whole object, interprets a status, or owns state.

The existing local constructors in [`bmff.c`](../../src/bmff.c),
[`container.c`](../../src/container.c), [`query.c`](../../src/query.c),
[`output_layout.c`](../../src/output_layout.c),
[`decode.c`](../../src/decode.c),
[`internal/obu.h`](../../src/decoder/internal/obu.h), and
[`frame.c`](../../src/decoder/frame.c) use the shared initialization.
Offsets, OBU identity overrides, syntax values, and stage interpretation remain
with their existing owners.

Important differences are deliberately retained:

- Container/BMFF/query/layout failures update only the five core fields.
- Decode/OBU failures reset extended diagnostics; their status-to-reason maps
  remain distinct. Decode resource failures retain the resource reason.
- Frame-header failures retain their own reason mapping, bit-derived offset,
  entropy position, closed-key OBU type, and syntax details.
- Public decode still rejects an error record overlapping workspace/output
  before clearing it. Validation precedence and failed-output nonmutation
  remain unchanged.
- Decode success still clears diagnostics, reports completion, and preserves
  the previously untouched core fields. Query success is not normalized.

M4's result/context helpers in
[`frame_output.c`](../../src/decoder/frame_output.c) are byte-identical to their
pre-M7 source. Root decode continues projecting those results; there is no
second frame diagnostic mechanism or new global state.

Both artifact-laboratory provenance lists now include the new header:
[`run_artifact_lab.py`](../../tools/run_artifact_lab.py) and
[`run_m3_lab.py`](../../tools/run_m3_lab.py).

## Persistent characterization

[`error_refactor_unit.c`](../../tests/error_refactor_unit.c) was first run
against the unchanged pre-M7 implementation. All frozen expectations remain:

| Group | Scenario groups | Frozen hash |
| --- | ---: | --- |
| Container/BMFF/query/layout | 22,188 | `f0c503598e08b4d9` |
| OBU/sequence/frame syntax | 21,534 | `32793b145c5456fd` |
| Public errors/output, diagnostics absent | 156 | `e21571ba9b343da5` |
| Public errors/output, diagnostics enabled | 156 | `33dcc7194e86770d` |

Each configuration checks **43,878** scenario groups. The records serialize
the returned status and all 13 scalar error fields individually, excluding
padding and absolute pointers. Unknown OBU indexes are normalized for hashing.

Coverage includes truncation and bit mutations, caller-seeded diagnostics,
query/layout failures, decode binding/resource/output errors, overlap
rejections, and retained-frame entropy variants. Failed decodes additionally
check output nonmutation; pre-decode validation failures also check workspace
nonmutation.

An additional **2,316** direct helper cases use independently assigned expected
records, including preserved fields/padding and null error pointers:

- **1,920** core cases: every status, contexts 0-18 and `UINT32_MAX`, four
  offset boundaries through `SIZE_MAX`, and three seed variants.
- **396** diagnostic-reset cases: every stage/reason plus `UINT8_MAX`, with
  three seed variants.

The unit is registered in the existing host unit list and runs in `test-c`.

## Differential preservation evidence

Temporary test translation units compiled the original and new constructors
side by side under ASan/UBSan, with renamed exports and no production hooks.
Each diagnostic configuration passed **84,144** returned-status/whole-record
comparisons:

- Every frame-result stage and an unknown stage, with returned status varied
  independently of result cause.
- Leaf/coefficient interpretation, luma/chroma reconstruction, cross-transform,
  filter and output projection, all three planes, and zero/boundary details.
- Frame-reader null/bit-boundary cases and OBU null/identity overrides.
- Seeded records and null error/result pointers.

These synthetic comparisons exercise chroma-reconstruction and filter error
projection beyond the naturally failing retained-frame mutations. They do not
claim new image admission or full AV2 coverage. Temporary probe sources,
objects, and executables were removed after validation.

All seven original error owners also compare exactly after normalizing only
the initializer substitutions, removed local clear helper, added includes,
and whitespace. M4 orchestration is unchanged.

## Validation

Fresh `build/refactor-m7-*` builds passed:

- **31 host C units** and **2,460** public malformed cases under ASan/UBSan
  with diagnostics absent and enabled; runtime substrate, output policy, and
  six golden-output SHA checks also pass.
- Spatial admission in both modes: **16** exact fixtures, **eight** focused
  witnesses, and **11,850** deterministic malformed cases each.
- Both complete M0 resource/layout inventories, byte-for-byte unchanged.
- **53** standalone private headers, **35** value-only kernel dependency
  roots, no include cycles, and five dependency-tool tests.
- **88** photographic and **28** AVM-default reference images, exact.
- Native filter parity: tiny, odd, ordinary, and high-detail Y/U/V, exact.
- Freestanding core/runtime symbol checks and decoder evidence checks.
- Fresh Wasm module (**776,038 bytes**) and **16** exact Node spatial cases.
- Windows x86-64/ARM64 cross-builds, with exactly **10/1** decoder/smoke
  imports from KERNEL32.dll only.
- Current-oracle Baya: **four** cases in retained/fresh-encode modes, with
  zero errors, skips, or partial results.
- All **130** reference-lock records; **116** local reference manifests.
- Python syntax checks and all **15** recorded provenance paths in each
  laboratory tool.

Existing aggregate limitations remain:

- `make test`: C checks pass, then the legacy corpus gate stops at missing
  `images/artifacts/chimp-93x111-qp064.obu`.
- Python discovery: **80/81** pass; the remaining test needs missing
  `images/artifacts/emerald-192x96-qp208-decoded.i420`.
- All **14** target-baseline manifests remain absent.
- The full 10-bit Wasm manifest remains absent at
  `images/artifacts/10bit/avm-reference/manifest.json`; that full gate was not
  rerun.

No encoded fixture, golden pixel hash, reference lock, admission rule, public
layout, or oracle revision changed. Pinned AVM remains
`966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`; the optional current oracle remains
`edaf3ebc7ce3666290003f70a66efe5a508982a1`. Neither was fetched, advanced, or
rebuilt.

Cross-compilation is not Windows runtime parity. Windows/Linux-no-libc/browser
runtime checks, native 32-bit layouts, full photographic regeneration, and
maximum-image/peak-stack measurements remain outside this host validation.
