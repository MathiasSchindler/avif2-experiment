#include "decoder/internal/leaf_modules.h"
#include "decoder/internal/reconstruct_block.h"
#include "decoder/internal/reconstruct_modules.h"
#include "decoder/tx_geometry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static uint16_t clip(int32_t value, int32_t maximum) {
    return (uint16_t)(value < 0 ? 0 : value > maximum ? maximum : value);
}

static void test_specialized_wide_transform_policies(void) {
    typedef Av2hStatus (*WideTransform)(
        const Av2hLeafMode *, const Av2hCoeffPrefix *, const Av2hTransformType *,
        const Av2hCoeffBlock *, const uint16_t *, size_t, Av2hReconstructionScratchU16 *,
        Av2hBlockReconstructionU16 *, const Av2hSampleDomain *);
    static const WideTransform transforms[] = {
        av2h_reconstruct_transform_8x16_u16, av2h_reconstruct_transform_8x4_u16
    };
    const Av2hSampleDomain *domains[] = {
        &av2h_sample_domain_8bit, &av2h_sample_domain_10bit, &av2h_sample_domain_12bit
    };
    static Av2hReconstructionScratchU16 scratch;
    static Av2hReconstructionScratchU16 generic_scratch;
    static Av2hBlockReconstructionU16 output;
    static Av2hBlockReconstructionU16 generic_output;
    uint16_t predictor[11U * 16U];
    size_t geometry;
    size_t depth;
    size_t index;
    for (index = 0U; index < sizeof(predictor) / sizeof(predictor[0]); ++index)
        predictor[index] = (uint16_t)((index * 7U) & 255U);
    for (geometry = 0U; geometry < 2U; ++geometry) {
        WideTransform reconstruct = transforms[geometry];
        for (depth = 0U; depth < 3U; ++depth) {
            Av2hLeafMode mode = {0};
            Av2hCoeffPrefix prefix = {0};
            Av2hTransformType transform = {0};
            Av2hCoeffBlock coefficients = {0};
            int32_t scaled[AV2H_COEFF_CAPACITY] = {0};
            size_t count = geometry == 0U ? 128U : 32U;
            unsigned int argument;
            prefix.tx_size = geometry == 0U ? AV2H_TX_8X16 : AV2H_TX_8X4;
            coefficients.width = 8U;
            coefficients.height = geometry == 0U ? 16U : 4U;
            coefficients.coefficient_count = (uint16_t)count;
            for (index = 0U; index < count; ++index)
                coefficients.dequantized[index] = (int32_t)(index * 53U) - 900;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) == AV2H_OK);
            CHECK(av2h_reconstruct_transform_generic_u16(
                      NULL, &mode, &prefix, &transform, &coefficients, predictor, 11U,
                      &generic_scratch, &generic_output, domains[depth]) == AV2H_OK);
            CHECK(output.width == 8U && output.height == coefficients.height);
            for (index = 0U; index < count; ++index) {
                CHECK(output.residual[index] == generic_output.residual[index]);
                CHECK(output.pixels[index] == generic_output.pixels[index]);
            }
            for (index = 0U; index < count; ++index) {
                coefficients.dequantized[index] = index & 1U ? 300000 : -300001;
                scaled[index] = index & 1U ? 131071 : -131072;
            }
            transform.secondary_set = 8U;
            transform.secondary_symbol = 1U;
            CHECK(av2h_inverse_secondary_transform(
                      &mode, &prefix, &transform, &coefficients, scaled) == AV2H_OK);
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) == AV2H_OK);
            for (index = 0U; index < count; ++index) CHECK(scratch.scaled[index] == scaled[index]);
            transform.secondary_symbol = 0U;
            mode.mrl_index = mode.use_intrabc = mode.fsc_mode = mode.palette_size = 1U;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) ==
                  (geometry == 0U ? AV2H_UNSUPPORTED : AV2H_OK));
            memset(&mode, 0, sizeof(mode));
            mode.skip_transform = 1U;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) == AV2H_OK);
            mode.skip_transform = 0U;
            transform.secondary_signaled = 1U;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) == AV2H_UNSUPPORTED);
            transform.secondary_signaled = 0U;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 7U,
                              &scratch, &output, domains[depth]) == AV2H_UNSUPPORTED);
            for (argument = 0U; argument < 8U; ++argument) {
                CHECK(reconstruct(
                          argument == 0U ? NULL : &mode, argument == 1U ? NULL : &prefix,
                          argument == 2U ? NULL : &transform, argument == 3U ? NULL : &coefficients,
                          argument == 4U ? NULL : predictor, 11U,
                          argument == 5U ? NULL : &scratch, argument == 6U ? NULL : &output,
                          argument == 7U ? NULL : domains[depth]) == AV2H_INVALID_ARGUMENT);
            }
            memset(coefficients.dequantized, 0, sizeof(coefficients.dequantized));
            prefix.all_zero = 1U;
            coefficients.quantized[17U] = 1;
            memset(&output, 0x55, sizeof(output));
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) ==
                  (geometry == 0U ? AV2H_INVALID_DATA : AV2H_OK));
            for (index = 0U; index < count; ++index) {
                if (geometry == 0U && index >= 17U) {
                    CHECK(output.residual[index] == 0x55555555);
                    CHECK(output.pixels[index] == 0x5555U);
                } else {
                    CHECK(output.residual[index] == 0);
                    CHECK(output.pixels[index] == predictor[(index / 8U) * 11U + index % 8U]);
                }
            }
            coefficients.quantized[17U] = 0;
            prefix.eob = 1U;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) ==
                  (geometry == 0U ? AV2H_INVALID_DATA : AV2H_OK));
            prefix.eob = 0U;
            coefficients.nonzero_count = 1U;
            CHECK(reconstruct(&mode, &prefix, &transform, &coefficients, predictor, 11U,
                              &scratch, &output, domains[depth]) ==
                  (geometry == 0U ? AV2H_INVALID_DATA : AV2H_OK));
        }
    }
}

static void test_zero_geometry(void) {
    static uint8_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    static uint16_t predictor_u16[AV2H_RECON_SAMPLE_CAPACITY];
    static Av2hReconstructionScratch scratch;
    static Av2hReconstructionScratchU16 scratch_u16;
    static Av2hBlockReconstruction output;
    static Av2hBlockReconstructionU16 output_u16;
    Av2hLeafMode mode = { 0 };
    Av2hChromaLeafMode chroma = { 0 };
    Av2hTransformType transform = { 0 };
    unsigned int tx;
    size_t index;
    for (index = 0U; index < AV2H_RECON_SAMPLE_CAPACITY; ++index) {
        predictor[index] = (uint8_t)(index * 37U);
        predictor_u16[index] = (uint16_t)((index * 37U) & 1023U);
    }
    for (tx = 0U; tx < AV2H_TX_SIZE_COUNT; ++tx) {
        const Av2hTxDescriptor *descriptor;
        Av2hCoeffPrefix prefix = { 0 };
        Av2hCoeffBlock coefficients = { 0 };
        unsigned int path;
        size_t count;
        CHECK(av2h_tx_descriptor_get((Av2hTxSize)tx, &descriptor) ==
              AV2H_OK);
        prefix.tx_size = (Av2hTxSize)tx;
        prefix.q_index = 152U;
        prefix.all_zero = 1U;
        coefficients.width = descriptor->coefficient_width;
        coefficients.height = descriptor->coefficient_height;
        coefficients.coefficient_count = descriptor->eob_capacity;
        count = (size_t)descriptor->output_width * descriptor->output_height;
        for (path = 0U; path < 4U; ++path) {
            Av2hStatus status;
            memset(&output, 0x7d, sizeof(output));
            memset(&output_u16, 0x7d, sizeof(output_u16));
            if (path == 0U) {
                status = av2h_luma_reconstruct(
                    NULL, &mode, &prefix, &transform, &coefficients, predictor,
                    descriptor->output_width, &scratch, &output);
            } else if (path == 1U) {
                status = av2h_luma_reconstruct_u16(
                    0, &mode, &prefix, &transform, &coefficients, predictor_u16,
                    &scratch_u16, &output_u16, &av2h_sample_domain_10bit);
            } else if (path == 2U) {
                status = av2h_chroma_reconstruct(
                    0, &chroma, &prefix, &transform, &coefficients, predictor,
                    descriptor->output_width, descriptor->output_height,
                    &scratch, &output);
            } else {
                status = av2h_chroma_reconstruct_u16(
                    0, &chroma, &prefix, &transform, &coefficients, predictor_u16,
                    &scratch_u16, &output_u16, &(Av2hLeafMode){0}, &av2h_sample_domain_10bit);
            }
            CHECK(status == AV2H_OK);
            CHECK((path & 1U ? output_u16.width : output.width) ==
                  descriptor->output_width);
            CHECK((path & 1U ? output_u16.height : output.height) ==
                  descriptor->output_height);
            for (index = 0U; index < AV2H_RECON_SAMPLE_CAPACITY; ++index) {
                if (path & 1U) {
                    CHECK(output_u16.pixels[index] ==
                          (index < count ? predictor_u16[index] : 0x7d7dU));
                    CHECK(output_u16.residual[index] ==
                          (index < count ? 0 : 0x7d7d7d7d));
                } else {
                    CHECK(output.pixels[index] ==
                          (index < count ? predictor[index] : 0x7dU));
                    CHECK(output.residual[index] ==
                          (index < count ? 0 : 0x7d7d));
                }
            }
        }
    }
}

static void test_dpcm_rounding_and_clipping(void) {
    static const int32_t values[] = { -17, 16, 12000, -12000 };
    Av2hLeafMode mode = { 0 };
    Av2hCoeffPrefix prefix = { 0 };
    Av2hTransformType transform = { 0 };
    Av2hCoeffBlock coefficients = { 0 };
    static Av2hReconstructionScratch scratch;
    static Av2hReconstructionScratchU16 scratch_u16;
    static Av2hBlockReconstruction output;
    static Av2hBlockReconstructionU16 output_u16;
    uint8_t predictor[64U];
    uint16_t predictor_u16[64U];
    unsigned int horizontal;
    size_t index;
    mode.use_dpcm_y = 1U;
    mode.fsc_mode = 1U;
    prefix.tx_size = AV2H_TX_8X8;
    transform.primary_type = AV2H_TX_IDTX;
    coefficients.width = 8U;
    coefficients.height = 8U;
    coefficients.coefficient_count = 64U;
    for (index = 0U; index < 64U; ++index) {
        coefficients.dequantized[index] = values[index % 4U];
        predictor[index] = index & 1U ? 255U : 0U;
        predictor_u16[index] = index & 1U ? 1023U : 0U;
    }
    for (horizontal = 0U; horizontal < 2U; ++horizontal) {
        int32_t residual[64U] = { 0 };
        int32_t residual_u16[64U] = { 0 };
        mode.dpcm_mode_y = (uint8_t)horizontal;
        CHECK(av2h_luma_reconstruct(
            NULL, &mode, &prefix, &transform, &coefficients, predictor, 8U,
            &scratch, &output) == AV2H_OK);
        CHECK(av2h_luma_reconstruct_u16(
                  0, &mode, &prefix, &transform, &coefficients, predictor_u16,
                  &scratch_u16, &output_u16, &av2h_sample_domain_10bit) == AV2H_OK);
        for (index = 0U; index < 64U; ++index) {
            int32_t value = coefficients.dequantized[index];
            size_t step = horizontal ? 1U : 8U;
            unsigned int have_previous = horizontal ?
                index % 8U != 0U : index >= 8U;
            residual[index] = value >= 0 ? value / 8 : -((-value + 7) / 8);
            residual_u16[index] = value / 8;
            if (have_previous) {
                residual[index] += residual[index - step];
                residual_u16[index] += residual_u16[index - step];
            }
            CHECK(output.residual[index] == residual[index]);
            CHECK(output_u16.residual[index] == residual_u16[index]);
            CHECK(output.pixels[index] ==
                  clip(predictor[index] + residual[index], 255));
            CHECK(output_u16.pixels[index] ==
                  clip(predictor_u16[index] + residual_u16[index], 1023));
        }
    }
}

static void test_lossless_chroma_and_rejection(void) {
    Av2hChromaLeafMode mode = { 0 };
    Av2hCoeffPrefix prefix = { 0 };
    Av2hTransformType transform = { 0 };
    Av2hCoeffBlock coefficients = { 0 };
    static Av2hReconstructionScratch scratch;
    static Av2hReconstructionScratchU16 scratch_u16;
    static Av2hBlockReconstruction output;
    static Av2hBlockReconstructionU16 output_u16;
    static Av2hBlockReconstruction original;
    static Av2hBlockReconstructionU16 original_u16;
    uint8_t predictor[16U];
    uint16_t predictor_u16[16U];
    size_t index;
    memset(predictor, 128, sizeof(predictor));
    for (index = 0U; index < 16U; ++index) predictor_u16[index] = 512U;
    mode.use_dpcm_uv = 1U;
    mode.dpcm_mode_uv = 1U;
    prefix.tx_size = AV2H_TX_4X4;
    transform.primary_type = AV2H_TX_IDTX;
    coefficients.width = 4U;
    coefficients.height = 4U;
    coefficients.coefficient_count = 16U;
    coefficients.dequantized[0] = -17;
    CHECK(av2h_chroma_reconstruct(
              0, &mode, &prefix, &transform, &coefficients, predictor, 4U, 4U,
              &scratch, &output) == AV2H_OK);
    CHECK(av2h_chroma_reconstruct_u16(
              0, &mode, &prefix, &transform, &coefficients, predictor_u16,
              &scratch_u16, &output_u16, &(Av2hLeafMode){0}, &av2h_sample_domain_10bit) == AV2H_OK);
    for (index = 0U; index < 16U; ++index) {
        CHECK(output.residual[index] == (index < 4U ? -3 : 0));
        CHECK(output_u16.residual[index] == (index < 4U ? -3 : 0));
        CHECK(output.pixels[index] == (index < 4U ? 125U : 128U));
        CHECK(output_u16.pixels[index] == (index < 4U ? 509U : 512U));
    }
    prefix.all_zero = 1U;
    prefix.q_index = 152U;
    memset(&output, 0x7d, sizeof(output));
    memset(&output_u16, 0x7d, sizeof(output_u16));
    original = output;
    original_u16 = output_u16;
    CHECK(av2h_chroma_reconstruct(
              0, &mode, &prefix, &transform, &coefficients, predictor, 4U, 4U,
              &scratch, &output) == AV2H_UNSUPPORTED);
    CHECK(memcmp(&output, &original, sizeof(output)) == 0);
    coefficients.dequantized[0] = 0;
    coefficients.coefficient_count = 15U;
    CHECK(av2h_chroma_reconstruct_u16(
              0, &mode, &prefix, &transform, &coefficients, predictor_u16,
              &scratch_u16, &output_u16, &(Av2hLeafMode){0}, &av2h_sample_domain_10bit) == AV2H_UNSUPPORTED);
    CHECK(memcmp(&output_u16, &original_u16, sizeof(output_u16)) == 0);
    CHECK(av2h_chroma_reconstruct_u16(
              0, &mode, &prefix, &transform, &coefficients, predictor_u16,
              &scratch_u16, &output_u16, NULL, &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
    CHECK(memcmp(&output_u16, &original_u16, sizeof(output_u16)) == 0);
    CHECK(av2h_luma_reconstruct_u16(
              0, 0, &prefix, &transform, &coefficients, predictor_u16,
              &scratch_u16, &output_u16, &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_chroma_reconstruct(
              0, &mode, &prefix, &transform, &coefficients, predictor, 0U, 4U,
              &scratch, &output) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_chroma_reconstruct_u16(
              0, &mode, &prefix, &transform, &coefficients, 0,
              &scratch_u16, &output_u16, &(Av2hLeafMode){0}, &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
}

static void test_dpcm_scale_and_stride(void) {
    static const Av2hTxSize sizes[] = {AV2H_TX_16X16, AV2H_TX_16X32, AV2H_TX_32X32};
    static Av2hCoeffBlock coefficients;
    static Av2hReconstructionScratch scratch;
    static Av2hReconstructionScratchU16 scratch_u16;
    static Av2hBlockReconstruction output;
    static Av2hBlockReconstructionU16 output_u16;
    static uint8_t predictor[35U * 32U];
    static uint16_t predictor_u16[1024U];
    int32_t expected[1024U];
    Av2hLeafMode mode = {0};
    Av2hCoeffPrefix prefix = {0};
    Av2hTransformType transform = {0};
    size_t geometry;
    mode.use_dpcm_y = 1U;
    mode.fsc_mode = 1U;
    transform.primary_type = AV2H_TX_IDTX;
    for (geometry = 0U; geometry < sizeof(sizes) / sizeof(sizes[0]); ++geometry) {
        const Av2hTxDescriptor *descriptor;
        size_t width;
        size_t height;
        size_t stride;
        size_t index;
        unsigned int horizontal;
        int32_t divisor = geometry == 0U ? 8 : 4;
        CHECK(av2h_tx_descriptor_get(sizes[geometry], &descriptor) == AV2H_OK);
        prefix.tx_size = sizes[geometry];
        width = descriptor->output_width;
        height = descriptor->output_height;
        stride = width + 3U;
        coefficients.width = (uint16_t)width;
        coefficients.height = (uint16_t)height;
        coefficients.coefficient_count = width * height;
        memset(predictor, 0xD6, sizeof(predictor));
        for (index = 0U; index < width * height; ++index) {
            coefficients.dequantized[index] = index % 3U == 0U ? -17 : 16;
            predictor[index / width * stride + index % width] = 128U;
            predictor_u16[index] = 2048U;
        }
        for (horizontal = 0U; horizontal < 2U; ++horizontal) {
            mode.dpcm_mode_y = (uint8_t)horizontal;
            CHECK(av2h_luma_reconstruct(NULL, &mode, &prefix, &transform, &coefficients, predictor,
                                        stride, &scratch, &output) == AV2H_OK);
            for (index = 0U; index < width * height; ++index) {
                int32_t value = coefficients.dequantized[index];
                size_t step = horizontal ? 1U : width;
                int previous = horizontal ? index % width != 0U : index >= width;
                expected[index] =
                    value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
                if (previous)
                    expected[index] += expected[index - step];
                CHECK(output.residual[index] == expected[index]);
                CHECK(output.pixels[index] == clip(128 + expected[index], 255));
            }
            CHECK(av2h_luma_reconstruct_u16(NULL, &mode, &prefix, &transform, &coefficients,
                                            predictor_u16, &scratch_u16, &output_u16,
                                            &av2h_sample_domain_12bit) == AV2H_OK);
            for (index = 0U; index < width * height; ++index) {
                size_t step = horizontal ? 1U : width;
                int previous = horizontal ? index % width != 0U : index >= width;
                expected[index] = coefficients.dequantized[index] / 8;
                if (previous)
                    expected[index] += expected[index - step];
                CHECK(output_u16.residual[index] == expected[index]);
                CHECK(output_u16.pixels[index] == clip(2048 + expected[index], 4095));
            }
        }
        for (index = 0U; index < height; ++index) {
            CHECK(predictor[index * stride + width] == 0xD6);
        }
    }
}

static void test_u16_transform_selection(void) {
    static const uint8_t types[] = {
        AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST, AV2H_TX_IDTX,
        AV2H_TX_H_ADST, AV2H_TX_V_DCT
    };
    Av2hLeafMode mode = { 0 };
    Av2hChromaLeafMode chroma = { 0 };
    static uint16_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    static Av2hReconstructionScratchU16 scratch;
    static Av2hReconstructionScratchU16 expected_scratch;
    static Av2hBlockReconstructionU16 output;
    static Av2hBlockReconstructionU16 expected;
    unsigned int tx;
    size_t type;
    size_t index;
    for (index = 0U; index < AV2H_RECON_SAMPLE_CAPACITY; ++index) {
        predictor[index] = (uint16_t)((index * 37U) & 1023U);
    }
    for (tx = 0U; tx < AV2H_TX_SIZE_COUNT; ++tx) {
        const Av2hTxDescriptor *descriptor;
        Av2hCoeffPrefix prefix = { 0 };
        Av2hCoeffBlock coefficients = { 0 };
        CHECK(av2h_tx_descriptor_get((Av2hTxSize)tx, &descriptor) ==
              AV2H_OK);
        prefix.tx_size = (Av2hTxSize)tx;
        prefix.q_index = 152U;
        prefix.eob = 2U;
        coefficients.width = descriptor->coefficient_width;
        coefficients.height = descriptor->coefficient_height;
        coefficients.coefficient_count = descriptor->eob_capacity;
        coefficients.nonzero_count = 2U;
        coefficients.dequantized[0] = -512;
        coefficients.dequantized[1] = 128;
        for (type = 0U; type < sizeof(types) / sizeof(types[0]); ++type) {
            Av2hTransformType transform = { 0 };
            Av2hStatus expected_status;
            transform.primary_type = types[type];
            memset(&expected, 0x7d, sizeof(expected));
            expected_status = av2h_reconstruct_transform_generic_u16(
                NULL, &mode, &prefix, &transform, &coefficients, predictor,
                descriptor->output_width, &expected_scratch, &expected, &av2h_sample_domain_10bit);
            memset(&output, 0x7d, sizeof(output));
            CHECK(av2h_luma_reconstruct_u16(
                      0, &mode, &prefix, &transform, &coefficients, predictor,
                      &scratch, &output, &av2h_sample_domain_10bit) == expected_status);
            CHECK(memcmp(&output, &expected, sizeof(output)) == 0);
            memset(&output, 0x7d, sizeof(output));
            CHECK(av2h_chroma_reconstruct_u16(
                      0, &chroma, &prefix, &transform, &coefficients, predictor,
                      &scratch, &output, &(Av2hLeafMode){0}, &av2h_sample_domain_10bit) == expected_status);
            CHECK(memcmp(&output, &expected, sizeof(output)) == 0);
        }
    }
}

static void test_u16_lossless_wht(void) {
    Av2hLeafMode mode = { 0 };
    Av2hCoeffPrefix prefix = { 0 };
    Av2hTransformType transform = { 0 };
    Av2hCoeffBlock coefficients = { 0 };
    static Av2hReconstructionScratchU16 scratch;
    static Av2hBlockReconstructionU16 output;
    static Av2hBlockReconstruction expected;
    uint8_t predictor[16U];
    uint16_t predictor_u16[16U];
    unsigned int direction;
    size_t index;
    prefix.tx_size = AV2H_TX_4X4;
    coefficients.width = 4U;
    coefficients.height = 4U;
    coefficients.coefficient_count = 16U;
    for (index = 0U; index < 16U; ++index) {
        predictor[index] = 128U;
        predictor_u16[index] = 512U;
        coefficients.dequantized[index] = (int32_t)index * 3 - 17;
    }
    for (direction = 0U; direction < 3U; ++direction) {
        mode.use_dpcm_y = direction != 0U;
        mode.dpcm_mode_y = direction == 2U;
        CHECK(av2h_reconstruct_lossless_4x4(
                  &mode, &transform, &coefficients, predictor, 4U, &expected) ==
              AV2H_OK);
        CHECK(av2h_luma_reconstruct_u16(
                  0, &mode, &prefix, &transform, &coefficients, predictor_u16,
                  &scratch, &output, &av2h_sample_domain_10bit) == AV2H_OK);
        for (index = 0U; index < 16U; ++index) {
            CHECK(output.residual[index] == expected.residual[index]);
            CHECK(output.pixels[index] == 512 + expected.residual[index]);
        }
    }
}

static void test_extreme_coefficient_bounds(void) {
    static const int32_t extreme_patterns[6U] = {
        -32768, 32767, -524288, 524287, INT32_MIN, INT32_MAX
    };
    static const Av2hTxSize tx_sizes[] = {
        AV2H_TX_4X4, AV2H_TX_8X8, AV2H_TX_16X16, AV2H_TX_32X32, AV2H_TX_64X64, AV2H_TX_8X16
    };
    static const Av2hTxType tx_types[] = {
        AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST, AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_IDTX
    };
    const Av2hSampleDomain *domains[] = {
        &av2h_sample_domain_8bit, &av2h_sample_domain_10bit, &av2h_sample_domain_12bit
    };
    static Av2hCoeffBlock coeff_u;
    static Av2hCoeffBlock coeff_v;
    static Av2hReconstructionScratch scratch8;
    static Av2hReconstructionScratchU16 scratch16;
    static Av2hBlockReconstruction out8;
    static Av2hBlockReconstructionU16 out16;
    static uint8_t pred8[AV2H_RECON_SAMPLE_CAPACITY];
    static uint16_t pred16[AV2H_RECON_SAMPLE_CAPACITY];
    size_t p_idx;
    size_t index;

    memset(pred8, 255, sizeof(pred8));
    for (index = 0U; index < AV2H_RECON_SAMPLE_CAPACITY; ++index) {
        pred16[index] = 4095U;
    }

    for (p_idx = 0U; p_idx < 7U; ++p_idx) {
        size_t s_idx;
        uint8_t cctx;
        uint8_t depth_bits;
        int32_t residual_4x4[16U];

        for (index = 0U; index < AV2H_COEFF_CAPACITY; ++index) {
            int32_t val = p_idx < 6U ? extreme_patterns[p_idx] :
                ((index & 1U) ? INT32_MAX : INT32_MIN);
            coeff_u.dequantized[index] = val;
            coeff_v.dequantized[index] = (index & 1U) ? -1 : val;
        }

        /* Lossless 4x4 WHT + DPCM under extreme int32 inputs. */
        av2h_inverse_lossless_4x4(coeff_u.dequantized, 1U, 1U, 0U, residual_4x4);
        av2h_inverse_lossless_4x4(coeff_u.dequantized, 1U, 1U, 1U, residual_4x4);

        /* CCTX across 8/10/12-bit domains under extreme int32 inputs. */
        coeff_u.coefficient_count = 64U;
        coeff_v.coefficient_count = 64U;
        for (cctx = 1U; cctx <= 6U; ++cctx) {
            for (depth_bits = 8U; depth_bits <= 12U; depth_bits += 2U) {
                Av2hCoeffBlock u_copy = coeff_u;
                Av2hCoeffBlock v_copy = coeff_v;
                CHECK(av2h_chroma_inverse_cross_transform(&u_copy, &v_copy, cctx, depth_bits) ==
                      AV2H_OK);
            }
        }

        /* 32x32 DPCM lanes under extreme int32 inputs. */
        {
            Av2hLeafMode dpcm_mode = { 0 };
            Av2hCoeffPrefix dpcm_prefix = { 0 };
            Av2hTransformType dpcm_tx = { 0 };
            dpcm_mode.use_dpcm_y = 1U;
            dpcm_mode.fsc_mode = 1U;
            dpcm_prefix.tx_size = AV2H_TX_32X32;
            dpcm_prefix.eob = 1024U;
            dpcm_tx.primary_type = AV2H_TX_IDTX;
            coeff_u.width = 32U;
            coeff_u.height = 32U;
            coeff_u.coefficient_count = 1024U;
            coeff_u.nonzero_count = 1024U;
            CHECK(av2h_luma_reconstruct(NULL, &dpcm_mode, &dpcm_prefix, &dpcm_tx, &coeff_u,
                                        pred8, 32U, &scratch8, &out8) == AV2H_OK);
            CHECK(av2h_luma_reconstruct_u16(NULL, &dpcm_mode, &dpcm_prefix, &dpcm_tx, &coeff_u,
                                            pred16, &scratch16, &out16,
                                            &av2h_sample_domain_12bit) == AV2H_OK);
        }

        /* Primary + secondary transforms across 8/10/12-bit domains. */
        for (s_idx = 0U; s_idx < sizeof(tx_sizes) / sizeof(tx_sizes[0]); ++s_idx) {
            const Av2hTxDescriptor *desc = NULL;
            size_t t_idx;
            CHECK(av2h_tx_descriptor_get(tx_sizes[s_idx], &desc) == AV2H_OK);
            coeff_u.width = desc->coefficient_width;
            coeff_u.height = desc->coefficient_height;
            coeff_u.coefficient_count = desc->eob_capacity;
            coeff_u.nonzero_count = desc->eob_capacity;
            for (t_idx = 0U; t_idx < sizeof(tx_types) / sizeof(tx_types[0]); ++t_idx) {
                Av2hLeafMode mode = { 0 };
                Av2hCoeffPrefix prefix = { 0 };
                Av2hTransformType transform = { 0 };
                size_t d_idx;
                mode.fsc_mode = tx_types[t_idx] == AV2H_TX_IDTX ? 1U : 0U;
                prefix.tx_size = tx_sizes[s_idx];
                prefix.q_index = 128U;
                prefix.eob = desc->eob_capacity;
                transform.primary_type = tx_types[t_idx];
                transform.secondary_symbol = tx_types[t_idx] == AV2H_TX_DCT_DCT ? 1U : 0U;
                transform.secondary_set = 0U;
                (void)av2h_reconstruct_transform_generic(
                    NULL, &mode, &prefix, &transform, &coeff_u, pred8, desc->output_width,
                    &scratch8, &out8);
                for (d_idx = 0U; d_idx < 3U; ++d_idx) {
                    Av2hStatus st = av2h_reconstruct_transform_generic_u16(
                        NULL, &mode, &prefix, &transform, &coeff_u, pred16, desc->output_width,
                        &scratch16, &out16, domains[d_idx]);
                    if (st == AV2H_OK) {
                        CHECK(out16.pixels[0] <= domains[d_idx]->maximum);
                    }
                }
            }
        }
    }
}

static void test_s3_palette_ist_and_plane_bounds(void) {
    static const uint8_t palette_sizes[] = { 2U, 4U, 8U };
    size_t s;

    /* Palette map index overflow protection across 1/2/3-bit widths. */
    for (s = 0U; s < sizeof(palette_sizes) / sizeof(palette_sizes[0]); ++s) {
        Av2hLeafMode mode = { 0 };
        unsigned int bits = palette_sizes[s] <= 2U ? 1U : palette_sizes[s] <= 4U ? 2U : 3U;
        size_t max_index = (AV2H_PALETTE_MAP_CAPACITY * 8U - bits) / bits;
        mode.palette_size = palette_sizes[s];
        CHECK(av2h_palette_map_set(&mode, max_index, (uint8_t)(palette_sizes[s] - 1U)) == AV2H_OK);
        CHECK(av2h_palette_map_get(&mode, max_index) == (uint8_t)(palette_sizes[s] - 1U));
        CHECK(av2h_palette_map_set(&mode, max_index + 1U, 0U) == AV2H_OUT_OF_MEMORY);
        CHECK(av2h_palette_map_get(&mode, max_index + 1U) == AV2H_PALETTE_MAX_SIZE);
        CHECK(av2h_palette_map_set(&mode, SIZE_MAX, 0U) == AV2H_OUT_OF_MEMORY);
        CHECK(av2h_palette_map_get(&mode, SIZE_MAX) == AV2H_PALETTE_MAX_SIZE);
        if (bits > 1U) {
            CHECK(av2h_palette_map_set(&mode, SIZE_MAX / bits + 1U, 0U) == AV2H_OUT_OF_MEMORY);
            CHECK(av2h_palette_map_get(&mode, SIZE_MAX / bits + 1U) == AV2H_PALETTE_MAX_SIZE);
        }
    }

    /* Corrupted neighbor palette_size > AV2H_PALETTE_MAX_SIZE must not overflow cache[16]. */
    {
        uint8_t raw[16U] = { 0 };
        Av2hSpan span = { raw, sizeof(raw) };
        Av2hRangeDecoder decoder;
        Av2hSequenceState seq = { 0 };
        Av2hLumaModeNeighbors neighbors = { 0 };
        Av2hLeafMode mode = { 0 };
        CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
        seq.info.bit_depth_idc = 0U;
        neighbors.palette_size[0] = 255U;
        neighbors.palette_size[1] = 255U;
        mode.palette_size = 8U;
        (void)av2h_leaf_palette_read_colors(&decoder, &seq, &neighbors, &mode);
    }

    /* IST capacity and zero-dimension guards on exact heap buffer. */
    {
        int32_t *deq = (int32_t *)calloc(AV2H_COEFF_CAPACITY, sizeof(int32_t));
        Av2hLeafMode mode = { 0 };
        Av2hCoeffPrefix prefix = { 0 };
        Av2hTransformType transform = { 0 };
        Av2hCoeffBlock coefficients = { 0 };
        CHECK(deq != NULL);
        prefix.tx_size = AV2H_TX_8X8;
        transform.secondary_symbol = 1U;
        coefficients.width = 8U;
        coefficients.height = 8U;
        coefficients.coefficient_count = AV2H_COEFF_CAPACITY + 1U;
        CHECK(av2h_inverse_secondary_transform(&mode, &prefix, &transform, &coefficients, deq) ==
              AV2H_INVALID_ARGUMENT);
        coefficients.width = 0U;
        coefficients.coefficient_count = 64U;
        CHECK(av2h_inverse_secondary_transform(&mode, &prefix, &transform, &coefficients, deq) ==
              AV2H_INVALID_ARGUMENT);
        free(deq);
    }

    /* Plane reconstruction guards: out-of-range mrl_index and undersized reconstructed grid. */
    {
        static Av2hReconstructionScratch scratch8;
        static Av2hReconstructionScratchU16 scratch16;
        uint8_t pixels8[16U * 16U] = { 0 };
        uint16_t pixels16[16U * 16U] = { 0 };
        uint8_t rec[16U] = { 0 };
        Av2hReconstructionPlane plane8 = { 0 };
        Av2hReconstructionPlaneU16 plane16 = { 0 };
        Av2hPartitionNode leaf = { 0 };
        Av2hLeafMode mode = { 0 };
        Av2hChromaLeafMode chroma = { 0 };
        Av2hCoeffPrefix prefix = { 0 };
        Av2hTransformType transform = { 0 };
        Av2hCoeffBlock coefficients = { 0 };

        leaf.column = 2U;
        leaf.row = 2U;
        leaf.block_size = AV2H_BLOCK_4X4;
        memset(rec, 1, sizeof(rec));
        plane8.data = pixels8;
        plane8.stride_samples = 16U;
        plane8.width = 16U;
        plane8.height = 16U;
        plane8.reference_width = 16U;
        plane8.reference_height = 16U;
        plane8.reconstructed = rec;
        plane8.reconstructed_stride_cells = 4U;
        plane8.reconstructed_count_cells = 16U;

        prefix.tx_size = AV2H_TX_4X4;
        prefix.all_zero = 1U;
        coefficients.width = 4U;
        coefficients.height = 4U;
        coefficients.coefficient_count = 16U;
        mode.mode = AV2H_INTRA_D67;
        mode.mrl_index = 255U;
        CHECK(av2h_luma_reconstruct_to_plane(NULL, &leaf, &mode, &prefix, &transform,
                                             &coefficients, &plane8, &scratch8) ==
              AV2H_UNSUPPORTED);

        plane16.data = pixels16;
        plane16.stride_samples = 16U;
        plane16.width = 16U;
        plane16.height = 16U;
        plane16.reference_width = 16U;
        plane16.reference_height = 16U;
        plane16.reconstructed = rec;
        plane16.reconstructed_stride_cells = 4U;
        plane16.reconstructed_count_cells = 15U;
        CHECK(av2h_chroma_reconstruct_to_plane_u16(
                  NULL, &leaf, &chroma, &prefix, &transform, &coefficients, &plane16, NULL, 1U, 0U,
                  &scratch16, &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
    }
}

int main(void) {
    test_specialized_wide_transform_policies();
    test_zero_geometry();
    test_dpcm_rounding_and_clipping();
    test_dpcm_scale_and_stride();
    test_lossless_chroma_and_rejection();
    test_u16_transform_selection();
    test_u16_lossless_wht();
    test_extreme_coefficient_bounds();
    test_s3_palette_ist_and_plane_bounds();
    if (!failures) puts("supplied-predictor block reconstruction: passed");
    return failures != 0;
}
