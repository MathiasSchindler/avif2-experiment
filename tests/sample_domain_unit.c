#include "decoder/filter.h"
#include "decoder/internal/chroma_prediction.h"
#include "decoder/internal/reconstruct_block.h"
#include "decoder/internal/reconstruct_modules.h"
#include "decoder/internal/sample_ops.h"

#include <stdio.h>

#if defined(AV2H_KERNEL_WIDE) || defined(AV2H_KERNEL_TYPE) || defined(AV2H_KERNEL_NAME)
#error sample kernel configuration leaked out of its owner
#endif

static int failures;

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression);         \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void test_domains(void) {
    static const uint8_t depths[] = {8U, 10U, 12U};
    static const uint8_t idcs[] = {1U, 0U, 2U};
    unsigned int index;
    for (index = 0U; index < 3U; ++index) {
        const Av2hSampleDomain *domain = av2h_sample_domain_get(depths[index]);
        Av2hSampleDomain copy = *domain;
        CHECK(av2h_sample_domain_from_idc(idcs[index]) == domain);
        CHECK(av2h_sample_domain_valid(&copy));
        CHECK(domain->midpoint == (1U << (depths[index] - 1U)));
        CHECK(domain->maximum == (1U << depths[index]) - 1U);
        CHECK(av2h_sample_q3_maximum(domain) == (1U << (depths[index] + 3U)) - 1U);
        CHECK(av2h_sample_clip_to_domain(INT32_MIN, domain) == 0U);
        CHECK(av2h_sample_clip_to_domain(INT32_MAX, domain) == domain->maximum);
        CHECK(av2h_sample_clip_to_domain(domain->midpoint, domain) == domain->midpoint);
        ++copy.maximum;
        CHECK(!av2h_sample_domain_valid(&copy));
    }
    CHECK(av2h_sample_domain_get(9U) == NULL);
    CHECK(av2h_sample_domain_get(16U) == NULL);
    CHECK(av2h_sample_domain_from_idc(3U) == NULL);
    CHECK(!av2h_sample_domain_valid(NULL));
}

static void test_prediction(void) {
    static const Av2hIntraMode modes[] = {AV2H_INTRA_VERTICAL,
                                          AV2H_INTRA_HORIZONTAL,
                                          AV2H_INTRA_SMOOTH,
                                          AV2H_INTRA_SMOOTH_VERTICAL,
                                          AV2H_INTRA_SMOOTH_HORIZONTAL,
                                          AV2H_INTRA_PAETH,
                                          AV2H_INTRA_D45,
                                          AV2H_INTRA_D67,
                                          AV2H_INTRA_D113,
                                          AV2H_INTRA_D135,
                                          AV2H_INTRA_D157,
                                          AV2H_INTRA_D203};
    uint16_t edge[140U];
    uint16_t predictor[24U];
    Av2hLumaReferencesU16 references = {0};
    Av2hLeafMode mode = {0};
    size_t index;
    for (index = 0U; index < 140U; ++index)
        edge[index] = 3000U;
    references.above = edge;
    references.left = edge;
    references.above_count = 140U;
    references.left_count = 140U;
    references.above_left = 3000U;
    references.has_above_left = 1U;
    for (index = 0U; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        size_t sample;
        mode.mode = modes[index];
        av2h_plane_fill_u16(predictor, 6U, 6U, 4U, 0x5555U);
        CHECK(av2h_luma_predict_u16(&mode, 4U, 4U, &references, predictor, 6U,
                                    &av2h_sample_domain_12bit) == AV2H_OK);
        for (sample = 0U; sample < 24U; ++sample) {
            CHECK(predictor[sample] == (sample % 6U < 4U ? 3000U : 0x5555U));
        }
    }
    references = (Av2hLumaReferencesU16){0};
    mode.mode = AV2H_INTRA_VERTICAL;
    CHECK(av2h_luma_predict_u16(&mode, 4U, 4U, &references, predictor, 6U,
                                &av2h_sample_domain_12bit) == AV2H_OK);
    CHECK(predictor[0] == 2047U);
    mode.mode = AV2H_INTRA_HORIZONTAL;
    CHECK(av2h_luma_predict_u16(&mode, 4U, 4U, &references, predictor, 6U,
                                &av2h_sample_domain_12bit) == AV2H_OK);
    CHECK(predictor[0] == 2049U);
    CHECK(av2h_luma_predict_u16(&mode, 4U, 4U, &references, predictor, 6U, NULL) ==
          AV2H_INVALID_ARGUMENT);
    {
        Av2hSampleDomain invalid = {7U, 1U, 64U, 127U};
        CHECK(av2h_luma_predict_u16(&mode, 4U, 4U, &references, predictor, 6U, &invalid) ==
              AV2H_INVALID_ARGUMENT);
    }
}

static void test_cfl(void) {
    uint16_t luma[64U];
    uint16_t predictor[16U];
    uint8_t ready[4U] = {1U, 1U, 1U, 1U};
    Av2hReconstructionPlaneU16 plane = {luma, 8U, 8U, 8U, 8U, 8U, ready, 2U, 4U};
    unsigned int row;
    unsigned int column;
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            luma[row * 8U + column] = (uint16_t)(2000U + 32U * (row / 2U * 4U + column / 2U));
        }
    }
    av2h_plane_fill_u16(predictor, 4U, 4U, 4U, 2048U);
    CHECK(av2h_cfl_explicit_predict_u16(&plane, 0U, 8, 64U, 0U, 0U, 4U, 4U, 0U, 0U, predictor, 4U,
                                        &av2h_sample_domain_12bit) == AV2H_OK);
    for (row = 0U; row < 16U; ++row)
        CHECK(predictor[row] == 2000U + 32U * row);
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            luma[row * 8U + column] = (uint16_t)(2000U + 32U * (row * 4U + column));
        }
    }
    av2h_plane_fill_u16(predictor, 4U, 4U, 4U, 2048U);
    CHECK(av2h_cfl_explicit_predict_u16_444(&plane, 8, 0U, 0U, 4U, 4U, 0U, 0U, predictor, 4U,
                                            &av2h_sample_domain_12bit) == AV2H_OK);
    for (row = 0U; row < 16U; ++row)
        CHECK(predictor[row] == 2000U + 32U * row);
    {
        uint16_t samples[256U];
        uint8_t map[16U];
        Av2hReconstructionPlaneU16 neighbours = {samples, 16U, 16U, 16U,        16U,
                                                 16U,     map, 4U,  sizeof(map)};
        av2h_memory_fill(map, 1U, sizeof(map));
        av2h_plane_fill_u16(samples, 16U, 16U, 16U, 2500U);
        av2h_plane_fill_u16(samples + 4U * 16U + 4U, 16U, 8U, 8U, 3000U);
        av2h_plane_fill_u16(predictor, 4U, 4U, 4U, 2048U);
        CHECK(av2h_cfl_explicit_predict_u16(&neighbours, 0U, 8, 64U, 2U, 2U, 4U, 4U, 1U, 1U,
                                            predictor, 4U, &av2h_sample_domain_12bit) == AV2H_OK);
        for (row = 0U; row < 16U; ++row)
            CHECK(predictor[row] == 2548U);
        av2h_plane_fill_u16(predictor, 4U, 4U, 4U, 2048U);
        CHECK(av2h_cfl_explicit_predict_u16_444(&neighbours, 8, 4U, 4U, 4U, 4U, 1U, 1U, predictor,
                                                4U, &av2h_sample_domain_12bit) == AV2H_OK);
        for (row = 0U; row < 16U; ++row)
            CHECK(predictor[row] == 2548U);
    }
}

static void test_cfl_filter_boundaries(void) {
    static uint8_t luma[128U * 128U];
    static uint16_t luma_u16[128U * 128U];
    static uint8_t ready[32U * 32U];
    static uint8_t predictor[67U * 64U];
    static uint16_t predictor_u16[67U * 64U];
    Av2hReconstructionPlane plane = {.data = luma,
                                     .stride_samples = 128U,
                                     .width = 128U,
                                     .height = 128U,
                                     .reference_width = 128U,
                                     .reference_height = 128U,
                                     .reconstructed = ready,
                                     .reconstructed_stride_cells = 32U,
                                     .reconstructed_count_cells = sizeof(ready)};
    Av2hReconstructionPlaneU16 plane_u16 = {.data = luma_u16,
                                            .stride_samples = 128U,
                                            .width = 128U,
                                            .height = 128U,
                                            .reference_width = 128U,
                                            .reference_height = 128U,
                                            .reconstructed = ready,
                                            .reconstructed_stride_cells = 32U,
                                            .reconstructed_count_cells = sizeof(ready)};
    const Av2hSampleDomain *domains[] = {&av2h_sample_domain_10bit, &av2h_sample_domain_12bit};
    size_t depth;
    size_t index;
    uint8_t filter;
    av2h_memory_fill(ready, 1U, sizeof(ready));
    for (index = 0U; index < sizeof(luma); ++index) {
        size_t row = index / 128U;
        size_t column = index % 128U;
        luma[index] = (uint8_t)((7U * row + 13U * column + row * column % 17U) & 255U);
    }
    for (depth = 0U; depth < sizeof(domains) / sizeof(domains[0]); ++depth) {
        const Av2hSampleDomain *domain = domains[depth];
        int32_t scale = (int32_t)1 << (domain->bit_depth - 8U);
        for (index = 0U; index < sizeof(luma); ++index)
            luma_u16[index] = (uint16_t)(luma[index] * scale);
        for (filter = 0U; filter < 3U; ++filter) {
            av2h_memory_fill(predictor, 0xD6, sizeof(predictor));
            av2h_plane_fill_u16(predictor_u16, 67U, 67U, 64U, 0xD6D6);
            av2h_plane_fill_u8(predictor, 67U, 64U, 64U, 128U);
            av2h_plane_fill_u16(predictor_u16, 67U, 64U, 64U, domain->midpoint);
            CHECK(av2h_cfl_derived_predict(&plane, &plane, filter, 0U, 8, 64U, 0U, 0U, 0U, 64U, 64U,
                                           0U, 0U, predictor, 67U) == AV2H_OK);
            CHECK(av2h_cfl_explicit_predict_u16(&plane_u16, filter, 8, 64U, 0U, 0U, 64U, 64U, 0U,
                                                0U, predictor_u16, 67U, domain) == AV2H_OK);
            for (index = 0U; index < 64U * 64U; ++index) {
                size_t row = index / 64U;
                size_t column = index % 64U;
                size_t x = column * 2U;
                size_t y = row * 2U;
                size_t left = (column & 31U) == 0U ? x : x - 1U;
                size_t top = (row & 31U) == 0U ? y : y - 1U;
                int32_t q3 =
                    filter == 0U ? 2 * (luma[y * 128U + x] + luma[y * 128U + x + 1U] +
                                        luma[(y + 1U) * 128U + x] + luma[(y + 1U) * 128U + x + 1U])
                    : filter == 1U
                        ? luma[y * 128U + left] + 2 * luma[y * 128U + x] + luma[y * 128U + x + 1U] +
                              luma[(y + 1U) * 128U + left] + 2 * luma[(y + 1U) * 128U + x] +
                              luma[(y + 1U) * 128U + x + 1U]
                        : luma[y * 128U + left] + 4 * luma[y * 128U + x] + luma[y * 128U + x + 1U] +
                              luma[top * 128U + x] + luma[(y + 1U) * 128U + x];
                int32_t correction = q3 - 1024;
                int32_t correction_u16 = correction * scale;
                int32_t expected =
                    128 + (correction < 0 ? -((-correction + 4) / 8) : (correction + 4) / 8);
                int32_t expected_u16 =
                    domain->midpoint +
                    (correction_u16 < 0 ? -((-correction_u16 + 4) / 8) : (correction_u16 + 4) / 8);
                CHECK(predictor[row * 67U + column] == expected);
                CHECK(predictor_u16[row * 67U + column] == expected_u16);
                CHECK(predictor[row * 67U + 64U] == 0xD6);
                CHECK(predictor_u16[row * 67U + 64U] == 0xD6D6);
            }
        }
    }
}

static void test_reconstruction(void) {
    static Av2hReconstructionScratchU16 scratch;
    static Av2hBlockReconstructionU16 reconstruction;
    Av2hLeafMode mode = {0};
    Av2hCoeffPrefix prefix = {0};
    Av2hTransformType transform = {0};
    Av2hCoeffBlock coefficients = {0};
    uint16_t predictor[16U];
    size_t index;
    mode.mode = AV2H_INTRA_DC;
    mode.use_dpcm_y = 1U;
    mode.fsc_mode = 1U;
    prefix.tx_size = AV2H_TX_4X4;
    prefix.q_index = 1U;
    transform.primary_type = AV2H_TX_IDTX;
    coefficients.width = 4U;
    coefficients.height = 4U;
    coefficients.coefficient_count = 16U;
    for (index = 0U; index < 16U; ++index) {
        predictor[index] = 4090U;
        coefficients.dequantized[index] = index < 4U ? 128 : -128;
    }
    CHECK(av2h_luma_reconstruct_u16(NULL, &mode, &prefix, &transform, &coefficients, predictor,
                                    &scratch, &reconstruction,
                                    &av2h_sample_domain_12bit) == AV2H_OK);
    for (index = 0U; index < 16U; ++index) {
        int32_t expected = 16 - 16 * (int32_t)(index / 4U);
        CHECK(reconstruction.residual[index] == expected);
        CHECK(reconstruction.pixels[index] == (index < 4U ? 4095U : (uint16_t)(4090 + expected)));
    }
    {
        int32_t residual[16U];
        mode.use_dpcm_y = 0U;
        mode.fsc_mode = 0U;
        transform.primary_type = AV2H_TX_DCT_DCT;
        av2h_memory_fill(&coefficients, 0U, sizeof(coefficients));
        coefficients.width = 4U;
        coefficients.height = 4U;
        coefficients.coefficient_count = 16U;
        coefficients.nonzero_count = 1U;
        coefficients.quantized[0] = 1;
        coefficients.dequantized[0] = 4096;
        prefix.eob = 1U;
        av2h_plane_fill_u16(predictor, 4U, 4U, 4U, 512U);
        CHECK(av2h_reconstruct_transform_generic_u16(
                  NULL, &mode, &prefix, &transform, &coefficients, predictor, 4U, &scratch,
                  &reconstruction, &av2h_sample_domain_10bit) == AV2H_OK);
        av2h_memory_copy(residual, reconstruction.residual, sizeof(residual));
        av2h_plane_fill_u16(predictor, 4U, 4U, 4U, 2048U);
        CHECK(av2h_reconstruct_transform_generic_u16(
                  NULL, &mode, &prefix, &transform, &coefficients, predictor, 4U, &scratch,
                  &reconstruction, &av2h_sample_domain_12bit) == AV2H_OK);
        CHECK(residual[0] > 0 && residual[0] < 2048);
        for (index = 0U; index < 16U; ++index) {
            CHECK(reconstruction.residual[index] == residual[index]);
            CHECK(reconstruction.pixels[index] == 2048 + residual[index]);
        }
    }
}

static void test_filters(void) {
    uint8_t low[64U];
    uint16_t high[64U];
    uint16_t output[64U];
    uint16_t baseline[64U];
    Av2hFilterPlane low_plane = {low, 8U, 8U, 8U};
    Av2hFilterPlaneU16 high_plane = {high, 8U, 8U, 8U};
    Av2hFilterPlaneU16 destination = {output, 8U, 8U, 8U};
    Av2hFilterPlaneU16 deblocked = {baseline, 8U, 8U, 8U};
    uint8_t direction;
    uint8_t direction_high;
    uint32_t variance;
    uint32_t variance_high;
    Av2hFrameState frame = {0};
    size_t index;
    for (index = 0U; index < 64U; ++index) {
        low[index] = (uint8_t)(index * 17U + index / 8U * 29U);
        high[index] = (uint16_t)low[index] << 4U;
    }
    CHECK(av2h_cdef_find_direction(&low_plane, 0U, 0U, &direction, &variance) == AV2H_OK);
    CHECK(av2h_cdef_find_direction_u16(&high_plane, 0U, 0U, &direction_high, &variance_high,
                                       &av2h_sample_domain_12bit) == AV2H_OK);
    CHECK(direction_high == direction && variance_high == variance);
    av2h_plane_fill_u16(high, 8U, 8U, 8U, 3000U);
    CHECK(av2h_cdef_filter_block_u16(&destination, &high_plane, 0U, 0U, 8U, 8U, 0U, 5U, 2U, 5U, 2U,
                                     100U, &av2h_sample_domain_12bit) == AV2H_OK);
    for (index = 0U; index < 64U; ++index)
        CHECK(output[index] == 3000U);
    CHECK(av2h_deblock_edge_u16(&high_plane, 4U, 4U, 1U, 4U, 4U, 150U, 50U,
                                &av2h_sample_domain_12bit) == AV2H_OK);
    for (index = 0U; index < 64U; ++index)
        CHECK(high[index] == 3000U);
    frame.gdf_mode = 1U;
    frame.gdf_qp_index = 3U;
    frame.gdf_scale_index = 3U;
    av2h_plane_copy_u16(baseline, 8U, high, 8U, 8U, 8U);
    CHECK(av2h_gdf_filter_frame_u16(&destination, &high_plane, &deblocked, NULL, &frame, NULL,
                                    &av2h_sample_domain_12bit) == AV2H_OK);
    for (index = 0U; index < 64U; ++index)
        CHECK(output[index] == 3000U);
    {
        uint16_t u[16U];
        uint16_t v[16U];
        uint8_t control = 1U;
        Av2hCcsoControlMap controls = {0};
        Av2hFilterPlanesU16 planes = {0};
        frame = (Av2hFrameState){0};
        frame.width = 8U;
        frame.height = 8U;
        frame.ccso[1].enabled = 1U;
        frame.ccso[1].band_only = 1U;
        frame.ccso[1].max_band_log2 = 1U;
        frame.ccso[1].offset_count = 2U;
        frame.ccso[1].filter_offset[16U] = 3;
        controls.plane[1] = &control;
        controls.stride_units[1] = 1U;
        controls.columns = 1U;
        controls.rows = 1U;
        planes.plane[0] = high_plane;
        planes.plane[1] = (Av2hFilterPlaneU16){u, 4U, 4U, 4U};
        planes.plane[2] = (Av2hFilterPlaneU16){v, 4U, 4U, 4U};
        av2h_plane_fill_u16(baseline, 8U, 8U, 8U, 2300U);
        av2h_plane_fill_u16(u, 4U, 4U, 4U, 2000U);
        av2h_plane_fill_u16(v, 4U, 4U, 4U, 3000U);
        u[0] = 4094U;
        CHECK(av2h_ccso_filter_frame_u16(&planes, baseline, 8U, NULL, &frame, &controls,
                                         &av2h_sample_domain_12bit) == AV2H_OK);
        CHECK(u[0] == 4095U);
        for (index = 1U; index < 16U; ++index)
            CHECK(u[index] == 2003U);
    }
}

static void test_gdf_depth_scaling(void) {
    uint16_t source[2U][80U];
    uint16_t baseline[2U][80U];
    uint16_t output[2U][80U];
    Av2hFrameState frame = {0};
    unsigned int qp_index;
    unsigned int row;
    unsigned int column;
    frame.width = 8U;
    frame.height = 8U;
    frame.base_q_index = 100U;
    frame.gdf_mode = 1U;
    frame.gdf_scale_index = 3U;
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 10U; ++column) {
            size_t index = (size_t)row * 10U + column;
            source[0U][index] =
                (uint16_t)(400U + (column * 17U + row * 29U + column * row * 7U) % 200U);
            source[1U][index] = (uint16_t)(source[0U][index] * 4U);
            baseline[0U][index] = source[0U][index];
            baseline[1U][index] = source[1U][index];
        }
    }
    for (qp_index = 0U; qp_index < 4U; ++qp_index) {
        unsigned int depth_index;
        unsigned int corrected_samples = 0U;
        frame.gdf_qp_index = (uint8_t)qp_index;
        for (depth_index = 0U; depth_index < 2U; ++depth_index) {
            Av2hFilterPlaneU16 destination = {output[depth_index], 10U, 8U, 8U};
            Av2hFilterPlaneU16 cdef = {source[depth_index], 10U, 8U, 8U};
            Av2hFilterPlaneU16 deblocked = {baseline[depth_index], 10U, 8U, 8U};
            av2h_plane_fill_u16(output[depth_index], 10U, 10U, 8U, 0x5555U);
            CHECK(av2h_gdf_filter_frame_u16(
                      &destination, &cdef, &deblocked, NULL, &frame, NULL,
                      av2h_sample_domain_get(depth_index == 0U ? 10U : 12U)) == AV2H_OK);
        }
        for (row = 0U; row < 8U; ++row) {
            for (column = 0U; column < 10U; ++column) {
                size_t index = (size_t)row * 10U + column;
                if (column < 8U) {
                    CHECK(output[1U][index] == output[0U][index] * 4U);
                    if (output[0U][index] != source[0U][index])
                        ++corrected_samples;
                } else {
                    CHECK(output[0U][index] == 0x5555U);
                    CHECK(output[1U][index] == 0x5555U);
                }
            }
        }
        CHECK(corrected_samples != 0U);
    }
}

static void test_mhccp_extreme(void) {
    static uint16_t luma[192U * 192U];
    static uint16_t chroma[192U * 192U];
    static uint8_t ready[48U * 48U];
    static uint16_t predictor[64U * 64U];
    Av2hReconstructionPlaneU16 luma_plane = {luma, 192U,  192U, 192U,         192U,
                                             192U, ready, 48U,  sizeof(ready)};
    Av2hReconstructionPlaneU16 chroma_plane = luma_plane;
    Av2hChromaLeafMode mode = {0};
    size_t index;
    chroma_plane.data = chroma;
    mode.cfl_kind = AV2H_CFL_MULTI_PARAM;
    mode.chroma_format_idc = 2U;
    mode.superblock_size = 128U;
    av2h_plane_fill_u16(luma, 192U, 192U, 192U, 4095U);
    av2h_plane_fill_u16(chroma, 192U, 192U, 192U, 4095U);
    av2h_memory_fill(ready, 1U, sizeof(ready));
    CHECK(av2h_mhccp_predict_observed_u16(&mode, &luma_plane, &chroma_plane, 64U, 64U, 64U, 64U,
                                          predictor, 64U, &av2h_sample_domain_12bit) == AV2H_OK);
    for (index = 0U; index < 4096U; ++index) {
        CHECK(predictor[index] >= 4090U && predictor[index] <= 4095U);
    }
}

int main(void) {
    test_domains();
    test_prediction();
    test_cfl();
    test_cfl_filter_boundaries();
    test_reconstruction();
    test_filters();
    test_gdf_depth_scaling();
    test_mhccp_extreme();
    if (failures != 0)
        return 1;
    puts("sample domain unit: pass");
    return 0;
}
