# Security Audit and Hardening Plan

**Status:** S1–S2 complete; S3–S10 planned
**Scope:** [`src/`](../src/), [`include/av2heif.h`](../include/av2heif.h), [`Makefile`](../Makefile), and [`tools/`](../tools/)

## Purpose and Scope

Because the decoder is a freestanding, zero-allocation C11 library with a compile-time enforced `< 4 KB` stack frame limit, several traditional C vulnerability classes (heap metadata corruption, `use-after-free`, `double-free`, `printf` format-string bugs, and unbounded recursion stack overflows) are structurally absent from the core.

However, C's lack of implicit bounds checking, signed-integer Undefined Behavior (UB), shared flat arena memory, and complex 2D codec coordinate arithmetic still expose distinct attack surfaces. This document organizes the security audit and hardening work into ten milestones (**S1–S10**), where each milestone targets a specific **vulnerability class, structural risk, hardening mechanism, or non-bitstream threat vector**.

Every milestone must preserve:
- Exact bit-for-bit decoded output on all valid admitted 8-bit and 10-bit reference streams.
- Freestanding C11 compliance (`-std=c11 -ffreestanding -fno-builtin -nostdinc`), zero heap allocation, and `< 4 KB` stack frames.
- Existing formatting, naming, include-order, and header guards (`make format-check`).

---

## Milestone Overview

| Milestone | Category / Threat Vector | Primary Targets | Prerequisites |
| :--- | :--- | :--- | :--- |
| **S1** | **Compiler-Level UB & Warning Hardening:** Eliminate signed-overflow UB (`-fwrapv`), uninitialized stack variables, and implicit narrowing/sign conversions (`-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Wvla`) | [`Makefile`](../Makefile), [`src/`](../src/), [`include/av2heif.h`](../include/av2heif.h) | None |
| **S2** | **Signed Arithmetic, Shift, and `INT32_MIN` UB Audit:** Audit fixed-point solvers, transforms, and motion-vector math for signed overflow, negative shifts, and `INT32_MIN` negation | [`sample_math.h`](../src/decoder/internal/sample_math.h), [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c), [`inverse_transform.c`](../src/decoder/inverse_transform.c), [`secondary_transform.c`](../src/decoder/secondary_transform.c), [`intrabc_prediction.c`](../src/decoder/intrabc_prediction.c), [`restoration.c`](../src/decoder/restoration.c) | S1 |
| **S3** | **2D Coordinate, Negative-Stride, and Boundary OOB Audit:** Audit negative pointer offsets, odd-dimension chroma subsampling, MRL/IBP extensions, and IntraBC/palette/LUT indexing | [`deblock_kernel.inc`](../src/decoder/internal/deblock_kernel.inc), [`intra_prediction_*.c`](../src/decoder/intra_prediction.c), [`reconstruct.c`](../src/decoder/reconstruct.c), [`ccso.c`](../src/decoder/ccso.c), [`gdf_kernel.inc`](../src/decoder/internal/gdf_kernel.inc), [`palette.c`](../src/decoder/palette.c) | S1 |
| **S4** | **Syntax-to-Reconstruction State Desynchronization:** Verify invariants where bitstream syntax (TX size, partition size, SDP, EOB, tile bounds) meets pixel/metadata writers | [`tile_luma.c`](../src/decoder/tile_luma.c), [`tile_chroma.c`](../src/decoder/tile_chroma.c), [`frame_output.c`](../src/decoder/frame_output.c), [`frame_filter_metadata.c`](../src/decoder/frame_filter_metadata.c) | S3 |
| **S5** | **Arena Layout Segregation & Control-Pointer Protection:** Separate pointer-bearing structs from raw pixel/coefficient buffers in `Av2hArena` to prevent pixel overflows from corrupting pointers | [`frame_workspace.c`](../src/decoder/frame_workspace.c), [`frame_output.h`](../src/decoder/frame_output.h) | None |
| **S6** | **Cross-Decode Information Disclosure & Uninitialized Memory:** Ensure reused workspaces and failed decodes cannot leak stale pixels or host heap memory across decodes | [`frame_workspace.c`](../src/decoder/frame_workspace.c), [`frame_output.c`](../src/decoder/frame_output.c), [`decode.c`](../src/decode.c), [`cli_common.c`](../src/shared/cli_common.c) | S5 |
| **S7** | **Algorithmic Complexity, Decompression Bombs & Resource Exhaustion (DoS):** Harden default limits, CLI output amplification (BMP/PNG/RAW bombs), and worst-case parser/filter loops | [`query.c`](../src/query.c), [`container.c`](../src/container.c), [`cli_common.c`](../src/shared/cli_common.c), [`include/av2heif.h`](../include/av2heif.h) | None |
| **S8** | **TOCTOU, Shared-Memory & Concurrent Caller Races:** Protect against concurrent input mutation between `av2h_query` and `av2h_decode` and `mmap` file-truncation hazards | [`query.c`](../src/query.c), [`decode.c`](../src/decode.c), [`src/platform/`](../src/platform/) | None |
| **S9** | **Side-Channel Timing & Cache-Leakage Assessment:** Document and mitigate data-dependent timing/cache side channels for multi-tenant / browser deployments | [`range.c`](../src/decoder/range.c), [`ccso_kernel.inc`](../src/decoder/internal/ccso_kernel.inc), [`gdf_kernel.inc`](../src/decoder/internal/gdf_kernel.inc), [`SECURITY.md`](../SECURITY.md) | S2, S3 |
| **S10** | **Continuous Coverage-Guided Fuzzing & Supply-Chain / Generator Integrity:** Add persistent `libFuzzer` / sanitizer harnesses and verify `.inc` table generators and custom linker invariants | [`tests/`](../tests/), [`tools/`](../tools/), [`Makefile`](../Makefile) | S1–S8 |

---

## S1: Compiler-Level Undefined Behavior and Warning Hardening

### Threat / Problem Category
In standard C11 at `-O2`, the compiler assumes Undefined Behavior (such as signed integer overflow or uninitialized local reads) never occurs and may silently optimize away security bounds checks that depend on wrapped values. Furthermore, [`Makefile`](../Makefile) enables `-Wconversion -Wshadow` for `CENSUS_CFLAGS`, but **not** in `COMMON_WARNINGS` / `FREESTANDING_CFLAGS`, leaving implicit integer narrowing (`size_t` $\rightarrow$ `uint16_t`, `int32_t` $\rightarrow$ `uint8_t`) and signed/unsigned comparisons unchecked by the compiler in the core decoder.

### Actions
1. **Define Signed Overflow Semantics (`-fwrapv`):**
   - Add `-fwrapv` to `COMMON_WARNINGS` / `FREESTANDING_CFLAGS` so signed integer overflow has deterministic two's-complement wraparound semantics across all optimization levels (`-O2`, `-Oz`, `-flto`).
2. **Enable Strict Type-Conversion and Scope Warnings:**
   - Audit and enable `-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Wvla`, and `-Wundef` across [`src/`](../src/) and [`include/`](../include/), making all narrowing casts and sign conversions explicit and reviewed.
3. **Zero-Initialize Stack Automatic Variables & Enable Hosted Stack Canaries:**
   - Enable `-ftrivial-auto-var-init=zero` where supported by the compiler to prevent uninitialized stack reads.
   - Keep `-fno-stack-protector` scoped strictly to freestanding/nolibc object builds while enabling `-fstack-protector-strong` on hosted native test/CLI builds.
4. **Add Compile-Time `_Static_assert` Invariants:**
   - Add C11 `_Static_assert` checks for critical enum bounds, CDF table dimensions, and scratch capacity relationships directly in headers.

### Exit Criteria
- All 76 production C files compile cleanly with `-fwrapv -Wconversion -Wsign-conversion -Wshadow -Wvla -Wundef -Werror`.
- Zero measurable performance regression on the paired benchmark suite.

### S1 Implementation Notes
- Enabled `-Wconversion -Wsign-conversion -Wshadow -Wvla -Wundef` across all production builds (`FREESTANDING_CFLAGS`, hosted sanitized core/CLI/artifact builds, `nolibc`, Windows targets, and [`tools/check_private_headers.py`](../tools/check_private_headers.py)), resolving all 78 narrowing, sign-conversion, and variable-shadowing sites across [`src/`](../src/).
- Enabled `-fwrapv` on all production freestanding, `nolibc`, Windows, and native hosted builds while omitting `-fwrapv` from UBSan builds (`HOST_CFLAGS`) so `-fsanitize=undefined` continues to detect signed integer overflow during S2 and S10.
- Probed and enabled `-ftrivial-auto-var-init=zero` (with `-ftrivial-auto-var-init-max-size=64` on freestanding builds to avoid external `memset`/`bzero` symbol emission) and `-fstack-protector-strong` on hosted builds while keeping `-fno-stack-protector` scoped to freestanding/`nolibc` objects.
- Added compile-time C11 `_Static_assert` invariants across [`include/av2heif.h`](../include/av2heif.h) and private decoder headers covering enum-to-`uint8_t` bounds, CDF metadata/symbol dimensions, and reconstruction/tile scratch capacities.

---

## S2: Signed Arithmetic, Shift, and `INT32_MIN` UB Audit

### Threat / Problem Category
Codec math performs dense fixed-point multiplications, shifts, and matrix inversions on attacker-controlled syntax values (quantized coefficients, CfL alphas, MHCCP neighbor samples, IntraBC motion vectors, Wiener taps). Specific C11 hazards include:
- **Negating `INT32_MIN`:** Expressions like `value < 0 ? -value : value` (present in [`sample_math.h`](../src/decoder/internal/sample_math.h), [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c) `mhccp_mul_fixed`, and [`reconstruct_block.c`](../src/decoder/reconstruct_block.c)) invoke UB if `value == INT32_MIN`.
- **Out-of-range bit shifts:** Shifting a 32-bit integer by `< 0` or $\ge 32$ bits (or left-shifting a negative signed value in C11) is UB. Functions like `av2h_sample_round_pow2(value, shift)`, `av2h_sample_floor_shift`, `mhccp_division_scale`, and `cdef_constrain` compute shift amounts dynamically from bitstream or pixel parameters.
- **32-bit Accumulator Overflow:** Intermediate dot products in `recon_transform_1d` ([`inverse_transform.c`](../src/decoder/inverse_transform.c)), `mhccp_solve` ([`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c)), `pc_wiener_q_adjustment` ([`restoration.c`](../src/decoder/restoration.c)), and `av2h_intrabc_convolve` ([`intrabc_prediction.c`](../src/decoder/intrabc_prediction.c)) must be proven to stay within `int32_t` bounds for all legal 8-bit, 10-bit, and 12-bit inputs, or widened to `int64_t`.

### Actions
1. **Audit and Harden All Shift and Abs/Negation Helpers:**
   - Ensure `av2h_sample_abs`, `av2h_sample_round_pow2`, `av2h_sample_round_pow2_signed`, and `av2h_sample_floor_shift` handle `shift == 0`, upper shift bounds, and extreme negative inputs without UB.
2. **Prove Range Bounds on Transform, MHCCP, IntraBC, and Restoration Accumulators:**
   - Verify worst-case accumulator extrema for 8/10/12-bit domains across `inverse_transform.c`, `secondary_transform.c`, `chroma_prediction_mhccp.c`, `intrabc_prediction.c`, and `restoration.c`, widening to `int64_t` wherever 32-bit products can overflow under adversarial inputs.
3. **Add Extreme-Value Unit Tests Under `-fsanitize=undefined,integer`:**
   - Feed minimum/maximum dequantized coefficients (`-32768..32767` and 12-bit limits), degenerate MHCCP matrices (all-zero, alternating min/max), and extreme IntraBC vectors into every mathematical kernel.

### Exit Criteria
- Every mathematical kernel passes exhaustive boundary-value tests under UBSan (`signed-integer-overflow`, `shift`, `negate-overflow`, `divide-by-zero`) with zero findings.

### S2 Implementation Notes
- Hardened [`sample_math.h`](../src/decoder/internal/sample_math.h), [`sample_math.c`](../src/decoder/sample_math.c), and [`bit_math.h`](../src/shared/bit_math.h) with overflow-free unsigned magnitude helpers (`av2h_sample_abs_u32`, `av2h_sample_abs_u64`), saturating `av2h_sample_abs(INT32_MIN)`, full shift-range guards (`shift <= 0`, `shift >= 64`) in `av2h_sample_floor_shift`, `av2h_sample_round_pow2`, and `av2h_sample_round_pow2_signed`, zero-divisor handling in `av2h_sample_resolve_divisor_32`, and 64-bit variance/covariance regression math in `av2h_sample_regression_slope`.
- Eliminated `INT32_MIN` negation, shift-range hazards, unsigned coordinate underflows, and 32-bit accumulator overflows across [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c), [`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), [`inverse_transform.c`](../src/decoder/inverse_transform.c), [`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`reconstruct_block_kernel.inc`](../src/decoder/internal/reconstruct_block_kernel.inc), [`reconstruct.c`](../src/decoder/reconstruct.c), [`intrabc_prediction.c`](../src/decoder/intrabc_prediction.c), [`intrabc.c`](../src/decoder/intrabc.c), [`restoration.c`](../src/decoder/restoration.c), [`cdef.c`](../src/decoder/cdef.c), [`intra_prediction_edges.c`](../src/decoder/intra_prediction_edges.c), and [`base.c`](../src/base.c).
- Enabled `-fno-sanitize-recover=undefined` globally in `HOST_CFLAGS` and `-fsanitize=integer -fno-sanitize-recover=integer` across all 22 mathematical kernel translation units (`MATH_KERNEL_SOURCES`) in [`Makefile`](../Makefile), and added extreme boundary-value tests in [`sample_math_unit.c`](../tests/sample_math_unit.c), [`prediction_math_unit.c`](../tests/prediction_math_unit.c), and [`reconstruction_block_unit.c`](../tests/reconstruction_block_unit.c).

---

## S3: 2D Coordinate, Negative-Stride, and Boundary OOB Audit

### Threat / Problem Category
Spatial memory safety in an image decoder hinges on 2D index calculations `base + y * stride + x` and lookup table indexing:
- **Negative pointer indexing:**
  - In [`deblock_kernel.inc`](../src/decoder/internal/deblock_kernel.inc), `deblock_filter_choice` and `deblock_filter_lane` index `sample[-3 * step]` and `sample[-((ptrdiff_t)index + 1) * step]` up to 8 samples behind the edge pointer.
  - In [`intra_prediction_directional_kernel.inc`](../src/decoder/internal/intra_prediction_directional_kernel.inc), `above_edge` and `left_edge` are indexed at negative offsets (`reference[-1]`, `base + tap - 1` where `min_base = -1 - mode->mrl_index`) relying on the 5-sample negative prefix in `Av2hDirectionalEdges`.
- **Subsampled chroma & odd frame dimensions:**
  - When `frame->width` or `frame->height` is odd on `4:2:0` streams, luma coordinates `(block_x, block_y)` and coded plane dimensions (`av2h_filter_coded_dimension`) round differently across luma and chroma planes. Any mismatch in CfL/MHCCP luma-to-chroma coordinate mapping (`center_x = luma_x - 2`, `center_y = luma_y + row * 2`) can read past the right or bottom of `luma_plane->data`.
- **Bitstream-driven LUT indices:**
  - Palette color maps (`av2h_palette_map_get` / `set`), CCSO LUT offsets (`lut_index = (band << 4) + (first_class << 2) + second_class`), GDF tables (`qp_index * 4096 + position`), and IST scan tables (`input_scan[index]`, `output_scan[index]`).

### Actions
1. **Audit Negative-Offset Preconditions in Deblock and Directional Edge Kernels:**
   - Verify and assert that `deblock_edge`'s `negative_required` / `positive_required` guard (`x >= negative_required`, `width - x > positive_required`) strictly dominates every `sample[offset * step]` read/write in `deblock_filter_choice` and `deblock_filter_lane`.
   - Verify that `above_edge` / `left_edge` negative indices in Zones 1, 2, and 3 cannot step before the 5-sample prefix for any combination of `mrl_index` (`0..3`), `angle`, and `tap` (`0..3`).
2. **Audit Cross-Plane (Luma $\leftrightarrow$ Chroma) Coordinate Accesses:**
   - Audit every `luma_plane->data[...]` access in [`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c), [`ccso_kernel.inc`](../src/decoder/internal/ccso_kernel.inc), and [`restoration.c`](../src/decoder/restoration.c) (`wiener_nonsep_luma_sample`) against minimum/odd frame dimensions (`1×1`, `3×3`, `17×15`) and non-zero tile start offsets.
3. **Verify LUT Index Bounds:**
   - Confirm that every table lookup indexed by derived pixel/syntax features is either masked or bounds-checked against the array's `_Static_assert` capacity.

### Exit Criteria
- All 2D plane and LUT index expressions have verified lower (`>= 0`) and upper (`< capacity`) bounds across odd crops, tile boundaries, and maximum MRL/filter widths.

---

## S4: Syntax-to-Reconstruction State Desynchronization Audit

### Threat / Problem Category
In complex codecs, vulnerabilities frequently arise when the **syntax parser** and the **reconstruction/filter engine** disagree on the state of a block or frame:
- **Deferred Shared Chroma (`pending_chroma_leaves`):** In [`frame_output.c`](../src/decoder/frame_output.c), up to `AV2H_PENDING_CHROMA_CAPACITY` (4) chroma leaves are queued while waiting for colocated luma subtrees to finish. If a crafted partition tree queues more than 4 shared chroma leaves, or if the tile ends while `pending_chroma_count != 0` without draining, state could desynchronize.
- **Lossless Transform Splitting (`u_eobs[1024]`, `luma_partition_nodes[2048]`):** In [`tile.h`](../src/decoder/tile.h) and [`tile_chroma.c`](../src/decoder/tile_chroma.c), lossless coding blocks split into 4×4 transform sub-nodes. Bounds on `tx_index`, `tx_count`, and `u_eobs` must never exceed fixed array capacities even on $128\times 128$ or $256\times 256$ roots.
- **Filter Control Handoff vs. Partial Tile Decode:** Ensure that CDEF, CCSO, GDF, and Restoration unit counts decoded in [`filter_entropy.c`](../src/decoder/filter_entropy.c) cannot exceed the capacities allocated in [`frame_workspace.c`](../src/decoder/frame_workspace.c) when superblock or tile geometry is non-uniform.

### Actions
1. **Audit Queue and Fixed-Array Capacities in `Av2hTileDecoder`:**
   - Verify that `pending_chroma_count < AV2H_PENDING_CHROMA_CAPACITY`, `tx_index < AV2H_MAX_LOSSLESS_CHROMA_TX_NODES`, and `luma_partition_node_count < AV2H_MAX_LUMA_PARTITION_NODES` fail closed with `AV2H_INVALID_DATA` or `AV2H_OVERFLOW` before any array write, and that `finish_frame_tiles` rejects any undrained `pending_chroma_count != 0`.
2. **Audit Filter Entropy Unit Indexing:**
   - Verify that every write to `gdf_block_flags`, `cdef_unit_strengths`, `ccso_unit_controls`, `restoration_unit_flags`, and `restoration_unit_taps` in [`filter_entropy.c`](../src/decoder/filter_entropy.c) checks against the arena-bound capacity from `Av2hTileContextStorage`.

### Exit Criteria
- Every fixed-capacity queue and arena-backed control array in tile/leaf/filter parsing is guarded against overflow and checked for completeness before filter execution.

---

## S5: Arena Layout Segregation and Control-Pointer Protection

### Threat / Problem Category
Currently, [`frame_workspace.c`](../src/decoder/frame_workspace.c) allocates buffers inside `Av2hArena` in the following order:
1. `filter_memory` (pixel planes and deblock cells),
2. `coded[0..2]` pixel planes and `reconstructed` maps,
3. `tile_context` arrays,
4. `decode_scratch` (`Av2hFrameDecodeScratch`),
5. `reconstruction_scratch`,
6. `tile_workers` (`Av2hFrameTileWorker` structs, which contain embedded `Av2hFrameDecodeWorkspace` pointers, `diagnostics` pointers, and `Av2hFilterWorkspace` pointers).

Because `decode_scratch` and `tile_workers` are placed at **higher memory addresses** than the pixel buffers (`filter_memory`, `coded`) and `reconstruction_scratch`, a forward buffer overflow in a pixel plane or reconstruction scratch buffer could overwrite pointer fields inside `tile_workers` or `decode_scratch`, escalating a pixel OOB write into arbitrary pointer corruption.

### Actions
1. **Reorder `Av2hArena` Allocations (Control Structs First, Raw Sample Buffers Last):**
   - In `frame_decode_workspace_bind` ([`src/decoder/frame_workspace.c`](../src/decoder/frame_workspace.c)), allocate all pointer-bearing and control structures (`decode_scratch`, `tile_workers`) at the **lowest addresses** of the arena, followed by metadata/context grids (`tile_context`), and place all bulk sample/coefficient buffers (`reconstruction_scratch`, `coded` planes, `filter_memory` planes) at the **highest addresses** of the arena.
2. **Optional Canary Words Between Arena Regions (Debug / Hardened Builds):**
   - Place lightweight magic canary words between major arena sub-allocations in diagnostic/sanitized builds (verified at the end of `av2h_decode`) to detect even 1-byte intra-arena overruns immediately.

### Exit Criteria
- No pointer-bearing struct in `Av2hArena` resides at a higher memory address than any pixel plane or transform/predictor scratch buffer.

---

## S6: Cross-Decode Information Disclosure and Output-Buffer Hygiene

### Threat / Problem Category
Two information-disclosure risks exist when integrating a C decoder into a larger host application (such as a server, browser, or thumbnailer):
1. **Cross-Decode Workspace Bleed:** A host application that allocates a single large `workspace` buffer and reuses it across decodes of different users' images relies on the decoder never reading uninitialized workspace bytes from a prior decode into the current image's output.
2. **Unchecked Return Code in Host Caller ("Stagefright-Style" Heap Leak):** By contract, `av2h_decode` leaves `output` untouched on failure. However, if a buggy host caller allocates `output` via `malloc()` (containing sensitive uninitialized heap data), calls `av2h_decode`, **forgets to check the return status**, and encodes/displays `output`, the host leaks its own heap memory as an image.

### Actions
1. **Expand Poisoned-Workspace Differential Testing:**
   - Extend the poisoned-workspace test suite (`0x00`, `0xFF`, `0xA5`, `0x5A` patterns) to cover multi-tile parallel execution (`tile_workers`), all loop-filter bypass combinations, and odd-dimension crops, proving that decoded output bytes are 100% independent of initial `workspace` contents.
2. **Document and Offer Zero-on-Failure / Safe Output Semantics:**
   - Document the caller's obligation to check `Av2hStatus` in [`include/av2heif.h`](../include/av2heif.h), and ensure that inside the CLI frontends ([`src/shared/cli_common.c`](../src/shared/cli_common.c)), `output` memory is never written to disk or previewed if `av2h_cli_decode` returns a non-`AV2H_OK` status.

### Exit Criteria
- Decoded outputs and error records are proven invariant across arbitrary initial `workspace` poison patterns in both serial and multi-threaded execution.

---

## S7: Algorithmic Complexity, Decompression Bombs, and Resource Exhaustion (DoS)

### Threat / Problem Category
An attacker can craft a tiny (< 1 KB) valid AV2-HEIF file or raw OBU stream that triggers disproportionate CPU or memory consumption:
1. **Permissive Default Limits (`av2h_limits_default`):**
   - In [`src/query.c`](../src/query.c), `av2h_limits_default` sets `max_width = 16384`, `max_height = 16384`, `max_pixels = 268,435,456` (268 MP), `max_output_size = 402 MB`, and `max_workspace_size = 1 GiB`. A caller passing `limits = NULL` can be forced to allocate ~1.4 GiB of RAM per image.
2. **CLI Output Amplification Bombs:**
   - The standalone CLI frontends (`av2h_cli_decode` in [`src/shared/cli_common.c`](../src/shared/cli_common.c)) decode raw OBU streams directly without consulting `Av2hLimits`, allowing up to the 16-bit frame header maximum ($65536 \times 65536$ if sequence header bits permit) and writing hundreds of megabytes of uncompressed BMP/PNG/RAW data to disk from a few kilobytes of input.
3. **Box / OBU / Partition Traversal Complexity:**
   - Ensure container box loops ([`container.c`](../src/container.c)), OBU scans ([`obu.c`](../src/decoder/obu.c)), and zero-byte/empty-span edge cases strictly make forward progress and terminate in $O(\text{input\_size})$ or $O(\text{pixels})$ steps.

### Actions
1. **Enforce Reasonable Default Limits & Bound CLI Decode Allocations:**
   - Review `av2h_limits_default` in [`src/query.c`](../src/query.c) and apply explicit dimension, pixel, and workspace caps inside `av2h_cli_decode` ([`src/shared/cli_common.c`](../src/shared/cli_common.c)) so the standalone CLI cannot be used as an unbounded memory/disk-amplification vector.
2. **Audit Loop Termination and Forward Progress:**
   - Verify that every `while` / `for (;;)` loop in [`src/`](../src/) (`range_read_symbol`, `av2h_parallel_for`, `decode_tile_leaves`, `av2h_partition_step`, `av2h_obu_parse`, `read_box`) has a strictly decreasing/increasing bounded variant that guarantees termination without hangs.

### Exit Criteria
- Both public API (`av2h_query` / `av2h_decode`) and CLI (`av2h_cli_decode`) enforce bounded pixel, workspace, and output-allocation limits.
- Every unbounded `while`/`for (;;)` loop in `src/` has a documented termination invariant.

---

## S8: TOCTOU, Shared-Memory, and Concurrent Caller Races

### Threat / Problem Category
When the decoder is called on memory buffers shared across threads or processes (e.g., `mmap`ed files or IPC shared memory):
1. **Time-of-Check to Time-of-Use (TOCTOU) Between `av2h_query` and `av2h_decode`:**
   - Although `av2h_decode` re-verifies `info_matches(info, &fresh)` at the start of `av2h_decode`, the subsequent `av2h_decode_materialize` and `av2h_decode_frame_group_planar_private` steps read from `input.data` again. If a concurrent thread mutates `input.data` *during* `av2h_decode`, the header invariants checked during `av2h_query_with_preparation` could be violated mid-decode.
2. **Input File Size / `mmap` Races in CLI Frontends:**
   - In [`src/platform/nolibc/decoder.c`](../src/platform/nolibc/decoder.c), ` src/platform/posix/decoder.c`, and [`src/platform/windows/decoder.c`](../src/platform/windows/decoder.c), files are sized with `lseek(SEEK_END)` / `fseek` / `GetFileSizeEx` and then read into an anonymously allocated buffer (`MAP_PRIVATE | MAP_ANONYMOUS` / `posix_memalign` / `VirtualAlloc`). This copy-on-read pattern already protects against `SIGBUS` from concurrent file truncation after read completes, provided short reads are rejected.

### Actions
1. **Verify Single-Read / Fail-Closed Semantics in `av2h_decode_materialize`:**
   - Ensure `av2h_decode_materialize` and tile-group span slicing re-check bounds and fail closed with `AV2H_INVALID_DATA` / `AV2H_TRUNCATED` even if underlying bytes change between `av2h_query_with_preparation` and `av2h_decode_materialize`, and document in [`include/av2heif.h`](../include/av2heif.h) that `input.data` must not be mutated concurrently during an active `av2h_decode` call.
2. **Audit Platform File Readers for Special Files (FIFOs, Devices, `/proc`):**
   - Verify that `read_entire_file` across `posix`, `nolibc`, and `windows` safely handles non-seekable streams, zero-length files, and files that shrink or grow between `seek` and `read`.

### Exit Criteria
- Mutating `input` bytes between `av2h_query` and `av2h_decode` (or shrinking/growing files during CLI reads) never causes out-of-bounds memory access.

---

## S9: Side-Channel Timing and Cache-Leakage Assessment

### Threat / Problem Category
Media decoders are not traditionally written to be constant-time (unlike cryptographic primitives). In [`src/decoder/`](../src/decoder/):
- Execution time depends on image content (e.g., `all_zero` transform-block skips, CDEF zero-variance skips, Golomb-Rice escape lengths).
- Memory addresses depend on pixel values (e.g., `gdf_intra_error[qp_index * 4096 + position]`, `parameters->filter_offset[lut_index]` in CCSO, `mode->palette_colors[color_index]`, and IntraBC source coordinates).

In a multi-tenant or browser environment where untrusted code can time cross-origin image decodes or mount Prime+Probe cache attacks, these data-dependent branches and table lookups can leak pixel characteristics.

### Actions
1. **Document Threat Model Boundaries in [`SECURITY.md`](../SECURITY.md):**
   - Explicitly state that the decoder is **not constant-time** with respect to bitstream content or pixel values, so callers processing secret/cross-origin images in untrusted execution environments must enforce process/site isolation and coarse-grained timer mitigations.
2. **Audit Table Lookups for Out-of-Spec Secret Leakage:**
   - Ensure data-dependent table lookups (palette, CCSO, GDF, IST, CDEF) are strictly clamped to their table array bounds so speculative or architectural accesses never touch memory outside the static table.

### Exit Criteria
- All data-dependent table indices are strictly masked/clamped to table bounds, and timing/cache side-channel properties are explicitly documented in [`SECURITY.md`](../SECURITY.md).

---

## S10: Continuous Coverage-Guided Fuzzing and Supply-Chain Integrity

### Threat / Problem Category
1. **Structured Bitstream Fuzzing Depth:**
   - Deterministic byte-truncation and bit-flip tests (`2,460` cases in `make test-c`) validate known feature seeds, but cannot explore deep multi-symbol entropy trajectories (because flipping a raw byte in a range-coded stream usually desynchronizes the entropy state within a few leaves).
2. **Generator and Custom Linker Integrity:**
   - The build relies on 23 generated `.inc` tables and (for macOS `nolibc`) a custom linker under [`tools/macho_linker/`](../tools/macho_linker/). Any corrupted constant in a `.inc` file could violate range-decoder CDF invariants or array bounds assumed by the C code.

### Actions
1. **Add a Dedicated `libFuzzer` / Sanitizer Target (`fuzz_decode`):**
   - Add an opt-in `LLVMFuzzerTestOneInput` harness in [`tests/`](../tests/) (compiled with `-fsanitize=fuzzer,address,undefined,integer`) that exercises both `av2h_query` + `av2h_decode` (HEIF container path) and raw OBU decoding (`av2h_cli_decode` path) seeded with all 179 corpus streams.
2. **Automated `.inc` Table Invariant Verification:**
   - Add a Python test guard that parses all 23 `.inc` data files and verifies their structural invariants (monotonicity and `0`-termination of all static CDF tables, exact array lengths, and value bounds for IST/GDF/PC-Wiener weights).

### Exit Criteria
- Static `.inc` table invariants are automatically verified in the Python test suite.
- The `libFuzzer` + ASan/UBSan/IntegerSanitizer target runs cleanly over the seed corpus with zero crashes, leaks, or UB reports.
