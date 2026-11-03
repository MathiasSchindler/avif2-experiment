#include "decoder/internal/frame_workspace.h"
#include "decoder/tx_geometry.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static uint64_t hash_value(uint64_t hash, uint64_t value) {
    unsigned int byte;
    for (byte = 0U; byte < 8U; ++byte) {
        hash = (hash ^ (uint8_t)(value >> (byte * 8U))) *
            UINT64_C(1099511628211);
    }
    return hash;
}

static void initialize(Av2hSequenceState *sequence, Av2hFrameState *frame,
                       uint16_t width, uint16_t height, uint8_t format,
                       unsigned int bytes) {
    memset(sequence, 0, sizeof(*sequence));
    memset(frame, 0, sizeof(*frame));
    sequence->superblock_size = 64U;
    sequence->info.bit_depth_idc = bytes == 1U ? 1U : 0U;
    frame->width = width;
    frame->height = height;
    frame->chroma_format_idc = format;
    frame->plane_count = format == 1U ? 1U : 3U;
}

static void test_storage_offsets(void) {
    static _Alignas(max_align_t) uint8_t memory[2U * 1024U * 1024U];
    /* Pointer offsets measured before M4; absolute addresses are excluded. */
    static const uint64_t expected[6] = {
        UINT64_C(0x09c68eb3b138720f), UINT64_C(0x39f5479409609aad),
        UINT64_C(0x686b478e6b9736e5), UINT64_C(0x52fd92ced2bb7d99),
        UINT64_C(0xe7dc32915fe9b3a4), UINT64_C(0xd6ccb38278c49ae9)
    };
    unsigned int format;
    unsigned int bytes;
    for (format = 0U; format < 3U; ++format) {
        for (bytes = 1U; bytes <= 2U; ++bytes) {
            uint64_t hash = UINT64_C(14695981039346656037);
            unsigned int shape;
            unsigned int superblock;
            unsigned int restoration;
            for (shape = 0U; shape < 2U; ++shape) {
                for (superblock = 64U; superblock <= 128U; superblock *= 2U) {
                    for (restoration = 0U; restoration < 2U; ++restoration) {
                        Av2hSequenceState sequence;
                        Av2hFrameState frame;
                        Av2hFrameDecodeWorkspaceLayout layout;
                        Av2hFrameDecodeWorkspace workspace;
                        Av2hFilterWorkspace filter;
                        Av2hFilterWorkspaceStorage storage;
                        unsigned int plane;
                        initialize(&sequence, &frame, shape ? 128U : 17U,
                                   shape ? 65U : 15U, (uint8_t)format, bytes);
                        sequence.superblock_size = (uint16_t)superblock;
                        frame.gdf_block_count = 3U;
                        if (restoration) {
                            frame.restoration_type[0] = AV2H_RESTORE_PC_WIENER;
                            frame.restoration_unit_count[0] = 3U;
                        }
                        CHECK(av2h_frame_decode_workspace_query(
                                  &sequence, &frame, bytes, &layout) == AV2H_OK);
                        CHECK(layout.size_bytes < sizeof(memory));
                        if (layout.size_bytes >= sizeof(memory)) return;
                        memset(memory, 0xa5, sizeof(memory));
                        CHECK(av2h_frame_decode_workspace_init(
                                  &sequence, &frame, bytes, &layout, memory,
                                  layout.size_bytes, &workspace, &filter,
                                  &storage) == AV2H_OK);
                        CHECK(memory[layout.size_bytes] == 0xa5U);
                        hash = hash_value(hash, layout.size_bytes);
                        hash = hash_value(hash, layout.alignment);
#define POINTER(pointer)                                                       \
    hash = hash_value(hash, (pointer) == 0 ? UINT64_MAX :                       \
        (uint64_t)((const uint8_t *)(pointer) - memory))
                        for (plane = 0U; plane < 3U; ++plane) {
                            POINTER(workspace.coded[plane].data);
                            POINTER(workspace.coded[plane].reconstructed);
                            POINTER(workspace.coded_u16[plane].data);
                            POINTER(workspace.coded_u16[plane].reconstructed);
                            POINTER(filter.deblocked.plane[plane].data);
                            POINTER(filter.cdef.plane[plane].data);
                            POINTER(filter.deblocked_u16.plane[plane].data);
                            POINTER(filter.cdef_u16.plane[plane].data);
                            POINTER(storage.deblock_cells[plane]);
                            POINTER(storage.ccso_controls[plane]);
                            POINTER(workspace.tile_context.ccso_unit_controls[plane]);
                            hash = hash_value(hash, workspace.coded_data_size_bytes[plane]);
                        }
                        POINTER(filter.restoration.data);
                        POINTER(filter.restoration_tx_skip);
                        POINTER(workspace.reconstruction_scratch);
                        POINTER(workspace.reconstruction_scratch_u16);
                        POINTER(workspace.tile_context.luma_partition_above);
                        POINTER(workspace.tile_context.luma_partition_left);
                        POINTER(workspace.tile_context.chroma_partition_above);
                        POINTER(workspace.tile_context.chroma_partition_left);
                        POINTER(workspace.tile_context.luma_mode_above);
                        POINTER(workspace.tile_context.luma_mode_left);
                        POINTER(workspace.tile_context.coefficient_above);
                        POINTER(workspace.tile_context.coefficient_left);
                        POINTER(workspace.tile_context.colocated_luma);
                        POINTER(workspace.tile_context.gdf_block_flags);
                        POINTER(workspace.tile_context.cdef_unit_strengths);
                        POINTER(workspace.tile_context.restoration_unit_flags);
                        POINTER(workspace.tile_context.restoration_unit_taps);
#undef POINTER
                    }
                }
            }
            printf("storage %u/%u: %016" PRIx64 "\n", format, bytes, hash);
            if (sizeof(void *) == 8U && sizeof(size_t) == 8U) {
                CHECK(hash == expected[format * 2U + bytes - 1U]);
            }
        }
    }
}

typedef struct {
    size_t calls;
    size_t counts[3];
    size_t chunks[3];
    unsigned int fail;
} Dispatch;

static int dispatch(void *context, size_t count, size_t min_chunk,
                    Av2hParallelBody body, void *argument) {
    Dispatch *state = context;
    size_t end = count;
    CHECK(state->calls < 3U);
    if (state->calls >= 3U) return 1;
    state->counts[state->calls] = count;
    state->chunks[state->calls++] = min_chunk;
    if (state->fail == 1U) return 1;
    while (end != 0U) {
        size_t begin = end > min_chunk ? end - min_chunk : 0U;
        if (body(begin, end, 2U, argument) != 0) return 1;
        if (state->fail == 2U) return 1;
        end = begin;
    }
    return 0;
}

static void test_copy(unsigned int bytes, uint8_t format, uint16_t width,
                      uint16_t height, unsigned int execution_kind) {
    enum { SAMPLES = 272 * 272, OUTPUT = 3 * SAMPLES * 2 + 16 };
    static uint8_t source[3][SAMPLES];
    static uint16_t source_u16[3][SAMPLES];
    static uint8_t output[OUTPUT];
    static uint8_t expected[OUTPUT];
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout layout;
    Av2hFilterPlanes coded = { 0 };
    Av2hReconstructionPlaneU16 coded_u16[3] = { 0 };
    size_t capacities[3] = { 0 };
    Dispatch state = { 0 };
    Av2hExecution execution = { &state, dispatch, 4U };
    size_t required;
    size_t offset = 0U;
    size_t expected_calls = 0U;
    unsigned int plane;
    initialize(&sequence, &frame, width, height, format, bytes);
    CHECK(av2h_filter_workspace_query(&frame, bytes, &layout) == AV2H_OK);
    CHECK(av2h_planar_output_size(&frame, bytes, &required) == AV2H_OK);
    CHECK(required + 16U < sizeof(output));
    if (required + 16U >= sizeof(output)) return;
    memset(output, 0xa5, sizeof(output));
    memset(expected, 0xa5, sizeof(expected));
    if (execution_kind == 1U) execution.parallel_for = 0;
    if (execution_kind == 2U) execution.width = 1U;
    if (execution_kind >= 4U) state.fail = execution_kind - 3U;
    for (plane = 0U; plane < av2h_frame_plane_count(&frame); ++plane) {
        size_t stride = layout.plane_width[plane] + 3U;
        size_t row;
        size_t column;
        CHECK(stride * layout.plane_height[plane] <= SAMPLES);
        coded.plane[plane] = (Av2hFilterPlane){
            source[plane], stride, layout.plane_width[plane],
            layout.plane_height[plane]
        };
        coded_u16[plane] = (Av2hReconstructionPlaneU16){
            source_u16[plane], stride, layout.plane_width[plane],
            layout.plane_height[plane], 0U, 0U, 0, 0U, 0U
        };
        capacities[plane] = stride * layout.plane_height[plane] * bytes;
        for (row = 0U; row < layout.plane_height[plane]; ++row) {
            for (column = 0U; column < stride; ++column) {
                uint16_t value = (uint16_t)(row * 257U + column * 37U + plane * 53U);
                source[plane][row * stride + column] = (uint8_t)value;
                source_u16[plane][row * stride + column] = value;
            }
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(&frame); ++plane) {
        size_t stride = coded.plane[plane].stride_samples;
        size_t w = av2h_frame_plane_dimension(&frame, plane, frame.width);
        size_t h = av2h_frame_plane_dimension(&frame, plane, frame.height);
        size_t row;
        size_t column;
        if (execution_kind >= 3U && h > 1U && w * h * bytes >= 65536U) {
            size_t chunk = 16384U / (w * bytes);
            if (chunk == 0U) chunk = 1U;
            CHECK(expected_calls < 3U);
            ++expected_calls;
            if (state.fail == 1U) break;
            if (state.fail == 2U) {
                for (row = h > chunk ? h - chunk : 0U; row < h; ++row) {
                    for (column = 0U; column < w; ++column) {
                        uint16_t value = source_u16[plane][row * stride + column];
                        expected[offset + (row * w + column) * bytes] = (uint8_t)value;
                        if (bytes == 2U) expected[offset + (row * w + column) * bytes + 1U] =
                            (uint8_t)(value >> 8U);
                    }
                }
                break;
            }
        }
        for (row = 0U; row < h; ++row) {
            for (column = 0U; column < w; ++column) {
                uint16_t value = source_u16[plane][row * stride + column];
                expected[offset++] = (uint8_t)value;
                if (bytes == 2U) expected[offset++] = (uint8_t)(value >> 8U);
            }
        }
    }
    {
        Av2hStatus status = bytes == 1U ?
            av2h_copy_visible_planar_private(
                &frame, &coded, capacities, output, required + 16U,
                execution_kind == 0U ? 0 : &execution) :
            av2h_copy_visible_planar_u16_private(
                &frame, coded_u16, capacities, output, required + 16U,
                execution_kind == 0U ? 0 : &execution);
        CHECK(status == (state.fail && expected_calls ?
              AV2H_INVALID_DATA : AV2H_OK));
    }
    CHECK(state.calls == expected_calls);
    CHECK(memcmp(output, expected, sizeof(output)) == 0);
    {
        size_t call = 0U;
        for (plane = 0U; plane < av2h_frame_plane_count(&frame); ++plane) {
            size_t w = av2h_frame_plane_dimension(&frame, plane, frame.width);
            size_t h = av2h_frame_plane_dimension(&frame, plane, frame.height);
            if (execution_kind >= 3U && h > 1U && w * h * bytes >= 65536U) {
                size_t chunk = 16384U / (w * bytes);
                CHECK(state.counts[call] == h);
                CHECK(state.chunks[call] == (chunk ? chunk : 1U));
                ++call;
                if (state.fail) break;
            }
        }
    }
    memset(output, 0xa5, sizeof(output));
    CHECK((bytes == 1U ? av2h_copy_visible_planar_private(
        &frame, &coded, capacities, output, required - 1U, NULL) :
          av2h_copy_visible_planar_u16_private(
              &frame, coded_u16, capacities, output, required - 1U, NULL)) ==
          AV2H_OUT_OF_MEMORY);
    if (!state.fail) {
        unsigned int last = av2h_frame_plane_count(&frame) - 1U;
        --capacities[last];
        CHECK((bytes == 1U ? av2h_copy_visible_planar_private(
            &frame, &coded, capacities, output, sizeof(output), NULL) :
              av2h_copy_visible_planar_u16_private(
                  &frame, coded_u16, capacities, output, sizeof(output), NULL)) ==
              AV2H_INVALID_ARGUMENT);
        ++capacities[last];
        CHECK((bytes == 1U ? av2h_copy_visible_planar_private(
            &frame, &coded, capacities, source[0], required, NULL) :
              av2h_copy_visible_planar_u16_private(
                  &frame, coded_u16, capacities, (uint8_t *)source_u16[0],
                  required, NULL)) == AV2H_INVALID_ARGUMENT);
    }
    for (offset = 0U; offset < sizeof(output); ++offset) CHECK(output[offset] == 0xa5U);
}

static uint64_t hash_result(uint64_t hash, const Av2hFrameDecodeResult *result) {
#define FIELD(name) hash = hash_value(hash, (uint64_t)result->name)
    FIELD(luma_leaf_count); FIELD(chroma_leaf_count); FIELD(entropy_tell);
    FIELD(cause); FIELD(stage); FIELD(row); FIELD(column); FIELD(block_size);
    FIELD(plane); FIELD(syntax_stage); FIELD(prefix_stage); FIELD(txb_skip_context);
    FIELD(all_zero); FIELD(eob_point); FIELD(mode); FIELD(angle_delta);
    FIELD(mrl_index); FIELD(multi_line_mrl); FIELD(enable_ibp); FIELD(use_intrabc);
    FIELD(displacement_x); FIELD(displacement_y); FIELD(morph_pred); FIELD(fsc_mode);
    FIELD(skip_transform); FIELD(palette_size); FIELD(tx_size);
    FIELD(inverse_transform_stage); FIELD(intrabc_source_x); FIELD(intrabc_source_y);
    FIELD(intrabc_map_index); FIELD(primary_type); FIELD(primary_symbol);
    FIELD(primary_set_symbol); FIELD(secondary_symbol); FIELD(secondary_set);
    FIELD(cctx_type); FIELD(is_cfl); FIELD(cfl_kind); FIELD(mhccp_direction);
    FIELD(cfl_filter_index); FIELD(mhccp_reason); FIELD(chroma_parse_reason);
    FIELD(chroma_mode_index); FIELD(filter_reason); FIELD(eob);
    FIELD(u_coefficient_count); FIELD(v_coefficient_count); FIELD(after_all_zero_tell);
    FIELD(after_eob_point_tell); FIELD(eob_range); FIELD(skip_range);
    FIELD(before_prefix_range); FIELD(eob_tx_size); FIELD(eob_class);
    FIELD(coefficient_position); FIELD(coefficient_context); FIELD(coefficient_kind);
#undef FIELD
    return hash_value(hash, result->diagnostics != 0);
}

static void test_deblock_metadata(void) {
    uint8_t format;
    for (format = 0U; format < 3U; ++format) {
        unsigned int plane;
        for (plane = 0U; plane < (format == 1U ? 1U : 3U); ++plane) {
            unsigned int tx;
            for (tx = 0U; tx < AV2H_TX_SIZE_COUNT; ++tx) {
                unsigned int edge;
                for (edge = 0U; edge < 2U; ++edge) {
                    unsigned int flags;
                    for (flags = 0U; flags < 8U; ++flags) {
                        Av2hDeblockCell cells[8U * 5U] = { { 0 } };
                        Av2hDeblockMap map = { 0 };
                        Av2hPartitionNode leaf = { 0 };
                        Av2hCoeffPrefix prefix = { 0 };
                        const Av2hTxDescriptor *descriptor;
                        size_t columns;
                        size_t rows;
                        size_t index;
                        size_t grid = 4U >> av2h_chroma_shift(format, plane);
                        leaf.column = edge ? 5U : 0U;
                        leaf.row = edge ? 4U : 0U;
                        leaf.block_size = AV2H_BLOCK_64X64;
                        prefix.tx_size = (Av2hTxSize)tx;
                        prefix.q_index = 300U;
                        map.plane[plane] = cells;
                        map.stride_cells[plane] = 8U;
                        map.columns[plane] = 6U;
                        map.rows[plane] = 5U;
                        CHECK(av2h_tx_descriptor_get(prefix.tx_size, &descriptor) ==
                              AV2H_OK);
                        columns = descriptor->output_width / grid;
                        rows = descriptor->output_height / grid;
                        if (columns > 16U) columns = 16U;
                        if (rows > 16U) rows = 16U;
                        if (columns > 6U - leaf.column) columns = 6U - leaf.column;
                        if (rows > 5U - leaf.row) rows = 5U - leaf.row;
                        CHECK(av2h_deblock_map_commit_transform(
                                  &map, format, plane, &leaf, &prefix,
                                  (uint8_t)(flags & 1U),
                                  (uint8_t)((flags >> 1U) & 1U),
                                  (uint8_t)((flags >> 2U) & 1U)) == AV2H_OK);
                        for (index = 0U; index < 40U; ++index) {
                            unsigned int inside = index / 8U >= leaf.row &&
                                index / 8U < leaf.row + rows &&
                                index % 8U >= leaf.column &&
                                index % 8U < leaf.column + columns;
#define CELL(field, value) CHECK(cells[index].field == (inside ? (value) : 0U))
                            CELL(block_column, leaf.column);
                            CELL(block_row, leaf.row);
                            CELL(transform_column, leaf.column);
                            CELL(transform_row, leaf.row);
                            CELL(transform_width, descriptor->output_width);
                            CELL(transform_height, descriptor->output_height);
                            CELL(q_index, 300U);
                            CELL(skip, (flags & 3U) != 0U);
                            CELL(lossless, (flags >> 2U) & 1U);
#undef CELL
                        }
                    }
                }
            }
        }
    }
    {
        Av2hDeblockCell cells[8U * 5U] = { { 0 } };
        Av2hDeblockMap map = { 0 };
        Av2hPartitionNode leaf = { 0 };
        Av2hCoeffPrefix prefix = { 0 };
        map.plane[0] = cells;
        map.stride_cells[0] = 8U;
        map.columns[0] = 6U;
        map.rows[0] = 5U;
        leaf.block_size = AV2H_BLOCK_64X64;
        prefix.tx_size = AV2H_TX_8X8;
        cells[1].transform_width = 1U;
        CHECK(av2h_deblock_map_commit_transform(
                  &map, 0U, 0U, &leaf, &prefix, 0U, 0U, 0U) ==
              AV2H_INVALID_DATA);
        CHECK(cells[0].transform_width == 8U);
        CHECK(cells[1].transform_width == 1U);
        CHECK(cells[8].transform_width == 0U);
    }
}

static void test_restoration_metadata(void) {
    unsigned int tx;
    for (tx = 0U; tx < AV2H_TX_SIZE_COUNT; ++tx) {
        unsigned int edge;
        for (edge = 0U; edge < 2U; ++edge) {
            unsigned int zero;
            for (zero = 0U; zero < 3U; ++zero) {
                uint8_t cells[8U * 5U];
                Av2hFilterWorkspace workspace = { 0 };
                Av2hPartitionNode leaf = { 0 };
                Av2hCoeffPrefix prefix = { 0 };
                const Av2hTxDescriptor *descriptor;
                size_t columns;
                size_t rows;
                size_t index;
                memset(cells, 0xa5, sizeof(cells));
                workspace.restoration_tx_skip = cells;
                workspace.restoration_controls.tx_skip_stride = 8U;
                workspace.deblock_map.columns[0] = 6U;
                workspace.deblock_map.rows[0] = 5U;
                leaf.column = edge ? 5U : 0U;
                leaf.row = edge ? 4U : 0U;
                prefix.tx_size = (Av2hTxSize)tx;
                prefix.all_zero = (uint8_t)zero;
                CHECK(av2h_tx_descriptor_get(prefix.tx_size, &descriptor) ==
                      AV2H_OK);
                columns = descriptor->output_width / 4U;
                rows = descriptor->output_height / 4U;
                if (columns > 6U - leaf.column) columns = 6U - leaf.column;
                if (rows > 5U - leaf.row) rows = 5U - leaf.row;
                CHECK(av2h_restoration_tx_skip_commit(
                          &workspace, &leaf, &prefix) == AV2H_OK);
                for (index = 0U; index < sizeof(cells); ++index) {
                    unsigned int inside = index / 8U >= leaf.row &&
                        index / 8U < leaf.row + rows &&
                        index % 8U >= leaf.column &&
                        index % 8U < leaf.column + columns;
                    CHECK(cells[index] == (inside ? zero : 0xa5U));
                }
                memset(cells, 0xa5, sizeof(cells));
                leaf.column = 6U;
                CHECK(av2h_restoration_tx_skip_commit(
                          &workspace, &leaf, &prefix) == AV2H_INVALID_ARGUMENT);
                leaf.column = 0U;
                workspace.restoration_controls.tx_skip_stride = 0U;
                CHECK(av2h_restoration_tx_skip_commit(
                          &workspace, &leaf, &prefix) == AV2H_INVALID_ARGUMENT);
                for (index = 0U; index < sizeof(cells); ++index) {
                    CHECK(cells[index] == 0xa5U);
                }
            }
        }
    }
}

static void test_frame_results(void) {
    static uint8_t input[16384U];
    static uint8_t output[96U * 80U * 3U];
    static _Alignas(max_align_t) uint8_t memory[2U * 1024U * 1024U];
    Av2hObu entries[4];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hFrameDecodeWorkspaceLayout layout;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter;
    Av2hFilterWorkspaceStorage storage;
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t stages[AV2H_FRAME_DECODE_STAGE_OUTPUT + 1U] = { 0 };
    FILE *file = fopen("tests/fixtures/i444-high-detail.obu", "rb");
    size_t size;
    size_t index;
    unsigned int variant;
    CHECK(file != 0);
    if (file == 0) return;
    size = fread(input, 1U, sizeof(input), file);
    CHECK(!ferror(file) && feof(file));
    CHECK(fclose(file) == 0);
    CHECK(av2h_obu_parse((Av2hSpan){ input, size }, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) sequence_obu = &entries[index];
        if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) frame_obu = &entries[index];
    }
    CHECK(sequence_obu != 0 && frame_obu != 0);
    if (sequence_obu == 0 || frame_obu == 0) return;
    CHECK(av2h_sequence_state_parse((Av2hSpan){ input, size }, sequence_obu,
                                   &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset, frame_obu->payload_size },
        &sequence, &frame, NULL) == AV2H_OK);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ input + frame_obu->payload_offset, frame_obu->payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(group.entropy.size >= 8U);
    CHECK(av2h_frame_decode_workspace_query(
        &sequence, &frame, 1U, &layout) == AV2H_OK);
    CHECK(layout.size_bytes <= sizeof(memory));
    if (group.entropy.size < 8U || layout.size_bytes > sizeof(memory)) return;
    CHECK(av2h_frame_decode_workspace_init(
        &sequence, &frame, 1U, &layout, memory, layout.size_bytes, &workspace,
        &filter, &storage) == AV2H_OK);
    for (variant = 0U; variant <= 64U; ++variant) {
        Av2hFrameDecodeResult result = { 0 };
        size_t byte = variant ? (variant - 1U) / 8U : 0U;
        uint8_t bit = variant ? (uint8_t)(1U << ((variant - 1U) % 8U)) : 0U;
        size_t entropy_offset = (size_t)(group.entropy.data - input);
        Av2hStatus status;
        input[entropy_offset + byte] ^= bit;
        memset(output, 0xa5, sizeof(output));
        status = av2h_decode_frame_planar_private(
            group.entropy, &sequence, &frame, &workspace, output, sizeof(output),
            &result, NULL);
        input[entropy_offset + byte] ^= bit;
        CHECK(result.stage <= AV2H_FRAME_DECODE_STAGE_OUTPUT);
        if (result.stage <= AV2H_FRAME_DECODE_STAGE_OUTPUT) ++stages[result.stage];
        hash = hash_value(hash, status);
        hash = hash_result(hash, &result);
        for (index = 0U; index < sizeof(output); ++index) {
            if (status != AV2H_OK) CHECK(output[index] == 0xa5U);
            hash = hash_value(hash, output[index]);
        }
    }
    printf("frame results: %016" PRIx64 "; stages", hash);
    for (index = 0U; index < sizeof(stages) / sizeof(stages[0]); ++index) {
        if (stages[index]) printf(" %zu=%zu", index, stages[index]);
    }
    puts("");
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    CHECK(hash == UINT64_C(0x89fa7c2c44953ee3));
#else
    CHECK(hash == UINT64_C(0xa33580bc4d759373));
#endif
}

int main(void) {
    unsigned int bytes;
    uint8_t format;
    unsigned int kind;
    test_storage_offsets();
    test_deblock_metadata();
    test_restoration_metadata();
    for (bytes = 1U; bytes <= 2U; ++bytes) {
        for (format = 0U; format < 3U; ++format) {
            for (kind = 0U; kind < 6U; ++kind) {
                test_copy(bytes, format, 17U, 15U, kind);
                test_copy(bytes, format, (uint16_t)(256U / bytes), 255U, kind);
                test_copy(bytes, format, (uint16_t)(256U / bytes), 256U, kind);
                test_copy(bytes, format, (uint16_t)(256U / bytes + 1U), 256U, kind);
            }
            for (kind = 0U; kind < 6U; ++kind) test_copy(2U, 1U, 8193U, 4U, kind);
        }
    }
    test_frame_results();
    return failures != 0;
}
