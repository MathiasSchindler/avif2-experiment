#include "decoder/coeff_level.h"

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

static uint64_t coefficient_hash(const Av2hFirstCoeffBlock *block) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0U; index < block->coefficient_count; ++index) {
        hash ^= (uint32_t)block->quantized[index];
        hash *= UINT64_C(1099511628211);
    }
    for (index = 0U; index < block->coefficient_count; ++index) {
        hash ^= (uint32_t)block->dequantized[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t word_hash(const uint16_t *words, size_t count) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= words[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_words(uint64_t hash,
                           const uint16_t *words,
                           size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= words[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t complete_bank_hash(const Av2hFirstCoeffLevelCdfs *cdfs) {
    uint64_t hash = UINT64_C(1469598103934665603);
#define HASH_FIELD(field)                                                     \
    hash = hash_words(hash, &cdfs->field[0][0][0],                            \
                      sizeof(cdfs->field) / sizeof(uint16_t))
    HASH_FIELD(full_eob_base);
    HASH_FIELD(full_eob_base_lf);
    HASH_FIELD(full_base);
    HASH_FIELD(full_base_lf);
#undef HASH_FIELD
    hash = hash_words(hash, &cdfs->full_br[0][0],
                      sizeof(cdfs->full_br) / sizeof(uint16_t));
    return hash_words(hash, &cdfs->full_br_lf[0][0],
                      sizeof(cdfs->full_br_lf) / sizeof(uint16_t));
}

static uint64_t uv_bank_hash(const Av2hFirstCoeffLevelCdfs *cdfs) {
    uint64_t hash = UINT64_C(1469598103934665603);
#define HASH_UV_FIELD(field)                                                  \
    hash = hash_words(hash, &cdfs->field[0][0],                              \
                      sizeof(cdfs->field) / sizeof(uint16_t))
    HASH_UV_FIELD(full_uv_eob_base);
    HASH_UV_FIELD(full_uv_eob_base_lf);
    HASH_UV_FIELD(full_uv_base);
    HASH_UV_FIELD(full_uv_base_lf);
    HASH_UV_FIELD(full_uv_br);
#undef HASH_UV_FIELD
    return hash;
}

static void check_words(const uint16_t *actual,
                        const uint16_t *expected,
                        size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "CDF word %zu: actual %u, expected %u\n",
                    index, actual[index], expected[index]);
            ++failures;
        }
    }
}

static void test_q_context_cdfs(void) {
    static const uint16_t q_indices[] = { 64U, 128U, 160U, 224U };
    static const uint64_t complete_hashes[] = {
        UINT64_C(0x76cfaa1d19f5e4b8),
        UINT64_C(0x84b75b0d983a8dc3),
        UINT64_C(0xae7bb6b10aa8e7eb),
        UINT64_C(0x9551373a1d5217db)
    };
    static const uint64_t uv_hashes[] = {
        UINT64_C(0x4dbf06299d7c65ac),
        UINT64_C(0x25287573f266a67a),
        UINT64_C(0x30e92f0c0684ae2d),
        UINT64_C(0x4a25244a348191f7)
    };
    static const uint16_t q2_base_ctx0[] = {
        5584U, 96U, 20U, 0U, 0U, 3U, 4U, 4U
    };
    static const uint16_t q2_base_lf_ctx18[] = {
        17195U, 4126U, 850U, 196U, 50U, 0U, 0U, 3U, 4U, 4U
    };
    static const uint16_t q2_dc_sign_ctx0[] = {
        15193U, 0U, 0U, 3U, 3U, 4U
    };
    static const uint16_t q3_eob_lf_ctx0[] = {
        2636U, 554U, 214U, 101U, 0U, 0U, 3U, 3U, 3U
    };
    static const uint16_t q3_eob_lf_txs2_ctx0[] = {
        1420U, 262U, 114U, 44U, 0U, 0U, 3U, 3U, 4U
    };
    static const uint16_t q3_base_lf_ctx16[] = {
        9392U, 154U, 28U, 24U, 20U, 0U, 0U, 3U, 3U, 5U
    };
    static const uint16_t q3_eob_lf_txs2_ctx1[] = {
        467U, 65U, 24U, 20U, 0U, 0U, 4U, 5U, 5U
    };
    static const uint16_t q3_base_lf_txs2_ctx1[] = {
        31256U, 11985U, 4231U, 1535U, 738U, 0U, 0U, 3U, 4U, 5U
    };
    static const uint16_t q3_base_lf_txs2_ctx9[] = {
        10445U, 648U, 182U, 45U, 36U, 0U, 0U, 3U, 3U, 4U
    };
    static const uint16_t q3_base_lf_txs2_ctx10[] = {
        23701U, 4307U, 965U, 122U, 36U, 0U, 0U, 4U, 5U, 5U
    };
    static const uint16_t q1_base_txs3_ctx10[] = {
        1056U, 18U, 14U, 0U, 0U, 4U, 4U, 5U
    };
    static const uint16_t q1_base_lf_txs3_ctx20[] = {
        29375U, 22832U, 17500U, 13281U, 10301U,
        0U, 0U, 4U, 4U, 4U
    };
    static const uint16_t q1_base_lf_txs0_ctx11[] = {
        26380U, 15719U, 8349U, 4385U, 2175U,
        0U, 0U, 4U, 5U, 5U
    };
    static const uint16_t q3_dc_sign_ctx0[] = {
        12660U, 0U, 0U, 2U, 3U, 3U
    };
    Av2hFirstCoeffLevelCdfs cdfs;
    size_t index;
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(0, 160U) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(&cdfs, 160U) == AV2H_OK);
    CHECK(cdfs.q_context == 2U);
    check_words(cdfs.base[21], q2_base_ctx0, 8U);
    check_words(cdfs.base_lf[46], q2_base_lf_ctx18, 10U);
    check_words(cdfs.dc_sign[0], q2_dc_sign_ctx0, 6U);
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(&cdfs, 224U) == AV2H_OK);
    CHECK(cdfs.q_context == 3U);
    check_words(cdfs.eob_lf[8], q3_eob_lf_ctx0, 9U);
    check_words(cdfs.eob_lf[9], q3_eob_lf_txs2_ctx0, 9U);
    check_words(cdfs.base_lf[44], q3_base_lf_ctx16, 10U);
    check_words(cdfs.eob_lf[5], q3_eob_lf_txs2_ctx1, 9U);
    check_words(cdfs.base_lf[51], q3_base_lf_txs2_ctx1, 10U);
    check_words(cdfs.base_lf[54], q3_base_lf_txs2_ctx9, 10U);
    check_words(cdfs.base_lf[55], q3_base_lf_txs2_ctx10, 10U);
    check_words(cdfs.dc_sign[0], q3_dc_sign_ctx0, 6U);
    CHECK(cdfs.q3_uv_eob_lf[0][0] == 1423U);
    CHECK(cdfs.q3_uv_eob[1][0] == 781U);
    CHECK(cdfs.q3_uv_base_lf[0][0] == 24782U);
    CHECK(cdfs.q3_uv_base[0][0] == 6638U);
    CHECK(cdfs.q3_uv_br[0][0] == 5791U);
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(&cdfs, 96U) == AV2H_OK);
    CHECK(cdfs.q_context == 1U);
    check_words(cdfs.base[28], q1_base_txs3_ctx10, 8U);
    check_words(cdfs.base_lf[49], q1_base_lf_txs3_ctx20, 10U);
    check_words(cdfs.full_base_lf[0U][11U],
                q1_base_lf_txs0_ctx11, 10U);
    for (index = 0U; index < sizeof(q_indices) / sizeof(q_indices[0]);
         ++index) {
        CHECK(av2h_first_coeff_level_cdfs_init_for_q(
                  &cdfs, q_indices[index]) == AV2H_OK);
        CHECK(complete_bank_hash(&cdfs) == complete_hashes[index]);
        CHECK(uv_bank_hash(&cdfs) == uv_hashes[index]);
    }
}

static void test_fsc_cdf_banks(void) {
    static const uint16_t q_indices[] = { 64U, 128U, 160U, 224U };
    static const uint64_t expected[4U][4U] = {
        {
            UINT64_C(0x094cd69b88eba867),
            UINT64_C(0x807d3fbfa64a243d),
            UINT64_C(0xfa31f73af01078b2),
            UINT64_C(0xb5bb7e87141c0a52)
        },
        {
            UINT64_C(0x2edc927b1e1d2c5c),
            UINT64_C(0xf931960d4bd73cb6),
            UINT64_C(0x4ab36fde2612104b),
            UINT64_C(0x3b6e244a3b627032)
        },
        {
            UINT64_C(0x8131e83cef5e59db),
            UINT64_C(0x6c7b332b1c7842a4),
            UINT64_C(0x03e05f78b76f8151),
            UINT64_C(0x37f9104dc5f5178b)
        },
        {
            UINT64_C(0x256f7c1b9ce89afd),
            UINT64_C(0x5a70cf4625ac379f),
            UINT64_C(0xe6d905a45efe6172),
            UINT64_C(0x4c4d1c86e6c919c8)
        }
    };
    Av2hFirstCoeffLevelCdfs cdfs;
    size_t q_context;
    for (q_context = 0U; q_context < 4U; ++q_context) {
        CHECK(av2h_first_coeff_level_cdfs_init_for_q(
                  &cdfs, q_indices[q_context]) == AV2H_OK);
        CHECK(cdfs.q_context == q_context);
        CHECK(word_hash(&cdfs.fsc_bob_all[0][0][0], 3U * 3U * 7U) ==
              expected[q_context][0]);
        CHECK(word_hash(&cdfs.fsc_base_all[0][0][0], 3U * 7U * 8U) ==
              expected[q_context][1]);
        CHECK(word_hash(&cdfs.fsc_br_all[0][0][0], 3U * 7U * 8U) ==
              expected[q_context][2]);
        CHECK(word_hash(&cdfs.fsc_sign_all[0][0][0], 3U * 9U * 6U) ==
              expected[q_context][3]);
    }
}

static void test_fsc_coefficient_matrix(void) {
    static const unsigned char bytes[4096U] = { 0U };
    static const uint16_t q_indices[] = { 64U, 128U, 160U, 224U };
    static const uint8_t tx_sizes[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_8X16, AV2H_TX_16X4
    };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstTransformType transform = { 0 };
    size_t q_context;
    mode.fsc_mode = 1U;
    transform.primary_type = AV2H_TX_IDTX;
    for (q_context = 0U; q_context < 4U; ++q_context) {
        size_t tx_index;
        for (tx_index = 0U;
             tx_index < sizeof(tx_sizes) / sizeof(tx_sizes[0]); ++tx_index) {
            Av2hFrameState frame = { 0 };
            Av2hFirstCoeffPrefix prefix = { 0 };
            Av2hFirstCoeffLevelCdfs cdfs;
            Av2hFirstCoeffBlock block;
            Av2hCoeffTxGeometry geometry;
            Av2hRangeDecoder decoder;
            Av2hStatus status;
            frame.base_q_index = q_indices[q_context];
            frame.y_dc_delta_q = (int8_t)(q_context + 1U);
            prefix.tx_size = tx_sizes[tx_index];
            CHECK(av2h_coeff_tx_geometry(prefix.tx_size, &geometry) ==
                  AV2H_OK);
            prefix.eob = (uint16_t)geometry.coefficient_count;
            CHECK(av2h_first_coeff_level_cdfs_init_for_q(
                      &cdfs, frame.base_q_index) == AV2H_OK);
            CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
            status = av2h_first_coeff_block_read(
                &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
                &transform, &block);
            if (status != AV2H_OK) {
                fprintf(stderr, "FSC q%zu tx%u: status=%d kind=%u ctx=%u\n",
                        q_context, (unsigned int)prefix.tx_size, status,
                        block.diagnostic_kind, block.diagnostic_context);
                ++failures;
                continue;
            }
            CHECK(block.coefficient_count == geometry.coefficient_count);
            CHECK(block.nonzero_count != 0U);
            CHECK(block.after_levels_bits > block.before_levels_bits);
            CHECK(block.after_coefficients_bits >= block.after_levels_bits);
            CHECK(block.dequantized[0] == av2h_coeff_dequantize_level(
                      (uint32_t)(block.quantized[0] < 0 ?
                          -block.quantized[0] : block.quantized[0]),
                      (uint8_t)(block.quantized[0] < 0),
                      av2h_coeff_dequant_step_8bit(
                          frame.base_q_index, frame.y_dc_delta_q),
                      geometry.dequant_shift));
        }
    }
}

static void test_fsc_bob_context_scales_with_transform_area(void) {
    static const unsigned char bytes[4096U] = { 0U };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffLevelCdfs cdfs;
    Av2hFirstCoeffBlock block;
    Av2hCoeffTxGeometry geometry;
    Av2hRangeDecoder decoder;
    uint64_t before[3U];
    size_t context;
    frame.base_q_index = 128U;
    mode.fsc_mode = 1U;
    prefix.tx_size = AV2H_TX_8X4;
    transform.primary_type = AV2H_TX_IDTX;
    CHECK(av2h_coeff_tx_geometry(prefix.tx_size, &geometry) == AV2H_OK);
    prefix.eob = (uint16_t)(geometry.coefficient_count * 3U / 4U);
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(
              &cdfs, frame.base_q_index) == AV2H_OK);
    for (context = 0U; context < 3U; ++context) {
        before[context] = word_hash(
            cdfs.fsc_bob_all[geometry.tx_context][context], 7U);
    }
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_first_coeff_block_read(
              &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
              &transform, &block) == AV2H_OK);
    CHECK(word_hash(cdfs.fsc_bob_all[geometry.tx_context][0], 7U) ==
          before[0]);
    CHECK(word_hash(cdfs.fsc_bob_all[geometry.tx_context][1], 7U) !=
          before[1]);
    CHECK(word_hash(cdfs.fsc_bob_all[geometry.tx_context][2], 7U) ==
          before[2]);
}

static void test_adst_dct_coefficient_matrix(void) {
    static const unsigned char bytes[4096U] = { 0U };
    static const uint16_t q_indices[] = { 128U, 224U };
    static const uint8_t tx_sizes[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_4X16, AV2H_TX_16X4, AV2H_TX_8X8,
        AV2H_TX_8X16, AV2H_TX_16X8, AV2H_TX_16X16,
        AV2H_TX_32X4, AV2H_TX_32X8, AV2H_TX_32X16,
        AV2H_TX_64X4, AV2H_TX_64X8, AV2H_TX_64X16
    };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstTransformType transform = { 0 };
    size_t q_context;

    transform.primary_type = AV2H_TX_ADST_DCT;
    for (q_context = 0U;
         q_context < sizeof(q_indices) / sizeof(q_indices[0]); ++q_context) {
        size_t tx_index;
        for (tx_index = 0U;
             tx_index < sizeof(tx_sizes) / sizeof(tx_sizes[0]); ++tx_index) {
            Av2hFrameState frame = { 0 };
            Av2hFirstCoeffPrefix prefix = { 0 };
            Av2hFirstCoeffLevelCdfs cdfs;
            Av2hFirstCoeffBlock block;
            Av2hCoeffTxGeometry geometry;
            Av2hRangeDecoder decoder;
            Av2hStatus status;
            frame.base_q_index = q_indices[q_context];
            prefix.tx_size = tx_sizes[tx_index];
            prefix.eob = 1U;
            CHECK(av2h_coeff_tx_geometry(prefix.tx_size, &geometry) ==
                  AV2H_OK);
            CHECK(geometry.height <= 16U);
            CHECK(av2h_first_coeff_level_cdfs_init_for_q(
                      &cdfs, frame.base_q_index) == AV2H_OK);
            CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
            status = av2h_first_coeff_block_read(
                &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
                &transform, &block);
            if (status != AV2H_OK) {
                fprintf(stderr, "ADST_DCT q%zu tx%u: status=%d\n",
                        q_context, (unsigned int)prefix.tx_size, status);
                ++failures;
                continue;
            }
            CHECK(block.coefficient_count == geometry.coefficient_count);
            CHECK(block.after_coefficients_bits >= block.after_levels_bits);
        }
    }
}

static void test_adst_flipadst_coefficient_matrix(void) {
    static const unsigned char bytes[4096U] = { 0U };
    static const uint16_t q_indices[] = { 128U, 224U };
    static const uint8_t tx_sizes[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_4X16, AV2H_TX_16X4, AV2H_TX_8X8,
        AV2H_TX_8X16, AV2H_TX_16X8, AV2H_TX_16X16
    };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstTransformType transform = { 0 };
    size_t q_context;

    transform.primary_type = AV2H_TX_ADST_FLIPADST;
    for (q_context = 0U;
         q_context < sizeof(q_indices) / sizeof(q_indices[0]); ++q_context) {
        size_t tx_index;
        for (tx_index = 0U;
             tx_index < sizeof(tx_sizes) / sizeof(tx_sizes[0]); ++tx_index) {
            Av2hFrameState frame = { 0 };
            Av2hFirstCoeffPrefix prefix = { 0 };
            Av2hFirstCoeffLevelCdfs cdfs;
            Av2hFirstCoeffBlock block;
            Av2hCoeffTxGeometry geometry;
            Av2hRangeDecoder decoder;
            Av2hStatus status;
            frame.base_q_index = q_indices[q_context];
            prefix.tx_size = tx_sizes[tx_index];
            prefix.eob = 1U;
            CHECK(av2h_coeff_tx_geometry(prefix.tx_size, &geometry) ==
                  AV2H_OK);
            CHECK(geometry.width <= 16U);
            CHECK(geometry.height <= 16U);
            CHECK(av2h_first_coeff_level_cdfs_init_for_q(
                      &cdfs, frame.base_q_index) == AV2H_OK);
            CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
            status = av2h_first_coeff_block_read(
                &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
                &transform, &block);
            if (status != AV2H_OK) {
                fprintf(stderr, "ADST_FLIPADST q%zu tx%u: status=%d\n",
                        q_context, (unsigned int)prefix.tx_size, status);
                ++failures;
                continue;
            }
            CHECK(block.coefficient_count == geometry.coefficient_count);
            CHECK(block.after_coefficients_bits >= block.after_levels_bits);
        }
    }
}

static void test_requested_transform_coefficient_matrix(void) {
    static const unsigned char bytes[4096U] = { 0U };
    static const uint16_t q_indices[] = { 128U, 224U };
    static const uint8_t flipadst_adst[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_4X16, AV2H_TX_16X4, AV2H_TX_8X8,
        AV2H_TX_8X16, AV2H_TX_16X8, AV2H_TX_16X16
    };
    static const uint8_t h_adst[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_8X8, AV2H_TX_16X4, AV2H_TX_16X8
    };
    static const uint8_t v_adst[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_16X4, AV2H_TX_16X8
    };
    static const uint8_t dct_flipadst[] = {
        AV2H_TX_4X4, AV2H_TX_8X4, AV2H_TX_16X4,
        AV2H_TX_4X16, AV2H_TX_8X8, AV2H_TX_16X8,
        AV2H_TX_8X16, AV2H_TX_4X32, AV2H_TX_8X32,
        AV2H_TX_16X32
    };
    static const uint8_t v_dct[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4, AV2H_TX_8X8,
        AV2H_TX_4X16, AV2H_TX_8X16, AV2H_TX_16X4, AV2H_TX_4X32
    };
    static const uint8_t flipadst_flipadst[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4, AV2H_TX_8X8,
        AV2H_TX_4X16, AV2H_TX_8X16, AV2H_TX_16X4, AV2H_TX_16X8
    };
    static const uint8_t h_dct[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_16X4, AV2H_TX_32X4
    };
    static const uint8_t flipadst_dct[] = {
        AV2H_TX_4X4, AV2H_TX_8X4, AV2H_TX_16X4, AV2H_TX_32X4,
        AV2H_TX_4X16, AV2H_TX_8X8, AV2H_TX_16X8, AV2H_TX_16X16
    };
    static const uint8_t v_flipadst[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4
    };
    static const uint8_t h_flipadst[] = {
        AV2H_TX_4X4, AV2H_TX_4X8, AV2H_TX_8X4,
        AV2H_TX_8X8, AV2H_TX_16X8
    };
    static const struct {
        Av2hFirstTxType type;
        const uint8_t *sizes;
        size_t count;
    } matrices[] = {
        { AV2H_TX_FLIPADST_ADST, flipadst_adst,
          sizeof(flipadst_adst) / sizeof(flipadst_adst[0]) },
        { AV2H_TX_H_ADST, h_adst,
                    sizeof(h_adst) / sizeof(h_adst[0]) },
                { AV2H_TX_V_ADST, v_adst,
                    sizeof(v_adst) / sizeof(v_adst[0]) },
                { AV2H_TX_DCT_FLIPADST, dct_flipadst,
                    sizeof(dct_flipadst) / sizeof(dct_flipadst[0]) },
                { AV2H_TX_V_DCT, v_dct,
                    sizeof(v_dct) / sizeof(v_dct[0]) },
                { AV2H_TX_FLIPADST_FLIPADST, flipadst_flipadst,
                    sizeof(flipadst_flipadst) / sizeof(flipadst_flipadst[0]) },
                { AV2H_TX_H_DCT, h_dct,
                    sizeof(h_dct) / sizeof(h_dct[0]) },
                { AV2H_TX_FLIPADST_DCT, flipadst_dct,
                    sizeof(flipadst_dct) / sizeof(flipadst_dct[0]) },
                { AV2H_TX_V_FLIPADST, v_flipadst,
                    sizeof(v_flipadst) / sizeof(v_flipadst[0]) },
                { AV2H_TX_H_FLIPADST, h_flipadst,
                    sizeof(h_flipadst) / sizeof(h_flipadst[0]) }
    };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    size_t matrix;

    for (matrix = 0U; matrix < sizeof(matrices) / sizeof(matrices[0]);
         ++matrix) {
        size_t q_context;
        for (q_context = 0U;
             q_context < sizeof(q_indices) / sizeof(q_indices[0]);
             ++q_context) {
            size_t tx_index;
            for (tx_index = 0U; tx_index < matrices[matrix].count;
                 ++tx_index) {
                Av2hFrameState frame = { 0 };
                Av2hFirstCoeffPrefix prefix = { 0 };
                Av2hFirstTransformType transform = { 0 };
                Av2hFirstCoeffLevelCdfs cdfs;
                Av2hFirstCoeffBlock block;
                Av2hCoeffTxGeometry geometry;
                Av2hRangeDecoder decoder;
                Av2hStatus status;
                frame.base_q_index = q_indices[q_context];
                prefix.tx_size = matrices[matrix].sizes[tx_index];
                prefix.eob = 1U;
                transform.primary_type = matrices[matrix].type;
                CHECK(av2h_coeff_tx_geometry(prefix.tx_size, &geometry) ==
                      AV2H_OK);
                CHECK(av2h_first_coeff_level_cdfs_init_for_q(
                          &cdfs, frame.base_q_index) == AV2H_OK);
                CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
                status = av2h_first_coeff_block_read(
                    &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
                    &transform, &block);
                CHECK(status == AV2H_OK);
                if (status != AV2H_OK) continue;
                CHECK(block.coefficient_count == geometry.coefficient_count);
                CHECK(block.after_coefficients_bits >=
                      block.after_levels_bits);
            }
        }
    }
}

static void test_dequantized_clamp(void) {
    CHECK(av2h_coeff_clamp_dequantized_8bit(32766U, 0U) == 32766);
    CHECK(av2h_coeff_clamp_dequantized_8bit(32768U, 0U) == 32767);
    CHECK(av2h_coeff_clamp_dequantized_8bit(32767U, 1U) == -32767);
    CHECK(av2h_coeff_clamp_dequantized_8bit(32768U, 1U) == -32768);
    CHECK(av2h_coeff_clamp_dequantized_8bit(UINT64_MAX, 1U) == -32768);
}

static void check_fixture(const char *name,
                          const unsigned char *bytes,
                          size_t size,
                          uint16_t width,
                          uint16_t height,
                          Av2hFrameState frame,
                          uint16_t expected_coefficients,
                          uint16_t expected_nonzero,
                          int32_t expected_dc,
                          size_t expected_before,
                          size_t expected_after_levels,
                          size_t expected_after_coefficients,
                          uint64_t expected_hash) {
    Av2hSpan span = { bytes, size };
    Av2hSequenceState sequence = { 0 };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs partition_cdfs;
    Av2hLeafCdfs leaf_cdfs;
    Av2hFirstCoeffCdfs coeff_cdfs;
    Av2hFirstTransformCdfs transform_cdfs;
    Av2hFirstCoeffLevelCdfs level_cdfs;
    Av2hFirstLeafPath path;
    Av2hFirstLeafMode mode;
    Av2hFirstCoeffPrefix prefix;
    Av2hFirstTransformType transform;
    Av2hFirstCoeffBlock block;
    Av2hStatus status;
    sequence.enable_sdp = 1U;
    sequence.enable_idtx_intra = 1U;
    sequence.enable_ist = 1U;
    CHECK(av2h_partition_cdfs_init(&partition_cdfs) == AV2H_OK);
    CHECK(av2h_leaf_cdfs_init(&leaf_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_cdfs_init(&coeff_cdfs) == AV2H_OK);
    CHECK(av2h_first_transform_cdfs_init(&transform_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_level_cdfs_init(&level_cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_first_leaf_read(
              &decoder, &partition_cdfs, width, height, 1U, &path) == AV2H_OK);
    CHECK(av2h_first_leaf_mode_read(
              &decoder, &leaf_cdfs, &sequence, &frame, &path.leaf, &mode) ==
          AV2H_OK);
    CHECK(av2h_first_coeff_prefix_read(
              &decoder, &coeff_cdfs, &frame, &path.leaf, &mode, &prefix) ==
          AV2H_OK);
    CHECK(av2h_first_transform_type_read(
              &decoder, &transform_cdfs, &sequence, &frame, &mode, &prefix,
              &transform) == AV2H_OK);
    status = av2h_first_coeff_block_read(
        &decoder, &level_cdfs, &sequence, &frame, &mode, &prefix, &transform,
        &block);
    if (status != AV2H_OK) {
        fprintf(stderr, "%s: coefficient level status=%d tell=%zu\n", name,
                status, av2h_range_tell(&decoder));
        ++failures;
        return;
    }
    CHECK(block.coefficient_count == expected_coefficients);
    CHECK(block.nonzero_count == expected_nonzero);
    CHECK(block.dc_value == expected_dc);
    CHECK(block.cul_level == (expected_dc > 0 ? 23U : 15U));
    CHECK(block.before_levels_bits == expected_before);
    CHECK(block.after_levels_bits == expected_after_levels);
    CHECK(block.after_coefficients_bits == expected_after_coefficients);
    CHECK(coefficient_hash(&block) == expected_hash);
}

static void test_fixture_coefficients(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U,
        0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U, 0x35U,
        0x9cU, 0x23U, 0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU, 0x83U,
        0x17U, 0x25U, 0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU, 0xdbU,
        0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU, 0xf5U
    };
    static const unsigned char odd[] = {
        0xf3U, 0x99U, 0x9cU, 0x59U, 0x42U, 0xfbU, 0xe6U, 0x50U,
        0x15U, 0x7dU, 0x6eU, 0x92U, 0x8aU, 0x57U, 0x4aU, 0x49U,
        0xd0U, 0xb7U, 0x83U, 0xaeU, 0x2fU, 0x1dU, 0x7fU, 0xcaU,
        0x2dU, 0xd3U, 0x46U, 0xf6U, 0x12U, 0x54U, 0xa5U, 0x0aU,
        0x1fU, 0xb1U, 0x8eU, 0xe1U, 0x60U, 0x41U, 0xa3U, 0xf1U,
        0x29U, 0xb4U, 0x5bU, 0xf3U, 0xa4U, 0x7cU, 0x8eU, 0x9aU
    };
    static const unsigned char ordinary[] = {
        0xcdU, 0x77U, 0xc5U, 0x58U, 0x5fU, 0x4dU, 0x54U, 0x7fU,
        0xf3U, 0x9eU, 0xfcU, 0x9cU, 0x09U, 0x95U, 0xb7U, 0x37U,
        0xbdU, 0x19U, 0x38U, 0x54U, 0x85U, 0x67U, 0x37U, 0x09U,
        0xb1U, 0x96U, 0x89U, 0xb7U, 0x9aU, 0xb9U, 0xbaU, 0x98U,
        0xaeU, 0xd3U, 0x6cU, 0x14U, 0x15U, 0x2bU, 0xffU, 0xf8U,
        0xdcU, 0x8eU, 0xb8U, 0x66U, 0xd7U, 0x14U, 0x09U, 0x49U
    };
    static const unsigned char detail[] = {
        0xc7U, 0x43U, 0xf5U, 0xb7U, 0xcbU, 0xdeU, 0xd0U, 0x78U,
        0xe8U, 0xfdU, 0x2dU, 0x87U, 0x60U, 0xdaU, 0x52U, 0x31U,
        0xafU, 0x94U, 0xd1U, 0xf3U, 0x35U, 0x52U, 0x34U, 0xd0U,
        0x2fU, 0x39U, 0x3bU, 0x8cU, 0x8eU, 0x8dU, 0x46U, 0x08U,
        0x0fU, 0x5eU, 0x77U, 0x93U, 0x7eU, 0x48U, 0x4fU, 0xcbU,
        0x8cU, 0xe1U, 0xb3U, 0x2dU, 0x88U, 0x32U, 0xc3U, 0x4aU
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
                  32U, 4U, -8, 20U, 43U, 51U,
                  UINT64_C(0x06677bdba1ac3fc7));
    check_fixture("odd", odd, sizeof(odd), 17U, 15U, odd_frame,
                  32U, 4U, -8, 26U, 49U, 57U,
                  UINT64_C(0x06677bdba1ac3fc7));
    check_fixture("ordinary", ordinary, sizeof(ordinary), 160U, 120U,
                  ordinary_frame, 1024U, 8U, -44, 19U, 64U, 96U,
                  UINT64_C(0xf82640e34ed0b9c9));
    check_fixture("high-detail", detail, sizeof(detail), 96U, 80U,
                  detail_frame, 64U, 54U, 1, 34U, 249U, 314U,
                  UINT64_C(0x1fe5fa5f0e64c0f3));
}

/*
 * Emerald Q-context-3 remaining-luma coefficient evidence (base_q_index = 208,
 * emerald-192x96-qp208.obu, traversal indices 5..19).  Pinned directly from the
 * authoritative per-block trace JSON: transform geometry, exact quantized and
 * dequantized coefficient arrays, non-zero counts, DC values, the normative
 * running cul_level, and the trace's informational cul_level_capped
 * (sum(min(|level|,15))).  These exercise the enum-independent coefficient
 * machinery for every unique reached transform-context / geometry path,
 * including the sizes (TX_64X32, TX_32X16, TX_16X64) whose enum names the
 * transform-geometry package has not yet added to coeff.h. */
typedef struct { uint16_t pos; int32_t q; int32_t dq; } TraceCoeff;
typedef struct {
    const char *name; uint16_t tx_w, tx_h, exp_w, exp_h;
    uint8_t exp_ctx, exp_shift; uint32_t exp_count; uint8_t all_zero;
    uint16_t nonzero; int32_t dc_q, dc_dq; uint32_t sum_levels;
    uint8_t cul_level; uint16_t cul_capped;
    const TraceCoeff *coeffs; size_t coeff_count;
} TraceBlock;
static const TraceCoeff coeffs_5[] = { {0U,4,7936}, {32U,1,1984} };
static const TraceCoeff coeffs_6[] = { {0U,-1,-992}, {2U,1,992}, {3U,1,992}, {33U,-1,-992}, {64U,1,992}, {65U,1,992}, {66U,-1,-992}, {67U,-1,-992}, {96U,1,992}, {97U,1,992}, {129U,-1,-992} };
static const TraceCoeff coeffs_7[] = { {0U,-2,-992}, {1U,1,496}, {2U,-1,-496} };
static const TraceCoeff coeffs_8[] = { {1U,1,992}, {32U,-1,-992}, {33U,-1,-992}, {64U,1,992} };
static const TraceCoeff coeffs_10[] = { {0U,-1,-1984}, {1U,-1,-1984}, {18U,1,1984}, {32U,1,1984} };
static const TraceCoeff coeffs_11[] = { {0U,2,1984}, {1U,-1,-992}, {2U,-1,-992}, {32U,-1,-992}, {33U,1,992}, {34U,1,992}, {64U,-1,-992}, {98U,-1,-992}, {160U,1,992} };
static const TraceCoeff coeffs_12[] = { {0U,-1,-992}, {32U,1,992} };
static const TraceCoeff coeffs_14[] = { {0U,1,496}, {1U,-1,-496}, {64U,1,496} };
static const TraceCoeff coeffs_15[] = { {1U,1,496}, {3U,-1,-496}, {4U,1,496}, {33U,1,496}, {34U,-1,-496}, {35U,-1,-496}, {36U,1,496}, {64U,-1,-496}, {66U,-1,-496}, {96U,-1,-496}, {128U,1,496}, {131U,1,496}, {160U,-1,-496}, {192U,-1,-496}, {194U,1,496}, {224U,1,496}, {257U,1,496}, {258U,1,496} };
static const TraceCoeff coeffs_16[] = { {3U,2,992}, {5U,-1,-496}, {32U,-2,-992}, {34U,1,496}, {35U,1,496}, {36U,-1,-496}, {37U,-1,-496}, {65U,1,496}, {66U,-1,-496}, {96U,1,496}, {132U,1,496}, {226U,1,496} };
static const TraceCoeff coeffs_17[] = { {1U,-1,-992}, {34U,1,992}, {35U,-1,-992}, {64U,1,992}, {65U,1,992}, {66U,-1,-992} };
static const TraceCoeff coeffs_18[] = { {0U,2,1984}, {1U,-1,-992}, {32U,-1,-992}, {33U,1,992}, {66U,-1,-992}, {96U,-1,-992}, {160U,1,992} };
static const TraceCoeff coeffs_19[] = { {1U,1,992}, {32U,-2,-1984}, {33U,-1,-992}, {66U,-1,-992}, {103U,1,992}, {129U,1,992}, {132U,-1,-992}, {162U,1,992} };
static const TraceBlock trace_blocks[] = {
    { "(14,0)", 32U, 8U, 32U, 8U, 2U, 0U, 256U, 0U, 2U, 4, 7936, 5U, 21U, 5U, coeffs_5, sizeof(coeffs_5)/sizeof(coeffs_5[0]) },
    { "(8,8)", 32U, 32U, 32U, 32U, 3U, 1U, 1024U, 0U, 11U, -1, -992, 11U, 15U, 11U, coeffs_6, sizeof(coeffs_6)/sizeof(coeffs_6[0]) },
    { "(0,16)", 64U, 32U, 32U, 32U, 4U, 2U, 1024U, 0U, 3U, -2, -992, 4U, 12U, 4U, coeffs_7, sizeof(coeffs_7)/sizeof(coeffs_7[0]) },
    { "(8,16)", 64U, 16U, 32U, 16U, 3U, 1U, 512U, 0U, 4U, 0, 0, 4U, 4U, 4U, coeffs_8, sizeof(coeffs_8)/sizeof(coeffs_8[0]) },
    { "(12,16)", 16U, 16U, 16U, 16U, 2U, 0U, 256U, 1U, 0U, 0, 0, 0U, 0U, 0U, 0, 0U },
    { "(12,20)", 16U, 16U, 16U, 16U, 2U, 0U, 256U, 0U, 4U, -1, -1984, 4U, 12U, 4U, coeffs_10, sizeof(coeffs_10)/sizeof(coeffs_10[0]) },
    { "(12,24)", 32U, 16U, 32U, 16U, 3U, 1U, 512U, 0U, 9U, 2, 1984, 10U, 23U, 10U, coeffs_11, sizeof(coeffs_11)/sizeof(coeffs_11[0]) },
    { "(0,32)", 16U, 64U, 16U, 32U, 3U, 1U, 512U, 0U, 2U, -1, -992, 2U, 10U, 2U, coeffs_12, sizeof(coeffs_12)/sizeof(coeffs_12[0]) },
    { "(0,36)", 16U, 64U, 16U, 32U, 3U, 1U, 512U, 1U, 0U, 0, 0, 0U, 0U, 0U, 0, 0U },
    { "(0,40)", 32U, 64U, 32U, 32U, 4U, 2U, 1024U, 0U, 3U, 1, 496, 3U, 19U, 3U, coeffs_14, sizeof(coeffs_14)/sizeof(coeffs_14[0]) },
    { "(16,0)", 64U, 32U, 32U, 32U, 4U, 2U, 1024U, 0U, 18U, 0, 0, 18U, 7U, 18U, coeffs_15, sizeof(coeffs_15)/sizeof(coeffs_15[0]) },
    { "(16,16)", 64U, 32U, 32U, 32U, 4U, 2U, 1024U, 0U, 12U, 0, 0, 14U, 7U, 14U, coeffs_16, sizeof(coeffs_16)/sizeof(coeffs_16[0]) },
    { "(16,32)", 32U, 16U, 32U, 16U, 3U, 1U, 512U, 0U, 6U, 0, 0, 6U, 6U, 6U, coeffs_17, sizeof(coeffs_17)/sizeof(coeffs_17[0]) },
    { "(16,40)", 32U, 16U, 32U, 16U, 3U, 1U, 512U, 0U, 7U, 2, 1984, 8U, 23U, 8U, coeffs_18, sizeof(coeffs_18)/sizeof(coeffs_18[0]) },
    { "(20,32)", 64U, 16U, 32U, 16U, 3U, 1U, 512U, 0U, 8U, 0, 0, 9U, 7U, 9U, coeffs_19, sizeof(coeffs_19)/sizeof(coeffs_19[0]) },
};

static void test_emerald_remaining_luma_blocks(void) {
    /* Emerald frame quantiser: base_q_index 208 -> q-context 3; DC and AC share
     * dequant step 15872 (y_dc_delta_q == 0). */
    const uint32_t step = av2h_coeff_dequant_step_8bit(208U, 0);
    Av2hFirstCoeffLevelCdfs cdfs;
    size_t block_index;
    CHECK(step == 15872U);
    CHECK(av2h_coeff_dequantize_level(1U, 0U, step, 1U) == 992);
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(&cdfs, 208U) == AV2H_OK);
    CHECK(cdfs.q_context == 3U);
    for (block_index = 0U;
         block_index < sizeof(trace_blocks) / sizeof(trace_blocks[0]);
         ++block_index) {
        const TraceBlock *tb = &trace_blocks[block_index];
        Av2hCoeffTxGeometry geometry;
        uint32_t summed = 0U;
        uint32_t summed_capped = 0U;
        size_t coeff_index;
        CHECK(av2h_coeff_tx_geometry_from_dims(
                  tb->tx_w, tb->tx_h, &geometry) == AV2H_OK);
        if (geometry.width != tb->exp_w || geometry.height != tb->exp_h ||
            geometry.tx_context != tb->exp_ctx ||
            geometry.dequant_shift != tb->exp_shift ||
            geometry.coefficient_count != tb->exp_count) {
            fprintf(stderr, "%s: geometry mismatch w=%u h=%u ctx=%u sh=%u n=%u\n",
                    tb->name, geometry.width, geometry.height,
                    geometry.tx_context, geometry.dequant_shift,
                    geometry.coefficient_count);
            ++failures;
        }
        for (coeff_index = 0U; coeff_index < tb->coeff_count; ++coeff_index) {
            const TraceCoeff *c = &tb->coeffs[coeff_index];
            uint8_t sign = (uint8_t)(c->q < 0);
            uint32_t level = (uint32_t)(c->q < 0 ? -c->q : c->q);
            int32_t dq = av2h_coeff_dequantize_level(
                level, sign, step, tb->exp_shift);
            if (dq != c->dq) {
                fprintf(stderr, "%s: pos %u q %d dequant got %d exp %d\n",
                        tb->name, c->pos, c->q, dq, c->dq);
                ++failures;
            }
            summed += level;
            summed_capped += level > 15U ? 15U : level;
        }
        if (tb->all_zero) {
            CHECK(tb->coeff_count == 0U);
            continue;
        }
        CHECK(tb->coeff_count == tb->nonzero);
        CHECK(summed == tb->sum_levels);
        CHECK(summed_capped == tb->cul_capped);
        CHECK(av2h_coeff_finalize_cul_level(summed, tb->dc_q) ==
              tb->cul_level);
    }
}

static void test_tx_geometry_helpers(void) {
    Av2hCoeffTxGeometry geometry;
    /* Enum wrapper mirrors the pure-dimension mapping for every currently
     * defined transform size reached in the Emerald luma set. */
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_32X8, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 2U && geometry.dequant_shift == 0U &&
          geometry.coefficient_count == 256U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_32X32, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 3U && geometry.dequant_shift == 1U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_64X16, &geometry) == AV2H_OK);
    CHECK(geometry.width == 32U && geometry.height == 16U &&
          geometry.tx_context == 3U && geometry.dequant_shift == 1U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_32X64, &geometry) == AV2H_OK);
    CHECK(geometry.width == 32U && geometry.height == 32U &&
          geometry.tx_context == 4U && geometry.dequant_shift == 2U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_16X16, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 2U && geometry.dequant_shift == 0U);
    /* TX_8X16 averages its 8x8 and 16x16 square classes. */
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_8X16, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 2U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_4X64, &geometry) == AV2H_OK);
    CHECK(geometry.width == 4U && geometry.height == 32U &&
          geometry.coefficient_count == 128U && geometry.tx_context == 2U &&
          geometry.dequant_shift == 0U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_64X4, &geometry) == AV2H_OK);
    CHECK(geometry.width == 32U && geometry.height == 4U &&
          geometry.coefficient_count == 128U && geometry.tx_context == 2U &&
          geometry.dequant_shift == 0U);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_INVALID, &geometry) ==
          AV2H_UNSUPPORTED);
    CHECK(av2h_coeff_tx_geometry(AV2H_TX_4X4, 0) == AV2H_INVALID_ARGUMENT);
    /* Dimension helper validates power-of-two transform sides in [4,64]. */
    CHECK(av2h_coeff_tx_geometry_from_dims(0U, 8U, &geometry) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_coeff_tx_geometry_from_dims(12U, 8U, &geometry) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_coeff_tx_geometry_from_dims(128U, 8U, &geometry) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_coeff_tx_geometry_from_dims(8U, 8U, 0) ==
          AV2H_INVALID_ARGUMENT);
    /* Missing-enum sizes still resolve through the dimension helper, matching
     * the integration adaptation documented in coeff_level.h. */
    CHECK(av2h_coeff_tx_geometry_from_dims(64U, 32U, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 4U && geometry.dequant_shift == 2U &&
          geometry.coefficient_count == 1024U);
    CHECK(av2h_coeff_tx_geometry_from_dims(32U, 16U, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 3U && geometry.dequant_shift == 1U &&
          geometry.coefficient_count == 512U);
    CHECK(av2h_coeff_tx_geometry_from_dims(16U, 64U, &geometry) == AV2H_OK);
    CHECK(geometry.tx_context == 3U && geometry.dequant_shift == 1U &&
          geometry.coefficient_count == 512U);
}

static void test_dequant_helper_edges(void) {
    /* Exact 24-bit working range and rounding of the shared dequant helper. */
    CHECK(av2h_coeff_dequantize_level(4U, 0U, 15872U, 0U) == 7936);
    CHECK(av2h_coeff_dequantize_level(1U, 1U, 15872U, 2U) == -496);
    CHECK(av2h_coeff_dequantize_level(0U, 0U, 15872U, 0U) == 0);
    /* Signed clamp at the 8-bit dequantized limits. */
    CHECK(av2h_coeff_dequantize_level(32768U, 0U, 8U, 0U) == 32767);
    CHECK(av2h_coeff_dequantize_level(32768U, 1U, 8U, 0U) == -32768);
    /* cul_level DC-sign flags mirror set_dc_sign(). */
    CHECK(av2h_coeff_finalize_cul_level(3U, 5) == 3U + 16U);
    CHECK(av2h_coeff_finalize_cul_level(3U, -5) == (3U | 8U));
    CHECK(av2h_coeff_finalize_cul_level(18U, 0) == 7U);
}

static void test_chroma_all_zero_geometry(void) {
    static const Av2hFirstTxSize sizes[] = {
        AV2H_TX_4X4, AV2H_TX_32X32, AV2H_TX_64X32, AV2H_TX_32X64
    };
    static const uint16_t widths[] = { 4U, 32U, 32U, 32U };
    static const uint16_t heights[] = { 4U, 32U, 32U, 32U };
    static const uint16_t counts[] = { 16U, 1024U, 1024U, 1024U };
    static const unsigned char bytes[] = { 0U, 0U, 0U, 0U };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hFirstCoeffLevelCdfs cdfs;
    Av2hRangeDecoder decoder;
    size_t index;
    frame.base_q_index = 160U;
    CHECK(av2h_first_coeff_level_cdfs_init_for_q(&cdfs, 160U) == AV2H_OK);
    for (index = 0U; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        Av2hFirstCoeffPrefix prefix = { 0 };
        Av2hFirstCoeffBlock block;
        CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
        prefix.tx_size = sizes[index];
        prefix.all_zero = 1U;
        CHECK(av2h_first_chroma_coeff_block_read(
                  &decoder, &cdfs, &sequence, &frame, 1U, &prefix,
                  &block) == AV2H_OK);
        CHECK(block.width == widths[index]);
        CHECK(block.height == heights[index]);
        CHECK(block.coefficient_count == counts[index]);
        CHECK(block.nonzero_count == 0U);
        CHECK(block.before_levels_bits == block.after_coefficients_bits);
    }
}

static void test_arguments(void) {
    Av2hFirstCoeffLevelCdfs cdfs;
    Av2hRangeDecoder decoder;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block;
    CHECK(sizeof(Av2hFirstCoeffLevelCdfs) == 20464U);
    CHECK(av2h_first_coeff_level_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_coeff_block_read(
              0, &cdfs, &sequence, &frame, &mode, &prefix, &transform,
              &block) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_coeff_level_cdfs_init(&cdfs) == AV2H_OK);
    frame.base_q_index = 160U;
    mode.fsc_mode = 1U;
    prefix.tx_size = AV2H_TX_4X4;
    prefix.eob = 1U;
    transform.primary_type = AV2H_TX_IDTX;
    CHECK(av2h_first_coeff_block_read(
              &decoder, &cdfs, &sequence, &frame, &mode, &prefix,
              &transform, &block) == AV2H_UNSUPPORTED);
    mode.fsc_mode = 0U;
    sequence.enable_tcq = 1U;
    frame.base_q_index = 128U;
    prefix.eob = 1U;
    CHECK(av2h_first_coeff_block_read(
              &decoder, &cdfs, &sequence, &frame, &mode, &prefix, &transform,
              &block) == AV2H_UNSUPPORTED);
}

int main(void) {
    test_q_context_cdfs();
    test_chroma_all_zero_geometry();
    test_fsc_cdf_banks();
    test_fsc_coefficient_matrix();
    test_fsc_bob_context_scales_with_transform_area();
    test_adst_dct_coefficient_matrix();
    test_adst_flipadst_coefficient_matrix();
    test_requested_transform_coefficient_matrix();
    test_dequantized_clamp();
    test_tx_geometry_helpers();
    test_dequant_helper_edges();
    test_emerald_remaining_luma_blocks();
    test_fixture_coefficients();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d coefficient level checks failed\n", failures);
        return 1;
    }
    puts("coefficient level unit: pass");
    return 0;
}