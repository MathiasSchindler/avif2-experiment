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

static void test_arguments(void) {
    Av2hFirstCoeffLevelCdfs cdfs;
    Av2hRangeDecoder decoder;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock block;
    CHECK(sizeof(Av2hFirstCoeffLevelCdfs) == 3276U);
    CHECK(av2h_first_coeff_level_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_coeff_block_read(
              0, &cdfs, &sequence, &frame, &mode, &prefix, &transform,
              &block) == AV2H_INVALID_ARGUMENT);
    sequence.enable_tcq = 1U;
    frame.base_q_index = 128U;
    prefix.eob = 1U;
    CHECK(av2h_first_coeff_block_read(
              &decoder, &cdfs, &sequence, &frame, &mode, &prefix, &transform,
              &block) == AV2H_UNSUPPORTED);
}

int main(void) {
    test_fixture_coefficients();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d coefficient level checks failed\n", failures);
        return 1;
    }
    puts("coefficient level unit: pass");
    return 0;
}