# Code Quality and Consistency Cleanup Plan

**Status:** C1 complete; C2-C6 planned  
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
1. **Header & Macro Ordering:** Move all `#include` directives to the top of every `.c` and `.h` file:
   - Corresponding module header first,
   - Project headers in alphabetical order,
   - Shared/freestanding standard headers (`<stddef.h>`, `<stdint.h>`, `<limits.h>`) last.
   - Place all file-scope `#define` constants and type definitions immediately after `#include`s and before any function definitions.
2. **Pointer Nullability (`NULL`):** Standardize on `NULL` (from [`src/shared/stddef.h`](../src/shared/stddef.h)) for all pointer comparisons (`ptr == NULL`, `ptr != NULL`), pointer assignments, and null-pointer returns (`return NULL;`), reserving `0` / `0U` strictly for integer and enum values.
3. **Boolean Convention:** Establish and apply a single rule across [`src/`](../src/):
   - **Struct fields and compact arrays:** Use `uint8_t` (`0U` / `1U`) so memory layout and workspace sizing remain explicit and architecture-independent.
   - **Predicate function return types:** Use `int` (`0` / `1`) consistently across all validation and query helpers (matching C's comparison operator type and existing [`base.h`](../src/base.h) predicates like `av2h_size_add`).

### Exit Criteria
- No `#include` appears after the first declaration or function in any file.
- Pointer checks and null pointer literals use `NULL` consistently across [`src/`](../src/).
- Predicate return types and struct boolean fields follow the documented convention.

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

---

## C4: Retire Historical Wrappers and Compatibility Aliases

### Problem
Incremental milestones left behind multi-layered forwarding functions and preprocessor aliases that no longer serve an architectural boundary:
- **Multi-generation function chains:** In [`src/decoder/reconstruct.h`](../src/decoder/reconstruct.h), chroma plane reconstruction exposes six overlapping entry points (`av2h_chroma_reconstruct_to_plane`, `_ex`, `_format`, `_format_with_diagnostics`, `_u16`, `_u16_with_diagnostics`), and luma reconstruction similarly duplicates `av2h_first_luma_reconstruct`, `av2h_luma_reconstruct`, `av2h_luma_reconstruct_with_diagnostics`, `av2h_luma_reconstruct_to_plane`, and `av2h_luma_reconstruct_to_plane_with_diagnostics`.
- **Similar `_with_diagnostics` / `_with_execution` pass-throughs:** Several internal modules expose both a base function and a `_with_diagnostics` or `_with_sequence` wrapper where `diagnostics == NULL` is already safely handled as a no-op by the underlying implementation.
- **Compatibility `#define`s after file splits:** Files like [`reconstruct.c`](../src/decoder/reconstruct.c) (`#define wide_angle_map av2h_luma_wide_angle`), [`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), and [`tile_luma.c`](../src/decoder/tile_luma.c) use `#define` macros to alias old local function names to new module-prefixed names.

### Actions
1. **Collapse Internal Function Chains:** Where an internal function accepts optional parameters (`diagnostics`, `predictor_plane`, `execution`) that safely accept `NULL`, keep a single canonical internal entry point and update callers directly.
2. **Retire Single-Leaf Test Scaffolding Wrappers:** If a legacy wrapper is used only by early unit tests, either migrate those unit tests to the canonical API or move test-only convenience helpers into the test suite rather than keeping them in production headers.
3. **Remove Preprocessor Name Aliases:** Replace `#define` symbol aliases in `.c` files with direct calls to the actual `av2h_*` function names.

### Exit Criteria
- Each internal operation in [`src/decoder/`](../src/decoder/) has one canonical entry point per responsibility.
- Zero `#define` function-renaming shims remain in `.c` files.

---

## C5: Deduplicate Platform CLI Frontends

### Problem
The three standalone CLI entry points—[`src/platform/posix/decoder.c`](../src/platform/posix/decoder.c) (480 lines), [`src/platform/nolibc/decoder.c`](../src/platform/nolibc/decoder.c) (521 lines), and [`src/platform/windows/decoder.c`](../src/platform/windows/decoder.c) (877 lines)—duplicate significant platform-independent logic:
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

---

## C6: Deduplicate 8-Bit, 10-Bit, and Future 12-Bit Sample Paths

### Problem
The AV2 specification supports 8-bit, 10-bit, and 12-bit sample depths (`bit_depth_idc` 1, 0, and 2—note [`src/container.c`](../src/container.c) already recognizes `bit_depth_idc == 2` as 12-bit). Currently, [`src/decoder/`](../src/decoder/) duplicates nearly the entire sample-processing pipeline between `uint8_t` and `uint16_t` (`_u16`, ~275 occurrences across 31 files):
- **Plane and Scratch Types:** `Av2hReconstructionPlane` vs `Av2hReconstructionPlaneU16`, `Av2hReconstructionScratch` vs `Av2hReconstructionScratchU16`, `Av2hFilterPlane` vs `Av2hFilterPlaneU16`.
- **Prediction & Reconstruction:** Near-identical functions in [`reconstruct.c`](../src/decoder/reconstruct.c), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c), [`intra_prediction_*.c`](../src/decoder/intra_prediction.c), and [`frame_output.c`](../src/decoder/frame_output.c).
- **Loop Filters:** Repeated 8-bit and 16-bit kernels in [`deblock.c`](../src/decoder/deblock.c), [`cdef.c`](../src/decoder/cdef.c), [`ccso.c`](../src/decoder/ccso.c), [`gdf.c`](../src/decoder/gdf.c), and [`restoration.c`](../src/decoder/restoration.c).
- Adding 12-bit support under the current pattern would either require parameterizing `_u16` everywhere by `bit_depth` / `Av2hSampleDomain` or risk further branching duplication.

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
