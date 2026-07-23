#ifndef AV2H_DECODER_RECONSTRUCT_H
#define AV2H_DECODER_RECONSTRUCT_H

#include "coeff_level.h"

typedef struct {
    int16_t residual[AV2H_FIRST_COEFF_CAPACITY];
    uint8_t pixels[AV2H_FIRST_COEFF_CAPACITY];
    uint16_t width;
    uint16_t height;
} Av2hFirstReconstruction;

typedef struct {
    int32_t scaled[AV2H_FIRST_COEFF_CAPACITY];
    int32_t transposed[AV2H_FIRST_COEFF_CAPACITY];
    int32_t residual[AV2H_FIRST_COEFF_CAPACITY];
    uint8_t predictor[AV2H_FIRST_COEFF_CAPACITY];
    uint8_t above[64U];
    uint8_t left[64U];
} Av2hReconstructionScratch;

typedef struct {
    uint8_t *data;
    size_t stride;
    uint16_t width;
    uint16_t height;
} Av2hReconstructionPlane;

typedef struct {
    const uint8_t *left;
    size_t left_count;
    const uint8_t *above;
    size_t above_count;
    uint8_t above_left;
    uint8_t has_above_left;
} Av2hLumaReferences;

Av2hStatus av2h_luma_predict(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride);
Av2hStatus av2h_chroma_predict(
    const Av2hFirstChromaMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride);

Av2hStatus av2h_first_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);
Av2hStatus av2h_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);
Av2hStatus av2h_luma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch);
Av2hStatus av2h_first_chroma_reconstruct(
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);

#endif