# Refactoring M6: narrow private interfaces

Completed 2 November 2026.

## Ownership

[`reconstruct.h`](../../src/decoder/reconstruct.h) no longer includes the
coefficient-level parser, transform parser, coefficient parser, leaf parser,
frame state, sequence state, range decoder, or CDF interfaces. It retains
reconstruction results, scratch, and block/plane integration declarations.

The existing value declarations now have focused owners:

| Interface | Responsibility |
| --- | --- |
| [`block_geometry.h`](../../src/decoder/block_geometry.h) | Block sizes, partition-node coordinates, block dimensions, and full output-sample capacity |
| [`tx_geometry.h`](../../src/decoder/tx_geometry.h) | Transform-size enum, descriptors, and transform-node geometry |
| [`quantizer.h`](../../src/decoder/quantizer.h) | Quantizer-index scalar |
| [`plane.h`](../../src/decoder/plane.h) | Plane count, typed reconstruction-plane views, and readiness query |
| [`leaf_mode.h`](../../src/decoder/leaf_mode.h) | Luma/chroma mode values, palette accessors, and mode-domain limits |
| [`coeff_block.h`](../../src/decoder/coeff_block.h) | Coefficient prefix/block values, dequantization widths, coefficient geometry, and pure coefficient utilities |
| [`transform_type.h`](../../src/decoder/transform_type.h) | Transform-kind/result values, symbol-domain limits, and transform arithmetic widths |
| [`prediction.h`](../../src/decoder/prediction.h) | Typed reference edges, prediction sum width, and prediction entry points |

These are moved declarations, not redesigned representations. Field order,
array bounds, enum values, entropy bit counters, and integer widths remain
unchanged. There is no universal types header.

The existing private intra/chroma prediction headers consume prediction/plane
values rather than reconstruction or parser interfaces. IntraBC kernel
declarations now live in
[`internal/intrabc_prediction.h`](../../src/decoder/internal/intrabc_prediction.h).
The remaining
[`internal/reconstruct_modules.h`](../../src/decoder/internal/reconstruct_modules.h)
is a block/transform interface, not a second prediction-family registry.
Secondary-transform geometry helpers no longer import coefficient CDF banks.

[`tile.h`](../../src/decoder/tile.h) directly includes the parser/CDF interfaces
that its state actually stores. Affected callers and tests likewise name their
required owners instead of recovering them through reconstruction.

[`internal/decode_diagnostics.h`](../../src/decoder/internal/decode_diagnostics.h)
now forward-declares the partition decision behind its trace pointer. Tagging
that existing record changes no field or layout. Reconstruction and prediction
interfaces also declare diagnostic tags at file scope, avoiding accidental
prototype-scope types.

## Shared mathematics

The reciprocal table and bounded fixed-point regression previously owned by
CfL now live in [`sample_math.c`](../../src/decoder/sample_math.c), with
declarations and the existing rounding helpers in
[`internal/sample_math.h`](../../src/decoder/internal/sample_math.h).

- `av2h_cfl_resolve_divisor_32` becomes `av2h_sample_resolve_divisor_32`.
- `av2h_cfl_derive_alpha` becomes `av2h_sample_regression_slope`.

Luma DC/directional prediction and IntraBC no longer depend on a CfL
implementation for these mathematical operations. The reciprocal table,
rounding, saturation, zero-count behavior, and arithmetic bodies are unchanged.
The new translation unit is registered in the authoritative
[`CORE_SOURCES`](../../Makefile), also used by the Wasm and Windows builds.

## Persistent dependency guard

```sh
make test-private-headers
```

[`check_private_headers.py`](../../tools/check_private_headers.py) checks:

- All **52** root/decoder private headers compile independently as strict,
  freestanding C11 using repository standard headers and `-nostdinc`.
- Diagnostics are tested with the macro absent and with it enabled.
- The quoted-include graph has no cycles, including cycles hidden by guards.
- Actual compiler dependency closures for **18** numerical headers and **17**
  numerical/integration sources contain only the allowed value interfaces,
  foundational declarations, and standard scalar headers.

Checking source closures as well as headers prevents a kernel implementation
from directly reintroducing a parser/frame dependency. Numerical table
attachments remain local data, not parser interfaces.

The gate is part of `test-c` and `test`.
[`test_private_headers.py`](../../tests/test_private_headers.py) adds five
tests for guarded cycles, shared dependencies, include resolution, compiler
dependency escaping, and rejection of transitive parser headers. A separate
compiler-backed negative probe also confirmed that an indirect frame include
is rejected.

## Preservation evidence

Pre-change sources were retained outside the worktree. Comparison verified:

- All **162** original function bodies in the affected implementation owners
  remain unchanged after normalizing relocated helper names.
- All **103** complete decoder struct/enum declarations remain unchanged after
  normalizing only the partition-decision tag.
- Both diagnostic configurations reproduce the M0 resource inventory
  byte-for-byte, including public/workspace layouts, parser state, scratch,
  reference records, and allocation-query boundaries.
- All existing M0-M5 characterization expectations remain unchanged and pass.

[`prediction_math_unit.c`](../../tests/prediction_math_unit.c) freezes values
measured against the original implementation before extraction:

| Group | Calls | Frozen hash |
| --- | ---: | --- |
| Reciprocal/divisor scale | 65,632 | `3cc8148f050d5c66` |
| Bounded regression slope | 4,680 | `d6f776f1ee629f75` |

Coverage includes positive divisors through 65,536, power-of-two boundaries,
`UINT32_MAX`, positive/negative slopes, fixed-point scales, and degenerate
sample counts/variance.

Two source-bound witness identities in
[`spatial-still-profile.json`](../../tests/spatial-still-profile.json) were
refreshed because their test sources gained direct includes. Their test bodies
are unchanged. No encoded fixture, golden pixel hash, reference lock, admission
rule, error-construction policy, or oracle revision changed.

## Validation

Builds used `build/refactor-m6-*`; the diagnostic-absent build was fully
recompiled after the interface extraction. Diagnostics disabled means the
macro is absent, not defined to zero.

Passed:

- **30 host C units**, runtime substrate, **2,460** public malformed cases,
  output policy, and six golden-output SHA checks, under ASan/UBSan in both
  diagnostic configurations.
- Spatial admission in both configurations: **16** exact fixtures, **eight**
  source-bound witnesses, and **11,850** deterministic malformed cases each.
- **88** photographic and **28** AVM-default reference images, exact.
- Native filter parity: tiny, odd, ordinary, and high-detail Y/U/V, exact.
- Freestanding core/runtime symbol checks and decoder table/evidence gates.
- Fresh Wasm build and **16** Node spatial cases, exact.
- Windows x86-64/ARM64 cross-builds; decoder/smoke imports remain exactly
  **10/1**, from KERNEL32.dll only.
- Current-oracle Baya: **four** cases, retained and fresh-encode modes,
  zero errors/skips/partial results.
- All **130** reference-lock records; **116** local reference manifests.
- Header checks, five dependency-tool tests, and clean Pylance diagnostics for
  both new Python files.

Existing aggregate limitations were reproduced, not bypassed:

- `make test`: C checks pass, then the 31-file legacy corpus gate stops at
  missing `images/artifacts/chimp-93x111-qp064.obu`.
- Python discovery: **80/81** pass; the remaining test needs missing
  `images/artifacts/emerald-192x96-qp208-decoded.i420`.
- Required target-baseline parity: all **14** local manifests remain absent.
- Full 10-bit Wasm gate: missing
  `images/artifacts/10bit/avm-reference/manifest.json`.

Pinned AVM remains `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`; the optional
current oracle remains `edaf3ebc7ce3666290003f70a66efe5a508982a1`.
Neither oracle was fetched, advanced, or rebuilt.

Cross-compilation is not Windows runtime parity. Windows/Linux no-libc runtime,
browser runtime, maximum-image decode/peak-stack measurements, and native
32-bit layout measurements remain outside this host validation. M7's
error-record work and M8's broader integration work remain separate milestones.
