#include "decoder/transform.h"

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
                          Av2hFirstTxType expected_type,
                          uint8_t expected_primary_signaled,
                          uint8_t expected_primary_symbol,
                          uint8_t expected_secondary_signaled,
                          size_t expected_before_primary,
                          size_t expected_after_primary,
                          size_t expected_after_secondary) {
    Av2hSpan span = { bytes, size };
    Av2hSequenceState sequence = { 0 };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs partition_cdfs;
    Av2hLeafCdfs leaf_cdfs;
    Av2hFirstCoeffCdfs coeff_cdfs;
    Av2hFirstTransformCdfs transform_cdfs;
    Av2hFirstLeafPath path;
    Av2hFirstLeafMode mode;
    Av2hFirstCoeffPrefix prefix;
    Av2hFirstTransformType transform;
    Av2hStatus status;
    sequence.enable_sdp = 1U;
    sequence.enable_idtx_intra = 1U;
    sequence.enable_ist = 1U;
    CHECK(av2h_partition_cdfs_init(&partition_cdfs) == AV2H_OK);
    CHECK(av2h_leaf_cdfs_init(&leaf_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_cdfs_init(&coeff_cdfs) == AV2H_OK);
    CHECK(av2h_first_transform_cdfs_init(&transform_cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_first_leaf_read(
              &decoder, &partition_cdfs, width, height, 1U, &path) == AV2H_OK);
    CHECK(av2h_first_leaf_mode_read(
              &decoder, &leaf_cdfs, &sequence, &frame, &path.leaf, &mode) ==
          AV2H_OK);
    CHECK(av2h_first_coeff_prefix_read(
              &decoder, &coeff_cdfs, &frame, &path.leaf, &mode, &prefix) ==
          AV2H_OK);
    status = av2h_first_transform_type_read(
        &decoder, &transform_cdfs, &sequence, &frame, &mode, &prefix,
        &transform);
    if (status != AV2H_OK) {
        fprintf(stderr, "%s: transform status=%d tell=%zu\n", name, status,
                av2h_range_tell(&decoder));
        ++failures;
        return;
    }
    CHECK(transform.primary_type == expected_type);
    CHECK(transform.primary_signaled == expected_primary_signaled);
    CHECK(transform.primary_symbol == expected_primary_symbol);
    CHECK(transform.secondary_signaled == expected_secondary_signaled);
    CHECK(transform.secondary_symbol == 0U);
    CHECK(transform.before_primary_bits == expected_before_primary);
    CHECK(transform.after_primary_bits == expected_after_primary);
    CHECK(transform.after_secondary_bits == expected_after_secondary);
    CHECK(transform_cdfs.primary_size0[AV2H_PRIMARY_TX_TYPE_SYMBOLS] ==
          (expected_primary_signaled && expected_primary_symbol == 0U));
    CHECK(transform_cdfs.primary_size1[AV2H_PRIMARY_TX_TYPE_SYMBOLS] ==
          (expected_primary_signaled && expected_primary_symbol == 4U));
    CHECK(transform_cdfs.secondary_32x32[AV2H_SECONDARY_TX_TYPE_SYMBOLS] ==
          expected_secondary_signaled);
}

static void test_fixture_transform_types(void) {
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
                  AV2H_TX_DCT_DCT, 1U, 0U, 0U, 18U, 20U, 20U);
    check_fixture("odd", odd, sizeof(odd), 17U, 15U, odd_frame,
                  AV2H_TX_DCT_DCT, 1U, 0U, 0U, 24U, 26U, 26U);
    check_fixture("ordinary", ordinary, sizeof(ordinary), 160U, 120U,
                  ordinary_frame, AV2H_TX_DCT_DCT, 0U, 0U, 1U,
                  15U, 15U, 19U);
    check_fixture("high-detail", detail, sizeof(detail), 96U, 80U,
                  detail_frame, AV2H_TX_V_DCT, 1U, 4U, 0U, 29U, 34U, 34U);
}

static void test_arguments(void) {
    static const uint16_t primary_size2[] = {
        19009U, 6660U, 5080U, 2975U, 2503U, 1192U,
        0U, 0U, 2U, 2U, 5U
    };
    Av2hRangeDecoder decoder;
    Av2hFirstTransformCdfs cdfs;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform;
    CHECK(sizeof(Av2hFirstTransformCdfs) == 322U);
    CHECK(av2h_first_transform_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_transform_cdfs_init(&cdfs) == AV2H_OK);
    for (size_t index = 0U;
         index < sizeof(primary_size2) / sizeof(primary_size2[0]); ++index) {
        CHECK(cdfs.primary_size2[index] == primary_size2[index]);
    }
    CHECK(av2h_first_transform_type_read(
              0, &cdfs, &sequence, &frame, &mode, &prefix, &transform) ==
          AV2H_INVALID_ARGUMENT);
    frame.base_q_index = 128U;
    frame.reduced_tx_set = 1U;
    prefix.eob = 2U;
    CHECK(av2h_first_transform_type_read(
              &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
              &transform) == AV2H_UNSUPPORTED);
}

static void test_fsc_single_coefficient_uses_idtx(void) {
    static const unsigned char bytes[] = { 0U, 0U, 0U, 0U };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hRangeDecoder decoder;
    Av2hFirstTransformCdfs cdfs;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform;

    frame.base_q_index = 128U;
    mode.fsc_mode = 1U;
    prefix.tx_size = AV2H_TX_16X4;
    prefix.eob = 1U;
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_first_transform_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_first_transform_type_read(
              &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
              &transform) == AV2H_OK);
    CHECK(transform.primary_type == AV2H_TX_IDTX);
    CHECK(transform.primary_signaled == 0U);
    CHECK(transform.secondary_signaled == 0U);
}

static void check_primary_map(Av2hFirstTxSize tx_size,
                              Av2hFirstLeafIntraMode mode,
                              uint8_t long_side_symbol,
                              uint8_t primary_symbol,
                              Av2hFirstTxType expected_type) {
    Av2hFirstTxType tx_type = AV2H_TX_TYPE_INVALID;
    CHECK(av2h_primary_tx_type_map(
              tx_size, mode, long_side_symbol, primary_symbol, &tx_type) ==
          AV2H_OK);
    CHECK(tx_type == expected_type);
}

static void test_emerald_primary_mappings(void) {
    Av2hFirstTxType tx_type;
    check_primary_map(AV2H_TX_32X8, AV2H_INTRA_DC, 1U, 1U,
                      AV2H_TX_ADST_DCT);
    check_primary_map(AV2H_TX_32X32, AV2H_INTRA_SMOOTH, 0U, 0U,
                      AV2H_TX_DCT_DCT);
    check_primary_map(AV2H_TX_64X32, AV2H_INTRA_DC, 0U, 0U,
                      AV2H_TX_DCT_DCT);
    check_primary_map(AV2H_TX_32X64, AV2H_INTRA_DC, 0U, 0U,
                      AV2H_TX_DCT_DCT);
    check_primary_map(AV2H_TX_32X16, AV2H_INTRA_HORIZONTAL, 1U, 0U,
                      AV2H_TX_DCT_DCT);
    check_primary_map(AV2H_TX_16X64, AV2H_INTRA_D157, 1U, 0U,
                      AV2H_TX_DCT_DCT);
    check_primary_map(AV2H_TX_16X64, AV2H_INTRA_SMOOTH, 1U, 1U,
                      AV2H_TX_DCT_ADST);
    check_primary_map(AV2H_TX_64X16, AV2H_INTRA_SMOOTH_HORIZONTAL, 1U, 0U,
                      AV2H_TX_DCT_DCT);
    check_primary_map(AV2H_TX_16X16, AV2H_INTRA_SMOOTH, 0U, 1U,
                      AV2H_TX_ADST_ADST);

    check_primary_map(AV2H_TX_32X16, AV2H_INTRA_HORIZONTAL, 1U, 1U,
                      AV2H_TX_ADST_DCT);
    CHECK(av2h_primary_tx_type_map(
              AV2H_TX_64X32, AV2H_INTRA_DC, 1U, 0U, &tx_type) ==
          AV2H_UNSUPPORTED);
    CHECK(av2h_primary_tx_type_map(
              AV2H_TX_32X8, AV2H_INTRA_DC, 0U, 2U, &tx_type) ==
          AV2H_UNSUPPORTED);
    CHECK(av2h_primary_tx_type_map(
              AV2H_TX_32X8, AV2H_INTRA_DC, 1U, 1U, 0) ==
          AV2H_INVALID_ARGUMENT);
}

static void check_secondary_signal(Av2hFirstTxSize tx_size,
                                   Av2hFirstLeafIntraMode mode_value,
                                   uint16_t eob,
                                   Av2hFirstTxType primary_type,
                                   uint8_t expected) {
    Av2hSequenceState sequence = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix coeff = { 0 };
    uint8_t signaled = 0U;
    sequence.enable_ist = 1U;
    mode.mode = mode_value;
    coeff.tx_size = tx_size;
    coeff.eob = eob;
    CHECK(av2h_secondary_tx_type_signal(
              &sequence, &mode, &coeff, primary_type, &signaled) == AV2H_OK);
    CHECK(signaled == expected);
}

static void test_emerald_secondary_signaling(void) {
    Av2hSequenceState sequence = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix coeff = { 0 };
    uint8_t signaled;
    check_secondary_signal(
        AV2H_TX_32X32, AV2H_INTRA_SMOOTH, 19U, AV2H_TX_DCT_DCT, 1U);
    check_secondary_signal(
        AV2H_TX_64X32, AV2H_INTRA_DC, 6U, AV2H_TX_DCT_DCT, 1U);
    check_secondary_signal(
        AV2H_TX_64X32, AV2H_INTRA_HORIZONTAL, 58U, AV2H_TX_DCT_DCT, 0U);
    check_secondary_signal(
        AV2H_TX_64X16, AV2H_INTRA_DC, 5U, AV2H_TX_DCT_DCT, 1U);
    check_secondary_signal(
        AV2H_TX_64X16, AV2H_INTRA_SMOOTH_HORIZONTAL, 63U,
        AV2H_TX_DCT_DCT, 0U);
    check_secondary_signal(
        AV2H_TX_16X16, AV2H_INTRA_SMOOTH, 4U, AV2H_TX_ADST_ADST, 1U);
    check_secondary_signal(
        AV2H_TX_16X16, AV2H_INTRA_SMOOTH, 9U, AV2H_TX_ADST_ADST, 1U);
    check_secondary_signal(
        AV2H_TX_32X16, AV2H_INTRA_HORIZONTAL, 18U, AV2H_TX_DCT_DCT, 1U);
    check_secondary_signal(
        AV2H_TX_16X64, AV2H_INTRA_D157, 4U, AV2H_TX_DCT_DCT, 1U);
    check_secondary_signal(
        AV2H_TX_32X64, AV2H_INTRA_DC, 4U, AV2H_TX_DCT_DCT, 1U);
    check_secondary_signal(
        AV2H_TX_32X8, AV2H_INTRA_DC, 2U, AV2H_TX_ADST_DCT, 0U);

    CHECK(av2h_secondary_tx_type_signal(
              0, &mode, &coeff, AV2H_TX_DCT_DCT, &signaled) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_secondary_tx_type_signal(
              &sequence, &mode, &coeff, AV2H_TX_DCT_DCT, 0) ==
          AV2H_INVALID_ARGUMENT);
}

int main(void) {
    test_fixture_transform_types();
    test_fsc_single_coefficient_uses_idtx();
    test_emerald_primary_mappings();
    test_emerald_secondary_signaling();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d transform checks failed\n", failures);
        return 1;
    }
    puts("transform unit: pass");
    return 0;
}