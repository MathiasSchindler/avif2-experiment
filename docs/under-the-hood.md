# Under the Hood: AV2-HEIF Decoder Architecture and Internals

This document describes the internal architecture, data structures, memory model, and execution flow of the AV2-HEIF still-image decoder as implemented in [`src/`](../src/) and [`include/av2heif.h`](../include/av2heif.h).

---

## 1. High-Level System Architecture

The decoder is a freestanding C11 library (`-ffreestanding -fno-builtin -nostdinc`) with zero dynamic memory allocations (`malloc`/`free` do not exist in the core) and a compile-time enforced `< 4 KB` per-function stack frame limit. All variable-sized and large working state resides in a single caller-supplied workspace buffer managed by a deterministic bump allocator (`Av2hArena`).

The codebase is organized into five strictly layered subsystems:

```text
+-----------------------------------------------------------------------+
| 1. Platform & CLI Frontends (src/platform/*, src/shared/cli_*)        |
|    OS file/console I/O, memory mapping, PNG/BMP/RAW output, traces    |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 2. Public Query & Decode Facade (include/av2heif.h, query.c, decode.c)|
|    Limit checks, input fingerprinting, workspace sizing & binding     |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 3. Container & Carriage Layer (bmff.c, container.c, carriage.c, obu.c)|
|    ISOBMFF/HEIF box parsing, av2C configuration, OBU ledger framing   |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 4. Bitstream & Frame Coordinator (src/decoder/*)                      |
|    Sequence/Frame headers -> Tile state machine -> Leaf syntax ->     |
|    Prediction -> Inverse Transform -> Plane Reconstruction -> Filters |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 5. Freestanding Substrate & Runtime (base.c, src/shared/*, runtime/*) |
|    Checked size_t math, Bit/Byte readers, Arena, parallel_for pool    |
+-----------------------------------------------------------------------+
```

Dependency direction is strictly top-down: numerical prediction, transform, and filter kernels operate on value structs and plane descriptors without importing syntax parsers or container types.

---

## 2. Foundational Substrate ([`src/base.h`](../src/base.h), [`src/shared/`](../src/shared/))

### 2.1 Checked Size Arithmetic and Memory Primitives
Because freestanding builds do not link `libc`, [`src/base.c`](../src/base.c) provides:
- `av2h_memory_copy`, `av2h_memory_fill`, and `av2h_memory_compare`.
- Endian load/store helpers (`av2h_load_u16be`/`u32be`/`u64be`, `av2h_load_u16le`/`u32le`, `av2h_store_u16le`/`u32le`/`u16be`/`u32be`).
- Overflow-checked `size_t` arithmetic: `av2h_size_add`, `av2h_size_multiply`, and power-of-two `av2h_size_align`. Every buffer size, stride, and offset calculation in the decoder routes through these predicates.

### 2.2 Sticky-Status Readers (`Av2hByteReader` and `Av2hBitReader`)
Fixed-bit and variable-length header syntax is parsed through `Av2hByteReader` and `Av2hBitReader`:
- Both readers hold a sticky `Av2hStatus status` field initialized to `AV2H_OK`.
- If a read request exceeds the remaining byte/bit span or violates an alignment/argument constraint, the reader transitions `status` to `AV2H_TRUNCATED` or `AV2H_INVALID_ARGUMENT` and returns `0`.
- Subsequent reads on a failed reader are no-ops returning `0`. This allows complex header parsers ([`sequence.c`](../src/decoder/sequence.c), [`frame.c`](../src/decoder/frame.c)) to parse sequential syntax fields cleanly and check `reader.status` at logical milestones without risking out-of-bounds reads.
- `Av2hBitReader` extracts multi-bit fields in byte chunks via `av2h_read_bits_msb` ([`src/shared/bit_math.h`](../src/shared/bit_math.h)) and implements AV2's unsigned/signed variable-length codes (`uvlc`, `svlc`), `inverse_signed`, and `quasi_uniform` primitives.

### 2.3 Dual-Mode Workspace Arena (`Av2hArena`)
`Av2hArena` is a bump allocator that operates in two modes controlled by its `sizing_only` flag:
1. **Sizing Mode (`av2h_arena_init_sizing`):** `av2h_arena_allocate` performs alignment rounding and overflow-checked offset advancement without dereferencing or returning memory pointers (`returns NULL`), accumulating the exact byte count in `arena->used`.
2. **Binding Mode (`av2h_arena_init`):** Given a caller-allocated buffer of at least `av2h_arena_required(&sizing_arena)` bytes, `av2h_arena_allocate` carves out aligned sub-buffers from `arena->data`.

Because [`frame_workspace.c`](../src/decoder/frame_workspace.c) executes the exact same `frame_decode_workspace_bind` function in both modes, it is structurally impossible for the workspace size reported by `av2h_query` to diverge from the memory consumed during `av2h_decode`.

---

## 3. Container, Carriage, and Two-Phase Decode Flow

### 3.1 ISOBMFF / HEIF Container Parsing ([`container.c`](../src/container.c), [`bmff.c`](../src/bmff.c))
`av2h_heif_parse` parses the ISO Base Media File Format (ISOBMFF) box hierarchy:
- Validates the `ftyp` box (`mif1` brand) and top-level `meta` box.
- Resolves the primary item ID from `pitm`, locates its byte extent in `iloc` / `mdat`, validates item type `av02` in `iinf`, and walks item properties in `iprp` (`ipco` + `ipma`).
- Reconciles container-level properties (`ispe` dimensions, `pixi` channel depths, `colr` color description, and the `av2C` decoder configuration record) against the Sequence Header OBU carried inside `av2C.configOBUs`.
- [`query.c`](../src/query.c) also supports dialect detection (`av2h_dialect_detect`) to distinguish canonical v0 files (9-byte draft `av2C` with Sequence Header in `configOBUs`) from legacy experimental `libavif`/AVM files (4-byte `av2C` with Sequence Header in the item payload).

### 3.2 OBU Framing ([`obu.c`](../src/decoder/obu.c), [`carriage.c`](../src/carriage.c))
`av2h_obu_parse` scans an input span into an `Av2hObuLedger` (an array of `Av2hObu` descriptors recording OBU type, header size, LEB128 payload size, and payload offset). In canonical still-image carriage:
- `configOBUs` carries the `AV2H_OBU_SEQUENCE_HEADER` (and optional `AV2H_OBU_CONTENT_INTERPRETATION` / `AV2H_OBU_QUANTIZATION_MATRIX`).
- `item_payload` carries the `AV2H_OBU_CLOSED_LOOP_KEY` frame (and optional quantization-matrix or tile-group OBUs).

### 3.3 Query vs. Decode Lifecycle ([`query.c`](../src/query.c), [`decode.c`](../src/decode.c))
Decoding a container image uses a two-call contract:

1. **`av2h_query(input, limits, &info, &error)`:**
   - Parses the HEIF container and validates all user/default `Av2hLimits` (input size, dimensions, pixel count, OBU count, output size, workspace size).
   - Runs `av2h_decode_prepare` in metadata-only mode: parses the Sequence Header (`Av2hSequenceState`), Frame Header (`Av2hFrameState`), Quantization Matrix references, and Tile Group headers without materializing large lookup tables on the stack.
   - Runs the sizing arena to populate `info->workspace_size` and `info->workspace_alignment`, computes packed planar output layouts (`info->planes[0..2]` and `info->output_size`), and records a 64-bit FNV-1a fingerprint (`info->input_fingerprint`) over the container metadata and OBU spans.

2. **`av2h_decode(input, &info, workspace, workspace_size, output, output_size, &error)`:**
   - Re-validates `input` via `av2h_query_with_preparation` and checks `info_matches(info, &fresh)` to ensure the caller did not mutate or swap the input buffer or `Av2hInfo` descriptor between `av2h_query` and `av2h_decode`.
   - Verifies that `workspace`, `output`, `input`, `info`, and `error` memory ranges are mutually disjoint and properly aligned.
   - Initializes `Av2hFrameDecodeWorkspace` from the caller's `workspace` buffer and calls `av2h_decode_materialize` to populate the arena-backed `Av2hFrameSyntaxStorage` (custom quantization matrices, restoration filters, CCSO offsets) and `Av2hTileGroup` spans.
   - Invokes `av2h_decode_frame_group_planar_private` to decode all tiles, run loop filters, and copy the visible image to `output`.

*(Note: The CLI frontends in [`src/platform/`](../src/platform/) also support decoding raw OBU streams directly via [`src/shared/cli_common.c`](../src/shared/cli_common.c), which runs the same sequence/frame/qmatrix/tile-group preparation and workspace binding steps without the HEIF container wrapper.)*

---

## 4. Workspace Memory Layout ([`frame_workspace.c`](../src/decoder/frame_workspace.c))

When `av2h_frame_decode_workspace_init` binds a workspace buffer, the `Av2hArena` partitions it into the following disjoint regions:

```text
+--------------------------------------------------------------------------+
| Caller-Provided Workspace Buffer (aligned to 8 bytes)                    |
+--------------------------------------------------------------------------+
| 1. Filter Workspace Sub-Arena (Av2hFilterWorkspace)                      |
|    - deblocked plane buffers (Y, U, V)                                   |
|    - cdef plane buffers (Y, U, V)                                        |
|    - restoration luma plane buffer (when Restoration enabled on 8-bit)   |
|    - deblock_cells[0..2] (4x4 cell metadata grid per plane)              |
|    - ccso_controls[0..2] & restoration_tx_skip map                       |
+--------------------------------------------------------------------------+
| 2. Coded Reconstruction Planes (coded[0..2] or coded_u16[0..2])          |
|    - Padded sample buffers (uint8_t or uint16_t) for Y, U, V             |
|    - reconstructed[0..2] readiness maps (1 byte per 4x4 MI cell)         |
+--------------------------------------------------------------------------+
| 3. Tile Entropy & Spatial Context Storage (Av2hTileContextStorage)       |
|    - Above & Left partition contexts (luma & chroma)                     |
|    - Above & Left luma mode contexts (Av2hLumaModeContext)               |
|    - Above & Left coefficient contexts                                   |
|    - Colocated luma mode grid (Av2hTileLumaModeCell per 4x4 MI cell)     |
|    - Filter control arrays (GDF flags, CDEF strengths, CCSO, Restoration)|
+--------------------------------------------------------------------------+
| 4. Frame Decode Scratch (Av2hFrameDecodeScratch, ~180 KB)                |
|    - Av2hTileDecoder (all adaptive CDFs, range decoder, traversal stacks)|
|    - active_blocks[2] (Av2hCoeffBlock for current luma or U/V leaf)      |
|    - pending_chroma_leaves[4] & pending_chroma_blocks[4][2]              |
|    - Av2hDecodePreparation, Av2hFrameSyntaxStorage, Av2hTileGroup        |
+--------------------------------------------------------------------------+
| 5. Block Reconstruction Scratch (Av2hReconstructionScratch / U16)        |
|    - Transform buffers: scaled[1024], transposed[1024], residual[1024]   |
|    - Predictor buffer: predictor[4096]                                   |
|    - Reference edges: above[128], left[128], secondary_above/left[128]   |
|    - Block output: Av2hBlockReconstruction / U16                         |
+--------------------------------------------------------------------------+
| 6. Optional Parallel Tile Worker State (when execution->width > 1)       |
|    - Per-worker Av2hFrameTileWorker descriptors                          |
|    - Per-worker Av2hTileContextStorage, Av2hFrameDecodeScratch,          |
|      and Av2hReconstructionScratch for workers 1..N-1                    |
+--------------------------------------------------------------------------+
```

---

## 5. Entropy Coding Engine ([`range.c`](../src/decoder/range.c), [`cdf.c`](../src/decoder/cdf.c))

AV2 uses a multi-symbol Q15 range decoder (`Av2hRangeDecoder`) with up to 16 symbols per alphabet:
- **State:** `range` ($\in [32768, 65535]$ before symbol subdivision), `value` (15-bit window into the bitstream XORed with `0x7fff`), `available_bits`, and `tell` (exact fractional/bit position tracker).
- **CDF Representation:** Stored as inverted Q15 cumulative distribution arrays (`uint16_t inverse_cdf[]`), terminated by `0` at `inverse_cdf[symbol_count - 1]`, followed by an adaptation counter at `inverse_cdf[symbol_count]` and rate parameters.
- **Symbol Decoding (`av2h_range_read_symbol`):**
  - Fast-paths binary symbols (`symbol_count == 2U`) with a single probability scale and comparison.
  - For multi-symbol alphabets ($3 \le N \le 16$), steps through `inverse_cdf` computing `lower = range_probability_scale(...)` while simultaneously verifying monotonic non-increasing CDF order.
- **CDF Adaptation (`av2h_range_read_adaptive`):**
  - Unless `frame->disable_cdf_update` is set, updates `inverse_cdf[0 .. symbol_count - 2]` in place after each symbol read using a shift rate derived from the symbol count and adaptation counter (up to 32 updates).
- **Normalization (`range_normalize`):**
  - Computes `shift = 15U - av2h_floor_log2_u32(decoder->range)` using a single CLZ instruction (`__builtin_clz` with portable fallback), shifts `range` left, and pulls `shift` bits from the tile span in a single byte-chunk read (`av2h_read_bits_msb`).

---

## 6. Tile & Superblock State Machine ([`tile.c`](../src/decoder/tile.c), [`tile_luma.c`](../src/decoder/tile_luma.c), [`tile_chroma.c`](../src/decoder/tile_chroma.c))

### 6.1 Superblock and Partition Tree Traversal ([`partition.c`](../src/decoder/partition.c))
A tile is a rectangular grid of superblocks ($64\times 64$ or $128\times 128$ luma samples, i.e., $16\times 16$ or $32\times 32$ 4×4 MI units).
- For each superblock, `Av2hPartitionTraversal` maintains an explicit stack of `Av2hPartitionNode`s (`row`, `column`, `block_size`).
- At each internal node, `av2h_partition_step` computes the neighborhood context from `Av2hPartitionContextState` (`above` and `left` MI arrays) and reads the partition symbol (`NONE`, `HORZ`, `VERT`, `SPLIT`, extended 3-way `HORZ_A`/`HORZ_B`/`VERT_A`/`VERT_B`/`HORZ_3`/`VERT_3`, or uneven 4-way `HORZ_4A`/`HORZ_4B`/`VERT_4A`/`VERT_4B`), pushing child nodes onto the traversal stack in reverse coding order so they are popped in causal Z-scan order.

### 6.2 Luma / Chroma Phase Interleaving and SDP
AV2 supports **Separate Decomposition Tree (SDP)** (`sequence->enable_sdp`), where luma and chroma have partially independent partition trees within a superblock. `Av2hTileDecoder` coordinates this via `decoder->control.phase`:
1. **`AV2H_TILE_DECODE_LUMA`:** Pops luma leaf nodes (`av2h_tile_decoder_next_luma`), parses luma mode syntax, transform partitions, and coefficient blocks, and returns each leaf to [`frame_output.c`](../src/decoder/frame_output.c) for immediate plane reconstruction.
   - As each luma leaf is decoded, its mode (`mode`, `angle_delta`, `q_index`, `chroma_cfl`) is recorded into the `colocated_luma` MI grid so the subsequent chroma tree can query colocated luma modes and CfL eligibility.
2. **Shared Chroma Mode Handoff:** For large root partitions ($128\times 128$, $128\times 64$, $64\times 128$) where chroma mode syntax is signaled in the bitstream before all colocated luma transform units finish, `Av2hTileDecoder` temporarily switches to `AV2H_TILE_DECODE_CHROMA`, parses up to 4 chroma leaves into `pending_chroma_leaves` / `pending_chroma_blocks`, sets `decoder->chroma.shared_complete = 1U`, and resumes `AV2H_TILE_DECODE_LUMA`. Once the colocated luma pixels are reconstructed in the plane buffer, `flush_pending_chroma_leaves` reconstructs the queued chroma leaves.
3. **`AV2H_TILE_DECODE_CHROMA`:** After the superblock's luma tree completes, the chroma traversal pops chroma leaves (`av2h_tile_decoder_next_chroma`), reading U and V transform/coefficient syntax and reconstructing U and V planes.
4. **Superblock-Level Loop Filter Syntax ([`filter_entropy.c`](../src/decoder/filter_entropy.c)):** Interleaved at superblock/coding-unit boundaries in the bitstream, the tile decoder also entropy-decodes per-unit control flags and filter taps for **GDF**, **CDEF**, **CCSO**, and **Restoration** (`PC-Wiener` / `Wiener-nonsep`).

---

## 7. Leaf Syntax, Transform Trees, and Coefficients

For each coding leaf popped from the partition tree:

1. **Leaf Mode Syntax ([`leaf.c`](../src/decoder/leaf.c), [`palette.c`](../src/decoder/palette.c), [`intrabc.c`](../src/decoder/intrabc.c)):**
   - Reads segment ID, delta-Q (`current_block_q_index`), and superblock filter flags.
   - For luma: reads `skip_transform`, `use_intrabc` (plus IntraBC displacement vector via DRL/shell coding and `morph_pred`), or intra directional/non-directional mode (`DC`, `V`, `H`, `D45`..`D203`, `SMOOTH`, `SMOOTH_V`, `SMOOTH_H`, `PAETH`), `angle_delta`, Multi-Reference Line index (`mrl_index` / `multi_line_mrl`), Intra Block Prediction (`enable_ibp`), Intra-DIP (`use_intra_dip`), Palette mode (`palette_size`, colors, and color index map), DPCM flags, and Forward Scan Coding (`fsc_mode`).
   - For chroma: reads `is_cfl`, MHCCP switch and direction (`AV2H_CFL_MULTI_PARAM`), or CfL index (`AV2H_CFL_DERIVED_ALPHA` vs `AV2H_CFL_EXPLICIT_ALPHA` signs and magnitudes), or standard intra UV mode.

2. **Transform Partitioning & Geometry ([`tx_geometry.c`](../src/decoder/tx_geometry.c), [`tile_luma.c`](../src/decoder/tile_luma.c)):**
   - A coding block may split into multiple transform units (TX partitions up to `AV2H_MAX_TX_PARTITION_NODES`, or 4×4 sub-blocks in lossless mode `q_index == 0`).

3. **Coefficient Prefix & Transform Type ([`coeff.c`](../src/decoder/coeff.c), [`transform.c`](../src/decoder/transform.c)):**
   - Reads `all_zero` (TXB skip) and End-of-Block (`eob_point` + `eob` position).
   - Reads primary 2D transform type (`DCT_DCT`, `ADST_DCT`, `DCT_ADST`, `ADST_ADST`, `FLIPADST_*`, 1D `V_DCT`/`H_DCT`/`V_ADST`/`H_ADST`, or `IDTX`) and secondary transform (`IST`) set/symbol when `eob` falls within the signaling threshold.

4. **Coefficient Levels & Dequantization ([`coeff_level.c`](../src/decoder/coeff_level.c), [`coeff_dequant.c`](../src/decoder/coeff_dequant.c), [`qmatrix.c`](../src/decoder/qmatrix.c)):**
   - Standard blocks scan from `eob - 1` down to `0` along the transform's 2D scan order (`av2h_coeff_scan_position`: diagonal, vertical, or horizontal scan), decoding base levels (`full_base` / `q0_txs*`), low-frequency/high-frequency mid-range levels (`full_br`), Golomb-Rice escape levels (`read_high_range`), and signs (with optional parity hiding and Trellis-Coded Quantization `TCQ` state transitions).
   - Forward Scan Coding (`fsc_mode`) blocks scan forward from Beginning-of-Block (`bob = count - eob`) to the end of the block.
   - Each non-zero coefficient is dequantized immediately using the plane's DC/AC quantizer step (`coeff_dequant.c`) scaled by the active quantization matrix (`qmatrix.c`) and stored in `block->dequantized[]`.

---

## 8. Typed Sample Kernels: The `*_kernel.inc` Pattern

The decoder supports 8-bit samples (`uint8_t`, 1 byte/sample) and high-bit-depth samples (`uint16_t`, 2 bytes/sample, parameterized by `Av2hSampleDomain` for 10-bit and 12-bit dynamic ranges).

To avoid duplicating numerical algorithms while keeping 8-bit workspace memory compact (1 byte per pixel rather than widening all 8-bit planes to `uint16_t`), 17 internal modules use a **compile-time typed kernel instantiation pattern**:
- [`sample_kernel_u8.inc`](../src/decoder/internal/sample_kernel_u8.inc) defines:
  ```c
  #define AV2H_KERNEL_WIDE 0
  #define AV2H_KERNEL_TYPE(low, high) low
  #define AV2H_KERNEL_NAME(low, high) low
  ```
- [`sample_kernel_u16.inc`](../src/decoder/internal/sample_kernel_u16.inc) defines:
  ```c
  #define AV2H_KERNEL_WIDE 1
  #define AV2H_KERNEL_TYPE(low, high) high
  #define AV2H_KERNEL_NAME(low, high) high
  ```
- At the bottom of each owning `.c` or `.h` file, the shared `*_kernel.inc` body is included twice—once bracketed by `sample_kernel_u8.inc` / `sample_kernel_end.inc` and once by `sample_kernel_u16.inc` / `sample_kernel_end.inc`.
- All sample clipping, midpoint defaults, and bit-depth shifts inside high-bit-depth kernels are driven by `const Av2hSampleDomain *domain` ([`sample_math.h`](../src/decoder/internal/sample_math.h)).

---

## 9. Prediction and Block/Plane Reconstruction

When [`frame_output.c`](../src/decoder/frame_output.c) receives a decoded leaf from `Av2hTileDecoder`, it invokes [`reconstruct.c`](../src/decoder/reconstruct.c) (`av2h_luma_reconstruct_to_plane[_u16]` or `av2h_chroma_reconstruct_to_plane[_u16]`):

### 9.1 Reference Edge Collection & Readiness Maps
- Each plane tracks reconstructed 4×4 MI cells in `plane->reconstructed[]` (`0` = unreconstructed, `1` = reconstructed) and restricts neighbor access to the active tile's `[tile_x0, tile_x1) × [tile_y0, tile_y1)` bounds.
- [`reconstruct.c`](../src/decoder/reconstruct.c) gathers available `above` (including top-right extension) and `left` (including bottom-left extension) reference samples from `plane->data` into `scratch->above` and `scratch->left` (plus `secondary_above` / `secondary_left` for multi-line MRL).
- [`intra_prediction_edges.c`](../src/decoder/intra_prediction_edges.c) applies reference-edge upsampling, boundary extension, and intra edge smoothing filters when signaled.

### 9.2 Predictor Generation
Depending on the leaf's mode, the predictor buffer (`scratch->predictor`) is filled by one of five engines:
1. **Basic Intra ([`intra_prediction_basic.c`](../src/decoder/intra_prediction_basic.c)):** `DC` (average of available top/left edges), `SMOOTH`, `SMOOTH_V`, `SMOOTH_H` (quadratic interpolation tables), and `PAETH` (planar gradient selector).
2. **Directional Intra ([`intra_prediction_directional.c`](../src/decoder/intra_prediction_directional.c)):** Projects reference samples along the wide-angle-mapped directional ray (Zones 1, 2, and 3) using 4-tap Interpolation Directional Intra Filters (`dr_interp_filter`), optional Multi-Reference Line (`MRL`) offsets, and Intra Block Prediction (`IBP`) secondary-edge blending.
3. **Intra-DIP ([`intra_prediction_dip.c`](../src/decoder/intra_prediction_dip.c)):** Runs learned neural/matrix weights (`intra_dip_data.inc`) over neighboring reference lines for small luma blocks.
4. **Chroma from Luma — CfL & MHCCP ([`chroma_prediction_cfl.c`](../src/decoder/chroma_prediction_cfl.c), [`chroma_prediction_mhccp.c`](../src/decoder/chroma_prediction_mhccp.c)):**
   - **Explicit Alpha CfL:** Downsamples colocated reconstructed luma samples to Q3 precision, subtracts the block DC average, scales by the signaled `alpha_q3` parameter, and adds the chroma DC predictor.
   - **Derived Alpha CfL:** Fits a linear regression slope (`av2h_sample_regression_slope` in [`sample_math.c`](../src/decoder/sample_math.c)) between neighboring reconstructed luma and chroma boundary samples without transmitting alpha in the bitstream.
   - **Multi-Hypothesis Cross-Component Prediction (MHCCP):** Builds a $3\times 3$ normal-equation matrix over directional luma/chroma neighbor templates, solves for 3 cross-component filter weights via fixed-point Cramer/Gaussian elimination, and predicts the chroma block from colocated luma samples.
5. **Screen Content — Palette & IntraBC ([`reconstruct.c`](../src/decoder/reconstruct.c), [`intrabc_prediction.c`](../src/decoder/intrabc_prediction.c)):**
   - **Palette:** Maps decoded palette color indices directly to sample values in `scratch->predictor`.
   - **IntraBC:** Validates that the displaced source block inside the current tile is already marked `1` in `plane->reconstructed`, performs sub-pel interpolation (`av2h_intrabc_convolve`), and applies optional affine/morphological warping (`av2h_intrabc_morph`).

### 9.3 Cross-Component & Inverse Transforms ([`reconstruct_block.c`](../src/decoder/reconstruct_block.c), [`secondary_transform.c`](../src/decoder/secondary_transform.c), [`inverse_transform.c`](../src/decoder/inverse_transform.c))
1. **Chroma Cross-Transform (`CCTX`):** For joint U/V chroma leaves with `cctx_type > 0`, `av2h_chroma_inverse_cross_transform` rotates the dequantized U and V coefficient arrays using a 2×2 fixed-point orthogonal matrix before inverse transformation.
2. **Inverse Secondary Transform (`IST`):** When `secondary_symbol > 0`, [`secondary_transform.c`](../src/decoder/secondary_transform.c) gathers the low-frequency 8, 20, or 32 dequantized coefficients, multiplies them by the selected non-separable kernel (`ist_*_data.inc`), and scatters the transformed low-frequency coefficients back into the 2D coefficient block.
3. **Inverse Primary 2D Transform:**
   - **Lossless 4×4 (`q_index == 0`):** [`inverse_transform.c`](../src/decoder/inverse_transform.c) (`av2h_inverse_lossless_4x4`) applies the exact reversible 4×4 Walsh-Hadamard Transform (`iwht4`) or identity shift, followed by horizontal/vertical DPCM prefix-sum accumulation.
   - **Lossy Separable 2D (`DCT`, `ADST`, `FLIPADST`, `IDTX`):** Applies rectangular scaling ($\sqrt{2}$ factor `2896 >> 12` when aspect ratio is 1:2 or 1:8), runs 1D row transforms into `scratch->transposed`, and runs 1D column transforms into `scratch->residual`.
4. **Plane Commit:** Each residual sample is added to `scratch->predictor`, clipped to `Av2hSampleDomain` (`[0, 255]` or `[0, 1023]`/`[0, 4095]`), written to `workspace->coded[plane].data`, and the corresponding 4×4 cells in `plane->reconstructed` and `workspace->filter_workspace->deblock_map` are marked complete.

---

## 10. Five-Stage In-Loop Filter Pipeline ([`filter.c`](../src/decoder/filter.c))

Once all tiles in the frame are reconstructed and completeness checks pass (`finish_frame_tiles`), `filter_and_output_frame` runs up to five in-loop filter stages in strict specification order:

```text
Reconstructed Coded Planes
            |
            v
  +--------------------+
  | 1. Deblocking      |  (In-place on active deblocked plane buffer;
  |    (deblock.c)     |   serial across 4x4 transform edges)
  +--------------------+
            |
            +---------------------------------------+ (Immutable pre-CDEF
            |                                       |  deblocked reference
            v                                       |  rows retained for
  +--------------------+                            |  CCSO, Restoration,
  | 2. CDEF            |  (64-row parallel stripes) |  and GDF stripe edges)
  |    (cdef.c)        |<---------------------------+
  +--------------------+                            |
            |                                       |
            v                                       |
  +--------------------+                            |
  | 3. CCSO            |  (64-row parallel stripes) |
  |    (ccso.c)        |<---------------------------+
  +--------------------+                            |
            |                                       |
            v                                       |
  +--------------------+                            |
  | 4. Restoration     |  (64-row parallel stripes: |
  |    (restoration.c) |   PC-Wiener / Wiener-NS)  <+
  +--------------------+                            |
            |                                       |
            v                                       |
  +--------------------+                            |
  | 5. GDF             |  (64-row parallel stripes) |
  |    (gdf.c)         |<---------------------------+
  +--------------------+
            |
            v
  Visible Crop & Copy to Caller Output (frame_copy.c)
```

1. **Deblocking ([`deblock.c`](../src/decoder/deblock.c)):** Filters vertical then horizontal transform-block edges on a 4×4 sample grid using per-cell transform sizes, `q_index`, and `skip`/`lossless` flags recorded in `Av2hDeblockMap`. Chooses narrow/wide filter tap lengths (up to 16 samples) based on second-derivative flatness and directional error checks.
2. **Constrained Directional Enhancement Filter — CDEF ([`cdef.c`](../src/decoder/cdef.c)):** For each 8×8 block, searches 8 edge directions (`av2h_cdef_find_direction`) by maximizing directional variance, then applies non-linear primary (along direction) and secondary (45° off-direction) taps constrained by `cdef_damping` and per-64×64-unit strengths.
3. **Cross-Component Sample Offset — CCSO ([`ccso.c`](../src/decoder/ccso.c)):** Classifies each sample using colocated pre-CDEF luma band and 2-point edge classifiers (`ccso_class`), adding a signed offset from a 1024-entry frame-header lookup table (`frame->ccso[plane].filter_offset`).
4. **Loop Restoration ([`restoration.c`](../src/decoder/restoration.c)):** Operates on 64-row processing stripes (using pre-CDEF deblocked boundary rows at stripe edges):
   - **Pixel-Classified Wiener (`PC-Wiener`):** Classifies 4×4 luma blocks into 64 classes based on gradient features and transform-skip density, applying a 13-tap non-separable symmetric luma filter.
   - **Non-Separable Wiener (`Wiener-nonsep`):** Applies signaled per-unit or multi-class frame-level filter banks (up to 18 taps, including cross-component luma taps for chroma planes).
5. **Guided Directional Filter — GDF ([`gdf.c`](../src/decoder/gdf.c)):** For each 4×4 luma block enabled in `gdf_controls`, materializes a 16×16 neighborhood (switching to `deblocked` luma across 64-row stripe boundaries), computes 4 directional gradients and 2×2 sub-block orientation classes, evaluates a 22-input×3-output learned linear layer (`gdf_weight` + `gdf_bias`), and indexes a 4096-entry lookup table (`gdf_intra_error`) to add a scaled correction to each pixel.

**Active Stage Routing:** [`filter.c`](../src/decoder/filter.c) tracks active plane views so any disabled filter stage is bypassed without copying planes, while preserving the immutable pre-CDEF `deblocked` plane whenever downstream stages (`CCSO`, `Restoration`, `GDF`) need pre-CDEF stripe-boundary or luma-guide samples.

---

## 11. Parallel Execution Model ([`runtime/concurrency.c`](../src/runtime/concurrency.c), [`internal/parallel.h`](../src/decoder/internal/parallel.h))

Concurrency is optional and injected through the 3-field `Av2hExecution` struct (`context`, `parallel_for`, `width`). The core decoder never spawns threads itself.

When an `Av2hExecution` pool with `width > 1` is supplied:
1. **Multi-Tile Wave Parallelism ([`frame_output.c`](../src/decoder/frame_output.c)):**
   - When `tiles_parallel_safe` holds and the workspace was initialized with `execution->width > 1`, tiles are decoded in waves of up to `min(tile_count, execution->width, 16)` tiles.
   - Each batch slot uses its own `Av2hFrameTileWorker` (`tile_decoder`, entropy contexts, `reconstruction_scratch`) while writing directly into its tile's disjoint `[tile_x0, tile_x1) × [tile_y0, tile_y1)` region of `workspace->coded` and `deblock_map`.
   - After a wave joins, per-tile CDEF and CCSO control maps are merged in canonical tile order.
   - **Deterministic Failure Replay:** If any tile in a parallel wave returns an error, the coordinator resets the frame state and replays the tile sequence serially from tile 0 up to the first failing tile. This guarantees that error statuses, diagnostic fields, entropy tell positions, and partial buffer mutations are 100% identical between single-threaded and multi-threaded runs.
2. **Loop-Filter Stripe Parallelism ([`internal/filter_stage.h`](../src/decoder/internal/filter_stage.h)):**
   - CDEF, CCSO, PC-Wiener, Wiener-nonsep, and GDF partition the frame height into 64-luma-row stripes and dispatch them via `av2h_parallel_ranges`, joining at a barrier between each stage.
3. **Output Plane Copy Parallelism ([`frame_copy.c`](../src/decoder/frame_copy.c)):**
   - For planes $\ge 64\text{ KB}$, `copy_rows_dispatch` parallelizes the visible row crop/copy into the caller's packed output buffer in ~16 KB chunks.

---

## 12. Error Reporting and Diagnostics ([`error.h`](../src/error.h), [`internal/decode_diagnostics.h`](../src/decoder/internal/decode_diagnostics.h))

Error handling operates at two levels:
1. **Always-On Public `Av2hError` ([`include/av2heif.h`](../include/av2heif.h)):**
   Every public function populates `Av2hError` on failure with:
   - `status` (`AV2H_INVALID_ARGUMENT`, `AV2H_TRUNCATED`, `AV2H_INVALID_DATA`, `AV2H_OVERFLOW`, `AV2H_LIMIT_EXCEEDED`, `AV2H_OUT_OF_MEMORY`, `AV2H_UNSUPPORTED`),
   - `offset` (byte offset in the container or OBU stream),
   - `context` (`Av2hErrorContext`: box, sequence header, frame, tile, reconstruction, filter, etc.),
   - `diagnostic_stage`, `diagnostic_reason`, `syntax_stage`, `plane`, block `(row, column)`, `entropy_tell`, and `parsed_value`.
2. **Compile-Time Extended Diagnostics (`AV2HEIF_ENABLE_DIAGNOSTICS`):**
   When compiled with `-DAV2HEIF_ENABLE_DIAGNOSTICS=1`, `Av2hDecodeDiagnostics` records fine-grained stage transitions, range decoder intervals before/after coefficient prefixes, IntraBC candidate banks, and optional full partition/leaf decision traces formatted by [`src/shared/cli_diagnostics.c`](../src/shared/cli_diagnostics.c). When disabled, all diagnostic setter functions compile to inline no-ops with zero runtime overhead.
