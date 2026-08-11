#include "decoder/internal/frame_workspace.h"

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

static void initialize(Av2hSequenceState *sequence,
                       Av2hFrameState *frame,
                       uint16_t width,
                       uint16_t height,
                       uint8_t chroma_format_idc) {
    av2h_memory_fill(sequence, 0U, sizeof(*sequence));
    av2h_memory_fill(frame, 0U, sizeof(*frame));
    sequence->info.bit_depth_idc = 1U;
    sequence->superblock_size = 64U;
    frame->width = width;
    frame->height = height;
    frame->plane_count = chroma_format_idc == 1U ? 1U : 3U;
    frame->chroma_format_idc = chroma_format_idc;
}

static size_t stage_sample_count(const Av2hFilterWorkspaceLayout *layout) {
    return layout->stage_plane_bytes[0] + layout->stage_plane_bytes[1] +
           layout->stage_plane_bytes[2];
}

static void test_storage_width_accounting(uint8_t chroma_format_idc) {
    static _Alignas(max_align_t) uint8_t storage_memory[
        128U * 1024U + 2U * _Alignof(max_align_t)];
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout filter_u8;
    Av2hFilterWorkspaceLayout filter_u16;
    Av2hFrameDecodeWorkspaceLayout workspace_u8;
    Av2hFrameDecodeWorkspaceLayout workspace_u16;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hPlanarOutputU16 output_planes;
    static _Alignas(uint16_t) uint8_t output_memory[2048U];
    size_t output_size;
    size_t sample_count;
    size_t coded_plane_alignment_padding;
    uint8_t *memory = storage_memory + _Alignof(max_align_t);
    unsigned int plane;
    initialize(&sequence, &frame, 17U, 15U, chroma_format_idc);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, 1U, &filter_u8) == AV2H_OK);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, 2U, &filter_u16) == AV2H_OK);
    sample_count = stage_sample_count(&filter_u8);
    CHECK(stage_sample_count(&filter_u16) == 2U * sample_count);
        CHECK(filter_u16.size_bytes ==
            filter_u8.size_bytes + 2U * sample_count);
    CHECK(filter_u16.alignment == filter_u8.alignment);
    CHECK(filter_u16.deblock_cell_count[0] ==
          filter_u8.deblock_cell_count[0]);
    CHECK(filter_u16.deblock_cell_count[1] ==
          filter_u8.deblock_cell_count[1]);
    CHECK(filter_u16.deblock_cell_count[2] ==
          filter_u8.deblock_cell_count[2]);
    CHECK(filter_u16.ccso_control_count == filter_u8.ccso_control_count);

    CHECK(av2h_frame_decode_workspace_query_bytes(
              &sequence, &frame, 1U, &workspace_u8) == AV2H_OK);
    CHECK(av2h_frame_decode_workspace_query_bytes(
              &sequence, &frame, 2U, &workspace_u16) == AV2H_OK);
    coded_plane_alignment_padding = av2h_frame_plane_count(&frame);
    CHECK(workspace_u16.size_bytes == workspace_u8.size_bytes +
          3U * sample_count + AV2H_RECON_SAMPLE_CAPACITY + 4U * 128U +
          coded_plane_alignment_padding);
    CHECK(workspace_u16.alignment == workspace_u8.alignment);
    CHECK(workspace_u8.size_bytes != 0U);
          CHECK(workspace_u16.size_bytes <=
              sizeof(storage_memory) - 2U * _Alignof(max_align_t));
        CHECK(av2h_frame_decode_workspace_init(
              &sequence, &frame, &workspace_u8, memory,
              workspace_u8.size_bytes,
              &workspace, &filter_workspace, &filter_storage) == AV2H_OK);
          CHECK(workspace.bytes_per_sample == 1U);
          CHECK(workspace.reconstruction_scratch != 0);
          CHECK(workspace.reconstruction_scratch_u16 == 0);
        CHECK(workspace.coded[0].reconstructed_stride_cells >=
            ((size_t)workspace.coded[0].width + 3U) / 4U);
            CHECK(workspace.coded[0].reconstructed_count_cells >=
                workspace.coded[0].reconstructed_stride_cells *
            (((size_t)workspace.coded[0].height + 3U) / 4U));
        av2h_memory_fill(&workspace, 0xa5U, sizeof(workspace));
        av2h_memory_fill(&filter_workspace, 0xa5U, sizeof(filter_workspace));
        av2h_memory_fill(&filter_storage, 0xa5U, sizeof(filter_storage));
        CHECK(av2h_frame_decode_workspace_init(
              &sequence, &frame, &workspace_u16, memory,
              workspace_u16.size_bytes,
              &workspace, &filter_workspace, &filter_storage) ==
            AV2H_INVALID_ARGUMENT);
        CHECK(((const uint8_t *)&workspace)[0] == 0xa5U);
        CHECK(((const uint8_t *)&filter_workspace)[0] == 0xa5U);
        CHECK(((const uint8_t *)&filter_storage)[0] == 0xa5U);
        sequence.info.bit_depth_idc = 0U;
        av2h_memory_fill(storage_memory, 0xa5U, sizeof(storage_memory));
        CHECK(av2h_frame_decode_workspace_init_bytes(
              &sequence, &frame, 2U, &workspace_u16, memory,
              workspace_u16.size_bytes,
              &workspace, &filter_workspace, &filter_storage) ==
            AV2H_OK);
        CHECK(storage_memory[_Alignof(max_align_t) - 1U] == 0xa5U);
        CHECK(memory[workspace_u16.size_bytes] == 0xa5U);
        CHECK(workspace.bytes_per_sample == 2U);
        CHECK(workspace.reconstruction_scratch == 0);
        CHECK(workspace.reconstruction_scratch_u16 != 0);
        CHECK(((uintptr_t)workspace.reconstruction_scratch_u16 & 1U) == 0U);
        for (plane = 0U; plane < av2h_frame_plane_count(&frame); ++plane) {
            CHECK(workspace.coded[plane].data == 0);
            CHECK(workspace.coded_u16[plane].data != 0);
            CHECK(((uintptr_t)workspace.coded_u16[plane].data & 1U) == 0U);
            CHECK(workspace.coded_u16[plane].stride_samples ==
                  filter_u16.plane_width[plane]);
            CHECK(workspace.coded_u16[plane].reconstructed != 0);
            CHECK(sizeof(*workspace.coded_u16[plane].reconstructed) == 1U);
            CHECK(filter_workspace.deblocked.plane[plane].data == 0);
            CHECK(filter_workspace.cdef.plane[plane].data == 0);
            CHECK(filter_workspace.deblocked_u16.plane[plane].data != 0);
            CHECK(filter_workspace.cdef_u16.plane[plane].data != 0);
            CHECK(((uintptr_t)
                  filter_workspace.deblocked_u16.plane[plane].data & 1U) ==
                  0U);
            CHECK(((uintptr_t)
                  filter_workspace.cdef_u16.plane[plane].data & 1U) == 0U);
            CHECK(filter_storage.deblock_cells[plane] != 0);
            CHECK(filter_storage.ccso_controls[plane] != 0);
            CHECK(sizeof(*filter_storage.ccso_controls[plane]) == 1U);
        }
        CHECK(av2h_planar_output_size_bytes(
                  &frame, 2U, &output_size) == AV2H_OK);
        CHECK(output_size + 2U <= sizeof(output_memory));
        av2h_memory_fill(output_memory, 0xa5U, sizeof(output_memory));
        CHECK(av2h_planar_output_bind_u16(
                  &frame, output_memory, output_size,
                  &output_planes) == AV2H_OK);
        CHECK(output_planes.plane[0].data == (uint16_t *)output_memory);
        CHECK(output_planes.plane[0].stride_samples == frame.width);
        CHECK(output_planes.plane[0].width == frame.width);
        CHECK(output_planes.plane[0].height == frame.height);
        if (chroma_format_idc == 1U) {
            CHECK(output_planes.plane[1].data == 0);
            CHECK(output_planes.plane[2].data == 0);
        } else {
            size_t chroma_width = av2h_frame_plane_dimension(
                &frame, 1U, frame.width);
            size_t chroma_height = av2h_frame_plane_dimension(
                &frame, 1U, frame.height);
            CHECK((uint8_t *)output_planes.plane[1].data ==
                  output_memory + (size_t)frame.width * frame.height * 2U);
            CHECK(output_planes.plane[1].stride_samples == chroma_width);
            CHECK(output_planes.plane[1].width == chroma_width);
            CHECK(output_planes.plane[1].height == chroma_height);
            CHECK((uint8_t *)output_planes.plane[2].data ==
                  (uint8_t *)output_planes.plane[1].data +
                  chroma_width * chroma_height * 2U);
        }
        av2h_memory_fill(&output_planes, 0xa5U, sizeof(output_planes));
        CHECK(av2h_planar_output_bind_u16(
                  &frame, output_memory, output_size - 1U,
                  &output_planes) == AV2H_OUT_OF_MEMORY);
        CHECK(((const uint8_t *)&output_planes)[0] == 0xa5U);
        CHECK(av2h_planar_output_bind_u16(
                  &frame, output_memory + 1U, output_size,
                  &output_planes) == AV2H_INVALID_ARGUMENT);
        CHECK(((const uint8_t *)&output_planes)[0] == 0xa5U);

#define CHECK_U16_INIT_FAILURE(expected, pointer, size, candidate_layout)     \
    do {                                                                       \
        av2h_memory_fill(&workspace, 0xa5U, sizeof(workspace));                \
        av2h_memory_fill(                                                       \
            &filter_workspace, 0xa5U, sizeof(filter_workspace));              \
        av2h_memory_fill(&filter_storage, 0xa5U, sizeof(filter_storage));      \
        CHECK(av2h_frame_decode_workspace_init_bytes(                          \
              &sequence, &frame, 2U, (candidate_layout), (pointer), (size),   \
              &workspace, &filter_workspace, &filter_storage) == (expected)); \
        CHECK(((const uint8_t *)&workspace)[0] == 0xa5U);                     \
        CHECK(((const uint8_t *)&filter_workspace)[0] == 0xa5U);              \
        CHECK(((const uint8_t *)&filter_storage)[0] == 0xa5U);                \
    } while (0)
        CHECK_U16_INIT_FAILURE(
            AV2H_OUT_OF_MEMORY, memory, workspace_u16.size_bytes - 1U,
            &workspace_u16);
        CHECK_U16_INIT_FAILURE(
            AV2H_INVALID_ARGUMENT, memory + 1U, workspace_u16.size_bytes,
            &workspace_u16);
        CHECK_U16_INIT_FAILURE(
            AV2H_INVALID_ARGUMENT, memory, workspace_u16.size_bytes,
            &workspace_u8);
#undef CHECK_U16_INIT_FAILURE
}

static void test_maximum_workspace(void) {
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFrameDecodeWorkspaceLayout layout_u8;
    Av2hFrameDecodeWorkspaceLayout layout_u16;
    Av2hLimits limits;
    uint8_t chroma_format_idc;
    av2h_limits_default(&limits);
    for (chroma_format_idc = 0U; chroma_format_idc <= 2U;
         ++chroma_format_idc) {
        initialize(&sequence, &frame, 16384U, 16384U, chroma_format_idc);
        CHECK(av2h_frame_decode_workspace_query_bytes(
                  &sequence, &frame, 1U, &layout_u8) == AV2H_OK);
        CHECK(av2h_frame_decode_workspace_query_bytes(
                  &sequence, &frame, 2U, &layout_u16) == AV2H_OK);
        CHECK(layout_u16.size_bytes > layout_u8.size_bytes);
        CHECK(layout_u16.size_bytes > limits.max_workspace_size);
        CHECK(layout_u16.alignment == layout_u8.alignment);
    }
}

static void test_odd_u16_visible_crop(void) {
    uint16_t y[26U * 16U];
    uint16_t u[14U * 8U];
    uint16_t v[14U * 8U];
    uint8_t output[802U];
    size_t capacities[AV2H_FRAME_PLANE_COUNT] = {
        sizeof(y), sizeof(u), sizeof(v)
    };
    Av2hReconstructionPlaneU16 coded[AV2H_FRAME_PLANE_COUNT] = {
        { y, 26U, 24U, 16U, 24U, 16U, 0, 0U, 0U },
        { u, 14U, 12U, 8U, 12U, 8U, 0, 0U, 0U },
        { v, 14U, 12U, 8U, 12U, 8U, 0, 0U, 0U }
    };
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    size_t expected_size;
    size_t offset;
    size_t row;
    initialize(&sequence, &frame, 17U, 15U, 0U);
    (void)sequence;
    av2h_memory_fill(y, 0xeeU, sizeof(y));
    av2h_memory_fill(u, 0xeeU, sizeof(u));
    av2h_memory_fill(v, 0xeeU, sizeof(v));
    for (row = 0U; row < 15U; ++row) {
        size_t column;
        for (column = 0U; column < 17U; ++column) {
            y[row * 26U + column] = (uint16_t)(256U + row * 17U + column);
        }
    }
    for (row = 0U; row < 8U; ++row) {
        size_t column;
        for (column = 0U; column < 9U; ++column) {
            u[row * 14U + column] = (uint16_t)(600U + row * 9U + column);
            v[row * 14U + column] = (uint16_t)(800U + row * 9U + column);
        }
    }
    CHECK(av2h_planar_output_size_bytes(
              &frame, 2U, &expected_size) == AV2H_OK);
    CHECK(expected_size == 798U);
    av2h_memory_fill(output, 0xa5U, sizeof(output));
    CHECK(av2h_copy_visible_planar_u16_private(
              &frame, coded, capacities, output,
              expected_size - 1U) == AV2H_OUT_OF_MEMORY);
    for (offset = 0U; offset < sizeof(output); ++offset) {
        CHECK(output[offset] == 0xa5U);
    }
    CHECK(av2h_copy_visible_planar_u16_private(
              &frame, coded, capacities, output,
              sizeof(output)) == AV2H_OK);
    offset = 0U;
#define CHECK_PLANE_BYTES(samples, stride, width, height)                     \
    do {                                                                       \
        size_t crop_row;                                                       \
        for (crop_row = 0U; crop_row < (height); ++crop_row) {                \
            size_t crop_column;                                                \
            for (crop_column = 0U; crop_column < (width); ++crop_column) {    \
                uint16_t sample = (samples)[                                  \
                    crop_row * (stride) + crop_column];                       \
                CHECK(output[offset++] == (uint8_t)sample);                   \
                CHECK(output[offset++] == (uint8_t)(sample >> 8U));           \
            }                                                                  \
        }                                                                      \
    } while (0)
    CHECK_PLANE_BYTES(y, 26U, 17U, 15U);
    CHECK(offset == 510U);
    CHECK_PLANE_BYTES(u, 14U, 9U, 8U);
    CHECK(offset == 654U);
    CHECK_PLANE_BYTES(v, 14U, 9U, 8U);
#undef CHECK_PLANE_BYTES
    CHECK(offset == expected_size);
    while (offset < sizeof(output)) CHECK(output[offset++] == 0xa5U);
}

static void test_invalid_and_overflow(void) {
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout filter;
    Av2hFrameDecodeWorkspaceLayout workspace;
    initialize(&sequence, &frame, 17U, 15U, 0U);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, 0U, &filter) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, 3U, &filter) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_frame_decode_workspace_query_bytes(
              &sequence, &frame, 0U, &workspace) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_frame_decode_workspace_query_bytes(
              &sequence, &frame, 3U, &workspace) == AV2H_INVALID_ARGUMENT);
    frame.width = UINT16_MAX;
    CHECK(av2h_frame_decode_workspace_query_bytes(
              &sequence, &frame, 1U, &workspace) == AV2H_OVERFLOW);
    CHECK(av2h_frame_decode_workspace_query_bytes(
              &sequence, &frame, 2U, &workspace) == AV2H_OVERFLOW);
}

int main(void) {
    test_storage_width_accounting(0U);
    test_storage_width_accounting(1U);
    test_storage_width_accounting(2U);
    test_odd_u16_visible_crop();
    test_maximum_workspace();
    test_invalid_and_overflow();
    if (failures != 0) return 1;
    puts("workspace accounting unit: pass");
    return 0;
}