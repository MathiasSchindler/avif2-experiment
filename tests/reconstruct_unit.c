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

/*
 * Sample-exact reconstruction tests for the 15 remaining Emerald Q208
 * luma blocks.  Inputs (dequantized coefficients and intra predictor)
 * and the reference djb2 hash come from the AVM trace arrays under
 * emerald-q208-luma-trace.  Coefficient geometry (adjusted, capped at 32)
 * is provided separately from the full output geometry (up to 64x64).
 */
static const uint8_t predictor_block_05_0_14[256] = {
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98
};
static const uint16_t coeff_pos_block_05_0_14[2] = {
    0, 32
};
static const int32_t coeff_val_block_05_0_14[2] = {
    7936, 1984
};
static const uint8_t recon_first_block_05_0_14[8] = {
    117, 117, 117, 117, 117, 117, 117, 117
};
static const uint8_t recon_last_block_05_0_14[8] = {
    163, 163, 163, 163, 163, 163, 163, 163
};
static const uint8_t predictor_block_06_8_8[1024] = {
    94, 93, 92, 92, 91, 90, 90, 89, 89, 89, 89, 88, 88, 88, 88, 88,
    87, 87, 87, 87, 86, 86, 86, 86, 86, 85, 85, 85, 85, 85, 84, 84,
    95, 95, 94, 93, 92, 91, 91, 91, 90, 90, 89, 89, 89, 89, 88, 88,
    88, 88, 88, 87, 87, 87, 87, 87, 86, 86, 86, 86, 85, 85, 85, 85,
    97, 96, 95, 95, 94, 93, 93, 92, 92, 91, 91, 91, 91, 90, 90, 90,
    90, 90, 89, 89, 89, 89, 88, 88, 88, 88, 87, 87, 87, 87, 86, 86,
    98, 97, 96, 96, 95, 94, 94, 93, 93, 92, 92, 92, 92, 91, 91, 91,
    91, 91, 90, 90, 90, 90, 89, 89, 89, 89, 88, 88, 88, 88, 87, 87,
    99, 99, 98, 98, 97, 96, 96, 95, 94, 94, 94, 93, 93, 93, 93, 93,
    92, 92, 92, 92, 91, 91, 91, 91, 90, 90, 90, 90, 89, 89, 89, 89,
    100, 100, 99, 98, 97, 97, 97, 96, 95, 95, 95, 94, 94, 94, 94, 94,
    93, 93, 93, 93, 92, 92, 92, 92, 91, 91, 91, 91, 90, 90, 90, 90,
    102, 101, 100, 100, 99, 98, 98, 97, 97, 97, 97, 96, 96, 96, 96, 96,
    95, 95, 95, 95, 94, 94, 94, 94, 93, 93, 93, 93, 92, 92, 92, 92,
    103, 103, 102, 101, 100, 100, 100, 99, 98, 98, 98, 97, 97, 97, 97, 97,
    96, 96, 96, 96, 95, 95, 95, 95, 94, 94, 94, 94, 93, 93, 93, 93,
    104, 104, 103, 102, 102, 101, 101, 100, 100, 99, 99, 99, 99, 98, 98, 98,
    98, 98, 97, 97, 97, 97, 96, 96, 96, 96, 95, 95, 95, 95, 94, 94,
    107, 106, 105, 105, 104, 103, 103, 103, 102, 101, 101, 101, 101, 100, 100, 100,
    99, 99, 99, 99, 98, 98, 98, 98, 97, 97, 97, 96, 96, 96, 96, 95,
    112, 111, 111, 110, 109, 108, 108, 107, 106, 106, 105, 105, 105, 104, 104, 104,
    103, 103, 102, 102, 102, 101, 101, 100, 100, 100, 99, 99, 98, 98, 98, 97,
    118, 118, 117, 116, 115, 114, 113, 113, 111, 111, 110, 110, 109, 108, 108, 107,
    107, 106, 106, 105, 104, 104, 103, 103, 102, 102, 101, 101, 100, 99, 99, 98,
    127, 126, 124, 124, 122, 121, 121, 119, 118, 118, 116, 116, 115, 114, 113, 113,
    112, 111, 110, 109, 109, 108, 107, 106, 105, 105, 104, 103, 102, 101, 101, 100,
    137, 136, 135, 133, 131, 130, 129, 128, 126, 125, 123, 122, 121, 120, 119, 118,
    117, 116, 115, 114, 113, 112, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101,
    146, 145, 143, 142, 140, 138, 136, 135, 133, 132, 130, 129, 127, 126, 125, 124,
    122, 121, 120, 118, 117, 116, 114, 113, 112, 110, 109, 108, 106, 105, 104, 102,
    154, 152, 150, 149, 147, 145, 143, 142, 139, 137, 136, 134, 132, 131, 129, 128,
    126, 125, 123, 122, 120, 119, 117, 116, 114, 112, 111, 109, 108, 106, 105, 103,
    158, 156, 154, 152, 150, 148, 146, 145, 143, 141, 139, 138, 136, 134, 132, 131,
    129, 128, 126, 124, 123, 121, 119, 118, 116, 115, 113, 111, 110, 108, 106, 105,
    158, 157, 155, 153, 151, 149, 147, 145, 143, 142, 140, 138, 136, 135, 133, 132,
    130, 128, 127, 125, 123, 122, 120, 119, 117, 115, 114, 112, 111, 109, 107, 106,
    154, 153, 151, 150, 148, 146, 144, 143, 141, 140, 138, 137, 135, 133, 132, 131,
    129, 128, 126, 125, 123, 122, 120, 119, 117, 116, 115, 113, 112, 110, 109, 107,
    148, 147, 145, 144, 142, 141, 139, 138, 136, 135, 134, 132, 131, 130, 129, 128,
    126, 125, 124, 123, 121, 120, 119, 118, 117, 115, 114, 113, 112, 111, 109, 108,
    139, 139, 137, 137, 135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125, 124,
    123, 122, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 112, 111, 110,
    132, 131, 130, 129, 129, 128, 127, 126, 125, 125, 124, 123, 123, 122, 121, 121,
    120, 120, 119, 118, 118, 117, 116, 116, 115, 115, 114, 113, 113, 112, 111, 111,
    125, 125, 124, 124, 123, 123, 122, 121, 121, 121, 120, 120, 119, 119, 119, 118,
    118, 118, 117, 117, 116, 116, 116, 115, 115, 115, 114, 114, 113, 113, 113, 112,
    122, 122, 121, 121, 121, 120, 120, 119, 119, 119, 119, 118, 118, 118, 118, 117,
    117, 117, 117, 116, 116, 116, 116, 115, 115, 115, 115, 114, 114, 114, 114, 113,
    132, 132, 131, 130, 130, 129, 128, 128, 127, 126, 126, 125, 125, 124, 124, 123,
    123, 122, 122, 121, 121, 120, 120, 119, 118, 118, 117, 117, 116, 116, 115, 115,
    146, 145, 144, 144, 142, 141, 140, 139, 138, 137, 135, 135, 134, 133, 132, 131,
    130, 129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 120, 119, 118, 117, 116,
    157, 156, 154, 153, 152, 150, 149, 147, 146, 145, 143, 142, 141, 139, 138, 137,
    136, 135, 133, 132, 131, 130, 128, 127, 126, 125, 123, 122, 121, 120, 118, 117,
    163, 162, 161, 160, 157, 156, 154, 153, 151, 150, 148, 147, 145, 144, 142, 141,
    139, 138, 137, 135, 134, 132, 131, 130, 128, 127, 125, 124, 122, 121, 120, 118,
    165, 164, 162, 162, 160, 158, 156, 155, 153, 152, 150, 149, 147, 146, 144, 143,
    141, 140, 138, 137, 136, 134, 133, 131, 130, 128, 127, 126, 124, 123, 121, 120,
    165, 164, 162, 161, 159, 158, 156, 155, 153, 152, 150, 149, 147, 146, 144, 143,
    142, 140, 139, 137, 136, 135, 133, 132, 130, 129, 128, 126, 125, 124, 122, 121,
    164, 163, 161, 161, 159, 157, 156, 154, 153, 152, 150, 149, 147, 146, 145, 143,
    142, 141, 139, 138, 137, 135, 134, 133, 131, 130, 129, 128, 126, 125, 124, 122,
    163, 162, 161, 160, 158, 157, 155, 154, 153, 151, 150, 148, 147, 146, 145, 143,
    142, 141, 140, 138, 137, 136, 135, 133, 132, 131, 130, 128, 127, 126, 125, 123
};
static const uint16_t coeff_pos_block_06_8_8[11] = {
    0, 2, 3, 33, 64, 65, 66, 67, 96, 97, 129
};
static const int32_t coeff_val_block_06_8_8[11] = {
    -992, 992, 992, -992, 992, 992, -992, -992, 992, 992, -992
};
static const uint8_t recon_first_block_06_8_8[8] = {
    99, 99, 99, 100, 101, 102, 104, 105
};
static const uint8_t recon_last_block_06_8_8[8] = {
    121, 120, 120, 118, 118, 117, 117, 115
};
static const uint8_t predictor_block_07_16_0[2048] = {
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83
};
static const uint16_t coeff_pos_block_07_16_0[3] = {
    0, 1, 2
};
static const int32_t coeff_val_block_07_16_0[3] = {
    -992, 496, -496
};
static const uint8_t recon_first_block_07_16_0[8] = {
    72, 72, 72, 72, 73, 73, 73, 73
};
static const uint8_t recon_last_block_07_16_0[8] = {
    59, 59, 58, 58, 57, 57, 57, 57
};
static const uint8_t predictor_block_08_16_8[1024] = {
    75, 75, 75, 75, 75, 75, 75, 75, 76, 76, 76, 76, 77, 77, 77, 77,
    77, 77, 78, 78, 78, 78, 78, 78, 79, 79, 79, 79, 79, 79, 78, 78,
    78, 78, 77, 77, 77, 77, 76, 76, 76, 76, 75, 75, 74, 74, 73, 73,
    73, 73, 72, 72, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 69, 69,
    76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 77, 77, 77, 77,
    77, 77, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
    78, 78, 77, 77, 77, 77, 76, 76, 76, 76, 76, 76, 75, 75, 74, 74,
    74, 74, 73, 73, 73, 73, 72, 72, 72, 72, 71, 71, 71, 71, 71, 71,
    76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 76, 76, 76, 76, 76, 76, 75, 75,
    75, 75, 74, 74, 74, 74, 74, 74, 73, 73, 73, 73, 73, 73, 73, 73,
    76, 76, 76, 76, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 76, 76, 76, 76, 76, 76,
    76, 76, 76, 76, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
    82, 82, 81, 81, 81, 81, 80, 80, 80, 79, 79, 79, 78, 78, 78, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    81, 81, 80, 80, 80, 80, 79, 79, 79, 79, 78, 78, 78, 78, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    80, 80, 80, 80, 79, 79, 79, 79, 79, 78, 78, 78, 78, 78, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    80, 80, 80, 80, 79, 79, 79, 79, 79, 78, 78, 78, 78, 78, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    81, 81, 81, 80, 80, 80, 80, 79, 79, 79, 79, 78, 78, 78, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    83, 82, 82, 82, 81, 81, 81, 80, 80, 79, 79, 79, 78, 78, 78, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    85, 85, 84, 84, 83, 83, 82, 82, 81, 81, 80, 80, 79, 79, 78, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    88, 88, 87, 86, 85, 85, 84, 83, 83, 82, 81, 81, 80, 79, 78, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 82, 81, 80, 79, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    95, 94, 92, 92, 90, 89, 88, 87, 86, 85, 84, 83, 81, 80, 79, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    95, 94, 93, 92, 91, 89, 89, 87, 86, 85, 84, 83, 82, 80, 79, 78,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77
};
static const uint16_t coeff_pos_block_08_16_8[4] = {
    1, 32, 33, 64
};
static const int32_t coeff_val_block_08_16_8[4] = {
    992, -992, -992, 992
};
static const uint8_t recon_first_block_08_16_8[8] = {
    70, 70, 70, 70, 71, 71, 71, 71
};
static const uint8_t recon_last_block_08_16_8[8] = {
    74, 74, 73, 73, 72, 72, 72, 72
};
static const uint8_t predictor_block_09_16_12[256] = {
    132, 141, 142, 140, 139, 137, 135, 134, 132, 131, 129, 128, 126, 124, 122, 120,
    127, 139, 142, 141, 140, 137, 136, 135, 133, 132, 130, 129, 127, 125, 123, 120,
    122, 135, 142, 141, 140, 138, 136, 135, 133, 132, 130, 129, 127, 125, 123, 121,
    114, 130, 141, 142, 141, 139, 137, 136, 134, 133, 131, 130, 128, 126, 124, 122,
    109, 125, 138, 142, 141, 140, 138, 136, 135, 133, 132, 130, 129, 127, 125, 123,
    102, 119, 134, 142, 142, 140, 139, 136, 135, 133, 132, 130, 129, 128, 125, 123,
    96, 112, 128, 140, 142, 141, 139, 137, 136, 134, 133, 131, 130, 128, 126, 124,
    89, 106, 124, 137, 142, 141, 140, 138, 136, 135, 133, 132, 130, 129, 127, 125,
    83, 100, 116, 132, 141, 142, 140, 139, 137, 135, 134, 132, 131, 129, 128, 126,
    80, 93, 111, 127, 139, 142, 141, 140, 137, 136, 135, 133, 132, 130, 129, 127,
    79, 87, 104, 122, 135, 142, 141, 140, 138, 136, 135, 133, 132, 130, 129, 127,
    80, 82, 98, 114, 130, 141, 142, 141, 139, 137, 136, 134, 133, 131, 130, 128,
    83, 79, 91, 108, 125, 138, 142, 141, 140, 138, 136, 135, 133, 132, 130, 129,
    88, 79, 85, 102, 118, 134, 142, 142, 140, 139, 136, 135, 133, 132, 130, 129,
    94, 81, 81, 95, 112, 128, 140, 142, 141, 139, 137, 136, 134, 133, 131, 130,
    101, 85, 79, 89, 106, 124, 137, 142, 141, 140, 138, 136, 135, 133, 132, 130
};
static const uint8_t recon_first_block_09_16_12[8] = {
    132, 141, 142, 140, 139, 137, 135, 134
};
static const uint8_t recon_last_block_09_16_12[8] = {
    141, 140, 138, 136, 135, 133, 132, 130
};
static const uint8_t predictor_block_10_20_12[256] = {
    119, 118, 116, 115, 113, 113, 111, 110, 108, 107, 106, 105, 103, 102, 100, 100,
    119, 118, 117, 116, 114, 113, 111, 111, 109, 108, 106, 106, 104, 103, 101, 101,
    120, 119, 118, 116, 115, 114, 113, 112, 110, 109, 107, 107, 104, 104, 102, 102,
    121, 120, 119, 117, 116, 115, 114, 113, 111, 110, 109, 108, 106, 105, 104, 103,
    121, 121, 119, 118, 117, 116, 114, 114, 112, 111, 110, 109, 107, 106, 105, 104,
    122, 121, 120, 119, 117, 117, 115, 115, 113, 112, 110, 109, 108, 107, 106, 105,
    123, 122, 121, 120, 118, 117, 116, 115, 113, 113, 111, 111, 109, 108, 107, 106,
    124, 123, 122, 120, 119, 118, 117, 116, 115, 114, 112, 112, 110, 109, 108, 107,
    125, 124, 123, 121, 120, 119, 118, 117, 115, 115, 113, 112, 111, 110, 109, 108,
    126, 125, 124, 122, 121, 120, 119, 118, 116, 115, 114, 113, 112, 111, 110, 109,
    126, 125, 124, 123, 121, 120, 119, 119, 117, 116, 115, 114, 112, 112, 111, 110,
    127, 126, 125, 124, 123, 122, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111,
    128, 127, 126, 125, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 112, 111,
    128, 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 113, 112,
    129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 118, 117, 116, 115, 114, 113,
    130, 129, 128, 127, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114
};
static const uint16_t coeff_pos_block_10_20_12[4] = {
    0, 1, 18, 32
};
static const int32_t coeff_val_block_10_20_12[4] = {
    -1984, -1984, 1984, 1984
};
static const uint8_t recon_first_block_10_20_12[8] = {
    123, 126, 125, 125, 121, 119, 114, 111
};
static const uint8_t recon_last_block_10_20_12[8] = {
    131, 141, 144, 142, 135, 127, 119, 114
};
static const uint8_t predictor_block_11_24_12[512] = {
    122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
    122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
    141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
    141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
    158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
    158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
    142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
    142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
    110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
    96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
    96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
    87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87,
    87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87,
    84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84,
    84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84,
    86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86,
    86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86,
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
    101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
    101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114,
    114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114
};
static const uint16_t coeff_pos_block_11_24_12[9] = {
    0, 1, 2, 32, 33, 34, 64, 98, 160
};
static const int32_t coeff_val_block_11_24_12[9] = {
    1984, -992, -992, -992, 992, 992, -992, -992, 992
};
static const uint8_t recon_first_block_11_24_12[8] = {
    119, 119, 120, 121, 123, 125, 128, 130
};
static const uint8_t recon_last_block_11_24_12[8] = {
    148, 148, 147, 146, 145, 144, 144, 143
};
static const uint8_t predictor_block_12_32_0[1024] = {
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    58, 58, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    62, 61, 60, 59, 58, 58, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    66, 65, 64, 63, 62, 61, 60, 59, 58, 58, 57, 57, 57, 57, 57, 57,
    69, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 58, 57, 57,
    71, 71, 71, 70, 69, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59,
    70, 70, 71, 71, 71, 71, 71, 70, 69, 69, 68, 67, 66, 65, 64, 63,
    67, 68, 68, 69, 70, 70, 71, 71, 71, 71, 71, 70, 69, 69, 68, 67,
    64, 65, 65, 66, 67, 68, 68, 69, 70, 70, 71, 71, 71, 71, 71, 70,
    60, 61, 62, 63, 64, 65, 65, 66, 67, 68, 68, 69, 70, 70, 71, 71,
    57, 58, 58, 59, 60, 61, 62, 63, 64, 65, 65, 66, 67, 68, 68, 69,
    56, 56, 57, 57, 57, 58, 58, 59, 60, 61, 62, 63, 64, 65, 65, 66,
    57, 56, 56, 56, 56, 56, 57, 57, 57, 58, 58, 59, 60, 61, 62, 63,
    58, 58, 57, 57, 57, 56, 56, 56, 56, 56, 57, 57, 57, 58, 58, 59,
    61, 60, 60, 59, 58, 58, 57, 57, 57, 56, 56, 56, 56, 56, 57, 57,
    64, 64, 63, 62, 61, 60, 60, 59, 58, 58, 57, 57, 57, 56, 56, 56,
    67, 66, 66, 65, 64, 64, 63, 62, 61, 60, 60, 59, 58, 58, 57, 57,
    72, 70, 69, 68, 67, 66, 66, 65, 64, 64, 63, 62, 61, 60, 60, 59,
    82, 79, 76, 74, 72, 70, 69, 68, 67, 66, 66, 65, 64, 64, 63, 62,
    98, 93, 89, 86, 82, 79, 76, 74, 72, 70, 69, 68, 67, 66, 66, 65,
    116, 111, 106, 102, 98, 93, 89, 86, 82, 79, 76, 74, 72, 70, 69, 68,
    137, 132, 126, 121, 116, 111, 106, 102, 98, 93, 89, 86, 82, 79, 76, 74,
    157, 152, 147, 142, 137, 132, 126, 121, 116, 111, 106, 102, 98, 93, 89, 86,
    174, 170, 166, 161, 157, 152, 147, 142, 137, 132, 126, 121, 116, 111, 106, 102,
    183, 182, 180, 177, 174, 170, 166, 161, 157, 152, 147, 142, 137, 132, 126, 121,
    179, 182, 183, 183, 183, 182, 180, 177, 174, 170, 166, 161, 157, 152, 147, 142,
    160, 166, 171, 175, 179, 182, 183, 183, 183, 182, 180, 177, 174, 170, 166, 161,
    135, 141, 148, 154, 160, 166, 171, 175, 179, 182, 183, 183, 183, 182, 180, 177,
    112, 117, 123, 129, 135, 141, 148, 154, 160, 166, 171, 175, 179, 182, 183, 183,
    103, 104, 106, 109, 112, 117, 123, 129, 135, 141, 148, 154, 160, 166, 171, 175,
    107, 105, 104, 104, 103, 104, 106, 109, 112, 117, 123, 129, 135, 141, 148, 154,
    119, 116, 113, 110, 107, 105, 104, 104, 103, 104, 106, 109, 112, 117, 123, 129,
    133, 130, 126, 123, 119, 116, 113, 110, 107, 105, 104, 104, 103, 104, 106, 109,
    140, 139, 137, 135, 133, 130, 126, 123, 119, 116, 113, 110, 107, 105, 104, 104,
    143, 143, 142, 141, 140, 139, 137, 135, 133, 130, 126, 123, 119, 116, 113, 110
};
static const uint16_t coeff_pos_block_12_32_0[2] = {
    0, 32
};
static const int32_t coeff_val_block_12_32_0[2] = {
    -992, 992
};
static const uint8_t recon_first_block_12_32_0[8] = {
    60, 60, 60, 60, 60, 60, 60, 60
};
static const uint8_t recon_last_block_12_32_0[8] = {
    136, 133, 129, 126, 122, 119, 116, 113
};
static const uint8_t predictor_block_13_36_0[1024] = {
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    58, 58, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    57, 57, 58, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    57, 57, 57, 58, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60,
    56, 56, 57, 57, 57, 58, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60,
    55, 55, 56, 57, 57, 57, 58, 59, 59, 59, 60, 60, 60, 60, 60, 60,
    54, 54, 55, 56, 57, 57, 57, 57, 58, 59, 59, 59, 60, 60, 60, 60,
    53, 53, 54, 55, 55, 56, 57, 57, 57, 58, 59, 59, 59, 60, 60, 60,
    52, 52, 53, 54, 54, 55, 56, 57, 57, 57, 57, 58, 59, 59, 59, 60,
    51, 51, 52, 53, 53, 54, 55, 56, 56, 57, 57, 57, 58, 59, 59, 59,
    50, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 57, 57, 58, 58, 59,
    49, 49, 50, 51, 51, 52, 53, 54, 54, 55, 56, 56, 57, 57, 57, 58,
    48, 48, 49, 50, 50, 51, 52, 53, 53, 54, 55, 55, 56, 57, 57, 57,
    47, 47, 48, 49, 49, 50, 51, 52, 52, 53, 54, 54, 55, 56, 56, 57,
    46, 46, 47, 48, 48, 49, 50, 51, 51, 52, 53, 53, 54, 55, 55, 56,
    45, 45, 46, 47, 47, 48, 49, 50, 50, 51, 52, 52, 53, 54, 54, 55,
    44, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 51, 52, 53, 53, 54,
    43, 43, 44, 45, 45, 46, 47, 48, 48, 49, 50, 50, 51, 52, 52, 53,
    42, 42, 43, 44, 44, 45, 46, 47, 47, 48, 49, 49, 50, 51, 51, 52,
    41, 41, 42, 43, 43, 44, 45, 46, 46, 47, 48, 48, 49, 50, 50, 51,
    41, 41, 41, 42, 42, 43, 44, 45, 45, 46, 47, 47, 48, 49, 49, 50,
    40, 40, 41, 41, 41, 42, 43, 44, 44, 45, 46, 46, 47, 48, 48, 49,
    40, 40, 40, 41, 41, 41, 42, 43, 43, 44, 45, 45, 46, 47, 47, 48,
    39, 39, 40, 40, 40, 41, 41, 41, 42, 43, 44, 44, 45, 46, 46, 47,
    39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 43, 43, 44, 45, 45, 46,
    38, 38, 39, 39, 39, 40, 40, 41, 41, 41, 41, 42, 43, 44, 44, 45,
    38, 38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 43, 43, 44,
    38, 38, 38, 38, 38, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 43,
    41, 39, 38, 38, 38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42,
    43, 41, 40, 38, 38, 38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41,
    46, 44, 42, 40, 39, 38, 38, 38, 38, 39, 39, 39, 40, 40, 40, 41,
    50, 48, 45, 43, 41, 40, 38, 38, 38, 38, 39, 39, 39, 40, 40, 40,
    53, 51, 49, 46, 44, 42, 40, 39, 38, 38, 38, 38, 39, 39, 39, 40,
    53, 53, 52, 50, 47, 45, 42, 41, 40, 38, 38, 38, 38, 39, 39, 39,
    52, 53, 53, 53, 51, 49, 46, 43, 42, 40, 39, 38, 38, 38, 38, 39,
    50, 52, 52, 53, 53, 52, 50, 47, 44, 42, 41, 39, 38, 38, 38, 38,
    48, 49, 51, 52, 53, 53, 52, 51, 48, 45, 43, 41, 40, 39, 38, 38,
    47, 47, 48, 50, 52, 52, 53, 53, 52, 49, 47, 44, 42, 41, 39, 38,
    46, 46, 47, 48, 49, 51, 52, 53, 53, 52, 51, 48, 45, 43, 41, 40,
    46, 46, 46, 47, 47, 48, 50, 52, 52, 53, 53, 51, 49, 46, 44, 42,
    48, 46, 46, 46, 46, 47, 48, 50, 51, 52, 53, 53, 52, 50, 48, 45,
    51, 49, 47, 46, 46, 46, 47, 47, 49, 51, 52, 52, 53, 53, 51, 49,
    55, 52, 50, 48, 46, 46, 46, 46, 47, 48, 50, 51, 52, 53, 53, 52,
    60, 56, 54, 51, 48, 47, 46, 46, 46, 47, 47, 49, 51, 52, 52, 53,
    66, 61, 58, 55, 52, 50, 47, 46, 46, 46, 46, 47, 48, 50, 51, 52,
    75, 68, 64, 59, 56, 54, 51, 48, 47, 46, 46, 46, 47, 47, 49, 51,
    89, 79, 71, 65, 61, 57, 55, 52, 49, 47, 46, 46, 46, 47, 47, 48,
    107, 94, 84, 75, 68, 63, 59, 56, 53, 50, 48, 47, 46, 46, 46, 47,
    126, 113, 100, 88, 79, 71, 65, 61, 57, 54, 52, 49, 47, 46, 46, 46,
    145, 132, 119, 105, 93, 83, 74, 67, 63, 59, 56, 53, 50, 48, 46, 46,
    163, 152, 138, 125, 111, 99, 88, 78, 70, 65, 60, 57, 54, 51, 49, 47,
    173, 167, 157, 144, 131, 118, 104, 92, 82, 73, 67, 62, 58, 55, 53, 50,
    174, 174, 170, 162, 150, 137, 124, 110, 98, 87, 77, 70, 64, 60, 57, 54,
    164, 172, 174, 172, 166, 156, 143, 130, 116, 103, 91, 81, 73, 66, 62, 58,
    148, 160, 169, 174, 174, 169, 161, 149, 135, 122, 109, 97, 86, 76, 69, 64,
    131, 142, 154, 165, 172, 174, 172, 165, 155, 141, 128, 115, 102, 90, 80, 72,
    118, 126, 137, 149, 161, 169, 174, 174, 169, 160, 148, 134, 121, 108, 96, 85,
    113, 116, 122, 132, 143, 155, 166, 173, 174, 171, 165, 154, 140, 127, 114, 101
};
static const uint8_t recon_first_block_13_36_0[8] = {
    60, 60, 60, 60, 60, 60, 60, 60
};
static const uint8_t recon_last_block_13_36_0[8] = {
    174, 171, 165, 154, 140, 127, 114, 101
};
static const uint8_t predictor_block_14_40_0[2048] = {
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 56, 55, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 55, 55, 55, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    56, 55, 55, 55, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    55, 55, 55, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    55, 55, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    55, 55, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    52, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    51, 51, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    51, 51, 51, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    50, 51, 51, 51, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    50, 50, 51, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    49, 50, 50, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    49, 49, 50, 50, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    49, 49, 50, 50, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    48, 49, 49, 50, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    48, 48, 49, 50, 50, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    48, 48, 49, 50, 50, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    47, 48, 49, 49, 50, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    47, 48, 49, 49, 50, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    47, 48, 48, 49, 50, 51, 51, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    47, 48, 48, 49, 50, 51, 51, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    46, 47, 48, 49, 50, 51, 51, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    46, 47, 48, 49, 50, 51, 51, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    46, 47, 48, 49, 50, 51, 51, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    47, 48, 49, 49, 50, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    48, 49, 49, 50, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    49, 50, 50, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    51, 51, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    51, 51, 51, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    50, 51, 51, 51, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    50, 50, 51, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    50, 50, 51, 51, 51, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    50, 51, 51, 51, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    52, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    55, 55, 55, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    58, 57, 57, 56, 55, 55, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    61, 60, 59, 58, 57, 56, 55, 54, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    67, 66, 64, 62, 60, 58, 57, 55, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    74, 72, 69, 67, 64, 61, 58, 56, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53
};
static const uint16_t coeff_pos_block_14_40_0[3] = {
    0, 1, 64
};
static const int32_t coeff_val_block_14_40_0[3] = {
    496, -496, 496
};
static const uint8_t recon_first_block_14_40_0[8] = {
    61, 61, 61, 61, 61, 61, 61, 60
};
static const uint8_t recon_last_block_14_40_0[8] = {
    72, 72, 73, 73, 73, 74, 74, 74
};
static const uint8_t predictor_block_15_0_16[2048] = {
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
    163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163
};
static const uint16_t coeff_pos_block_15_0_16[18] = {
    1, 3, 4, 33, 34, 35, 36, 64, 66, 96, 128, 131, 160, 192, 194, 224,
    257, 258
};
static const int32_t coeff_val_block_15_0_16[18] = {
    496, -496, 496, 496, -496, -496, 496, -496, -496, -496, 496, 496,
    -496, -496, 496, 496, 496, 496
};
static const uint8_t recon_first_block_15_0_16[8] = {
    185, 185, 183, 183, 179, 179, 173, 173
};
static const uint8_t recon_last_block_15_0_16[8] = {
    165, 165, 163, 163, 161, 161, 160, 160
};
static const uint8_t predictor_block_16_16_16[2048] = {
    122, 126, 131, 137, 139, 143, 145, 147, 146, 147, 149, 149, 146, 141, 135, 126,
    114, 110, 112, 118, 124, 134, 142, 148, 149, 148, 143, 138, 129, 116, 107, 98,
    94, 96, 99, 102, 105, 110, 113, 118, 122, 126, 131, 134, 137, 140, 143, 145,
    147, 148, 149, 150, 150, 151, 150, 150, 149, 148, 147, 146, 146, 147, 147, 147,
    131, 137, 141, 145, 145, 147, 148, 150, 149, 149, 149, 145, 135, 126, 119, 116,
    112, 118, 127, 136, 142, 148, 150, 149, 143, 138, 130, 118, 107, 98, 96, 98,
    99, 102, 107, 111, 113, 118, 122, 127, 131, 134, 137, 140, 143, 145, 147, 149,
    149, 150, 150, 151, 150, 150, 149, 148, 147, 146, 146, 147, 147, 147, 147, 146,
    143, 146, 148, 151, 151, 154, 158, 160, 158, 156, 153, 148, 137, 135, 136, 140,
    141, 147, 153, 156, 156, 155, 151, 147, 139, 130, 122, 116, 112, 114, 117, 119,
    119, 123, 126, 129, 131, 134, 137, 140, 141, 143, 145, 147, 148, 150, 151, 152,
    152, 152, 151, 151, 150, 150, 149, 148, 148, 148, 148, 148, 148, 147, 146, 146,
    148, 151, 154, 158, 158, 160, 162, 162, 153, 148, 145, 143, 136, 140, 146, 151,
    153, 156, 158, 156, 151, 147, 141, 132, 122, 116, 115, 117, 117, 119, 123, 126,
    126, 129, 132, 135, 137, 140, 142, 144, 145, 147, 148, 150, 151, 152, 152, 152,
    151, 151, 151, 150, 149, 148, 148, 149, 148, 148, 148, 147, 146, 146, 144, 142,
    155, 159, 164, 168, 167, 167, 167, 166, 155, 153, 154, 157, 154, 158, 162, 164,
    162, 160, 158, 155, 148, 141, 135, 131, 126, 128, 131, 133, 132, 135, 137, 139,
    138, 140, 142, 144, 144, 146, 148, 149, 150, 151, 152, 153, 153, 153, 153, 153,
    152, 151, 150, 149, 149, 150, 150, 149, 149, 149, 148, 147, 145, 144, 142, 140,
    164, 168, 172, 174, 167, 166, 165, 163, 154, 157, 160, 162, 162, 164, 164, 162,
    158, 155, 150, 144, 135, 131, 131, 133, 131, 133, 135, 138, 137, 139, 140, 142,
    142, 144, 145, 146, 148, 149, 150, 151, 152, 153, 153, 153, 153, 153, 152, 151,
    150, 149, 149, 150, 150, 149, 149, 149, 148, 147, 146, 144, 142, 140, 137, 135,
    174, 176, 178, 179, 171, 170, 170, 171, 165, 167, 169, 169, 167, 165, 163, 161,
    155, 150, 146, 144, 139, 141, 143, 145, 143, 144, 146, 146, 145, 146, 147, 148,
    147, 148, 149, 151, 151, 152, 152, 153, 154, 154, 154, 154, 153, 152, 152, 151,
    151, 151, 151, 151, 151, 150, 149, 149, 147, 146, 144, 142, 139, 137, 135, 133,
    178, 179, 180, 179, 170, 171, 172, 172, 169, 169, 169, 167, 163, 161, 158, 154,
    146, 144, 144, 146, 143, 145, 147, 148, 146, 146, 147, 148, 147, 148, 149, 149,
    149, 151, 151, 152, 152, 153, 154, 154, 154, 154, 153, 153, 152, 151, 151, 151,
    151, 151, 151, 150, 149, 149, 147, 146, 144, 142, 140, 138, 135, 133, 131, 130,
    182, 181, 182, 181, 175, 175, 175, 174, 171, 169, 167, 166, 161, 158, 156, 155,
    149, 151, 153, 155, 152, 153, 154, 154, 151, 151, 151, 151, 150, 150, 151, 152,
    152, 153, 153, 154, 155, 155, 155, 155, 154, 154, 153, 152, 152, 152, 152, 152,
    152, 151, 150, 150, 148, 147, 145, 144, 141, 140, 137, 135, 133, 132, 132, 134,
    182, 181, 181, 179, 175, 174, 173, 172, 167, 166, 164, 162, 156, 155, 156, 158,
    153, 155, 156, 157, 154, 154, 154, 153, 151, 151, 151, 151, 151, 152, 152, 153,
    153, 154, 155, 155, 155, 155, 155, 154, 153, 152, 152, 153, 152, 152, 152, 151,
    150, 150, 149, 148, 145, 144, 142, 140, 137, 135, 134, 133, 132, 134, 138, 143,
    182, 181, 180, 178, 174, 173, 171, 171, 166, 165, 164, 164, 159, 161, 163, 164,
    160, 161, 161, 160, 156, 156, 155, 155, 153, 152, 153, 153, 153, 154, 154, 155,
    155, 156, 156, 156, 155, 155, 154, 153, 153, 153, 153, 153, 153, 152, 152, 151,
    150, 149, 147, 146, 143, 142, 140, 138, 136, 135, 135, 137, 140, 145, 151, 157,
    180, 178, 177, 175, 171, 171, 170, 170, 164, 164, 165, 167, 163, 164, 165, 165,
    161, 160, 159, 158, 155, 155, 154, 153, 153, 153, 153, 154, 154, 155, 156, 156,
    156, 156, 156, 155, 154, 153, 153, 154, 153, 153, 153, 152, 152, 151, 150, 149,
    147, 146, 144, 142, 140, 138, 136, 136, 135, 137, 141, 145, 151, 157, 162, 165,
    177, 175, 174, 174, 171, 171, 171, 172, 167, 169, 171, 172, 168, 168, 168, 166,
    161, 160, 158, 157, 155, 154, 154, 154, 154, 154, 155, 156, 156, 157, 157, 157,
    156, 156, 155, 154, 154, 154, 154, 154, 153, 153, 153, 152, 151, 150, 148, 148,
    145, 144, 142, 140, 138, 138, 138, 139, 142, 146, 152, 157, 162, 165, 166, 164,
    174, 174, 175, 175, 171, 172, 174, 176, 171, 172, 172, 172, 168, 166, 164, 162,
    158, 157, 156, 155, 154, 154, 154, 155, 155, 156, 156, 157, 157, 157, 156, 156,
    155, 154, 154, 155, 154, 154, 154, 153, 153, 152, 151, 150, 148, 148, 146, 144,
    142, 140, 139, 138, 138, 139, 143, 147, 152, 157, 162, 165, 166, 164, 161, 156,
    176, 177, 178, 180, 175, 177, 179, 179, 175, 174, 173, 171, 166, 164, 162, 160,
    157, 156, 155, 155, 154, 155, 155, 156, 157, 157, 158, 158, 157, 157, 156, 155,
    155, 155, 155, 155, 154, 154, 153, 153, 152, 151, 150, 149, 147, 145, 144, 142,
    140, 140, 140, 141, 144, 147, 153, 157, 161, 164, 165, 164, 161, 156, 150, 144,
    178, 180, 182, 184, 179, 179, 180, 179, 173, 171, 168, 166, 162, 160, 159, 157,
    155, 155, 155, 155, 155, 156, 157, 158, 158, 158, 157, 157, 156, 155, 155, 156,
    155, 155, 155, 154, 153, 153, 152, 152, 150, 149, 147, 146, 144, 142, 141, 141,
    140, 141, 144, 148, 153, 157, 161, 164, 165, 164, 161, 156, 150, 144, 138, 134,
    182, 184, 185, 185, 181, 179, 178, 175, 169, 167, 164, 162, 159, 157, 156, 156,
    155, 156, 156, 157, 157, 158, 159, 159, 158, 158, 157, 156, 156, 156, 156, 155,
    155, 155, 154, 154, 153, 152, 151, 150, 148, 147, 145, 144, 142, 142, 142, 143,
    145, 149, 153, 157, 161, 164, 165, 164, 161, 156, 151, 145, 139, 136, 136, 136,
    185, 185, 185, 184, 178, 175, 172, 169, 164, 162, 160, 158, 156, 156, 156, 156,
    156, 157, 158, 158, 159, 159, 158, 158, 157, 156, 156, 157, 156, 155, 155, 155,
    154, 154, 153, 153, 151, 150, 149, 148, 145, 144, 143, 143, 142, 143, 146, 149,
    153, 157, 161, 164, 165, 164, 161, 157, 151, 145, 140, 136, 136, 136, 136, 136,
    186, 185, 183, 179, 173, 170, 167, 164, 161, 159, 157, 157, 156, 156, 156, 157,
    158, 159, 159, 159, 158, 158, 158, 157, 157, 157, 157, 156, 156, 156, 155, 155,
    154, 153, 152, 152, 150, 149, 147, 146, 145, 144, 144, 145, 147, 150, 154, 158,
    161, 164, 164, 163, 161, 157, 152, 146, 141, 138, 138, 138, 137, 137, 137, 137,
    183, 179, 176, 172, 167, 164, 162, 160, 157, 157, 156, 157, 156, 157, 158, 159,
    159, 159, 159, 159, 158, 157, 157, 157, 157, 156, 156, 156, 155, 155, 154, 154,
    152, 152, 150, 149, 147, 146, 145, 145, 144, 145, 148, 151, 154, 158, 161, 163,
    164, 163, 161, 157, 152, 146, 142, 138, 138, 138, 138, 138, 137, 137, 137, 137,
    176, 172, 169, 166, 162, 160, 158, 158, 156, 157, 157, 158, 158, 159, 160, 160,
    159, 159, 159, 158, 157, 158, 158, 157, 156, 156, 156, 156, 155, 154, 153, 153,
    151, 150, 149, 148, 146, 146, 146, 147, 148, 151, 155, 158, 161, 163, 164, 163,
    161, 157, 153, 148, 143, 140, 140, 140, 139, 139, 139, 139, 138, 138, 138, 138,
    169, 166, 163, 161, 158, 158, 157, 157, 157, 158, 159, 160, 160, 160, 160, 160,
    159, 158, 158, 158, 158, 157, 157, 157, 156, 156, 155, 155, 153, 153, 152, 151,
    149, 148, 147, 147, 146, 147, 149, 152, 155, 158, 161, 163, 164, 163, 161, 157,
    153, 148, 144, 141, 140, 140, 140, 140, 139, 139, 139, 139, 138, 138, 138, 138,
    164, 161, 159, 158, 157, 157, 157, 158, 159, 160, 160, 160, 160, 160, 160, 159,
    158, 158, 158, 157, 157, 157, 157, 156, 155, 155, 154, 154, 152, 152, 151, 150,
    148, 148, 148, 149, 150, 152, 155, 158, 161, 163, 163, 163, 161, 157, 153, 149,
    145, 142, 142, 142, 141, 141, 141, 141, 140, 140, 140, 140, 139, 139, 139, 139,
    159, 158, 157, 158, 157, 158, 159, 160, 160, 160, 160, 160, 160, 159, 159, 159,
    158, 157, 157, 157, 157, 156, 156, 156, 154, 154, 153, 152, 151, 150, 149, 149,
    148, 149, 150, 153, 155, 158, 161, 163, 163, 163, 161, 158, 153, 149, 145, 143,
    142, 142, 142, 142, 141, 141, 141, 141, 140, 140, 140, 140, 139, 139, 139, 139,
    157, 158, 158, 159, 159, 160, 161, 161, 160, 160, 160, 159, 159, 159, 159, 158,
    158, 157, 157, 157, 156, 156, 155, 155, 153, 153, 152, 151, 150, 149, 149, 150,
    151, 153, 156, 158, 161, 163, 163, 162, 161, 158, 154, 150, 146, 144, 144, 144,
    143, 143, 143, 143, 142, 142, 142, 142, 141, 141, 141, 141, 140, 140, 140, 140,
    158, 159, 160, 161, 161, 161, 161, 161, 160, 159, 159, 159, 159, 158, 158, 158,
    157, 157, 157, 156, 155, 155, 154, 154, 152, 151, 151, 151, 149, 150, 152, 154,
    156, 158, 161, 162, 163, 162, 161, 158, 154, 150, 147, 145, 144, 144, 144, 144,
    143, 143, 143, 143, 142, 142, 142, 142, 141, 141, 141, 141, 140, 140, 140, 140,
    160, 161, 161, 161, 161, 161, 161, 160, 159, 160, 160, 159, 158, 158, 158, 158,
    157, 157, 156, 156, 154, 154, 153, 153, 151, 151, 151, 151, 152, 154, 157, 159,
    161, 162, 163, 162, 161, 158, 155, 151, 148, 145, 145, 145, 145, 145, 145, 145,
    144, 144, 144, 144, 143, 143, 143, 143, 142, 142, 142, 142, 141, 141, 141, 141,
    161, 161, 161, 161, 161, 160, 160, 160, 160, 159, 159, 159, 158, 158, 158, 157,
    156, 156, 155, 155, 153, 153, 152, 152, 151, 151, 153, 155, 157, 159, 161, 162,
    163, 162, 160, 158, 155, 151, 148, 146, 145, 145, 145, 145, 145, 145, 145, 145,
    144, 144, 144, 144, 143, 143, 143, 143, 142, 142, 142, 142, 141, 141, 141, 141,
    162, 162, 161, 160, 160, 160, 160, 159, 159, 159, 158, 158, 158, 157, 157, 157,
    156, 155, 155, 154, 153, 153, 153, 153, 153, 155, 157, 159, 161, 162, 162, 162,
    160, 158, 155, 152, 149, 147, 147, 147, 146, 146, 146, 146, 145, 145, 145, 145,
    144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142,
    161, 160, 160, 161, 160, 159, 159, 159, 158, 158, 158, 158, 157, 157, 156, 156,
    155, 154, 154, 154, 153, 153, 154, 156, 157, 159, 161, 162, 162, 162, 160, 158,
    155, 152, 150, 148, 147, 147, 147, 147, 146, 146, 146, 146, 145, 145, 145, 145,
    144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142,
    160, 161, 161, 160, 159, 159, 159, 159, 158, 158, 158, 158, 157, 156, 156, 156,
    154, 154, 154, 154, 155, 156, 158, 159, 161, 162, 162, 161, 160, 158, 156, 154,
    150, 149, 149, 149, 148, 148, 148, 148, 147, 147, 147, 147, 146, 146, 146, 146,
    145, 145, 145, 145, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 143, 143,
    161, 160, 160, 160, 159, 159, 159, 159, 158, 158, 157, 157, 156, 156, 155, 155,
    154, 154, 155, 156, 158, 159, 160, 161, 162, 161, 160, 159, 156, 154, 151, 150,
    149, 149, 149, 149, 148, 148, 148, 148, 147, 147, 147, 147, 146, 146, 146, 146,
    145, 145, 145, 145, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 143, 143
};
static const uint16_t coeff_pos_block_16_16_16[12] = {
    3, 5, 32, 34, 35, 36, 37, 65, 66, 96, 132, 226
};
static const int32_t coeff_val_block_16_16_16[12] = {
    992, -496, -992, 496, 496, -496, -496, 496, -496, 496, 496, 496
};
static const uint8_t recon_first_block_16_16_16[8] = {
    143, 147, 154, 160, 164, 168, 172, 174
};
static const uint8_t recon_last_block_16_16_16[8] = {
    118, 118, 119, 119, 120, 120, 122, 122
};
static const uint8_t predictor_block_17_32_16[512] = {
    132, 129, 131, 128, 127, 124, 123, 121, 119, 116, 115, 113, 112, 112, 113, 117,
    124, 132, 142, 152, 160, 166, 168, 166, 161, 152, 142, 131, 118, 106, 96, 89,
    123, 119, 119, 115, 114, 112, 112, 110, 109, 108, 109, 110, 112, 116, 123, 131,
    141, 149, 158, 163, 166, 164, 160, 153, 143, 134, 122, 112, 103, 98, 93, 92,
    113, 108, 108, 105, 106, 104, 105, 105, 106, 108, 111, 116, 123, 130, 139, 148,
    156, 161, 164, 162, 159, 153, 145, 136, 126, 117, 110, 105, 101, 100, 97, 96,
    103, 100, 101, 99, 101, 102, 104, 106, 110, 115, 122, 130, 138, 146, 154, 159,
    162, 161, 158, 153, 146, 139, 130, 123, 116, 112, 108, 108, 105, 104, 99, 98,
    97, 96, 98, 99, 102, 105, 109, 114, 122, 129, 137, 145, 152, 157, 160, 160,
    158, 153, 147, 141, 133, 127, 121, 118, 115, 115, 111, 110, 106, 104, 100, 98,
    95, 97, 101, 104, 109, 114, 121, 129, 136, 143, 151, 156, 159, 159, 157, 153,
    148, 143, 137, 131, 126, 124, 121, 121, 118, 117, 112, 110, 106, 103, 100, 99,
    99, 103, 108, 114, 121, 128, 135, 142, 150, 154, 158, 157, 157, 153, 149, 145,
    139, 135, 131, 129, 126, 126, 123, 122, 118, 116, 111, 109, 105, 103, 101, 99,
    107, 113, 121, 128, 135, 141, 149, 153, 156, 157, 156, 153, 150, 146, 142, 138,
    135, 134, 131, 132, 129, 128, 123, 121, 116, 114, 109, 107, 104, 102, 100, 100,
    120, 127, 134, 141, 148, 152, 155, 156, 156, 154, 151, 148, 144, 141, 139, 138,
    135, 136, 133, 133, 128, 126, 121, 118, 113, 111, 108, 106, 103, 103, 102, 102,
    134, 140, 147, 151, 155, 155, 155, 154, 151, 150, 146, 144, 142, 142, 140, 141,
    138, 137, 133, 130, 126, 122, 117, 115, 112, 109, 106, 106, 104, 104, 104, 104,
    146, 150, 154, 154, 155, 154, 152, 151, 148, 147, 146, 146, 144, 145, 143, 142,
    138, 135, 129, 126, 120, 118, 115, 112, 109, 109, 107, 107, 106, 107, 106, 107,
    153, 154, 155, 154, 152, 152, 150, 150, 148, 150, 148, 149, 147, 146, 142, 139,
    133, 130, 124, 121, 118, 115, 111, 111, 109, 109, 109, 110, 109, 110, 109, 109,
    154, 154, 153, 153, 152, 152, 151, 153, 151, 152, 150, 150, 145, 143, 137, 133,
    127, 125, 120, 117, 114, 114, 112, 112, 111, 112, 111, 112, 112, 112, 110, 109,
    153, 154, 153, 154, 154, 156, 155, 156, 154, 154, 149, 146, 140, 136, 131, 128,
    123, 120, 116, 116, 114, 114, 114, 115, 114, 115, 114, 114, 112, 111, 109, 108,
    155, 156, 156, 159, 157, 159, 157, 157, 153, 150, 144, 140, 134, 130, 126, 122,
    119, 118, 116, 116, 116, 117, 116, 116, 116, 116, 114, 114, 111, 110, 108, 106,
    159, 161, 161, 163, 160, 160, 156, 153, 147, 143, 136, 133, 128, 125, 121, 121,
    118, 118, 118, 119, 118, 119, 118, 118, 116, 116, 113, 112, 110, 108, 106, 105
};
static const uint16_t coeff_pos_block_17_32_16[6] = {
    1, 34, 35, 64, 65, 66
};
static const int32_t coeff_val_block_17_32_16[6] = {
    -992, 992, -992, 992, 992, -992
};
static const uint8_t recon_first_block_17_32_16[8] = {
    132, 131, 136, 138, 143, 146, 150, 154
};
static const uint8_t recon_last_block_17_32_16[8] = {
    135, 121, 103, 89, 75, 63, 54, 49
};
static const uint8_t predictor_block_18_40_16[512] = {
    100, 86, 79, 75, 73, 70, 68, 65, 64, 62, 62, 62, 63, 64, 64, 65,
    66, 67, 67, 68, 69, 70, 70, 71, 71, 72, 72, 73, 73, 73, 74, 74,
    116, 100, 86, 79, 75, 73, 70, 68, 65, 64, 62, 62, 62, 63, 64, 64,
    65, 66, 67, 67, 68, 69, 70, 70, 71, 71, 72, 72, 73, 73, 73, 74,
    125, 116, 100, 86, 79, 75, 73, 70, 68, 65, 64, 62, 62, 62, 63, 64,
    64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 71, 72, 72, 73, 73, 73,
    134, 125, 116, 100, 86, 79, 75, 73, 70, 68, 65, 64, 62, 62, 62, 63,
    64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 71, 72, 72, 73, 73,
    140, 134, 125, 116, 100, 86, 79, 75, 73, 70, 68, 65, 64, 62, 62, 62,
    63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 71, 72, 72, 73,
    145, 140, 134, 125, 116, 100, 86, 79, 75, 73, 70, 68, 65, 64, 62, 62,
    62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 71, 72, 72,
    148, 145, 140, 134, 125, 116, 100, 86, 79, 75, 73, 70, 68, 65, 64, 62,
    62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 71, 72,
    148, 148, 145, 140, 134, 125, 116, 100, 86, 79, 75, 73, 70, 68, 65, 64,
    62, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 71,
    145, 148, 148, 145, 140, 134, 125, 116, 100, 86, 79, 75, 73, 70, 68, 65,
    64, 62, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71,
    138, 145, 148, 148, 145, 140, 134, 125, 116, 100, 86, 79, 75, 73, 70, 68,
    65, 64, 62, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70,
    128, 138, 145, 148, 148, 145, 140, 134, 125, 116, 100, 86, 79, 75, 73, 70,
    68, 65, 64, 62, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70,
    116, 128, 138, 145, 148, 148, 145, 140, 134, 125, 116, 100, 86, 79, 75, 73,
    70, 68, 65, 64, 62, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69,
    101, 116, 128, 138, 145, 148, 148, 145, 140, 134, 125, 116, 100, 86, 79, 75,
    73, 70, 68, 65, 64, 62, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68,
    86, 101, 116, 128, 138, 145, 148, 148, 145, 140, 134, 125, 116, 100, 86, 79,
    75, 73, 70, 68, 65, 64, 62, 62, 62, 63, 64, 64, 65, 66, 67, 67,
    72, 86, 101, 116, 128, 138, 145, 148, 148, 145, 140, 134, 125, 116, 100, 86,
    79, 75, 73, 70, 68, 65, 64, 62, 62, 62, 63, 64, 64, 65, 66, 67,
    60, 72, 86, 101, 116, 128, 138, 145, 148, 148, 145, 140, 134, 125, 116, 100,
    86, 79, 75, 73, 70, 68, 65, 64, 62, 62, 62, 63, 64, 64, 65, 66
};
static const uint16_t coeff_pos_block_18_40_16[7] = {
    0, 1, 32, 33, 66, 96, 160
};
static const int32_t coeff_val_block_18_40_16[7] = {
    1984, -992, -992, 992, -992, -992, 992
};
static const uint8_t recon_first_block_18_40_16[8] = {
    90, 77, 71, 70, 70, 71, 72, 73
};
static const uint8_t recon_last_block_18_40_16[8] = {
    126, 124, 122, 122, 121, 120, 120, 120
};
static const uint8_t predictor_block_19_32_20[1024] = {
    166, 166, 165, 164, 163, 163, 161, 160, 160, 159, 158, 157, 157, 156, 155, 155,
    154, 153, 152, 152, 151, 150, 149, 149, 148, 147, 147, 146, 145, 144, 144, 143,
    142, 142, 141, 140, 139, 139, 138, 137, 137, 136, 135, 134, 134, 133, 132, 132,
    131, 130, 129, 129, 128, 127, 126, 126, 125, 124, 124, 123, 122, 121, 121, 120,
    173, 172, 172, 171, 170, 169, 167, 166, 166, 165, 164, 163, 162, 161, 161, 160,
    159, 158, 157, 156, 156, 155, 154, 153, 152, 151, 151, 150, 149, 148, 147, 147,
    146, 145, 144, 143, 142, 142, 141, 140, 139, 138, 137, 137, 136, 135, 134, 133,
    132, 132, 131, 130, 129, 128, 127, 127, 126, 125, 124, 123, 122, 122, 121, 120,
    174, 173, 172, 172, 171, 170, 168, 167, 166, 166, 165, 164, 163, 162, 161, 161,
    160, 159, 158, 157, 156, 155, 155, 154, 153, 152, 151, 150, 150, 149, 148, 147,
    146, 145, 144, 144, 143, 142, 141, 140, 139, 139, 138, 137, 136, 135, 134, 134,
    133, 132, 131, 130, 129, 128, 128, 127, 126, 125, 124, 123, 123, 122, 121, 120,
    167, 167, 166, 165, 164, 164, 162, 161, 160, 160, 159, 158, 157, 157, 156, 155,
    155, 154, 153, 152, 152, 151, 150, 149, 149, 148, 147, 146, 146, 145, 144, 144,
    143, 142, 141, 141, 140, 139, 138, 138, 137, 136, 135, 135, 134, 133, 132, 132,
    131, 130, 130, 129, 128, 127, 127, 126, 125, 124, 124, 123, 122, 121, 121, 120,
    156, 156, 155, 155, 153, 153, 152, 152, 151, 150, 150, 149, 149, 148, 148, 147,
    146, 146, 145, 145, 144, 144, 143, 143, 142, 141, 141, 140, 140, 139, 139, 138,
    137, 137, 136, 136, 135, 135, 134, 134, 133, 132, 132, 131, 131, 130, 130, 129,
    128, 128, 127, 127, 126, 126, 125, 125, 124, 123, 123, 122, 122, 121, 121, 120,
    144, 144, 143, 143, 142, 142, 141, 141, 141, 140, 140, 140, 139, 139, 138, 138,
    138, 137, 137, 137, 136, 136, 135, 135, 135, 134, 134, 134, 133, 133, 132, 132,
    132, 131, 131, 131, 130, 130, 129, 129, 129, 128, 128, 128, 127, 127, 126, 126,
    126, 125, 125, 125, 124, 124, 123, 123, 123, 122, 122, 122, 121, 121, 120, 120,
    134, 134, 133, 133, 133, 133, 132, 132, 132, 132, 132, 131, 131, 131, 131, 131,
    130, 130, 130, 130, 129, 129, 129, 129, 129, 128, 128, 128, 128, 127, 127, 127,
    127, 127, 126, 126, 126, 126, 125, 125, 125, 125, 125, 124, 124, 124, 124, 124,
    123, 123, 123, 123, 122, 122, 122, 122, 122, 121, 121, 121, 121, 120, 120, 120,
    130, 130, 130, 129, 129, 129, 129, 129, 129, 128, 128, 128, 128, 128, 128, 128,
    127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 125, 125, 125, 125,
    125, 125, 125, 124, 124, 124, 124, 124, 124, 123, 123, 123, 123, 123, 123, 123,
    122, 122, 122, 122, 122, 122, 121, 121, 121, 121, 121, 121, 120, 120, 120, 120,
    131, 131, 130, 130, 130, 130, 130, 130, 129, 129, 129, 129, 129, 129, 128, 128,
    128, 128, 128, 128, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126,
    125, 125, 125, 125, 125, 124, 124, 124, 124, 124, 124, 123, 123, 123, 123, 123,
    123, 122, 122, 122, 122, 122, 122, 121, 121, 121, 121, 121, 121, 120, 120, 120,
    135, 135, 134, 134, 134, 134, 133, 133, 133, 133, 132, 132, 132, 132, 131, 131,
    131, 131, 131, 130, 130, 130, 130, 129, 129, 129, 129, 128, 128, 128, 128, 128,
    127, 127, 127, 127, 126, 126, 126, 126, 125, 125, 125, 125, 124, 124, 124, 124,
    124, 123, 123, 123, 123, 122, 122, 122, 122, 121, 121, 121, 121, 120, 120, 120,
    139, 139, 138, 138, 138, 137, 137, 137, 136, 136, 136, 135, 135, 135, 135, 134,
    134, 134, 133, 133, 133, 132, 132, 132, 132, 131, 131, 131, 130, 130, 130, 130,
    129, 129, 129, 128, 128, 128, 127, 127, 127, 127, 126, 126, 126, 125, 125, 125,
    124, 124, 124, 124, 123, 123, 123, 122, 122, 122, 121, 121, 121, 121, 120, 120,
    140, 140, 139, 139, 138, 138, 138, 138, 137, 137, 137, 136, 136, 136, 135, 135,
    135, 134, 134, 134, 133, 133, 133, 133, 132, 132, 132, 131, 131, 131, 130, 130,
    130, 129, 129, 129, 128, 128, 128, 128, 127, 127, 127, 126, 126, 126, 125, 125,
    125, 124, 124, 124, 123, 123, 123, 123, 122, 122, 122, 121, 121, 121, 120, 120,
    138, 138, 137, 137, 137, 136, 136, 136, 135, 135, 135, 135, 134, 134, 134, 134,
    133, 133, 133, 132, 132, 132, 132, 131, 131, 131, 130, 130, 130, 130, 129, 129,
    129, 128, 128, 128, 128, 127, 127, 127, 126, 126, 126, 126, 125, 125, 125, 125,
    124, 124, 124, 123, 123, 123, 123, 122, 122, 122, 121, 121, 121, 121, 120, 120,
    132, 132, 131, 131, 131, 131, 131, 131, 130, 130, 130, 130, 130, 129, 129, 129,
    129, 129, 128, 128, 128, 128, 128, 128, 127, 127, 127, 127, 127, 126, 126, 126,
    126, 126, 125, 125, 125, 125, 125, 125, 124, 124, 124, 124, 124, 123, 123, 123,
    123, 123, 122, 122, 122, 122, 122, 122, 121, 121, 121, 121, 121, 120, 120, 120,
    126, 126, 126, 126, 126, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
    124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 123, 123, 123, 123, 123, 123,
    123, 123, 123, 123, 123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
    121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 120, 120, 120, 120, 120, 120,
    122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
    121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121,
    121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121,
    120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120
};
static const uint16_t coeff_pos_block_19_32_20[8] = {
    1, 32, 33, 66, 103, 129, 132, 162
};
static const int32_t coeff_val_block_19_32_20[8] = {
    992, -1984, -992, -992, 992, 992, -992, 992
};
static const uint8_t recon_first_block_19_32_20[8] = {
    152, 152, 147, 146, 139, 139, 133, 132
};
static const uint8_t recon_last_block_19_32_20[8] = {
    67, 67, 67, 67, 70, 70, 73, 73
};

typedef struct {
    const char *name;
    uint8_t tx_size;
    Av2hFirstTxType tx_type;
    uint8_t all_zero;
    uint16_t eob;
    uint16_t output_width;
    uint16_t output_height;
    uint16_t coeff_width;
    uint16_t coeff_height;
    uint16_t nonzero_count;
    const uint16_t *coeff_positions;
    const int32_t *coeff_values;
    uint16_t coeff_used;
    const uint8_t *predictor;
    uint32_t recon_hash;
    const uint8_t *recon_first;
    const uint8_t *recon_last;
} Av2hEmeraldCase;

static const Av2hEmeraldCase emerald_cases[] = {
    { "block_05_0_14 (TX_32X8 DC_PRED ADST_DCT)", 16, AV2H_TX_ADST_DCT, 0, 2, 32, 8, 32, 8, 2,
      coeff_pos_block_05_0_14, coeff_val_block_05_0_14, 2, predictor_block_05_0_14, 0x82fa6505u, recon_first_block_05_0_14, recon_last_block_05_0_14 },
    { "block_06_8_8 (TX_32X32 SMOOTH_PRED DCT_DCT)", 3, AV2H_TX_DCT_DCT, 0, 19, 32, 32, 32, 32, 11,
      coeff_pos_block_06_8_8, coeff_val_block_06_8_8, 11, predictor_block_06_8_8, 0xbb0ab043u, recon_first_block_06_8_8, recon_last_block_06_8_8 },
    { "block_07_16_0 (TX_64X32 DC_PRED DCT_DCT)", 12, AV2H_TX_DCT_DCT, 0, 6, 64, 32, 32, 32, 3,
      coeff_pos_block_07_16_0, coeff_val_block_07_16_0, 3, predictor_block_07_16_0, 0x83a65d05u, recon_first_block_07_16_0, recon_last_block_07_16_0 },
    { "block_08_16_8 (TX_64X16 DC_PRED DCT_DCT)", 18, AV2H_TX_DCT_DCT, 0, 5, 64, 16, 32, 16, 4,
      coeff_pos_block_08_16_8, coeff_val_block_08_16_8, 4, predictor_block_08_16_8, 0xe133f5b9u, recon_first_block_08_16_8, recon_last_block_08_16_8 },
    { "block_09_16_12 (TX_16X16 D113_PRED DCT_DCT)", 2, AV2H_TX_DCT_DCT, 1, 0, 16, 16, 16, 16, 0,
      0, 0, 0, predictor_block_09_16_12, 0x7fab3a56u, recon_first_block_09_16_12, recon_last_block_09_16_12 },
    { "block_10_20_12 (TX_16X16 SMOOTH_PRED ADST_ADST)", 2, AV2H_TX_ADST_ADST, 0, 9, 16, 16, 16, 16, 4,
      coeff_pos_block_10_20_12, coeff_val_block_10_20_12, 4, predictor_block_10_20_12, 0x5bc9af2du, recon_first_block_10_20_12, recon_last_block_10_20_12 },
    { "block_11_24_12 (TX_32X16 H_PRED DCT_DCT)", 10, AV2H_TX_DCT_DCT, 0, 18, 32, 16, 32, 16, 9,
      coeff_pos_block_11_24_12, coeff_val_block_11_24_12, 9, predictor_block_11_24_12, 0xcf9a30fbu, recon_first_block_11_24_12, recon_last_block_11_24_12 },
    { "block_12_32_0 (TX_16X64 D157_PRED DCT_DCT)", 17, AV2H_TX_DCT_DCT, 0, 4, 16, 64, 16, 32, 2,
      coeff_pos_block_12_32_0, coeff_val_block_12_32_0, 2, predictor_block_12_32_0, 0xfe18403au, recon_first_block_12_32_0, recon_last_block_12_32_0 },
    { "block_13_36_0 (TX_16X64 D135_PRED DCT_DCT)", 17, AV2H_TX_DCT_DCT, 1, 0, 16, 64, 16, 32, 0,
      0, 0, 0, predictor_block_13_36_0, 0x5ab6d6cfu, recon_first_block_13_36_0, recon_last_block_13_36_0 },
    { "block_14_40_0 (TX_32X64 DC_PRED DCT_DCT)", 11, AV2H_TX_DCT_DCT, 0, 4, 32, 64, 32, 32, 3,
      coeff_pos_block_14_40_0, coeff_val_block_14_40_0, 3, predictor_block_14_40_0, 0x02dfd214u, recon_first_block_14_40_0, recon_last_block_14_40_0 },
    { "block_15_0_16 (TX_64X32 H_PRED DCT_DCT)", 12, AV2H_TX_DCT_DCT, 0, 58, 64, 32, 32, 32, 18,
      coeff_pos_block_15_0_16, coeff_val_block_15_0_16, 18, predictor_block_15_0_16, 0xe8ce8b65u, recon_first_block_15_0_16, recon_last_block_15_0_16 },
    { "block_16_16_16 (TX_64X32 D203_PRED DCT_DCT)", 12, AV2H_TX_DCT_DCT, 0, 48, 64, 32, 32, 32, 12,
      coeff_pos_block_16_16_16, coeff_val_block_16_16_16, 12, predictor_block_16_16_16, 0xce052797u, recon_first_block_16_16_16, recon_last_block_16_16_16 },
    { "block_17_32_16 (TX_32X16 D203_PRED DCT_DCT)", 10, AV2H_TX_DCT_DCT, 0, 14, 32, 16, 32, 16, 6,
      coeff_pos_block_17_32_16, coeff_val_block_17_32_16, 6, predictor_block_17_32_16, 0x08dbda35u, recon_first_block_17_32_16, recon_last_block_17_32_16 },
    { "block_18_40_16 (TX_32X16 D135_PRED DCT_DCT)", 10, AV2H_TX_DCT_DCT, 0, 16, 32, 16, 32, 16, 7,
      coeff_pos_block_18_40_16, coeff_val_block_18_40_16, 7, predictor_block_18_40_16, 0x06610d3au, recon_first_block_18_40_16, recon_last_block_18_40_16 },
    { "block_19_32_20 (TX_64X16 SMOOTH_H_PRED DCT_DCT)", 18, AV2H_TX_DCT_DCT, 0, 63, 64, 16, 32, 16, 8,
      coeff_pos_block_19_32_20, coeff_val_block_19_32_20, 8, predictor_block_19_32_20, 0x3ce2f851u, recon_first_block_19_32_20, recon_last_block_19_32_20 },
};

static void test_emerald_q208_luma_blocks(void) {
    static Av2hReconstructionScratch scratch;
    static Av2hFirstReconstruction reconstruction;
    size_t case_index;
    for (case_index = 0U;
         case_index < sizeof(emerald_cases) / sizeof(emerald_cases[0]);
         ++case_index) {
        const Av2hEmeraldCase *test_case = &emerald_cases[case_index];
        Av2hFirstLeafMode mode = { 0 };
        Av2hFirstCoeffPrefix prefix = { 0 };
        Av2hFirstTransformType transform = { 0 };
        Av2hFirstCoeffBlock block = { 0 };
        size_t sample_count =
            (size_t)test_case->output_width * test_case->output_height;
        uint16_t coeff_index;
        size_t index;
        Av2hStatus status;
        prefix.tx_size = (Av2hFirstTxSize)test_case->tx_size;
        prefix.all_zero = test_case->all_zero;
        prefix.eob = test_case->eob;
        transform.primary_type = test_case->tx_type;
        block.width = test_case->coeff_width;
        block.height = test_case->coeff_height;
        block.coefficient_count =
            (uint16_t)((uint32_t)test_case->coeff_width *
                       test_case->coeff_height);
        block.nonzero_count = test_case->nonzero_count;
        for (coeff_index = 0U; coeff_index < test_case->coeff_used;
             ++coeff_index) {
            block.dequantized[test_case->coeff_positions[coeff_index]] =
                test_case->coeff_values[coeff_index];
        }
        status = av2h_luma_reconstruct(
            &mode, &prefix, &transform, &block, test_case->predictor,
            test_case->output_width, &scratch, &reconstruction);
        if (status != AV2H_OK) {
            fprintf(stderr, "%s: reconstruct status %d\n",
                    test_case->name, status);
            ++failures;
            continue;
        }
        CHECK(reconstruction.width == test_case->output_width);
        CHECK(reconstruction.height == test_case->output_height);
        if (trace_sample_hash(reconstruction.pixels, sample_count) !=
            test_case->recon_hash) {
            fprintf(stderr, "%s: reconstruction hash mismatch\n",
                    test_case->name);
            ++failures;
        }
        for (index = 0U; index < 8U; ++index) {
            CHECK(reconstruction.pixels[index] ==
                  test_case->recon_first[index]);
            CHECK(reconstruction.pixels[sample_count - 8U + index] ==
                  test_case->recon_last[index]);
        }
        for (index = 0U; index < sample_count; ++index) {
            CHECK(reconstruction.pixels[index] <= 255U);
        }
    }
}


static void test_emerald_reconstruct_clipping(void) {
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    static Av2hReconstructionScratch scratch;
    static Av2hFirstReconstruction reconstruction;
    uint8_t predictor[256U];
    size_t index;

    prefix.tx_size = AV2H_TX_16X16;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 16U;
    block.height = 16U;
    block.coefficient_count = 256U;
    block.nonzero_count = 1U;

    /* A large positive DC residual saturates every sample to the maximum. */
    av2h_memory_fill(predictor, 240U, sizeof(predictor));
    block.dequantized[0U] = 32000;
    CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                predictor, 16U, &scratch,
                                &reconstruction) == AV2H_OK);
    for (index = 0U; index < 256U; ++index) {
        CHECK(reconstruction.pixels[index] == 255U);
    }

    /* A large negative DC residual saturates every sample to the minimum. */
    av2h_memory_fill(predictor, 12U, sizeof(predictor));
    block.dequantized[0U] = -32000;
    CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                predictor, 16U, &scratch,
                                &reconstruction) == AV2H_OK);
    for (index = 0U; index < 256U; ++index) {
        CHECK(reconstruction.pixels[index] == 0U);
    }
}

static void test_emerald_reconstruct_invalid_preserves_output(void) {
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    static Av2hReconstructionScratch scratch;
    static Av2hFirstReconstruction reconstruction;
    uint8_t predictor[256U];
    size_t index;

    av2h_memory_fill(predictor, 100U, sizeof(predictor));
    for (index = 0U; index < AV2H_RECON_SAMPLE_CAPACITY; ++index) {
        reconstruction.pixels[index] = 0xABU;
    }

    /* An all-zero block that still carries a coefficient is rejected and the
     * caller's reconstruction buffer is left untouched. */
    prefix.tx_size = AV2H_TX_16X16;
    prefix.all_zero = 1U;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 16U;
    block.height = 16U;
    block.coefficient_count = 256U;
    block.nonzero_count = 0U;
    block.dequantized[5U] = 42;
    CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                predictor, 16U, &scratch,
                                &reconstruction) == AV2H_UNSUPPORTED);
    for (index = 0U; index < 256U; ++index) {
        CHECK(reconstruction.pixels[index] == 0xABU);
    }

    /* A predictor stride narrower than the output width is rejected without
     * writing any output. */
    block.dequantized[5U] = 0;
    prefix.all_zero = 0U;
    block.nonzero_count = 1U;
    block.dequantized[0U] = 2048;
    CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                predictor, 8U, &scratch,
                                &reconstruction) == AV2H_INVALID_ARGUMENT);
    for (index = 0U; index < 256U; ++index) {
        CHECK(reconstruction.pixels[index] == 0xABU);
    }
}


static const uint16_t l_00_00_u_cp[1]={0};
static const int32_t l_00_00_u_cv[1]={1984};
static const uint16_t l_00_00_v_cp[1]={0};
static const int32_t l_00_00_v_cv[1]={-992};
static const uint8_t l_08_00_u_above[32]={150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150};
static const uint16_t l_08_00_u_cp[5]={0,1,32,64,65};
static const int32_t l_08_00_u_cv[5]={-1984,992,992,992,-992};
static const uint8_t l_08_00_v_above[32]={117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117};
static const uint16_t l_08_00_v_cp[2]={0,32};
static const int32_t l_08_00_v_cv[2]={992,-992};
static const uint8_t l_00_16_u_left[16]={150,150,150,150,150,150,150,150,150,150,150,150,150,150,150,150};
static const uint16_t l_00_16_u_cp[1]={0};
static const int32_t l_00_16_u_cv[1]={-992};
static const uint8_t l_00_16_v_left[16]={117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117};
static const uint8_t l_08_16_u_above[32]={139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139};
static const uint8_t l_08_16_u_left[4]={165,158,147,132};
static const uint8_t l_08_16_v_above[32]={117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117};
static const uint8_t l_08_16_v_left[4]={113,113,114,116};
static const uint8_t l_10_16_u_above[16]={136,137,137,138,138,139,139,140,140,140,140,140,140,140,140,140};
static const uint8_t l_10_16_u_left[8]={115,99,86,78,74,77,85,96};
static const uint8_t l_10_16_v_above[16]={117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117};
static const uint8_t l_10_16_v_left[8]={118,121,124,126,129,132,135,138};
static const uint8_t l_10_24_u_above[16]={140,140,140,140,140,140,140,140,140,140,140,140,140,140,140,140};
static const uint8_t l_10_24_u_left[8]={115,99,86,78,74,77,85,96};
static const uint8_t l_10_24_v_above[16]={117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117};
static const uint8_t l_10_24_v_left[8]={118,121,124,126,129,132,135,138};
static const uint8_t l_14_16_u_above[32]={96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,112,115,118,120,123,123,123,123,123,123,123,123,123,123,123,123};
static const uint8_t l_14_16_u_left[4]={108,120,129,134};
static const uint8_t l_14_16_v_above[32]={138,138,138,138,138,138,138,138,138,138,138,138,138,138,138,138,128,126,124,123,121,121,121,121,121,121,121,121,121,121,121,121};
static const uint8_t l_14_16_v_left[4]={140,142,143,143};
static const uint8_t l_16_00_u_above[32]={122,122,122,122,122,123,123,123,124,124,125,125,126,126,127,128,128,129,129,130,131,131,132,132,132,133,133,133,134,134,134,134};
static const uint16_t l_16_00_u_cp[1]={0};
static const int32_t l_16_00_u_cv[1]={-992};
static const uint8_t l_16_00_v_above[32]={143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143};
static const uint8_t l_16_16_u_above[32]={121,119,118,117,115,114,113,111,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110};
static const uint8_t l_16_16_u_left[16]={111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111};
static const uint8_t l_16_16_v_above[32]={136,136,135,134,134,133,132,132,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131};
static const uint8_t l_16_16_v_left[16]={143,143,143,143,143,143,143,143,143,143,143,143,143,143,143,143};
static const uint8_t l_16_32_u_above[32]={114,114,115,115,116,117,118,119,118,115,112,109,110,113,117,121,125,125,126,127,127,127,127,126,126,126,126,125,125,125,125,125};
static const uint8_t l_16_32_u_left[16]={111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111};
static const uint8_t l_16_32_v_above[32]={124,124,123,123,123,122,122,122,122,123,125,125,125,124,123,121,120,120,119,119,119,119,119,119,119,119,119,119,120,120,120,120};
static const uint8_t l_16_32_v_left[16]={133,134,134,135,135,135,135,135,135,135,135,135,135,135,135,135};
static const uint16_t l_16_32_v_cp[1]={0};
static const int32_t l_16_32_v_cv[1]={992};

static const uint8_t cfl_luma[4224]={57,57,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,61,61,61,61,61,61,61,60,61,62,62,63,64,64,65,66,67,67,68,69,70,70,71,71,72,72,73,73,73,74,74,74,57,57,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,61,61,61,61,61,61,61,60,61,62,62,63,64,64,65,66,67,67,68,69,70,70,71,71,72,72,73,73,73,74,74,74,57,57,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,61,61,60,61,61,60,61,60,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,73,73,73,73,74,74,57,57,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,61,61,60,61,61,60,61,60,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,73,73,73,73,74,74,57,57,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,61,61,60,60,60,60,60,60,60,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,72,73,73,73,73,57,57,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,60,60,60,60,60,60,60,60,60,60,60,60,60,60,61,61,60,60,60,60,60,60,60,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,72,73,73,73,73,57,57,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,59,59,59,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,59,59,59,59,59,59,59,60,60,61,62,63,63,64,65,66,66,67,68,69,69,70,70,71,71,72,72,72,72,72,57,57,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,58,59,59,59,60,60,60,60,60,60,60,60,60,60,60,60,60,59,59,59,59,59,59,59,60,60,61,62,63,63,64,65,66,66,67,68,69,69,70,70,71,71,72,72,72,72,72,57,57,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,57,57,58,59,59,59,60,60,60,60,60,60,60,60,60,60,59,59,58,58,58,58,58,58,58,59,59,60,61,61,62,63,64,64,65,66,67,67,68,69,69,70,70,70,71,71,71,71,57,57,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,57,57,57,58,59,59,59,60,60,60,60,60,60,60,60,60,59,59,58,58,58,58,58,58,58,59,59,60,61,61,62,63,64,64,65,66,67,67,68,69,69,70,70,70,71,71,71,71,57,57,54,54,54,54,54,54,54,54,54,54,54,54,54,54,54,54,56,56,57,57,57,58,59,59,59,60,60,60,60,60,60,60,57,57,57,57,57,56,57,56,57,58,58,59,60,60,61,62,63,63,64,65,66,66,67,67,68,68,69,69,69,70,70,70,57,57,54,54,54,54,54,54,54,54,54,54,54,54,54,54,54,54,55,55,56,57,57,57,58,59,59,59,60,60,60,60,60,60,57,57,57,57,57,56,57,56,57,58,58,59,60,60,61,62,63,63,64,65,66,66,67,67,68,68,69,69,69,70,70,70,57,57,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,54,54,55,56,57,57,57,57,58,59,59,59,60,60,60,60,56,56,55,55,56,55,55,55,55,56,57,57,58,59,60,60,61,62,63,63,64,65,65,66,66,67,67,68,68,68,68,68,57,57,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,53,53,54,55,55,56,57,57,57,58,59,59,59,60,60,60,56,56,55,55,56,55,55,55,55,56,57,57,58,59,60,60,61,62,63,63,64,65,65,66,66,67,67,68,68,68,68,68,57,57,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,52,52,53,54,54,55,56,57,57,57,57,58,59,59,59,60,55,55,54,54,54,54,54,54,54,55,55,56,57,57,58,59,60,60,61,62,63,63,64,64,65,66,66,66,67,67,67,67,57,57,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,51,51,52,53,53,54,55,56,56,57,57,57,58,59,59,59,55,54,54,54,53,54,54,54,54,55,55,56,57,57,58,59,60,60,61,62,63,63,64,64,65,66,66,66,67,67,67,67,57,57,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,50,50,51,52,52,53,54,55,55,56,57,57,57,58,58,59,53,52,52,52,52,52,52,52,52,53,54,54,55,56,57,57,58,59,60,60,61,62,62,63,63,64,64,65,65,65,65,65,57,57,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,49,49,50,51,51,52,53,54,54,55,56,56,57,57,57,58,52,52,52,51,52,52,52,52,52,53,54,54,55,56,57,57,58,59,60,60,61,62,62,63,63,64,64,65,65,65,65,65,57,57,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,48,48,49,50,50,51,52,53,53,54,55,55,56,57,57,57,51,51,50,50,50,51,50,51,51,52,52,53,54,54,55,56,57,57,58,59,60,60,61,61,62,63,63,63,64,64,64,64,57,57,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,47,47,48,49,49,50,51,52,52,53,54,54,55,56,56,57,51,51,50,50,50,51,50,51,51,52,52,53,54,54,55,56,57,57,58,59,60,60,61,61,62,63,63,63,64,64,64,64,57,57,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,46,46,47,48,48,49,50,51,51,52,53,53,54,55,55,56,48,48,48,49,49,48,49,49,50,50,51,51,52,53,54,54,55,56,57,57,58,59,59,60,61,61,61,62,62,62,63,63,57,57,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,45,45,46,47,47,48,49,50,50,51,52,52,53,54,54,55,48,48,48,49,48,48,49,49,50,50,51,51,52,53,54,54,55,56,57,57,58,59,59,60,61,61,61,62,62,62,63,63,57,57,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,44,44,45,46,46,47,48,49,49,50,51,51,52,53,53,54,46,46,46,46,47,47,47,48,48,49,50,50,51,52,52,53,54,55,55,56,57,58,58,59,59,60,60,61,61,61,61,61,57,57,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,43,44,45,45,46,47,48,48,49,50,50,51,52,52,53,46,46,46,46,47,47,47,48,48,49,50,50,51,52,52,53,54,55,55,56,57,58,58,59,59,60,60,61,61,61,61,61,57,57,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,42,42,43,44,44,45,46,47,47,48,49,49,50,51,51,52,45,45,45,45,45,46,46,47,47,48,48,49,50,51,51,52,53,54,54,55,56,57,57,58,58,59,59,60,60,60,60,60,57,57,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,42,43,43,44,45,46,46,47,48,48,49,50,50,51,44,44,44,44,45,46,46,47,47,48,48,49,50,51,51,52,53,54,54,55,56,57,57,58,58,59,59,60,60,60,60,60,57,57,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,41,41,41,42,42,43,44,45,45,46,47,47,48,49,49,50,43,43,43,43,44,45,45,46,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,59,59,59,59,59,57,57,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,41,41,41,42,43,44,44,45,46,46,47,48,48,49,42,42,43,43,44,44,45,46,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,59,59,59,59,59,57,57,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,40,40,40,41,41,41,42,43,43,44,45,45,46,47,47,48,41,41,41,43,43,43,44,45,46,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,58,59,59,59,57,57,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,40,40,40,41,41,41,42,43,44,44,45,46,46,47,40,41,41,42,43,43,44,45,46,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,58,59,59,59,57,57,39,39,38,38,38,38,38,38,38,38,38,38,38,38,38,38,39,39,39,40,40,40,41,41,41,42,43,43,44,45,45,46,40,40,41,41,42,43,44,45,46,46,47,47,48,49,50,50,51,52,53,53,54,55,55,56,57,57,57,58,58,58,58,58,57,57,43,42,41,40,39,39,38,38,38,38,38,38,38,38,38,38,38,38,39,39,39,40,40,41,41,41,41,42,43,44,44,45,39,40,40,41,42,43,44,45,46,46,47,47,48,49,50,50,51,52,53,53,54,55,55,56,57,57,57,58,58,58,58,58,73,73,47,46,45,44,43,42,41,40,39,39,38,38,38,38,38,38,38,38,38,39,39,39,40,40,40,41,41,41,42,43,43,44,39,39,40,40,42,43,44,45,46,46,47,47,48,49,50,50,51,52,53,53,54,55,55,56,57,57,57,58,58,58,58,58,73,73,50,50,49,48,47,46,45,44,43,42,41,40,39,39,38,38,38,38,38,38,38,39,39,40,40,40,41,41,41,42,42,43,39,39,40,40,42,42,44,44,46,46,47,47,48,49,50,50,51,52,53,53,54,55,55,56,57,57,57,58,58,58,58,58,72,72,53,53,53,52,51,51,50,49,48,47,46,45,44,43,42,41,41,39,38,38,38,38,39,39,39,40,40,40,41,41,41,42,38,39,39,41,42,42,44,44,46,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,58,59,59,59,70,70,52,52,53,53,53,53,53,52,51,51,50,49,48,47,46,45,43,41,40,38,38,38,38,39,39,39,40,40,40,41,41,41,38,38,39,41,41,42,44,44,46,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,58,59,59,59,67,67,50,51,51,52,53,53,54,54,54,54,54,53,52,52,51,50,46,44,42,40,39,38,38,38,38,39,39,39,40,40,40,41,39,39,40,41,42,43,44,45,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,59,59,59,59,59,62,62,47,48,48,49,50,51,51,52,53,53,54,54,54,54,54,53,50,48,45,43,41,40,38,38,38,38,39,39,39,40,40,40,38,39,40,40,42,43,44,45,46,47,48,48,49,50,51,51,52,53,54,54,55,56,56,57,57,58,58,59,59,59,59,59,58,58,44,45,46,47,48,49,49,50,51,52,52,53,54,54,55,55,53,51,49,46,44,42,40,39,38,38,38,38,39,39,39,40,39,40,41,41,42,44,45,46,47,48,48,49,50,51,51,52,53,54,54,55,56,57,57,58,58,59,59,60,60,60,60,60,56,56,41,42,42,43,44,45,46,47,48,49,49,50,51,52,52,53,53,53,52,50,47,45,42,41,40,38,38,38,38,39,39,39,39,40,40,41,42,44,44,46,47,48,48,49,50,51,51,52,53,54,54,55,56,57,57,58,58,59,59,60,60,60,60,60,55,55,41,41,42,42,42,43,43,44,45,46,47,48,49,50,50,51,52,53,53,53,51,49,46,43,42,40,39,38,38,38,38,39,40,41,41,42,44,45,45,47,48,49,50,50,51,52,52,53,54,55,55,56,57,58,58,59,59,60,60,61,61,61,61,61,56,56,42,41,41,41,41,41,42,42,42,43,43,44,45,46,47,48,50,52,52,53,53,52,50,47,44,42,41,39,38,38,38,38,39,40,41,42,44,45,45,47,48,49,50,50,51,52,52,53,54,55,55,56,57,58,58,59,59,60,60,61,61,61,61,61,58,58,45,45,44,44,44,43,43,43,43,43,44,44,44,45,45,46,48,49,51,52,53,53,52,51,48,45,43,41,40,39,38,38,40,41,42,44,45,46,47,48,50,50,51,51,52,53,54,54,55,56,57,57,58,59,59,60,61,61,61,62,62,62,63,63,61,61,48,47,47,46,45,45,44,44,44,43,43,43,43,43,44,44,47,47,48,50,52,52,53,53,52,49,47,44,42,41,39,38,40,41,42,44,45,46,47,48,50,50,51,51,52,53,54,54,55,56,57,57,58,59,59,60,61,61,61,62,62,62,63,63,65,65,53,53,52,51,50,49,49,48,47,47,46,46,46,45,45,45,46,46,47,48,49,51,52,53,53,52,51,48,45,43,41,40,43,44,45,45,46,48,49,50,51,52,52,53,54,54,55,56,57,57,58,59,60,60,61,61,62,63,63,63,64,64,64,64,68,68,56,55,55,54,53,53,52,51,50,49,49,48,47,47,46,46,46,46,46,47,47,48,50,52,52,53,53,51,49,46,44,42,44,45,45,46,47,48,49,50,51,52,52,53,54,54,55,56,57,57,58,59,60,60,61,61,62,63,63,63,64,64,64,64,71,71,63,61,60,59,58,57,57,56,55,55,54,53,52,51,51,50,48,46,46,46,46,47,48,50,51,52,53,53,52,50,48,45,46,47,47,48,49,50,50,52,52,53,54,54,55,56,57,57,58,59,60,60,61,62,62,63,63,64,64,65,65,65,65,65,72,72,73,70,67,65,63,61,60,59,58,57,57,56,55,55,54,53,51,49,47,46,46,46,47,47,49,51,52,52,53,53,51,49,48,48,49,49,50,50,51,52,52,53,54,54,55,56,57,57,58,59,60,60,61,62,62,63,63,64,64,65,65,65,65,65,107,106,91,86,82,79,75,72,69,67,65,63,62,61,60,59,59,58,55,52,50,48,46,46,46,46,47,48,50,51,52,53,53,52,52,52,52,52,52,53,53,54,54,55,55,56,57,57,58,59,60,60,61,62,63,63,64,64,65,66,66,66,67,67,67,67,123,123,109,104,99,95,91,86,82,79,75,72,69,67,65,63,62,61,60,56,54,51,48,47,46,46,46,47,47,49,51,52,52,53,52,52,52,52,52,53,53,54,54,55,55,56,57,57,58,59,60,60,61,62,63,63,64,64,65,66,66,66,67,67,67,67,139,140,132,127,121,116,111,106,101,97,93,88,84,81,77,74,71,69,66,61,58,55,52,50,47,46,46,46,46,47,48,50,51,52,53,53,53,53,54,54,54,55,55,56,57,57,58,59,60,60,61,62,63,63,64,65,65,66,66,67,67,68,68,68,68,68,160,160,152,147,142,137,132,127,121,116,111,106,101,97,93,88,84,81,75,68,64,59,56,54,51,48,47,46,46,46,47,47,49,51,52,52,52,52,54,54,54,55,55,56,57,57,58,59,60,60,61,62,63,63,64,65,65,66,66,67,67,68,68,68,68,68,183,184,171,167,163,158,154,149,144,139,134,129,123,118,113,108,103,99,89,79,71,65,61,57,55,52,49,47,46,46,46,47,47,48,52,52,53,54,54,54,55,56,57,58,58,59,60,60,61,62,63,63,64,65,66,66,67,67,68,68,69,69,69,70,70,70,196,197,180,179,177,174,171,167,163,158,154,149,144,139,134,129,123,118,107,94,84,75,68,63,59,56,53,50,48,47,46,46,46,47,51,52,53,53,54,54,55,56,57,58,58,59,60,60,61,62,63,63,64,65,66,66,67,67,68,68,69,69,69,70,70,70,189,190,178,181,182,182,182,181,179,176,173,169,165,160,156,151,146,141,126,113,100,88,79,71,65,61,57,54,52,49,47,46,46,46,53,53,54,54,54,56,56,58,58,59,59,60,61,61,62,63,64,64,65,66,67,67,68,69,69,70,70,70,71,71,71,71,159,159,159,165,170,174,178,181,182,182,182,181,179,176,173,169,165,160,145,132,119,105,93,83,74,67,63,59,56,53,50,48,46,46,53,53,54,54,54,56,56,58,58,59,59,60,61,61,62,63,64,64,65,66,67,67,68,69,69,70,70,70,71,71,71,71,122,121,136,142,149,155,161,167,172,176,180,183,184,184,184,183,181,178,163,152,138,125,111,99,88,78,70,65,60,57,54,51,49,47,54,55,55,55,56,57,57,59,59,60,60,61,62,63,63,64,65,66,66,67,68,69,69,70,70,71,71,72,72,72,72,72,95,94,113,118,124,130,136,142,149,155,161,167,172,176,180,183,184,184,173,167,157,144,131,118,104,92,82,73,67,62,58,55,53,50,56,56,56,56,56,58,58,59,59,60,60,61,62,63,63,64,65,66,66,67,68,69,69,70,70,71,71,72,72,72,72,72,92,90,105,106,108,111,114,119,125,131,137,143,150,156,162,168,173,177,174,174,170,162,150,137,124,110,98,87,77,70,64,60,57,54,58,58,58,58,58,59,59,60,60,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,72,73,73,73,73,108,106,109,107,106,106,105,106,108,111,114,119,125,131,137,143,150,156,164,172,174,172,166,156,143,130,116,103,91,81,73,66,62,58,60,60,60,59,59,60,60,60,60,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,72,73,73,73,73,129,127,122,119,116,113,110,108,107,107,106,107,109,112,115,120,126,132,148,160,169,174,174,169,161,149,135,122,109,97,86,76,69,64,63,62,62,62,61,61,61,61,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,73,73,73,73,74,74,143,142,136,133,129,126,122,119,116,113,110,108,107,107,106,107,109,112,131,142,154,165,172,174,172,165,155,141,128,115,102,90,80,72,66,65,64,64,63,62,62,61,61,61,62,63,63,64,65,66,66,67,68,69,69,70,71,71,72,72,73,73,73,73,74,74,146,145,143,142,140,138,136,133,129,126,122,119,116,113,110,108,107,107,118,126,137,149,161,169,174,174,169,160,148,134,121,108,96,85,72,71,70,68,66,65,64,62,61,62,62,63,64,64,65,66,67,67,68,69,70,70,71,71,72,72,73,73,73,74,74,74,144,143,146,146,145,144,143,142,140,138,136,133,129,126,122,119,116,113,113,116,122,132,143,155,166,173,174,171,165,154,140,127,114,101,79,77,75,73,70,68,65,63,61,62,62,63,64,64,65,66,67,67,68,69,70,70,71,71,72,72,73,73,73,74,74,74};
static const uint8_t cfl_u_left[32]={139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,139,140,140,140,140,129,126,123,123,123,123,123,123,113,110,110,110};
static const uint32_t cfl_u_pred_hash=0x8704476du;
static const uint32_t cfl_u_recon_hash=0x65f90e5du;
static const uint8_t cfl_u_pred8[8]={135,135,135,135,135,135,135,135};
static const uint8_t cfl_u_recon8[8]={127,127,127,127,127,127,127,127};
static const uint8_t cfl_v_left[32]={117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,117,120,120,121,121,121,121,121,121,129,131,131,131};
static const uint32_t cfl_v_pred_hash=0x159bffe0u;
static const uint32_t cfl_v_recon_hash=0x159bffe0u;
static const uint8_t cfl_v_pred8[8]={119,119,119,119,119,119,119,119};
static const uint8_t cfl_v_recon8[8]={119,119,119,119,119,119,119,119};
/* U DC coeff -992 at pos0; V all-zero */

/*
 * Emerald Q208 4:2:0 chroma reconstruction (SDP leaves).  Neighbour
 * reference arrays are sliced from the authoritative AVM pre-filter dumps
 * (emerald-q208-chroma-trace/prefiltered-{U,V}.txt); the expected predictor
 * and pre-filter djb2 hashes and edge samples come from predicted-{U,V}.txt
 * and prefiltered-{U,V}.txt.  Covers UV_DC_PRED (with IBP), UV_H_PRED and
 * the TX_32X16 / TX_16X8 / TX_32X4 / TX_32X32 geometries.
 */
typedef struct {
    const char *name;
    uint16_t mi_row;
    uint16_t mi_col;
    uint16_t width;
    uint16_t height;
    uint8_t tx_size;
    uint8_t plane;
    Av2hFirstLeafIntraMode mode;
    uint8_t all_zero;
    uint16_t nonzero;
    const uint8_t *above;
    const uint8_t *left;
    const uint16_t *coeff_pos;
    const int32_t *coeff_val;
    uint32_t pred_hash;
    uint32_t recon_hash;
    uint8_t pred_first[8];
    uint8_t pred_last[8];
    uint8_t recon_first[8];
    uint8_t recon_last[8];
} Av2hChromaLeafCase;

static const Av2hChromaLeafCase emerald_chroma_cases[] = {

    {"l_00_00_u",0,0,32,16,10,1,AV2H_INTRA_DC,0,1,0,0,l_00_00_u_cp,l_00_00_u_cv,0x460d5505u,0x84861505u,{128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128},{150,150,150,150,150,150,150,150},{150,150,150,150,150,150,150,150}},
    {"l_00_00_v",0,0,32,16,10,2,AV2H_INTRA_DC,0,1,0,0,l_00_00_v_cp,l_00_00_v_cv,0x460d5505u,0xdac2b505u,{128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128},{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117}},
    {"l_08_00_u",8,0,32,16,10,1,AV2H_INTRA_DC,0,5,l_08_00_u_above,0,l_08_00_u_cp,l_08_00_u_cv,0x84861505u,0xb7ce6651u,{150,150,150,150,150,150,150,150},{150,150,150,150,150,150,150,150},{153,153,153,153,153,154,154,154},{132,133,133,133,134,134,134,134}},
    {"l_08_00_v",8,0,32,16,10,2,AV2H_INTRA_DC,0,2,l_08_00_v_above,0,l_08_00_v_cp,l_08_00_v_cv,0xdac2b505u,0x94787305u,{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117},{113,113,113,113,113,113,113,113},{143,143,143,143,143,143,143,143}},
    {"l_00_16_u",0,16,32,16,10,1,AV2H_INTRA_DC,0,1,0,l_00_16_u_left,l_00_16_u_cp,l_00_16_u_cv,0x84861505u,0x07ca7505u,{150,150,150,150,150,150,150,150},{150,150,150,150,150,150,150,150},{139,139,139,139,139,139,139,139},{139,139,139,139,139,139,139,139}},
    {"l_00_16_v",0,16,32,16,10,2,AV2H_INTRA_DC,1,0,0,l_00_16_v_left,0,0,0xdac2b505u,0xdac2b505u,{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117}},
    {"l_08_16_u",8,16,32,4,20,1,AV2H_INTRA_DC,1,0,l_08_16_u_above,l_08_16_u_left,0,0,0x99cbc65bu,0x99cbc65bu,{140,140,140,140,140,140,140,140},{140,140,140,140,140,140,140,140},{140,140,140,140,140,140,140,140},{140,140,140,140,140,140,140,140}},
    {"l_08_16_v",8,16,32,4,20,2,AV2H_INTRA_DC,1,0,l_08_16_v_above,l_08_16_v_left,0,0,0xc5077464u,0xc5077464u,{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117},{117,117,117,117,117,117,117,117}},
    {"l_10_16_u",10,16,16,8,8,1,AV2H_INTRA_HORIZONTAL,1,0,l_10_16_u_above,l_10_16_u_left,0,0,0x46db0105u,0x46db0105u,{115,115,115,115,115,115,115,115},{96,96,96,96,96,96,96,96},{115,115,115,115,115,115,115,115},{96,96,96,96,96,96,96,96}},
    {"l_10_16_v",10,16,16,8,8,2,AV2H_INTRA_HORIZONTAL,1,0,l_10_16_v_above,l_10_16_v_left,0,0,0xc66d3005u,0xc66d3005u,{118,118,118,118,118,118,118,118},{138,138,138,138,138,138,138,138},{118,118,118,118,118,118,118,118},{138,138,138,138,138,138,138,138}},
    {"l_10_24_u",10,24,16,8,8,1,AV2H_INTRA_DC,1,0,l_10_24_u_above,l_10_24_u_left,0,0,0x8f285307u,0x8f285307u,{129,129,129,129,129,129,129,129},{123,123,123,123,123,123,123,123},{129,129,129,129,129,129,129,129},{123,123,123,123,123,123,123,123}},
    {"l_10_24_v",10,24,16,8,8,2,AV2H_INTRA_DC,1,0,l_10_24_v_above,l_10_24_v_left,0,0,0xcb010a1cu,0xcb010a1cu,{120,120,120,120,120,120,120,120},{121,121,121,121,121,121,121,121},{120,120,120,120,120,120,120,120},{121,121,121,121,121,121,121,121}},
    {"l_14_16_u",14,16,32,4,20,1,AV2H_INTRA_DC,1,0,l_14_16_u_above,l_14_16_u_left,0,0,0x8310b64eu,0x8310b64eu,{107,107,107,107,107,107,107,107},{110,110,110,110,110,110,110,110},{107,107,107,107,107,107,107,107},{110,110,110,110,110,110,110,110}},
    {"l_14_16_v",14,16,32,4,20,2,AV2H_INTRA_DC,1,0,l_14_16_v_above,l_14_16_v_left,0,0,0x759b15abu,0x759b15abu,{133,133,133,133,133,133,133,133},{131,131,131,131,131,131,131,131},{133,133,133,133,133,133,133,133},{131,131,131,131,131,131,131,131}},
    {"l_16_00_u",16,0,32,16,10,1,AV2H_INTRA_HORIZONTAL,0,1,l_16_00_u_above,0,l_16_00_u_cp,l_16_00_u_cv,0x085c5505u,0x5c79f505u,{122,122,122,122,122,122,122,122},{122,122,122,122,122,122,122,122},{111,111,111,111,111,111,111,111},{111,111,111,111,111,111,111,111}},
    {"l_16_00_v",16,0,32,16,10,2,AV2H_INTRA_HORIZONTAL,1,0,l_16_00_v_above,0,0,0,0x0d19f505u,0x0d19f505u,{143,143,143,143,143,143,143,143},{143,143,143,143,143,143,143,143},{143,143,143,143,143,143,143,143},{143,143,143,143,143,143,143,143}},
    {"l_16_16_u",16,16,32,16,10,1,AV2H_INTRA_DC,1,0,l_16_16_u_above,l_16_16_u_left,0,0,0xdc5baba5u,0xdc5baba5u,{115,114,114,113,113,112,112,111},{111,111,111,111,111,111,111,111},{115,114,114,113,113,112,112,111},{111,111,111,111,111,111,111,111}},
    {"l_16_16_v",16,16,32,16,10,2,AV2H_INTRA_DC,1,0,l_16_16_v_above,l_16_16_v_left,0,0,0xd3061a25u,0xd3061a25u,{135,135,135,135,135,134,134,134},{135,135,135,135,135,135,135,135},{135,135,135,135,135,134,134,134},{135,135,135,135,135,135,135,135}},
    {"l_16_32_u",16,32,32,16,10,1,AV2H_INTRA_DC,1,0,l_16_32_u_above,l_16_32_u_left,0,0,0x3b1b92efu,0x3b1b92efu,{116,116,116,116,117,117,117,118},{117,117,117,117,117,117,117,117},{116,116,116,116,117,117,117,118},{117,117,117,117,117,117,117,117}},
    {"l_16_32_v",16,32,32,16,10,2,AV2H_INTRA_DC,0,1,l_16_32_v_above,l_16_32_v_left,l_16_32_v_cp,l_16_32_v_cv,0x5336a9c7u,0xaeb8d14bu,{125,125,125,125,125,124,124,124},{126,126,126,126,126,126,126,126},{136,136,136,136,136,135,135,135},{137,137,137,137,137,137,137,137}},
};

static void test_emerald_q208_chroma_leaves(void) {
    static uint8_t chroma_data[96U * 48U];
    static uint8_t chroma_rec[(96U / 4U) * (48U / 4U)];
    static uint8_t pred_data[96U * 48U];
    static uint8_t pred_rec[(96U / 4U) * (48U / 4U)];
    static uint8_t pblk[512U];
    static uint8_t rblk[512U];
    static Av2hReconstructionScratch scratch;
    size_t case_index;
    for (case_index = 0U;
         case_index < sizeof(emerald_chroma_cases) /
                      sizeof(emerald_chroma_cases[0]);
         ++case_index) {
        const Av2hChromaLeafCase *tc = &emerald_chroma_cases[case_index];
        Av2hPartitionNode leaf = { tc->mi_row, tc->mi_col, 0U };
        Av2hFirstChromaMode mode = { 0 };
        Av2hFirstCoeffPrefix prefix = { 0 };
        Av2hFirstTransformType transform = { 0 };
        Av2hFirstCoeffBlock block = { 0 };
        size_t count = (size_t)tc->width * tc->height;
        uint16_t block_x = (uint16_t)(tc->mi_col * 2U);
        uint16_t block_y = (uint16_t)(tc->mi_row * 2U);
        size_t index;
        size_t r;
        size_t c;
        Av2hStatus status;
        av2h_memory_fill(chroma_data, 0U, sizeof(chroma_data));
        av2h_memory_fill(chroma_rec, 1U, sizeof(chroma_rec));
        av2h_memory_fill(pred_data, 0U, sizeof(pred_data));
        av2h_memory_fill(pred_rec, 1U, sizeof(pred_rec));
        /* Place the authoritative neighbour edge samples. */
        if (tc->above != 0) {
            for (c = 0U; c < tc->width; ++c) {
                chroma_data[((size_t)block_y - 1U) * 96U + block_x + c] =
                    tc->above[c];
            }
        }
        if (tc->left != 0) {
            for (r = 0U; r < tc->height; ++r) {
                chroma_data[((size_t)block_y + r) * 96U + block_x - 1U] =
                    tc->left[r];
            }
        }
        Av2hReconstructionPlane cp = {
            chroma_data, 96U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        Av2hReconstructionPlane pp = {
            pred_data, 96U, 96U, 48U, 96U, 48U, pred_rec, 96U / 4U,
            sizeof(pred_rec)
        };
        mode.mode = tc->mode;
        mode.angle_delta = 0;
        prefix.tx_size = (Av2hFirstTxSize)tc->tx_size;
        prefix.all_zero = tc->all_zero;
        prefix.eob = tc->all_zero ? 0U : tc->nonzero;
        transform.primary_type = AV2H_TX_DCT_DCT;
        block.width = tc->width;
        block.height = tc->height;
        block.coefficient_count = (uint32_t)count;
        block.nonzero_count = tc->all_zero ? 0U : tc->nonzero;
        for (index = 0U; index < (tc->all_zero ? 0U : tc->nonzero); ++index) {
            block.dequantized[tc->coeff_pos[index]] = tc->coeff_val[index];
        }
        status = av2h_chroma_reconstruct_to_plane_ex(
            &leaf, &mode, &prefix, &transform, &block, &cp, 0, &pp, &scratch);
        if (status != AV2H_OK) {
            fprintf(stderr, "%s: reconstruct status %d\n", tc->name, status);
            ++failures;
            continue;
        }
        for (r = 0U; r < tc->height; ++r) {
            for (c = 0U; c < tc->width; ++c) {
                pblk[r * tc->width + c] =
                    pred_data[((size_t)block_y + r) * 96U + block_x + c];
                rblk[r * tc->width + c] =
                    chroma_data[((size_t)block_y + r) * 96U + block_x + c];
            }
        }
        if (trace_sample_hash(pblk, count) != tc->pred_hash) {
            fprintf(stderr, "%s: predictor hash mismatch\n", tc->name);
            ++failures;
        }
        if (trace_sample_hash(rblk, count) != tc->recon_hash) {
            fprintf(stderr, "%s: reconstruction hash mismatch\n", tc->name);
            ++failures;
        }
        for (index = 0U; index < 8U; ++index) {
            CHECK(pblk[index] == tc->pred_first[index]);
            CHECK(pblk[count - 8U + index] == tc->pred_last[index]);
            CHECK(rblk[index] == tc->recon_first[index]);
            CHECK(rblk[count - 8U + index] == tc->recon_last[index]);
        }
    }
}

static void test_emerald_q208_chroma_cfl(void) {
    static uint8_t luma_data[192U * 96U];
    static uint8_t luma_rec[(192U / 4U) * (96U / 4U)];
    static uint8_t chroma_data[96U * 48U];
    static uint8_t chroma_rec[(96U / 4U) * (48U / 4U)];
    static uint8_t pred_data[96U * 48U];
    static uint8_t pred_rec[(96U / 4U) * (48U / 4U)];
    static uint8_t pblk[1024U];
    static uint8_t rblk[1024U];
    static Av2hReconstructionScratch scratch;
    size_t r;
    size_t c;
    size_t index;
    int plane_index;
    av2h_memory_fill(luma_data, 0U, sizeof(luma_data));
    for (r = 0U; r < 64U; ++r) {
        for (c = 0U; c < 66U; ++c) {
            luma_data[r * 192U + 126U + c] = cfl_luma[r * 66U + c];
        }
    }
    av2h_memory_fill(luma_rec, 1U, sizeof(luma_rec));
    Av2hReconstructionPlane luma = {
        luma_data, 192U, 192U, 96U, 192U, 96U, luma_rec, 192U / 4U,
        sizeof(luma_rec)
    };
    for (plane_index = 0; plane_index < 2; ++plane_index) {
        const uint8_t *left = plane_index == 0 ? cfl_u_left : cfl_v_left;
        uint32_t pred_hash = plane_index == 0 ?
            cfl_u_pred_hash : cfl_v_pred_hash;
        uint32_t recon_hash = plane_index == 0 ?
            cfl_u_recon_hash : cfl_v_recon_hash;
        const uint8_t *pred8 = plane_index == 0 ? cfl_u_pred8 : cfl_v_pred8;
        const uint8_t *recon8 = plane_index == 0 ? cfl_u_recon8 : cfl_v_recon8;
        uint8_t all_zero = plane_index == 0 ? 0U : 1U;
        Av2hPartitionNode leaf = { 0U, 32U, 0U };
        Av2hFirstChromaMode mode = { 0 };
        Av2hFirstCoeffPrefix prefix = { 0 };
        Av2hFirstTransformType transform = { 0 };
        Av2hFirstCoeffBlock block = { 0 };
        Av2hStatus status;
        av2h_memory_fill(chroma_data, 0U, sizeof(chroma_data));
        for (r = 0U; r < 32U; ++r) {
            chroma_data[r * 96U + 63U] = left[r];
        }
        av2h_memory_fill(chroma_rec, 1U, sizeof(chroma_rec));
        av2h_memory_fill(pred_data, 0U, sizeof(pred_data));
        av2h_memory_fill(pred_rec, 1U, sizeof(pred_rec));
        Av2hReconstructionPlane cp = {
            chroma_data, 96U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        Av2hReconstructionPlane pp = {
            pred_data, 96U, 96U, 48U, 96U, 48U, pred_rec, 96U / 4U,
            sizeof(pred_rec)
        };
        mode.is_cfl = 1U;
        mode.mode = AV2H_INTRA_DC;
        prefix.tx_size = (Av2hFirstTxSize)3;
        prefix.all_zero = all_zero;
        prefix.eob = all_zero ? 0U : 1U;
        transform.primary_type = AV2H_TX_DCT_DCT;
        block.width = 32U;
        block.height = 32U;
        block.coefficient_count = 1024U;
        block.nonzero_count = all_zero ? 0U : 1U;
        if (!all_zero) block.dequantized[0] = -992;
        status = av2h_chroma_reconstruct_to_plane_ex(
            &leaf, &mode, &prefix, &transform, &block, &cp, &luma, &pp,
            &scratch);
        if (status != AV2H_OK) {
            fprintf(stderr, "cfl plane %d: status %d\n", plane_index, status);
            ++failures;
            continue;
        }
        for (r = 0U; r < 32U; ++r) {
            for (c = 0U; c < 32U; ++c) {
                pblk[r * 32U + c] = pred_data[r * 96U + 64U + c];
                rblk[r * 32U + c] = chroma_data[r * 96U + 64U + c];
            }
        }
        if (trace_sample_hash(pblk, 1024U) != pred_hash) {
            fprintf(stderr, "cfl plane %d: predictor hash\n", plane_index);
            ++failures;
        }
        if (trace_sample_hash(rblk, 1024U) != recon_hash) {
            fprintf(stderr, "cfl plane %d: recon hash\n", plane_index);
            ++failures;
        }
        for (index = 0U; index < 8U; ++index) {
            CHECK(pblk[index] == pred8[index]);
            CHECK(rblk[index] == recon8[index]);
        }
    }
}

static void test_emerald_chroma_plane_ex_checks(void) {
    static uint8_t luma_data[192U * 96U];
    static uint8_t luma_rec[(192U / 4U) * (96U / 4U)];
    static uint8_t chroma_data[96U * 48U];
    static uint8_t chroma_rec[(96U / 4U) * (48U / 4U)];
    static Av2hReconstructionScratch scratch;
    size_t r;
    size_t c;
    size_t index;
    Av2hPartitionNode cfl_leaf = { 0U, 32U, 0U };
    Av2hFirstChromaMode cfl_mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block = { 0 };
    Av2hStatus status;
    av2h_memory_fill(luma_data, 60U, sizeof(luma_data));
    for (r = 0U; r < 64U; ++r) {
        for (c = 0U; c < 66U; ++c) {
            luma_data[r * 192U + 126U + c] = cfl_luma[r * 66U + c];
        }
    }
    av2h_memory_fill(luma_rec, 1U, sizeof(luma_rec));
    Av2hReconstructionPlane luma = {
        luma_data, 192U, 192U, 96U, 192U, 96U, luma_rec, 192U / 4U,
        sizeof(luma_rec)
    };
    cfl_mode.is_cfl = 1U;
    cfl_mode.mode = AV2H_INTRA_DC;
    prefix.tx_size = (Av2hFirstTxSize)3;
    prefix.all_zero = 1U;
    transform.primary_type = AV2H_TX_DCT_DCT;
    block.width = 32U;
    block.height = 32U;
    block.coefficient_count = 1024U;
    block.nonzero_count = 0U;

    /* CFL fails closed (output preserved) when the co-located luma is not
     * yet reconstructed. */
    av2h_memory_fill(chroma_data, 0U, sizeof(chroma_data));
    for (r = 0U; r < 32U; ++r) chroma_data[r * 96U + 63U] = cfl_u_left[r];
    av2h_memory_fill(chroma_rec, 1U, sizeof(chroma_rec));
    for (r = 0U; r < 32U; ++r) {
        for (c = 0U; c < 32U; ++c) chroma_data[r * 96U + 64U + c] = 0xABU;
    }
    av2h_memory_fill(luma_rec, 0U, sizeof(luma_rec));
    {
        Av2hReconstructionPlane cp = {
            chroma_data, 96U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        CHECK(av2h_chroma_reconstruct_to_plane_ex(
                  &cfl_leaf, &cfl_mode, &prefix, &transform, &block, &cp,
                  &luma, 0, &scratch) == AV2H_UNSUPPORTED);
        for (r = 0U; r < 32U; ++r) {
            for (c = 0U; c < 32U; ++c) {
                CHECK(chroma_data[r * 96U + 64U + c] == 0xABU);
            }
        }
        /* A NULL luma plane also fails closed for CFL. */
        CHECK(av2h_chroma_reconstruct_to_plane_ex(
                  &cfl_leaf, &cfl_mode, &prefix, &transform, &block, &cp,
                  0, 0, &scratch) == AV2H_UNSUPPORTED);
        for (r = 0U; r < 32U; ++r) {
            for (c = 0U; c < 32U; ++c) {
                CHECK(chroma_data[r * 96U + 64U + c] == 0xABU);
            }
        }
    }
    av2h_memory_fill(luma_rec, 1U, sizeof(luma_rec));

    /* Unsupported geometry is rejected without touching the plane. */
    {
        Av2hReconstructionPlane cp = {
            chroma_data, 96U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        Av2hFirstChromaMode dc_mode = { 0 };
        Av2hFirstCoeffBlock bad = { 0 };
        dc_mode.mode = AV2H_INTRA_DC;
        bad.width = 8U;
        bad.height = 8U;
        bad.coefficient_count = 64U;
        CHECK(av2h_chroma_reconstruct_to_plane_ex(
                  &cfl_leaf, &dc_mode, &prefix, &transform, &bad, &cp,
                  &luma, 0, &scratch) == AV2H_INVALID_ARGUMENT);
    }

    /* A malformed predictor plane is rejected. */
    {
        Av2hReconstructionPlane cp = {
            chroma_data, 96U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        Av2hReconstructionPlane bad_pred = {
            chroma_data, 8U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        Av2hFirstChromaMode dc_mode = { 0 };
        dc_mode.mode = AV2H_INTRA_DC;
        CHECK(av2h_chroma_reconstruct_to_plane_ex(
                  &cfl_leaf, &dc_mode, &prefix, &transform, &block, &cp,
                  &luma, &bad_pred, &scratch) == AV2H_INVALID_ARGUMENT);
    }

    /* A neighbour-only all-zero DC leaf reconstructs to the flat DC surface
     * and marks its samples ready. */
    {
        Av2hReconstructionPlane cp = {
            chroma_data, 96U, 96U, 48U, 96U, 48U, chroma_rec, 96U / 4U,
            sizeof(chroma_rec)
        };
        Av2hPartitionNode leaf = { 0U, 16U, 0U };
        Av2hFirstChromaMode dc_mode = { 0 };
        Av2hFirstCoeffBlock az = { 0 };
        dc_mode.mode = AV2H_INTRA_DC;
        prefix.tx_size = (Av2hFirstTxSize)10;
        az.width = 32U;
        az.height = 16U;
        az.coefficient_count = 512U;
        av2h_memory_fill(chroma_data, 0U, sizeof(chroma_data));
        for (r = 0U; r < 16U; ++r) chroma_data[r * 96U + 31U] = 150U;
        av2h_memory_fill(chroma_rec, 1U, sizeof(chroma_rec));
        status = av2h_chroma_reconstruct_to_plane_ex(
            &leaf, &dc_mode, &prefix, &transform, &az, &cp, 0, 0, &scratch);
        CHECK(status == AV2H_OK);
        for (r = 0U; r < 16U; ++r) {
            for (c = 0U; c < 32U; ++c) {
                CHECK(chroma_data[r * 96U + 32U + c] == 150U);
            }
        }
    }
    (void)index;
}

static void test_emerald_chroma_dct_geometries(void) {
    static Av2hReconstructionScratch scratch;
    static Av2hFirstReconstruction reconstruction;
    static uint8_t predictor[1024U];
    static const struct { uint8_t tx; uint16_t w; uint16_t h; } geos[] = {
        { 10U, 32U, 16U }, { 3U, 32U, 32U }
    };
    size_t g;
    for (g = 0U; g < sizeof(geos) / sizeof(geos[0]); ++g) {
        Av2hFirstLeafMode mode = { 0 };
        Av2hFirstCoeffPrefix prefix = { 0 };
        Av2hFirstTransformType transform = { 0 };
        Av2hFirstCoeffBlock block = { 0 };
        size_t count = (size_t)geos[g].w * geos[g].h;
        size_t index;
        mode.mode = AV2H_INTRA_DC;
        transform.primary_type = AV2H_TX_DCT_DCT;
        prefix.tx_size = (Av2hFirstTxSize)geos[g].tx;
        block.width = geos[g].w;
        block.height = geos[g].h;
        block.coefficient_count = (uint32_t)count;
        av2h_memory_fill(predictor, 100U, sizeof(predictor));

        /* Predictor-only all-zero path preserves the predictor exactly. */
        prefix.all_zero = 1U;
        block.nonzero_count = 0U;
        CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                    predictor, geos[g].w, &scratch,
                                    &reconstruction) == AV2H_OK);
        CHECK(reconstruction.width == geos[g].w);
        CHECK(reconstruction.height == geos[g].h);
        for (index = 0U; index < count; ++index) {
            CHECK(reconstruction.pixels[index] == 100U);
        }

        /* A DC-only coefficient yields a flat (constant) residual surface. */
        prefix.all_zero = 0U;
        prefix.eob = 1U;
        block.nonzero_count = 1U;
        block.dequantized[0] = 1984;
        CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                    predictor, geos[g].w, &scratch,
                                    &reconstruction) == AV2H_OK);
        for (index = 1U; index < count; ++index) {
            CHECK(reconstruction.pixels[index] == reconstruction.pixels[0]);
        }

        /* Saturating DC residuals clip to the valid range. */
        block.dequantized[0] = 400000;
        CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                    predictor, geos[g].w, &scratch,
                                    &reconstruction) == AV2H_OK);
        for (index = 0U; index < count; ++index) {
            CHECK(reconstruction.pixels[index] == 255U);
        }
        block.dequantized[0] = -400000;
        CHECK(av2h_luma_reconstruct(&mode, &prefix, &transform, &block,
                                    predictor, geos[g].w, &scratch,
                                    &reconstruction) == AV2H_OK);
        for (index = 0U; index < count; ++index) {
            CHECK(reconstruction.pixels[index] == 0U);
        }
    }
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
    test_emerald_q208_luma_blocks();
    test_emerald_reconstruct_clipping();
    test_emerald_reconstruct_invalid_preserves_output();
    test_emerald_q208_chroma_leaves();
    test_emerald_q208_chroma_cfl();
    test_emerald_chroma_plane_ex_checks();
    test_emerald_chroma_dct_geometries();
    if (failures != 0) {
        fprintf(stderr, "%d reconstruction checks failed\n", failures);
        return 1;
    }
    puts("reconstruction unit: pass");
    return 0;
}
