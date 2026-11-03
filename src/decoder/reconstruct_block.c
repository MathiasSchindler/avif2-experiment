#include "internal/reconstruct_block.h"

#include "internal/decode_diagnostics.h"
#include "internal/reconstruct_modules.h"
#include "internal/sample_math.h"
#include "tx_geometry.h"

#include <stddef.h>

Av2hStatus av2h_luma_reconstruct(Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode,
                                 const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
                                 const Av2hCoeffBlock *coefficients, const uint8_t *predictor,
                                 size_t predictor_stride_samples,
                                 Av2hReconstructionScratch *scratch,
                                 Av2hBlockReconstruction *reconstruction);
Av2hStatus av2h_luma_reconstruct_u16(Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode,
                                     const Av2hCoeffPrefix *prefix,
                                     const Av2hTransformType *transform,
                                     const Av2hCoeffBlock *coefficients, const uint16_t *predictor,
                                     Av2hReconstructionScratchU16 *scratch,
                                     Av2hBlockReconstructionU16 *reconstruction,
                                     const Av2hSampleDomain *domain);
Av2hStatus av2h_chroma_reconstruct(Av2hDecodeDiagnostics *diagnostics,
                                   const Av2hChromaLeafMode *mode, const Av2hCoeffPrefix *prefix,
                                   const Av2hTransformType *transform,
                                   const Av2hCoeffBlock *coefficients, const uint8_t *predictor,
                                   uint16_t width, uint16_t height,
                                   Av2hReconstructionScratch *scratch,
                                   Av2hBlockReconstruction *reconstruction);
Av2hStatus
av2h_chroma_reconstruct_u16(Av2hDecodeDiagnostics *diagnostics, const Av2hChromaLeafMode *mode,
                            const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
                            const Av2hCoeffBlock *coefficients, const uint16_t *predictor,
                            Av2hReconstructionScratchU16 *scratch,
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

Av2hStatus av2h_chroma_inverse_cross_transform(
    Av2hCoeffBlock *u_coefficients,
    Av2hCoeffBlock *v_coefficients,
    uint8_t cctx_type,
    uint8_t bit_depth) {
    static const int32_t matrix[6U][2U] = {
        { 181, 181 }, { 222, 128 }, { 128, 222 },
        { 181, -181 }, { 222, -128 }, { 128, -222 }
    };
    size_t index;
    int32_t coefficient_limit;
    if (u_coefficients == NULL || v_coefficients == NULL || cctx_type > 6U ||
        av2h_sample_domain_get(bit_depth) == NULL ||
        u_coefficients->coefficient_count != v_coefficients->coefficient_count) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cctx_type == 0U) return AV2H_OK;
    coefficient_limit = 1 << (7U + bit_depth);
    u_coefficients->nonzero_count = 0U;
    v_coefficients->nonzero_count = 0U;
    for (index = 0U; index < u_coefficients->coefficient_count; ++index) {
        int32_t u = u_coefficients->dequantized[index];
        int32_t v = v_coefficients->dequantized[index];
        int32_t first = av2h_sample_round_pow2_signed(
            (int64_t)matrix[cctx_type - 1U][0U] * u -
                (int64_t)matrix[cctx_type - 1U][1U] * v,
            8U);
        int32_t second = av2h_sample_round_pow2_signed(
            (int64_t)matrix[cctx_type - 1U][1U] * u +
                (int64_t)matrix[cctx_type - 1U][0U] * v,
            8U);
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

#include "internal/sample_kernel_u8.inc"
#include "internal/reconstruct_block_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/reconstruct_block_kernel.inc"
#include "internal/sample_kernel_end.inc"
