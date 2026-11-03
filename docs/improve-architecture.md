# Decoder Architecture and Runtime Improvement Plan

**Status:** A1 in progress (frame-decode and reconstruction arena steps complete); A2-A6 planned
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
| **A3** | Replace bit-by-bit loops and per-symbol CDF invariant scans in [`range.c`](../src/decoder/range.c) and [`base.c`](../src/base.c) with byte/word-buffered refills and debug-only CDF assertions | [`range.c`](../src/decoder/range.c), [`range.h`](../src/decoder/range.h), [`base.c`](../src/base.c) | None |
| **A4** | Eliminate unconditional full-frame plane copies and redundant $O(H)$ overlap scans in the loop-filter pipeline | [`filter.c`](../src/decoder/filter.c), [`frame_workspace.c`](../src/decoder/frame_workspace.c), [`internal/filter_common.h`](../src/decoder/internal/filter_common.h) | A1 |
| **A5** | Extend `Av2hExecution` parallelism from final output `memcpy` to multi-tile decoding and loop-filter stripes | [`frame_output.c`](../src/decoder/frame_output.c), [`frame_workspace.c`](../src/decoder/frame_workspace.c), [`filter.c`](../src/decoder/filter.c), [`cdef.c`](../src/decoder/cdef.c), [`gdf.c`](../src/decoder/gdf.c) | A1, A4 |
| **A6** | Converge 8-bit and 16-bit (`_u16`) admission rules, DPCM semantics, and loop-filter support | [`reconstruct.c`](../src/decoder/reconstruct.c), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`chroma_prediction_*.c`](../src/decoder/chroma_prediction.c), [`filter.c`](../src/decoder/filter.c), [`restoration.c`](../src/decoder/restoration.c) | A4 |

---

## A1: Move Large Decoder State Off the Call Stack Into the Workspace Arena

### Problem
Although the decoder enforces a zero-heap-allocation policy via caller-provided workspace memory, a single call to `av2h_decode` currently places **~140–155 KB** of state onto the C call stack:
1. **`Av2hTileDecoder` (~120 KB on stack):** Declared as a local variable in `av2h_decode_frame_group_planar_private` ([`src/decoder/frame_output.c`](../src/decoder/frame_output.c)). Its `Av2hCoeffLevelCdfs` member alone holds ~95 KB of 16-bit CDF arrays, alongside `luma_partition_nodes[2048]` (~12 KB) and `chroma.u_eobs[1024]` (2 KB).
2. **`Av2hCoeffBlock`s (8–16 KB on stack):** `decode_luma_leaf` allocates one `Av2hCoeffBlock` (8 KB: `quantized[1024]` + `dequantized[1024]`) and `decode_chroma_leaf` allocates `Av2hCoeffBlock blocks[2U]` (16 KB) on the stack, in addition to `pending_chroma_blocks[4U][2U]` (64 KB) inside `Av2hFrameDecodeContext`.
3. **`Av2hDecodePreparation` (~10+ KB on stack, twice):** Allocated on the stack in `av2h_query` and again in `av2h_decode` (`Av2hFrameState` embeds `custom_qm[15][3][128]` and `restoration_frame_filter[3][16][18]`).

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

The first implementation step is complete; **A1's global 4 KB exit criterion is
not yet met**.

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

The persistent native 4 KB build guard now covers
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

### Remaining A1 work

Full native stack inventories in both diagnostic configurations now have
**10**, rather than the original 19, frames exceeding 4 KB:

- Preparation and public orchestration: `av2h_query` (16,112 bytes),
  `av2h_decode` (17,712 bytes), and `av2h_cli_decode` (18,384 bytes). Query has no
  workspace argument, so preparation sizing/validation must be separated from
  arena-backed materialized state without adding hidden allocation or breaking
  the two-call public API.
- Six directional-prediction helpers (4,208-11,952 bytes).
- Tile-group ledger parsing (4,256 bytes).

These are real remaining migrations, not suppressed warnings. The global
`-Wframe-larger-than=4096 -Werror` check must only be declared passing after they
are addressed. Other platform-only sources also need the applicable compile
checks before claiming the all-source exit criterion.

---

## A2: Eliminate Redundant Parsing and Full-Input Hashing in `av2h_query` / `av2h_decode`

### Problem
In the normal two-step public API workflow (`av2h_query` followed by `av2h_decode`), header parsing and full-file hashing are repeated multiple times before decoding starts:
1. **Double HEIF Container Parse in `av2h_query`:** `av2h_query` ([`src/query.c`](../src/query.c)) calls `av2h_dialect_detect(input, ...)`—which runs `av2h_heif_parse(input, &image, &strict_error)`—and on the very next step calls `av2h_heif_parse(input, &info->image, error)` again on the same bytes.
2. **Redundant Full `av2h_query` + Double `av2h_decode_prepare` in `av2h_decode`:**
   - `av2h_decode` ([`src/decode.c`](../src/decode.c)) calls `av2h_query(input, NULL, &fresh, error)` from scratch (re-running `av2h_dialect_detect`, both `av2h_heif_parse` passes, `av2h_decode_prepare`, and the full-buffer `input_fingerprint` FNV-1a loop over every byte of `input`).
   - Immediately after `info_matches(info, &fresh)`, `av2h_decode` calls `av2h_decode_prepare(&fresh.image, &preparation, error)` **again**, even though `av2h_query` just executed `av2h_decode_prepare` to compute `fresh.workspace_size`!
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

---

## A3: Optimize Entropy Range Decoder and Bit Reader Hot Loops

### Problem
As noted in [`dev/performance-baseline.md`](dev/performance-baseline.md), lossless photographs and coefficient-heavy 10-bit streams spend a large share of decode time in entropy decoding ([`src/decoder/range.c`](../src/decoder/range.c)). Inspecting [`range.c`](../src/decoder/range.c) and [`base.c`](../src/base.c) reveals three inner-loop inefficiencies:
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

### Actions
1. **Fast Byte/Word Bit Refill in `Av2hRangeDecoder` and `Av2hBitReader`:**
   - Since `Av2hTileDecoder` initializes `Av2hRangeDecoder` with a single contiguous tile span (`span_count == 1`), add a fast path (or cursor pointer + byte-remaining counter) in `Av2hRangeDecoder` that reads full bytes/words when `available_bits >= 8` and `span_count == 1`, falling back to `range_byte_at` only at span boundaries or tail bits.
2. **Separate Trusted Internal CDF Reads from Untrusted Unit-Test Validation:**
   - Keep CDF validity checks in unit tests / debug builds (or on `av2h_cdf_init`), or fold the monotonicity check into the single symbol-search loop so `inverse_cdf` is traversed once instead of twice while preserving exact `AV2H_INVALID_ARGUMENT` behavior on malformed test CDFs.

### Exit Criteria
- `range_input_bits` and `av2h_bit_reader_read` avoid per-bit span/byte re-indexing on contiguous byte-aligned/multi-bit reads.
- Measurable speedup on lossless and high-bitrate cases in [`dev/performance-baseline.md`](dev/performance-baseline.md) with 100% identical decoded hashes and fuzz/unit test results.

---

## A4: Streamline Loop-Filter Staging and Plane Copies

### Problem
In [`src/decoder/filter.c`](../src/decoder/filter.c) and [`src/decoder/frame_workspace.c`](../src/decoder/frame_workspace.c):
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
- Disabled filter stages do not perform redundant full-frame copies.
- Exact pixel parity across all 179 reference streams and all filter unit tests.

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
