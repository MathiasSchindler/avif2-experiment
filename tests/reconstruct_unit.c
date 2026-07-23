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

static uint64_t pixel_hash(const Av2hFirstReconstruction *reconstruction) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    size_t count = (size_t)reconstruction->width * reconstruction->height;
    for (index = 0U; index < count; ++index) {
        hash ^= reconstruction->pixels[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void check_fixture(const char *name,
                          const unsigned char *bytes,
                          size_t size,
                          uint16_t width,
                          uint16_t height,
                          Av2hFrameState frame,
                          uint64_t expected_hash,
                          const uint8_t *expected_pixels,
                          size_t expected_pixel_count) {
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
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    Av2hStatus status;
    size_t index;
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
    CHECK(av2h_first_coeff_block_read(
              &decoder, &level_cdfs, &sequence, &frame, &mode, &prefix,
              &transform, &block) == AV2H_OK);
    status = av2h_first_luma_reconstruct(
        &mode, &prefix, &transform, &block, &scratch, &reconstruction);
    if (status != AV2H_OK) {
        fprintf(stderr, "%s: reconstruction status=%d\n", name, status);
        ++failures;
        return;
    }
    CHECK(reconstruction.width == block.width);
    CHECK(reconstruction.height == block.height);
    CHECK(pixel_hash(&reconstruction) == expected_hash);
    for (index = 0U; index < expected_pixel_count; ++index) {
        CHECK(reconstruction.pixels[index] == expected_pixels[index]);
    }
}

static void test_fixture_reconstruction(void) {
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
    static const uint8_t small_pixels[] = {
        23U, 37U, 55U, 70U, 83U, 98U, 116U, 130U,
        33U, 47U, 65U, 80U, 93U, 107U, 126U, 140U,
        47U, 61U, 79U, 94U, 107U, 121U, 140U, 154U,
        57U, 71U, 89U, 104U, 117U, 131U, 150U, 163U
    };
    static const uint8_t detail_pixels[] = {
        19U, 59U, 245U, 8U, 140U, 159U, 97U, 129U,
        57U, 104U, 43U, 81U, 230U, 5U, 198U, 236U,
        255U, 62U, 206U, 255U, 210U, 253U, 138U, 189U,
        34U, 109U, 10U, 63U, 38U, 84U, 244U, 53U,
        168U, 246U, 218U, 35U, 200U, 22U, 255U, 81U,
        211U, 31U, 13U, 110U, 29U, 115U, 103U, 188U,
        155U, 246U, 180U, 26U, 8U, 105U, 38U, 141U,
        188U, 29U, 246U, 94U, 98U, 214U, 149U, 6U
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
                  UINT64_C(0x6adb5720dbef6cf3), small_pixels,
                  sizeof(small_pixels));
    check_fixture("odd", odd, sizeof(odd), 17U, 15U, odd_frame,
                  UINT64_C(0x6adb5720dbef6cf3), small_pixels,
                  sizeof(small_pixels));
    check_fixture("ordinary", ordinary, sizeof(ordinary), 160U, 120U,
                  ordinary_frame, UINT64_C(0xb666ec5be3701956), 0, 0U);
    check_fixture("high-detail", detail, sizeof(detail), 96U, 80U,
                  detail_frame, UINT64_C(0x62cd66e73a5893f4), detail_pixels,
                  sizeof(detail_pixels));
}

static void test_arguments(void) {
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    block.width = 8U;
    block.height = 8U;
    block.coefficient_count = 64U;
    CHECK(av2h_first_luma_reconstruct(
              0, &prefix, &transform, &block, &scratch, &reconstruction) ==
          AV2H_INVALID_ARGUMENT);
    mode.mode = AV2H_INTRA_SMOOTH;
    prefix.tx_size = AV2H_TX_8X8;
    transform.primary_type = AV2H_TX_V_DCT;
    CHECK(av2h_first_luma_reconstruct(
              &mode, &prefix, &transform, &block, &scratch,
              &reconstruction) ==
          AV2H_UNSUPPORTED);
}

static void test_d45_minus_one_16x4_prediction(void) {
    static const uint8_t above[20U] = {
        134U, 136U, 138U, 140U, 142U, 143U, 145U, 148U,
        150U, 152U, 153U, 155U, 157U, 159U, 161U, 163U,
        165U, 168U, 170U, 172U
    };
    static const uint8_t expected[64U] = {
        136U, 138U, 140U, 142U, 144U, 146U, 148U, 150U,
        152U, 153U, 155U, 157U, 159U, 161U, 163U, 165U,
        139U, 141U, 143U, 145U, 147U, 149U, 151U, 152U,
        153U, 156U, 158U, 160U, 162U, 164U, 166U, 169U,
        141U, 143U, 145U, 147U, 149U, 151U, 152U, 154U,
        156U, 158U, 160U, 162U, 164U, 166U, 169U, 171U,
        143U, 145U, 147U, 149U, 151U, 153U, 154U, 156U,
        158U, 160U, 162U, 164U, 167U, 169U, 171U, 171U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hLumaReferences references = { 0 };
    uint8_t predictor[80U];
    size_t row;
    size_t column;

    mode.mode = AV2H_INTRA_D45;
    mode.angle_delta = -1;
    references.above = above;
    references.above_count = sizeof(above);
    av2h_memory_fill(predictor, 0xa5U, sizeof(predictor));

    CHECK(av2h_luma_predict(
              &mode, 16U, 4U, &references, predictor, 20U) == AV2H_OK);
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 16U; ++column) {
            CHECK(predictor[row * 20U + column] ==
                  expected[row * 16U + column]);
        }
        for (column = 16U; column < 20U; ++column) {
            CHECK(predictor[row * 20U + column] == 0xa5U);
        }
    }
}

static void test_tx16x4_dct_reconstruction(void) {
    static const uint8_t predictor[64U] = {
        136U, 138U, 140U, 142U, 144U, 146U, 148U, 150U,
        152U, 153U, 155U, 157U, 159U, 161U, 163U, 165U,
        139U, 141U, 143U, 145U, 147U, 149U, 151U, 152U,
        153U, 156U, 158U, 160U, 162U, 164U, 166U, 169U,
        141U, 143U, 145U, 147U, 149U, 151U, 152U, 154U,
        156U, 158U, 160U, 162U, 164U, 166U, 169U, 171U,
        143U, 145U, 147U, 149U, 151U, 153U, 154U, 156U,
        158U, 160U, 162U, 164U, 167U, 169U, 171U, 171U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    size_t index;

    mode.mode = AV2H_INTRA_D45;
    mode.angle_delta = -1;
    prefix.tx_size = AV2H_TX_16X4;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 16U;
    block.height = 4U;
    block.coefficient_count = 64U;
    block.dequantized[0] = 5096;

    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 16U,
              &scratch, &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 16U);
    CHECK(reconstruction.height == 4U);
    for (index = 0U; index < 64U; ++index) {
        CHECK(reconstruction.residual[index] == 80);
        CHECK(reconstruction.pixels[index] == predictor[index] + 80U);
    }
}

static void test_tx16x4_fsc_reconstruction(void) {
    static const uint8_t predictor[64U] = {
        225U, 227U, 229U, 231U, 233U, 234U, 236U, 238U,
        240U, 242U, 244U, 247U, 249U, 250U, 251U, 251U,
        228U, 230U, 232U, 233U, 234U, 237U, 239U, 241U,
        243U, 245U, 248U, 249U, 250U, 251U, 251U, 251U,
        230U, 232U, 233U, 235U, 237U, 239U, 241U, 243U,
        245U, 248U, 249U, 250U, 251U, 251U, 251U, 251U,
        232U, 234U, 235U, 237U, 239U, 241U, 243U, 246U,
        248U, 250U, 251U, 251U, 251U, 251U, 251U, 251U
    };
    static const uint8_t expected[64U] = {
        225U, 227U, 229U, 231U, 233U, 234U, 236U, 238U,
        240U, 242U, 244U, 247U, 249U, 250U, 251U, 251U,
        228U, 230U, 232U, 233U, 234U, 237U, 239U, 241U,
        243U, 245U, 248U, 249U, 250U, 251U, 6U, 6U,
        230U, 232U, 233U, 235U, 237U, 239U, 241U, 243U,
        245U, 248U, 249U, 250U, 251U, 6U, 6U, 6U,
        232U, 234U, 235U, 237U, 239U, 241U, 243U, 246U,
        248U, 250U, 251U, 6U, 6U, 6U, 6U, 6U
    };
    static const uint8_t nonzero_positions[10U] = {
        59U, 60U, 45U, 30U, 61U, 46U, 31U, 62U, 47U, 63U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    size_t index;

    mode.mode = AV2H_INTRA_D45;
    mode.angle_delta = -1;
    mode.fsc_mode = 1U;
    prefix.tx_size = AV2H_TX_16X4;
    transform.primary_type = AV2H_TX_IDTX;
    block.width = 16U;
    block.height = 4U;
    block.coefficient_count = 64U;
    for (index = 0U; index < sizeof(nonzero_positions); ++index) {
        block.dequantized[nonzero_positions[index]] = -1960;
    }

    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 16U,
              &scratch, &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 16U);
    CHECK(reconstruction.height == 4U);
    for (index = 0U; index < 64U; ++index) {
        CHECK(reconstruction.residual[index] ==
              (block.dequantized[index] == 0 ? 0 : -245));
        CHECK(reconstruction.pixels[index] == expected[index]);
    }
}

static void test_tx4x4_intrabc_idtx_reconstruction(void) {
    static const uint8_t predictor[16U] = {
        247U, 249U, 251U, 251U,
        249U, 250U, 251U, 251U,
        250U, 251U, 6U, 6U,
        251U, 6U, 6U, 6U
    };
    static const uint8_t expected[16U] = {
        247U, 249U, 251U, 251U,
        249U, 250U, 251U, 251U,
        250U, 251U, 251U, 6U,
        251U, 6U, 6U, 6U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    size_t index;

    mode.mode = AV2H_INTRA_DC;
    mode.use_intrabc = 1U;
    prefix.tx_size = AV2H_TX_4X4;
    transform.primary_type = AV2H_TX_IDTX;
    block.width = 4U;
    block.height = 4U;
    block.coefficient_count = 16U;
    block.dequantized[10U] = 1960;

    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 4U,
              &scratch, &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 4U);
    CHECK(reconstruction.height == 4U);
    for (index = 0U; index < 16U; ++index) {
        CHECK(reconstruction.residual[index] ==
              (index == 10U ? 245 : 0));
        CHECK(reconstruction.pixels[index] == expected[index]);
    }
}

static void test_tx4x4_d203_fsc_reconstruction(void) {
    static const uint8_t left[4U] = { 251U, 251U, 6U, 6U };
    static const uint8_t predictor[16U] = {
        255U, 211U, 54U, 0U,
        102U, 0U, 0U, 6U,
        0U, 6U, 6U, 6U,
        6U, 6U, 6U, 6U
    };
    static const uint8_t expected[16U] = {
        255U, 255U, 5U, 0U,
        4U, 0U, 0U, 6U,
        0U, 6U, 6U, 6U,
        6U, 6U, 6U, 6U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hLumaReferences references = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    uint8_t predicted[16U];
    size_t index;

    mode.mode = AV2H_INTRA_D203;
    mode.angle_delta = 3;
    mode.fsc_mode = 1U;
    references.left = left;
    references.left_count = sizeof(left);
    CHECK(av2h_luma_predict(
              &mode, 4U, 4U, &references, predicted, 4U) == AV2H_OK);
    for (index = 0U; index < 16U; ++index) {
        CHECK(predicted[index] == predictor[index]);
    }
    prefix.tx_size = AV2H_TX_4X4;
    transform.primary_type = AV2H_TX_IDTX;
    block.width = 4U;
    block.height = 4U;
    block.coefficient_count = 16U;
    block.dequantized[1U] = 392;
    block.dequantized[2U] = -392;
    block.dequantized[4U] = -784;

    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 4U,
              &scratch, &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 4U);
    CHECK(reconstruction.height == 4U);
    for (index = 0U; index < 16U; ++index) {
        int32_t residual =
            index == 1U ? 49 : index == 2U ? -49 :
            index == 4U ? -98 : 0;
        CHECK(reconstruction.residual[index] == residual);
        CHECK(reconstruction.pixels[index] == expected[index]);
    }
}

static void test_tx8x4_d203_reconstruction(void) {
    static const uint8_t above[12U] = {
        251U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U
    };
    static const uint8_t predictor[32U] = {
        16U, 1U, 6U, 6U, 6U, 6U, 6U, 6U,
        6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U,
        6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U,
        6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U
    };
    static const uint8_t expected[32U] = {
        9U, 0U, 1U, 4U, 6U, 8U, 10U, 11U,
        2U, 3U, 5U, 7U, 9U, 11U, 13U, 14U,
        7U, 8U, 9U, 11U, 14U, 16U, 18U, 19U,
        10U, 11U, 13U, 15U, 17U, 19U, 21U, 22U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hLumaReferences references = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    uint8_t predicted[32U];
    size_t index;

    mode.mode = AV2H_INTRA_D203;
    mode.angle_delta = 3;
    references.above = above;
    references.above_count = sizeof(above);
    references.has_above_left = 1U;
    references.above_left = 251U;
    CHECK(av2h_luma_predict(
              &mode, 8U, 4U, &references, predicted, 8U) == AV2H_OK);
    for (index = 0U; index < 32U; ++index) {
        CHECK(predicted[index] == predictor[index]);
    }

    prefix.tx_size = AV2H_TX_8X4;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 8U;
    block.height = 4U;
    block.coefficient_count = 32U;
    block.dequantized[0U] = 196;
    block.dequantized[1U] = -196;
    block.dequantized[8U] = -196;
    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 8U,
              &scratch, &reconstruction) == AV2H_OK);
    for (index = 0U; index < 32U; ++index) {
        int32_t expected_residual =
            index == 1U ? -6 :
            (int32_t)expected[index] - predictor[index];
        CHECK(reconstruction.pixels[index] == expected[index]);
        CHECK(reconstruction.residual[index] == expected_residual);
    }
}

static void test_tx8x8_d203_mrl_reconstruction(void) {
    static const uint8_t left[8U] = {
        5U, 0U, 6U, 6U, 10U, 13U, 18U, 21U
    };
    static const uint8_t predictor[64U] = {
        1U, 5U, 6U, 7U, 9U, 11U, 14U, 17U,
        6U, 6U, 8U, 11U, 12U, 15U, 19U, 21U,
        7U, 10U, 12U, 14U, 17U, 20U, 21U, 21U,
        11U, 13U, 16U, 19U, 21U, 21U, 21U, 21U,
        14U, 18U, 20U, 21U, 21U, 21U, 21U, 21U,
        19U, 21U, 21U, 21U, 21U, 21U, 21U, 21U,
        21U, 21U, 21U, 21U, 21U, 21U, 21U, 21U,
        21U, 21U, 21U, 21U, 21U, 21U, 21U, 21U
    };
    static const uint8_t expected[64U] = {
        1U, 5U, 6U, 6U, 8U, 10U, 13U, 16U,
        6U, 6U, 7U, 10U, 11U, 14U, 18U, 20U,
        7U, 10U, 12U, 14U, 17U, 20U, 21U, 21U,
        11U, 14U, 17U, 21U, 23U, 23U, 24U, 24U,
        15U, 20U, 23U, 25U, 26U, 27U, 27U, 28U,
        20U, 25U, 27U, 28U, 30U, 31U, 32U, 32U,
        23U, 26U, 29U, 31U, 33U, 35U, 36U, 36U,
        23U, 27U, 30U, 32U, 35U, 37U, 38U, 39U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hLumaReferences references = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    uint8_t predicted[64U];
    size_t index;

    mode.mode = AV2H_INTRA_D203;
    mode.angle_delta = 3;
    mode.mrl_index = 1U;
    references.left = left;
    references.left_count = sizeof(left);
    references.has_above_left = 1U;
    references.above_left = 175U;
    CHECK(av2h_luma_predict(
              &mode, 8U, 8U, &references, predicted, 8U) == AV2H_OK);
    for (index = 0U; index < 64U; ++index) {
        CHECK(predicted[index] == predictor[index]);
    }

    prefix.tx_size = AV2H_TX_8X8;
    transform.primary_type = AV2H_TX_ADST_ADST;
    block.width = 8U;
    block.height = 8U;
    block.coefficient_count = 64U;
    block.dequantized[0U] = 392;
    block.dequantized[8U] = -196;
    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 8U,
              &scratch, &reconstruction) == AV2H_OK);
    for (index = 0U; index < 64U; ++index) {
        CHECK(reconstruction.pixels[index] == expected[index]);
        CHECK(reconstruction.residual[index] ==
              (int32_t)expected[index] - predictor[index]);
    }
}

static void test_live_plane_reconstruction(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U,
        0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U, 0x35U,
        0x9cU, 0x23U, 0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU, 0x83U,
        0x17U, 0x25U, 0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU, 0xdbU,
        0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU, 0xf5U
    };
    static const uint8_t expected_second[16] = {
        138U, 150U, 161U, 170U, 151U, 167U, 181U, 189U,
        167U, 184U, 196U, 203U, 173U, 189U, 201U, 208U
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
    Av2hPartitionContextState partition_state;
    Av2hLumaModeContextState mode_state;
    Av2hCoeffContextState coeff_state;
    Av2hPartitionTraversal traversal;
    uint8_t partition_above[16U];
    uint8_t partition_left[16U];
    uint8_t chroma_partition_above[16U];
    uint8_t chroma_partition_left[16U];
    Av2hLumaModeContext mode_above[16U];
    Av2hLumaModeContext mode_left[16U];
    uint8_t coeff_above[AV2H_FRAME_PLANE_COUNT * 16U];
    uint8_t coeff_left[AV2H_FRAME_PLANE_COUNT * 16U];
    uint8_t pixels[16U * 16U];
    uint8_t reconstructed[4U * 4U];
    Av2hReconstructionPlane plane = {
        pixels, 16U, 16U, 16U, 16U, 16U,
        reconstructed, 4U, sizeof(reconstructed)
    };
    Av2hReconstructionScratch scratch;
    Av2hFirstLeafPath path;
    Av2hFirstLeafMode mode;
    Av2hFirstCoeffPrefix prefix;
    Av2hFirstTransformType transform;
    Av2hFirstCoeffBlock block;
    uint8_t dc_sign_context;
    uint8_t has_leaf;
    size_t index;
    av2h_memory_fill(pixels, 0U, sizeof(pixels));
    av2h_memory_fill(reconstructed, 0U, sizeof(reconstructed));
    sequence.enable_sdp = 1U;
    sequence.enable_ext_partitions = 1U;
    sequence.enable_idtx_intra = 1U;
    sequence.enable_ist = 1U;
    sequence.enable_mrls = 1U;
    frame.width = 16U;
    frame.height = 16U;
    frame.base_q_index = 128U;
    frame.tx_mode = 1U;
    frame.cdef_strength_count = 1U;
    CHECK(av2h_partition_cdfs_init(&partition_cdfs) == AV2H_OK);
    CHECK(av2h_leaf_cdfs_init(&leaf_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_cdfs_init(&coeff_cdfs) == AV2H_OK);
    CHECK(av2h_first_transform_cdfs_init(&transform_cdfs) == AV2H_OK);
    CHECK(av2h_first_coeff_level_cdfs_init(&level_cdfs) == AV2H_OK);
    CHECK(av2h_partition_context_state_init(
              &partition_state, partition_above, partition_left,
              chroma_partition_above, chroma_partition_left, 16U, 16U) ==
          AV2H_OK);
    CHECK(av2h_luma_mode_context_state_init(
              &mode_state, mode_above, 16U, mode_left, 16U) == AV2H_OK);
    CHECK(av2h_coeff_context_state_init(
              &coeff_state, coeff_above, 16U, coeff_left, 16U) == AV2H_OK);
    CHECK(av2h_partition_traversal_init(
              &traversal, 4U, 4U,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 }, 0U, 1U) ==
          AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    for (index = 0U; index < 2U; ++index) {
        CHECK(av2h_partition_traversal_next(
                  &traversal, &decoder, &partition_cdfs, &partition_state,
                  &path, &has_leaf) == AV2H_OK);
        CHECK(has_leaf == 1U);
        CHECK(av2h_luma_mode_read_with_state(
                  &decoder, &leaf_cdfs, &sequence, &frame, &path.leaf,
                  &mode_state, &mode) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_update(
                  &mode_state, &path.leaf, &mode) == AV2H_OK);
        CHECK(av2h_coeff_prefix_read_with_state(
                  &decoder, &coeff_cdfs, &frame, &path.leaf, &mode,
                  &coeff_state, &prefix, &dc_sign_context) == AV2H_OK);
        CHECK(av2h_first_transform_type_read(
                  &decoder, &transform_cdfs, &sequence, &frame, &mode,
                  &prefix, &transform) == AV2H_OK);
        CHECK(av2h_coeff_block_read_with_state(
                  &decoder, &level_cdfs, &sequence, &frame, &path.leaf,
                  &mode, &prefix, &transform, &coeff_state, &block) ==
              AV2H_OK);
        CHECK(av2h_luma_reconstruct_to_plane(
                  &path.leaf, &mode, &prefix, &transform, &block,
                  &plane, &scratch) == AV2H_OK);
        CHECK(av2h_partition_traversal_commit_leaf(
                  &traversal, &partition_state, path.leaf) == AV2H_OK);
    }
    for (index = 0U; index < 16U; ++index) {
        CHECK(pixels[(index / 4U) * 16U + 8U + index % 4U] ==
              expected_second[index]);
    }
    CHECK(av2h_range_tell(&decoder) == 74U);
}

int main(void) {
    test_fixture_reconstruction();
    test_d45_minus_one_16x4_prediction();
    test_tx16x4_dct_reconstruction();
    test_tx16x4_fsc_reconstruction();
    test_tx4x4_intrabc_idtx_reconstruction();
    test_tx4x4_d203_fsc_reconstruction();
    test_tx8x4_d203_reconstruction();
    test_tx8x8_d203_mrl_reconstruction();
    test_live_plane_reconstruction();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d reconstruction checks failed\n", failures);
        return 1;
    }
    puts("reconstruction unit: pass");
    return 0;
}
