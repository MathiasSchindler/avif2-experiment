#include "decoder/reconstruct.h"

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

static void test_tiny_second_luma_leaf(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U,
        0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U, 0x35U,
        0x9cU, 0x23U, 0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU, 0x83U,
        0x17U, 0x25U, 0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU, 0xdbU,
        0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU, 0xf5U,
        0x35U, 0x75U, 0xb2U, 0xd6U, 0xd4U, 0xc1U, 0x5bU, 0xe2U,
        0xc6U, 0x99U, 0x46U, 0x47U, 0x4eU, 0x59U, 0x14U, 0xa2U,
        0xe0U, 0x35U, 0x23U, 0x50U, 0x71U, 0x5bU, 0xc0U, 0x5dU,
        0x24U, 0x8bU, 0xa5U, 0x4bU, 0xcbU, 0x90U, 0xa9U, 0x75U,
        0xfcU, 0xb0U, 0xbaU, 0x59U, 0xf2U, 0x43U, 0xc5U, 0x73U,
        0x44U, 0xeeU, 0xd8U
    };
    Av2hSpan span = { tiny, sizeof(tiny) };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs partition_cdfs;
    Av2hLeafCdfs leaf_cdfs;
    Av2hFirstCoeffCdfs coeff_cdfs;
    Av2hFirstTransformCdfs transform_cdfs;
    Av2hFirstCoeffLevelCdfs level_cdfs;
    Av2hFirstLeafPath path;
    Av2hFirstLeafMode first_mode;
    Av2hFirstChromaMode chroma_mode;
    Av2hFirstCoeffPrefix prefix;
    Av2hFirstTransformType transform;
    Av2hFirstCoeffBlock block;
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    Av2hPartitionContextState partition_state;
    Av2hPartitionTraversal luma_traversal;
    Av2hPartitionTraversal chroma_traversal;
    Av2hLumaModeContextState mode_state;
    Av2hCoeffContextState coeff_state;
    uint8_t luma_partition_above[16U];
    uint8_t luma_partition_left[16U];
    uint8_t chroma_partition_above[16U];
    uint8_t chroma_partition_left[16U];
    Av2hLumaModeContext mode_above[16U];
    Av2hLumaModeContext mode_left[16U];
    uint8_t coeff_above[AV2H_FRAME_PLANE_COUNT * 16U];
    uint8_t coeff_left[AV2H_FRAME_PLANE_COUNT * 16U];
    uint8_t dc_sign_context;
    uint8_t has_leaf;
    uint8_t left[4U];
    uint8_t above[16U];
    uint8_t second_above[16U];
    uint8_t lower_above[8U];
    uint8_t top_lower_above[8U];
    uint8_t final_above[4U];
    uint8_t final_left[4U];
    uint8_t predictor[64U];
    Av2hLumaReferences references = {
        .left = left,
        .left_count = 4U,
        .above = 0,
        .above_count = 0U,
        .above_left = 0U,
        .has_above_left = 0U
    };
    Av2hPartitionNode second_leaf = { 0U, 2U, AV2H_BLOCK_4X4 };
    Av2hPartitionNode third_leaf = { 0U, 3U, AV2H_BLOCK_4X4 };
    Av2hFirstLeafMode second_mode;
    sequence.enable_sdp = 1U;
    sequence.enable_idtx_intra = 1U;
    sequence.enable_ist = 1U;
    sequence.enable_mrls = 1U;
    sequence.enable_cfl_intra = 1U;
    frame.base_q_index = 128U;
    frame.tx_mode = 1U;
    frame.cdef_strength_count = 1U;
    CHECK(av2h_partition_cdfs_init(&partition_cdfs) == AV2H_OK);
    CHECK(av2h_leaf_cdfs_init(&leaf_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_cdfs_init(&coeff_cdfs) == AV2H_OK);
    CHECK(av2h_first_transform_cdfs_init(&transform_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_level_cdfs_init(&level_cdfs) == AV2H_OK);
    CHECK(av2h_partition_context_state_init(
              &partition_state, luma_partition_above, luma_partition_left,
              chroma_partition_above, chroma_partition_left, 16U, 16U) ==
          AV2H_OK);
    CHECK(av2h_luma_mode_context_state_init(
              &mode_state, mode_above, 16U, mode_left, 16U) == AV2H_OK);
    CHECK(av2h_coeff_context_state_init(
              &coeff_state, coeff_above, 16U, coeff_left, 16U) == AV2H_OK);
    CHECK(av2h_partition_traversal_init(
              &luma_traversal, 4U, 4U,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 }, 0U, 1U) ==
          AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs, &partition_state,
              &path, &has_leaf) == AV2H_OK);
    CHECK(has_leaf == 1U);
    CHECK(av2h_first_leaf_mode_read(
              &decoder, &leaf_cdfs, &sequence, &frame, &path.leaf,
              &first_mode) == AV2H_OK);
    CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &path.leaf, &first_mode) == AV2H_OK);
    CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &path.leaf, &first_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
    CHECK(dc_sign_context == 0U);
    CHECK(av2h_first_transform_type_read(
              &decoder, &transform_cdfs, &sequence, &frame, &first_mode,
              &prefix, &transform) == AV2H_OK);
    CHECK(av2h_coeff_block_read_with_state(
              &decoder, &level_cdfs, &sequence, &frame, &path.leaf,
              &first_mode, &prefix, &transform, &coeff_state, &block) ==
          AV2H_OK);
    CHECK(av2h_range_tell(&decoder) == 51U);
    CHECK(av2h_first_luma_reconstruct(
              &first_mode, &prefix, &transform, &block, &scratch,
              &reconstruction) == AV2H_OK);
    above[0] = reconstruction.pixels[24U];
    above[1] = reconstruction.pixels[25U];
    above[2] = reconstruction.pixels[26U];
    above[3] = reconstruction.pixels[27U];
    above[4] = reconstruction.pixels[28U];
    above[5] = reconstruction.pixels[29U];
    above[6] = reconstruction.pixels[30U];
    above[7] = reconstruction.pixels[31U];
    left[0] = reconstruction.pixels[7U];
    left[1] = reconstruction.pixels[15U];
    left[2] = reconstruction.pixels[23U];
    left[3] = reconstruction.pixels[31U];
    CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, path.leaf) == AV2H_OK);
    CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs, &partition_state,
              &path, &has_leaf) == AV2H_OK);
    CHECK(has_leaf == 1U);
    CHECK(path.leaf.row == second_leaf.row);
    CHECK(path.leaf.column == second_leaf.column);
    CHECK(path.leaf.block_size == second_leaf.block_size);
    CHECK(av2h_range_tell(&decoder) == 53U);
    CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &second_leaf,
              &mode_state, &second_mode) == AV2H_OK);
    CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &second_leaf, &second_mode) == AV2H_OK);
    CHECK(second_mode.before_mode_bits == 53U);
    CHECK(second_mode.mode_set == 0U);
    CHECK(second_mode.mode_index == 12U);
    CHECK(second_mode.joint_mode == 57U);
    CHECK(second_mode.mode == AV2H_INTRA_D203);
    CHECK(second_mode.angle_delta == 0);
    CHECK(second_mode.after_mode_index_bits == 59U);
    CHECK(second_mode.fsc_block_context == 0U);
    CHECK(second_mode.fsc_mode == 0U);
    CHECK(second_mode.after_fsc_bits == 59U);
    CHECK(second_mode.mrl_index == 0U);
    CHECK(second_mode.multi_line_mrl == 0U);
    CHECK(second_mode.after_mrl_bits == 59U);
    CHECK(leaf_cdfs.y_mode_set[AV2H_Y_MODE_SET_SYMBOLS] == 2U);
    CHECK(leaf_cdfs.y_mode_index[0][AV2H_Y_MODE_INDEX_SYMBOLS] == 2U);
    CHECK(leaf_cdfs.y_mode_offset[0][AV2H_Y_MODE_OFFSET_SYMBOLS] == 1U);
    CHECK(leaf_cdfs.fsc_mode[0][0][2] == 1U);
    CHECK(leaf_cdfs.mrl_index[0][AV2H_MRL_INDEX_SYMBOLS] == 1U);
    CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &second_leaf, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
    CHECK(prefix.tx_size == AV2H_TX_4X4);
    CHECK(prefix.tx_size_context == 0U);
    CHECK(prefix.txb_skip_context == 0U);
    CHECK(prefix.all_zero == 0U);
    CHECK(prefix.before_all_zero_bits == 59U);
    CHECK(prefix.after_all_zero_bits == 59U);
    CHECK(prefix.eob_point == 2U);
    CHECK(prefix.eob == 2U);
    CHECK(prefix.after_eob_bits == 64U);
    CHECK(av2h_first_transform_type_read(
              &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
              &prefix, &transform) == AV2H_OK);
    CHECK(transform.primary_signaled == 1U);
    CHECK(transform.primary_symbol == 3U);
    CHECK(transform.primary_type == AV2H_TX_DCT_ADST);
    CHECK(transform.before_primary_bits == 64U);
    CHECK(transform.after_primary_bits == 67U);
    CHECK(transform.after_secondary_bits == 67U);
    CHECK(av2h_coeff_block_read(
              &decoder, &level_cdfs, &sequence, &frame, &second_mode,
              &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
    CHECK(av2h_coeff_context_state_update(
              &coeff_state, 0U, &second_leaf, block.cul_level) == AV2H_OK);
    CHECK(block.width == 4U);
    CHECK(block.height == 4U);
    CHECK(block.nonzero_count == 2U);
    CHECK(block.cul_level == 21U);
    CHECK(block.dc_value == 4);
    CHECK(block.quantized[0] == 4);
    CHECK(block.quantized[4] == -1);
    CHECK(block.dequantized[0] == 784);
    CHECK(block.dequantized[4] == -196);
    CHECK(block.before_levels_bits == 67U);
    CHECK(block.after_levels_bits == 72U);
    CHECK(block.after_coefficients_bits == 74U);
    {
        static const uint8_t expected_predictor[] = {
            133U, 137U, 142U, 147U,
            145U, 151U, 156U, 160U,
            159U, 162U, 163U, 164U,
            164U, 164U, 163U, 163U
        };
        static const uint8_t pixels[] = {
            138U, 150U, 161U, 170U,
            151U, 167U, 181U, 189U,
            167U, 184U, 196U, 203U,
            173U, 189U, 201U, 208U
        };
        static const int16_t residual[] = {
            5, 13, 19, 23,
            6, 16, 25, 29,
            8, 22, 33, 39,
            9, 25, 38, 45
        };
        size_t index;
        CHECK(av2h_luma_predict(
              &second_mode, 4U, 4U, &references, predictor, 4U) == AV2H_OK);
        CHECK(av2h_luma_reconstruct(
                  &second_mode, &prefix, &transform, &block, predictor, 4U,
                  &scratch, &reconstruction) == AV2H_OK);
        CHECK(reconstruction.width == 4U);
        CHECK(reconstruction.height == 4U);
        for (index = 0U; index < 16U; ++index) {
            CHECK(predictor[index] == expected_predictor[index]);
            CHECK(reconstruction.residual[index] == residual[index]);
            CHECK(reconstruction.pixels[index] == pixels[index]);
        }
        above[8] = reconstruction.pixels[12U];
        above[9] = reconstruction.pixels[13U];
        above[10] = reconstruction.pixels[14U];
        above[11] = reconstruction.pixels[15U];
    }
    left[0] = reconstruction.pixels[3U];
    left[1] = reconstruction.pixels[7U];
    left[2] = reconstruction.pixels[11U];
    left[3] = reconstruction.pixels[15U];
    CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, second_leaf) == AV2H_OK);
    CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs, &partition_state,
              &path, &has_leaf) == AV2H_OK);
    CHECK(has_leaf == 1U);
    CHECK(path.leaf.row == third_leaf.row);
    CHECK(path.leaf.column == third_leaf.column);
    CHECK(path.leaf.block_size == third_leaf.block_size);
    CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &third_leaf,
              &mode_state, &second_mode) == AV2H_OK);
    CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &third_leaf, &second_mode) == AV2H_OK);
    CHECK(second_mode.before_mode_bits == 74U);
    CHECK(second_mode.mode_set == 0U);
    CHECK(second_mode.mode_index == 12U);
    CHECK(second_mode.joint_mode == 57U);
    CHECK(second_mode.mode == AV2H_INTRA_D203);
    CHECK(second_mode.angle_delta == 0);
    CHECK(second_mode.after_mode_index_bits == 79U);
    CHECK(second_mode.fsc_mode == 0U);
    CHECK(second_mode.mrl_index == 0U);
    CHECK(second_mode.after_mrl_bits == 80U);
    CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &third_leaf, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
    CHECK(prefix.tx_size == AV2H_TX_4X4);
    CHECK(prefix.txb_skip_context == 0U);
    CHECK(prefix.all_zero == 0U);
    CHECK(prefix.before_all_zero_bits == 80U);
    CHECK(prefix.eob_point == 5U);
    CHECK(prefix.eob == 16U);
    CHECK(prefix.after_eob_bits == 84U);
    CHECK(av2h_first_transform_type_read(
              &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
              &prefix, &transform) == AV2H_OK);
    CHECK(transform.primary_symbol == 6U);
    CHECK(transform.primary_type == AV2H_TX_H_ADST);
    CHECK(transform.after_primary_bits == 88U);
    CHECK(av2h_coeff_block_read(
              &decoder, &level_cdfs, &sequence, &frame, &second_mode,
              &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
    CHECK(av2h_coeff_context_state_update(
              &coeff_state, 0U, &third_leaf, block.cul_level) == AV2H_OK);
    CHECK(block.width == 4U);
    CHECK(block.height == 4U);
    CHECK(block.nonzero_count == 8U);
    CHECK(block.cul_level == 23U);
    CHECK(block.dc_value == 2);
    CHECK(block.quantized[0] == 2);
    CHECK(block.quantized[4] == 2);
    CHECK(block.quantized[8] == 3);
    CHECK(block.quantized[12] == -4);
    CHECK(block.quantized[9] == 1);
    CHECK(block.quantized[13] == 6);
    CHECK(block.quantized[14] == -4);
    CHECK(block.quantized[15] == 1);
    CHECK(block.before_levels_bits == 88U);
    CHECK(block.after_levels_bits == 118U);
    CHECK(block.after_coefficients_bits == 126U);
    {
        static const uint8_t expected_predictor[] = {
            176U, 184U, 191U, 197U,
            195U, 201U, 204U, 207U,
            206U, 208U, 208U, 208U,
            208U, 208U, 208U, 208U
        };
        static const uint8_t pixels[] = {
            183U, 203U, 220U, 231U,
            202U, 220U, 233U, 241U,
            226U, 254U, 255U, 245U,
            211U, 244U, 249U, 12U
        };
        size_t index;
        CHECK(av2h_luma_predict(
                  &second_mode, 4U, 4U, &references, predictor, 4U) == AV2H_OK);
        CHECK(av2h_luma_reconstruct(
                  &second_mode, &prefix, &transform, &block, predictor, 4U,
                  &scratch, &reconstruction) == AV2H_OK);
        for (index = 0U; index < 16U; ++index) {
            CHECK(predictor[index] == expected_predictor[index]);
            CHECK(reconstruction.pixels[index] == pixels[index]);
        }
        above[12] = reconstruction.pixels[12U];
        above[13] = reconstruction.pixels[13U];
        above[14] = reconstruction.pixels[14U];
        above[15] = reconstruction.pixels[15U];
    }
        CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, third_leaf) == AV2H_OK);
    {
        Av2hPartitionNode node = { 1U, 0U, AV2H_BLOCK_16X4 };
        Av2hPartitionNode child;
          CHECK(av2h_partition_traversal_next(
                &luma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
        CHECK(node.block_size == AV2H_BLOCK_8X4);
        CHECK(av2h_range_tell(&decoder) == 127U);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &node,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &node, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 127U);
        CHECK(second_mode.mode_set == 3U);
        CHECK(second_mode.mode_index == 50U);
        CHECK(second_mode.joint_mode == 11U);
        CHECK(second_mode.mode == AV2H_INTRA_D45);
        CHECK(second_mode.angle_delta == 3);
        CHECK(second_mode.fsc_block_context == 1U);
        CHECK(second_mode.fsc_mode == 0U);
        CHECK(second_mode.mrl_index == 0U);
        CHECK(second_mode.after_mrl_bits == 137U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &node, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.tx_size == AV2H_TX_8X4);
        CHECK(prefix.tx_size_context == 1U);
        CHECK(prefix.txb_skip_context == 0U);
        CHECK(prefix.all_zero == 0U);
        CHECK(prefix.before_all_zero_bits == 137U);
        CHECK(prefix.after_all_zero_bits == 137U);
        CHECK(prefix.eob_point == 4U);
        CHECK(prefix.eob == 6U);
        CHECK(prefix.after_eob_bits == 142U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_signaled == 1U);
        CHECK(transform.primary_symbol == 5U);
        CHECK(transform.primary_type == AV2H_TX_ADST_FLIPADST);
        CHECK(transform.before_primary_bits == 142U);
        CHECK(transform.after_primary_bits == 145U);
        CHECK(transform.after_secondary_bits == 145U);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.width == 8U);
        CHECK(block.height == 4U);
        CHECK(block.nonzero_count == 2U);
        CHECK(block.cul_level == 11U);
        CHECK(block.dc_value == -2);
        CHECK(block.quantized[0] == -2);
        CHECK(block.quantized[2] == -1);
        CHECK(block.dequantized[0] == -392);
        CHECK(block.dequantized[2] == -196);
        CHECK(block.before_levels_bits == 145U);
        CHECK(block.after_levels_bits == 150U);
        CHECK(block.after_coefficients_bits == 152U);
        {
            static const uint8_t expected_predictor[] = {
                65U, 83U, 100U, 113U, 126U, 144U, 160U, 170U,
                78U, 95U, 109U, 122U, 138U, 156U, 167U, 179U,
                90U, 105U, 118U, 132U, 151U, 164U, 174U, 190U,
                101U, 114U, 127U, 146U, 160U, 170U, 185U, 199U
            };
            static const int16_t residual[] = {
                -5, -3, -2, -1, -2, -3, -3, -1,
                -14, -10, -5, -3, -4, -7, -8, -4,
                -20, -15, -7, -4, -7, -11, -11, -6,
                -24, -17, -9, -5, -8, -13, -14, -7
            };
            static const uint8_t pixels[] = {
                60U, 80U, 98U, 112U, 124U, 141U, 157U, 169U,
                64U, 85U, 104U, 119U, 134U, 149U, 159U, 175U,
                70U, 90U, 111U, 128U, 144U, 153U, 163U, 184U,
                77U, 97U, 118U, 141U, 152U, 157U, 171U, 192U
            };
            size_t index;
            references.above = above;
            references.above_count = 12U;
            CHECK(av2h_luma_predict(
                      &second_mode, 8U, 4U, &references, predictor, 8U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 8U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 32U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.residual[index] == residual[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
            for (index = 0U; index < 8U; ++index) {
                second_above[index] = reconstruction.pixels[16U + index];
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &luma_traversal, &partition_state, node) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &luma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          child = path.leaf;
        CHECK(child.row == 1U && child.column == 2U);
        CHECK(child.block_size == AV2H_BLOCK_4X4);
        CHECK(av2h_range_tell(&decoder) == 153U);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &child,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &child, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 153U);
        CHECK(second_mode.mode_set == 0U);
        CHECK(second_mode.mode_index == 4U);
        CHECK(second_mode.joint_mode == 4U);
        CHECK(second_mode.mode == AV2H_INTRA_PAETH);
        CHECK(second_mode.angle_delta == 0);
        CHECK(second_mode.fsc_mode == 0U);
        CHECK(second_mode.mrl_index == 0U);
        CHECK(second_mode.after_mrl_bits == 159U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &child, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.tx_size == AV2H_TX_4X4);
        CHECK(prefix.tx_size_context == 0U);
        CHECK(prefix.txb_skip_context == 0U);
        CHECK(prefix.all_zero == 0U);
        CHECK(prefix.before_all_zero_bits == 159U);
        CHECK(prefix.after_all_zero_bits == 159U);
        CHECK(prefix.eob_point == 3U);
        CHECK(prefix.eob == 3U);
        CHECK(prefix.after_eob_bits == 163U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_signaled == 1U);
        CHECK(transform.primary_symbol == 0U);
        CHECK(transform.primary_type == AV2H_TX_DCT_DCT);
        CHECK(transform.before_primary_bits == 163U);
        CHECK(transform.after_primary_bits == 166U);
        CHECK(transform.after_secondary_bits == 166U);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &child, block.cul_level) == AV2H_OK);
        CHECK(block.width == 4U);
        CHECK(block.height == 4U);
        CHECK(block.nonzero_count == 3U);
        CHECK(block.cul_level == 20U);
        CHECK(block.dc_value == 2);
        CHECK(block.quantized[0] == 2);
        CHECK(block.quantized[1] == -1);
        CHECK(block.quantized[4] == -1);
        CHECK(block.dequantized[0] == 392);
        CHECK(block.dequantized[1] == -196);
        CHECK(block.dequantized[4] == -196);
        CHECK(block.before_levels_bits == 166U);
        CHECK(block.after_levels_bits == 169U);
        CHECK(block.after_coefficients_bits == 172U);
        {
            static const uint8_t expected_predictor[] = {
                173U, 189U, 201U, 208U,
                175U, 189U, 201U, 208U,
                184U, 189U, 201U, 208U,
                192U, 192U, 201U, 208U
            };
            static const int16_t residual[] = {
                -4, 1, 8, 12,
                1, 6, 12, 17,
                8, 12, 19, 24,
                12, 17, 24, 28
            };
            static const uint8_t pixels[] = {
                169U, 190U, 209U, 220U,
                176U, 195U, 213U, 225U,
                192U, 201U, 220U, 232U,
                204U, 209U, 225U, 236U
            };
            size_t index;
            left[0] = reconstruction.pixels[7U];
            left[1] = reconstruction.pixels[15U];
            left[2] = reconstruction.pixels[23U];
            left[3] = reconstruction.pixels[31U];
            references.above = &above[8U];
            references.above_count = 4U;
            references.above_left = above[7U];
            references.has_above_left = 1U;
            CHECK(av2h_luma_predict(
                      &second_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.residual[index] == residual[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
            for (index = 0U; index < 4U; ++index) {
                second_above[8U + index] = reconstruction.pixels[8U + index];
                lower_above[index] = reconstruction.pixels[12U + index];
            }
        }
        CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, child) == AV2H_OK);
        CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs,
              &partition_state, &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 1U);
        child = path.leaf;
        CHECK(child.row == 1U && child.column == 3U);
        CHECK(child.block_size == AV2H_BLOCK_4X4);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &child,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &child, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 172U);
        CHECK(second_mode.mode_set == 0U);
        CHECK(second_mode.mode_index == 7U);
        CHECK(second_mode.joint_mode == 8U);
        CHECK(second_mode.mode == AV2H_INTRA_D45);
        CHECK(second_mode.angle_delta == 0);
        CHECK(second_mode.fsc_mode == 1U);
        CHECK(second_mode.mrl_index == 0U);
        CHECK(second_mode.after_mrl_bits == 181U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &child, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.txb_skip_context == 9U);
        CHECK(prefix.all_zero == 0U);
        CHECK(prefix.eob == 15U);
        CHECK(prefix.after_eob_bits == 185U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_IDTX);
        CHECK(transform.primary_signaled == 0U);
        CHECK(transform.after_secondary_bits == 185U);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &child, block.cul_level) == AV2H_OK);
        CHECK(block.nonzero_count == 5U);
        CHECK(block.quantized[4] == -1);
        CHECK(block.quantized[8] == 9);
        CHECK(block.quantized[5] == 9);
        CHECK(block.quantized[13] == 10);
        CHECK(block.quantized[15] == 1);
        CHECK(block.cul_level == 7U);
        CHECK(block.dc_value == 0);
        CHECK(block.after_coefficients_bits == 232U);
        {
            static const uint8_t expected_predictor[] = {
                244U, 249U, 12U, 12U,
                249U, 12U, 12U, 12U,
                12U, 12U, 12U, 12U,
                12U, 12U, 12U, 12U
            };
            static const int16_t residual[] = {
                0, 0, 0, 0,
                -24, 221, 0, 0,
                221, 0, 0, 0,
                0, 245, 0, 25
            };
            static const uint8_t pixels[] = {
                244U, 249U, 12U, 12U,
                225U, 233U, 12U, 12U,
                233U, 12U, 12U, 12U,
                12U, 255U, 12U, 37U
            };
            size_t index;
            references.above = &above[12U];
            references.above_count = 4U;
            CHECK(av2h_luma_predict(
                      &second_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.residual[index] == residual[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
            for (index = 0U; index < 4U; ++index) {
                second_above[12U + index] = reconstruction.pixels[8U + index];
                lower_above[4U + index] = reconstruction.pixels[12U + index];
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &luma_traversal, &partition_state, child) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &luma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
          CHECK(node.row == 2U && node.column == 0U);
          CHECK(node.block_size == AV2H_BLOCK_8X8);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &node,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &node, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 233U);
        CHECK(second_mode.mode_set == 2U);
        CHECK(second_mode.mode_index == 36U);
        CHECK(second_mode.joint_mode == 14U);
        CHECK(second_mode.mode == AV2H_INTRA_D67);
        CHECK(second_mode.angle_delta == -1);
        CHECK(second_mode.fsc_mode == 0U);
        CHECK(second_mode.mrl_index == 1U);
        CHECK(second_mode.multi_line_mrl == 0U);
        CHECK(second_mode.after_mrl_bits == 248U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &node, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.tx_size == AV2H_TX_8X8);
        CHECK(prefix.eob == 2U);
        CHECK(prefix.after_eob_bits == 254U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_symbol == 3U);
        CHECK(transform.primary_type == AV2H_TX_DCT_ADST);
        CHECK(transform.after_secondary_bits == 258U);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == 2);
        CHECK(block.quantized[8] == -1);
        CHECK(block.nonzero_count == 2U);
        CHECK(block.cul_level == 19U);
        CHECK(block.dc_value == 2);
        CHECK(block.after_coefficients_bits == 262U);
        {
            static const uint8_t expected_predictor[] = {
                86U, 108U, 126U, 142U, 152U, 161U, 181U, 191U,
                95U, 115U, 132U, 147U, 155U, 168U, 187U, 193U,
                105U, 123U, 140U, 151U, 159U, 178U, 190U, 197U,
                113U, 130U, 145U, 154U, 165U, 185U, 193U, 203U,
                121U, 137U, 149U, 157U, 175U, 189U, 196U, 211U,
                127U, 143U, 153U, 162U, 183U, 192U, 200U, 219U,
                135U, 148U, 156U, 171U, 188U, 195U, 208U, 226U,
                141U, 151U, 160U, 180U, 191U, 198U, 216U, 231U
            };
            static const uint8_t pixels[] = {
                86U, 109U, 127U, 144U, 154U, 163U, 183U, 194U,
                95U, 116U, 134U, 149U, 158U, 171U, 190U, 197U,
                106U, 125U, 143U, 154U, 163U, 183U, 195U, 202U,
                114U, 132U, 149U, 159U, 171U, 191U, 200U, 210U,
                122U, 140U, 154U, 163U, 183U, 198U, 205U, 221U,
                128U, 147U, 159U, 170U, 192U, 203U, 211U, 231U,
                137U, 152U, 163U, 180U, 199U, 207U, 221U, 240U,
                143U, 156U, 167U, 189U, 202U, 211U, 230U, 245U
            };
            size_t index;
            references.above = second_above;
            references.above_count = 16U;
            CHECK(av2h_luma_predict(
                      &second_mode, 8U, 8U, &references, predictor, 8U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 8U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 64U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &luma_traversal, &partition_state, node) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &luma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
          CHECK(node.row == 2U && node.column == 2U);
        CHECK(node.block_size == AV2H_BLOCK_4X4);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &node,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &node, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 267U);
        CHECK(second_mode.mode_set == 1U);
        CHECK(second_mode.mode_index == 19U);
        CHECK(second_mode.joint_mode == 13U);
        CHECK(second_mode.mode == AV2H_INTRA_D67);
        CHECK(second_mode.angle_delta == -2);
        CHECK(second_mode.fsc_mode == 1U);
        CHECK(second_mode.mrl_index == 1U);
        CHECK(second_mode.after_mrl_bits == 282U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &node, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.eob == 8U);
        CHECK(prefix.after_eob_bits == 286U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_IDTX);
        CHECK(transform.after_secondary_bits == 286U);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[6] == -1);
        CHECK(block.quantized[7] == 5);
        CHECK(block.quantized[14] == -6);
        CHECK(block.nonzero_count == 3U);
        CHECK(block.cul_level == 7U);
        CHECK(block.after_coefficients_bits == 309U);
        {
            static const uint8_t expected_predictor[] = {
                201U, 219U, 232U, 237U,
                209U, 227U, 253U, 138U,
                219U, 232U, 238U, 22U,
                226U, 253U, 147U, 0U
            };
            static const int16_t residual[] = {
                0, 0, 0, 0,
                0, 0, -24, 123,
                0, 0, 0, 0,
                0, 0, -147, 0
            };
            static const uint8_t pixels[] = {
                201U, 219U, 232U, 237U,
                209U, 227U, 229U, 255U,
                219U, 232U, 238U, 22U,
                226U, 253U, 0U, 0U
            };
            size_t index;
            references.above = &second_above[8U];
            references.above_count = 8U;
            references.above_left = second_above[7U];
            references.has_above_left = 1U;
            CHECK(av2h_luma_predict(
                      &second_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.residual[index] == residual[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
            for (index = 0U; index < 4U; ++index) {
                top_lower_above[index] = reconstruction.pixels[index];
            }
        }
        CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, node) == AV2H_OK);
        CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs,
              &partition_state, &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 1U);
        node = path.leaf;
        CHECK(node.row == 2U && node.column == 3U);
        CHECK(node.block_size == AV2H_BLOCK_4X4);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &node,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &node, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 309U);
        CHECK(second_mode.mode_set == 0U);
        CHECK(second_mode.mode_index == 7U);
        CHECK(second_mode.joint_mode == 8U);
        CHECK(second_mode.mode == AV2H_INTRA_D45);
        CHECK(second_mode.angle_delta == 0);
        CHECK(second_mode.fsc_mode == 1U);
        CHECK(second_mode.mrl_index == 0U);
        CHECK(second_mode.after_mrl_bits == 316U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &node, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.eob == 13U);
        CHECK(prefix.after_eob_bits == 320U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_IDTX);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[8] == -1);
        CHECK(block.quantized[5] == -1);
        CHECK(block.quantized[15] == 1);
        CHECK(block.nonzero_count == 3U);
        CHECK(block.after_coefficients_bits == 335U);
        {
            static const uint8_t expected_predictor[] = {
                255U, 12U, 37U, 37U,
                12U, 37U, 37U, 37U,
                37U, 37U, 37U, 37U,
                37U, 37U, 37U, 37U
            };
            static const uint8_t pixels[] = {
                255U, 12U, 37U, 37U,
                12U, 13U, 37U, 37U,
                13U, 37U, 37U, 37U,
                37U, 37U, 37U, 62U
            };
            size_t index;
            references.above = &lower_above[4U];
            references.above_count = 4U;
            CHECK(av2h_luma_predict(
                      &second_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
            for (index = 0U; index < 4U; ++index) {
                top_lower_above[4U + index] = reconstruction.pixels[index];
                final_above[index] = reconstruction.pixels[12U + index];
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &luma_traversal, &partition_state, node) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &luma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
          CHECK(node.row == 3U && node.column == 2U);
        CHECK(node.block_size == AV2H_BLOCK_4X4);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &node,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &node, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 337U);
        CHECK(second_mode.mode_set == 3U);
        CHECK(second_mode.mode_index == 51U);
        CHECK(second_mode.joint_mode == 12U);
        CHECK(second_mode.mode == AV2H_INTRA_D67);
        CHECK(second_mode.angle_delta == -3);
        CHECK(second_mode.fsc_mode == 1U);
        CHECK(second_mode.mrl_index == 3U);
        CHECK(second_mode.after_mrl_bits == 352U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &node, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.eob == 13U);
        CHECK(prefix.after_eob_bits == 356U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_IDTX);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[8] == -10);
        CHECK(block.quantized[2] == 3);
        CHECK(block.quantized[12] == 1);
        CHECK(block.quantized[9] == -5);
        CHECK(block.quantized[3] == 1);
        CHECK(block.quantized[10] == 1);
        CHECK(block.quantized[14] == -1);
        CHECK(block.nonzero_count == 7U);
        CHECK(block.after_coefficients_bits == 399U);
        {
            static const uint8_t expected_predictor[] = {
                234U, 255U, 167U, 0U,
                237U, 255U, 17U, 35U,
                255U, 116U, 5U, 39U,
                223U, 2U, 38U, 37U
            };
            static const int16_t residual[] = {
                0, 0, 74, 25,
                0, 0, 0, 0,
                -245, -122, 25, 0,
                25, 0, -24, 0
            };
            static const uint8_t pixels[] = {
                234U, 255U, 241U, 25U,
                237U, 255U, 17U, 35U,
                10U, 0U, 30U, 39U,
                248U, 2U, 14U, 37U
            };
            size_t index;
            references.above = top_lower_above;
            references.above_count = 8U;
            CHECK(av2h_luma_predict(
                      &second_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.residual[index] == residual[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
            for (index = 0U; index < 4U; ++index) {
                final_left[index] = reconstruction.pixels[index * 4U + 3U];
            }
        }
        CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, node) == AV2H_OK);
        CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs,
              &partition_state, &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 1U);
        node = path.leaf;
        CHECK(node.row == 3U && node.column == 3U);
        CHECK(node.block_size == AV2H_BLOCK_4X4);
        CHECK(av2h_luma_mode_read_with_state(
              &decoder, &leaf_cdfs, &sequence, &frame, &node,
              &mode_state, &second_mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
              &mode_state, &node, &second_mode) == AV2H_OK);
        CHECK(second_mode.before_mode_bits == 399U);
        CHECK(second_mode.mode_set == 0U);
        CHECK(second_mode.mode_index == 1U);
        CHECK(second_mode.joint_mode == 1U);
        CHECK(second_mode.mode == AV2H_INTRA_SMOOTH);
        CHECK(second_mode.angle_delta == 0);
        CHECK(second_mode.fsc_mode == 0U);
        CHECK(second_mode.mrl_index == 0U);
        CHECK(second_mode.after_mrl_bits == 403U);
        CHECK(av2h_coeff_prefix_read_with_state(
              &decoder, &coeff_cdfs, &frame, &node, &second_mode,
              &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(prefix.eob == 2U);
        CHECK(prefix.after_eob_bits == 407U);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_DCT_ADST);
        CHECK(transform.after_primary_bits == 410U);
        CHECK(av2h_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, &second_mode,
                  &prefix, &transform, dc_sign_context, &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
                  &coeff_state, 0U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == 3);
        CHECK(block.quantized[4] == -1);
        CHECK(block.nonzero_count == 2U);
        CHECK(block.after_coefficients_bits == 415U);
        {
            static const uint8_t expected_predictor[] = {
                34U, 40U, 45U, 61U,
                38U, 42U, 46U, 57U,
                40U, 44U, 46U, 53U,
                39U, 43U, 46U, 50U
            };
            static const int16_t residual[] = {
                3, 8, 12, 15,
                4, 12, 18, 21,
                6, 17, 25, 30,
                7, 21, 31, 37
            };
            static const uint8_t pixels[] = {
                37U, 48U, 57U, 76U,
                42U, 54U, 64U, 78U,
                46U, 61U, 71U, 83U,
                46U, 64U, 77U, 87U
            };
            size_t index;
            references.above = final_above;
            references.above_count = 4U;
            references.left = final_left;
            references.left_count = 4U;
            CHECK(av2h_luma_predict(
                      &second_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.residual[index] == residual[index]);
                CHECK(reconstruction.pixels[index] == pixels[index]);
            }
        }
        CHECK(av2h_partition_traversal_commit_leaf(
              &luma_traversal, &partition_state, node) == AV2H_OK);
        CHECK(av2h_partition_traversal_next(
              &luma_traversal, &decoder, &partition_cdfs,
              &partition_state, &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 0U);
        CHECK(av2h_partition_traversal_init(
              &chroma_traversal, 4U, 4U,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_16X16 }, 1U,
              1U) == AV2H_OK);
        CHECK(av2h_partition_traversal_next(
              &chroma_traversal, &decoder, &partition_cdfs,
              &partition_state, &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 1U);
        node = path.leaf;
        CHECK(node.row == 0U && node.column == 0U);
        CHECK(node.block_size == AV2H_BLOCK_8X8);
        CHECK(av2h_range_tell(&decoder) == 420U);
        CHECK(av2h_chroma_mode_read(
                  &decoder, &leaf_cdfs, &sequence, AV2H_INTRA_DC, 0U,
                  &chroma_mode) == AV2H_OK);
        CHECK(chroma_mode.before_cfl_bits == 420U);
        CHECK(chroma_mode.is_cfl == 0U);
        CHECK(chroma_mode.mode_index == 0U);
        CHECK(chroma_mode.mode == AV2H_INTRA_DC);
        CHECK(chroma_mode.angle_delta == 0);
        CHECK(chroma_mode.after_mode_bits == 423U);
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 1U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.tx_size == AV2H_TX_4X4);
        CHECK(prefix.txb_skip_context == 6U);
        CHECK(prefix.before_all_zero_bits == 423U);
        CHECK(prefix.all_zero == 0U);
        CHECK(prefix.eob == 3U);
        CHECK(prefix.after_eob_bits == 428U);
        CHECK(av2h_first_chroma_transform_type_read(
              &decoder, &transform_cdfs, &frame, 1U, &prefix,
              &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_DCT_DCT);
        CHECK(transform.after_primary_bits == 429U);
        CHECK(av2h_chroma_coeff_block_read_with_state(
              &decoder, &level_cdfs, &sequence, &frame, &node, 1U,
              &prefix, &coeff_state, &block) == AV2H_OK);
        CHECK(block.quantized[0] == 0);
        CHECK(block.quantized[1] == -3);
        CHECK(block.quantized[4] == -1);
        CHECK(block.nonzero_count == 2U);
        CHECK(block.after_coefficients_bits == 445U);
        {
            static const uint8_t expected_u[] = {
                96U, 110U, 130U, 144U,
                101U, 115U, 135U, 148U,
                108U, 121U, 141U, 155U,
                112U, 126U, 146U, 160U
            };
            size_t index;
            CHECK(av2h_first_chroma_reconstruct(
                      &prefix, &transform, &block, &scratch,
                      &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(reconstruction.pixels[index] == expected_u[index]);
                CHECK(reconstruction.residual[index] ==
                      (int16_t)((int)expected_u[index] - 128));
            }
        }
          CHECK(av2h_chroma_coeff_prefix_read_with_state(
                &decoder, &coeff_cdfs, &frame, &node, 2U, &coeff_state,
                &prefix) == AV2H_OK);
        CHECK(prefix.eob == 3U);
        CHECK(prefix.after_eob_bits == 450U);
        CHECK(av2h_first_chroma_transform_type_read(
              &decoder, &transform_cdfs, &frame, 2U, &prefix,
              &transform) == AV2H_OK);
        CHECK(transform.after_primary_bits == 450U);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 2U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 2U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == 11);
        CHECK(block.quantized[1] == -3);
        CHECK(block.quantized[4] == -1);
        CHECK(block.nonzero_count == 3U);
        CHECK(block.after_coefficients_bits == 469U);
        {
            static const uint8_t expected_v[] = {
                164U, 177U, 197U, 211U,
                168U, 182U, 202U, 216U,
                175U, 189U, 209U, 223U,
                180U, 193U, 213U, 227U
            };
            size_t index;
            CHECK(av2h_first_chroma_reconstruct(
                      &prefix, &transform, &block, &scratch,
                      &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(reconstruction.pixels[index] == expected_v[index]);
                CHECK(reconstruction.residual[index] ==
                      (int16_t)((int)expected_v[index] - 128));
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &chroma_traversal, &partition_state, node) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &chroma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
          CHECK(node.row == 0U && node.column == 2U);
          CHECK(node.block_size == AV2H_BLOCK_8X8);
        CHECK(av2h_range_tell(&decoder) == 469U);
        CHECK(av2h_chroma_mode_read(
                  &decoder, &leaf_cdfs, &sequence, AV2H_INTRA_D203, 0U,
                  &chroma_mode) == AV2H_OK);
        CHECK(chroma_mode.before_cfl_bits == 469U);
        CHECK(chroma_mode.is_cfl == 0U);
        CHECK(chroma_mode.mode_index == 0U);
        CHECK(chroma_mode.mode == AV2H_INTRA_D203);
        CHECK(chroma_mode.angle_delta == 0);
        CHECK(chroma_mode.after_mode_bits == 470U);
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 1U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.txb_skip_context == 7U);
        CHECK(prefix.before_all_zero_bits == 470U);
        CHECK(prefix.all_zero == 0U);
        CHECK(prefix.eob == 2U);
        CHECK(prefix.after_eob_bits == 475U);
        CHECK(av2h_chroma_transform_type_read(
              &decoder, &transform_cdfs, &frame, &chroma_mode, 1U, &prefix,
                  &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_DCT_ADST);
        CHECK(transform.after_primary_bits == 476U);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 1U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 1U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == 6);
        CHECK(block.quantized[4] == -1);
        CHECK(block.nonzero_count == 2U);
        CHECK(block.after_coefficients_bits == 485U);
        {
            static const uint8_t chroma_left[] = { 144U, 148U, 155U, 160U };
            static const uint8_t expected_predictor[] = {
                146U, 147U, 149U, 152U,
                151U, 153U, 156U, 158U,
                157U, 159U, 160U, 160U,
                160U, 160U, 160U, 160U
            };
            static const uint8_t expected_u[] = {
                154U, 169U, 183U, 192U,
                160U, 179U, 195U, 204U,
                168U, 190U, 207U, 216U,
                173U, 195U, 212U, 222U
            };
            size_t index;
            second_mode = (Av2hFirstLeafMode){ 0 };
            second_mode.mode = chroma_mode.mode;
            second_mode.angle_delta = chroma_mode.angle_delta;
            second_mode.mrl_index = 0U;
            second_mode.multi_line_mrl = 0U;
            references.left = chroma_left;
            references.left_count = 4U;
            references.above = 0;
            references.above_count = 0U;
            CHECK(av2h_chroma_predict(
                      &chroma_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == expected_u[index]);
            }
        }
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 2U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.txb_skip_context == 7U);
        CHECK(prefix.eob == 16U);
        CHECK(prefix.after_eob_bits == 492U);
        CHECK(av2h_chroma_transform_type_read(
              &decoder, &transform_cdfs, &frame, &chroma_mode, 2U, &prefix,
                  &transform) == AV2H_OK);
        CHECK(transform.after_primary_bits == 492U);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 2U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 2U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == -20);
        CHECK(block.quantized[4] == 5);
        CHECK(block.quantized[1] == 6);
        CHECK(block.quantized[15] == -1);
        CHECK(block.nonzero_count == 15U);
        CHECK(block.after_coefficients_bits == 571U);
        {
            static const uint8_t chroma_left[] = { 211U, 216U, 223U, 227U };
            static const uint8_t expected_predictor[] = {
                213U, 215U, 217U, 220U,
                219U, 221U, 224U, 225U,
                225U, 226U, 227U, 227U,
                227U, 227U, 227U, 227U
            };
            static const uint8_t expected_v[] = {
                212U, 216U, 243U, 6U,
                234U, 234U, 6U, 9U,
                224U, 255U, 25U, 31U,
                247U, 16U, 21U, 37U
            };
            size_t index;
            references.left = chroma_left;
            references.left_count = 4U;
            CHECK(av2h_chroma_predict(
                      &chroma_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == expected_v[index]);
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &chroma_traversal, &partition_state, node) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &chroma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
          CHECK(node.row == 2U && node.column == 0U);
          CHECK(node.block_size == AV2H_BLOCK_8X8);
        CHECK(av2h_range_tell(&decoder) == 572U);
        CHECK(av2h_chroma_mode_read_with_delta(
                  &decoder, &leaf_cdfs, &sequence, AV2H_INTRA_D67, -1, 0U,
                  &chroma_mode) == AV2H_OK);
        CHECK(chroma_mode.mode_index == 0U);
        CHECK(chroma_mode.mode == AV2H_INTRA_D67);
        CHECK(chroma_mode.angle_delta == -1);
        CHECK(chroma_mode.after_mode_bits == 574U);
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 1U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.eob == 1U);
        CHECK(prefix.after_eob_bits == 577U);
        CHECK(av2h_chroma_transform_type_read(
                  &decoder, &transform_cdfs, &frame, &chroma_mode, 1U, &prefix,
                  &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_ADST_DCT);
        CHECK(transform.after_primary_bits == 577U);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 1U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 1U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == 2);
        CHECK(block.nonzero_count == 1U);
        CHECK(block.after_coefficients_bits == 582U);
        {
            static const uint8_t chroma_above[] = {
                112U, 126U, 146U, 160U, 173U, 195U, 212U, 222U
            };
            static const uint8_t expected_predictor[] = {
                118U, 135U, 152U, 166U,
                124U, 144U, 158U, 171U,
                132U, 150U, 164U, 180U,
                141U, 157U, 170U, 190U
            };
            static const uint8_t expected_u[] = {
                121U, 138U, 155U, 169U,
                134U, 154U, 168U, 181U,
                146U, 164U, 178U, 194U,
                158U, 174U, 187U, 207U
            };
            size_t index;
            second_mode = (Av2hFirstLeafMode){ 0 };
            second_mode.mode = chroma_mode.mode;
            second_mode.angle_delta = chroma_mode.angle_delta;
            references.above = chroma_above;
            references.above_count = 8U;
            CHECK(av2h_chroma_predict(
                      &chroma_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == expected_u[index]);
            }
        }
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 2U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.eob == 16U);
        CHECK(prefix.after_eob_bits == 587U);
        CHECK(av2h_chroma_transform_type_read(
                  &decoder, &transform_cdfs, &frame, &chroma_mode, 2U, &prefix,
                  &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_ADST_DCT);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 2U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 2U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == -2);
        CHECK(block.quantized[1] == 4);
        CHECK(block.quantized[15] == 1);
        CHECK(block.nonzero_count == 13U);
        CHECK(block.after_coefficients_bits == 643U);
        {
            static const uint8_t chroma_above[] = {
                180U, 193U, 213U, 227U, 247U, 16U, 21U, 37U
            };
            static const uint8_t expected_predictor[] = {
                186U, 202U, 219U, 236U,
                191U, 211U, 225U, 245U,
                199U, 217U, 233U, 175U,
                208U, 224U, 242U, 74U
            };
            static const uint8_t expected_v[] = {
                189U, 199U, 235U, 249U,
                206U, 227U, 235U, 242U,
                213U, 222U, 245U, 5U,
                210U, 221U, 247U, 18U
            };
            size_t index;
            references.above = chroma_above;
            references.above_count = 8U;
            CHECK(av2h_chroma_predict(
                      &chroma_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == expected_v[index]);
            }
        }
          CHECK(av2h_partition_traversal_commit_leaf(
                &chroma_traversal, &partition_state, node) == AV2H_OK);
          CHECK(av2h_partition_traversal_next(
                &chroma_traversal, &decoder, &partition_cdfs,
                &partition_state, &path, &has_leaf) == AV2H_OK);
          CHECK(has_leaf == 1U);
          node = path.leaf;
          CHECK(node.row == 2U && node.column == 2U);
          CHECK(node.block_size == AV2H_BLOCK_8X8);
        CHECK(av2h_range_tell(&decoder) == 643U);
        CHECK(av2h_chroma_mode_read_with_delta(
                  &decoder, &leaf_cdfs, &sequence, AV2H_INTRA_D67, -3, 0U,
                  &chroma_mode) == AV2H_OK);
        CHECK(chroma_mode.mode_index == 8U);
        CHECK(chroma_mode.mode == AV2H_INTRA_D45);
        CHECK(chroma_mode.angle_delta == 0);
        CHECK(chroma_mode.after_mode_bits == 652U);
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 1U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.eob == 15U);
        CHECK(prefix.after_eob_bits == 657U);
        CHECK(av2h_chroma_transform_type_read(
                  &decoder, &transform_cdfs, &frame, &chroma_mode, 1U, &prefix,
                  &transform) == AV2H_OK);
        CHECK(transform.primary_type == AV2H_TX_DCT_DCT);
        CHECK(transform.after_primary_bits == 658U);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 1U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 1U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[0] == -2);
        CHECK(block.quantized[4] == 3);
        CHECK(block.quantized[11] == 1);
        CHECK(block.nonzero_count == 15U);
        CHECK(block.after_coefficients_bits == 710U);
        {
            static const uint8_t chroma_above[] = {
                173U, 195U, 212U, 222U
            };
            static const uint8_t expected_predictor[] = {
                195U, 212U, 222U, 222U,
                212U, 222U, 222U, 222U,
                222U, 222U, 222U, 222U,
                222U, 222U, 222U, 222U
            };
            static const uint8_t expected_u[] = {
                188U, 212U, 222U, 240U,
                208U, 229U, 215U, 235U,
                210U, 209U, 235U, 225U,
                224U, 223U, 221U, 14U
            };
            size_t index;
            second_mode = (Av2hFirstLeafMode){ 0 };
            second_mode.mode = chroma_mode.mode;
            references.above = chroma_above;
            references.above_count = 4U;
            CHECK(av2h_chroma_predict(
                      &chroma_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == expected_u[index]);
            }
        }
        CHECK(av2h_chroma_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &node, 2U, &coeff_state,
                  &prefix) == AV2H_OK);
        CHECK(prefix.eob == 6U);
        CHECK(prefix.after_eob_bits == 715U);
        CHECK(av2h_chroma_transform_type_read(
                  &decoder, &transform_cdfs, &frame, &chroma_mode, 2U, &prefix,
                  &transform) == AV2H_OK);
        CHECK(transform.after_primary_bits == 715U);
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &level_cdfs, &sequence, &frame, 2U, &prefix,
                  &block) == AV2H_OK);
        CHECK(av2h_coeff_context_state_update(
              &coeff_state, 2U, &node, block.cul_level) == AV2H_OK);
        CHECK(block.quantized[1] == -1);
        CHECK(block.quantized[2] == 1);
        CHECK(block.quantized[8] == 1);
        CHECK(block.nonzero_count == 3U);
        CHECK(block.after_coefficients_bits == 725U);
        {
            static const uint8_t chroma_above[] = { 247U, 16U, 21U, 37U };
            static const uint8_t expected_predictor[] = {
                16U, 21U, 37U, 37U,
                21U, 37U, 37U, 37U,
                37U, 37U, 37U, 37U,
                37U, 37U, 37U, 37U
            };
            static const uint8_t expected_v[] = {
                20U, 18U, 40U, 57U,
                13U, 21U, 28U, 45U,
                29U, 21U, 28U, 45U,
                41U, 34U, 40U, 57U
            };
            size_t index;
            references.above = chroma_above;
            references.above_count = 4U;
            CHECK(av2h_chroma_predict(
                      &chroma_mode, 4U, 4U, &references, predictor, 4U) ==
                  AV2H_OK);
            CHECK(av2h_luma_reconstruct(
                      &second_mode, &prefix, &transform, &block, predictor, 4U,
                      &scratch, &reconstruction) == AV2H_OK);
            for (index = 0U; index < 16U; ++index) {
                CHECK(predictor[index] == expected_predictor[index]);
                CHECK(reconstruction.pixels[index] == expected_v[index]);
            }
        }
        CHECK(av2h_partition_traversal_commit_leaf(
                  &chroma_traversal, &partition_state, node) == AV2H_OK);
        CHECK(av2h_partition_traversal_next(
                  &chroma_traversal, &decoder, &partition_cdfs,
                  &partition_state, &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 0U);
    }
}

static void test_arguments(void) {
    Av2hRangeDecoder decoder;
    Av2hLeafCdfs cdfs;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_8X8 };
    Av2hFirstLeafMode mode;
    CHECK(sizeof(Av2hLeafCdfs) == 784U);
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
    test_tiny_second_luma_leaf();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d leaf checks failed\n", failures);
        return 1;
    }
    puts("leaf unit: pass");
    return 0;
}