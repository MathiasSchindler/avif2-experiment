#ifndef AV2H_DECODER_RECONSTRUCT_H
#define AV2H_DECODER_RECONSTRUCT_H

#include "block_geometry.h"
#include "coeff_block.h"
#include "leaf_mode.h"
#include "plane.h"
#include "transform_type.h"

struct Av2hDecodeDiagnostics;

typedef int64_t Av2hReconstructionSum;

typedef struct {
    int16_t residual[AV2H_RECON_SAMPLE_CAPACITY];
    uint8_t pixels[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t width;
    uint16_t height;
} Av2hFirstReconstruction;

typedef struct {
    Av2hTransformIntermediate residual[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t pixels[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t width;
    uint16_t height;
} Av2hFirstReconstructionU16;

typedef struct {
    Av2hTransformIntermediate scaled[AV2H_FIRST_COEFF_CAPACITY];
    Av2hTransformIntermediate transposed[AV2H_FIRST_COEFF_CAPACITY];
    Av2hTransformIntermediate residual[AV2H_FIRST_COEFF_CAPACITY];
    uint8_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    uint8_t above[128U];
    uint8_t left[128U];
    uint8_t secondary_above[128U];
    uint8_t secondary_left[128U];
} Av2hReconstructionScratch;

typedef struct {
    Av2hTransformIntermediate scaled[AV2H_FIRST_COEFF_CAPACITY];
    Av2hTransformIntermediate transposed[AV2H_FIRST_COEFF_CAPACITY];
    Av2hTransformIntermediate residual[AV2H_FIRST_COEFF_CAPACITY];
    uint16_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t above[128U];
    uint16_t left[128U];
    uint16_t secondary_above[128U];
    uint16_t secondary_left[128U];
} Av2hReconstructionScratchU16;

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
    size_t predictor_stride_samples,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);
Av2hStatus av2h_luma_reconstruct_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
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
Av2hStatus av2h_luma_reconstruct_to_plane_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch);
Av2hStatus av2h_luma_reconstruct_to_plane_u16(
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane,
    Av2hReconstructionScratchU16 *scratch);
Av2hStatus av2h_luma_reconstruct_to_plane_u16_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane,
    Av2hReconstructionScratchU16 *scratch);
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
 * predictor-plane hashing.  `chroma_plane` is 1 for U and 2 for V, selecting
 * the corresponding explicit-CfL alpha.  Both extra planes are optional; the
 * legacy av2h_chroma_reconstruct_to_plane forwards here as plane U with both
 * extra planes NULL.
 */
Av2hStatus av2h_chroma_reconstruct_to_plane_ex(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    uint8_t chroma_plane,
    Av2hReconstructionPlane *predictor_plane,
    Av2hReconstructionScratch *scratch);
Av2hStatus av2h_chroma_reconstruct_to_plane_format(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    uint8_t chroma_plane,
    Av2hReconstructionPlane *predictor_plane,
    uint8_t chroma_format_idc,
    Av2hReconstructionScratch *scratch);
Av2hStatus av2h_chroma_reconstruct_to_plane_format_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    uint8_t chroma_plane,
    Av2hReconstructionPlane *predictor_plane,
    uint8_t chroma_format_idc,
    Av2hReconstructionScratch *scratch);
Av2hStatus av2h_chroma_reconstruct_to_plane_u16(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane,
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t chroma_plane,
    uint8_t chroma_format_idc,
    Av2hReconstructionScratchU16 *scratch);
Av2hStatus av2h_chroma_reconstruct_to_plane_u16_with_diagnostics(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane,
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t chroma_plane,
    uint8_t chroma_format_idc,
    Av2hReconstructionScratchU16 *scratch);
Av2hStatus av2h_chroma_inverse_cross_transform(
    Av2hFirstCoeffBlock *u_coefficients,
    Av2hFirstCoeffBlock *v_coefficients,
    uint8_t cctx_type,
    uint8_t bit_depth);
Av2hStatus av2h_first_chroma_reconstruct(
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);

#endif