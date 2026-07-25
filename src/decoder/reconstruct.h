#ifndef AV2H_DECODER_RECONSTRUCT_H
#define AV2H_DECODER_RECONSTRUCT_H

#include "coeff_level.h"

/*
 * Predictor and reconstructed sample buffers must hold the full output
 * geometry of a luma transform block, which reaches 64x64 (4096 samples)
 * for the large Emerald Q208 blocks.  Coefficient buffers stay bounded at
 * AV2H_FIRST_COEFF_CAPACITY (1024) because AV2 clamps every coefficient
 * dimension to at most 32 (the adjusted transform size).
 */
#define AV2H_RECON_SAMPLE_CAPACITY 4096U

typedef struct {
    int16_t residual[AV2H_RECON_SAMPLE_CAPACITY];
    uint8_t pixels[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t width;
    uint16_t height;
} Av2hFirstReconstruction;

typedef struct {
    int32_t scaled[AV2H_FIRST_COEFF_CAPACITY];
    int32_t transposed[AV2H_FIRST_COEFF_CAPACITY];
    int32_t residual[AV2H_FIRST_COEFF_CAPACITY];
    uint8_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    uint8_t above[64U];
    uint8_t left[64U];
} Av2hReconstructionScratch;

typedef struct {
    uint8_t *data;
    size_t stride;
    uint16_t width;
    uint16_t height;
    uint16_t reference_width;
    uint16_t reference_height;
    uint8_t *reconstructed;
    size_t reconstructed_stride;
    size_t reconstructed_count;
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
Av2hStatus av2h_chroma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch);
/*
 * Extended chroma reconstruction.  `luma_plane` supplies the same-superblock
 * reconstructed luma consumed by CFL_DERIVED_ALPHA (may be NULL for
 * neighbour-only DC/H leaves).  When `predictor_plane` is non-NULL the intra
 * (pre-residual) predictor samples are also written to it, enabling
 * predictor-plane hashing.  Both extra planes are optional; the legacy
 * av2h_chroma_reconstruct_to_plane forwards here with both NULL.
 */
Av2hStatus av2h_chroma_reconstruct_to_plane_ex(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    Av2hReconstructionPlane *predictor_plane,
    Av2hReconstructionScratch *scratch);
Av2hStatus av2h_first_chroma_reconstruct(
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);

#endif