#ifndef AV2H_DECODER_INTERNAL_RECONSTRUCT_MODULES_H
#define AV2H_DECODER_INTERNAL_RECONSTRUCT_MODULES_H

#include "decoder/reconstruct.h"

Av2hStatus av2h_inverse_secondary_transform(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY]);

Av2hStatus av2h_inverse_rows(
    const int32_t *input,
    int32_t *transposed,
    uint16_t width,
    uint16_t height,
    uint8_t shift,
    uint8_t identity,
    uint8_t adst,
    uint8_t flip_adst);

Av2hStatus av2h_inverse_columns(
    const int32_t *transposed,
    int32_t *output_block,
    uint16_t width,
    uint16_t height,
    uint8_t shift,
    uint8_t identity,
    uint8_t adst,
    uint8_t flip_adst);

Av2hStatus av2h_reconstruct_transform_generic(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);
Av2hStatus av2h_reconstruct_transform_generic_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);

Av2hStatus av2h_reconstruct_lossless_4x4(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hFirstReconstruction *reconstruction);

Av2hStatus av2h_reconstruct_transform_generic_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);
Av2hStatus av2h_reconstruct_transform_generic_u16_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);

Av2hStatus av2h_reconstruct_transform_8x16_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);

Av2hStatus av2h_reconstruct_transform_8x4_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);

int av2h_reconstruct_generic_selected(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    uint8_t tx_size);

#endif