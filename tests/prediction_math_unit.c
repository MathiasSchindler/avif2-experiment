#include "decoder/filter.h"
#include "decoder/internal/chroma_prediction.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/intra_prediction.h"
#include "decoder/internal/intrabc_prediction.h"
#include "decoder/internal/sample_math.h"

#include <stdio.h>
#include <stdlib.h>
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

static void test_s3_deblock_tight_bounds(void) {
    unsigned int neg_len;
    unsigned int pos_len;
    for (neg_len = 4U; neg_len <= 18U; neg_len += 2U) {
        for (pos_len = neg_len; pos_len <= 18U; pos_len += 2U) {
            unsigned int neg_max = neg_len / 2U - 1U;
            unsigned int pos_max = pos_len / 2U - 1U;
            unsigned int neg_req = neg_max + 1U > 3U ? neg_max + 1U : 3U;
            unsigned int pos_req = pos_max > 3U ? pos_max : 3U;
            unsigned int span = neg_req + pos_req + 1U;
            unsigned int vertical;
            for (vertical = 0U; vertical < 2U; ++vertical) {
                uint16_t width = (uint16_t)(vertical ? span : 4U);
                uint16_t height = (uint16_t)(vertical ? 4U : span);
                uint16_t edge_x = (uint16_t)(vertical ? neg_req : 0U);
                uint16_t edge_y = (uint16_t)(vertical ? 0U : neg_req);
                size_t count = (size_t)width * height;
                uint8_t *buf8 = (uint8_t *)malloc(count);
                uint16_t *buf16 = (uint16_t *)malloc(count * sizeof(uint16_t));
                Av2hFilterPlane plane8 = { buf8, width, width, height };
                Av2hFilterPlaneU16 plane16 = { buf16, width, width, height };
                size_t i;
                CHECK(buf8 != NULL && buf16 != NULL);
                for (i = 0U; i < count; ++i) {
                    size_t coord = vertical ? (i % width) : (i / width);
                    buf8[i] = coord < neg_req ? 110U : 118U;
                    buf16[i] = coord < neg_req ? 440U : 472U;
                }
                CHECK(av2h_deblock_edge(&plane8, edge_x, edge_y, (uint8_t)vertical,
                                        (uint8_t)neg_len, (uint8_t)pos_len, 255U, 255U) == AV2H_OK);
                CHECK(av2h_deblock_edge_u16(&plane16, edge_x, edge_y, (uint8_t)vertical,
                                            (uint8_t)neg_len, (uint8_t)pos_len, 1023U, 1023U,
                                            &av2h_sample_domain_10bit) == AV2H_OK);
                /* Reject 1-pixel OOB shifts on both negative and positive sides. */
                if (vertical) {
                    CHECK(av2h_deblock_edge(&plane8, (uint16_t)(edge_x - 1U), edge_y, 1U,
                                            (uint8_t)neg_len, (uint8_t)pos_len, 255U, 255U) ==
                          AV2H_INVALID_ARGUMENT);
                    CHECK(av2h_deblock_edge(&plane8, (uint16_t)(edge_x + 1U), edge_y, 1U,
                                            (uint8_t)neg_len, (uint8_t)pos_len, 255U, 255U) ==
                          AV2H_INVALID_ARGUMENT);
                } else {
                    CHECK(av2h_deblock_edge(&plane8, edge_x, (uint16_t)(edge_y - 1U), 0U,
                                            (uint8_t)neg_len, (uint8_t)pos_len, 255U, 255U) ==
                          AV2H_INVALID_ARGUMENT);
                    CHECK(av2h_deblock_edge(&plane8, edge_x, (uint16_t)(edge_y + 1U), 0U,
                                            (uint8_t)neg_len, (uint8_t)pos_len, 255U, 255U) ==
                          AV2H_INVALID_ARGUMENT);
                }
                free(buf8);
                free(buf16);
            }
        }
    }
}

static void test_s3_directional_negative_prefix_and_helper_bounds(void) {
    static const Av2hIntraMode dir_modes[] = {
        AV2H_INTRA_D45, AV2H_INTRA_D67, AV2H_INTRA_VERTICAL, AV2H_INTRA_D113,
        AV2H_INTRA_D135, AV2H_INTRA_D157, AV2H_INTRA_HORIZONTAL, AV2H_INTRA_D203
    };
    uint8_t above8[128U];
    uint8_t left8[128U];
    uint16_t above16[128U];
    uint16_t left16[128U];
    Av2hLumaReferences ref8 = { 0 };
    Av2hLumaReferencesU16 ref16 = { 0 };
    uint8_t *pred8 = (uint8_t *)malloc(64U * 64U);
    uint16_t *pred16 = (uint16_t *)malloc(64U * 64U * sizeof(uint16_t));
    size_t i;
    size_t m;

    CHECK(pred8 != NULL && pred16 != NULL);
    for (i = 0U; i < 128U; ++i) {
        above8[i] = (uint8_t)((i * 17U) & 255U);
        left8[i] = (uint8_t)((i * 29U + 3U) & 255U);
        above16[i] = (uint16_t)((i * 73U) & 1023U);
        left16[i] = (uint16_t)((i * 97U + 11U) & 1023U);
    }
    ref8.above = above8;
    ref8.above_count = 128U;
    ref8.left = left8;
    ref8.left_count = 128U;
    ref8.secondary_above = above8;
    ref8.secondary_above_count = 128U;
    ref8.secondary_left = left8;
    ref8.secondary_left_count = 128U;
    ref8.has_above_left = 1U;
    ref8.above_left = 120U;
    ref8.negative_count = 4U;
    ref16.above = above16;
    ref16.above_count = 128U;
    ref16.left = left16;
    ref16.left_count = 128U;
    ref16.secondary_above = above16;
    ref16.secondary_above_count = 128U;
    ref16.secondary_left = left16;
    ref16.secondary_left_count = 128U;
    ref16.has_above_left = 1U;
    ref16.above_left = 500U;
    ref16.negative_count = 4U;
    for (i = 0U; i < 5U; ++i) {
        ref8.above_negative[i] = (uint8_t)(40U + i);
        ref8.left_negative[i] = (uint8_t)(80U + i);
        ref16.above_negative[i] = (uint16_t)(200U + i);
        ref16.left_negative[i] = (uint16_t)(300U + i);
    }

    /* Exercise mrl_index 0..3 across all directional modes at max 64x64 block size. */
    for (m = 0U; m < sizeof(dir_modes) / sizeof(dir_modes[0]); ++m) {
        uint8_t mrl;
        for (mrl = 0U; mrl < 4U; ++mrl) {
            int delta;
            for (delta = -3; delta <= 3; delta += 3) {
                Av2hLeafMode mode = { 0 };
                mode.mode = dir_modes[m];
                mode.mrl_index = mrl;
                mode.angle_delta = (int8_t)delta;
                (void)av2h_luma_predict(&mode, 64U, 64U, &ref8, pred8, 64U, NULL);
                (void)av2h_luma_predict_u16_adjusted(
                    &mode, 64U, 64U, &ref16, pred16, 64U, 0, 0U, 1U,
                    &av2h_sample_domain_10bit, NULL);
            }
        }
    }

    /* Regression: out-of-range mrl_index must never underflow directional edge storage. */
    {
        Av2hLeafMode bad_mode = { 0 };
        bad_mode.mode = AV2H_INTRA_D67;
        bad_mode.mrl_index = 255U;
        CHECK(av2h_luma_predict(&bad_mode, 4U, 4U, &ref8, pred8, 4U, NULL) == AV2H_UNSUPPORTED);
        CHECK(av2h_luma_predict_u16_adjusted(&bad_mode, 4U, 4U, &ref16, pred16, 4U, 0, 0U, 1U,
                                             &av2h_sample_domain_10bit, NULL) ==
              AV2H_INVALID_ARGUMENT);
        bad_mode.mode = AV2H_INTRA_VERTICAL;
        bad_mode.angle_delta = 2;
        bad_mode.enable_ibp = 1U;
        bad_mode.mrl_index = 4U;
        CHECK(av2h_luma_predict(&bad_mode, 64U, 64U, &ref8, pred8, 64U, NULL) == AV2H_UNSUPPORTED);
    }

    /* Direct specialized directional helpers must reject oversized dimensions and short strides. */
    {
        Av2hLeafMode mode = { 0 };
        mode.mode = AV2H_INTRA_D135;
        CHECK(av2h_predict_directional_multiline_ibp(&mode, 32U, 32U, &ref8, pred8, 32U) ==
              AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_vertical_multiline_tall(&mode, 16U, 32U, &ref8, pred8, 16U) ==
              AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_zone2_mrl2(&mode, 64U, 64U, &ref8, pred8, 64U) ==
              AV2H_INVALID_ARGUMENT);
        mode.mode = AV2H_INTRA_DC;
        CHECK(av2h_predict_zone2_mrl2(&mode, 16U, 16U, &ref8, pred8, 16U) == AV2H_UNSUPPORTED);
        CHECK(av2h_predict_d45_diagonal(32U, 32U, &ref8, pred8, 32U) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_d67_4x4_mrl1_minus_two(8U, 8U, &ref8, pred8, 8U) ==
              AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_d45_minus_one(32U, 16U, &ref8, pred8, 32U) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_d45_plus_three(8U, 8U, &ref8, pred8, 8U) == AV2H_INVALID_ARGUMENT);
        mode.mode = AV2H_INTRA_D203;
        mode.mrl_index = 1U;
        CHECK(av2h_predict_d203_mrl1_plus_three(&mode, 32U, 16U, &ref8, pred8, 32U) ==
              AV2H_INVALID_ARGUMENT);
        mode.mrl_index = 4U;
        CHECK(av2h_predict_d203_mrl1_plus_three(&mode, 8U, 8U, &ref8, pred8, 8U) ==
              AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_d203_plus_three(8U, 8U, &ref8, pred8, 8U) == AV2H_INVALID_ARGUMENT);
        mode.mrl_index = 4U;
        CHECK(av2h_predict_zone1_u16(&mode, 8U, 8U, &ref16, pred16, 8U, 0U, 1U, 45,
                                     &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_zone2_u16(&mode, 8U, 8U, &ref16, pred16, 8U, 0U, 1U, 135,
                                     &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_predict_zone3_u16(&mode, 8U, 8U, &ref16, pred16, 8U, 0U, 1U, 203,
                                     &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
    }

    free(pred8);
    free(pred16);
}

static void test_s3_odd_crop_cross_plane_and_lut_bounds(void) {
    static const uint16_t odd_dims[][2] = { {1U, 1U}, {3U, 3U}, {17U, 15U}, {63U, 65U} };
    size_t c;
    for (c = 0U; c < sizeof(odd_dims) / sizeof(odd_dims[0]); ++c) {
        uint16_t lw = odd_dims[c][0];
        uint16_t lh = odd_dims[c][1];
        uint16_t cw = (uint16_t)((lw + 1U) / 2U);
        uint16_t ch = (uint16_t)((lh + 1U) / 2U);
        size_t l_cells_stride = ((size_t)lw + 3U) / 4U;
        size_t l_cells = l_cells_stride * (((size_t)lh + 3U) / 4U);
        size_t c_cells_stride = ((size_t)cw + 3U) / 4U;
        size_t c_cells = c_cells_stride * (((size_t)ch + 3U) / 4U);
        uint8_t *l8 = (uint8_t *)malloc((size_t)lw * lh);
        uint8_t *c8 = (uint8_t *)malloc((size_t)cw * ch);
        uint16_t *l16 = (uint16_t *)malloc((size_t)lw * lh * sizeof(uint16_t));
        uint16_t *c16 = (uint16_t *)malloc((size_t)cw * ch * sizeof(uint16_t));
        uint16_t *c16_444 = (uint16_t *)malloc((size_t)lw * lh * sizeof(uint16_t));
        uint8_t *l_rec = (uint8_t *)malloc(l_cells);
        uint8_t *c_rec = (uint8_t *)malloc(c_cells);
        uint8_t pred8[16U * 16U];
        uint16_t pred16[16U * 16U];
        Av2hReconstructionPlane luma8 = { 0 };
        Av2hReconstructionPlane chroma8 = { 0 };
        Av2hReconstructionPlaneU16 luma16 = { 0 };
        Av2hReconstructionPlaneU16 chroma16 = { 0 };
        Av2hReconstructionPlaneU16 chroma16_444 = { 0 };
        uint8_t filter;
        uint8_t dir;
        size_t i;

        CHECK(l8 != NULL && c8 != NULL && l16 != NULL && c16 != NULL && c16_444 != NULL &&
              l_rec != NULL && c_rec != NULL);
        for (i = 0U; i < (size_t)lw * lh; ++i) {
            l8[i] = (uint8_t)((i * 31U + 7U) & 255U);
            l16[i] = (uint16_t)((i * 131U + 19U) & 1023U);
            c16_444[i] = (uint16_t)((i * 97U + 5U) & 1023U);
        }
        for (i = 0U; i < (size_t)cw * ch; ++i) {
            c8[i] = (uint8_t)((i * 43U + 13U) & 255U);
            c16[i] = (uint16_t)((i * 89U + 23U) & 1023U);
        }
        memset(l_rec, 1, l_cells);
        memset(c_rec, 1, c_cells);

        luma8.data = l8;
        luma8.stride_samples = lw;
        luma8.width = lw;
        luma8.height = lh;
        luma8.reference_width = lw;
        luma8.reference_height = lh;
        luma8.reconstructed = l_rec;
        luma8.reconstructed_stride_cells = l_cells_stride;
        luma8.reconstructed_count_cells = l_cells;

        chroma8.data = c8;
        chroma8.stride_samples = cw;
        chroma8.width = cw;
        chroma8.height = ch;
        chroma8.reference_width = cw;
        chroma8.reference_height = ch;
        chroma8.reconstructed = c_rec;
        chroma8.reconstructed_stride_cells = c_cells_stride;
        chroma8.reconstructed_count_cells = c_cells;

        luma16.data = l16;
        luma16.stride_samples = lw;
        luma16.width = lw;
        luma16.height = lh;
        luma16.reference_width = lw;
        luma16.reference_height = lh;
        luma16.reconstructed = l_rec;
        luma16.reconstructed_stride_cells = l_cells_stride;
        luma16.reconstructed_count_cells = l_cells;

        chroma16.data = c16;
        chroma16.stride_samples = cw;
        chroma16.width = cw;
        chroma16.height = ch;
        chroma16.reference_width = cw;
        chroma16.reference_height = ch;
        chroma16.reconstructed = c_rec;
        chroma16.reconstructed_stride_cells = c_cells_stride;
        chroma16.reconstructed_count_cells = c_cells;

        chroma16_444 = luma16;
        chroma16_444.data = c16_444;

        /* Boundary blocks straddling right/bottom edges of tight odd planes. */
        {
            uint16_t bx = cw > 2U ? 2U : 0U;
            uint16_t by = ch > 2U ? 2U : 0U;
            uint8_t have_left = (uint8_t)(bx >= 2U);
            uint8_t have_top = (uint8_t)(by >= 2U);
            for (filter = 0U; filter <= 2U; ++filter) {
                memset(pred8, 128, sizeof(pred8));
                for (i = 0U; i < 256U; ++i) pred16[i] = 512U;
                (void)av2h_cfl_derived_predict(&luma8, &chroma8, filter, 1U, 0, 64U, 0U,
                                               bx, by, 8U, 8U, have_top, have_left, pred8, 8U);
                (void)av2h_cfl_derived_predict_u16(&luma16, &chroma16, filter, 64U,
                                                   bx, by, 8U, 8U, have_top, have_left, pred16, 8U,
                                                   &av2h_sample_domain_10bit);
                (void)av2h_cfl_explicit_predict_u16(&luma16, filter, 12, 64U,
                                                    bx, by, 8U, 8U, have_top, have_left, pred16,
                                                    8U, &av2h_sample_domain_10bit);
                for (dir = 0U; dir <= 2U; ++dir) {
                    Av2hChromaLeafMode mode = { 0 };
                    mode.cfl_kind = AV2H_CFL_MULTI_PARAM;
                    mode.mhccp_direction = dir;
                    mode.cfl_downsample_filter_index = filter;
                    mode.superblock_size = 64U;
                    mode.chroma_format_idc = 0U;
                    (void)av2h_mhccp_predict_observed(NULL, &mode, &luma8, &chroma8,
                                                      bx, by, 8U, 8U, pred8, 8U);
                    (void)av2h_mhccp_predict_observed_u16(&mode, &luma16, &chroma16,
                                                          bx, by, 8U, 8U, pred16, 8U,
                                                          &av2h_sample_domain_10bit);
                    mode.chroma_format_idc = 2U;
                    (void)av2h_mhccp_predict_observed(NULL, &mode, &luma8, &luma8,
                                                      bx, by, 8U, 8U, pred8, 8U);
                    (void)av2h_mhccp_predict_observed_u16(&mode, &luma16, &chroma16_444,
                                                          bx, by, 8U, 8U, pred16, 8U,
                                                          &av2h_sample_domain_10bit);
                }
            }
        }

        free(l8);
        free(c8);
        free(l16);
        free(c16);
        free(c16_444);
        free(l_rec);
        free(c_rec);
    }

    /* CCSO LUT bounds under unclipped 0xFFFF luma samples and malformed tile bounds. */
    {
        uint16_t coded_w = av2h_filter_coded_dimension(17U);
        uint16_t coded_h = av2h_filter_coded_dimension(15U);
        uint16_t *y_buf = (uint16_t *)malloc((size_t)coded_w * coded_h * sizeof(uint16_t));
        uint16_t *pre_luma = (uint16_t *)malloc((size_t)coded_w * coded_h * sizeof(uint16_t));
        int8_t offsets[AV2H_CCSO_LUT_SIZE];
        uint8_t control_flag = 1U;
        Av2hFilterPlanesU16 planes = { 0 };
        Av2hSequenceState seq = { 0 };
        Av2hFrameState frame = { 0 };
        Av2hCcsoControlMap controls = { 0 };
        size_t i;

        CHECK(y_buf != NULL && pre_luma != NULL);
        for (i = 0U; i < (size_t)coded_w * coded_h; ++i) {
            y_buf[i] = 512U;
            pre_luma[i] = 0xFFFFU;
        }
        for (i = 0U; i < AV2H_CCSO_LUT_SIZE; ++i) {
            offsets[i] = 7;
        }
        planes.plane[0].data = y_buf;
        planes.plane[0].stride_samples = coded_w;
        planes.plane[0].width = coded_w;
        planes.plane[0].height = coded_h;
        seq.superblock_size = 64U;
        seq.disable_loopfilters_across_tiles = 1U;
        frame.width = 17U;
        frame.height = 15U;
        frame.plane_count = 1U;
        frame.chroma_format_idc = 1U;
        frame.tile_column_count = 255U;
        frame.tile_row_count = 255U;
        frame.tile_column_start_sb[0] = 4U;
        frame.tile_column_start_sb[1] = 4U;
        frame.ccso[0].enabled = 1U;
        frame.ccso[0].band_only = 1U;
        frame.ccso[0].max_band_log2 = 6U;
        frame.ccso[0].offset_count = 64U;
        frame.ccso[0].filter_offset = offsets;
        controls.columns = 1U;
        controls.rows = 1U;
        controls.plane[0] = &control_flag;
        controls.stride_units[0] = 1U;
        CHECK(av2h_ccso_filter_rows_u16(&planes, pre_luma, coded_w, &seq, &frame, &controls,
                                        0U, SIZE_MAX, &av2h_sample_domain_10bit) == AV2H_OK);
        CHECK(y_buf[0] == 519U);
        free(y_buf);
        free(pre_luma);
    }

    /* GDF side-threshold LUT clamping & Non-Separable Wiener chroma cross-plane validation. */
    CHECK(av2h_deblock_side_threshold(SIZE_MAX) == av2h_deblock_side_threshold(295U));
    {
        uint8_t c_buf[32U * 32U] = { 0 };
        uint8_t flag = AV2H_RESTORE_WIENER_NONSEP;
        int16_t taps[AV2H_WIENER_NONSEP_MAX_TAPS] = { 0 };
        Av2hFilterPlanes src = { {{NULL, 0U, 0U, 0U}, {c_buf, 32U, 32U, 32U}, {c_buf, 32U, 32U, 32U}} };
        Av2hFilterPlanes deb = src;
        Av2hFilterPlanes dst = src;
        Av2hSequenceState seq = { 0 };
        Av2hFrameState frame = { 0 };
        Av2hRestorationControlMap controls = { 0 };
        frame.plane_count = 3U;
        frame.chroma_format_idc = 0U;
        frame.restoration_type[1] = AV2H_RESTORE_WIENER_NONSEP;
        frame.restoration_num_filter_classes[1] = 1U;
        controls.unit_flags[1] = &flag;
        controls.unit_taps[1] = taps;
        controls.unit_columns[1] = 1U;
        controls.unit_rows[1] = 1U;
        controls.unit_size[1] = 32U;
        CHECK(av2h_wiener_nonsep_filter_rows(&dst, &src, &deb, &seq, &frame, &controls, 0U, 64U) ==
              AV2H_UNSUPPORTED);
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
    test_s3_deblock_tight_bounds();
    test_s3_directional_negative_prefix_and_helper_bounds();
    test_s3_odd_crop_cross_plane_and_lut_bounds();
    return failures != 0;
}
