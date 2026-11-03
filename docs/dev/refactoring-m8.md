# Refactoring M8: integration and final ownership

Integration work recorded 3 November 2026.

**Status: complete under revised local-macOS acceptance.** This is not an
all-gates-pass release claim. The earlier broader coverage results and
limitations below remain recorded.

## Acceptance update: local macOS execution

On 3 November 2026 the user clarified that cross-builds and complete image
corpora are not required; it is sufficient that the program runs on this
macOS computer.

The normal native decoder was freshly built in `build/refactor-m8-macos`.
It is a hosted Mach-O ARM64 executable linked to libSystem, not a sanitizer
build or the separate no-import/no-libc target.

```sh
make -j4 NATIVE_BUILD_DIR=build/refactor-m8-macos decoder
python3 tools/build_checks.py parity \
  --decoder build/refactor-m8-macos/av2heif-decoder \
  --input tests/fixtures/baya-weaver-current-i420-qp152.obu \
  --output build/refactor-m8-macos/baya-weaver-current-i420-qp152.i420 \
  --expected 7324889ae632f9e4f75eab47dcc8e430e1df8efde8c9ba68e23884bf5f492b1f
python3 tools/check_10bit_wp0_oracle.py verify \
  --decoder build/refactor-m8-macos/av2heif-decoder
```

All commands passed: retained Baya 8-bit output is exact, and the retained
`tiny-constrained-16x16-yuv420p10le-qp208` WP0 oracle verifies successfully.
This satisfies the revised acceptance criterion and closes M8.

The existing no-libc linker issue is not claimed fixed. Missing media and
unexecuted broader platform gates remain coverage limitations rather than
blockers for this local-macOS milestone.

## Final ownership

| Responsibility | Owner |
| --- | --- |
| Public query/decode validation, input binding, admission and result projection | [`query.c`](../../src/query.c), [`decode.c`](../../src/decode.c), [`output_layout.c`](../../src/output_layout.c) |
| Container box reading, HEIF interpretation and deterministic writing | [`bmff.c`](../../src/bmff.c), [`container.c`](../../src/container.c), [`container_write.c`](../../src/container_write.c) |
| Root carriage policy and codec metadata reconciliation | [`carriage.c`](../../src/carriage.c) |
| OBU framing, sequence syntax and content interpretation | [`obu.c`](../../src/decoder/obu.c), [`sequence.c`](../../src/decoder/sequence.c), [`content_interpretation.c`](../../src/decoder/content_interpretation.c) |
| Frame syntax and tile/leaf/coefficient parsing | [`frame.c`](../../src/decoder/frame.c), [`tile.h`](../../src/decoder/tile.h) and the existing focused parser/CDF owners |
| Prediction admission and family selection | [`intra_prediction.c`](../../src/decoder/intra_prediction.c), [`chroma_prediction.c`](../../src/decoder/chroma_prediction.c) |
| Edge preparation and luma prediction kernels/data | [`intra_prediction_edges.c`](../../src/decoder/intra_prediction_edges.c), [`intra_prediction_basic.c`](../../src/decoder/intra_prediction_basic.c), [`intra_prediction_directional.c`](../../src/decoder/intra_prediction_directional.c), [`intra_prediction_dip.c`](../../src/decoder/intra_prediction_dip.c) |
| Chroma/CfL/MHCCP and IntraBC prediction | [`chroma_prediction_common.c`](../../src/decoder/chroma_prediction_common.c), [`chroma_prediction_cfl.c`](../../src/decoder/chroma_prediction_cfl.c), [`chroma_prediction_mhccp.c`](../../src/decoder/chroma_prediction_mhccp.c), [`intrabc_prediction.c`](../../src/decoder/intrabc_prediction.c) |
| Supplied-predictor block reconstruction and residual composition | [`reconstruct_block.c`](../../src/decoder/reconstruct_block.c) |
| Inverse-transform arithmetic | [`inverse_transform.c`](../../src/decoder/inverse_transform.c), [`secondary_transform.c`](../../src/decoder/secondary_transform.c) |
| Typed plane validation, references, prediction preparation, visible writes and readiness commits | [`reconstruct.c`](../../src/decoder/reconstruct.c) |
| Arena sizing/binding, coded storage and filter-workspace preparation | [`frame_workspace.c`](../../src/decoder/frame_workspace.c) |
| Visible crop/copy, output validation and copy scheduling | [`frame_copy.c`](../../src/decoder/frame_copy.c) |
| Deblock/restoration metadata commits | [`frame_filter_metadata.c`](../../src/decoder/frame_filter_metadata.c) |
| Frame/tile/chroma scheduling, filtering, output handoff and failure context | [`frame_output.c`](../../src/decoder/frame_output.c) |
| Shared reciprocal/regression and rounding arithmetic | [`sample_math.c`](../../src/decoder/sample_math.c), [`internal/sample_math.h`](../../src/decoder/internal/sample_math.h) |
| Partial core error initialization and explicit diagnostic reset | [`error.h`](../../src/error.h); offsets, reason maps and stage interpretation remain local |

This is responsibility separation, not a new codec pipeline. Entropy order,
tile/chroma scheduling, filter order and per-path error precedence remain the
existing ones. Numerical filter, transform and geometry owners were not
reorganized merely to reduce file sizes.

### Dependency direction

The permitted consumption direction is:

```text
platform adapters / public API
  -> container / carriage / codec syntax and admission
  -> frame orchestration and caller-owned workspace
  -> typed plane integration / prediction dispatch / filtering
  -> prediction and supplied-predictor reconstruction kernels
  -> value interfaces, geometry and shared sample arithmetic
```

Parsing produces the mode, coefficient, transform and plane values consumed by
numerical owners; numerical headers do not import the parsers that produce
them. The value interfaces are
[`block_geometry.h`](../../src/decoder/block_geometry.h),
[`tx_geometry.h`](../../src/decoder/tx_geometry.h),
[`quantizer.h`](../../src/decoder/quantizer.h),
[`plane.h`](../../src/decoder/plane.h),
[`leaf_mode.h`](../../src/decoder/leaf_mode.h),
[`coeff_block.h`](../../src/decoder/coeff_block.h),
[`transform_type.h`](../../src/decoder/transform_type.h), and
[`prediction.h`](../../src/decoder/prediction.h).

Caller-owned diagnostic values remain a permitted dependency through
[`internal/decode_diagnostics.h`](../../src/decoder/internal/decode_diagnostics.h);
its partition-decision pointer uses a forward declaration, not a parser
include. This does not turn diagnostics into a second orchestration layer.

[`check_private_headers.py`](../../tools/check_private_headers.py), included
in `test-c` and `test`, enforces **53** independent private-header compilations,
the quoted-include cycle check, and **35** compiler-derived value-only kernel
roots: **18** headers and **17** implementation/integration sources. Both
diagnostic configurations pass. These allowlisted closures, not a new universal
types header, define the dependency boundary.

All platforms continue using the explicit
[`CORE_SOURCES`](../../Makefile) manifest. No source globbing or directory
regrouping was introduced.

### Compatibility entry points

No forwarding wrapper was removed in M8. Existing typed and diagnostic-free
entry points can encode different defaults or preserve direct-helper behavior
outside canonical bitstream admission. For example, the U16 luma/chroma
prediction entry points retain different IDIF defaults. A short forwarding body
alone is not evidence that these interfaces are interchangeable.

Removing any such entry point requires a separate complete caller migration
and validation; M8 does not trade compatibility for cosmetic cleanup.

## Measured integration results

Host: macOS ARM64, 64-bit pointers/`size_t`, Homebrew Clang **22.1.8**,
Python **3.14.7**, Node **26.7.0**. Fresh builds used `build/refactor-m8-*`.
Diagnostics disabled means the macro is absent, not defined to zero.

| Gate | Result |
| --- | --- |
| Native C, both diagnostic configurations | **31** ASan/UBSan host units each; runtime substrate, output policy and six golden-output SHA checks pass |
| Public malformed boundary, both configurations | **2,460** cases each |
| Spatial admission, both configurations | **16** exact fixtures, **eight** source-bound witnesses and **11,850** malformed cases each |
| M0 layout/resource inventory | Both complete CSV inventories byte-identical to M0 |
| Private interfaces | **53** headers, **35** value-only roots, no cycles and five dependency-tool tests pass |
| Eight-bit photographic/default native parity | **88 + 28 = 116** images exact in both diagnostic modes |
| Retained synthetic 10-bit native/public parity | **25** I420 and **21** I444 records exact in both diagnostic modes |
| Native filter parity | Tiny, odd, ordinary and high-detail final Y/U/V exact |
| Retained WP0 oracle | Verify command passes |
| Freestanding | Core/runtime undefined-symbol and decoder evidence checks pass |
| WebAssembly | Fresh **776,038-byte** module; **16** spatial cases and all **116** available photographic/default references exact in Node |
| Synthetic 10-bit WebAssembly | All **46** retained records exact, with result-layout and sample-range checks |
| Windows x86-64/ARM64 | Cross-builds pass; exactly **10/1** decoder/smoke imports, KERNEL32.dll only |
| Current-oracle Baya | **Four** cases pass in retained and fresh-encode modes, with no errors/skips/partial results |
| Reference locks | All **130** records verify; **116** local reference manifests present |

Thus all **162** available reference images (116 eight-bit and 46 synthetic
ten-bit) agree in hosted native and Node WebAssembly runs. The 46 ten-bit
records additionally pass canonical public-container decode. This is not the
full **82**-record ten-bit release corpus: its 36 photographic records are
unavailable here.

The temporary synthetic-Wasm probe explicitly required **25/21** records from
the retained manifest. It checked encoded and AVM-output identities, returned
status, dimensions, channels, chroma, 10-bit/two-byte/little-endian metadata,
output alignment and geometric size, full bytes/SHA, and `[0, 1023]` samples.
It did not change the full Wasm validator or substitute for its missing
photographic manifest. The probe was removed after passing.

All persistent M0-M7 characterizations pass without changed expectations,
including M7's **43,878** frozen error scenarios and **2,316** direct helper
cases per configuration. The resource comparison includes U8/U16 scratch,
public/private records, and odd, photographic and maximum-size sizing queries.
Maximum-size queries are not maximum-image decode or peak-stack measurements.

M8 changed no production source, build manifest, test, fixture, witness record,
golden pixel hash or reference lock. **1,088** pre-existing non-documentation
file identities were compared before/after validation. The eight source-bound
witnesses pass unchanged; no source-hash refresh was necessary.

## Optional broader gates and coverage limitations

| Gate | Observed blocker or host limitation |
| --- | --- |
| `make test` / legacy I420 corpus | All **31** legacy OBUs are absent; the gate stops at `images/artifacts/chimp-93x111-qp064.obu` after C checks pass |
| Dedicated I400/I444 native/public corpora | Missing `images/artifacts-8bit-mono/manifest.json` and `images/artifacts-8bit-444/manifest.json` |
| All-suites required-file parity | All **14** target-baseline manifests are absent |
| Python discovery | **80/81** pass; the remaining test needs `images/artifacts/emerald-192x96-qp208-decoded.i420` |
| Full ten-bit Wasm/release corpus | Missing `images/artifacts/10bit/avm-reference/manifest.json`; the full Wasm command fails explicitly |
| Spatial frontier runner | Stops at missing `spatial-still-8bit/baya-weaver-le896-i420-qp128/encoded.obu` in the pinned reference tree |
| macOS no-libc runtime | Custom Mach-O linker rejects the compiler-emitted `__ustring` section before producing an executable |
| Linux no-libc / Windows runtime | This is not a Linux/Windows host; no compatible emulator/container runtime was available |
| Browser runtime | Not exercised; Node Wasm results are not browser runtime evidence |

The macOS failure was reproduced with Homebrew Clang 22 using default LTO and
`NOLIBC_LTO=0`, and with Apple Clang **21.0.0** without LTO. Crucially, the same
Homebrew default-LTO failure also reproduces in isolated source snapshots of
pre-M0 commit `4e8662e9527668e17dcfbc632434aea49b7a7e25` and pre-M6 commit
`a0d8395`. It is not a new M0-M8 platform regression. No linker, compiler
optimization policy or production admission change was made to hide it.
Temporary baseline source trees were removed after comparison.

Native 32-bit layouts, browser runtime, maximum-image decode/peak stack, and
full photographic regeneration are not claimed. Cross-compilation and
freestanding symbol inspection are not runtime parity.

## Reproduction and closeout

The main successful host commands were:

```sh
make -j4 HOST_BUILD_DIR=build/refactor-m8-diag-absent \
  NATIVE_BUILD_DIR=build/refactor-m8-native \
  test-c spatial-still-admission test-freestanding test-decoder-evidence

flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m8-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission

python3 experimental/wasm/build.py --output-directory build/refactor-m8-wasm
node tools/validate_spatial_still_wasm.js build/refactor-m8-wasm/decoder.wasm
node tools/validate_image_references_wasm.js \
  build/refactor-m8-wasm/decoder.wasm photographic-8bit
node tools/validate_image_references_wasm.js \
  build/refactor-m8-wasm/decoder.wasm avm-default-8bit
```

For retained synthetic ten-bit native/public parity, the existing
[`validate_10bit_420_corpus.py`](../../tools/validate_10bit_420_corpus.py) and
[`validate_10bit_444_corpus.py`](../../tools/validate_10bit_444_corpus.py) were
invoked with the matching decoder/artifact tool and explicit
`--manifest tests/fixtures/10bit/avm-reference/manifest.json`, in both modes.
This explicitly identifies the tested subset instead of implying that the
missing photographic corpus passed.

The original broader closeout would require restoring locked media, resolving
the macOS linker limitation in a separately validated change, and rerunning
the full native/platform gates on suitable hosts. These are now optional
follow-up coverage, not M8 exit conditions; the revised local-macOS acceptance
is complete in [`refactor.md`](../history/refactor.md). Any future restoration must use
the recorded provenance rather than replacing pinned data with new encodes or
relaxing hashes.

Pinned AVM remains `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`. The optional
current oracle remains `edaf3ebc7ce3666290003f70a66efe5a508982a1`, with encoder
SHA-256 `1463b6e289bc2b0887f687de69bae4284570d72620cfa0cefd0648ecd369cc69`
and decoder SHA-256
`a2081a6207c82cceac9c44ec4646416309bb9bcdc36002b23408c365d1a29121`.
Neither oracle was fetched, advanced or rebuilt.
