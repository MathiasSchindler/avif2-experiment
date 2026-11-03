#include "decoder/internal/reconstruct_block.h"
#include "decoder/internal/reconstruct_modules.h"
#include "decoder/tx_geometry.h"

#include <stdio.h>
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
                    &scratch_u16, &output_u16, &av2h_sample_domain_10bit);
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
              &scratch_u16, &output_u16, &av2h_sample_domain_10bit) == AV2H_OK);
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
              &scratch_u16, &output_u16, &av2h_sample_domain_10bit) == AV2H_UNSUPPORTED);
    CHECK(memcmp(&output_u16, &original_u16, sizeof(output_u16)) == 0);
    CHECK(av2h_luma_reconstruct_u16(
              0, 0, &prefix, &transform, &coefficients, predictor_u16,
              &scratch_u16, &output_u16, &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_chroma_reconstruct(
              0, &mode, &prefix, &transform, &coefficients, predictor, 0U, 4U,
              &scratch, &output) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_chroma_reconstruct_u16(
              0, &mode, &prefix, &transform, &coefficients, 0,
              &scratch_u16, &output_u16, &av2h_sample_domain_10bit) == AV2H_INVALID_ARGUMENT);
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
                      &scratch, &output, &av2h_sample_domain_10bit) == expected_status);
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

int main(void) {
    test_zero_geometry();
    test_dpcm_rounding_and_clipping();
    test_lossless_chroma_and_rejection();
    test_u16_transform_selection();
    test_u16_lossless_wht();
    if (!failures) puts("supplied-predictor block reconstruction: passed");
    return failures != 0;
}
