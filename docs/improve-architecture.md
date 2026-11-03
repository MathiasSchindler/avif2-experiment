# Decoder Architecture and Runtime Improvement Plan

**Status:** A1-A4 complete; A5-A6 planned
**Scope:** [`src/`](../src/) and [`include/av2heif.h`](../include/av2heif.h)

## Purpose and Scope

With structural ownership ([`history/refactor.md`](history/refactor.md)) and source-level hygiene ([`history/cleanup.md`](history/cleanup.md)) complete, the codebase has clean module boundaries, uniform naming and formatting, and strong automated guards.

This document defines the next series of architectural milestones (**A1–A6**) addressing **runtime mechanics, memory/stack footprint, redundant work in the decode pipeline, entropy-decoding throughput, parallel execution, and 8-bit/16-bit behavioral convergence**.

Every milestone must preserve:

- Exact decoded pixel outputs for all admitted 8-bit and 10-bit streams (unless an explicit capability-admission milestone documents a newly admitted profile).
- Freestanding C11 compliance (`-std=c11 -ffreestanding -fno-builtin -nostdinc -Wall -Wextra -Wpedantic -Werror`), zero heap allocation inside core decoding, and full `nolibc` build compatibility.
- Existing source-consistency, naming, entrypoint, private-header, and formatting guards (`make format-check`).

---

## Milestone Overview

| Milestone | Focus | Primary Targets | Prerequisites |
| :--- | :--- | :--- | :--- |
| **A1** | Move large decoder state (`Av2hTileDecoder`, `Av2hCoeffBlock`, preparation state) off the C call stack into the caller-owned workspace arena | [`frame_workspace.c`](../src/decoder/frame_workspace.c), [`frame_output.c`](../src/decoder/frame_output.c), [`tile.h`](../src/decoder/tile.h), [`decode.c`](../src/decode.c) | None |
| **A2** | Eliminate redundant parsing and full-input hashing across `av2h_query` and `av2h_decode` | [`query.c`](../src/query.c), [`decode.c`](../src/decode.c), [`decode.h`](../src/decode.h) | A1 |
| **A3** | Replace bit-by-bit refills with byte-chunk reads, fuse CDF validation/search, and accelerate range normalization while retaining checked decoding | [`range.c`](../src/decoder/range.c), [`base.c`](../src/base.c), [`shared/bit_math.h`](../src/shared/bit_math.h) | None |
| **A4** | Eliminate redundant inactive-stage copies and $O(H)$ overlap scans; reuse GDF neighborhoods and gradients | [`filter.c`](../src/decoder/filter.c), [`internal/filter_common_kernel.inc`](../src/decoder/internal/filter_common_kernel.inc), [`internal/gdf_kernel.inc`](../src/decoder/internal/gdf_kernel.inc) | A1 |
| **A5** | Extend `Av2hExecution` parallelism from final output `memcpy` to multi-tile decoding and loop-filter stripes | [`frame_output.c`](../src/decoder/frame_output.c), [`frame_workspace.c`](../src/decoder/frame_workspace.c), [`filter.c`](../src/decoder/filter.c), [`cdef.c`](../src/decoder/cdef.c), [`gdf.c`](../src/decoder/gdf.c) | A1, A4 |
| **A6** | Converge 8-bit and 16-bit (`_u16`) admission rules, DPCM semantics, and loop-filter support | [`reconstruct.c`](../src/decoder/reconstruct.c), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`chroma_prediction_*.c`](../src/decoder/chroma_prediction.c), [`filter.c`](../src/decoder/filter.c), [`restoration.c`](../src/decoder/restoration.c) | A4 |

---

## A1: Move Large Decoder State Off the Call Stack Into the Workspace Arena

### Problem
Before A1, the native frame-group coordinator alone placed **146,128 bytes** of state onto the C call stack despite the caller-provided workspace policy:
1. **`Av2hTileDecoder` (70,088 bytes on the native ABI):** Declared as a local variable in `av2h_decode_frame_group_planar_private` ([`src/decoder/frame_output.c`](../src/decoder/frame_output.c)). Its coefficient-level CDF storage occupies 25,480 bytes, alongside partition nodes and chroma contexts.
2. **`Av2hCoeffBlock`s (8–16 KB on stack):** `decode_luma_leaf` allocates one `Av2hCoeffBlock` (8 KB: `quantized[1024]` + `dequantized[1024]`) and `decode_chroma_leaf` allocates `Av2hCoeffBlock blocks[2U]` (16 KB) on the stack, in addition to `pending_chroma_blocks[4U][2U]` (64 KB) inside `Av2hFrameDecodeContext`.
3. **Preparation and tile groups (16–18 KB public-entrypoint frames):** Allocated on the stack in `av2h_query` and again in `av2h_decode`. The 11,088-byte `Av2hFrameState` embedded custom quantization matrices, restoration filters, and CCSO tables.

On freestanding / `nolibc` worker threads, WebAssembly linear-memory stacks, Windows threads, or embedded targets, >128 KB stack frames risk stack overflow and contradict the explicit workspace-arena design.

### Actions
1. **Allocate `Av2hTileDecoder` and Coefficient Block Scratch in `Av2hFrameDecodeWorkspace`:**
   - Add `Av2hTileDecoder *tile_decoder` and active/pending `Av2hCoeffBlock` storage to `Av2hFrameDecodeWorkspace` in [`src/decoder/frame_workspace.c`](../src/decoder/frame_workspace.c) using the existing dual-mode `Av2hArena` (`sizing_only` during query, bound pointer during init).
2. **Slim Stack Frames in `av2h_query` / `av2h_decode` / `frame_output.c`:**
   - Ensure no function in [`src/`](../src/) has a stack frame exceeding 4 KB (verifiable with `-Wframe-larger-than=4096`).
3. **Update Frozen Workspace Size Baselines Explicitly:**
   - Update the workspace size expectations in unit tests to reflect the arena-owned `Av2hTileDecoder` and coefficient scratch.

### Exit Criteria
- Compiling all of [`src/`](../src/) with `-Wframe-larger-than=4096 -Werror` succeeds with zero warnings.
- `Av2hTileDecoder` and `Av2hCoeffBlock` buffers reside in the caller-supplied workspace arena.

### A1 progress: frame-decode arena ownership

The first implementation step established frame-decode arena ownership.
The global ceiling was verified after the subsequent steps below.

- [`Av2hFrameDecodeScratch`](../src/decoder/frame_output.h) now owns the serial
  tile decoder, two shared active coefficient blocks, and four deferred chroma
  leaves with their coefficient pairs. One typed allocation in the existing
  sizing/binding arena keeps these related buffers together and accounts for
  their alignment identically during query and initialization. Luma and chroma
  share the active pair because their decode stages do not overlap; deferred
  coefficients have separate storage.
- [`frame_output.c`](../src/decoder/frame_output.c) no longer allocates a tile
  decoder, active coefficient blocks, or deferred buffers on the stack. The
  single-tile entry point passes a one-span view to the same coordinator instead
  of constructing a full 256-entry tile group. No public API change, heap
  allocation, static mutable decoder state, or A2 parsing optimization was added.
- Native ARM64 measurements with Clang 22.1.8 at `-O2` put the shared coordinator
  at **2,704 bytes** without diagnostics and **2,720 bytes** with diagnostics,
  versus the original **146,128-byte** frame-group coordinator. The former
  20,576-byte leaf-loop frame is eliminated by inlining its now-small helpers.
  The single-tile wrapper falls from **4,240 to 48 bytes**. Every compiled frame
  in this owner is below 4,096 bytes in both configurations. The native object
  has a persistent `-Wframe-larger-than=4096` build guard.
- On this native ABI the arena scratch is **164,152 bytes**, with **8-byte**
  alignment. Workspace queries increase by **164,159 bytes**, including the
  existing sizing arena's seven worst-case padding bytes, and now report
  8-byte rather than 4-byte alignment. Public workspace-size baselines, private
  layouts, resource inventories, and storage-offset fingerprints were updated
  explicitly. Existing byte/wide plane-storage differences remain unchanged.
- New scratch storage is validated for presence, alignment, and output
  disjointness before clearing or decoding. Tests cover overlap with every
  scratch component, absent/misaligned storage, buffer guards, and reuse with
  four different poison patterns.

The stack relocation exposed initialization dependencies in malformed-input
tests. The arena-owned decoder is reset at the start of each frame. In addition,
[`av2h_luma_mode_neighbors_from_state`](../src/decoder/leaf_context.c) previously
left unused `line_present` / `line_skip_transform` entries uninitialized when
fewer than two line neighbors existed. A poisoned-output regression reproduced
the defect; those absent entries are now initialized explicitly. This is a
tightly coupled correctness fix, not an attempt to preserve stack garbage.
The lossless and 10-bit 4:4:4 malformed-record fingerprints were updated
explicitly; valid lossless retries with a NULL result now succeed after prior
malformed attempts. All other retained frame-result fingerprints are unchanged.

Validation for this step:

- All **33 sanitized C suites** and **2,460 deterministic public-decode fuzz
  cases**, with diagnostics absent and enabled.
- **560** frozen orchestration variants plus **56** poisoned-scratch reuse cases
  per diagnostic configuration; dedicated zero/one/two-neighbor initialization
  coverage.
- **179** unchanged native decoded-image hashes and **14** exact CLI
  output/error/destination-preservation contracts.
- **38** related Python checks, canonical formatting, freestanding undefined
  symbols, **55** standalone private headers, and **35** value-only kernel roots.

### A1 progress: reconstruction result ownership

The second implementation step moves the byte/wide block-reconstruction results
into the existing arena-backed
[`Av2hReconstructionScratch`](../src/decoder/reconstruct.h) and
[`Av2hReconstructionScratchU16`](../src/decoder/reconstruct.h). Luma and chroma
reuse one result per storage domain; predictor, transform, and result storage
remain disjoint. The four plane-reconstruction entry points no longer allocate
these large results on the stack. The two direct tile-reconstruction helpers now
require caller-owned coefficient blocks instead of allocating them internally;
the normal frame path continues using its arena-owned active pair.

Allocation now uses `sizeof` and `_Alignof` of the selected scratch type instead
of duplicating its field-size arithmetic in
[`frame_workspace.c`](../src/decoder/frame_workspace.c). On the native ABI:

- Byte scratch grows from **16,896 to 29,188 bytes**.
- Wide scratch grows from **21,504 to 46,084 bytes**.
- Queried workspace grows by a further **12,292 / 24,580 bytes** for byte/wide
  samples, respectively, beyond the first A1 step. Overall workspace alignment
  remains **8 bytes**; both reconstruction scratch types remain 4-byte aligned.
- Public size expectations, private-layout inventories, byte/wide accounting,
  and storage-offset fingerprints explicitly include these changes. Tests check
  result ownership, alignment, predictor disjointness, arena bounds, and output
  overlap rejection through the final scratch byte in both storage domains.

Native ARM64 Clang 22.1.8 `-O2` stack measurements:

| Entry point | Before this step | After, diagnostics absent / enabled |
| :--- | ---: | ---: |
| Byte luma reconstruction | 14,352 | 2,048 / 2,048 |
| Byte chroma reconstruction | 12,640 | 352 / 368 |
| Wide luma reconstruction | 24,944 | 2,112 / 2,112 |
| Wide chroma reconstruction | 26,704 | 2,112 / 2,112 |
| Direct tile luma reconstruction | 9,344 | 64 / 64 |
| Direct tile chroma reconstruction | 18,656 | 112 / 112 |

At this step, the persistent native 4 KB build guard covered
[`reconstruct.c`](../src/decoder/reconstruct.c),
[`tile_luma.c`](../src/decoder/tile_luma.c), and
[`tile_chroma.c`](../src/decoder/tile_chroma.c), as well as the first step's
frame-output owner.

Expanded poison testing also reproduced a pre-existing wide multi-line
reference defect: at the picture's bottom edge, the secondary-left fill loop
stopped without reducing the advertised reference count. Prediction could then
consume stale scratch samples. The count now includes only initialized samples.
Two accepted bit-mutated 10-bit 4:4:4 streams exercise this path with four scratch
patterns. Their output is now scratch-history independent; the corresponding
mutation-record fingerprint was deliberately rebaselined. No admission rule or
return status was changed, and all **179 retained native image hashes** and
**14 CLI contracts** remain unchanged.

Validation includes both diagnostic configurations of the **33 sanitized C
suites**, **2,460 deterministic public-decode fuzz cases**, **560 frozen
orchestration variants**, and now **64 poisoned-scratch reuse cases**. The poison
checks cover the entire reconstruction scratch as well as frame-decode scratch.
The **38 related Python checks**, standalone private headers, freestanding
symbols, formatting, and explicit resource/layout baselines are also checked.

### A1 completion: preparation, prediction, and the all-source ceiling

The final step removes the remaining ten oversized native frames:

- [`Av2hFrameState`](../src/decoder/frame.h) is now **600 bytes**. Its pointers
  refer to a separate **10,560-byte** syntax store for custom matrices,
  restoration filters, and CCSO offsets. Query validates the same syntax using
  bounded per-definition/per-plane buffers without materializing those tables.
  Decoding materializes the tables in the caller's arena. Execution rejects
  missing required tables explicitly; metadata-only query remains supported.
- [`Av2hFrameDecodeScratch`](../src/decoder/frame_output.h) additionally owns the
  **1,024-byte** preparation and **4,136-byte** tile group. Public decode binds
  these only after its existing size, alignment, input-binding, and overlap
  checks. Workspace preparation clears the decoder, not the live preparation.
  The raw CLI preserves validation-before-allocation and error ordering by
  replaying materialization into its allocated arena. A2's redundant-parse and
  hashing optimizations remain separate.
- Tile-group validation uses a small header and optionally writes spans directly
  to the caller's group. It no longer needs a local 256-span group or a frame
  copy. Partial-error state is preserved.
- Directional prediction borrows secondary pixels from the arena-backed
  reconstruction result, whose lifetime does not overlap prediction. Serial
  primary/secondary passes reuse one mode projection. Wide chroma also borrows
  its caller's projection after prediction, avoiding a second reconstruction
  projection. Neither change adds another prediction workspace allocation.
- Explicit out-of-line phase boundaries prevent LTO from recombining bounded
  CLI, workspace-binding, frame, reconstruction, and prediction working sets.
  The production build guard covers core, CLI, platform, and runtime C recipes;
  sanitizer test fixtures are not subject to the production frame limit.
  No frame warnings are disabled or suppressed.

The final native scratch is **179,872 bytes**, aligned to **8 bytes**. Its query
reservation is **179,879 bytes**, including worst-case alignment padding.
Preparation adds **15,720 bytes** beyond step two; total queried growth over the
pre-A1 baseline is **192,171 bytes** for byte samples and **204,459 bytes** for
wide samples. All resource/layout and storage-offset baselines were updated
explicitly; decoded-output fingerprints remain unchanged.

Final ARM64 Clang 22.1.8 `-O2` frames, diagnostics absent / enabled:

| Entry point | Bytes |
| :--- | ---: |
| Public query | 1,504 / 1,504 |
| Public decode | 2,064 / 2,064 |
| Raw CLI decode | 3,760 / 3,760 |
| Frame coordinator | 2,704 / 2,720 |
| Byte directional feature prediction | 1,952 / 1,952 |
| Wide multiline prediction | 2,000 / 2,000 |
| Tile-group ledger parser | 176 / 176 |

Validation:

- **All 76 production C sources** are represented in guarded object/stack
  inventories in both diagnostic configurations, with **zero frames over 4,096
  bytes**. Native core/CLI and actual macOS, Linux x86-64/ARM64, and Windows
  x86-64/ARM64 platform/runtime branches were compiled. Foreign binaries were
  not linked or executed.
- Both complete configurations pass **33 sanitized C suites**, **2,460**
  deterministic public-decode fuzz cases, **560** frozen orchestration variants,
  and **64** poisoned-scratch reuse cases. New checks cover metadata/materialized
  parser parity, truncated custom matrices, arena ownership, missing tables,
  missing prediction/projection scratch, and rejection without buffer writes.
  A newly exposed uninitialized range-decoder argument-test fixture was fixed.
- All **179 native image hashes** and **14 exact CLI contracts** match their
  retained baselines in each configuration.
- **38** related Python checks, **55** standalone private headers, **35**
  value-only kernel roots, freestanding symbols, formatting, and whitespace
  checks pass.
- The size-optimized native nolibc objects also pass the frame guard. Actual
  default-LTO backend code generation emits no oversized-frame warnings after
  the phase-boundary fixes. End-to-end nolibc execution could not be verified:
  the existing custom Mach-O linker rejects `__ustring` emitted from unchanged
  [`coeff_level_cdf.c`](../src/decoder/coeff_level_cdf.c), with both LLVM and
  Apple Clang. A separate non-LTO `-Oz` symbol audit also reports legacy
  compiler-emitted `memcpy` / `bzero` calls in
  [`container.c`](../src/container.c),
  [`frame_copy.c`](../src/decoder/frame_copy.c), and
  [`frame_output.c`](../src/decoder/frame_output.c); compiling the HEAD
  frame-output source reproduces both symbols. These baseline toolchain
  limitations are not changed by A1. Native `-O2` freestanding symbol checks pass.

---

## A2: Eliminate Redundant Parsing in `av2h_query` / `av2h_decode`

**Status: complete.** Exact full-input fingerprinting is retained.

### Problem
Before A2, in the normal two-step public API workflow (`av2h_query` followed by `av2h_decode`), header parsing and full-file hashing were repeated multiple times before decoding started:
1. **Double HEIF Container Parse in `av2h_query`:** `av2h_query` ([`src/query.c`](../src/query.c)) calls `av2h_dialect_detect(input, ...)`—which runs `av2h_heif_parse(input, &image, &strict_error)`—and on the very next step calls `av2h_heif_parse(input, &info->image, error)` again on the same bytes.
2. **Redundant Full `av2h_query` + Double `av2h_decode_prepare` in `av2h_decode`:**
   - `av2h_decode` ([`src/decode.c`](../src/decode.c)) calls `av2h_query(input, NULL, &fresh, error)` from scratch (re-running `av2h_dialect_detect`, both `av2h_heif_parse` passes, `av2h_decode_prepare`, and the full-buffer `input_fingerprint` FNV-1a loop over every byte of `input`).
   - Immediately after `info_matches(info, &fresh)` and workspace validation, `av2h_decode` calls `av2h_decode_prepare` **again**, materializing arena-owned state even though `av2h_query` just validated the same syntax to compute `fresh.workspace_size`!
3. **Result:** A single `av2h_query` + `av2h_decode` call parses the HEIF container **4 times**, parses the Sequence/Frame/QMatrix/TileGroup OBUs **3 times**, and hashes the entire input file byte-by-byte **2 times**.

### Actions
1. **Unify Dialect Detection and HEIF Parsing in `av2h_query`:**
   - Factor an internal helper `av2h_dialect_and_heif_parse(input, &info->dialect, &info->image, error)` so canonical v0 inputs parse the HEIF box tree once during `av2h_query`.
2. **Reuse `Av2hDecodePreparation` Between Query Validation and Decode:**
   - In `av2h_decode`, factor an internal `query_with_preparation(input, NULL, &fresh, &preparation, error)` so `av2h_decode` validates `info_matches(info, &fresh)` and immediately uses the already-parsed `preparation` without calling `av2h_decode_prepare` a second time.
3. **Bound or Reuse Input Binding Verification:**
   - Keep exact pointer/size/fingerprint binding checks in `info_matches`, or evaluate hashing only the container headers + OBU headers/extents if full-payload hashing becomes measurable on multi-megabyte images, while preserving all existing tamper/mismatch tests.

### Exit Criteria
- `av2h_query` parses a canonical HEIF container once and runs `av2h_decode_prepare` once.
- `av2h_decode` runs `av2h_decode_prepare` once rather than twice.
- All public `av2h_query` / `av2h_decode` tamper, mismatch, and error-precedence tests pass unchanged.

### Implementation

- [`query.c`](../src/query.c) shares canonical dialect detection and HEIF parsing.
  The parsed image is published only on canonical success; the public dialect
  detector still leaves the caller's error record untouched on that path.
  Legacy/conflicting-container probing and strict-error precedence are unchanged.
- The private `av2h_query_with_preparation` returns validated compact preparation
  and borrowed syntax references. [`decode.c`](../src/decode.c) compares the fresh
  information and checks buffer sizes, alignment, and overlaps **before any arena
  writes**. It then copies that preparation into A1's arena rather than invoking
  full preparation again. The query phase remains an explicit no-inline stack
  boundary under LTO.
- Large CCSO, restoration, and custom-matrix tables are materialized from captured
  bit positions using the same table-body readers as direct parsing. Sequence and
  frame control syntax are not replayed. Tile lengths are walked again to build
  arena-owned spans; canonical input reuses its captured frame OBU without
  reparsing its ledger. Private multi-OBU preparation retains support for regular
  tile groups. References borrow immutable input only for the current call.
- Public structures, arena accounting, and queried workspace sizes do not grow.
  Full FNV-1a hashing remains once per query/decode call, including entropy bytes:
  the two-call workflow deliberately hashes twice to preserve exact
  pointer/size/fingerprint binding and payload-tamper detection.

### Verification

- Test-only parser counters confirm **one HEIF parse, one preparation, and one
  full sequence/frame parse per canonical query or decode call**. Direct and
  deferred tables/spans compare exactly, including poisoned storage, truncations,
  three-plane matrices, multiple matrix OBUs, and regular tile groups.
- Both diagnostic configurations pass **33 sanitized C suites**, **2,460**
  deterministic public-decode fuzz cases, the existing **560** orchestration
  variants, and **64** poisoned-scratch reuse cases. Frozen error records,
  tamper/mismatch checks, buffer-rejection behavior, and resource baselines are
  unchanged.
- All **179 raw image hashes** and **14 exact CLI contracts** remain unchanged
  per configuration. All **176 canonically admitted corpus cases** also match
  through instrumented public decoding, the native canonical artifact frontend,
  and its full-LTO build. The other three raw fixtures carry multiple key OBUs
  and remain rejected by canonical carriage; admission was not broadened.
- All **76 production C sources** pass the 4,096-byte frame guard in both
  configurations. Native public query, private query phase, and public decode
  frames are **1,056**, **496**, and **3,696 bytes**, respectively; the native
  maximum remains **3,760 bytes**. Changed sources also pass Linux and Windows
  x86-64/ARM64 and Wasm32 frame guards; their largest measured frame is **3,848
  bytes**. Full-LTO canonical backend objects retain the changed decode/query
  phases and emit no oversized-frame warnings. LTO verification links explicit
  LLVM objects to bypass the host archive-loader incompatibility.
- **38** related Python checks, **55** standalone private headers, **35**
  value-only kernel roots, native freestanding symbols, formatting, and
  whitespace checks pass. The previously documented nolibc toolchain limitations
  are outside A2.

---

## A3: Optimize Entropy Range Decoder and Bit Reader Hot Loops

### Problem
As noted in [`dev/performance-baseline.md`](dev/performance-baseline.md), lossless photographs and coefficient-heavy 10-bit streams spend a large share of decode time in entropy decoding ([`src/decoder/range.c`](../src/decoder/range.c)). Before A3, [`range.c`](../src/decoder/range.c) and [`base.c`](../src/base.c) had four inner-loop inefficiencies:
1. **Bit-by-bit refill with per-bit span lookup in `range_input_bits`:**
   `range_input_bits` ([`src/decoder/range.c`](../src/decoder/range.c)) loops `for (index = 0U; index < bit_count; ++index)` one bit at a time, calling `range_byte_at(decoder, position >> 3U, &byte)` on **every single bit** (which itself loops over `decoder->spans`).
2. **Bit-by-bit bypass reading in `av2h_range_read_bypass`:**
   Loops one bit at a time, calling `range_input_bits(decoder, 1U)` and `range_check_overflow` on each bit.
3. **Unconditional $O(\text{symbol\_count})$ CDF Monotonicity Scan on Every Symbol Read:**
   In `av2h_range_read_symbol` ([`src/decoder/range.c`](../src/decoder/range.c)), every symbol decode runs:
   ```c
   if (inverse_cdf[symbol_count - 1U] != 0U) { ... }
   for (index = 1U; index < symbol_count; ++index) {
       if (inverse_cdf[index - 1U] < inverse_cdf[index]) { ... }
   }
   ```
   before scanning the CDF for `decoder->value >= lower`. Because CDF tables are initialized and adapted exclusively by internal decoder code (which preserves monotonicity), scanning the entire CDF for validity on every symbol read doubles the loop work in the hottest function of the entropy decoder.
4. **Repeated shifting during range normalization:**
   `av2h_floor_log2_u32` shifted through the range on every normalization.
   The measured ARM64 compiler retained this loop rather than recognizing a
   count-leading-zeros operation.

### Actions
1. **Fast Byte/Word Bit Refill in `Av2hRangeDecoder` and `Av2hBitReader`:**
   - Since `Av2hTileDecoder` initializes `Av2hRangeDecoder` with a single contiguous tile span (`span_count == 1`), add a fast path (or cursor pointer + byte-remaining counter) in `Av2hRangeDecoder` that reads full bytes/words when `available_bits >= 8` and `span_count == 1`, falling back to `range_byte_at` only at span boundaries or tail bits.
2. **Separate Trusted Internal CDF Reads from Untrusted Unit-Test Validation:**
   - Keep CDF validity checks in unit tests / debug builds (or on `av2h_cdf_init`), or fold the monotonicity check into the single symbol-search loop so `inverse_cdf` is traversed once instead of twice while preserving exact `AV2H_INVALID_ARGUMENT` behavior on malformed test CDFs.

### Exit Criteria
- `range_input_bits` and `av2h_bit_reader_read` avoid per-bit span/byte re-indexing on contiguous byte-aligned/multi-bit reads.
- Measurable speedup on lossless and high-bitrate cases in [`dev/performance-baseline.md`](dev/performance-baseline.md) with 100% identical decoded hashes and fuzz/unit test results.

### A3 completed: bounded refills and checked entropy decoding

- The range and ordinary bit readers share a bounded MSB-first byte-chunk
  extractor, with a direct single-byte path. Contiguous range input avoids span
  searches; scattered input gathers at most five bytes, locating each byte
  once. Reads never require speculative, unaligned, or out-of-bounds word loads.
  No cursor fields, workspace reservations, or public/private ABI sizes changed.
- Bypass reads fetch their input together while retaining the dependent
  arithmetic steps. A truncated read stops after exactly the original first
  failing step, preserving `value`, `tell`, `available_bits`, input position,
  sticky errors, and zero-bit behavior.
- CDF validation remains enabled in every build. The search validates its prefix
  and then checks the unsearched tail before committing entropy state, using
  one traversal instead of two. Binary CDFs use a constant-size checked path:
  the terminal zero check already establishes monotonicity for their unsigned
  first entry.
  Adaptive metadata checks retain their original ordering after symbol decoding.
- The shared floor-log2 helper uses guarded GCC/Clang leading-zero builtins,
  explicitly handles zero, and retains the portable C fallback for other
  compilers. Native disassembly confirms that normalization's shift loop is
  gone. No ISA-specific source, SIMD, new build flags, or AVM code was added.
- Nine alternating paired whole-decode rounds against A2 at identical `-O2`
  flags show **1.156x** on lossless Pastel, **1.149x** on its high-bitrate case,
  and **1.100-1.136x** on coefficient-heavy 10-bit cases. The four filter-heavy
  controls also improve (**1.035-1.055x**). All paired bootstrap intervals are
  above 1.0; methodology and individual timings are recorded in
  [`dev/performance-baseline.md`](dev/performance-baseline.md#a3-entropy-loop-implementation).

### Verification

- Both diagnostic configurations pass **33 sanitized C suites**, **2,460**
  deterministic public-decode fuzz cases, **560** orchestration variants, and
  **64** poisoned-scratch reuse cases. Frozen error/resource records remain
  unchanged. A new **49,920-operation** entropy-state transcript retains its
  pre-optimization fingerprint, `6b158c47ce691433`, across contiguous, split,
  and scattered spans, including exhaustion and malformed CDFs.
- Exact-sized bit-reader buffers exercise every cursor and 0-33-bit request
  around small-buffer boundaries under ASan/UBSan. Additional CDF tests reject
  invalid entries even after an earlier symbol could have been selected.
- All **179 raw hashes** and **14 exact CLI contracts** match per configuration.
  All **176 canonical public query/decode hashes** also match through native and
  full-LTO frontends. Canonical admission is unchanged; the three raw-only
  multi-key fixtures retain identical rejection records.
- All **76 production C sources** pass the **4,096-byte** frame guard in both
  configurations; the native maximum remains **3,760 bytes**. All nine changed
  or helper-dependent sources also pass on Linux and Windows x86-64/ARM64 and
  Wasm32; their largest measured frame is **872 bytes**. Full-LTO canonical
  backend objects emit no oversized-frame warnings; verification links
  explicit LLVM objects to bypass the documented host archive-loader issue.
- **38** related Python checks, **55** standalone private headers, **35**
  value-only kernel roots, freestanding symbols, formatting, and whitespace
  checks pass. The previously documented nolibc toolchain limitations remain
  outside A3.

---

## A4: Streamline Loop-Filter Staging and Plane Copies

### Problem
Before A4, in [`src/decoder/filter.c`](../src/decoder/filter.c) and [`src/decoder/frame_workspace.c`](../src/decoder/frame_workspace.c):
1. **Unconditional Multi-Plane Copies Across Inactive Filter Stages:**
   - `av2h_filter_frame` unconditionally copies all 3 planes from `planes` (`coded`) $\rightarrow$ `workspace->deblocked`, then runs CDEF into `workspace->cdef` (copying every block even when CDEF is disabled), then runs GDF / copies chroma back into `planes`, and finally `frame_copy.c` copies `planes` into the caller's `output` buffer.
   - When a filter stage (Deblock, CDEF, CCSO, Restoration, GDF) is disabled in the sequence/frame header, copying full-frame YUV planes into intermediate stage buffers wastes memory bandwidth.
2. **3–4× Full-Frame Plane Allocation in Workspace:**
   - `Av2hFrameDecodeWorkspace` + `Av2hFilterWorkspace` allocate `coded[3]`, `deblocked[3]`, `cdef[3]`, and `restoration` simultaneously as full-frame buffers, even when the frame header disables CDEF, Restoration, or GDF, or when two stage buffers can ping-pong safely.
3. **$O(\text{height})$ Row-by-Row Overlap Checks on Every Frame:**
   - `filter_plane_rows_overlap` / `filter_plane_rows_overlap_u16` in [`src/decoder/internal/filter_common.h`](../src/decoder/internal/filter_common.h) iterates row-by-row (`while (left_row < left->height && right_row < right->height)`) with 6 overflow-checked multiplications per row across all 36 pairs of stage planes. A simple bounding-box interval check (with a row-strided fallback only when bounding boxes intersect) is $O(1)$ for all normal arena-allocated planes.

### Actions
1. **$O(1)$ Fast Path for `filter_plane_rows_overlap`:**
   - Check whether the total byte spans `[start, end)` of the two planes are disjoint first ($O(1)$); only run the row-by-row stride-interleaving loop if the outer bounding spans overlap.
2. **Bypass Copies for Disabled Filter Stages:**
   - In `av2h_filter_frame` and `av2h_filter_stage_u16`, track the active source plane pointer per stage (while keeping `deblocked` luma available whenever CDEF, CCSO, Restoration, or GDF require pre-CDEF deblocked luma reference rows) so disabled stages do not perform full-plane `memcpy` passes.
3. **Optimize GDF Inner Loop Locality:**
   - Integrate the portable C locality/reuse improvements identified in [`dev/performance-baseline.md`](dev/performance-baseline.md) (where GDF accounts for 44–67% of lossy photographic decode time).

### Exit Criteria
- Disjoint plane overlap checks complete in $O(1)$ time.
- Disabled filter stages do not perform redundant full-frame copies in normal disjoint layouts; snapshots required for accepted aliases retain their original semantics.
- Exact pixel parity across all 179 reference streams and all filter unit tests.

### A4 completed: active stage views and portable GDF reuse

- Both sample widths share checked bounding-span rejection and the exact
  row-strided fallback. Disjoint arena planes return in constant time.
  Intersecting bounds still permit disjoint interleaved rows. Overflowing
  bounds also fall back rather than changing the old row walk's result.
- The coordinator tracks active planes and immutable pre-CDEF views. Only
  enabled deblocking planes are snapshotted; disabled CDEF and GDF stages are
  bypassed. CCSO without CDEF materializes only the affected planes before
  mutation. Restoration and GDF select disjoint writable destinations while
  retaining their original pre-CDEF references. Final results still reside
  in the original coded planes, preserving output-copy ownership.
- The U8 entry point previously accepted some cross-plane aliases. Those
  layouts retain the original snapshot/materialization and commit order;
  eliminating those copies would change pixels. U16's stricter original
  overlap rejection is unchanged. Standalone checked filter APIs retain
  disabled-stage copies and their existing argument errors.
- Checked GDF validation is separate from application. The coordinator
  validates the original workspace and controls at the original stage, then
  applies GDF only when enabled, allowing identical immutable source views
  without relaxing writable-destination separation.
- The shared typed GDF kernel resolves one exact 16x16 neighborhood per 4x4
  block and retains its 16 directional class-region sums. Boundary-resolved
  sample queries fall from **1,024 to 256**, and directional sum evaluations
  from **80 to 16**. Tile/stripe selection, U8/U16 scaling, rounding, saturation,
  and control indexing are unchanged.
- Workspace capacities, allocation layout, persistent structure sizes,
  admission rules, and error/resource records are unchanged. This milestone
  removes memory traffic and repeated computation, not reserved stage
  capacity. No heap allocation, mutable global state, SIMD, new compiler
  flags, or AVM code was added.
- Nine alternating paired whole-decode rounds against A3 at identical `-O2`
  flags show **1.413-1.765x** on selected GDF-heavy photographs, **1.359x** on
  the PC-Wiener fixture, and **2.063x** on the filtered 10-bit boundary case.
  Twenty-one additional paired control rounds do not reproduce the original
  prototype's non-GDF regressions. Individual timings, intervals, and limits
  are recorded in
  [`dev/performance-baseline.md`](dev/performance-baseline.md#a4-filter-staging-and-gdf-implementation).

### Verification

- **24,000** U8/U16 overlap comparisons retain the original checked row-walk
  results, including arithmetic overflow, plus explicit interleaved-row and
  maximum-height disjoint cases.
- **288** filter combinations compare every coded sample against the original
  materialized pipeline across monochrome, I420, I444, odd crops, multiple
  tiles, stripe boundaries, and enabled-stage combinations. Their frozen
  transcript, **`cd102d5953a1827e`**, was independently verified against the
  unmodified A3 core. Poisoned scratch verifies inactive stages are untouched.
  Another **48** cases preserve accepted U8 cross-plane aliases.
- Both diagnostic configurations pass **33 ASan/UBSan C suites**, **2,460**
  deterministic public-decode fuzz cases, orchestration and poisoned-scratch
  reuse tests, and unchanged frozen error/resource transcripts.
  Intermediate oracle assertions follow the active source when a stage is
  bypassed; no reference pixels or complete output expectations were changed.
- All **179 raw hashes**, **14 exact CLI contracts**, and **176 canonical
  public query/decode hashes** match per configuration. Native and full-LTO
  canonical frontends preserve container sizes and the three raw-only
  fixtures' exact rejection records.
- All **76 production C sources** pass the **4,096-byte** frame guard in both
  configurations; the native maximum remains **3,760 bytes**. The six changed
  or helper-dependent filter consumers also pass on Linux and Windows
  x86-64/ARM64 and Wasm32; their largest measured frame is **1,312 bytes**.
  Full-LTO canonical backend links emit no oversized-frame warnings.
- **38** related Python checks, **55** standalone private headers, **35**
  value-only kernel roots, freestanding symbol checks, formatting, and
  whitespace checks pass.

---

## A5: Multi-Tile and Loop-Filter Parallelism via `Av2hExecution`

### Problem
The repository includes a custom freestanding thread pool (`Av2hTaskPool` in [`src/runtime/concurrency.c`](../src/runtime/concurrency.c)) and an `Av2hExecution` interface, but:
1. `Av2hExecution` is currently only wired into `av2h_copy_visible_planar_private` ([`src/decoder/frame_copy.c`](../src/decoder/frame_copy.c)), parallelizing only the final `memcpy` of decoded rows into the output buffer.
2. All computationally heavy stages—tile entropy/reconstruction (`decode_tile_leaves`), Deblock, CDEF, CCSO, Restoration, and GDF—run strictly single-threaded.
3. Furthermore, `Av2hFrameDecodeWorkspace` currently allocates a single `tile_context`, a single `reconstruction_scratch`, and mutates a single shared `coded[plane].reconstructed` readiness map (filling outside-tile cells with `2U` in `tile_readiness_clear`), which prevents decoding independent tiles in parallel.

### Actions
1. **Parallelize Independent Loop-Filter Stripes / Blocks First (Zero Workspace Growth):**
   - CDEF (64×64 units), CCSO, Restoration (64-row stripes), and GDF (4×4 blocks within 64-row stripes) read from immutable prior-stage planes (`deblocked` / `cdef`) and write to disjoint destination blocks.
   - Dispatch row stripes across `Av2hExecution->parallel_for` whenever `execution != NULL && execution->width > 1U`.
2. **Decouple Per-Tile State for Parallel Tile Decoding:**
   - Replace the global `2U` ("outside active tile") sentinel mutation in `coded[plane].reconstructed` with an explicit tile-bounds check (`x >= tile_x0 && x < tile_x1 && y >= tile_y0 && y < tile_y1`) during reference collection, so multiple tiles can mark `1U` in their own disjoint tile regions concurrently without clobbering each other's readiness maps.
   - Partition or replicate per-worker `Av2hTileDecoder`, `Av2hTileContextStorage`, and `Av2hReconstructionScratch` when parallel tile execution is requested.

### Exit Criteria
- Loop-filter stages (CDEF, CCSO, Restoration, GDF) and multi-tile frames scale across worker threads when an `Av2hExecution` pool is provided.
- Single-threaded (`execution == NULL`) and multi-threaded (`execution->width > 1`) runs produce bit-for-bit identical output and deterministic error reporting.

---

## A6: Converge 8-Bit and 16-Bit (`_u16`) Pipeline Capabilities and Semantics

### Problem
While C6 and C7 unified the shared numerical kernels, top-level admission and orchestration still reflect historical differences between the 8-bit pipeline and the 16-bit (`_u16`) pipeline:
1. **DPCM Scaling Difference:** In [`src/decoder/reconstruct_block.c`](../src/decoder/reconstruct_block.c), 8-bit DPCM uses arithmetic right shift (`>> scale_bits` where `scale_bits` depends on transform block size), whereas 16-bit DPCM truncates toward zero with a fixed `/ 8`.
2. **Loop-Filter Admission in `av2h_filter_stage_u16`:** [`src/decoder/filter.c`](../src/decoder/filter.c) rejects Restoration on 16-bit frames (`if (sequence->enable_restoration != 0U) return AV2H_UNSUPPORTED;`), hardcodes `domain->bit_depth != 10U`, and does not populate `workspace->diagnostics->filter.reason` on 16-bit filter failures the way `av2h_filter_frame` does for 8-bit.
3. **Chroma Deblock Commit Ordering in [`src/decoder/frame_output.c`](../src/decoder/frame_output.c):** In `decode_chroma_leaf` and `flush_pending_chroma_leaves`, the 8-bit path reconstructs both U and V planes before committing deblock transforms for U and V, whereas the 16-bit path interleaves U reconstruct $\rightarrow$ U deblock $\rightarrow$ V reconstruct $\rightarrow$ V deblock.
4. **Prediction & Transform Admission Gaps:** Several modes (such as Intra-DIP, general MHCCP directions in 4:2:0/4:4:4, and rectangular transform combinations in `reconstruct.c` / `inverse_transform.c`) are admitted in the 8-bit path but gated off in the 16-bit wrappers.

### Actions
1. **Unify Chroma Reconstruction/Deblock Ordering and Filter Diagnostics:**
   - Align the U/V reconstruction and deblock commit order in [`frame_output.c`](../src/decoder/frame_output.c) and add `set_filter_reason` diagnostics to `av2h_filter_stage_u16` in [`filter.c`](../src/decoder/filter.c).
2. **Generalize Restoration (`pc_wiener` / `wiener_nonsep`) and Intra-DIP to High Bit Depth:**
   - Extend [`restoration.c`](../src/decoder/restoration.c) and [`intra_prediction_dip.c`](../src/decoder/intra_prediction_dip.c) with `Av2hSampleDomain`-parameterized 16-bit support (validated against AVM 10-bit/12-bit reference streams).
3. **Align 16-Bit Transform, CfL, MHCCP, and DPCM Rules with AVM Reference:**
   - Verify each remaining 8-bit vs 16-bit divergence against the pinned AVM reference decoder, replacing bring-up restrictions with spec-accurate, bit-depth-parameterized logic and updating [`capabilities.md`](capabilities.md) accordingly.

### Exit Criteria
- 8-bit and 16-bit pipelines follow identical orchestration, diagnostic reporting, and AV2 specification rules, differing only in sample storage type (`uint8_t` vs `uint16_t`) and `Av2hSampleDomain`.
