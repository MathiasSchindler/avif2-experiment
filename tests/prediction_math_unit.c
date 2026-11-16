#include "decoder/filter.h"
#include "decoder/internal/chroma_prediction.h"
#include "decoder/internal/intra_prediction.h"
#include "decoder/internal/intrabc_prediction.h"
#include "decoder/internal/sample_math.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                      \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    unsigned int byte;
    for (byte = 0U; byte < 4U; ++byte) {
        hash ^= (value >> (byte * 8U)) & 255U;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t divisor_case(uint64_t hash, uint32_t divisor) {
    int shift;
    uint16_t scale = av2h_sample_resolve_divisor_32(divisor, &shift);
    hash = hash_u32(hash, divisor);
    hash = hash_u32(hash, scale);
    return hash_u32(hash, (uint32_t)shift);
}

static void test_extreme_divisor_and_regression(void) {
    int shift = -1;
    CHECK(av2h_sample_resolve_divisor_32(0U, &shift) == 512U);
    CHECK(shift == 9);
    CHECK(av2h_sample_resolve_divisor_32(UINT32_C(0x80000000), &shift) == 512U);
    CHECK(shift == 40);
    CHECK(av2h_sample_resolve_divisor_32(UINT32_C(0x7fffffff), &shift) == 256U);
    CHECK(shift == 39);

    CHECK(av2h_sample_regression_slope(0, 0, 0, 0, -1, 8) == 0);
    CHECK(av2h_sample_regression_slope(10, 10, 100, 100, 1, -1) == 0);
    CHECK(av2h_sample_regression_slope(10, 10, 100, 100, 1, 30) == 0);
    CHECK(av2h_sample_regression_slope(INT32_MAX, INT32_MIN, INT32_MIN, INT32_MAX, 1, 8) == 0);
    CHECK(av2h_sample_regression_slope(0, 0, INT32_MAX, INT32_MIN, 2, 8) == -256);
    CHECK(av2h_sample_regression_slope(0, 0, INT32_MAX / 4, INT32_MIN, 2, 8) == -511);
    {
        int32_t slope =
            av2h_sample_regression_slope(INT32_MIN / 2, INT32_MAX / 2, INT32_MAX, INT32_MIN, 4, 8);
        CHECK(slope >= -511 && slope <= 511);
    }
}

static void test_degenerate_mhccp_and_cfl(void) {
    static uint8_t luma8_buf[64U * 64U];
    static uint8_t chroma8_buf[32U * 32U];
    static uint8_t recon_map[16U * 16U];
    static uint8_t pred8[32U * 32U];
    static uint16_t luma16_buf[64U * 64U];
    static uint16_t chroma16_buf[64U * 64U];
    static uint16_t pred16[32U * 32U];
    const Av2hSampleDomain *domains[] = {
        &av2h_sample_domain_8bit,
        &av2h_sample_domain_10bit,
        &av2h_sample_domain_12bit
    };
    Av2hReconstructionPlane luma8 = {
        luma8_buf, 64U, 64U, 64U, 64U, 64U, recon_map, 16U, sizeof(recon_map), {0}
    };
    Av2hReconstructionPlane chroma8 = {
        chroma8_buf, 32U, 32U, 32U, 32U, 32U, recon_map, 8U, 64U, {0}
    };
    Av2hReconstructionPlaneU16 luma16 = {
        luma16_buf, 64U, 64U, 64U, 64U, 64U, recon_map, 16U, sizeof(recon_map), {0}
    };
    Av2hReconstructionPlaneU16 chroma16_420 = {
        chroma16_buf, 32U, 32U, 32U, 32U, 32U, recon_map, 8U, 64U, {0}
    };
    Av2hReconstructionPlaneU16 chroma16_444 = {
        chroma16_buf, 64U, 64U, 64U, 64U, 64U, recon_map, 16U, sizeof(recon_map), {0}
    };
    unsigned int pattern;
    memset(recon_map, 0xff, sizeof(recon_map));

    for (pattern = 0U; pattern < 4U; ++pattern) {
        size_t i;
        unsigned int dir;
        unsigned int filt;
        size_t d;
        for (i = 0U; i < 64U * 64U; ++i) {
            uint8_t v8 = pattern == 0U ? 0U : pattern == 1U ? 255U :
                pattern == 2U ? ((i & 1U) ? 255U : 0U) : (uint8_t)((i * 97U) & 255U);
            uint16_t v16 = pattern == 0U ? 0U : pattern == 1U ? 4095U :
                pattern == 2U ? ((i & 1U) ? 65535U : 0U) : 65535U;
            luma8_buf[i] = v8;
            if (i < 32U * 32U) chroma8_buf[i] = (uint8_t)(255U - v8);
            luma16_buf[i] = v16;
            chroma16_buf[i] = (uint16_t)(65535U - v16);
        }
        for (dir = 0U; dir < 3U; ++dir) {
            for (filt = 0U; filt < 3U; ++filt) {
                Av2hChromaLeafMode mode = { 0 };
                mode.is_cfl = 1U;
                mode.cfl_kind = AV2H_CFL_MULTI_PARAM;
                mode.mhccp_direction = (uint8_t)dir;
                mode.cfl_downsample_filter_index = (uint8_t)filt;
                mode.superblock_size = 64U;
                mode.chroma_format_idc = 0U;
                CHECK(av2h_mhccp_predict_observed(
                          NULL, &mode, &luma8, &chroma8, 4U, 4U, 8U, 8U, pred8, 8U) == AV2H_OK);
                for (d = 0U; d < 3U; ++d) {
                    CHECK(av2h_mhccp_predict_observed_u16(
                              &mode, &luma16, &chroma16_420, 4U, 4U, 8U, 8U,
                              pred16, 8U, domains[d]) == AV2H_OK);
                }
                mode.chroma_format_idc = 2U;
                for (d = 0U; d < 3U; ++d) {
                    CHECK(av2h_mhccp_predict_observed_u16(
                              &mode, &luma16, &chroma16_444, 4U, 4U, 8U, 8U,
                              pred16, 8U, domains[d]) == AV2H_OK);
                }
            }
        }
        memset(pred8, 128, sizeof(pred8));
        CHECK(av2h_cfl_derived_predict(
                  &luma8, &chroma8, 0U, 0U, INT32_MIN, 64U, 0U,
                  4U, 4U, 8U, 8U, 1U, 1U, pred8, 8U) == AV2H_OK);
        CHECK(av2h_cfl_derived_predict(
                  &luma8, &chroma8, 2U, 0U, INT32_MAX, 64U, 0U,
                  4U, 4U, 8U, 8U, 1U, 1U, pred8, 8U) == AV2H_OK);
        for (d = 0U; d < 3U; ++d) {
            for (i = 0U; i < 64U; ++i) pred16[i] = domains[d]->midpoint;
            CHECK(av2h_cfl_derived_predict_u16(
                      &luma16, &chroma16_420, 1U, 64U, 4U, 4U, 8U, 8U,
                      1U, 1U, pred16, 8U, domains[d]) == AV2H_OK);
            CHECK(av2h_cfl_derived_predict_u16_444(
                      &luma16, &chroma16_444, 4U, 4U, 8U, 8U,
                      1U, 1U, pred16, 8U, domains[d]) == AV2H_OK);
            CHECK(av2h_cfl_explicit_predict_u16(
                      &luma16, 0U, INT32_MIN, 64U, 4U, 4U, 8U, 8U,
                      1U, 1U, pred16, 8U, domains[d]) == AV2H_OK);
            CHECK(av2h_cfl_explicit_predict_u16_444(
                      &luma16, INT32_MAX, 4U, 4U, 8U, 8U,
                      1U, 1U, pred16, 8U, domains[d]) == AV2H_OK);
        }
    }
}

static void test_extreme_intrabc_and_filters(void) {
    static uint16_t src16[32U * 32U];
    static uint16_t dst16[16U * 16U];
    static uint8_t src8[64U * 64U];
    static uint8_t deblocked8[64U * 64U];
    static uint8_t dst8[64U * 64U];
    static uint8_t tx_skip[16U * 16U];
    uint8_t unit_flag = AV2H_RESTORE_PC_WIENER;
    int16_t nonsep_taps[AV2H_WIENER_NONSEP_MAX_TAPS];
    size_t i;
    uint8_t depth;

    CHECK(av2h_intrabc_plane_position(UINT16_MAX, INT32_MAX, 0U) == INT32_MAX);
    CHECK(av2h_intrabc_plane_position(0U, INT32_MIN, 0U) == INT32_MIN);
    CHECK(av2h_intrabc_plane_position(UINT16_MAX, INT32_MIN, 255U) < 0);
    CHECK(av2h_intrabc_integer_position(INT32_MIN) == -(INT32_C(1) << 27));
    CHECK(av2h_intrabc_full_pel(INT16_MIN) == -4096);
    CHECK(av2h_intrabc_full_pel(INT16_MAX) == 4096);

    for (i = 0U; i < sizeof(src16) / sizeof(src16[0]); ++i) {
        src16[i] = (i & 1U) ? 65535U : 0U;
    }
    CHECK(av2h_intrabc_convolve(NULL, src16, 32U, 0, 0, 16U, 0U, 8U, 8U, 10U, NULL, dst16) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_intrabc_convolve(NULL, src16, 32U, 0, 0, 0U, 16U, 8U, 8U, 10U, NULL, dst16) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_intrabc_morph_u16(src16, 32U, 32U, 32U, 8U, 8U, 8U, 8U, 2, 2, 8U, 8U, 32U, dst16) ==
          AV2H_INVALID_DATA);
    for (depth = 8U; depth <= 12U; depth += 2U) {
        uint16_t max_sample = (uint16_t)((1U << depth) - 1U);
        CHECK(av2h_intrabc_convolve(NULL, src16, 32U, 0, 0, 15U, 15U, 8U, 8U, depth, NULL, dst16) ==
              AV2H_OK);
        CHECK(av2h_intrabc_morph_u16(src16, 32U, 32U, 32U, 8U, 8U, 8U, 8U, 2, 2, 8U, 8U, depth,
                                     dst16) == AV2H_OK);
        for (i = 0U; i < 64U; ++i) CHECK(dst16[i] <= max_sample);
    }

    CHECK(av2h_directional_filter_strength(INT32_MAX, INT32_MAX, INT32_MIN, 1U) == 3);
    CHECK(av2h_directional_filter_strength(4, 4, INT32_MIN, 0U) == 1);

    for (i = 0U; i < sizeof(src8); ++i) {
        src8[i] = (i & 1U) ? 255U : 0U;
        deblocked8[i] = (i & 1U) ? 0U : 255U;
    }
    memset(tx_skip, 1, sizeof(tx_skip));
    {
        Av2hFilterPlane src_plane = { src8, 64U, 64U, 64U };
        Av2hFilterPlane deb_plane = { deblocked8, 64U, 64U, 64U };
        Av2hFilterPlane dst_plane = { dst8, 64U, 64U, 64U };
        Av2hFrameState frame = { 0 };
        Av2hRestorationControlMap controls = { 0 };
        frame.restoration_type[0] = AV2H_RESTORE_PC_WIENER;
        frame.base_q_index = 303U;
        controls.unit_flags[0] = &unit_flag;
        controls.tx_skip = tx_skip;
        controls.tx_skip_stride = 16U;
        controls.unit_columns[0] = 1U;
        controls.unit_rows[0] = 1U;
        controls.unit_size[0] = 64U;
        CHECK(av2h_pc_wiener_filter_rows(&dst_plane, &src_plane, &deb_plane, &frame, &controls,
                                         0U, 64U) == AV2H_OK);
        frame.base_q_index = UINT16_MAX;
        CHECK(av2h_pc_wiener_filter_rows(&dst_plane, &src_plane, &deb_plane, &frame, &controls,
                                         0U, 64U) == AV2H_OK);
    }
    {
        Av2hFilterPlanes src_planes = { {{src8, 64U, 64U, 64U}, {src8, 32U, 32U, 32U}, {src8, 32U, 32U, 32U}} };
        Av2hFilterPlanes deb_planes = { {{deblocked8, 64U, 64U, 64U}, {deblocked8, 32U, 32U, 32U}, {deblocked8, 32U, 32U, 32U}} };
        Av2hFilterPlanes dst_planes = { {{dst8, 64U, 64U, 64U}, {dst8, 32U, 32U, 32U}, {dst8, 32U, 32U, 32U}} };
        Av2hSequenceState sequence = { 0 };
        Av2hFrameState frame = { 0 };
        Av2hRestorationControlMap controls = { 0 };
        uint8_t nonsep_flag = AV2H_RESTORE_WIENER_NONSEP;
        for (i = 0U; i < AV2H_WIENER_NONSEP_MAX_TAPS; ++i) {
            nonsep_taps[i] = (i & 1U) ? INT16_MIN : INT16_MAX;
        }
        frame.plane_count = 3U;
        frame.chroma_format_idc = 0U;
        for (i = 0U; i < 3U; ++i) {
            frame.restoration_type[i] = AV2H_RESTORE_WIENER_NONSEP;
            frame.restoration_num_filter_classes[i] = 1U;
            controls.unit_flags[i] = &nonsep_flag;
            controls.unit_taps[i] = nonsep_taps;
            controls.unit_columns[i] = 1U;
            controls.unit_rows[i] = 1U;
            controls.unit_size[i] = i == 0U ? 64U : 32U;
        }
        CHECK(av2h_wiener_nonsep_filter_rows(&dst_planes, &src_planes, &deb_planes, &sequence,
                                             &frame, &controls, 0U, 64U) == AV2H_OK);
    }
    {
        Av2hFilterPlaneU16 src_u16 = { src16, 8U, 8U, 8U };
        Av2hFilterPlaneU16 dst_u16 = { dst16, 8U, 8U, 8U };
        CHECK(av2h_cdef_filter_block_u16(&dst_u16, &src_u16, 0U, 0U, 8U, 8U, 0U, 15U, 4U, 6U,
                                         3U, UINT32_MAX, &av2h_sample_domain_12bit) == AV2H_OK);
    }
}

int main(void) {
    static const int counts[] = { 0, 1, 2, 8, 32 };
    uint64_t divisor_hash = UINT64_C(14695981039346656037);
    uint64_t regression_hash = UINT64_C(14695981039346656037);
    uint32_t divisor;
    unsigned int bit;
    size_t index;
    for (divisor = 1U; divisor <= 65536U; ++divisor) {
        divisor_hash = divisor_case(divisor_hash, divisor);
    }
    for (bit = 0U; bit < 32U; ++bit) {
        int delta;
        for (delta = -1; delta <= 1; ++delta) {
            int64_t value = ((int64_t)1 << bit) + delta;
            if (value > 0) {
                divisor_hash = divisor_case(divisor_hash, (uint32_t)value);
            }
        }
    }
    divisor_hash = divisor_case(divisor_hash, UINT32_MAX);
    for (index = 0U; index < sizeof(counts) / sizeof(counts[0]); ++index) {
        int slope;
        for (slope = -4; slope <= 4; ++slope) {
            int shift;
            for (shift = 0; shift <= 12; ++shift) {
                int shape;
                for (shape = 0; shape < 8; ++shape) {
                    int32_t sum_x = 0;
                    int32_t sum_y = 0;
                    int32_t sum_xx = 0;
                    int32_t sum_xy = 0;
                    int sample;
                    for (sample = 0; sample < counts[index]; ++sample) {
                        int32_t x = (sample * sample + 3 * sample +
                                     shape * 7) % 1024;
                        int32_t y = slope * x + shape - 4;
                        sum_x += x;
                        sum_y += y;
                        sum_xx += x * x;
                        sum_xy += x * y;
                    }
                    regression_hash = hash_u32(
                        regression_hash, (uint32_t)av2h_sample_regression_slope(
                            sum_x, sum_y, sum_xx, sum_xy,
                            counts[index], shift));
                }
            }
        }
    }
    printf("prediction math: divisor=%016llx regression=%016llx\n",
           (unsigned long long)divisor_hash,
           (unsigned long long)regression_hash);
    if (divisor_hash != UINT64_C(0x3cc8148f050d5c66) ||
        regression_hash != UINT64_C(0xd6f776f1ee629f75)) {
        fputs("prediction math characterization mismatch\n", stderr);
        return 1;
    }
    test_extreme_divisor_and_regression();
    test_degenerate_mhccp_and_cfl();
    test_extreme_intrabc_and_filters();
    return failures != 0;
}
