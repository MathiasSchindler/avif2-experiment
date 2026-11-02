# Refactoring M1: prediction-family ownership

**Status:** complete, 2 November 2026.

This implements [M1](../refactor.md#m1-extract-prediction-families) against the
unchanged [M0 baseline](refactoring-m0.md). It extracts numerical ownership,
not prediction admission or the supported AV2 profile.

## Resulting owners

| Owner | Responsibility |
| --- | --- |
| [intra_prediction.c](../../src/decoder/intra_prediction.c) | Existing admission conditions, dispatch precedence, and luma/chroma entry points |
| [intra_prediction_basic.c](../../src/decoder/intra_prediction_basic.c) | Typed DC, smooth, and Paeth kernels and their tables |
| [intra_prediction_directional.c](../../src/decoder/intra_prediction_directional.c) | Directional interpolation, angle tables, MRL/IBP, cardinal and geometry-specific kernels |
| [intra_prediction_edges.c](../../src/decoder/intra_prediction_edges.c) | General U8 and three U16-zone reference preparation paths, filter strength and typed edge filtering |
| [intra_prediction_dip.c](../../src/decoder/intra_prediction_dip.c) | Intra-DIP and its existing data include |
| [internal/intra_prediction.h](../../src/decoder/internal/intra_prediction.h) | Typed private interfaces and unchanged small rounding/log2 helpers |

Dependency direction is dispatch to numerical kernels, with the general
directional kernels calling edge preparation/filtering. No untyped sample
buffers, variant-dispatch framework, or macro-generated numerical kernels
were introduced. Existing externally used prediction entry points remain.

Compiler-delimited bodies and tables were moved before changing ownership.
No arithmetic, constants, allowlists, or branch ordering were intentionally
changed. Overlapping and shadowed legacy branches remain; their
geometry-specific scratch preparation stays with their numerical kernels.
Consolidating that repeated setup or deleting redundant branches belongs to
M2, with separate evidence.

The general edge paths preserve their original 141-sample caller-owned
buffers, five-sample negative prefix, 136-sample capacity guard, reference
extension order, filtering, and status propagation. U8 and U16 retain distinct
typed preparation routines. The smaller legacy scratch buffers were not
enlarged.

The new header still depends on the existing reconstruction types; the basic
and directional owners retain the existing reconstruction-module dependency
for the shared CfL divisor helper. Broader header/helper ownership cleanup
remains M6, rather than becoming an unrelated change here.

## Build and evidence tooling

The [Makefile](../../Makefile) explicitly lists all four new C owners in
`CORE_SOURCES`. Hosted, freestanding, no-libc, and Windows builds derive their
core objects from that list. The [WebAssembly builder](../../experimental/wasm/build.py)
reads the same list, and the viewer delegates to it; neither needs a duplicate
source manifest.

[check_prediction.py](../../tools/check_prediction.py) now defaults to the
directional and basic table owners. Repeated `--production PATH` arguments
replace those defaults; a single monolithic override remains supported.
Missing source files still fail explicitly.

The checker already failed before M1 because its geometric-smooth spelling
check expected a local named `shift`, while the unchanged implementation uses
`distance`. Its expectation now matches the actual helper; no smoothing
arithmetic was changed to satisfy the checker. The numerical expectations and
pinned AVM comparisons remain intact.

Six [checker regressions](../../tests/test_prediction_evidence.py) cover split
defaults, explicit replacement, single-file compatibility, missing sources,
and rejection of changed smooth-table entries and geometric expressions.
Both edited Python files pass Pylance syntax checks; the checker has no
Pylance diagnostics.

## Verified behavior

Host: macOS ARM64, 64-bit pointers/`size_t`, Homebrew Clang 22.1.8.
Native tests use ASan/UBSan and strict C11 warnings. Builds use separate
directories; diagnostics-disabled means the macro is absent, not defined as
zero.

| Gate | Result |
| --- | --- |
| Focused baseline/reconstruction/transform/tile units | Pass against unchanged expectations |
| Complete `test-c`, diagnostics absent and enabled | Both pass: 25 host units, runtime unit, 2,460 malformed cases, output-policy checks, and six raw SHA-256 checks including Baya |
| Frozen prediction inventory | All 13,568 calls and 48 group digests identical to M0 and between configurations: 12,472 OK, 200 unsupported, 896 invalid-argument |
| Resource/layout and failure characterization | All M0 workspace, alignment, status, diagnostic, padding and nonmutation assertions pass unchanged |
| Spatial admission, both configurations | 16 exact fixtures, eight source-bound witnesses, and 11,850 malformed cases pass per configuration |
| Native photographic parity | 88 legacy and 28 AVM-default references exact |
| Native filter parity | Tiny, odd, ordinary and high-detail Y/U/V exact |
| Reference locks | 130 valid records; 116 local manifests present |
| Freestanding symbol gate | Fresh core passes; no undefined symbols |
| Source-table evidence | Complete `test-decoder-evidence` passes, including unchanged generated data |
| WebAssembly | Fresh module builds from `CORE_SOURCES`; all 16 spatial fixtures exact under Node |
| Windows x86-64 and ARM64 | Fresh cross-builds pass; decoder/smoke imports remain exactly 10/1, only from KERNEL32.dll |
| Focused Python checker tests | Six pass, including three geometric-expression mutations |
| Complete Python discovery | 76 tests: 75 pass, one pre-existing missing-artifact error |

No expected prediction digest, fixture, golden pixel, reference lock,
workspace size, public signature, or public layout was updated.

### Reproduction

Use unused build directories when changing flags; object rules do not track
compiler flags reliably.

```sh
make -j4 HOST_BUILD_DIR=build/refactor-m1-diag-absent \
  test-c spatial-still-admission
flags='-std=c11 -Wall -Wextra -Wpedantic -Werror -pipe -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc'
make -j4 HOST_BUILD_DIR=build/refactor-m1-diag-on \
  HOST_CFLAGS="$flags -DAV2HEIF_ENABLE_DIAGNOSTICS=1" \
  test-c spatial-still-admission

build/refactor-m1-diag-absent/refactor-baseline-unit --inventory \
  > build/refactor-m1-diag-absent/inventory.csv
build/refactor-m1-diag-on/refactor-baseline-unit --inventory \
  > build/refactor-m1-diag-on/inventory.csv
cmp build/refactor-m0-diag-absent/inventory.csv \
  build/refactor-m1-diag-absent/inventory.csv
cmp build/refactor-m1-diag-absent/inventory.csv \
  build/refactor-m1-diag-on/inventory.csv

make NATIVE_BUILD_DIR=build/refactor-m1-native \
  test-freestanding test-decoder-evidence
python3 -m unittest discover -s tests -p 'test_prediction_evidence.py'
python3 tools/check_prediction.py \
  --production src/decoder/intra_prediction_directional.c \
  --production src/decoder/intra_prediction_basic.c
python3 tools/run_filter_parity.py \
  --decoder build/refactor-m1-diag-absent/av2heif-decoder --jobs 4
python3 tools/verify_image_references.py --suite photographic-8bit \
  --require-files --decoder build/refactor-m1-diag-absent/av2heif-decoder --jobs 4
python3 tools/verify_image_references.py --suite avm-default-8bit \
  --require-files --decoder build/refactor-m1-diag-absent/av2heif-decoder --jobs 4

python3 experimental/wasm/build.py \
  --output-directory build/refactor-m1-wasm
node tools/validate_spatial_still_wasm.js build/refactor-m1-wasm/decoder.wasm
make windows WINDOWS_BUILD_DIR=build/refactor-m1-windows-x86_64
make windows WINDOWS_ARCH=arm64 \
  WINDOWS_BUILD_DIR=build/refactor-m1-windows-arm64
python3 tools/check_windows_imports.py --objdump llvm-objdump \
  --decoder build/refactor-m1-windows-x86_64/av2heif-decoder.exe \
  --smoke build/refactor-m1-windows-x86_64/av2heif-windows-smoke.exe
python3 tools/check_windows_imports.py --objdump llvm-objdump \
  --decoder build/refactor-m1-windows-arm64/av2heif-decoder.exe \
  --smoke build/refactor-m1-windows-arm64/av2heif-windows-smoke.exe
```

### Existing moving-oracle replay

No oracle was fetched, rebuilt, or advanced. The existing isolated checkout
was clean and matched its recorded build revision:

- Pinned baseline: `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d`.
- Resolved current oracle: `edaf3ebc7ce3666290003f70a66efe5a508982a1`.
- Encoder SHA-256: `1463b6e289bc2b0887f687de69bae4284570d72620cfa0cefd0648ecd369cc69`.
- Decoder SHA-256: `a2081a6207c82cceac9c44ec4646416309bb9bcdc36002b23408c365d1a29121`.

```sh
python3 tools/check_avm_current.py \
  --decoder build/refactor-m1-diag-absent/av2heif-decoder \
  --suite photographic-8bit --source baya-weaver --preparation le896 \
  --mode both --output-dir build/refactor-m1-current-baya
```

The report records four passed cases, zero errors/skips/partial results:
I400 QP152, I420 QP96, I420 QP152, and I444 QP152. The oracle decodes existing
streams exactly against the pinned references; all four fresh encodes differ
from their pinned streams, but project and current-oracle decoded samples
agree exactly.

The fresh I420 QP152 stream still has encoded SHA-256
`dc7eeffcccbcc20654d5e189f362b701701dfa2f5a63f542fdedf6c9a5f14ff3`
and decoded SHA-256
`7324889ae632f9e4f75eab47dcc8e430e1df8efde8c9ba68e23884bf5f492b1f`.
It reproduces the retained regression without replacing any pinned reference.
The output directory must be new when replaying this command.

## Incomplete gates and limits

These are not passing aggregate gates:

- `make test` passes its C gate, then fails on the same absent artifact
  `images/artifacts/chimp-93x111-qp064.obu`.
- Python discovery still fails only at
  `test_emerald_q208_final_artifact_hashes`, because
  `images/artifacts/emerald-192x96-qp208-decoded.i420` is absent.
- `make test-reference-images` fails at the absent target-baseline Baya
  manifest. All 14 target-baseline manifests remain unavailable; the 116
  separately verified local references do not replace them.
- The 10-bit WebAssembly corpus validator cannot load
  `images/artifacts/10bit/avm-reference/manifest.json`. No full 10-bit
  WebAssembly corpus/runtime parity is claimed; native 10-bit unit and frozen
  prediction coverage do pass.

Other missing-media limits recorded by M0 remain limitations, not new skips or
changed expected outputs. No missing media were regenerated to turn gates
green.

Windows validation is cross-build/import evidence, not Windows runtime
parity. Linux no-libc runtime, browser UI, maximum-image decoding, and
peak-stack measurement were not executed here. The Wasm module is a wasm32
build, but the complete native layout baseline was run only on the 64-bit
host. These limits remain explicit for M8.
