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

int main(void) {
    test_fixture_reconstruction();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d reconstruction checks failed\n", failures);
        return 1;
    }
    puts("reconstruction unit: pass");
    return 0;
}
