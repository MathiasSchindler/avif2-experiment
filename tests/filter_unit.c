#include "decoder/frame_output.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/frame_workspace.h"

#include <stddef.h>
#include <stdio.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static int buffer_is_value(const uint8_t *data,
                           size_t size,
                           uint8_t value) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (data[index] != value) return 0;
    }
    return 1;
}

static void initialize_planes(Av2hFilterPlanes *planes,
                              uint8_t *y,
                              uint8_t *u,
                              uint8_t *v) {
    planes->plane[0] = (Av2hFilterPlane){ y, 8U, 8U, 8U };
    planes->plane[1] = (Av2hFilterPlane){ u, 4U, 4U, 4U };
    planes->plane[2] = (Av2hFilterPlane){ v, 4U, 4U, 4U };
}

static void populate_workspace_maps(
    const Av2hFilterWorkspaceLayout *layout,
    Av2hFilterWorkspaceStorage *storage,
    Av2hQIndex q_index) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        size_t row;
        for (row = 0U; row < layout->deblock_rows[plane]; ++row) {
            size_t column;
            for (column = 0U;
                 column < layout->deblock_columns[plane]; ++column) {
                storage->deblock_cells[plane][
                    row * layout->deblock_columns[plane] + column] =
                    (Av2hDeblockCell){
                        (uint16_t)column,
                        (uint16_t)row,
                        (uint16_t)column,
                        (uint16_t)row,
                        4U,
                        4U,
                        q_index,
                        0U,
                        0U
                    };
            }
        }
    }
}

static void test_wide_q_index_handoff(void) {
    Av2hDeblockCell cell;
    Av2hDeblockMap map;
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_4X4 };
    Av2hFirstCoeffPrefix prefix;
    av2h_memory_fill(&cell, 0U, sizeof(cell));
    av2h_memory_fill(&map, 0U, sizeof(map));
    av2h_memory_fill(&prefix, 0U, sizeof(prefix));
    map.plane[0] = &cell;
    map.stride_cells[0] = 1U;
    map.columns[0] = 1U;
    map.rows[0] = 1U;
    prefix.tx_size = AV2H_TX_4X4;
    prefix.q_index = 400U;
    CHECK(sizeof(Av2hQIndex) == 2U);
    CHECK(prefix.q_index == 400U);
    CHECK(av2h_deblock_map_commit_transform(
              &map, 0U, 0U, &leaf, &prefix, 0U, 0U, 0U) == AV2H_OK);
    CHECK(cell.q_index == 400U);
}

static void test_quantizer_threshold_domains(void) {
    Av2hDeblockCell cell = { 0 };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    uint16_t q_threshold = 0U;
    uint16_t side_threshold = 0U;
    sequence.info.bit_depth_idc = 1U;
    cell.q_index = 255U;
    CHECK(av2h_deblock_quantizer_thresholds(
              &cell, &sequence, &frame, 0U, 0U,
              &q_threshold, &side_threshold) == AV2H_OK);
    CHECK(q_threshold == 120U && side_threshold == 40U);
    sequence.info.bit_depth_idc = 0U;
    cell.q_index = 256U;
    CHECK(av2h_deblock_quantizer_thresholds(
              &cell, &sequence, &frame, 0U, 0U,
              &q_threshold, &side_threshold) == AV2H_OK);
    CHECK(q_threshold == 124U && side_threshold == 102U);
    cell.q_index = 511U;
    CHECK(av2h_deblock_quantizer_thresholds(
              &cell, &sequence, &frame, 0U, 0U,
              &q_threshold, &side_threshold) == AV2H_OK);
    CHECK(q_threshold == 480U && side_threshold == 160U);
    cell.q_index = 1U;
    frame.deblock_delta[0] = -1;
    CHECK(av2h_deblock_quantizer_thresholds(
              &cell, &sequence, &frame, 0U, 0U,
              &q_threshold, &side_threshold) == AV2H_OK);
    CHECK(q_threshold == 0U && side_threshold == 0U);
    CHECK(av2h_deblock_quantizer_thresholds(
              0, &sequence, &frame, 0U, 0U,
              &q_threshold, &side_threshold) == AV2H_INVALID_ARGUMENT);
}

static void copy_test_planes(Av2hFilterPlanes *destination,
                             const Av2hFilterPlanes *source) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        uint16_t row;
        for (row = 0U; row < source->plane[plane].height; ++row) {
            av2h_memory_copy(
                destination->plane[plane].data +
                    (size_t)row * destination->plane[plane].stride_samples,
                source->plane[plane].data +
                    (size_t)row * source->plane[plane].stride_samples,
                source->plane[plane].width);
        }
    }
}

static void test_workspace_model(void) {
    _Alignas(16) uint8_t memory[4096];
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout layout;
    Av2hFilterWorkspace workspace;
    Av2hFilterWorkspaceStorage storage;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    frame.width = 17U;
    frame.height = 15U;
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    CHECK(layout.plane_width[0] == 24U);
    CHECK(layout.plane_height[0] == 16U);
    CHECK(layout.plane_width[1] == 12U);
    CHECK(layout.plane_height[1] == 8U);
    CHECK(layout.stage_plane_bytes[0] == 384U);
    CHECK(layout.stage_plane_bytes[1] == 96U);
    CHECK(layout.stage_plane_bytes[2] == 96U);
    CHECK(layout.deblock_cell_count[0] == 24U);
    CHECK(layout.deblock_cell_count[1] == 24U);
    CHECK(layout.deblock_cell_count[2] == 24U);
    CHECK(layout.ccso_columns == 1U);
    CHECK(layout.ccso_rows == 1U);
    CHECK(layout.ccso_control_count == 1U);
    CHECK(layout.alignment == _Alignof(Av2hDeblockCell));
    CHECK(layout.size_bytes <= sizeof(memory));
    CHECK(av2h_filter_workspace_init(
              &frame, memory + 1U, sizeof(memory) - 1U,
              &workspace, &storage) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_filter_workspace_init(
              &frame, memory, layout.size_bytes - 1U,
              &workspace, &storage) == AV2H_OUT_OF_MEMORY);
    CHECK(av2h_filter_workspace_init(
              &frame, memory, sizeof(memory),
              &workspace, &storage) == AV2H_OK);
    CHECK(workspace.deblocked.plane[0].width == 24U);
    CHECK(workspace.cdef.plane[2].height == 8U);
    CHECK(workspace.deblock_map.columns[0] == 6U);
    CHECK(workspace.deblock_map.rows[0] == 4U);
    CHECK(workspace.ccso_controls.columns == 1U);
    CHECK(storage.deblock_cells[0] != 0);
    CHECK(storage.ccso_controls[2] != 0);
    CHECK(storage.deblock_cells[0][0].transform_width == 0U);
    CHECK(storage.ccso_controls[2][0] == 0U);

    frame.width = 160U;
    frame.height = 120U;
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    CHECK(layout.deblock_cell_count[0] == 1200U);
    CHECK(layout.deblock_cell_count[1] == 1200U);
    CHECK(layout.deblock_cell_count[2] == 1200U);
    CHECK(layout.ccso_columns == 3U);
    CHECK(layout.ccso_rows == 2U);
    CHECK(layout.ccso_control_count == 6U);
    frame.ccso_unit_size = 256U;
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    CHECK(layout.ccso_columns == 1U);
    CHECK(layout.ccso_rows == 1U);
    CHECK(layout.ccso_control_count == 1U);
    frame.ccso_unit_size = 63U;
    CHECK(av2h_filter_workspace_query(&frame, &layout) ==
          AV2H_INVALID_ARGUMENT);

    frame.width = 0U;
    CHECK(av2h_filter_workspace_query(&frame, &layout) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_frame_decode_workspace_model(void) {
    static _Alignas(max_align_t) uint8_t memory[4U * 1024U * 1024U];
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFrameDecodeWorkspaceLayout layout;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    sequence.info.bit_depth_idc = 1U;
    sequence.superblock_size = 64U;
    frame.width = 17U;
    frame.height = 15U;
    CHECK(av2h_frame_decode_workspace_query(
              &sequence, &frame, &layout) == AV2H_OK);
    CHECK(layout.size_bytes != 0U);
    CHECK(layout.alignment != 0U);
    CHECK(layout.size_bytes <= sizeof(memory));
    CHECK(av2h_frame_decode_workspace_init(
              &sequence, &frame, &layout, memory, layout.size_bytes,
              &workspace, &filter_workspace, &filter_storage) == AV2H_OK);

#define CHECK_FRAME_WORKSPACE_FAILURE(expected, pointer, size)                 \
    do {                                                                       \
        av2h_memory_fill(&workspace, 0xa5U, sizeof(workspace));                \
        av2h_memory_fill(                                                       \
            &filter_workspace, 0xa5U, sizeof(filter_workspace));              \
        av2h_memory_fill(&filter_storage, 0xa5U, sizeof(filter_storage));      \
        CHECK(av2h_frame_decode_workspace_init(                                \
                  &sequence, &frame, &layout, (pointer), (size),              \
                  &workspace, &filter_workspace, &filter_storage) ==          \
              (expected));                                                     \
        CHECK(buffer_is_value(                                                  \
            (const uint8_t *)&workspace, sizeof(workspace), 0xa5U));          \
        CHECK(buffer_is_value(                                                  \
            (const uint8_t *)&filter_workspace,                               \
            sizeof(filter_workspace), 0xa5U));                                \
        CHECK(buffer_is_value(                                                  \
            (const uint8_t *)&filter_storage,                                 \
            sizeof(filter_storage), 0xa5U));                                  \
    } while (0)
    CHECK_FRAME_WORKSPACE_FAILURE(
        AV2H_OUT_OF_MEMORY, memory, layout.size_bytes - 1U);
    CHECK_FRAME_WORKSPACE_FAILURE(
        AV2H_INVALID_ARGUMENT, memory + 1U, layout.size_bytes);
    frame.width = 33U;
    CHECK_FRAME_WORKSPACE_FAILURE(
        AV2H_INVALID_ARGUMENT, memory, layout.size_bytes);
#undef CHECK_FRAME_WORKSPACE_FAILURE
}

static void test_frame_workspace_prepare_rejection(void) {
    static _Alignas(max_align_t) uint8_t memory[4U * 1024U * 1024U];
    uint8_t output[512U];
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFrameDecodeWorkspaceLayout decode_layout;
    Av2hFilterWorkspaceLayout filter_layout;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeResult result;
    size_t output_size = 0U;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&result, 0U, sizeof(result));
    sequence.info.bit_depth_idc = 1U;
    sequence.superblock_size = 64U;
    frame.width = 17U;
    frame.height = 15U;
    CHECK(av2h_frame_decode_workspace_query(
              &sequence, &frame, &decode_layout) == AV2H_OK);
    CHECK(decode_layout.size_bytes <= sizeof(memory));
    CHECK(av2h_frame_decode_workspace_init(
              &sequence, &frame, &decode_layout, memory,
              decode_layout.size_bytes, &workspace, &filter_workspace,
              &filter_storage) == AV2H_OK);
    CHECK(av2h_planar_output_size(&frame, &output_size) == AV2H_OK);
    CHECK(output_size <= sizeof(output));

#define CHECK_PREPARE_FAILURE(pointer, result_pointer)                        \
    do {                                                                       \
        av2h_memory_fill(memory, 0xa5U, decode_layout.size_bytes);                  \
        CHECK(av2h_frame_workspace_prepare(                                   \
              &sequence, &frame, &workspace, (uint8_t *)(pointer),        \
              output_size,                                                \
                  (result_pointer), &filter_layout) ==                        \
              AV2H_INVALID_ARGUMENT);                                         \
        CHECK(buffer_is_value(memory, decode_layout.size_bytes, 0xa5U));            \
    } while (0)
    CHECK_PREPARE_FAILURE(workspace.coded[0].data, &result);
    CHECK_PREPARE_FAILURE(filter_workspace.cdef.plane[1].data, &result);
    CHECK_PREPARE_FAILURE(workspace.tile_context.luma_mode_above, &result);
    CHECK_PREPARE_FAILURE(
        (uint8_t *)workspace.reconstruction_scratch, &result);
    CHECK_PREPARE_FAILURE((uint8_t *)&result, &result);
    frame.width = 33U;
    CHECK_PREPARE_FAILURE(output, &result);
#undef CHECK_PREPARE_FAILURE
}

static void test_filter_pipeline(void) {
    _Alignas(16) uint8_t actual_memory[4096];
    _Alignas(16) uint8_t expected_memory[4096];
    uint8_t actual_y[64];
    uint8_t actual_u[16];
    uint8_t actual_v[16];
    uint8_t expected_y[64];
    uint8_t expected_u[16];
    uint8_t expected_v[16];
    uint8_t original_y[64];
    uint8_t cdef_y[64];
    Av2hFilterPlanes actual;
    Av2hFilterPlanes expected;
    Av2hFilterWorkspace actual_workspace;
    Av2hFilterWorkspace expected_workspace;
    Av2hFilterWorkspaceStorage actual_storage;
    Av2hFilterWorkspaceStorage expected_storage;
    Av2hFilterWorkspaceLayout layout;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    frame.width = 8U;
    frame.height = 8U;
    frame.deblock_enabled[0] = 1U;
    frame.deblock_enabled[1] = 1U;
    frame.cdef_strength_count = 1U;
    frame.cdef_on_skip_transform = 1U;
    frame.cdef_damping = 5U;
    frame.cdef_y_strength[0] = 58U;
    frame.cdef_uv_strength[0] = 56U;
    frame.ccso[0].enabled = 1U;
    frame.ccso[0].band_only = 1U;
    frame.ccso[0].offset_count = 1U;
    frame.ccso[0].filter_offset[0] = 1;
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 0U;
    sequence.enable_cdef = 1U;
    sequence.enable_ccso = 1U;
    sequence.enable_gdf = 1U;
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            actual_y[row * 8U + column] = (uint8_t)(
                (column < 4U ? 100U : 104U) +
                (row < 4U ? 0U : 2U));
        }
    }
    for (column = 0U; column < 16U; ++column) {
        actual_u[column] = column < 8U ? 90U : 94U;
        actual_v[column] = column < 8U ? 80U : 84U;
    }
    av2h_memory_copy(original_y, actual_y, sizeof(original_y));
    av2h_memory_copy(expected_y, actual_y, sizeof(actual_y));
    av2h_memory_copy(expected_u, actual_u, sizeof(actual_u));
    av2h_memory_copy(expected_v, actual_v, sizeof(actual_v));
    initialize_planes(&actual, actual_y, actual_u, actual_v);
    initialize_planes(&expected, expected_y, expected_u, expected_v);
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    CHECK(av2h_filter_workspace_init(
              &frame, actual_memory, sizeof(actual_memory),
              &actual_workspace, &actual_storage) == AV2H_OK);
    CHECK(av2h_filter_workspace_init(
              &frame, expected_memory, sizeof(expected_memory),
              &expected_workspace, &expected_storage) == AV2H_OK);
    populate_workspace_maps(&layout, &actual_storage, 128U);
    populate_workspace_maps(&layout, &expected_storage, 128U);
    actual_storage.ccso_controls[0][0] = 1U;
    expected_storage.ccso_controls[0][0] = 1U;

    copy_test_planes(&expected_workspace.deblocked, &expected);
    CHECK(av2h_deblock_filter_frame(
              &expected_workspace.deblocked, &sequence, &frame,
              &expected_workspace.deblock_map) == AV2H_OK);
    CHECK(av2h_memory_compare(
              original_y, expected_workspace.deblocked.plane[0].data,
              sizeof(original_y)) != 0);
    CHECK(av2h_cdef_filter_frame(
              &expected_workspace.cdef, &expected_workspace.deblocked,
              &frame) == AV2H_OK);
    av2h_memory_copy(
        cdef_y, expected_workspace.cdef.plane[0].data, sizeof(cdef_y));
    CHECK(av2h_ccso_filter_frame(
              &expected_workspace.cdef,
              expected_workspace.deblocked.plane[0].data,
              expected_workspace.deblocked.plane[0].stride_samples,
              &frame, &expected_workspace.ccso_controls) == AV2H_OK);
    CHECK(av2h_memory_compare(
              cdef_y, expected_workspace.cdef.plane[0].data,
              sizeof(cdef_y)) != 0);
    CHECK(av2h_gdf_filter_frame(
              &expected.plane[0], &expected_workspace.cdef.plane[0],
              &expected_workspace.deblocked.plane[0], &frame) == AV2H_OK);
    av2h_memory_copy(
        expected.plane[1].data,
        expected_workspace.cdef.plane[1].data,
        expected_workspace.cdef.plane[1].width *
            expected_workspace.cdef.plane[1].height);
    av2h_memory_copy(
        expected.plane[2].data,
        expected_workspace.cdef.plane[2].data,
        expected_workspace.cdef.plane[2].width *
            expected_workspace.cdef.plane[2].height);

    CHECK(av2h_filter_frame(
              &actual, &sequence, &frame, &actual_workspace) == AV2H_OK);
    CHECK(av2h_memory_compare(actual_y, expected_y, sizeof(actual_y)) == 0);
    CHECK(av2h_memory_compare(actual_u, expected_u, sizeof(actual_u)) == 0);
    CHECK(av2h_memory_compare(actual_v, expected_v, sizeof(actual_v)) == 0);
    CHECK(av2h_memory_compare(
              actual_workspace.deblocked.plane[0].data,
              expected_workspace.deblocked.plane[0].data,
              sizeof(actual_y)) == 0);
    CHECK(av2h_memory_compare(
              actual_workspace.cdef.plane[0].data,
              expected_workspace.cdef.plane[0].data,
              sizeof(actual_y)) == 0);
}

static void test_disabled_filters_no_op(void) {
    _Alignas(16) uint8_t memory[4096];
    uint8_t y[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t expected_y[64];
    uint8_t expected_u[16];
    uint8_t expected_v[16];
    Av2hFilterPlanes planes;
    Av2hFilterWorkspace workspace;
    Av2hFilterWorkspaceStorage storage;
    Av2hFilterWorkspaceLayout layout;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t index;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    frame.width = 8U;
    frame.height = 8U;
    for (index = 0U; index < sizeof(y); ++index) {
        y[index] = (uint8_t)(index * 3U + 7U);
        expected_y[index] = y[index];
    }
    for (index = 0U; index < sizeof(u); ++index) {
        u[index] = (uint8_t)(index * 5U + 11U);
        v[index] = (uint8_t)(index * 7U + 13U);
        expected_u[index] = u[index];
        expected_v[index] = v[index];
    }
    initialize_planes(&planes, y, u, v);
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    CHECK(av2h_filter_workspace_init(
              &frame, memory, sizeof(memory),
              &workspace, &storage) == AV2H_OK);
    populate_workspace_maps(&layout, &storage, 208U);
    CHECK(av2h_filter_frame(
              &planes, &sequence, &frame, &workspace) == AV2H_OK);
    CHECK(av2h_memory_compare(y, expected_y, sizeof(y)) == 0);
    CHECK(av2h_memory_compare(u, expected_u, sizeof(u)) == 0);
    CHECK(av2h_memory_compare(v, expected_v, sizeof(v)) == 0);
    CHECK(av2h_memory_compare(
              workspace.deblocked.plane[0].data,
              expected_y, sizeof(expected_y)) == 0);
    CHECK(av2h_memory_compare(
              workspace.cdef.plane[1].data,
              expected_u, sizeof(expected_u)) == 0);
    CHECK(av2h_memory_compare(
              workspace.cdef.plane[2].data,
              expected_v, sizeof(expected_v)) == 0);
}

static void test_visible_i420_crop(void) {
    uint8_t y[26U * 16U];
    uint8_t u[14U * 8U];
    uint8_t v[14U * 8U];
    uint8_t output[403U];
    size_t capacities[AV2H_FRAME_PLANE_COUNT] = {
        sizeof(y), sizeof(u), sizeof(v)
    };
    Av2hFilterPlanes coded = {
        {
            { y, 26U, 24U, 16U },
            { u, 14U, 12U, 8U },
            { v, 14U, 12U, 8U }
        }
    };
    Av2hFrameState frame;
    size_t expected_size = 0U;
    size_t offset;
    size_t row;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    frame.width = 17U;
    frame.height = 15U;
    av2h_memory_fill(y, 0xeeU, sizeof(y));
    av2h_memory_fill(u, 0xeeU, sizeof(u));
    av2h_memory_fill(v, 0xeeU, sizeof(v));
    for (row = 0U; row < 15U; ++row) {
        size_t column;
        for (column = 0U; column < 17U; ++column) {
            y[row * 26U + column] = (uint8_t)(row + column);
        }
    }
    for (row = 0U; row < 8U; ++row) {
        size_t column;
        for (column = 0U; column < 9U; ++column) {
            u[row * 14U + column] = (uint8_t)(40U + row + column);
            v[row * 14U + column] = (uint8_t)(90U + row + column);
        }
    }
    CHECK(av2h_planar_output_size(&frame, &expected_size) == AV2H_OK);
    CHECK(expected_size == 399U);
    av2h_memory_fill(output, 0xa5U, sizeof(output));
    CHECK(av2h_copy_visible_planar_private(
              &frame, &coded, capacities, output,
              expected_size - 1U) == AV2H_OUT_OF_MEMORY);
    CHECK(buffer_is_value(output, sizeof(output), 0xa5U));
    CHECK(av2h_copy_visible_planar_private(
              &frame, &coded, capacities, output,
              sizeof(output)) == AV2H_OK);
    offset = 0U;
    for (row = 0U; row < 15U; ++row) {
        CHECK(av2h_memory_compare(
                  output + offset, y + row * 26U, 17U) == 0);
        offset += 17U;
    }
    for (row = 0U; row < 8U; ++row) {
        CHECK(av2h_memory_compare(
                  output + offset, u + row * 14U, 9U) == 0);
        offset += 9U;
    }
    for (row = 0U; row < 8U; ++row) {
        CHECK(av2h_memory_compare(
                  output + offset, v + row * 14U, 9U) == 0);
        offset += 9U;
    }
    CHECK(offset == expected_size);
    CHECK(buffer_is_value(
              output + expected_size,
              sizeof(output) - expected_size, 0xa5U));
}

static void test_deblock_edge(void) {
    uint8_t pixels[16U * 12U];
    Av2hFilterPlane plane = { pixels, 16U, 16U, 12U };
    size_t row;
    size_t column;
    for (row = 0U; row < 12U; ++row) {
      for (column = 0U; column < 16U; ++column) {
        pixels[row * 16U + column] = column < 8U ? 100U : 104U;
      }
    }
    CHECK(av2h_deblock_edge(&plane, 8U, 2U, 1U, 4U, 4U, 3U, 3U) ==
        AV2H_OK);
    for (row = 2U; row < 6U; ++row) {
      CHECK(pixels[row * 16U + 7U] == 101U);
      CHECK(pixels[row * 16U + 8U] == 103U);
    }
    CHECK(pixels[1U * 16U + 7U] == 100U);
    CHECK(pixels[6U * 16U + 8U] == 104U);

    CHECK(av2h_deblock_edge(&plane, 2U, 4U, 0U, 4U, 4U, 3U, 3U) ==
        AV2H_OK);
    CHECK(av2h_deblock_edge(&plane, 2U, 4U, 0U, 4U, 4U, 0U, 3U) ==
        AV2H_OK);
    CHECK(av2h_deblock_edge(&plane, 2U, 2U, 0U, 4U, 4U, 3U, 3U) ==
        AV2H_INVALID_ARGUMENT);
    CHECK(av2h_deblock_edge(&plane, 8U, 2U, 1U, 5U, 6U, 3U, 3U) ==
        AV2H_INVALID_ARGUMENT);
}

static void test_deblock_edge_u16(void) {
    uint16_t pixels[16U * 12U];
    Av2hFilterPlaneU16 plane = { pixels, 16U, 16U, 12U };
    size_t row;
    size_t column;
    for (row = 0U; row < 12U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            pixels[row * 16U + column] = column < 8U ? 400U : 416U;
        }
    }
    CHECK(av2h_deblock_edge_u16(
              &plane, 8U, 2U, 1U, 4U, 4U, 12U, 12U) == AV2H_OK);
    for (row = 2U; row < 6U; ++row) {
        CHECK(pixels[row * 16U + 7U] == 405U);
        CHECK(pixels[row * 16U + 8U] == 411U);
    }
    CHECK(pixels[1U * 16U + 7U] == 400U);
    CHECK(pixels[6U * 16U + 8U] == 416U);

    for (row = 0U; row < 12U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            pixels[row * 16U + column] = row < 8U ? 400U : 416U;
        }
    }
    CHECK(av2h_deblock_edge_u16(
              &plane, 2U, 8U, 0U, 4U, 4U, 12U, 12U) == AV2H_OK);
    for (column = 2U; column < 6U; ++column) {
        CHECK(pixels[7U * 16U + column] == 405U);
        CHECK(pixels[8U * 16U + column] == 411U);
    }
    CHECK(pixels[7U * 16U + 1U] == 400U);
    CHECK(pixels[8U * 16U + 6U] == 416U);

    for (row = 0U; row < 12U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            pixels[row * 16U + column] = column < 8U ? 400U : 416U;
        }
    }
    CHECK(av2h_deblock_edge_u16(
              &plane, 8U, 2U, 1U, 4U, 4U, 0U, 12U) == AV2H_OK);
    CHECK(pixels[2U * 16U + 7U] == 400U);
    CHECK(pixels[2U * 16U + 8U] == 416U);

    for (row = 0U; row < 12U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            pixels[row * 16U + column] = 0U;
        }
    }
    CHECK(av2h_deblock_edge_u16(
              &plane, 8U, 2U, 1U, 4U, 4U, 12U, 12U) == AV2H_OK);
    CHECK(pixels[2U * 16U + 7U] == 0U);
    CHECK(pixels[2U * 16U + 8U] == 0U);
    for (row = 0U; row < 12U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            pixels[row * 16U + column] = 1023U;
        }
    }
    CHECK(av2h_deblock_edge_u16(
              &plane, 8U, 2U, 1U, 4U, 4U, 12U, 12U) == AV2H_OK);
    CHECK(pixels[2U * 16U + 7U] == 1023U);
    CHECK(pixels[2U * 16U + 8U] == 1023U);
}

static void test_deblock_asymmetric_lengths(void) {
    static const uint8_t expected_full[16] = {
        101U, 102U, 104U, 105U, 106U, 107U, 108U, 109U,
        111U, 112U, 113U, 114U, 115U, 116U, 118U, 119U
    };
    static const uint8_t expected_capped[16] = {
        100U, 100U, 102U, 103U, 105U, 106U, 108U, 109U,
        111U, 112U, 113U, 114U, 115U, 116U, 118U, 119U
    };
    uint8_t full[4U * 25U];
    uint8_t capped[4U * 25U];
    Av2hFilterPlane full_plane = { full, 4U, 4U, 25U };
    Av2hFilterPlane capped_plane = { capped, 4U, 4U, 25U };
    size_t row;
    size_t column;
    for (row = 0U; row < 25U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            full[row * 4U + column] = row < 16U ? 100U : 120U;
            capped[row * 4U + column] = row < 16U ? 100U : 120U;
        }
    }
    CHECK(av2h_deblock_edge(
              &full_plane, 0U, 16U, 0U, 18U, 18U, 255U, 255U) ==
          AV2H_OK);
    CHECK(av2h_deblock_edge(
              &capped_plane, 0U, 16U, 0U, 14U, 18U, 255U, 255U) ==
          AV2H_OK);
    for (row = 0U; row < 16U; ++row) {
        CHECK(full[(row + 8U) * 4U] == expected_full[row]);
        CHECK(capped[(row + 8U) * 4U] == expected_capped[row]);
    }
}

static void test_deblock_frame(void) {
    uint8_t y[16U * 8U];
    uint8_t u[8U * 4U];
    uint8_t v[8U * 4U];
    Av2hDeblockCell y_cells[4U * 2U];
    Av2hDeblockCell u_cells[2U];
    Av2hDeblockCell v_cells[2U];
    Av2hFilterPlanes planes;
    Av2hDeblockMap map;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&map, 0U, sizeof(map));
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 100U : 104U;
        }
    }
    av2h_memory_fill(u, 90U, sizeof(u));
    av2h_memory_fill(v, 80U, sizeof(v));
    planes.plane[0] = (Av2hFilterPlane){ y, 16U, 16U, 8U };
    planes.plane[1] = (Av2hFilterPlane){ u, 8U, 8U, 4U };
    planes.plane[2] = (Av2hFilterPlane){ v, 8U, 8U, 4U };
    for (row = 0U; row < 2U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            Av2hDeblockCell *cell = &y_cells[row * 4U + column];
            *cell = (Av2hDeblockCell){
                (uint16_t)column, (uint16_t)row,
                (uint16_t)column, (uint16_t)row,
                4U, 4U, 128U, 0U, 0U
            };
        }
    }
    for (column = 0U; column < 2U; ++column) {
        u_cells[column] = (Av2hDeblockCell){
            (uint16_t)column, 0U, (uint16_t)column, 0U,
            4U, 4U, 128U, 0U, 0U
        };
        v_cells[column] = u_cells[column];
    }
    map.plane[0] = y_cells;
    map.plane[1] = u_cells;
    map.plane[2] = v_cells;
    map.stride_cells[0] = 4U;
    map.stride_cells[1] = 2U;
    map.stride_cells[2] = 2U;
    map.columns[0] = 4U;
    map.columns[1] = 2U;
    map.columns[2] = 2U;
    map.rows[0] = 2U;
    map.rows[1] = 1U;
    map.rows[2] = 1U;
    frame.width = 16U;
    frame.height = 8U;
    frame.deblock_enabled[0] = 1U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    for (row = 0U; row < 8U; ++row) {
        CHECK(y[row * 16U + 7U] == 101U);
        CHECK(y[row * 16U + 8U] == 103U);
    }
    map.columns[0] = 3U;
    y[0] = 77U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 77U);
}

static void test_deblock_frame_u16(void) {
    uint16_t y[16U * 8U];
    uint16_t u[8U * 4U];
    uint16_t v[8U * 4U];
    Av2hDeblockCell y_cells[4U * 2U];
    Av2hDeblockCell u_cells[2U];
    Av2hDeblockCell v_cells[2U];
    Av2hFilterPlanesU16 planes;
    Av2hDeblockMap map;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&map, 0U, sizeof(map));
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 400U : 416U;
        }
    }
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            u[row * 8U + column] = 360U;
            v[row * 8U + column] = 320U;
        }
    }
    planes.plane[0] = (Av2hFilterPlaneU16){ y, 16U, 16U, 8U };
    planes.plane[1] = (Av2hFilterPlaneU16){ u, 8U, 8U, 4U };
    planes.plane[2] = (Av2hFilterPlaneU16){ v, 8U, 8U, 4U };
    for (row = 0U; row < 2U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            y_cells[row * 4U + column] = (Av2hDeblockCell){
                (uint16_t)column, (uint16_t)row,
                (uint16_t)column, (uint16_t)row,
                4U, 4U, 128U, 0U, 0U
            };
        }
    }
    for (column = 0U; column < 2U; ++column) {
        u_cells[column] = (Av2hDeblockCell){
            (uint16_t)column, 0U, (uint16_t)column, 0U,
            4U, 4U, 128U, 0U, 0U
        };
        v_cells[column] = u_cells[column];
    }
    map.plane[0] = y_cells;
    map.plane[1] = u_cells;
    map.plane[2] = v_cells;
    map.stride_cells[0] = 4U;
    map.stride_cells[1] = 2U;
    map.stride_cells[2] = 2U;
    map.columns[0] = 4U;
    map.columns[1] = 2U;
    map.columns[2] = 2U;
    map.rows[0] = 2U;
    map.rows[1] = 1U;
    map.rows[2] = 1U;
    frame.width = 16U;
    frame.height = 8U;
    frame.deblock_enabled[0] = 1U;
    CHECK(av2h_deblock_filter_frame_u16(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    for (row = 0U; row < 8U; ++row) {
        CHECK(y[row * 16U + 7U] == 404U);
        CHECK(y[row * 16U + 8U] == 412U);
    }

    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 400U : 416U;
        }
    }
    y_cells[1].lossless = 1U;
    CHECK(av2h_deblock_filter_frame_u16(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    CHECK(y[7U] == 400U && y[8U] == 412U);
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 400U : 416U;
        }
    }
    y_cells[2].lossless = 1U;
    CHECK(av2h_deblock_filter_frame_u16(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    CHECK(y[7U] == 400U && y[8U] == 416U);
    map.columns[0] = 3U;
    y[0] = 77U;
    CHECK(av2h_deblock_filter_frame_u16(
              &planes, &sequence, &frame, &map) == AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 77U);
}

static void test_deblock_stage_u16_odd_noop(void) {
    uint16_t source_y[24U * 16U];
    uint16_t source_u[12U * 8U];
    uint16_t source_v[12U * 8U];
    uint16_t destination_y[24U * 16U];
    uint16_t destination_u[12U * 8U];
    uint16_t destination_v[12U * 8U];
    Av2hDeblockCell y_cells[6U * 4U];
    Av2hDeblockCell u_cells[3U * 2U];
    Av2hDeblockCell v_cells[3U * 2U];
    Av2hFilterPlanesU16 reconstructed = { 0 };
    Av2hFilterWorkspace workspace = { 0 };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    size_t plane;
    size_t index;
    uint16_t *sources[3U] = { source_y, source_u, source_v };
    uint16_t *destinations[3U] = {
        destination_y, destination_u, destination_v
    };
    Av2hDeblockCell *cells[3U] = { y_cells, u_cells, v_cells };
    const size_t widths[3U] = { 24U, 12U, 12U };
    const size_t heights[3U] = { 16U, 8U, 8U };
    const size_t columns[3U] = { 6U, 3U, 3U };
    const size_t rows[3U] = { 4U, 2U, 2U };
    frame.width = 17U;
    frame.height = 15U;
    for (plane = 0U; plane < 3U; ++plane) {
        for (index = 0U; index < widths[plane] * heights[plane]; ++index) {
            sources[plane][index] = index == 0U ? 0U :
                index + 1U == widths[plane] * heights[plane] ? 1023U :
                (uint16_t)((index * 37U + plane * 101U) & 1023U);
            destinations[plane][index] = 0xa5U;
        }
        reconstructed.plane[plane] = (Av2hFilterPlaneU16){
            sources[plane], widths[plane],
            (uint16_t)widths[plane], (uint16_t)heights[plane]
        };
        workspace.deblocked_u16.plane[plane] = (Av2hFilterPlaneU16){
            destinations[plane], widths[plane],
            (uint16_t)widths[plane], (uint16_t)heights[plane]
        };
        workspace.deblock_map.plane[plane] = cells[plane];
        workspace.deblock_map.stride_cells[plane] = columns[plane];
        workspace.deblock_map.columns[plane] = columns[plane];
        workspace.deblock_map.rows[plane] = rows[plane];
        for (index = 0U; index < columns[plane] * rows[plane]; ++index) {
            size_t column = index % columns[plane];
            size_t row = index / columns[plane];
            cells[plane][index] = (Av2hDeblockCell){
                (uint16_t)column, (uint16_t)row,
                (uint16_t)column, (uint16_t)row,
                4U, 4U, 128U, 0U, 0U
            };
        }
    }
    CHECK(av2h_deblock_stage_u16(
              &reconstructed, &sequence, &frame, &workspace) == AV2H_OK);
    for (plane = 0U; plane < 3U; ++plane) {
        CHECK(av2h_memory_compare(
                  sources[plane], destinations[plane],
                  widths[plane] * heights[plane] * sizeof(uint16_t)) == 0);
    }
    sequence.info.bit_depth_idc = 1U;
    destination_y[0] = 0xa5U;
    CHECK(av2h_deblock_stage_u16(
              &reconstructed, &sequence, &frame, &workspace) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(destination_y[0] == 0xa5U);
}

static void test_deblock_skip_and_lossless(void) {
    uint8_t y[16U * 8U];
    uint8_t u[8U * 4U];
    uint8_t v[8U * 4U];
    Av2hDeblockCell y_cells[4U * 2U];
    Av2hDeblockCell u_cells[2U];
    Av2hDeblockCell v_cells[2U];
    Av2hFilterPlanes planes;
    Av2hDeblockMap map;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&map, 0U, sizeof(map));
    av2h_memory_fill(u, 90U, sizeof(u));
    av2h_memory_fill(v, 80U, sizeof(v));
    planes.plane[0] = (Av2hFilterPlane){ y, 16U, 16U, 8U };
    planes.plane[1] = (Av2hFilterPlane){ u, 8U, 8U, 4U };
    planes.plane[2] = (Av2hFilterPlane){ v, 8U, 8U, 4U };
    for (row = 0U; row < 2U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            y_cells[row * 4U + column] = (Av2hDeblockCell){
                0U,
                0U,
                (uint16_t)(column < 2U ? 0U : 2U),
                0U,
                4U,
                4U,
                128U,
                0U,
                0U
            };
        }
    }
    for (column = 0U; column < 2U; ++column) {
        u_cells[column] = (Av2hDeblockCell){
            0U, 0U, 0U, 0U, 4U, 4U, 128U, 0U, 0U
        };
        v_cells[column] = u_cells[column];
    }
    map.plane[0] = y_cells;
    map.plane[1] = u_cells;
    map.plane[2] = v_cells;
    map.stride_cells[0] = 4U;
    map.stride_cells[1] = 2U;
    map.stride_cells[2] = 2U;
    map.columns[0] = 4U;
    map.columns[1] = 2U;
    map.columns[2] = 2U;
    map.rows[0] = 2U;
    map.rows[1] = 1U;
    map.rows[2] = 1U;
    frame.width = 16U;
    frame.height = 8U;
    frame.deblock_enabled[0] = 1U;

    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 100U : 104U;
        }
    }
    y_cells[2].skip = 1U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    CHECK(y[7U] == 100U && y[8U] == 104U);

    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 100U : 104U;
        }
    }
    y_cells[2].skip = 0U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    CHECK(y[7U] == 101U && y[8U] == 103U);

    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            y[row * 16U + column] = column < 8U ? 100U : 104U;
        }
    }
    y_cells[1].lossless = 1U;
    y_cells[2].lossless = 1U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    CHECK(y[7U] == 100U && y[8U] == 104U);

    y_cells[1].lossless = 0U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    CHECK(y[7U] == 101U && y[8U] == 104U);
}

static void filter_chroma_horizontal_edge(uint8_t chroma_format_idc,
                                          uint8_t output[4U * 64U]) {
    uint8_t y[8U * 128U];
    uint8_t u[8U * 64U];
    uint8_t v[8U * 64U];
    Av2hDeblockCell y_cells[2U * 32U];
    Av2hDeblockCell u_cells[4U * 32U];
    Av2hDeblockCell v_cells[4U * 32U];
    Av2hFilterPlanes planes;
    Av2hDeblockMap map;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t y_columns = 2U;
    size_t y_rows = chroma_format_idc == 2U ? 16U : 32U;
    size_t chroma_columns = chroma_format_idc == 2U ? 4U : 2U;
    size_t chroma_width = chroma_format_idc == 2U ? 8U : 4U;
    size_t row;
    size_t column;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&map, 0U, sizeof(map));
    av2h_memory_fill(y, 100U, sizeof(y));
    for (row = 0U; row < 64U; ++row) {
        for (column = 0U; column < chroma_width; ++column) {
            u[row * chroma_width + column] = row < 32U ? 100U : 104U;
            v[row * chroma_width + column] = 80U;
        }
    }
    planes.plane[0] = (Av2hFilterPlane){
        y, 8U, 8U,
        (uint16_t)(chroma_format_idc == 2U ? 64U : 128U)
    };
    planes.plane[1] = (Av2hFilterPlane){
        u, (uint16_t)chroma_width, (uint16_t)chroma_width, 64U
    };
    planes.plane[2] = (Av2hFilterPlane){
        v, (uint16_t)chroma_width, (uint16_t)chroma_width, 64U
    };
    for (row = 0U; row < y_rows; ++row) {
        for (column = 0U; column < y_columns; ++column) {
            y_cells[row * y_columns + column] = (Av2hDeblockCell){
                (uint16_t)column, (uint16_t)row,
                0U, 0U, 4U, 16U, 128U, 0U, 0U
            };
        }
    }
    for (row = 0U; row < 32U; ++row) {
        for (column = 0U; column < chroma_columns; ++column) {
            u_cells[row * chroma_columns + column] = (Av2hDeblockCell){
                (uint16_t)column, (uint16_t)row,
                0U, (uint16_t)(row < 16U ? 0U : 16U),
                4U, 16U, 128U, 0U, 0U
            };
            v_cells[row * chroma_columns + column] =
                u_cells[row * chroma_columns + column];
        }
    }
    map.plane[0] = y_cells;
    map.plane[1] = u_cells;
    map.plane[2] = v_cells;
    map.stride_cells[0] = y_columns;
    map.stride_cells[1] = chroma_columns;
    map.stride_cells[2] = chroma_columns;
    map.columns[0] = y_columns;
    map.columns[1] = chroma_columns;
    map.columns[2] = chroma_columns;
    map.rows[0] = y_rows;
    map.rows[1] = 32U;
    map.rows[2] = 32U;
    frame.width = planes.plane[0].width;
    frame.height = planes.plane[0].height;
    frame.chroma_format_idc = chroma_format_idc;
    frame.deblock_enabled[2] = 1U;
    CHECK(av2h_deblock_filter_frame(
              &planes, &sequence, &frame, &map) == AV2H_OK);
    for (row = 0U; row < 64U; ++row) {
        av2h_memory_copy(output + row * 4U,
                         u + row * chroma_width, 4U);
    }
}

static void test_i444_deblock_superblock_cadence(void) {
    uint8_t i420[4U * 64U];
    uint8_t i444[4U * 64U];
    filter_chroma_horizontal_edge(1U, i420);
    filter_chroma_horizontal_edge(2U, i444);
    CHECK(av2h_memory_compare(i420, i444, sizeof(i420)) != 0);
}

static void test_cdef(void) {
    uint8_t source_data[16U * 16U];
    uint8_t destination_data[16U * 16U];
    Av2hFilterPlane source = { source_data, 16U, 16U, 16U };
    Av2hFilterPlane destination = {
        destination_data, 16U, 16U, 16U
    };
    uint8_t direction;
    uint32_t variance;
    size_t row;
    size_t column;
    for (row = 0U; row < 16U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            source_data[row * 16U + column] = 128U;
            destination_data[row * 16U + column] = 128U;
        }
    }
    CHECK(av2h_cdef_find_direction(&source, 0U, 0U, &direction, &variance) ==
          AV2H_OK);
    CHECK(direction == 0U && variance == 0U);
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            source_data[row * 16U + column] = (uint8_t)(column * 16U);
        }
    }
    CHECK(av2h_cdef_find_direction(&source, 0U, 0U, &direction, &variance) ==
          AV2H_OK);
    CHECK(direction == 6U && variance != 0U);
    for (row = 0U; row < 16U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            source_data[row * 16U + column] = 100U;
            destination_data[row * 16U + column] = 100U;
        }
    }
    source_data[8U * 16U + 8U] = 140U;
    destination_data[8U * 16U + 8U] = 140U;
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 8U, 8U, 8U, 8U, 0U,
              14U, 0U, 5U, 2U, 262144U) == AV2H_OK);
    CHECK(destination_data[8U * 16U + 8U] == 137U);
    CHECK(source_data[8U * 16U + 8U] == 140U);
    CHECK(av2h_cdef_find_direction(&source, 9U, 9U, &direction, &variance) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_cdef_u16(void) {
    uint16_t source_data[16U * 16U];
    uint16_t destination_data[16U * 16U];
    Av2hFilterPlaneU16 source = { source_data, 16U, 16U, 16U };
    Av2hFilterPlaneU16 destination = {
        destination_data, 16U, 16U, 16U
    };
    uint8_t direction;
    uint32_t variance;
    size_t row;
    size_t column;
    for (row = 0U; row < 16U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            source_data[row * 16U + column] = 512U;
            destination_data[row * 16U + column] = 512U;
        }
    }
    CHECK(av2h_cdef_find_direction_u16(
              &source, 0U, 0U, &direction, &variance) == AV2H_OK);
    CHECK(direction == 0U && variance == 0U);
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            source_data[row * 16U + column] = (uint16_t)(column * 64U);
        }
    }
    CHECK(av2h_cdef_find_direction_u16(
              &source, 0U, 0U, &direction, &variance) == AV2H_OK);
    CHECK(direction == 6U && variance != 0U);
    for (row = 0U; row < 16U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            source_data[row * 16U + column] = 400U;
            destination_data[row * 16U + column] = 400U;
        }
    }
    source_data[8U * 16U + 8U] = 560U;
    destination_data[8U * 16U + 8U] = 560U;
    CHECK(av2h_cdef_filter_block_u16(
              &destination, &source, 8U, 8U, 8U, 8U, 0U,
              14U, 0U, 5U, 2U, 262144U) == AV2H_OK);
    CHECK(destination_data[8U * 16U + 8U] == 548U);
    CHECK(source_data[8U * 16U + 8U] == 560U);
}

static void test_cdef_u16_odd_geometry(void) {
    uint16_t source_y[24U * 16U];
    uint16_t source_u[12U * 8U];
    uint16_t source_v[12U * 8U];
    uint16_t destination_y[24U * 16U];
    uint16_t destination_u[12U * 8U];
    uint16_t destination_v[12U * 8U];
    Av2hFilterPlanesU16 source;
    Av2hFilterPlanesU16 destination;
    Av2hFrameState frame;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (index = 0U; index < 24U * 16U; ++index) {
        source_y[index] = 400U;
        destination_y[index] = 0U;
    }
    for (index = 0U; index < 12U * 8U; ++index) {
        source_u[index] = 400U;
        source_v[index] = 400U;
        destination_u[index] = 0U;
        destination_v[index] = 0U;
    }
    source_u[7U * 12U + 10U] = 456U;
    source.plane[0] = (Av2hFilterPlaneU16){ source_y, 24U, 24U, 16U };
    source.plane[1] = (Av2hFilterPlaneU16){ source_u, 12U, 12U, 8U };
    source.plane[2] = (Av2hFilterPlaneU16){ source_v, 12U, 12U, 8U };
    destination.plane[0] =
        (Av2hFilterPlaneU16){ destination_y, 24U, 24U, 16U };
    destination.plane[1] =
        (Av2hFilterPlaneU16){ destination_u, 12U, 12U, 8U };
    destination.plane[2] =
        (Av2hFilterPlaneU16){ destination_v, 12U, 12U, 8U };
    frame.width = 17U;
    frame.height = 15U;
    frame.cdef_strength_count = 1U;
    frame.cdef_on_skip_transform = 1U;
    frame.cdef_damping = 5U;
    frame.cdef_uv_strength[0] = 56U;
    CHECK(av2h_cdef_filter_frame_u16(
              &destination, &source, &frame) == AV2H_OK);
    CHECK(destination_y[15U * 24U + 23U] == 400U);
    CHECK(destination_u[7U * 12U + 10U] < 456U);
    CHECK(source_u[7U * 12U + 10U] == 456U);
}

static void test_cdef_rounding_and_parameters(void) {
    uint8_t source_data[8U * 8U];
    uint8_t destination_data[8U * 8U];
    Av2hFilterPlane source = { source_data, 8U, 8U, 8U };
    Av2hFilterPlane destination = {
        destination_data, 8U, 8U, 8U
    };
    av2h_memory_fill(source_data, 140U, sizeof(source_data));
    av2h_memory_fill(destination_data, 0U, sizeof(destination_data));
    source_data[4U * 8U + 4U] = 100U;
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 4U, 4U, 1U, 1U, 0U,
              14U, 0U, 5U, 2U, 262144U) == AV2H_OK);
    CHECK(destination_data[4U * 8U + 4U] == 103U);

    av2h_memory_fill(source_data, 100U, sizeof(source_data));
    source_data[4U * 8U + 4U] = 140U;
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 4U, 4U, 1U, 1U, 0U,
              14U, 0U, 5U, 2U, 262144U) == AV2H_OK);
    CHECK(destination_data[4U * 8U + 4U] == 137U);

    destination_data[4U * 8U + 4U] = 77U;
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 4U, 4U, 1U, 1U, 0U,
              16U, 0U, 5U, 2U, 262144U) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 4U, 4U, 1U, 1U, 0U,
              14U, 3U, 5U, 2U, 262144U) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_cdef_filter_block(
              &destination, &source, 4U, 4U, 1U, 1U, 0U,
              14U, 0U, 7U, 2U, 262144U) == AV2H_INVALID_ARGUMENT);
    CHECK(destination_data[4U * 8U + 4U] == 77U);
}

static void test_cdef_frame(void) {
    uint8_t source_y[16U * 16U];
    uint8_t source_u[8U * 8U];
    uint8_t source_v[8U * 8U];
    uint8_t destination_y[16U * 16U];
    uint8_t destination_u[8U * 8U];
    uint8_t destination_v[8U * 8U];
    uint8_t expected_y[16U * 16U];
    Av2hFilterPlanes source;
    Av2hFilterPlanes destination;
    Av2hFilterPlane expected;
    Av2hCdefControlMap controls;
    Av2hFrameState frame;
    uint8_t strength = 1U;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (index = 0U; index < sizeof(source_y); ++index) {
        source_y[index] = 100U;
        destination_y[index] = 100U;
    }
    for (index = 0U; index < sizeof(source_u); ++index) {
        source_u[index] = 100U;
        source_v[index] = 100U;
        destination_u[index] = 100U;
        destination_v[index] = 100U;
    }
    source_y[4U * 16U + 4U] = 108U;
    destination_y[4U * 16U + 4U] = 108U;
    source.plane[0] = (Av2hFilterPlane){ source_y, 16U, 16U, 16U };
    source.plane[1] = (Av2hFilterPlane){ source_u, 8U, 8U, 8U };
    source.plane[2] = (Av2hFilterPlane){ source_v, 8U, 8U, 8U };
    destination.plane[0] =
        (Av2hFilterPlane){ destination_y, 16U, 16U, 16U };
    destination.plane[1] =
        (Av2hFilterPlane){ destination_u, 8U, 8U, 8U };
    destination.plane[2] =
        (Av2hFilterPlane){ destination_v, 8U, 8U, 8U };
    frame.width = 16U;
    frame.height = 16U;
    frame.cdef_strength_count = 1U;
    frame.cdef_on_skip_transform = 1U;
    frame.cdef_damping = 5U;
    frame.cdef_y_strength[0] = 58U;
    frame.cdef_uv_strength[0] = 56U;
    CHECK(av2h_cdef_filter_frame(&destination, &source, &frame) == AV2H_OK);
    CHECK(destination_y[4U * 16U + 4U] < 108U);
    CHECK(source_y[4U * 16U + 4U] == 108U);
    CHECK(av2h_cdef_filter_frame(&source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    frame.cdef_strength_count = 2U;
        frame.cdef_y_strength[1] = 0U;
        frame.cdef_uv_strength[1] = 0U;
    controls = (Av2hCdefControlMap){ &strength, 1U, 1U };
    CHECK(av2h_cdef_filter_frame_with_controls(
              &destination, &source, &frame, &controls) == AV2H_OK);
        CHECK(destination_y[4U * 16U + 4U] == 108U);

    for (index = 0U; index < sizeof(source_y); ++index) {
        source_y[index] = (uint8_t)((index % 16U) * 16U);
    }
    av2h_memory_copy(expected_y, source_y, sizeof(expected_y));
    expected = (Av2hFilterPlane){ expected_y, 16U, 16U, 16U };
    frame.cdef_strength_count = 1U;
    frame.cdef_y_strength[0] = 2U;
    frame.cdef_uv_strength[0] = 0U;
    for (index = 0U; index < 4U; ++index) {
        uint16_t x = (uint16_t)((index % 2U) * 8U);
        uint16_t y = (uint16_t)((index / 2U) * 8U);
        CHECK(av2h_cdef_filter_block(
                  &expected, &source.plane[0], x, y, 8U, 8U, 0U,
                  0U, 2U, frame.cdef_damping, 0U, 0U) == AV2H_OK);
    }
    CHECK(av2h_cdef_filter_frame(&destination, &source, &frame) == AV2H_OK);
    CHECK(av2h_memory_compare(
              destination_y, expected_y, sizeof(expected_y)) == 0);
}

static void test_gdf_frame(void) {
    static const uint8_t expected[64] = {
        2U, 18U, 33U, 50U, 68U, 85U, 103U, 118U,
        29U, 48U, 66U, 86U, 108U, 130U, 150U, 170U,
        57U, 80U, 102U, 125U, 149U, 172U, 195U, 223U,
        86U, 112U, 139U, 165U, 191U, 220U, 243U, 12U,
        115U, 145U, 175U, 201U, 233U, 4U, 31U, 60U,
        145U, 179U, 207U, 242U, 15U, 51U, 80U, 109U,
        176U, 208U, 245U, 21U, 59U, 91U, 128U, 163U,
        203U, 244U, 22U, 58U, 97U, 139U, 177U, 212U
    };
    uint8_t source_data[64];
    uint8_t cdef_data[64];
    uint8_t destination_data[64];
    uint8_t overlap_data[65];
    uint8_t interleaved_data[32];
    uint8_t interleaved_destination_data[16];
    uint8_t gdf_flag = 0U;
    Av2hFilterPlane source = { source_data, 8U, 8U, 8U };
    Av2hFilterPlane cdef = { cdef_data, 8U, 8U, 8U };
    Av2hFilterPlane destination = { destination_data, 8U, 8U, 8U };
    Av2hFilterPlane overlap_source = { overlap_data, 8U, 8U, 8U };
    Av2hFilterPlane overlap_destination = {
        overlap_data + 1U, 8U, 8U, 8U
    };
    Av2hFilterPlane interleaved_cdef = {
        interleaved_data, 8U, 4U, 4U
    };
    Av2hFilterPlane interleaved_deblocked = {
        interleaved_data + 4U, 8U, 4U, 4U
    };
    Av2hFilterPlane interleaved_destination = {
        interleaved_destination_data, 4U, 4U, 4U
    };
    Av2hFrameState frame;
    Av2hGdfControlMap gdf_controls = {
        &gdf_flag, 1U, 1U, 1U, 8U
    };
    size_t row;
    size_t column;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(overlap_data, 33U, sizeof(overlap_data));
    av2h_memory_fill(interleaved_data, 44U, sizeof(interleaved_data));
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            source_data[row * 8U + column] = (uint8_t)(
                column * 17U + row * 29U + column * row * 3U);
            cdef_data[row * 8U + column] = source_data[row * 8U + column];
            destination_data[row * 8U + column] = 0U;
        }
    }
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 3U;
    CHECK(av2h_gdf_filter_frame(
              &destination, &cdef, &source, &frame) == AV2H_OK);
    for (row = 0U; row < 64U; ++row) {
        CHECK(destination_data[row] == expected[row]);
    }
    CHECK(av2h_gdf_filter_frame(&source, &source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(source_data[0] == 0U);
    cdef_data[0] = 255U;
    CHECK(av2h_gdf_filter_frame(&source, &cdef, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(source_data[0] == 0U);
    CHECK(av2h_gdf_filter_frame(
              &overlap_destination, &overlap_source, &source,
              &frame) == AV2H_INVALID_ARGUMENT);
    CHECK(overlap_data[1] == 33U);
    destination_data[0] = 77U;
    CHECK(av2h_gdf_filter_frame(
              &destination, &source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(destination_data[0] == 77U);
    frame.gdf_mode = 0U;
    CHECK(av2h_gdf_filter_frame(
              &interleaved_destination, &interleaved_cdef,
              &interleaved_deblocked, &frame) == AV2H_OK);
    CHECK(interleaved_destination_data[0] == 44U);
    frame.gdf_mode = 2U;
    frame.width = 8U;
    frame.height = 8U;
    frame.gdf_block_size = 8U;
    frame.gdf_block_columns = 1U;
    frame.gdf_block_rows = 1U;
    destination_data[0] = 77U;
    CHECK(av2h_gdf_filter_frame(
              &destination, &source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(destination_data[0] == 77U);
    cdef_data[0] = source_data[0];
    CHECK(av2h_gdf_filter_frame_with_controls(
              &destination, &cdef, &source, &frame,
              &gdf_controls) == AV2H_OK);
    CHECK(av2h_memory_compare(
              destination_data, cdef_data, sizeof(cdef_data)) == 0);
    gdf_flag = 1U;
    CHECK(av2h_gdf_filter_frame_with_controls(
              &destination, &cdef, &source, &frame,
              &gdf_controls) == AV2H_OK);
    CHECK(av2h_memory_compare(
              destination_data, expected, sizeof(expected)) == 0);
    gdf_flag = 2U;
    destination_data[0] = 77U;
    CHECK(av2h_gdf_filter_frame_with_controls(
              &destination, &cdef, &source, &frame,
              &gdf_controls) == AV2H_INVALID_ARGUMENT);
    CHECK(destination_data[0] == 77U);
}

static void test_gdf_frame_u16(void) {
    static const uint16_t expected[64] = {
        9U, 70U, 132U, 202U, 272U, 341U, 410U, 473U,
        116U, 193U, 266U, 345U, 433U, 520U, 600U, 678U,
        227U, 319U, 410U, 501U, 596U, 687U, 782U, 891U,
        344U, 449U, 556U, 660U, 764U, 879U, 973U, 48U,
        462U, 579U, 700U, 804U, 933U, 16U, 126U, 242U,
        581U, 714U, 828U, 968U, 62U, 205U, 322U, 435U,
        704U, 832U, 980U, 86U, 237U, 364U, 512U, 653U,
        813U, 974U, 88U, 234U, 387U, 555U, 706U, 850U
    };
    uint16_t source_data[64];
    uint16_t cdef_data[64];
    uint16_t destination_data[64];
    Av2hFilterPlaneU16 source = { source_data, 8U, 8U, 8U };
    Av2hFilterPlaneU16 cdef = { cdef_data, 8U, 8U, 8U };
    Av2hFilterPlaneU16 destination = {
        destination_data, 8U, 8U, 8U
    };
    Av2hFrameState frame;
    size_t row;
    size_t column;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            uint8_t source_8bit = (uint8_t)(
                column * 17U + row * 29U + column * row * 3U);
            source_data[row * 8U + column] = (uint16_t)source_8bit * 4U;
            cdef_data[row * 8U + column] =
                source_data[row * 8U + column];
            destination_data[row * 8U + column] = 0U;
        }
    }
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 3U;
    CHECK(av2h_gdf_filter_frame_u16(
              &destination, &cdef, &source, &frame) == AV2H_OK);
    for (row = 0U; row < 64U; ++row) {
        CHECK(destination_data[row] == expected[row]);
    }
    CHECK(av2h_gdf_filter_frame_u16(&source, &source, &source, &frame) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_gdf_stripe_boundary_sources(void) {
    uint8_t cdef_data[8U * 128U];
    uint8_t cdef_changed[8U * 128U];
    uint8_t deblocked_data[8U * 128U];
    uint8_t deblocked_changed[8U * 128U];
    uint8_t baseline_data[8U * 128U];
    uint8_t cdef_result[8U * 128U];
    uint8_t deblocked_result[8U * 128U];
    Av2hFilterPlane cdef = { cdef_data, 8U, 8U, 128U };
    Av2hFilterPlane changed_cdef = { cdef_changed, 8U, 8U, 128U };
    Av2hFilterPlane deblocked = { deblocked_data, 8U, 8U, 128U };
    Av2hFilterPlane changed_deblocked = {
        deblocked_changed, 8U, 8U, 128U
    };
    Av2hFilterPlane baseline = { baseline_data, 8U, 8U, 128U };
    Av2hFilterPlane cdef_destination = { cdef_result, 8U, 8U, 128U };
    Av2hFilterPlane deblocked_destination = {
        deblocked_result, 8U, 8U, 128U
    };
    Av2hFrameState frame;
    size_t row;
    size_t column;
    int deblocked_difference = 0;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (row = 0U; row < 128U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            uint8_t value = (uint8_t)(
                row * 13U + column * 29U + row * column * 3U);
            cdef_data[row * 8U + column] = value;
            cdef_changed[row * 8U + column] = value;
            deblocked_data[row * 8U + column] = value;
            deblocked_changed[row * 8U + column] = value;
        }
    }
    for (column = 0U; column < 8U; ++column) {
        cdef_changed[55U * 8U + column] ^= 255U;
        deblocked_changed[55U * 8U + column] ^= 255U;
    }
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 3U;
    CHECK(av2h_gdf_filter_frame(
              &baseline, &cdef, &deblocked, &frame) == AV2H_OK);
    CHECK(av2h_gdf_filter_frame(
              &cdef_destination, &changed_cdef, &deblocked,
              &frame) == AV2H_OK);
    CHECK(av2h_gdf_filter_frame(
              &deblocked_destination, &cdef, &changed_deblocked,
              &frame) == AV2H_OK);
    for (row = 56U; row < 60U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            size_t index = row * 8U + column;
            CHECK(cdef_result[index] == baseline_data[index]);
            if (deblocked_result[index] != baseline_data[index]) {
                deblocked_difference = 1;
            }
        }
    }
    CHECK(deblocked_difference != 0);
}

static void test_gdf_control_row_offset(void) {
    uint8_t cdef_data[8U * 136U];
    uint8_t deblocked_data[8U * 136U];
    uint8_t always_data[8U * 136U];
    uint8_t controlled_data[8U * 136U];
    uint8_t flags[2U] = { 0U, 1U };
    Av2hFilterPlane cdef = { cdef_data, 8U, 8U, 136U };
    Av2hFilterPlane deblocked = { deblocked_data, 8U, 8U, 136U };
    Av2hFilterPlane always = { always_data, 8U, 8U, 136U };
    Av2hFilterPlane controlled = { controlled_data, 8U, 8U, 136U };
    Av2hGdfControlMap controls = { flags, 1U, 1U, 2U, 128U };
    Av2hFrameState frame;
    size_t index;
    int saw_filtered_sample = 0;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    for (index = 0U; index < sizeof(cdef_data); ++index) {
        cdef_data[index] = (uint8_t)(index * 29U + index / 7U);
        deblocked_data[index] = cdef_data[index];
    }
    frame.width = 8U;
    frame.height = 136U;
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 3U;
    CHECK(av2h_gdf_filter_frame(
              &always, &cdef, &deblocked, &frame) == AV2H_OK);
    frame.gdf_mode = 2U;
    frame.gdf_block_size = 128U;
    frame.gdf_block_columns = 1U;
    frame.gdf_block_rows = 2U;
    CHECK(av2h_gdf_filter_frame_with_controls(
              &controlled, &cdef, &deblocked, &frame, &controls) == AV2H_OK);
    for (index = 0U; index < 8U * 120U; ++index) {
        CHECK(controlled_data[index] == cdef_data[index]);
    }
    for (; index < sizeof(controlled_data); ++index) {
        CHECK(controlled_data[index] == always_data[index]);
        if (always_data[index] != cdef_data[index]) saw_filtered_sample = 1;
    }
    CHECK(saw_filtered_sample);
}

static void test_band_only_and_control(void) {
    uint8_t y[64];
    uint8_t reference[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t control = 1U;
    Av2hFilterPlanes planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    for (index = 0U; index < 64U; ++index) {
        y[index] = 100U;
        reference[index] = 100U;
    }
    for (index = 0U; index < 16U; ++index) {
        u[index] = index == 0U ? 254U : 100U;
        v[index] = 77U;
    }
    initialize_planes(&planes, y, u, v);
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[1].enabled = 1U;
    frame.ccso[1].band_only = 1U;
    frame.ccso[1].offset_count = 1U;
    frame.ccso[1].filter_offset[0] = 3;
    controls.plane[1] = &control;
    controls.stride_units[1] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
    CHECK(av2h_ccso_filter_frame(&planes, reference, 8U, &frame,
                                  &controls) == AV2H_OK);
    CHECK(u[0] == 255U);
    for (index = 1U; index < 16U; ++index) CHECK(u[index] == 103U);
    for (index = 0U; index < 16U; ++index) CHECK(v[index] == 77U);

    control = 0U;
    CHECK(av2h_ccso_filter_frame(&planes, reference, 8U, &frame,
                                  &controls) == AV2H_OK);
    CHECK(u[1] == 103U);
}

static void test_ccso_u16(void) {
    uint16_t y[64];
    uint16_t reference[64];
    uint16_t u[16];
    uint16_t v[16];
    uint8_t control = 1U;
    Av2hFilterPlanesU16 planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    for (index = 0U; index < 64U; ++index) {
        y[index] = 1000U;
        reference[index] = index == 1U ? 900U : 700U;
    }
    for (index = 0U; index < 16U; ++index) {
        u[index] = index == 0U ? 1022U : 400U;
        v[index] = 300U;
    }
    planes.plane[0] = (Av2hFilterPlaneU16){ y, 8U, 8U, 8U };
    planes.plane[1] = (Av2hFilterPlaneU16){ u, 4U, 4U, 4U };
    planes.plane[2] = (Av2hFilterPlaneU16){ v, 4U, 4U, 4U };
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[1].enabled = 1U;
    frame.ccso[1].band_only = 1U;
    frame.ccso[1].max_band_log2 = 1U;
    frame.ccso[1].offset_count = 2U;
    frame.ccso[1].filter_offset[16] = 3;
    controls.plane[1] = &control;
    controls.stride_units[1] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
    CHECK(av2h_ccso_filter_frame_u16(
              &planes, reference, 8U, &frame, &controls) == AV2H_OK);
    CHECK(u[0] == 1023U);
    CHECK(u[1] == 403U);

    frame.ccso[1].band_only = 0U;
    frame.ccso[1].extended_filter_support = 1U;
    frame.ccso[1].max_band_log2 = 0U;
    frame.ccso[1].offset_count = 9U;
    frame.ccso[1].filter_offset[6] = -5;
    frame.ccso[1].filter_offset[9] = 7;
    u[0] = 3U;
    u[1] = 400U;
    reference[0] = 700U;
    reference[1] = 900U;
    CHECK(av2h_ccso_filter_frame_u16(
              &planes, reference, 8U, &frame, &controls) == AV2H_OK);
    CHECK(u[0] == 0U);
    CHECK(u[1] == 407U);
}

static void test_ccso_multi_unit_chroma(void) {
    uint8_t y[128U * 64U];
    uint8_t reference[128U * 64U];
    uint8_t u[64U * 32U];
    uint8_t v[64U * 32U];
    uint8_t control[2] = { 1U, 1U };
    Av2hFilterPlanes planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    size_t row;
    size_t column;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    av2h_memory_fill(y, 0U, sizeof(y));
    av2h_memory_fill(u, 100U, sizeof(u));
    av2h_memory_fill(v, 77U, sizeof(v));
    for (row = 0U; row < 64U; ++row) {
        for (column = 0U; column < 128U; ++column) {
            reference[row * 128U + column] =
                column < 64U ? 0U : 255U;
        }
    }
    planes.plane[0] = (Av2hFilterPlane){ y, 128U, 128U, 64U };
    planes.plane[1] = (Av2hFilterPlane){ u, 64U, 64U, 32U };
    planes.plane[2] = (Av2hFilterPlane){ v, 64U, 64U, 32U };
    frame.width = 128U;
    frame.height = 64U;
    frame.ccso[1].enabled = 1U;
    frame.ccso[1].band_only = 1U;
    frame.ccso[1].max_band_log2 = 1U;
    frame.ccso[1].offset_count = 2U;
    frame.ccso[1].filter_offset[0] = 3;
    frame.ccso[1].filter_offset[16] = -4;
    controls.plane[1] = control;
    controls.stride_units[1] = 2U;
    controls.columns = 2U;
    controls.rows = 1U;
    CHECK(av2h_ccso_filter_frame(
              &planes, reference, 128U, &frame, &controls) == AV2H_OK);
    for (row = 0U; row < 32U; ++row) {
        for (column = 0U; column < 64U; ++column) {
            CHECK(u[row * 64U + column] ==
                  (column < 32U ? 103U : 96U));
        }
    }
    av2h_memory_fill(u, 100U, sizeof(u));
    control[1] = 0U;
    CHECK(av2h_ccso_filter_frame(
              &planes, reference, 128U, &frame, &controls) == AV2H_OK);
    for (row = 0U; row < 32U; ++row) {
        for (column = 0U; column < 64U; ++column) {
            CHECK(u[row * 64U + column] ==
                  (column < 32U ? 103U : 100U));
        }
    }
}

static void test_edge_classes(void) {
    uint8_t y[64];
    uint8_t reference[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t control = 1U;
    Av2hFilterPlanes planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    size_t index;
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    for (index = 0U; index < 64U; ++index) {
        y[index] = 100U;
        reference[index] = 100U;
    }
    for (index = 0U; index < 16U; ++index) {
        u[index] = 0U;
        v[index] = 0U;
    }
    reference[0] = 50U;
    initialize_planes(&planes, y, u, v);
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[0].enabled = 1U;
    frame.ccso[0].extended_filter_support = 1U;
    frame.ccso[0].offset_count = 9U;
    frame.ccso[0].filter_offset[1] = -7;
    frame.ccso[0].filter_offset[6] = 5;
    controls.plane[0] = &control;
    controls.stride_units[0] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
    CHECK(av2h_ccso_filter_frame(&planes, reference, 8U, &frame,
                                  &controls) == AV2H_OK);
    CHECK(y[0] == 105U);
    CHECK(y[1] == 93U);
    CHECK(y[2] == 100U);
}

static void test_ccso_validation(void) {
    uint8_t y[64];
    uint8_t reference[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t control = 1U;
    Av2hFilterPlanes planes;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    av2h_memory_fill(y, 100U, sizeof(y));
    av2h_memory_fill(reference, 100U, sizeof(reference));
    av2h_memory_fill(u, 100U, sizeof(u));
    av2h_memory_fill(v, 100U, sizeof(v));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    initialize_planes(&planes, y, u, v);
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[0].enabled = 1U;
    frame.ccso[0].band_only = 1U;
    frame.ccso[0].filter_offset[0] = 3;
    controls.plane[0] = &control;
    controls.stride_units[0] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;

    CHECK(av2h_ccso_filter_frame(
              &planes, reference, 8U, &frame, &controls) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 100U);
    frame.ccso[0].offset_count = 1U;
    control = 2U;
    CHECK(av2h_ccso_filter_frame(
              &planes, reference, 8U, &frame, &controls) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 100U);
    control = 1U;
    frame.ccso[0].band_only = 2U;
    CHECK(av2h_ccso_filter_frame(
              &planes, reference, 8U, &frame, &controls) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 100U);
    frame.ccso[0].band_only = 0U;
    frame.ccso[0].max_band_log2 = 4U;
    frame.ccso[0].offset_count = 144U;
    CHECK(av2h_ccso_filter_frame(
              &planes, reference, 8U, &frame, &controls) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 100U);
}

static void test_fail_closed(void) {
    uint8_t y[64];
    uint8_t u[16];
    uint8_t v[16];
    uint8_t cdef_strength = 1U;
    uint8_t deblocked_y[64];
    uint8_t deblocked_u[16];
    uint8_t deblocked_v[16];
    uint8_t cdef_y[64];
    uint8_t cdef_u[16];
    uint8_t cdef_v[16];
    uint8_t control = 1U;
    Av2hDeblockCell y_cells[4];
    Av2hDeblockCell u_cell;
    Av2hDeblockCell v_cell;
    Av2hFilterPlanes planes;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hCcsoControlMap controls;
    Av2hFilterWorkspace workspace;
    size_t index;
    av2h_memory_fill(y, 91U, sizeof(y));
    av2h_memory_fill(u, 92U, sizeof(u));
    av2h_memory_fill(v, 93U, sizeof(v));
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    av2h_memory_fill(&frame, 0U, sizeof(frame));
    av2h_memory_fill(&controls, 0U, sizeof(controls));
    av2h_memory_fill(&workspace, 0U, sizeof(workspace));
    initialize_planes(&planes, y, u, v);
    initialize_planes(
        &workspace.deblocked, deblocked_y, deblocked_u, deblocked_v);
    initialize_planes(&workspace.cdef, cdef_y, cdef_u, cdef_v);
    for (index = 0U; index < 4U; ++index) {
        uint16_t row = (uint16_t)(index / 2U);
        uint16_t column = (uint16_t)(index % 2U);
        y_cells[index] = (Av2hDeblockCell){
            column, row, column, row, 4U, 4U, 128U, 0U, 0U
        };
    }
    u_cell = (Av2hDeblockCell){ 0U, 0U, 0U, 0U,
                                4U, 4U, 128U, 0U, 0U };
    v_cell = u_cell;
    workspace.deblock_map.plane[0] = y_cells;
    workspace.deblock_map.plane[1] = &u_cell;
    workspace.deblock_map.plane[2] = &v_cell;
    workspace.deblock_map.stride_cells[0] = 2U;
    workspace.deblock_map.stride_cells[1] = 1U;
    workspace.deblock_map.stride_cells[2] = 1U;
    workspace.deblock_map.columns[0] = 2U;
    workspace.deblock_map.columns[1] = 1U;
    workspace.deblock_map.columns[2] = 1U;
    workspace.deblock_map.rows[0] = 2U;
    workspace.deblock_map.rows[1] = 1U;
    workspace.deblock_map.rows[2] = 1U;
    frame.width = 8U;
    frame.height = 8U;
    frame.ccso[0].enabled = 1U;
    frame.ccso[0].band_only = 1U;
    frame.ccso[0].offset_count = 1U;
    frame.ccso[0].filter_offset[0] = 9;
    controls.plane[0] = &control;
    controls.stride_units[0] = 1U;
    controls.columns = 1U;
    controls.rows = 1U;
    workspace.ccso_controls = controls;
    sequence.enable_ccso = 1U;

    sequence.enable_restoration = 1U;
    CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
          AV2H_UNSUPPORTED);
    CHECK(y[0] == 91U);
    sequence.enable_restoration = 0U;
    sequence.enable_cdef = 1U;
    frame.cdef_strength_count = 2U;
    frame.cdef_on_skip_transform = 1U;
    frame.cdef_damping = 5U;
    workspace.cdef_controls.strengths = &cdef_strength;
    workspace.cdef_controls.columns = 1U;
    workspace.cdef_controls.rows = 1U;
    CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
            AV2H_OK);
        CHECK(y[0] == 100U);
        av2h_memory_fill(y, 91U, sizeof(y));
    sequence.enable_cdef = 0U;
    frame.cdef_strength_count = 1U;
    frame.cdef_on_skip_transform = 1U;
    frame.cdef_damping = 5U;
    CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 91U);
    frame.cdef_strength_count = 0U;
    sequence.enable_ccso = 0U;
    CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 91U);
    sequence.enable_ccso = 1U;
    CHECK(av2h_filter_frame(&planes, &sequence, &frame, &workspace) ==
          AV2H_OK);
    CHECK(y[0] == 100U);
    y[0] = 91U;
    controls.columns = 2U;
    CHECK(av2h_ccso_filter_frame(&planes, y, 8U, &frame,
                                  &controls) == AV2H_INVALID_ARGUMENT);
    CHECK(y[0] == 91U);
}

int main(void) {
    test_workspace_model();
    test_wide_q_index_handoff();
    test_quantizer_threshold_domains();
    test_frame_decode_workspace_model();
    test_frame_workspace_prepare_rejection();
    test_filter_pipeline();
    test_disabled_filters_no_op();
    test_visible_i420_crop();
    test_deblock_edge();
    test_deblock_edge_u16();
    test_deblock_asymmetric_lengths();
    test_deblock_frame();
    test_deblock_frame_u16();
    test_deblock_stage_u16_odd_noop();
    test_deblock_skip_and_lossless();
    test_i444_deblock_superblock_cadence();
    test_cdef();
    test_cdef_u16();
    test_cdef_u16_odd_geometry();
    test_cdef_rounding_and_parameters();
    test_cdef_frame();
    test_gdf_frame();
    test_gdf_frame_u16();
    test_gdf_stripe_boundary_sources();
    test_gdf_control_row_offset();
    test_band_only_and_control();
    test_ccso_u16();
    test_ccso_multi_unit_chroma();
    test_edge_classes();
    test_ccso_validation();
    test_fail_closed();
    if (failures != 0) {
        fprintf(stderr, "%d filter checks failed\n", failures);
        return 1;
    }
    puts("filter unit: pass");
    return 0;
}