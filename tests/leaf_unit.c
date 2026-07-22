#include "decoder/leaf.h"

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

static void check_fixture(const char *name,
                          const unsigned char *bytes,
                          size_t size,
                          uint16_t width,
                          uint16_t height,
                          Av2hSequenceState sequence,
                          Av2hFrameState frame,
                          Av2hBlockSize expected_leaf,
                          uint8_t expected_ccso_mask,
                          uint8_t expected_mode_index,
                          Av2hFirstLeafIntraMode expected_mode,
                          size_t expected_before_mode,
                          size_t expected_after_set,
                          size_t expected_after_index,
                          uint8_t expected_fsc_context) {
    Av2hSpan span = { bytes, size };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs partition_cdfs;
    Av2hLeafCdfs leaf_cdfs;
    Av2hFirstLeafPath path;
    Av2hFirstLeafMode mode;
    Av2hStatus status;
    size_t plane;
    CHECK(av2h_partition_cdfs_init(&partition_cdfs) == AV2H_OK);
    CHECK(av2h_leaf_cdfs_init(&leaf_cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_first_leaf_read(
              &decoder, &partition_cdfs, width, height, 1U, &path) == AV2H_OK);
    CHECK(path.leaf.block_size == expected_leaf);
    status = av2h_first_leaf_mode_read(
        &decoder, &leaf_cdfs, &sequence, &frame, &path.leaf, &mode);
    if (status != AV2H_OK) {
        fprintf(stderr, "%s: leaf mode status=%d tell=%zu\n", name, status,
                av2h_range_tell(&decoder));
        ++failures;
        return;
    }
    CHECK(mode.use_intrabc == 0U);
    CHECK(mode.ccso_present_mask == expected_ccso_mask);
    CHECK(mode.mode_set == 0U);
    CHECK(mode.mode_index == expected_mode_index);
    CHECK(mode.mode == expected_mode);
    CHECK(mode.fsc_mode == 0U);
    CHECK(mode.fsc_block_context == expected_fsc_context);
    if (mode.before_mode_bits != expected_before_mode ||
        mode.after_mode_set_bits != expected_after_set ||
        mode.after_mode_index_bits != expected_after_index) {
        fprintf(stderr, "%s: mode tells %zu, %zu, %zu\n", name,
                mode.before_mode_bits, mode.after_mode_set_bits,
                mode.after_mode_index_bits);
    }
        CHECK(mode.before_mode_bits == expected_before_mode);
        CHECK(mode.after_mode_set_bits == expected_after_set);
        CHECK(mode.after_mode_index_bits == expected_after_index);
        CHECK(mode.after_fsc_bits == expected_after_index);
        CHECK(leaf_cdfs.intrabc[0][2] ==
                    (frame.allow_intrabc && expected_leaf != AV2H_BLOCK_64X64 ? 1U : 0U));
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        CHECK(leaf_cdfs.ccso[plane][0][2] ==
              ((expected_ccso_mask >> plane) & 1U));
        CHECK(mode.ccso_block_control[plane] ==
              ((expected_ccso_mask >> plane) & 1U));
    }
    CHECK(leaf_cdfs.y_mode_set[AV2H_Y_MODE_SET_SYMBOLS] == 1U);
    CHECK(leaf_cdfs.y_mode_index[0][AV2H_Y_MODE_INDEX_SYMBOLS] == 1U);
    CHECK(leaf_cdfs.y_mode_offset[0][AV2H_Y_MODE_OFFSET_SYMBOLS] == 0U);
    CHECK(leaf_cdfs.fsc_mode[0][expected_fsc_context][2] == 1U);
}

static void test_fixture_modes(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U
    };
    static const unsigned char odd[] = {
        0xf3U, 0x99U, 0x9cU, 0x59U, 0x42U, 0xfbU, 0xe6U, 0x50U,
        0x15U, 0x7dU, 0x6eU, 0x92U, 0x8aU, 0x57U, 0x4aU, 0x49U
    };
    static const unsigned char ordinary[] = {
        0xcdU, 0x77U, 0xc5U, 0x58U, 0x5fU, 0x4dU, 0x54U, 0x7fU,
        0xf3U, 0x9eU, 0xfcU, 0x9cU, 0x09U, 0x95U, 0xb7U, 0x37U
    };
    static const unsigned char detail[] = {
        0xc7U, 0x43U, 0xf5U, 0xb7U, 0xcbU, 0xdeU, 0xd0U, 0x78U,
        0xe8U, 0xfdU, 0x2dU, 0x87U, 0x60U, 0xdaU, 0x52U, 0x31U
    };
    Av2hFrameState tiny_frame = { 0 };
    Av2hFrameState odd_frame = { 0 };
    Av2hFrameState ordinary_frame = { 0 };
    Av2hFrameState detail_frame = { 0 };
    Av2hSequenceState sequence = { 0 };
    sequence.enable_sdp = 1U;
    sequence.enable_idtx_intra = 1U;
    tiny_frame.base_q_index = 128U;
    tiny_frame.cdef_strength_count = 1U;
    odd_frame.base_q_index = 128U;
    odd_frame.allow_intrabc = 1U;
    odd_frame.gdf_mode = 1U;
    odd_frame.cdef_strength_count = 1U;
    odd_frame.ccso[1].enabled = 1U;
    odd_frame.ccso[2].enabled = 1U;
    ordinary_frame.base_q_index = 128U;
    ordinary_frame.allow_intrabc = 1U;
    ordinary_frame.cdef_strength_count = 1U;
    ordinary_frame.ccso[0].enabled = 1U;
    detail_frame.base_q_index = 128U;
    detail_frame.allow_intrabc = 1U;
    detail_frame.ccso[0].enabled = 1U;
    check_fixture("tiny", tiny, sizeof(tiny), 16U, 16U, sequence,
                  tiny_frame, AV2H_BLOCK_8X4, 0U, 0U, AV2H_INTRA_DC,
                  11U, 11U, 12U, 1U);
    check_fixture("odd", odd, sizeof(odd), 17U, 15U, sequence, odd_frame,
                  AV2H_BLOCK_8X4, 6U, 0U, AV2H_INTRA_DC,
                  17U, 17U, 18U, 1U);
    check_fixture("ordinary", ordinary, sizeof(ordinary), 160U, 120U,
                  sequence, ordinary_frame, AV2H_BLOCK_32X32, 1U, 0U,
                  AV2H_INTRA_DC, 8U, 8U, 9U, 5U);
    check_fixture("high-detail", detail, sizeof(detail), 96U, 80U,
                  sequence, detail_frame, AV2H_BLOCK_8X8, 1U, 4U,
                  AV2H_INTRA_PAETH, 18U, 18U, 22U, 2U);
}

static void test_arguments(void) {
    Av2hRangeDecoder decoder;
    Av2hLeafCdfs cdfs;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_8X8 };
    Av2hFirstLeafMode mode;
    CHECK(sizeof(Av2hLeafCdfs) == 616U);
    CHECK(av2h_leaf_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_first_leaf_mode_read(
              0, &cdfs, &sequence, &frame, &leaf, &mode) ==
          AV2H_INVALID_ARGUMENT);
    frame.base_q_index = 128U;
    frame.disable_cdf_update = 1U;
        CHECK(av2h_first_leaf_mode_read(
              &decoder, &cdfs, &sequence, &frame, &leaf, &mode) ==
            AV2H_UNSUPPORTED);
}

int main(void) {
    test_fixture_modes();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d leaf checks failed\n", failures);
        return 1;
    }
    puts("leaf unit: pass");
    return 0;
}