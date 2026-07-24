#include "decoder/tile.h"

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

static uint32_t trace_sample_hash(const uint8_t *samples, size_t count) {
    uint32_t hash = 5381U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash = (hash * 33U) ^ samples[index];
    }
    return hash;
}

static uint64_t sample_hash(const uint8_t *samples, size_t count) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= samples[index];
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

static void test_skipped_16x8_intrabc_reconstruction(void) {
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    uint8_t predictor[128U];
    size_t index;
    mode.mode = AV2H_INTRA_DC;
    mode.use_intrabc = 1U;
    mode.skip_transform = 1U;
    mode.intrabc_drl_index = 3U;
    mode.mv_precision = 3U;
    mode.displacement_x = 96;
    mode.displacement_y = -64;
    prefix.tx_size = AV2H_TX_16X8;
    prefix.all_zero = 1U;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 16U;
    block.height = 8U;
    block.coefficient_count = 128U;
    for (index = 0U; index < sizeof(predictor); ++index) {
        predictor[index] = (uint8_t)(index * 17U + 3U);
    }
    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 16U,
              &scratch, &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 16U);
    CHECK(reconstruction.height == 8U);
    for (index = 0U; index < sizeof(predictor); ++index) {
        CHECK(reconstruction.residual[index] == 0);
        CHECK(reconstruction.pixels[index] == predictor[index]);
    }
    block.nonzero_count = 1U;
    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 16U,
              &scratch, &reconstruction) == AV2H_UNSUPPORTED);
    block.nonzero_count = 0U;
    block.quantized[17U] = 1;
    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, predictor, 16U,
              &scratch, &reconstruction) == AV2H_UNSUPPORTED);
}

static void test_all_zero_16x16_reconstruction(void) {
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references = { 0 };
    uint8_t above[16U];
    uint8_t predictor[256U];
    size_t index;

    av2h_memory_fill(above, 98U, sizeof(above));
    mode.mode = AV2H_INTRA_DC;
    mode.enable_ibp = 1U;
    references.above = above;
    references.above_count = sizeof(above);
    CHECK(av2h_luma_predict(
                  &mode, 16U, 16U, &references, predictor, 16U) == AV2H_OK);
    for (index = 0U; index < sizeof(predictor); ++index) {
        CHECK(predictor[index] == 98U);
    }

    {
        static const uint8_t expected[256U] = {
            98U, 98U, 98U, 98U, 98U, 98U, 98U, 98U,
            98U, 98U, 98U, 98U, 98U, 98U, 98U, 98U,
            98U, 99U, 99U, 99U, 99U, 100U, 100U, 100U,
            100U, 100U, 101U, 101U, 101U, 101U, 101U, 101U,
            99U, 100U, 101U, 102U, 102U, 103U, 104U, 105U,
            105U, 106U, 106U, 107U, 107U, 108U, 108U, 108U,
            100U, 102U, 104U, 106U, 108U, 109U, 111U, 113U,
            114U, 115U, 116U, 117U, 118U, 119U, 119U, 119U,
            101U, 105U, 108U, 111U, 115U, 118U, 120U, 123U,
            125U, 128U, 129U, 131U, 132U, 134U, 134U, 134U,
            103U, 108U, 113U, 118U, 123U, 127U, 131U, 135U,
            138U, 142U, 144U, 147U, 149U, 150U, 151U, 152U,
            104U, 111U, 118U, 124U, 130U, 136U, 141U, 147U,
            151U, 155U, 159U, 162U, 164U, 166U, 167U, 168U,
            105U, 114U, 121U, 129U, 136U, 143U, 149U, 156U,
            160U, 166U, 170U, 173U, 176U, 179U, 180U, 181U,
            106U, 115U, 123U, 131U, 139U, 146U, 153U, 160U,
            165U, 171U, 175U, 179U, 182U, 185U, 186U, 187U,
            106U, 115U, 123U, 130U, 138U, 145U, 152U, 159U,
            164U, 170U, 174U, 178U, 181U, 184U, 185U, 186U,
            105U, 113U, 120U, 127U, 134U, 140U, 146U, 152U,
            157U, 162U, 165U, 169U, 171U, 174U, 175U, 176U,
            104U, 110U, 116U, 121U, 127U, 132U, 137U, 141U,
            145U, 149U, 152U, 155U, 157U, 159U, 160U, 160U,
            102U, 106U, 110U, 114U, 118U, 121U, 125U, 128U,
            130U, 133U, 135U, 137U, 139U, 140U, 141U, 141U,
            100U, 103U, 105U, 107U, 109U, 111U, 113U, 115U,
            117U, 118U, 120U, 121U, 122U, 122U, 123U, 123U,
            99U, 100U, 101U, 101U, 102U, 103U, 104U, 105U,
            105U, 106U, 106U, 107U, 107U, 107U, 107U, 107U,
            98U, 98U, 98U, 98U, 98U, 99U, 99U, 99U,
            99U, 99U, 99U, 99U, 99U, 99U, 99U, 99U
        };
        Av2hFirstLeafMode mode = { 0 };
        Av2hFirstCoeffPrefix prefix = { 0 };
        Av2hFirstTransformType transform = { 0 };
        Av2hFirstCoeffBlock block = { 0 };
        Av2hReconstructionScratch scratch;
        Av2hFirstReconstruction reconstruction;
        uint8_t predictor[256U];
        size_t index;

        av2h_memory_fill(predictor, 98U, sizeof(predictor));
        mode.mode = AV2H_INTRA_SMOOTH;
        prefix.tx_size = AV2H_TX_16X16;
        prefix.eob = 4U;
        transform.primary_type = AV2H_TX_ADST_ADST;
        block.width = 16U;
        block.height = 16U;
        block.coefficient_count = 256U;
        block.nonzero_count = 3U;
        block.dequantized[0U] = 3968;
        block.dequantized[16U] = 1984;
        block.dequantized[32U] = -1984;

        CHECK(av2h_luma_reconstruct(
                  &mode, &prefix, &transform, &block, predictor, 16U,
                  &scratch, &reconstruction) == AV2H_OK);
        CHECK(reconstruction.width == 16U);
        CHECK(reconstruction.height == 16U);
        CHECK(trace_sample_hash(reconstruction.pixels, 256U) == 0x7959e335U);
        for (index = 0U; index < 256U; ++index) {
            CHECK(reconstruction.pixels[index] == expected[index]);
            CHECK(reconstruction.residual[index] ==
                  (int32_t)expected[index] - 98);
        }
    }

    prefix.tx_size = AV2H_TX_16X16;
    prefix.all_zero = 1U;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 16U;
    block.height = 16U;
    block.coefficient_count = 256U;
    CHECK(av2h_luma_reconstruct(
                  &mode, &prefix, &transform, &block, predictor, 16U,
                  &scratch, &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 16U);
    CHECK(reconstruction.height == 16U);
    for (index = 0U; index < 256U; ++index) {
        CHECK(reconstruction.residual[index] == 0);
        CHECK(reconstruction.pixels[index] == 98U);
    }

    block.quantized[255U] = 1;
    CHECK(av2h_luma_reconstruct(
                  &mode, &prefix, &transform, &block, predictor, 16U,
                  &scratch, &reconstruction) == AV2H_UNSUPPORTED);
}

static void test_laarim_q96_32x32_reconstruction(void) {
    typedef struct {
        uint16_t position;
        int32_t quantized;
        int32_t dequantized;
    } Coefficient;
    static const Coefficient coefficients[] = {
        { 0U, 67, 2613 }, { 1U, 6, 234 }, { 2U, -15, -585 },
        { 3U, 3, 117 }, { 5U, 5, 195 }, { 6U, -2, -78 },
        { 7U, 1, 39 }, { 8U, 2, 78 }, { 9U, -2, -78 },
        { 32U, -24, -936 }, { 33U, 5, 195 }, { 34U, -3, -117 },
        { 35U, -7, -273 }, { 36U, 5, 195 }, { 37U, 3, 117 },
        { 38U, -3, -117 }, { 40U, -4, -156 }, { 41U, -2, -78 },
        { 43U, -1, -39 }, { 64U, -40, -1560 }, { 65U, 18, 702 },
        { 66U, -8, -312 }, { 67U, -6, -234 }, { 68U, -1, -39 },
        { 69U, 2, 78 }, { 73U, -2, -78 }, { 96U, 3, 117 },
        { 97U, 4, 156 }, { 98U, -8, -312 }, { 99U, -1, -39 },
        { 100U, 2, 78 }, { 101U, 1, 39 }, { 102U, 4, 156 },
        { 103U, 2, 78 }, { 104U, -1, -39 }, { 105U, -1, -39 },
        { 106U, -1, -39 }, { 128U, 11, 429 }, { 129U, 18, 702 },
        { 130U, 4, 156 }, { 131U, -6, -234 }, { 132U, -7, -273 },
        { 133U, -1, -39 }, { 134U, -1, -39 }, { 135U, 1, 39 },
        { 136U, -1, -39 }, { 160U, 5, 195 }, { 161U, -9, -351 },
        { 162U, 2, 78 }, { 163U, 11, 429 }, { 164U, -4, -156 },
        { 165U, -2, -78 }, { 166U, -1, -39 }, { 192U, 4, 156 },
        { 193U, 3, 117 }, { 194U, 9, 351 }, { 195U, -2, -78 },
        { 196U, 1, 39 }, { 198U, -2, -78 }, { 224U, 1, 39 },
        { 226U, -2, -78 }, { 227U, 4, 156 }, { 228U, 3, 117 },
        { 229U, -2, -78 }, { 230U, -4, -156 }, { 256U, -1, -39 },
        { 258U, 1, 39 }, { 259U, -1, -39 }, { 260U, 3, 117 },
        { 261U, 1, 39 }, { 262U, -2, -78 }, { 288U, 2, 78 },
        { 289U, 4, 156 }, { 290U, -1, -39 }, { 291U, -2, -78 },
        { 292U, 1, 39 }, { 323U, -1, -39 }, { 324U, -1, -39 },
        { 325U, 1, 39 }, { 353U, 2, 78 }, { 355U, -1, -39 },
        { 356U, -1, -39 }
    };
    static const uint8_t expected_first[32U] = {
        138U, 144U, 152U, 156U, 156U, 152U, 147U, 147U,
        151U, 155U, 158U, 158U, 154U, 151U, 148U, 146U,
        144U, 139U, 131U, 121U, 110U, 103U, 100U, 103U,
        109U, 113U, 113U, 109U, 102U, 95U, 89U, 87U
    };
    static const uint8_t expected_last[32U] = {
        159U, 158U, 157U, 158U, 162U, 167U, 172U, 174U,
        173U, 171U, 166U, 161U, 156U, 151U, 145U, 139U,
        133U, 126U, 119U, 114U, 114U, 117U, 124U, 130U,
        134U, 134U, 132U, 129U, 127U, 126U, 126U, 127U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hLumaReferences references = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    size_t index;

    mode.mode = AV2H_INTRA_D45;
    mode.enable_ibp = 1U;
    prefix.tx_size = AV2H_TX_32X32;
    prefix.eob = 126U;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 32U;
    block.height = 32U;
    block.coefficient_count = 1024U;
    block.nonzero_count =
        sizeof(coefficients) / sizeof(coefficients[0]);
    for (index = 0U;
         index < sizeof(coefficients) / sizeof(coefficients[0]); ++index) {
        block.quantized[coefficients[index].position] =
            coefficients[index].quantized;
        block.dequantized[coefficients[index].position] =
            coefficients[index].dequantized;
    }

    CHECK(av2h_luma_predict(
              &mode, 32U, 32U, &references, scratch.predictor, 32U) ==
          AV2H_OK);
    CHECK(sample_hash(scratch.predictor, 1024U) ==
          UINT64_C(0x75290844cee19be7));
    CHECK(av2h_luma_reconstruct(
              &mode, &prefix, &transform, &block, scratch.predictor, 32U,
              &scratch, &reconstruction) == AV2H_OK);
    CHECK(sample_hash(reconstruction.pixels, 1024U) ==
          UINT64_C(0x51b573f06c63948a));
    for (index = 0U; index < 32U; ++index) {
        CHECK(reconstruction.pixels[index] == expected_first[index]);
        CHECK(reconstruction.pixels[992U + index] == expected_last[index]);
    }
}

static void test_schwaebisch_q208_64x16_reconstruction(void) {
    enum {
        MI_COLUMNS = 105,
        MI_ROWS = 70,
        STREAM_CAPACITY = 1024,
        GDF_CAPACITY = 128,
        CDEF_CAPACITY = 128
    };
    static unsigned char stream[STREAM_CAPACITY];
    static uint8_t luma_partition_above[MI_COLUMNS];
    static uint8_t luma_partition_left[MI_ROWS];
    static uint8_t chroma_partition_above[MI_COLUMNS];
    static uint8_t chroma_partition_left[MI_ROWS];
    static Av2hLumaModeContext luma_mode_above[MI_COLUMNS];
    static Av2hLumaModeContext luma_mode_left[MI_ROWS];
    static uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * MI_COLUMNS];
    static uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * MI_ROWS];
    static Av2hTileLumaModeCell colocated_luma[MI_COLUMNS * MI_ROWS];
    static uint8_t gdf_block_flags[GDF_CAPACITY];
    static uint8_t cdef_unit_strengths[CDEF_CAPACITY];
    static uint8_t
        ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][CDEF_CAPACITY];
    static uint8_t pixels[417U * 277U];
    static uint8_t reconstructed[MI_COLUMNS * MI_ROWS];
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        MI_COLUMNS, MI_ROWS, MI_COLUMNS * MI_ROWS,
        gdf_block_flags, GDF_CAPACITY,
        cdef_unit_strengths, CDEF_CAPACITY,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
        CDEF_CAPACITY
    };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hFirstCoeffBlock block;
    Av2hReconstructionPlane plane = {
        pixels, 417U, 417U, 277U, 417U, 277U,
        reconstructed, MI_COLUMNS, sizeof(reconstructed)
    };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    Av2hStatus status;
    FILE *source;
    long file_size;
    size_t stream_size;
    size_t index;
    uint8_t has_leaf = 0U;

    source = fopen(
        "images/artifacts/schwaebisch-417x277-qp208.obu", "rb");
    if (source == 0) {
        CHECK(source != 0);
        return;
    }
    CHECK(fseek(source, 0L, SEEK_END) == 0);
    file_size = ftell(source);
    CHECK(file_size > 0L);
    CHECK((unsigned long)file_size <= STREAM_CAPACITY);
    CHECK(fseek(source, 0L, SEEK_SET) == 0);
    if (file_size <= 0L || (unsigned long)file_size > STREAM_CAPACITY) {
        fclose(source);
        return;
    }
    stream_size = fread(stream, 1U, (size_t)file_size, source);
    CHECK(stream_size == (size_t)file_size);
    CHECK(fclose(source) == 0);
    if (stream_size != (size_t)file_size) return;

    CHECK(av2h_obu_parse(
              (Av2hSpan){ stream, stream_size }, &ledger, &error) ==
          AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence_obu = &entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            frame_obu = &entries[index];
        }
    }
    CHECK(sequence_obu != 0);
    CHECK(frame_obu != 0);
    if (sequence_obu == 0 || frame_obu == 0) return;
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ stream, stream_size }, sequence_obu,
              &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ stream + frame_obu->payload_offset,
                           frame_obu->payload_size },
              &sequence, &frame) == AV2H_OK);
    CHECK(frame.width == 417U);
    CHECK(frame.height == 277U);
    CHECK(frame.base_q_index == 208U);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ stream + frame_obu->payload_offset,
                           frame_obu->payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(av2h_tile_decoder_init(
              &decoder, group.entropy, &sequence, &frame, &storage) ==
          AV2H_OK);

    av2h_memory_fill(pixels, 0U, sizeof(pixels));
    av2h_memory_fill(reconstructed, 0U, sizeof(reconstructed));
    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(&block, 0U, sizeof(block));
    status = av2h_tile_decoder_next_luma(
        &decoder, &leaf, &block, &has_leaf);
    CHECK(status == AV2H_OK);
    CHECK(has_leaf == 1U);
    CHECK(leaf.node.row == 0U);
    CHECK(leaf.node.column == 0U);
    CHECK(leaf.node.block_size == AV2H_BLOCK_64X16);
    CHECK(leaf.mode.before_mode_bits == 8U);
    CHECK(leaf.mode.after_mode_set_bits == 8U);
    CHECK(leaf.mode.after_mode_index_bits == 9U);
    CHECK(leaf.mode.after_fsc_bits == 9U);
    CHECK(leaf.mode.after_mrl_bits == 9U);
    CHECK(leaf.mode.mode == AV2H_INTRA_DC);
    CHECK(leaf.mode.angle_delta == 0);
    CHECK(leaf.mode.mrl_index == 0U);
    CHECK(leaf.mode.enable_ibp == 1U);
    CHECK(leaf.mode.use_intrabc == 0U);
    CHECK(leaf.mode.skip_transform == 0U);
    CHECK(leaf.mode.fsc_mode == 0U);
    CHECK(leaf.prefix.tx_size == AV2H_TX_64X16);
    CHECK(leaf.prefix.tx_size_context == 3U);
    CHECK(leaf.prefix.txb_skip_context == 0U);
    CHECK(leaf.prefix.before_all_zero_bits == 9U);
    CHECK(leaf.prefix.after_all_zero_bits == 17U);
    CHECK(leaf.prefix.after_eob_point_bits == 17U);
    CHECK(leaf.prefix.after_eob_extra_bit == 17U);
    CHECK(leaf.prefix.after_eob_bits == 17U);
    CHECK(leaf.prefix.all_zero == 1U);
    CHECK(leaf.prefix.eob == 0U);
    CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
    CHECK(leaf.transform.before_primary_bits == 17U);
    CHECK(leaf.transform.after_primary_bits == 17U);
    CHECK(leaf.transform.after_secondary_bits == 17U);
    CHECK(block.width == 32U);
    CHECK(block.height == 16U);
    CHECK(block.coefficient_count == 512U);
    CHECK(block.nonzero_count == 0U);
    CHECK(block.before_levels_bits == 17U);
    CHECK(block.after_levels_bits == 17U);
    CHECK(block.after_coefficients_bits == 17U);
    CHECK(av2h_range_tell(&decoder.range) == 17U);
    for (index = 0U; index < 512U; ++index) {
        CHECK(block.quantized[index] == 0);
        CHECK(block.dequantized[index] == 0);
    }

    CHECK(av2h_luma_reconstruct_to_plane(
              &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
              &block, &plane, &scratch) == AV2H_OK);
    CHECK(av2h_range_tell(&decoder.range) == 17U);
    for (index = 0U; index < 1024U; ++index) {
        size_t row = index >> 6U;
        size_t column = index & 63U;
        CHECK(scratch.predictor[index] == 128U);
        CHECK(pixels[row * 417U + column] == 128U);
    }
    CHECK(pixels[64U] == 0U);
    for (index = 0U; index < 4U * 16U; ++index) {
        size_t row = index >> 4U;
        size_t column = index & 15U;
        CHECK(reconstructed[row * MI_COLUMNS + column] == 1U);
    }
    CHECK(reconstructed[16U] == 0U);

    block.quantized[511U] = 1;
    CHECK(av2h_luma_reconstruct(
              &leaf.mode, &leaf.prefix, &leaf.transform, &block,
              scratch.predictor, 64U, &scratch, &reconstruction) ==
          AV2H_UNSUPPORTED);
    block.quantized[511U] = 0;
    CHECK(av2h_tile_decoder_commit_leaf(&decoder, leaf.node) == AV2H_OK);
    CHECK(av2h_range_tell(&decoder.range) == 17U);

    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(&block, 0U, sizeof(block));
    status = av2h_tile_decoder_next_luma(
        &decoder, &leaf, &block, &has_leaf);
    CHECK(status == AV2H_UNSUPPORTED);
    CHECK(has_leaf == 1U);
    CHECK(leaf.node.row == 4U);
    CHECK(leaf.node.column == 0U);
    CHECK(leaf.node.block_size == AV2H_BLOCK_32X16);
    CHECK(leaf.mode.before_mode_bits == 0U);
    CHECK(leaf.prefix.before_all_zero_bits == 0U);
    CHECK(av2h_range_tell(&decoder.range) == 19U);
    CHECK(av2h_tile_decoder_next_luma(
              &decoder, &leaf, &block, &has_leaf) == AV2H_UNSUPPORTED);
    CHECK(av2h_range_tell(&decoder.range) == 19U);
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

static void test_d45_8x4_ibp_prediction(void) {
    static const uint8_t above[12U] = {
        223U, 226U, 228U, 230U, 232U, 233U,
        235U, 237U, 239U, 241U, 244U, 246U
    };
    static const uint8_t left[8U] = {
        230U, 230U, 231U, 233U, 237U, 240U, 243U, 245U
    };
    static const uint8_t expected[32U] = {
        228U, 229U, 231U, 233U, 234U, 236U, 238U, 240U,
        230U, 232U, 234U, 235U, 237U, 239U, 240U, 242U,
        233U, 235U, 237U, 238U, 240U, 241U, 242U, 244U,
        236U, 238U, 240U, 241U, 242U, 243U, 244U, 246U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hLumaReferences references = { 0 };
    uint8_t predictor[40U];
    size_t row;
    size_t column;

    mode.mode = AV2H_INTRA_D45;
    references.above = above;
    references.above_count = sizeof(above);
    references.left = left;
    references.left_count = sizeof(left);
    references.has_above_left = 1U;
    references.above_left = 221U;
    av2h_memory_fill(predictor, 0xa5U, sizeof(predictor));

    CHECK(av2h_luma_predict(
              &mode, 8U, 4U, &references, predictor, 10U) == AV2H_OK);
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            CHECK(predictor[row * 10U + column] ==
                  expected[row * 8U + column]);
        }
        CHECK(predictor[row * 10U + 8U] == 0xa5U);
        CHECK(predictor[row * 10U + 9U] == 0xa5U);
    }

    references.left = 0;
    references.left_count = 0U;
    CHECK(av2h_luma_predict(
              &mode, 8U, 4U, &references, predictor, 10U) == AV2H_OK);
    references.above = 0;
    references.above_count = 0U;
    CHECK(av2h_luma_predict(
              &mode, 8U, 4U, &references, predictor, 10U) == AV2H_OK);
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
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 }, 0U, 1U, 0U) ==
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

static void test_tx16x32_dc_dct_reconstruction(void) {
    static const uint8_t expected_first_row[16U] = {
        117U, 117U, 116U, 116U, 115U, 114U, 113U, 112U,
        111U, 110U, 109U, 108U, 107U, 107U, 106U, 106U
    };
    static const uint8_t expected_last_row[16U] = {
        114U, 114U, 114U, 115U, 116U, 117U, 118U, 119U,
        120U, 121U, 122U, 123U, 123U, 124U, 124U, 125U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    size_t index;

    mode.mode = AV2H_INTRA_DC;
    mode.enable_ibp = 1U;
    prefix.tx_size = AV2H_TX_16X32;
    prefix.eob = 5U;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 16U;
    block.height = 32U;
    block.coefficient_count = 512U;
    block.nonzero_count = 4U;
    block.dequantized[0] = -1488;
    block.dequantized[16] = -248;
    block.dequantized[17] = 248;
    block.dequantized[32] = 248;

    CHECK(av2h_first_luma_reconstruct(
              &mode, &prefix, &transform, &block, &scratch,
              &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 16U);
    CHECK(reconstruction.height == 32U);
    CHECK(pixel_hash(&reconstruction) == UINT64_C(0xfb97d9776de897f5));
    for (index = 0U; index < 16U; ++index) {
        CHECK(reconstruction.pixels[index] == expected_first_row[index]);
    }
    for (index = 0U; index < 16U; ++index) {
        CHECK(reconstruction.pixels[496U + index] == expected_last_row[index]);
    }

    block.dequantized[96U] = 248;
    CHECK(av2h_first_luma_reconstruct(
              &mode, &prefix, &transform, &block, &scratch,
              &reconstruction) == AV2H_UNSUPPORTED);
}

static void test_tx16x32_dc_flipadst_reconstruction(void) {
    static const uint8_t expected_first_row[16U] = {
        21U, 29U, 43U, 62U, 82U, 99U, 112U, 122U,
        126U, 130U, 131U, 131U, 132U, 131U, 130U, 130U
    };
    static const uint8_t expected_last_row[16U] = {
        95U, 97U, 101U, 105U, 107U, 108U, 107U, 105U,
        103U, 102U, 103U, 106U, 111U, 116U, 120U, 125U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hReconstructionScratch scratch;
    Av2hFirstReconstruction reconstruction;
    size_t index;

    mode.mode = AV2H_INTRA_DC;
    mode.enable_ibp = 1U;
    prefix.tx_size = AV2H_TX_16X32;
    prefix.eob = 13U;
    transform.primary_type = AV2H_TX_DCT_FLIPADST;
    block.width = 16U;
    block.height = 32U;
    block.coefficient_count = 512U;
    block.nonzero_count = 10U;
    block.dequantized[0] = -1984;
    block.dequantized[1] = -744;
    block.dequantized[2] = -248;
    block.dequantized[3] = -248;
    block.dequantized[16] = -496;
    block.dequantized[17] = -992;
    block.dequantized[18] = -248;
    block.dequantized[32] = -496;
    block.dequantized[33] = -248;
    block.dequantized[34] = -248;

    CHECK(av2h_first_luma_reconstruct(
              &mode, &prefix, &transform, &block, &scratch,
              &reconstruction) == AV2H_OK);
    CHECK(reconstruction.width == 16U);
    CHECK(reconstruction.height == 32U);
    CHECK(pixel_hash(&reconstruction) == UINT64_C(0x73f9aa733e29f1c7));
    for (index = 0U; index < 16U; ++index) {
        CHECK(reconstruction.pixels[index] == expected_first_row[index]);
    }
    for (index = 0U; index < 16U; ++index) {
        CHECK(reconstruction.pixels[496U + index] == expected_last_row[index]);
    }

    block.dequantized[4U] = -248;
    CHECK(av2h_first_luma_reconstruct(
              &mode, &prefix, &transform, &block, &scratch,
              &reconstruction) == AV2H_UNSUPPORTED);
}

static void test_dc_16x32_prediction(void) {
    static const uint8_t above[16U] = {
        100U, 102U, 104U, 106U, 108U, 110U, 112U, 114U,
        116U, 118U, 120U, 122U, 124U, 126U, 128U, 130U
    };
    static const uint8_t left[32U] = {
        80U, 82U, 84U, 86U, 88U, 90U, 92U, 94U,
        96U, 98U, 100U, 102U, 104U, 106U, 108U, 110U,
        112U, 114U, 116U, 118U, 120U, 122U, 124U, 126U,
        128U, 130U, 132U, 134U, 136U, 138U, 140U, 142U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hLumaReferences references = { 0 };
    uint8_t predictor[16U * 32U];
    unsigned int sum;
    uint8_t expected_dc;
    size_t index;

    mode.mode = AV2H_INTRA_DC;
    references.above = above;
    references.above_count = sizeof(above);
    references.left = left;
    references.left_count = sizeof(left);

    sum = 0U;
    for (index = 0U; index < 16U; ++index) sum += above[index];
    for (index = 0U; index < 32U; ++index) sum += left[index];
    expected_dc = (uint8_t)((sum + 24U) / 48U);

    CHECK(av2h_luma_predict(
              &mode, 16U, 32U, &references, predictor, 16U) == AV2H_OK);
    for (index = 0U; index < 512U; ++index) {
        CHECK(predictor[index] == expected_dc);
    }

    references.above = 0;
    references.above_count = 0U;
    references.left = 0;
    references.left_count = 0U;
    CHECK(av2h_luma_predict(
              &mode, 16U, 32U, &references, predictor, 16U) == AV2H_OK);
    for (index = 0U; index < 512U; ++index) {
        CHECK(predictor[index] == 128U);
    }
}

static void test_dc_16x32_ibp_prediction(void) {
    static const uint8_t above[16U] = {
        200U, 200U, 200U, 200U, 200U, 200U, 200U, 200U,
        200U, 200U, 200U, 200U, 200U, 200U, 200U, 200U
    };
    static const uint8_t left[32U] = {
        50U, 50U, 50U, 50U, 50U, 50U, 50U, 50U,
        50U, 50U, 50U, 50U, 50U, 50U, 50U, 50U,
        50U, 50U, 50U, 50U, 50U, 50U, 50U, 50U,
        50U, 50U, 50U, 50U, 50U, 50U, 50U, 50U
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hLumaReferences references = { 0 };
    uint8_t predictor[16U * 32U];
    uint8_t dc;

    mode.mode = AV2H_INTRA_DC;
    mode.enable_ibp = 1U;
    references.above = above;
    references.above_count = sizeof(above);
    references.left = left;
    references.left_count = sizeof(left);

    dc = (uint8_t)((200U * 16U + 50U * 32U + 24U) / 48U);

    CHECK(av2h_luma_predict(
              &mode, 16U, 32U, &references, predictor, 16U) == AV2H_OK);
    CHECK(predictor[0] != dc);
    CHECK(predictor[255] != predictor[0]);
}

int main(void) {
    test_fixture_reconstruction();
    test_d45_minus_one_16x4_prediction();
    test_d45_8x4_ibp_prediction();
    test_tx16x4_dct_reconstruction();
    test_tx16x4_fsc_reconstruction();
    test_tx4x4_intrabc_idtx_reconstruction();
    test_tx4x4_d203_fsc_reconstruction();
    test_tx8x4_d203_reconstruction();
    test_tx8x8_d203_mrl_reconstruction();
    test_skipped_16x8_intrabc_reconstruction();
    test_all_zero_16x16_reconstruction();
    test_laarim_q96_32x32_reconstruction();
    test_schwaebisch_q208_64x16_reconstruction();
    test_tx16x32_dc_dct_reconstruction();
    test_tx16x32_dc_flipadst_reconstruction();
    test_dc_16x32_prediction();
    test_dc_16x32_ibp_prediction();
    test_live_plane_reconstruction();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d reconstruction checks failed\n", failures);
        return 1;
    }
    puts("reconstruction unit: pass");
    return 0;
}
