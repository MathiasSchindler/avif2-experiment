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
    sequence->superblock_size = 64U;
    frame->width = width;
    frame->height = height;
    frame->chroma_format_idc = chroma_format_idc;
}

static size_t stage_sample_count(const Av2hFilterWorkspaceLayout *layout) {
    return layout->stage_plane_bytes[0] + layout->stage_plane_bytes[1] +
           layout->stage_plane_bytes[2];
}

static void test_storage_width_accounting(uint8_t chroma_format_idc) {
    static _Alignas(max_align_t) uint8_t memory[128U * 1024U];
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout filter_u8;
    Av2hFilterWorkspaceLayout filter_u16;
    Av2hFrameDecodeWorkspaceLayout workspace_u8;
    Av2hFrameDecodeWorkspaceLayout workspace_u16;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    size_t sample_count;
    size_t coded_plane_alignment_padding;
    initialize(&sequence, &frame, 17U, 15U, chroma_format_idc);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, 1U, &filter_u8) == AV2H_OK);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, 2U, &filter_u16) == AV2H_OK);
    sample_count = stage_sample_count(&filter_u8);
    CHECK(stage_sample_count(&filter_u16) == 2U * sample_count);
    CHECK(filter_u16.size == filter_u8.size + 2U * sample_count);
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
    CHECK(workspace_u16.size == workspace_u8.size +
          3U * sample_count + AV2H_RECON_SAMPLE_CAPACITY + 4U * 128U +
          coded_plane_alignment_padding);
    CHECK(workspace_u16.alignment == workspace_u8.alignment);
    CHECK(workspace_u8.size != 0U);
        CHECK(workspace_u16.size <= sizeof(memory));
        CHECK(av2h_frame_decode_workspace_init(
              &sequence, &frame, &workspace_u8, memory, sizeof(memory),
              &workspace, &filter_workspace, &filter_storage) == AV2H_OK);
        CHECK(workspace.coded[0].reconstructed_stride >=
            ((size_t)workspace.coded[0].width + 3U) / 4U);
        CHECK(workspace.coded[0].reconstructed_count >=
            workspace.coded[0].reconstructed_stride *
            (((size_t)workspace.coded[0].height + 3U) / 4U));
        av2h_memory_fill(&workspace, 0xa5U, sizeof(workspace));
        av2h_memory_fill(&filter_workspace, 0xa5U, sizeof(filter_workspace));
        av2h_memory_fill(&filter_storage, 0xa5U, sizeof(filter_storage));
        CHECK(av2h_frame_decode_workspace_init(
              &sequence, &frame, &workspace_u16, memory, sizeof(memory),
              &workspace, &filter_workspace, &filter_storage) ==
            AV2H_INVALID_ARGUMENT);
        CHECK(((const uint8_t *)&workspace)[0] == 0xa5U);
        CHECK(((const uint8_t *)&filter_workspace)[0] == 0xa5U);
        CHECK(((const uint8_t *)&filter_storage)[0] == 0xa5U);
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
        CHECK(layout_u16.size > layout_u8.size);
        CHECK(layout_u16.size > limits.max_workspace_size);
        CHECK(layout_u16.alignment == layout_u8.alignment);
    }
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
    test_maximum_workspace();
    test_invalid_and_overflow();
    if (failures != 0) return 1;
    puts("workspace accounting unit: pass");
    return 0;
}