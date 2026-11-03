#include "internal/reconstruct_block.h"

#include "internal/decode_diagnostics.h"
#include "internal/reconstruct_modules.h"
#include "internal/sample_math.h"
#include "tx_geometry.h"

#include <stddef.h>

static unsigned int dpcm_scale_bits(size_t coefficient_count) {
    return 3U - (coefficient_count > 256U ? 1U : 0U) - (coefficient_count > 1024U ? 1U : 0U);
}

static void reconstruct_dpcm_samples(const Av2hCoeffBlock *coefficients, const uint8_t *predictor,
                                     size_t predictor_stride_samples, uint16_t width,
                                     uint16_t height, uint8_t horizontal, unsigned int scale_bits,
                                     uint8_t truncate_toward_zero,
                                     Av2hBlockReconstruction *reconstruction,
                                     const Av2hSampleDomain *domain);

static void reconstruct_dpcm_samples_u16(const Av2hCoeffBlock *coefficients,
                                         const uint16_t *predictor, size_t predictor_stride_samples,
                                         uint16_t width, uint16_t height, uint8_t horizontal,
                                         unsigned int scale_bits, uint8_t truncate_toward_zero,
                                         Av2hBlockReconstructionU16 *reconstruction,
                                         const Av2hSampleDomain *domain);

static Av2hStatus reconstruct_lossless_4x4_u16(const Av2hCoeffBlock *coefficients,
                                               const uint16_t *predictor, uint8_t use_dpcm,
                                               uint8_t dpcm_horizontal, uint8_t identity_transform,
                                               uint8_t direct_dpcm,
                                               Av2hBlockReconstructionU16 *reconstruction,
                                               const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    int32_t residual[16U];
    size_t index;
    if (coefficients == NULL || predictor == NULL || reconstruction == NULL ||
        coefficients->width != 4U || coefficients->height != 4U ||
        coefficients->coefficient_count != 16U || use_dpcm > 1U || dpcm_horizontal > 1U ||
        identity_transform > 1U || direct_dpcm > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_inverse_lossless_4x4(coefficients->dequantized,
                              (uint8_t)(identity_transform == 0U && direct_dpcm == 0U), use_dpcm,
                              dpcm_horizontal, residual);
    reconstruction->width = 4U;
    reconstruction->height = 4U;
    for (index = 0U; index < 16U; ++index) {
        reconstruction->residual[index] = residual[index];
        reconstruction->pixels[index] =
            av2h_sample_clip_to_domain((int32_t)predictor[index] + residual[index], domain);
    }
    return AV2H_OK;
}

Av2hStatus av2h_chroma_inverse_cross_transform(Av2hCoeffBlock *u_coefficients,
                                               Av2hCoeffBlock *v_coefficients, uint8_t cctx_type,
                                               uint8_t bit_depth) {
    static const int32_t matrix[6U][2U] = {{181, 181},  {222, 128},  {128, 222},
                                           {181, -181}, {222, -128}, {128, -222}};
    size_t index;
    int32_t coefficient_limit;
    if (u_coefficients == NULL || v_coefficients == NULL || cctx_type > 6U ||
        av2h_sample_domain_get(bit_depth) == NULL ||
        u_coefficients->coefficient_count != v_coefficients->coefficient_count) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cctx_type == 0U)
        return AV2H_OK;
    coefficient_limit = 1 << (7U + bit_depth);
    u_coefficients->nonzero_count = 0U;
    v_coefficients->nonzero_count = 0U;
    for (index = 0U; index < u_coefficients->coefficient_count; ++index) {
        int32_t u = u_coefficients->dequantized[index];
        int32_t v = v_coefficients->dequantized[index];
        int32_t first = av2h_sample_round_pow2_signed(
            (int64_t)matrix[cctx_type - 1U][0U] * u - (int64_t)matrix[cctx_type - 1U][1U] * v, 8U);
        int32_t second = av2h_sample_round_pow2_signed(
            (int64_t)matrix[cctx_type - 1U][1U] * u + (int64_t)matrix[cctx_type - 1U][0U] * v, 8U);
        u_coefficients->dequantized[index] =
            av2h_sample_clamp(first, -coefficient_limit, coefficient_limit - 1);
        v_coefficients->dequantized[index] =
            av2h_sample_clamp(second, -coefficient_limit, coefficient_limit - 1);
        if (u_coefficients->dequantized[index] != 0) {
            ++u_coefficients->nonzero_count;
        }
        if (v_coefficients->dequantized[index] != 0) {
            ++v_coefficients->nonzero_count;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_reconstruct(Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode,
                                 const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
                                 const Av2hCoeffBlock *coefficients, const uint8_t *predictor,
                                 size_t predictor_stride_samples,
                                 Av2hReconstructionScratch *scratch,
                                 Av2hBlockReconstruction *reconstruction) {
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
    size_t index;
    size_t count;
    uint8_t first_shift;
    uint8_t second_shift;
    uint8_t rectangular;
    uint8_t identity_rows;
    uint8_t adst_rows;
    uint8_t flip_adst_rows;
    uint8_t identity_columns;
    uint8_t adst_columns;
    uint8_t flip_adst_columns = 0U;
    Av2hStatus status;
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        scratch == NULL || reconstruction == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 &&
        (transform->primary_type == AV2H_TX_DCT_DCT || transform->primary_type == AV2H_TX_IDTX)) {
        return av2h_reconstruct_lossless_4x4(mode, transform, coefficients, predictor,
                                             predictor_stride_samples, reconstruction);
    }
    if (prefix->all_zero) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        Av2hStatus descriptor_status;
        if (prefix->eob != 0U || coefficients->nonzero_count != 0U || predictor == NULL) {
            return AV2H_UNSUPPORTED;
        }
        descriptor_status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
        if (descriptor_status != AV2H_OK)
            return descriptor_status;
        if (predictor_stride_samples < descriptor->output_width ||
            coefficients->coefficient_count != coefficients->width * coefficients->height) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        output_count = (size_t)descriptor->output_width * descriptor->output_height;
        if (output_count > AV2H_RECON_SAMPLE_CAPACITY) {
            return AV2H_LIMIT_EXCEEDED;
        }
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (prefix->q_index == 0U && transform->primary_type == AV2H_TX_IDTX &&
        mode->use_dpcm_y != 0U) {
        const Av2hTxDescriptor *descriptor;
        size_t width;
        size_t height;
        unsigned int scale_bits;
        if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK) {
            return AV2H_UNSUPPORTED;
        }
        width = descriptor->output_width;
        height = descriptor->output_height;
        if (coefficients->width != width || coefficients->height != height ||
            coefficients->coefficient_count != width * height || predictor == NULL ||
            predictor_stride_samples < width) {
            return AV2H_UNSUPPORTED;
        }
        scale_bits = dpcm_scale_bits(coefficients->coefficient_count);
        reconstruction->width = (uint16_t)width;
        reconstruction->height = (uint16_t)height;
        reconstruct_dpcm_samples(coefficients, predictor, predictor_stride_samples, (uint16_t)width,
                                 (uint16_t)height, mode->dpcm_mode_y != 0U, scale_bits, 0U,
                                 reconstruction, domain);
        return AV2H_OK;
    }
    if (av2h_reconstruct_generic_selected(mode, transform, prefix->tx_size)) {
        return av2h_reconstruct_transform_generic(diagnostics, mode, prefix, transform,
                                                  coefficients, predictor, predictor_stride_samples,
                                                  scratch, reconstruction);
    }
    if ((mode->mode != AV2H_INTRA_DC && mode->mode != AV2H_INTRA_VERTICAL &&
         mode->mode != AV2H_INTRA_HORIZONTAL && mode->mode != AV2H_INTRA_PAETH &&
         mode->mode != AV2H_INTRA_SMOOTH && mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
         mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL && mode->mode != AV2H_INTRA_D67 &&
         mode->mode != AV2H_INTRA_D113 && mode->mode != AV2H_INTRA_D135 &&
         mode->mode != AV2H_INTRA_D157 && mode->mode != AV2H_INTRA_D45 &&
         mode->mode != AV2H_INTRA_D203) ||
        coefficients->width == 0U || coefficients->height == 0U ||
        coefficients->coefficient_count != coefficients->width * coefficients->height) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->use_intrabc && mode->skip_transform) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK || descriptor == NULL ||
            mode->intrabc_mode > 1U || mode->mv_precision != 3U || !prefix->all_zero ||
            prefix->eob != 0U || transform->primary_type != AV2H_TX_DCT_DCT ||
            coefficients->width != descriptor->coefficient_width ||
            coefficients->height != descriptor->coefficient_height ||
            coefficients->coefficient_count != descriptor->eob_capacity ||
            coefficients->nonzero_count != 0U || predictor == NULL ||
            predictor_stride_samples < descriptor->output_width) {
            return AV2H_UNSUPPORTED;
        }
        output_count = (size_t)descriptor->output_width * descriptor->output_height;
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 && mode->mrl_index == 0U &&
        mode->enable_ibp == 1U && mode->use_intrabc == 0U && mode->skip_transform == 0U &&
        mode->fsc_mode == 0U && prefix->tx_size == AV2H_TX_16X16 && prefix->all_zero &&
        prefix->eob == 0U && transform->primary_type == AV2H_TX_DCT_DCT &&
        coefficients->width == 16U && coefficients->height == 16U &&
        coefficients->coefficient_count == 256U && coefficients->nonzero_count == 0U &&
        predictor != NULL && predictor_stride_samples >= 16U) {
        reconstruction->width = 16U;
        reconstruction->height = 16U;
        for (index = 0U; index < 256U; ++index) {
            size_t row = index >> 4U;
            size_t column = index & 15U;
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 && mode->mrl_index == 0U &&
        mode->enable_ibp == 1U && mode->use_intrabc == 0U && mode->skip_transform == 0U &&
        mode->fsc_mode == 0U && prefix->tx_size == AV2H_TX_64X16 && prefix->all_zero &&
        prefix->eob == 0U && transform->primary_type == AV2H_TX_DCT_DCT &&
        coefficients->width == 32U && coefficients->height == 16U &&
        coefficients->coefficient_count == 512U && coefficients->nonzero_count == 0U &&
        predictor != NULL && predictor_stride_samples >= 64U) {
        reconstruction->width = 64U;
        reconstruction->height = 16U;
        for (index = 0U; index < 512U; ++index) {
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < 1024U; ++index) {
            size_t row = index >> 6U;
            size_t column = index & 63U;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        !((prefix->tx_size == AV2H_TX_4X4 && (transform->primary_type == AV2H_TX_DCT_ADST ||
                                              transform->primary_type == AV2H_TX_DCT_DCT)) ||
          (prefix->tx_size == AV2H_TX_4X8 && (transform->primary_type == AV2H_TX_ADST_DCT ||
                                              transform->primary_type == AV2H_TX_ADST_ADST)) ||
          (prefix->tx_size == AV2H_TX_8X8 && (transform->primary_type == AV2H_TX_DCT_DCT ||
                                              transform->primary_type == AV2H_TX_ADST_ADST)) ||
          (prefix->tx_size == AV2H_TX_16X8 && (transform->primary_type == AV2H_TX_ADST_ADST ||
                                               transform->primary_type == AV2H_TX_DCT_DCT)) ||
          (prefix->tx_size == AV2H_TX_16X16 && transform->primary_type == AV2H_TX_ADST_ADST) ||
          (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_DCT) ||
          (prefix->tx_size == AV2H_TX_8X32 && transform->primary_type == AV2H_TX_H_DCT))) {
        return AV2H_UNSUPPORTED;
    }
    if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if ((prefix->tx_size == AV2H_TX_4X8 || prefix->tx_size == AV2H_TX_8X4) &&
               transform->primary_type == AV2H_TX_H_ADST &&
               (mode->mode == AV2H_INTRA_HORIZONTAL ||
                (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL && prefix->tx_size == AV2H_TX_8X4))) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_D67 || mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_D157)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL || mode->mode == AV2H_INTRA_D135)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_IDTX &&
               mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_IDTX &&
               mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_FLIPADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
        flip_adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_VERTICAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X16 && transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X16 && transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_PAETH)) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X16 && transform->primary_type == AV2H_TX_DCT_ADST) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_ADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_IDTX &&
               (mode->use_intrabc || mode->fsc_mode || mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_H_ADST &&
               (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_HORIZONTAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_H_FLIPADST &&
               mode->use_intrabc) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_D45) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_D45)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_HORIZONTAL) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X16 && transform->primary_type == AV2H_TX_IDTX &&
               (mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_D135) && mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D113 || mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D157 || mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_ADST_DCT &&
               (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D113 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST && mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X16 && transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_D157)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X16 && transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_DC)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL || mode->mode == AV2H_INTRA_PAETH ||
                mode->mode == AV2H_INTRA_HORIZONTAL)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_VERTICAL || mode->mode == AV2H_INTRA_DC)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 && transform->primary_type == AV2H_TX_H_DCT &&
               (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_DC) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->dequantized[index] != 0 && index != 0U && index != 16U &&
                index != 17U && index != 32U) {
                return AV2H_UNSUPPORTED;
            }
        }
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->dequantized[index] != 0 && index != 0U && index != 1U) {
                return AV2H_UNSUPPORTED;
            }
        }
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_FLIPADST && mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_D113)) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_H_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_IDTX &&
               mode->mode == AV2H_INTRA_D45 && mode->fsc_mode) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X4 && transform->primary_type == AV2H_TX_H_DCT &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X4 && transform->primary_type == AV2H_TX_FLIPADST_DCT &&
               (mode->mode == AV2H_INTRA_D67 || mode->mode == AV2H_INTRA_D157)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
        flip_adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_32X8 && transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D45)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X8 && transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_32X32 && transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    reconstruction->width = coefficients->width;
    reconstruction->height = coefficients->height;
    count = coefficients->coefficient_count;
    if (transform->secondary_symbol != 0U) {
        for (index = 0U; index < count; ++index) {
            scratch->scaled[index] = coefficients->dequantized[index];
        }
        status = av2h_inverse_secondary_transform(mode, prefix, transform, coefficients,
                                                  scratch->scaled);
        if (status != AV2H_OK)
            return status;
        if (rectangular) {
            for (index = 0U; index < count; ++index) {
                scratch->scaled[index] = av2h_sample_clamp(
                    av2h_sample_round_pow2((int64_t)scratch->scaled[index] * 2896, 12U), -32768,
                    32767);
            }
        }
    } else {
        for (index = 0U; index < count; ++index) {
            scratch->scaled[index] =
                rectangular
                    ? av2h_sample_round_pow2((int64_t)coefficients->dequantized[index] * 2896, 12U)
                    : coefficients->dequantized[index];
            scratch->scaled[index] = av2h_sample_clamp(scratch->scaled[index], -32768, 32767);
        }
    }
    status = av2h_inverse_rows(scratch->scaled, scratch->transposed, coefficients->width,
                               coefficients->height, first_shift, identity_rows, adst_rows,
                               flip_adst_rows);
    if (status != AV2H_OK)
        return status;
    status = av2h_inverse_columns(scratch->transposed, scratch->residual, coefficients->width,
                                  coefficients->height, second_shift, identity_columns,
                                  adst_columns, flip_adst_columns);
    if (status != AV2H_OK)
        return status;
    for (index = 0U; index < count; ++index) {
        int32_t predicted =
            predictor == NULL ? domain->midpoint
                              : predictor[(index / coefficients->width) * predictor_stride_samples +
                                          index % coefficients->width];
        int32_t pixel = av2h_sample_clip_to_domain(predicted + scratch->residual[index], domain);
        reconstruction->residual[index] = (int16_t)scratch->residual[index];
        reconstruction->pixels[index] = (uint8_t)pixel;
    }
    return AV2H_OK;
}
Av2hStatus av2h_chroma_reconstruct(Av2hDecodeDiagnostics *diagnostics,
                                   const Av2hChromaLeafMode *mode, const Av2hCoeffPrefix *prefix,
                                   const Av2hTransformType *transform,
                                   const Av2hCoeffBlock *coefficients, const uint8_t *predictor,
                                   uint16_t width, uint16_t height,
                                   Av2hReconstructionScratch *scratch,
                                   Av2hBlockReconstruction *reconstruction) {
    Av2hLeafMode luma_mode;
    Av2hCoeffPrefix effective_prefix;
    size_t index;
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        predictor == NULL || scratch == NULL || reconstruction == NULL || width == 0U ||
        height == 0U || (size_t)width * height > AV2H_RECON_SAMPLE_CAPACITY) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->all_zero && coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        reconstruction->width = width;
        reconstruction->height = height;
        for (index = 0U; index < (size_t)width * height; ++index) {
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[index];
        }
        return AV2H_OK;
    }
    av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
    if (mode->is_cfl) {
        luma_mode.mode = AV2H_INTRA_DC;
    } else {
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
    }
    luma_mode.use_dpcm_y = mode->use_dpcm_uv;
    luma_mode.dpcm_mode_y = mode->dpcm_mode_uv;
    luma_mode.fsc_mode = transform->primary_type == AV2H_TX_IDTX ? 1U : 0U;
    effective_prefix = *prefix;
    if (coefficients->nonzero_count != 0U) {
        effective_prefix.all_zero = 0U;
    }
    return av2h_luma_reconstruct(diagnostics, &luma_mode, &effective_prefix, transform,
                                 coefficients, predictor, width, scratch, reconstruction);
}

Av2hStatus av2h_luma_reconstruct_u16(Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode,
                                     const Av2hCoeffPrefix *prefix,
                                     const Av2hTransformType *transform,
                                     const Av2hCoeffBlock *coefficients, const uint16_t *predictor,
                                     Av2hReconstructionScratchU16 *scratch,
                                     Av2hBlockReconstructionU16 *reconstruction,
                                     const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        predictor == NULL || scratch == NULL || reconstruction == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 && mode->fsc_mode == 0U) {
        status = reconstruct_lossless_4x4_u16(coefficients, predictor, mode->use_dpcm_y,
                                              mode->dpcm_mode_y, 0U, 0U, reconstruction, domain);
    } else if (mode->use_dpcm_y != 0U && mode->fsc_mode != 0U) {
        if (coefficients->coefficient_count !=
                (size_t)descriptor->output_width * descriptor->output_height ||
            coefficients->width != descriptor->output_width ||
            coefficients->height != descriptor->output_height) {
            return AV2H_UNSUPPORTED;
        }
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        reconstruct_dpcm_samples_u16(coefficients, predictor, descriptor->output_width,
                                     descriptor->output_width, descriptor->output_height,
                                     mode->dpcm_mode_y != 0U, 3U, 1U, reconstruction, domain);
        status = AV2H_OK;
    } else {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 2U);
        status = av2h_reconstruct_transform_generic_u16(
            diagnostics, mode, prefix, transform, coefficients, predictor, descriptor->output_width,
            scratch, reconstruction, domain);
    }
    return status;
}
Av2hStatus
av2h_chroma_reconstruct_u16(Av2hDecodeDiagnostics *diagnostics, const Av2hChromaLeafMode *mode,
                            const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
                            const Av2hCoeffBlock *coefficients, const uint16_t *predictor,
                            Av2hReconstructionScratchU16 *scratch,
                            Av2hBlockReconstructionU16 *reconstruction, Av2hLeafMode *luma_mode,
                            const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        predictor == NULL || scratch == NULL || reconstruction == NULL || luma_mode == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4) {
        status = reconstruct_lossless_4x4_u16(coefficients, predictor, mode->use_dpcm_uv,
                                              mode->dpcm_mode_uv,
                                              (uint8_t)(transform->primary_type == AV2H_TX_IDTX),
                                              mode->use_dpcm_uv, reconstruction, domain);
    } else {
        av2h_memory_fill(luma_mode, 0U, sizeof(*luma_mode));
        luma_mode->mode = mode->mode;
        luma_mode->angle_delta = mode->angle_delta;
        luma_mode->edge_ibp = mode->edge_ibp;
        luma_mode->above_smooth = mode->above_smooth;
        luma_mode->left_smooth = mode->left_smooth;
        status = av2h_reconstruct_transform_generic_u16(
            diagnostics, luma_mode, prefix, transform, coefficients, predictor,
            descriptor->output_width, scratch, reconstruction, domain);
    }
    return status;
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/reconstruct_block_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/reconstruct_block_kernel.inc"
#include "internal/sample_kernel_end.inc"
