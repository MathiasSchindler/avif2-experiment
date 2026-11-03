# Code Quality and Consistency Cleanup Plan

**Status:** C1-C7 complete; C8-C10 planned

**Scope:** [`src/`](../src/) and [`include/av2heif.h`](../include/av2heif.h)

## Purpose and Scope

Following the structural decomposition in [refactor.md](refactor.md), this document defines the next set of code-quality milestones focused on **eliminating duplication, retiring historical scaffolding, unifying naming, consolidating shared helpers, and enforcing stylistic consistency** across [`src/`](../src/).

This is a strictly behavior-preserving cleanup pass. Every milestone must preserve:

- Exact bit-for-bit decoded pixel output across all admitted 8-bit and 10-bit streams (and prepare cleanly for 12-bit support where applicable).
- Exact status codes, error offsets, diagnostic stages, and entropy tell positions on valid, unsupported, and malformed inputs.
- Freestanding C11 compliance (`-std=c11 -ffreestanding -fno-builtin -nostdinc -Wall -Wextra -Wpedantic -Werror`), zero heap allocation in core decoding, and full `nolibc` static build support.
- Existing public API contracts in [`include/av2heif.h`](../include/av2heif.h).

---

## Milestone Overview

| Milestone | Focus | Primary Targets | Prerequisites |
| :--- | :--- | :--- | :--- |
| **C1** | Consolidate redundant math, bit, and endian helpers into [`src/shared/`](../src/shared/) / [`src/base.h`](../src/base.h) | [`src/shared/`](../src/shared/), [`src/base.h`](../src/base.h), [`src/decoder/`](../src/decoder/) | None |
| **C2** | Standardize `#include`/`#define` layout and unify `NULL` vs `0` and boolean types (`int` vs `uint8_t`) | All `.c` and `.h` files under [`src/`](../src/) | C1 |
| **C3** | Unify legacy `Av2hFirst*` type and function names to reflect full-frame architecture | [`src/decoder/*.h`](../src/decoder/), [`src/decoder/*.c`](../src/decoder/), tests | C2 |
| **C4** | Retire evolutionary wrapper functions and compatibility `#define` aliases | [`reconstruct.h`](../src/decoder/reconstruct.h), [`leaf.h`](../src/decoder/leaf.h), [`coeff.h`](../src/decoder/coeff.h), [`filter.h`](../src/decoder/filter.h) | C3 |
| **C5** | Deduplicate CLI frontends across POSIX, Windows, and `nolibc` | [`src/platform/`](../src/platform/), [`src/shared/`](../src/shared/) | C1, C2 |
| **C6** | Deduplicate 8-bit, 10-bit (and future 12-bit) sample paths | [`reconstruct*.c`](../src/decoder/reconstruct.c), [`*prediction*.c`](../src/decoder/intra_prediction.c), [`filter.c`](../src/decoder/filter.c), [`frame_output.c`](../src/decoder/frame_output.c) | C1, C3, C4 |
| **C7** | De-interleave heavily branched `*_kernel.inc` templates into clean shared helpers and inner kernels | [`reconstruct_kernel.inc`](../src/decoder/internal/reconstruct_kernel.inc), [`reconstruct_block_kernel.inc`](../src/decoder/internal/reconstruct_block_kernel.inc), [`chroma_prediction_cfl_kernel.inc`](../src/decoder/internal/chroma_prediction_cfl_kernel.inc), [`chroma_prediction_mhccp_kernel.inc`](../src/decoder/internal/chroma_prediction_mhccp_kernel.inc) | C6 |
| **C8** | Replace hardcoded tuple branches and boolean walls with table-driven rules and remove dead checks | [`secondary_transform.c`](../src/decoder/secondary_transform.c), [`inverse_transform.c`](../src/decoder/inverse_transform.c), [`coeff_level.c`](../src/decoder/coeff_level.c) | C4, C6 |
| **C9** | Decompose mega-function orchestration in [`frame_output.c`](../src/decoder/frame_output.c) | [`frame_output.c`](../src/decoder/frame_output.c) | C7 |
| **C10** | Normalize formatting, indentation, and line wrapping with a zero-binary-diff guarantee | All `.c`, `.h`, and `*_kernel.inc` files under [`src/`](../src/) and [`include/`](../include/) | C7, C8, C9 |

---

## C1: Consolidate Math, Bit, and Endian Helpers

### Problem
Multiple translation units define private `static` versions of identical arithmetic, rounding, bit-manipulation, and byte-order helpers:
- **Clamping:** `av2h_sample_clamp` exists in [`src/decoder/internal/sample_math.h`](../src/decoder/internal/sample_math.h), but local `clamp()`, `ccso_clamp()`, and `clamp_coordinate()` wrappers are repeated in [`inverse_transform.c`](../src/decoder/inverse_transform.c), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`secondary_transform.c`](../src/decoder/secondary_transform.c), [`ccso.c`](../src/decoder/ccso.c), and [`restoration.c`](../src/decoder/restoration.c).
- **Rounding / Shifting:** `round_shift`, `round_shift_signed`, `round_shift_symmetric`, `floor_shift_i32`, `lossless_floor_shift`, and `intrabc_round_shift` are duplicated across [`sample_math.h`](../src/decoder/internal/sample_math.h), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`inverse_transform.c`](../src/decoder/inverse_transform.c), [`secondary_transform.c`](../src/decoder/secondary_transform.c), [`chroma_prediction.c`](../src/decoder/chroma_prediction.c), [`intrabc_prediction.c`](../src/decoder/intrabc_prediction.c), [`internal/intra_prediction.h`](../src/decoder/internal/intra_prediction.h), and [`internal/filter_common.h`](../src/decoder/internal/filter_common.h).
- **Log2 / Bit Scanning:** `ceil_log2_u32` / `palette_ceil_log2` / `reciprocal_msb` are duplicated in [`frame.c`](../src/decoder/frame.c), [`sequence.c`](../src/decoder/sequence.c), [`palette.c`](../src/decoder/palette.c), and [`sample_math.c`](../src/decoder/sample_math.c).
- **Endian Stores/Loads:** [`src/base.h`](../src/base.h) provides `av2h_load_u16be`, `av2h_load_u32be`, and `av2h_load_u64be`, but lacks `le` loads and `le`/`be` stores. As a result, `store_u16le`, `store_u32le`, and `store_u32be` are re-implemented in [`src/shared/png_writer.c`](../src/shared/png_writer.c) and all three platform CLI drivers (`posix`, `windows`, `nolibc`).

### Actions
1. **Shared Endian & Bit Primitives:** Add `static inline` endian load/store helpers (`av2h_store_u16le`, `av2h_store_u32le`, `av2h_store_u16be`, `av2h_store_u32be`) and integer log2 helpers (`av2h_ceil_log2_u32`, `av2h_floor_log2_u32`) to [`src/shared/`](../src/shared/) (or [`src/base.h`](../src/base.h) where appropriate).
2. **Unified Fixed-Point Shift/Clamp Helpers:** Consolidate all distinct mathematical rounding modes (unsigned power-of-two rounding, asymmetric two's-complement rounding, symmetric sign-magnitude rounding, and arithmetic floor-shift) into well-documented `static inline` functions in [`src/decoder/internal/sample_math.h`](../src/decoder/internal/sample_math.h) (or a shared math header in [`src/shared/`](../src/shared/)).
3. **Remove Local Duplicates:** Delete all file-local `static` duplicates and call the canonical shared helpers directly.

### Exit Criteria
- Zero file-local re-definitions of `clamp`, `round_shift*`, `floor_shift*`, `ceil_log2*`, or `store_u*` across [`src/`](../src/).

### Completion

C1 is complete. Canonical owners are:

- [`src/base.h`](../src/base.h): unaligned little-endian loads and little-/big-endian stores. Existing externally defined big-endian loads retain their symbols. Byte writers, PNG serialization, and all three CLI frontends call these helpers directly.
- [`src/shared/bit_math.h`](../src/shared/bit_math.h): integer ceil/floor log2 and ceil log2 of a positive-unit ratio. Zero/one log2 behavior is retained; ratio calculations use a 64-bit limit to avoid overflowing 32-bit doubling.
- [`src/decoder/internal/sample_math.h`](../src/decoder/internal/sample_math.h): clamp, absolute value, fixed-block log2, floor shift, and the two distinct rounding modes.

Rounding remains deliberately distinct:

| Canonical helper | Semantics | Preserved callers |
| :--- | :--- | :--- |
| `av2h_sample_round_pow2` | Half-up for nonnegative values; signed ties toward positive infinity | Prediction, inverse transforms, reconstruction's asymmetric rounding, and the old filter `round_shift_signed` |
| `av2h_sample_round_pow2_signed` | Signed ties away from zero | Secondary transforms, reconstruction's symmetric rounding, and filter `round_shift_symmetric` |
| `av2h_sample_floor_shift` | Floor toward negative infinity, without shifting a negative signed integer | Lossless transforms, filter coordinates, and intra-block-copy coordinates |

Shift zero remains an identity. Fixed-block log2 still maps 4/8/16/32 to 2/3/4/5 and every other size to 6; clamp comparison order is unchanged, including reversed bounds. Helper range preconditions are documented in the header. No public API, workspace, or allocation-policy changes were made.

Validation:

- Expanded [`substrate_unit.c`](../tests/substrate_unit.c) and [`sample_math_unit.c`](../tests/sample_math_unit.c) cover log2 boundaries, exhaustive 16-bit endian operations and block-size mappings, unaligned accesses and sentinels, negative rounding ties, floor shifts, and representable extrema.
- [`test_shared_helpers.py`](../tests/test_shared_helpers.py) rejects legacy definitions and requires exactly one owner for each of the 15 canonical helpers; both tests pass and are included in standard Python test discovery.
- `make test-c` passes all 31 C unit suites, runtime checks, 2,460 malformed-input fuzz cases, and frozen pixel/status/diagnostic/output-policy checks in separate sanitized builds with `AV2HEIF_ENABLE_DIAGNOSTICS` absent and enabled.
- The private-header checker passes 54 standalone headers and 35 value-only kernel roots in both diagnostic modes. The new scalar bit header is explicitly admitted without relaxing parser-dependency or include-cycle checks.
- All 179 available OBU outputs match the pre-C1 SHA-256 baseline: 8-bit I400/I420/I444 and 10-bit I420/I444, including lossless, restoration, tiled, and current Baya Weaver cases.
- The normal macOS decoder rebuild and freestanding undefined-symbol checks pass. Affected macOS `nolibc` and Windows x86-64/ARM64 frontend objects compile cleanly. Separate macOS `nolibc` executable linking retains the previously established compiler-emitted `__ustring` limitation; it was not changed or claimed as a passing build.

---

## C2: Include/Macro Placement, `NULL` vs `0`, and Boolean Consistency

### Problem
1. **Mid-file `#include` and `#define` directives:** Several files (e.g., [`src/decoder/reconstruct.c`](../src/decoder/reconstruct.c), [`src/platform/nolibc/decoder.c`](../src/platform/nolibc/decoder.c)) place `#include` statements and compatibility `#define` aliases after function definitions rather than at the top of the file.
2. **`0` vs `NULL` for pointers:** [`src/shared/stddef.h`](../src/shared/stddef.h) defines `NULL` for freestanding builds, and [`src/platform/`](../src/platform/) uses `NULL`, whereas [`src/decoder/`](../src/decoder/) and [`src/base.c`](../src/base.c) predominantly compare and return pointer values as `0`.
3. **Boolean return and storage types (`int` vs `uint8_t`):** Predicate functions and boolean flags mix `int` and `uint8_t` without a clear convention between bitfield/compact-struct storage and function return values.

### Actions
1. **Header & Macro Ordering:** Move ordinary dependency headers and generated table includes to the top of every `.c` and `.h` file:
   - Corresponding module header first,
   - Project headers in alphabetical order,
   - Shared/freestanding standard headers (`<stddef.h>`, `<stdint.h>`, `<limits.h>`) last.
   - Place all file-scope `#define` constants and type definitions immediately after `#include`s and before any function definitions.
   - Typed implementation instantiations introduced by C6 use the narrowly scoped trailing-section convention documented below, rather than dependency-header ordering.
2. **Pointer Nullability (`NULL`):** Standardize on `NULL` (from [`src/shared/stddef.h`](../src/shared/stddef.h)) for all pointer comparisons (`ptr == NULL`, `ptr != NULL`), pointer assignments, and null-pointer returns (`return NULL;`), reserving `0` / `0U` strictly for integer and enum values.
3. **Boolean Convention:** Establish and apply a single rule across [`src/`](../src/):
   - **Struct fields and compact arrays:** Use `uint8_t` (`0U` / `1U`) so memory layout and workspace sizing remain explicit and architecture-independent.
   - **Predicate function return types:** Use `int` (`0` / `1`) consistently across all validation and query helpers (matching C's comparison operator type and existing [`base.h`](../src/base.h) predicates like `av2h_size_add`).

### Exit Criteria
- No dependency-header or generated-table `#include` appears after the first declaration or function. Only validated typed implementation instantiations may occupy the trailing section described below.
- Pointer checks and null pointer literals use `NULL` consistently across [`src/`](../src/).
- Predicate return types and struct boolean fields follow the documented convention.

### Completion

C2 is complete for the native macOS target.

- Module/interface headers come first, other project headers are alphabetical, and standard headers follow them. Generated table `.inc` fragments come after their prerequisite headers and constants, but before explicit declarations or functions. The CDF table types now live in [`internal/coeff_cdf_specs.h`](../src/decoder/internal/coeff_cdf_specs.h), allowing every CDF include to move to the top without changing generated tables. C6's typed implementation fragments follow the explicit exception below.
- File-scope constants, aliases, and type definitions precede function definitions. Conditional ABI imports and constants retain their original architecture guards. Header guards and `_POSIX_C_SOURCE` remain before includes where preprocessing requires it; function-local temporary macros retain their local uses and `#undef`s.
- Pointer comparisons, assignments, arguments, and returns use `NULL`, with direct `<stddef.h>` includes at use sites. Pointer-first universal zero initializers use designated `NULL` members, preserving implicit zero initialization of other members. Compiler-assisted rewrites and a separate macro audit distinguish pointer zero from integer, enum, pixel, and raw syscall values.
- All 14 formerly byte-returning predicates now return `int`, including their declarations and callers. Encoded context indices, quantizer selectors, pixel values, and other numeric results retain their original types even when their current value range happens to be 0/1.
- The three `sizing_only` fields and task-pool `stop` flag are `uint8_t`, stored as 0U/1U. Platform wait words, counters, thread IDs, and Win32 ABI types retain their required integer widths. Public API declarations and workspace budgets are unchanged.

Validation:

- Eight new checks in [`test_source_consistency.py`](../tests/test_source_consistency.py) cover include ordering, file-scope macro/type placement, direct `NULL` dependencies, ordinary and macro-based pointer checks, predicate signatures, and byte-sized flags. All 15 targeted Python checks, including retained C1 ownership and private-header checks, pass.
- Compiler audits of all 140 source/header files in both diagnostic modes find no remaining typed null literals, implicit pointer truth checks, or audited macro-pointer zero comparisons.
- [`substrate_unit.c`](../tests/substrate_unit.c) compares writer/arena sizes, alignments, and cursor/status/flag offsets with their pre-C2 layouts. [`runtime_substrate_unit.c`](../tests/runtime_substrate_unit.c) verifies byte-sized stop storage without moving or narrowing the platform synchronization word.
- Forced sanitized rebuilds pass all 31 C unit suites, runtime checks, 2,460 malformed-input fuzz cases, and frozen status/diagnostic/pixel/output-policy checks with `AV2HEIF_ENABLE_DIAGNOSTICS` absent and enabled. Private-header checks pass 55 standalone headers and 35 value-only kernel roots.
- The rebuilt native decoder matches all 179 available pre-cleanup decoded SHA-256 values across the existing 8-bit and 10-bit profiles. Freestanding undefined-symbol checks and affected macOS/Linux/Windows frontend object compilations pass; separate macOS `nolibc` executable linking retains the existing limitation recorded under C1.
- Full Python discovery runs 97 tests: 94 pass, two skip, and the unchanged legacy Emerald artifact check errors because its generated reference file is absent. That optional image asset was not regenerated or replaced as part of C2.

### C2 follow-up after C6

The original C2 criterion prohibited every late include. C3-C5 retained that
layout; C6 introduced implementation-template instantiations after shared
helpers and exempted their six-include sequences from the checker. That was a
change to the convention, not preservation of the original criterion.

The follow-up retains C6's shared algorithms and makes the exception explicit:

1. Keep ordinary headers, table includes, file constants, types, and necessary
   private forward declarations above function definitions. Do not duplicate
   declarations already supplied by an included interface header.
2. Put shared helpers before the typed implementations. Each of the 17 owners
   ends with one section beginning with
   `/* Instantiate typed kernels after the shared helpers. */`.
   In headers, this section immediately precedes the closing header guard.
3. Keep the two instantiations contiguous and in this exact order:
   byte configuration, the owner's kernel, reset; then 16-bit configuration,
   the same kernel, reset. Source owners use `internal/` paths; internal
   header owners use local paths. Never alphabetize these six includes.
4. The section must be at file scope, outside additional conditionals, with no
   declarations, functions, or other directives following it except the
   header's closing guard. Comments and whitespace are allowed. Kernel
   fragments cannot introduce dependency includes or define/undefine macros;
   the configuration/reset files own exactly the three specialization macros.

Removed 83 redundant forward declarations from the 15 source owners, including
all eight in [`ccso.c`](../src/decoder/ccso.c). Necessary private forwards remain
for earlier callers in directional prediction, MHCCP, and workspace helpers.
The filter-common header now follows the same trailing-section layout as the
other owners; no new headers, translation units, numerical branches, or
workspace types were needed.

[`test_source_consistency.py`](../tests/test_source_consistency.py) checks the
marker, ownership, file scope, terminal placement, include order, and exact
configuration/reset contents. Negative cases cover nested functions,
conditionals, commented-out sequences, duplicate sections, missing resets, and
code or directives after a section. Predicate checks now inspect expanded
kernel definitions as well as ordinary sources, rather than relying on
redundant prototypes to expose their return types.

Validation:

- All 27 related source/header/frontend tests pass; the updated Python guard
  has no syntax or editor diagnostics.
- All 15 source owners compile with `-Wredundant-decls -Werror` with
  diagnostics absent and enabled.
- Both ASan/UBSan configurations pass all 33 C suites, runtime checks,
  2,460 fuzz cases, and the 55-header/35-kernel-root dependency checks.
- Both frozen behavior/resource inventories, all 179 native decoded hashes,
  and all 14 exact CLI contracts remain unchanged.
- Default `make` and freestanding checks pass. The rebuilt native executable
  is byte-for-byte identical to the pre-follow-up executable, so this cleanup
  does not alter its numerical code or decode performance.

---

## C3: Unify Type and Symbol Naming (Remove `Av2hFirst*` Legacy Prefix)

### Problem
Roughly 700 occurrences across 54 files in [`src/decoder/`](../src/decoder/) still carry the `First` prefix from early single-leaf bring-up milestones:
- `Av2hFirstLeafMode`, `Av2hFirstLeafIntraMode`, `Av2hFirstChromaMode`
- `Av2hFirstPartitionPrefix`, `Av2hFirstLeafPath`
- `Av2hFirstCoeffCdfs`, `Av2hFirstCoeffPrefix`, `Av2hFirstCoeffBlock`, `Av2hFirstCoeffLevelCdfs`
- `Av2hFirstTransformCdfs`, `Av2hFirstTransformType`, `Av2hFirstTxSize`
- `Av2hFirstReconstruction`, `Av2hFirstReconstructionU16`
- Associated constants and functions (`AV2H_FIRST_COEFF_CAPACITY`, `av2h_first_coeff_cdfs_init`, etc.)

Because these types now model general per-block and per-transform-unit state across entire multi-tile frames, the `First` qualifier is misleading.

### Actions
1. **Rename Core Decoder Types:** Strip `First` / `first_` from all general-purpose decoder structs, enums, constants, and functions:
   - `Av2hFirstLeafMode` $\rightarrow$ `Av2hLeafMode` (or `Av2hLumaLeafMode`)
   - `Av2hFirstLeafIntraMode` $\rightarrow$ `Av2hIntraMode`
   - `Av2hFirstChromaMode` $\rightarrow$ `Av2hChromaLeafMode`
   - `Av2hFirstCoeffBlock` $\rightarrow$ `Av2hCoeffBlock` (`AV2H_COEFF_CAPACITY`)
   - `Av2hFirstCoeffPrefix` $\rightarrow$ `Av2hCoeffPrefix`
   - `Av2hFirstCoeffCdfs` / `Av2hFirstCoeffLevelCdfs` $\rightarrow$ `Av2hCoeffCdfs` / `Av2hCoeffLevelCdfs`
   - `Av2hFirstTransformType` / `Av2hFirstTransformCdfs` / `Av2hFirstTxSize` $\rightarrow$ `Av2hTransformType` / `Av2hTransformCdfs` / `Av2hTxSize`
   - `Av2hFirstReconstruction` $\rightarrow$ `Av2hBlockReconstruction`
2. **Update Internal Callers & Tests:** Update all internal headers, implementation files, and unit tests in lockstep without leaving compatibility `typedef`s behind.

### Exit Criteria
- `grep "Av2hFirst\|av2h_first_\|AV2H_FIRST_" src/` returns zero matches (unless a function specifically tests or parses only the very first syntactic element by design).

### Completion

C3 is complete. Renamed 15 types, the coefficient-capacity constant, and
12 functions throughout the decoder, internal interfaces, unit tests, and
experimental encoder test drivers. No compatibility typedefs or old-name
aliases were added. Enum values, struct members, array dimensions, signatures,
default arguments, and the 1024-coefficient capacity are unchanged.

The canonical types follow the mappings above, including `Av2hTxType`,
`Av2hPartitionPrefix`, `Av2hLeafPath`, and `Av2hBlockReconstructionU16`.
Coefficient/transform CDF initializers and generic readers no longer use
`first_`. Wrappers whose shorter destination names already existed retain
their separate implementations with descriptive names:

| Canonical wrapper | Preserved behavior |
| :--- | :--- |
| `av2h_luma_reconstruct_without_predictor` | Calls luma reconstruction with a `NULL` predictor and zero stride |
| `av2h_coeff_block_read_default_context` | Supplies DC-sign context zero |
| `av2h_chroma_coeff_prefix_read_default_context` | Supplies skip context 6 for U and 9 for V |
| `av2h_chroma_transform_type_read_dc` | Supplies a zero-initialized DC mode and `cctx_allowed = 1` |
| `av2h_chroma_reconstruct_dc_4x4` | Retains 4x4 DCT_DCT reconstruction with DC intra prediction |

At C3 completion, two origin-only parsers were explicit naming exceptions.
C4 subsequently moved them to [test helpers](../tests/decoder_test_helpers.h),
preserving their `AV2H_UNSUPPORTED` checks outside frame origin `(0, 0)`.
Production sources now have zero legacy-name occurrences. The wrapper names
in the table above record C3's rename step; C4 subsequently retired those
forwarding layers.

[`test_decoder_names.py`](../tests/test_decoder_names.py) rejects other legacy
names in source, public headers, C tests, generators, and experimental C
consumers; requires one owner for each canonical type; and guards both
first-element restrictions. The experimental parity drivers also import their
actual prediction interfaces rather than the obsolete reconstruction umbrella.
Developer API references use current names; the original measured labels in
the [M0 baseline record](dev/refactoring-m0.md) remain historical evidence.

Validation:

- Fresh ASan/UBSan builds with diagnostics absent and
  `AV2HEIF_ENABLE_DIAGNOSTICS=1` pass all 31 C unit suites, runtime tests,
  2,460 malformed-input fuzz cases, and frozen pixel/status/error checks.
- The complete 77-line behavioral/resource inventory is identical after
  normalizing type names, including private layouts and workspace measurements.
- C1/C2/C3 source guards and private-header tests pass; all 55 standalone
  headers and 35 value-only kernel roots compile in both diagnostic modes.
- Forced native macOS rebuild and freestanding symbol checks pass. All
  179 available corpus outputs retain their SHA-256 hashes: 133 8-bit
  I400/I420/I444 cases and 46 10-bit I420/I444 cases.
- Both experimental encoder project-driver parity tests compile and execute
  successfully against the rebuilt native library.

---

## C4: Retire Historical Wrappers and Compatibility Aliases

### Problem
Incremental milestones left behind multi-layered forwarding functions and preprocessor aliases that no longer served an architectural boundary:
- **Multi-generation function chains:** Before C4, [`src/decoder/reconstruct.h`](../src/decoder/reconstruct.h) exposed six overlapping chroma plane reconstruction entry points (`av2h_chroma_reconstruct_to_plane`, `_ex`, `_format`, `_format_with_diagnostics`, `_u16`, `_u16_with_diagnostics`), while luma reconstruction duplicated `av2h_luma_reconstruct_without_predictor`, `av2h_luma_reconstruct`, `av2h_luma_reconstruct_with_diagnostics`, `av2h_luma_reconstruct_to_plane`, and `av2h_luma_reconstruct_to_plane_with_diagnostics`.
- **Similar `_with_diagnostics` / `_with_execution` pass-throughs:** Several internal modules expose both a base function and a `_with_diagnostics` or `_with_sequence` wrapper where `diagnostics == NULL` is already safely handled as a no-op by the underlying implementation.
- **Compatibility `#define`s after file splits:** Files like [`reconstruct.c`](../src/decoder/reconstruct.c) (`#define wide_angle_map av2h_luma_wide_angle`), [`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), and [`tile_luma.c`](../src/decoder/tile_luma.c) use `#define` macros to alias old local function names to new module-prefixed names.

### Actions
1. **Collapse Internal Function Chains:** Where an internal function accepts optional parameters (`diagnostics`, `predictor_plane`, `execution`) that safely accept `NULL`, keep a single canonical internal entry point and update callers directly.
2. **Retire Single-Leaf Test Scaffolding Wrappers:** If a legacy wrapper is used only by early unit tests, either migrate those unit tests to the canonical API or move test-only convenience helpers into the test suite rather than keeping them in production headers.
3. **Remove Preprocessor Name Aliases:** Replace `#define` symbol aliases in `.c` files with direct calls to the actual `av2h_*` function names.

### Exit Criteria
- Each internal operation in [`src/decoder/`](../src/decoder/) has one canonical entry point per responsibility.
- Zero `#define` function-renaming shims remain in `.c` files.

### Completion

C4 is complete. Removed 40 forwarding definitions and four production
scaffolding definitions, and replaced all 15 decoder function-alias macros
with direct, explicitly parameterized calls. Internal declarations, production
callers, tests, and affected experimental consumers were migrated together.
No compatibility aliases were introduced.

Canonical interfaces now take formerly hidden defaults explicitly:

| Owner | Consolidated operation |
| :--- | :--- |
| [`reconstruct.h`](../src/decoder/reconstruct.h) and private reconstruction interfaces | Block and plane reconstruction receive optional diagnostics directly; chroma callers specify format, U/V selection, and any luma/predictor planes |
| [`filter.h`](../src/decoder/filter.h) | CDEF, CCSO, and GDF receive sequence/control maps directly; 8-bit GDF also receives its baseline plane explicitly |
| [`leaf.h`](../src/decoder/leaf.h) | Chroma syntax receives delta/SDP settings directly; stateful luma syntax receives optional filter/Q-index state and deferred-palette settings |
| [`coeff.h`](../src/decoder/coeff.h), [`coeff_level.h`](../src/decoder/coeff_level.h) | Prefix/block callers supply entropy contexts; coefficient-CDF initialization supplies the quantizer index |
| [`frame_output.h`](../src/decoder/frame_output.h) and [`frame_workspace.h`](../src/decoder/internal/frame_workspace.h) | Copy/decode operations receive optional execution; size/workspace operations receive bytes per sample |
| [`frame.h`](../src/decoder/frame.h), [`tile.h`](../src/decoder/tile.h), [`partition.h`](../src/decoder/partition.h) | Header parsing receives optional diagnostics; tile initialization and partition parsing receive their selection/structure values |

Previously omitted pointers still pass `NULL`; their existing no-op/default
semantics, validation order, and diagnostics are unchanged. Presets such as
QP 128, zero DC-sign context, U/V skip contexts 6/9, format 0, and automatic
tile selection remain explicit at the original callers.

The two origin-only parser conveniences now live exclusively in
[`decoder_test_helpers.h`](../tests/decoder_test_helpers.h). Their rejection
checks and executing tests are retained. The unused DC-4x4 reconstruction and
DC-mode chroma-transform conveniences were deleted outright.

Distinct responsibilities remain separate: block versus plane ownership,
entropy-span to tile-group adaptation, state-derived entropy contexts,
baseline versus quantizer-adjusted level-CDF initialization, binary-CDF and
coordinate-unit adapters, and typed luma/chroma or 8/16-bit interfaces.
The remaining six single-call adapters enforce these typed/unit contracts,
rather than supplying historical optional defaults. C5/C6 remained outside
the scope of C4; their completed implementations are documented below.
Platform syscall ABI-selection macros remain OS bindings, not decoder
compatibility shims.

[`test_decoder_entrypoints.py`](../tests/test_decoder_entrypoints.py) rejects
retired symbols and decoder function-alias macros, and permits only the six
documented typed/unit forwarding adapters.

Validation:

- Both diagnostic configurations pass all 31 ASan/UBSan C unit suites,
  runtime tests, 2,460 malformed-input fuzz cases, and frozen pixel/status/error
  checks.
- All 77 lines of the behavioral/resource inventory are byte-identical to
  the pre-C4 baseline, without name normalization.
- All 21 source/header guards pass; 55 standalone headers and 35 value-only
  kernel roots compile in both diagnostic modes.
- Forced native macOS rebuild and freestanding symbol checks pass. All
  179 decoded corpus SHA-256 hashes remain unchanged.
- Both experimental encoder project-driver parity tests compile and run
  against the rebuilt native library. The public API, workspace layouts,
  heap-allocation policy, media, oracle revisions, and expected hashes are
  unchanged.

---

## C5: Deduplicate Platform CLI Frontends

### Problem
Before C5, the three standalone CLI entry points—[`src/platform/posix/decoder.c`](../src/platform/posix/decoder.c) (471 lines), [`src/platform/nolibc/decoder.c`](../src/platform/nolibc/decoder.c) (519 lines), and [`src/platform/windows/decoder.c`](../src/platform/windows/decoder.c) (869 lines)—duplicated significant platform-independent logic:
- Raw OBU stream orchestration (`decode_obu`, `find_obu`, sequence/frame/qmatrix/tile-group setup, workspace query and binding).
- Output image encoding (`write_bmp`, `write_png` row callbacks, raw extension matching).
- Diagnostic formatting and stage-to-string tables (heavily expanded in [`windows/decoder.c`](../src/platform/windows/decoder.c) and partially duplicated or Divergent in POSIX/`nolibc`).

### Actions
1. **Extract Shared CLI Core into [`src/shared/`](../src/shared/):**
   Create a freestanding, OS-independent CLI support module (e.g., `src/shared/cli_common.h` / `src/shared/cli_common.c` or `src/preview.c` extensions) that owns:
   - Raw OBU decode orchestration (using a tiny allocator callback interface for workspace/output memory so `posix_memalign`, `mmap`, and `VirtualAlloc` plug in cleanly).
   - BMP header/row formatting and PNG writer wiring via `Av2hPngWriteCallback`.
   - Shared diagnostic text formatting into a caller-supplied write callback.
2. **Slim Platform Drivers to Pure OS Adapters:**
   Keep [`src/platform/posix/decoder.c`](../src/platform/posix/decoder.c), [`src/platform/nolibc/decoder.c`](../src/platform/nolibc/decoder.c), and [`src/platform/windows/decoder.c`](../src/platform/windows/decoder.c) strictly focused on:
   - Entry point and command-line argument parsing (`main` / `_start` / `mainCRTStartup` + UTF-16 handling on Windows),
   - File open/read/write/close primitives,
   - Page/heap memory allocation and task-pool initialization.

### Exit Criteria
- OBU decode orchestration, BMP/PNG serialization glue, and diagnostic formatting exist in exactly one place.
- Each platform `decoder.c` contains only OS-specific I/O, memory, and entry-point glue.

### Completed Implementation

- [`cli_common.h`](../src/shared/cli_common.h) defines the internal CLI-only
  allocator, owned-image, output-file, narrow/UTF-16 path, and diagnostic
  callback contracts.
- [`cli_common.c`](../src/shared/cli_common.c) owns OBU lookup and parsing,
  format admission, qmatrix and tile-group setup, workspace/output allocation,
  execution binding, optional retained diagnostics, and RAW/BMP/PNG output.
  It reuses the existing pixel-format registry, planar preview conversion,
  output-layout arithmetic, and PNG writer.
- [`cli_diagnostics.c`](../src/shared/cli_diagnostics.c) owns usage and error
  messages, POSIX JSON and compact diagnostics, and the complete Windows
  stage/syntax/coefficient/IntraBC/partition trace formatter. The first
  callback failure stops emission and is returned to the frontend.
- [`Makefile`](../Makefile) compiles shared CLI objects separately for native,
  sanitized-host, `nolibc`, and Windows executables. They are not added to the
  decoder core archive or public API. Native freestanding symbol validation
  includes the CLI objects.
- The frontends retain only startup/argument handling, OS allocation and
  file/console adapters, task-pool setup, and service invocation:

  | Frontend | Before | After |
  | :--- | ---: | ---: |
  | [POSIX](../src/platform/posix/decoder.c) | 471 lines | 146 lines |
  | [`nolibc`](../src/platform/nolibc/decoder.c) | 519 lines | 260 lines |
  | [Windows](../src/platform/windows/decoder.c) | 869 lines | 226 lines |

### Preserved Platform Policies

- POSIX retains late format admission, frame-parser error offsets, LF human
  errors on stderr, optional `--diagnostics` JSON on stdout, and success JSON
  only after successful output writing.
- `nolibc` retains early admission, status-only LF errors, syscall adapters,
  and task-pool execution/destruction around decoding.
- Windows retains UTF-16 parsing and `CreateFileW`, early admission, CRLF,
  and diagnostics-enabled rich traces on success and failure before output
  validation. Its separately allocated trace remains valid after decoder
  workspace release and is freed before preview/output serialization.
- Raw-extension mismatches still exit 2; unsupported decoding exits 3;
  other decode/output failures exit 1. Raw mismatch and high-bit-depth preview
  rejection do not open or truncate the destination.
- Allocator results are checked for capacity and alignment, and partial
  allocations are released. Owned sizes describe capacity; serialization
  writes logical pixel/row lengths, not extra allocator capacity. File write
  callbacks must complete the supplied buffer or return an error. Open,
  partial-write, close, and diagnostic-write failures are propagated;
  POSIX JSON flushing failures now explicitly report a diagnostic-output
  error rather than appearing successful.

### Validation

- Both diagnostics-absent and diagnostics-enabled ASan/UBSan builds pass all
  32 C suites, runtime tests, 2,460 fuzz cases, and existing output-policy and
  frozen error/pixel checks.
- [`cli_unit.c`](../tests/cli_unit.c) covers narrow/UTF-16 suffixes, lexical
  raw-extension matching, odd-sized BMP headers/padding and PNG dimensions,
  over-capacity allocators, failed/partial/invalid allocation results, trace
  lifetime, no-open rejection policies, every serialization write-failure
  position, close errors, and sticky diagnostic callback failures.
- [`test_cli_frontends.py`](../tests/test_cli_frontends.py) guards frontend
  ownership and separation of CLI objects from the core archive. All 23
  targeted cleanup/source/header guards pass, as do the existing 55 standalone
  private-header and 35 value-only kernel-root checks in both configurations.
- All 14 captured native CLI contracts match exactly: exit codes, stdout,
  stderr, output sizes/hashes, and pre-existing destination preservation.
  Nine success/unsupported/truncated rich traces match the original Windows
  formatter byte-for-byte using native sanitized callbacks.
- All 77 frozen behavior/resource inventory lines are unchanged. The rebuilt
  macOS binary passes freestanding symbol checks and retains all 179 available
  corpus hashes across I400, I420, I444, and both 10-bit color formats.
  Shared services and `nolibc`/Windows adapters pass freestanding syntax checks
  in both diagnostic modes; foreign runtime builds and media regeneration
  were not required.
- Decoder public APIs, workspace layouts, core allocation policy, samples,
  oracle revisions, and expected hashes are unchanged. Sample-kernel
  deduplication was not part of C5; it is completed in C6 below.

---

## C6: Deduplicate 8-Bit, 10-Bit, and Future 12-Bit Sample Paths

**Status:** Complete on native macOS.

### Problem
The AV2 specification supports 8-bit, 10-bit, and 12-bit sample depths (`bit_depth_idc` 1, 0, and 2—note [`src/container.c`](../src/container.c) already recognizes `bit_depth_idc == 2` as 12-bit). Before C6, [`src/decoder/`](../src/decoder/) duplicated nearly the entire sample-processing pipeline between `uint8_t` and `uint16_t` (`_u16`, ~275 occurrences across 31 files):
- **Plane and Scratch Types:** `Av2hReconstructionPlane` vs `Av2hReconstructionPlaneU16`, `Av2hReconstructionScratch` vs `Av2hReconstructionScratchU16`, `Av2hFilterPlane` vs `Av2hFilterPlaneU16`.
- **Prediction & Reconstruction:** Near-identical functions in [`reconstruct.c`](../src/decoder/reconstruct.c), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c), [`intra_prediction_*.c`](../src/decoder/intra_prediction.c), and [`frame_output.c`](../src/decoder/frame_output.c).
- **Loop Filters:** Repeated 8-bit and 16-bit kernels in [`deblock.c`](../src/decoder/deblock.c), [`cdef.c`](../src/decoder/cdef.c), [`ccso.c`](../src/decoder/ccso.c), [`gdf.c`](../src/decoder/gdf.c), and [`restoration.c`](../src/decoder/restoration.c).
- Adding 12-bit support under that pattern would either require parameterizing `_u16` everywhere by `bit_depth` / `Av2hSampleDomain` or risk further branching duplication.

### Actions
1. **Parameterize All High-Bit-Depth (`u16`) Kernels by `Av2hSampleDomain` / `bit_depth`:**
   - Replace hardcoded 10-bit assumptions (`av2h_sample_clip_10bit`, `av2h_sample_midpoint_10bit`, hardcoded `10U` shifts) in `_u16` functions with `const Av2hSampleDomain *domain` (8-bit, 10-bit, 12-bit).
2. **Unify Geometry, Admission, and Control Flow:**
   - In [`reconstruct.c`](../src/decoder/reconstruct.c), [`frame_output.c`](../src/decoder/frame_output.c), and [`filter.c`](../src/decoder/filter.c), factor out all bit-depth-independent logic (block coordinate calculation, reference availability checks, readiness maps,CfL alpha sign/magnitude decoding, transform descriptor lookup, deblock map commits) into shared functions so that `u8` and `u16` only diverge at the actual sample load/store or inner numerical kernel.
3. **Evaluate Unified `uint16_t` Internal Working Buffers vs Typed X-Macro/Template Headers:**
   Two clean C11 patterns are available (to be chosen per subsystem based on workspace budget and benchmark impact):
   - **Option A (Parameterized Typed Kernel Includes):** Keep `uint8_t` (1 byte/sample) and `uint16_t` (2 bytes/sample) plane storage to preserve the compact 8-bit workspace footprint, but move duplicated kernel bodies (`reconstruct`, `cfl`, `mhccp`, `deblock`, `cdef`, `ccso`, `gdf`) into single-source parameterized internal templates (instantiated once for `uint8_t` and once for `uint16_t` with `Av2hSampleDomain`).
   - **Option B (Unified `Av2hSampleDomain` Scratch/Kernel Path):** For small bounded scratch buffers (such as a single transform block's 128-sample reference edges `above[128]` / `left[128]` and predictor block), operate in `uint16_t` parameterized by `Av2hSampleDomain` (8, 10, or 12 bit) and convert only at plane load/commit boundaries.
4. **Fix Redundant Guards Discovered During Audit:**
   - Clean up copy-paste artifacts between the 8-bit and 16-bit paths (such as the duplicated `mode->is_cfl` condition in [`src/decoder/reconstruct.c`](../src/decoder/reconstruct.c)).

### Exit Criteria
- Prediction, reconstruction, and loop-filter algorithms are maintained as a single source of truth rather than parallel copy-pasted `u8` and `u16` implementations.
- All `u16` sample paths use `Av2hSampleDomain` (or explicit `bit_depth`) rather than hardcoded 10-bit constants, making 12-bit sample plumbing straightforward.
- Workspace sizes, performance, and exact decoded outputs remain within frozen baselines.

### Implemented

- Chose typed kernel includes rather than widening 8-bit planes, reference
  edges, predictors, or residual workspaces. Sixty-one typed function families
  now have a shared source across 17 internal kernel includes, covering
  prediction, reconstruction, inverse transforms, CDEF, CCSO, GDF, deblocking,
  plane operations, output copying/validation, and filter geometry.
  Each owner instantiates its kernel for byte and 16-bit storage using
  [`sample_kernel_u8.inc`](../src/decoder/internal/sample_kernel_u8.inc),
  [`sample_kernel_u16.inc`](../src/decoder/internal/sample_kernel_u16.inc), and
  an explicit macro-reset include.
- Directional projection loops are shared independently of typed reference
  preparation and admission. The lossless 4x4 inverse transform and DPCM
  accumulation share one implementation in
  [`inverse_transform.c`](../src/decoder/inverse_transform.c).
  CfL sign/magnitude decoding is shared in
  [`reconstruct.c`](../src/decoder/reconstruct.c). Existing common readiness
  maps, transform descriptors, and deblock-map commits remain common owners.
- [`sample_math.h`](../src/decoder/internal/sample_math.h) provides canonical
  8/10/12-bit domains, validated domain values, IDC-to-domain lookup, and Q3
  limits. High-depth numerical paths receive explicit domains or bit depths
  for clipping, fallback samples, transform bounds, prediction, and filter
  normalization. Optional sequence arguments in CCSO/GDF remain optional;
  sample domains are not obtained by dereferencing those arguments.
- Corrected tightly coupled 12-bit arithmetic: CfL average limits and MHCCP
  quadratic features/normalization are depth-dependent. Extreme 12-bit MHCCP
  reference products require bounded 64-bit statistics before narrowing into
  the existing fixed-point solver. These temporaries do not change allocated
  workspace layouts. GDF normalizes spatial inputs to its learned 10-bit
  units and scales output corrections to the sample depth. Its learned-network
  clamp and transform/diagnostic constants remain fixed rather than being
  mistaken for sample-depth limits.
- Preserved intentional policy differences in the typed branches, including
  predictor handling, transform admission, diagnostics, reference boundaries,
  and high-depth filter alias checks. Restoration and DIP remain 8-bit-only
  where previously supported. A dispatcher and a forwarding adapter are not
  treated as duplicated numerical algorithms.
- The decoder still admits the same 8/10-bit image profiles. This milestone
  prepares numerical plumbing for 12-bit; it does **not** enable 12-bit stream
  decoding, claim 12-bit AVM conformance, or introduce new restoration support.
  No AVM code, oracle revisions, fixtures, or golden hashes were changed.
- Make tracks transitive header/kernel-include dependencies for core and CLI
  objects. Source guards allow only complete, correctly ordered, owner-matched
  typed instantiation scopes; ordinary late includes and macros remain errors.

### Validation

- All 33 C suites plus runtime checks pass with ASan/UBSan and halting
  undefined-behavior checks, with diagnostics absent and enabled. Each
  configuration also passes 2,460 public-decode fuzz cases.
- [`sample_domain_unit.c`](../tests/sample_domain_unit.c) directly tests domain
  validation, 12-bit prediction/fallbacks and padded strides, 4:2:0/4:4:4 CfL
  averages above the old 10-bit limit, inverse-transform/DPCM results, CDEF
  direction normalization, CCSO band selection/clipping, deblocking, GDF, and
  extreme MHCCP accumulation. A nonconstant, padded-stride GDF regression
  verifies fourfold 10-to-12-bit input/output scaling across all four QP
  selections, with nonzero corrections and untouched padding.
- All 26 targeted source/ownership/header guards pass. The 55 standalone
  headers and 35 value-only kernel roots compile in both diagnostic modes,
  with no include cycles. All 77 frozen behavior/resource inventory lines
  remain byte-identical in each configuration.
- The default `make` produces the working macOS binary, with no undefined
  freestanding symbols. All 179 retained native corpus hashes and all 14 exact
  CLI output/error contracts remain unchanged, including 12-bit rejection and
  preservation of pre-existing output files.
- In-process decode timings exclude startup/input I/O and retain allocations.
  Fifteen cases cover the smallest, middle, and slowest previously measured
  case in each of I400, I420, I444, and both 10-bit color formats. Five
  interleaved before/after runs against retained executables show per-case
  median changes from -1.20% to +2.40%, a median change of -0.06%, and a
  time-weighted change of -0.43%. The largest relative increase was approximately
  0.7 microseconds on a 30-microsecond case. No material regression was observed; compiler
  optimization defaults are unchanged. Foreign runtime builds and media
  regeneration were not required.

---

## C7: De-interleave Heavily Branched `*_kernel.inc` Templates

**Status:** Complete on native macOS.

### Problem
While many of the C6 kernel templates ([`deblock_kernel.inc`](../src/decoder/internal/deblock_kernel.inc), [`ccso_kernel.inc`](../src/decoder/internal/ccso_kernel.inc), [`gdf_kernel.inc`](../src/decoder/internal/gdf_kernel.inc), [`cdef_kernel.inc`](../src/decoder/internal/cdef_kernel.inc)) share 90–98% of their code with only minimal `#if AV2H_KERNEL_WIDE` adjustments for bit-depth shifts, a few of the largest `.inc` files interleave two structurally different implementations using frequent `#if AV2H_KERNEL_WIDE ... #else ... #endif` blocks:
- **[`reconstruct_kernel.inc`](../src/decoder/internal/reconstruct_kernel.inc) (1,370 lines, 70 `#if AV2H_KERNEL_WIDE` blocks):** In `av2h_luma_reconstruct_to_plane` and `av2h_chroma_reconstruct_to_plane`, the 8-bit and 16-bit paths have different local variables, different admission checks, and different reference-collection flows braided together every few lines (occasionally splitting `if (...) {` and `}` across preprocessor branches).
- **[`reconstruct_block_kernel.inc`](../src/decoder/internal/reconstruct_block_kernel.inc) (1,009 lines, 25 `#if AV2H_KERNEL_WIDE` blocks):** Function parameter lists vary conditionally (`#if !AV2H_KERNEL_WIDE size_t predictor_stride_samples, #endif`), and large sections of `av2h_luma_reconstruct` / `av2h_reconstruct_transform_generic` execute distinct code under `#if AV2H_KERNEL_WIDE` vs `#else`.
- **[`chroma_prediction_cfl_kernel.inc`](../src/decoder/internal/chroma_prediction_cfl_kernel.inc) (457 lines) and [`chroma_prediction_mhccp_kernel.inc`](../src/decoder/internal/chroma_prediction_mhccp_kernel.inc) (413 lines):** `av2h_cfl_derived_predict` and `av2h_mhccp_predict_observed` have conditional parameters (`derive_alpha`, `supplied_alpha_q3`, `chroma_format_idc`, `diagnostics`) and diverge into largely separate 8-bit and 16-bit bodies inside the same function template.

Braiding divergent control flow with `#if / #else` inside a single function body hurts readability and makes reasoning about braces, variable lifetimes, and control flow harder than necessary.

### Actions
1. **Forbid Split Control-Flow Braces Across `#if AV2H_KERNEL_WIDE`:**
   Never open a block (`{`) inside `#if AV2H_KERNEL_WIDE` and close it in `#else` or outside the conditional. Every preprocessor branch must be syntactically balanced on its own.
2. **Separate Bit-Depth-Independent Admission and Reference Geometry from Typed Sample Loops:**
   Extract shared bit-depth-independent calculations (such as `above_target` / `left_target` edge lengths, MRL boundary flags, CfL/MHCCP region bounds, and DPCM scale bits) into non-templated helper structs/functions in the owning `.c` file.
3. **Keep Only Genuinely Shared Numerical Loops in `*_kernel.inc`:**
   - Where 8-bit and 16-bit paths share the same algorithm (e.g., downsampling filters, regression accumulation, predictor + residual clipping, palette lookup), keep them in clean, un-braided `*_kernel.inc` functions with uniform parameter lists (using `predictor_stride_samples` consistently or passing `const Av2hSampleDomain *domain` uniformly to internal helpers).
   - Where 8-bit and 16-bit entry points intentionally enforce different historical admission policies or dispatch rules, keep those top-level admission wrappers separate in the `.c` owner and have both call the shared typed inner kernels.

### Exit Criteria
- Zero unbalanced braces or split control-flow statements across `#if AV2H_KERNEL_WIDE` branches.
- `#if AV2H_KERNEL_WIDE` in `*_kernel.inc` files is restricted to genuine sample-width / domain differences (types, bit-depth shift amounts, and external signature adapters where required), rather than switching between two unrelated function bodies.

### Completed Implementation

- Moved the 18 historically divergent reconstruction, generic-transform, derived
  CfL/MHCCP, smooth, and Paeth entry points into their ordinary `.c` owners.
  Their admission order, optional predictors, return codes, and diagnostic
  stages remain separate and unchanged. The long legacy transform-selection
  ladder remains intact for C8.
- Extracted ordinary helpers for visible reconstruction bounds, reference
  extension lengths, MRL superblock/angle geometry, CfL reference counts and
  regression, MHCCP reference extents and vector prediction, and DPCM scaling.
- Kept shared typed numerical kernels for palette prediction, DPCM accumulation,
  generic inverse transforms, all three CfL current-block filters and 4:4:4
  application, MHCCP downsampling/statistics, smooth/Paeth, and directional
  projection. Their internal parameter lists are uniform. DPCM explicitly
  preserves byte-path floor rounding versus wide-path truncation, and MHCCP
  retains 64-bit wide statistics for future 12-bit samples.
- Directional projection receives explicit interpolation/boundary policy flags
  from its ordinary owner instead of selecting control flow with preprocessor
  branches. The seven policy-heavy templates now contain no
  `AV2H_KERNEL_WIDE` conditionals:

  | Template | Before (lines) | After (lines) |
  | :--- | ---: | ---: |
  | [`reconstruct_kernel.inc`](../src/decoder/internal/reconstruct_kernel.inc) | 1,370 | 36 |
  | [`reconstruct_block_kernel.inc`](../src/decoder/internal/reconstruct_block_kernel.inc) | 1,009 | 28 |
  | [`chroma_prediction_cfl_kernel.inc`](../src/decoder/internal/chroma_prediction_cfl_kernel.inc) | 457 | 81 |
  | [`chroma_prediction_mhccp_kernel.inc`](../src/decoder/internal/chroma_prediction_mhccp_kernel.inc) | 413 | 96 |
  | [`inverse_transform_kernel.inc`](../src/decoder/internal/inverse_transform_kernel.inc) | 212 | 69 |
  | [`intra_prediction_basic_kernel.inc`](../src/decoder/internal/intra_prediction_basic_kernel.inc) | 217 | 60 |
  | [`intra_prediction_directional_kernel.inc`](../src/decoder/internal/intra_prediction_directional_kernel.inc) | 263 | 198 |

- Removed split validation/control flow from the smaller filter, reference,
  edge, and frame-copy templates too. GDF's differing argument/alias validation
  now lives in ordinary owner helpers; its shared filtering traversal remains
  typed. Remaining width conditionals describe sample arithmetic, storage,
  accessors, or signature adapters.
- Preserved all 17 single-owner trailing instantiation sections and their
  configuration resets. Necessary private forwards remain above ordinary
  functions; both configurations also pass `-Wredundant-decls`. No public
  header, workspace layout, stream admission, oracle, or media changes were
  needed. C8-C10 remain future work.

### Validation

- All 33 sanitized C suites and 2,460 deterministic public-decode fuzz cases
  pass with diagnostics absent and enabled. Both frozen 77-line admission,
  numerical, layout, and diagnostic inventories match exactly.
- Added independent DPCM scale/negative-rounding/stride checks and CfL filter
  checks across the 32-sample boundary, padded output strides, and 8/10/12-bit
  sample arithmetic. This does not enable 12-bit stream decoding.
- All 30 related Python checks pass. The token-aware source guard checks every
  conditional branch in all 17 templates for unmatched delimiters and dangling
  control-flow headers, ignoring comments and strings. It also keeps divergent
  public admission outside the seven numerical templates and protects their
  uniform, width-branch-free implementations.
- All 55 standalone private headers and 35 value-only kernel roots pass in
  both configurations, without include cycles. The native default build and
  freestanding undefined-symbol check pass.
- All 179 available native corpus hashes and 14 exact CLI output/error/
  destination-preservation contracts match the retained baseline.
- Five interleaved before/after timing rounds over 15 cases spanning all five
  admitted profiles measured median **+0.45%**, aggregate-time-weighted
  **+0.66%**, and per-case changes from **-1.57% to +2.59%** on the M4 Max.
  No material decode-speed regression was observed; optimization defaults are
  unchanged. Cross-builds and media regeneration were not required.

---

## C8: Table-Drive Hardcoded Tuple Branches and Remove Dead Checks

### Problem
Several core decoder modules contain bring-up-era special-case cascades or irregularly formatted boolean walls that can be expressed more clearly and compactly:
1. **Repeated 4x4/8x4/4x8 Secondary Transform Special Cases in [`secondary_transform.c`](../src/decoder/secondary_transform.c):**
   Lines 269–365 contain four nearly identical 25-line blocks for specific `(mode, tx_size, secondary_symbol, secondary_set)` tuples (`(DC, 4x4, 1, 8)`, `(DC, 8x4, 2, 12)`, `(SMOOTH, 8x4, 1, 6)`, `(D135, 4x8, 1, 3)`), each copying 8 inputs via a scan table and running the same 8×16 matrix-multiply loop. Similarly, `inverse_secondary_transform_4x4` (lines 72–239) uses a 42-branch `if / else if` ladder to map `(secondary_set, secondary_symbol)` to `av2h_ist_4x4_setX_typeY_kernel`.
2. **Single-Geometry 16-Bit Transform Helpers in [`inverse_transform.c`](../src/decoder/inverse_transform.c):**
   [`av2h_reconstruct_transform_8x16_u16`](../src/decoder/inverse_transform.c) and [`av2h_reconstruct_transform_8x4_u16`](../src/decoder/inverse_transform.c) duplicate the scaling, secondary transform, 1D row/column transform, and residual-addition flow already present in `av2h_reconstruct_transform_generic_u16`.
3. **Boolean Walls and Unreachable Branches in [`coeff_level.c`](../src/decoder/coeff_level.c):**
   - In `q_eob_context_supported` (lines 54–56), `q_base_context_supported` (lines 87–89), and `q_br_context_supported` (lines 166–167), the check `if (cdfs->q_context < 4U) return ...;` is immediately followed by `if (cdfs->q_context == 1U) return 1;`, making the `q_context == 1U`, `2U`, and `3U` branches below it completely dead/unreachable.
   - If those branches are obsolete bring-up guards superseded by `cdfs->q_context < 4U`, they should be removed; if they are still intended under specific conditions, the guard logic must be clarified and represented as a compact lookup table or bitmask rather than a 70-line nested boolean expression.

### Actions
1. **Replace the 42-Branch 4x4 IST Kernel Ladder and 4 Tuple Blocks in [`secondary_transform.c`](../src/decoder/secondary_transform.c):**
   - Index a static 2D pointer table `ist_4x4_kernels[14][3]` by `[secondary_set][secondary_symbol - 1]`.
   - Factor the 8×16 small-input secondary transform loop into a single `static` helper driven by a small descriptor table for the four special-case tuples.
2. **Consolidate or Table-Drive [`coeff_level.c`](../src/decoder/coeff_level.c) Context Admission:**
   - Remove the unreachable `q_context == 1U / 2U / 3U` dead code after `if (cdfs->q_context < 4U)` (and verify with `-Wunreachable-code` and the full test suite).
3. **Unify `8x16_u16` and `8x4_u16` Transform Paths in [`inverse_transform.c`](../src/decoder/inverse_transform.c):**
   - Route or share the 16-bit 8×16 and 8×4 DCT transform bodies through the common 16-bit transform helper while preserving exact admission statuses (`AV2H_INVALID_ARGUMENT` vs `AV2H_UNSUPPORTED` vs `AV2H_INVALID_DATA`).

### Exit Criteria
- `inverse_secondary_transform_4x4` uses a table lookup instead of a 42-way `if / else if` chain, and the 8×16 IST inner loop appears once.
- Zero unreachable code branches remain in [`coeff_level.c`](../src/decoder/coeff_level.c) when compiled with `-Wunreachable-code`.

---

## C9: Decompose [`frame_output.c`](../src/decoder/frame_output.c) Orchestration

### Problem
`av2h_decode_frame_group_planar` in [`frame_output.c`](../src/decoder/frame_output.c) spans roughly 550 lines in a single function. Much of this bulk comes from:
- Duplicated loops that flush `pending_chroma_leaves` (once mid-tile when `decoder.chroma.shared_complete` transitions, and once at the end of the tile), with each loop further branching on `workspace->bytes_per_sample == 2U` vs `1U`.
- Nearly 200 lines of inline diagnostic field copying (`decoded.syntax_stage = ...`, `decoded.eob_range = ...`, `decoded.after_all_zero_tell = ...`) embedded directly inside the main tile-decoding `while` loop for luma parse, luma reconstruct, chroma parse, chroma cross-transform, and chroma reconstruct failures.

### Actions
1. **Extract Shared Pending-Chroma Flush Helper:**
   - Extract `flush_pending_chroma_leaves(...)` as a `static` helper in [`frame_output.c`](../src/decoder/frame_output.c) called both mid-tile and post-tile to reconstruct and commit deblock metadata for queued shared-chroma leaves.
2. **Extract Diagnostic-Failure Builders:**
   - Extract `populate_luma_parse_failure(...)` and `populate_chroma_parse_failure(...)` (or stage-specific failure helpers) out of the main tile loop so the high-level Luma $\rightarrow$ Chroma $\rightarrow$ Completeness $\rightarrow$ Filter $\rightarrow$ Output state machine fits on a single screen.

### Exit Criteria
- `av2h_decode_frame_group_planar` in [`frame_output.c`](../src/decoder/frame_output.c) is under 200 lines, with zero duplicated pending-chroma flush loops.
- All failure statuses, diagnostic fields, and entropy tell values remain identical.

---

## C10: Normalize Formatting, Indentation, and Line Wrapping

### Problem
Incremental manual edits and localized automated formatting passes across earlier milestones left visible formatting and indentation inconsistencies:
- **Unindented control-flow bodies:** In [`frame_output.c`](../src/decoder/frame_output.c), the `while (decoder.control.phase != AV2H_TILE_DECODE_DONE)` loop body (lines 258–580) is at the same indentation level as its enclosing `while` statement.
- **Deeply drifted boolean/continuation indentation:** In [`coeff_level.c`](../src/decoder/coeff_level.c) (lines 58–158), [`container.c`](../src/container.c), and [`reconstruct.c`](../src/decoder/reconstruct.c), continuation lines drift between 4-space, 8-space, and 60+-column alignments.
- **Mixed column limits and brace/pointer styles:** Older files use ~80-column manual wrapping with multi-line function signatures, whereas newer or C6-touched files (`*_kernel.inc`, [`reconstruct_block.c`](../src/decoder/reconstruct_block.c)) use ~100-column `clang-format` wrapping and compact single-line declarations.

### Actions
1. **Define a Canonical `.clang-format` Configuration:**
   - Establish a single project-wide `.clang-format` (4-space indentation, consistent column limit, pointer alignment, and macro-aware continuation rules for `AV2H_KERNEL_NAME` / `AV2H_KERNEL_TYPE`).
   - Protect dense static lookup tables (such as generated `.inc` tables or compact matrix literals in [`inverse_transform.c`](../src/decoder/inverse_transform.c)) with `/* clang-format off */` / `/* clang-format on */` where tabular layout aids readability.
2. **Apply Pure Whitespace/Formatting Pass Across [`src/`](../src/) and [`include/`](../include/):**
   - Run formatting as a standalone, behavior-free pass after C7–C9 so structural diffs and whitespace diffs are never mixed in the same commit.
3. **Add Automated Formatting & Zero-Binary-Diff Verification:**
   - Verify that stripped object files (`.o`) and the final decoder binary are byte-for-byte identical before and after C10.
   - Add a lightweight check to the Python source-consistency suite so future edits do not reintroduce unindented blocks or formatting drift.

### Exit Criteria
- Uniform indentation, brace placement, and line wrapping across all `.c`, `.h`, and `*_kernel.inc` files in [`src/`](../src/) and [`include/`](../include/).
- Byte-for-byte identical compiled code before and after the formatting pass.
