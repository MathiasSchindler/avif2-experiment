#include "decoder/coeff.h"

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
                          Av2hFrameState frame,
                          Av2hBlockSize expected_leaf,
                          Av2hFirstTxSize expected_tx,
                          uint8_t expected_tx_context,
                          size_t expected_before,
                          size_t expected_after_all_zero,
                          uint8_t expected_eob_point,
                          uint8_t expected_eob_offset_bits,
                          uint16_t expected_eob_extra,
                          uint16_t expected_eob,
                          size_t expected_after_point,
                          size_t expected_after_extra,
                          size_t expected_after_eob) {
    Av2hSpan span = { bytes, size };
    Av2hSequenceState sequence = { 0 };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs partition_cdfs;
    Av2hLeafCdfs leaf_cdfs;
    Av2hFirstCoeffCdfs coeff_cdfs;
    Av2hFirstLeafPath path;
    Av2hFirstLeafMode mode;
    Av2hFirstCoeffPrefix prefix;
    Av2hStatus status;
    sequence.enable_sdp = 1U;
    sequence.enable_idtx_intra = 1U;
    CHECK(av2h_partition_cdfs_init(&partition_cdfs) == AV2H_OK);
    CHECK(av2h_leaf_cdfs_init(&leaf_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_cdfs_init(&coeff_cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_first_leaf_read(
              &decoder, &partition_cdfs, width, height, 1U, &path) == AV2H_OK);
    CHECK(path.leaf.block_size == expected_leaf);
    CHECK(av2h_first_leaf_mode_read(
              &decoder, &leaf_cdfs, &sequence, &frame, &path.leaf, &mode) ==
          AV2H_OK);
    status = av2h_first_coeff_prefix_read(
        &decoder, &coeff_cdfs, &frame, &path.leaf, &mode, &prefix);
    if (status != AV2H_OK) {
        fprintf(stderr, "%s: coeff status=%d tell=%zu\n", name, status,
                av2h_range_tell(&decoder));
        ++failures;
        return;
    }
    CHECK(prefix.tx_size == expected_tx);
    CHECK(prefix.tx_size_context == expected_tx_context);
    CHECK(prefix.txb_skip_context == 0U);
    CHECK(prefix.all_zero == 0U);
    CHECK(prefix.before_all_zero_bits == expected_before);
    CHECK(prefix.after_all_zero_bits == expected_after_all_zero);
    CHECK(prefix.eob_point == expected_eob_point);
    CHECK(prefix.eob_offset_bits == expected_eob_offset_bits);
    CHECK(prefix.eob_extra == expected_eob_extra);
    CHECK(prefix.eob == expected_eob);
    CHECK(prefix.after_eob_point_bits == expected_after_point);
    CHECK(prefix.after_eob_extra_bit == expected_after_extra);
    CHECK(prefix.after_eob_bits == expected_after_eob);
    CHECK(coeff_cdfs.txb_skip_txs1[2] ==
          (expected_tx_context == 1U ? 1U : 0U));
    CHECK(coeff_cdfs.txb_skip_txs3[2] ==
          (expected_tx_context == 3U ? 1U : 0U));
        CHECK(coeff_cdfs.eob_pt_32[6] ==
            (expected_tx == AV2H_TX_8X4 ? 1U : 0U));
        CHECK(coeff_cdfs.eob_pt_64[7] ==
            (expected_tx == AV2H_TX_8X8 ? 1U : 0U));
        CHECK(coeff_cdfs.eob_pt_1024[8] ==
            (expected_tx == AV2H_TX_32X32 ? 1U : 0U));
        CHECK(coeff_cdfs.eob_extra[2] == 1U);
}

static void test_fixture_prefixes(void) {
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
    tiny_frame.base_q_index = 128U;
    tiny_frame.tx_mode = 1U;
    tiny_frame.cdef_strength_count = 1U;
    odd_frame.base_q_index = 128U;
    odd_frame.tx_mode = 1U;
    odd_frame.allow_intrabc = 1U;
    odd_frame.gdf_mode = 1U;
    odd_frame.cdef_strength_count = 1U;
    odd_frame.ccso[1].enabled = 1U;
    odd_frame.ccso[2].enabled = 1U;
    ordinary_frame.base_q_index = 128U;
    ordinary_frame.tx_mode = 1U;
    ordinary_frame.allow_intrabc = 1U;
    ordinary_frame.cdef_strength_count = 1U;
    ordinary_frame.ccso[0].enabled = 1U;
    detail_frame.base_q_index = 128U;
    detail_frame.tx_mode = 1U;
    detail_frame.allow_intrabc = 1U;
    detail_frame.ccso[0].enabled = 1U;
    check_fixture("tiny", tiny, sizeof(tiny), 16U, 16U, tiny_frame,
                  AV2H_BLOCK_8X4, AV2H_TX_8X4, 1U, 12U, 12U,
                  5U, 3U, 1U, 10U, 15U, 16U, 18U);
    check_fixture("odd", odd, sizeof(odd), 17U, 15U, odd_frame,
                  AV2H_BLOCK_8X4, AV2H_TX_8X4, 1U, 18U, 18U,
                  5U, 3U, 1U, 10U, 21U, 22U, 24U);
    check_fixture("ordinary", ordinary, sizeof(ordinary), 160U, 120U,
                  ordinary_frame, AV2H_BLOCK_32X32, AV2H_TX_32X32,
                  3U, 9U, 10U, 6U, 4U, 12U, 29U, 11U, 12U, 15U);
    check_fixture("high-detail", detail, sizeof(detail), 96U, 80U,
                  detail_frame, AV2H_BLOCK_8X8, AV2H_TX_8X8,
                  1U, 22U, 22U, 7U, 5U, 30U, 63U, 23U, 25U, 29U);
}

static void test_arguments(void) {
    Av2hRangeDecoder decoder;
    Av2hFirstCoeffCdfs cdfs;
    Av2hFrameState frame = { 0 };
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_8X8 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix;
    CHECK(sizeof(Av2hFirstCoeffCdfs) == 234U);
    CHECK(av2h_first_coeff_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_coeff_prefix_read(
              0, &cdfs, &frame, &leaf, &mode, &prefix) ==
          AV2H_INVALID_ARGUMENT);
    frame.base_q_index = 128U;
    frame.tx_mode = 2U;
    CHECK(av2h_first_coeff_prefix_read(
              &decoder, &cdfs, &frame, &leaf, &mode, &prefix) ==
          AV2H_UNSUPPORTED);
}

static void test_context_state(void) {
    uint8_t above[AV2H_FRAME_PLANE_COUNT * 4U];
    uint8_t left[AV2H_FRAME_PLANE_COUNT * 4U];
    Av2hCoeffContextState state;
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_4X4 };
    uint8_t skip_context;
    uint8_t dc_sign_context;

    CHECK(av2h_coeff_context_state_init(
              &state, above, 4U, left, 4U) == AV2H_OK);
    CHECK(av2h_coeff_contexts_from_state(
              &state, 1U, &leaf, 0U, &skip_context, &dc_sign_context) ==
          AV2H_OK);
    CHECK(skip_context == 6U);
    CHECK(dc_sign_context == 0U);
    CHECK(av2h_coeff_context_state_update(
              &state, 1U, &leaf, 18U) == AV2H_OK);
    CHECK(av2h_coeff_contexts_from_state(
              &state, 2U, &leaf, 0U, &skip_context, &dc_sign_context) ==
          AV2H_OK);
    CHECK(skip_context == 9U);

    CHECK(av2h_coeff_context_state_update(
              &state, 2U, &leaf, 11U) == AV2H_OK);
    leaf.column = 1U;
    CHECK(av2h_coeff_contexts_from_state(
              &state, 1U, &leaf, 0U, &skip_context, &dc_sign_context) ==
          AV2H_OK);
    CHECK(skip_context == 7U);
    CHECK(dc_sign_context == 2U);
    CHECK(av2h_coeff_context_state_update(
              &state, 1U, &leaf, 9U) == AV2H_OK);
    CHECK(av2h_coeff_contexts_from_state(
              &state, 2U, &leaf, 0U, &skip_context, &dc_sign_context) ==
          AV2H_OK);
    CHECK(skip_context == 7U);
    CHECK(dc_sign_context == 1U);

    leaf.row = 1U;
    leaf.column = 0U;
    CHECK(av2h_coeff_context_state_update(
              &state, 1U, &leaf, 5U) == AV2H_OK);
    leaf.column = 1U;
    CHECK(av2h_coeff_contexts_from_state(
              &state, 1U, &leaf, 0U, &skip_context, &dc_sign_context) ==
          AV2H_OK);
    CHECK(skip_context == 8U);
    CHECK(dc_sign_context == 1U);
    CHECK(av2h_coeff_contexts_from_state(
              &state, 0U, &leaf, 1U, &skip_context, &dc_sign_context) ==
          AV2H_OK);
    CHECK(skip_context == 9U);
    CHECK(dc_sign_context == 0U);

    leaf.column = 4U;
    CHECK(av2h_coeff_context_state_update(
              &state, 0U, &leaf, 0U) == AV2H_OVERFLOW);
    CHECK(av2h_coeff_context_state_init(
              0, above, 4U, left, 4U) == AV2H_INVALID_ARGUMENT);
    state.above_count = 0U;
    CHECK(av2h_coeff_contexts_from_state(
              &state, 0U, &leaf, 0U, &skip_context, &dc_sign_context) ==
          AV2H_INVALID_ARGUMENT);
}

int main(void) {
    test_fixture_prefixes();
    test_arguments();
    test_context_state();
    if (failures != 0) {
        fprintf(stderr, "%d coefficient checks failed\n", failures);
        return 1;
    }
    puts("coefficient unit: pass");
    return 0;
}