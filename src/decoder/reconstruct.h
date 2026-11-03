#ifndef AV2H_DECODER_RECONSTRUCT_H
#define AV2H_DECODER_RECONSTRUCT_H

#include "block_geometry.h"
#include "coeff_block.h"
#include "decoder/internal/sample_math.h"
#include "leaf_mode.h"
#include "plane.h"
#include "transform_type.h"

#include <stddef.h>

struct Av2hDecodeDiagnostics;

typedef int64_t Av2hReconstructionSum;

typedef struct {
    int16_t residual[AV2H_RECON_SAMPLE_CAPACITY];
    uint8_t pixels[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t width;
    uint16_t height;
} Av2hBlockReconstruction;

typedef struct {
    Av2hTransformIntermediate residual[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t pixels[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t width;
    uint16_t height;
} Av2hBlockReconstructionU16;

typedef struct {
    Av2hTransformIntermediate scaled[AV2H_COEFF_CAPACITY];
    Av2hTransformIntermediate transposed[AV2H_COEFF_CAPACITY];
    Av2hTransformIntermediate residual[AV2H_COEFF_CAPACITY];
    uint8_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    uint8_t above[128U];
    uint8_t left[128U];
    uint8_t secondary_above[128U];
    uint8_t secondary_left[128U];
} Av2hReconstructionScratch;

typedef struct {
    Av2hTransformIntermediate scaled[AV2H_COEFF_CAPACITY];
    Av2hTransformIntermediate transposed[AV2H_COEFF_CAPACITY];
    Av2hTransformIntermediate residual[AV2H_COEFF_CAPACITY];
    uint16_t predictor[AV2H_RECON_SAMPLE_CAPACITY];
    uint16_t above[128U];
    uint16_t left[128U];
    uint16_t secondary_above[128U];
    uint16_t secondary_left[128U];
} Av2hReconstructionScratchU16;

Av2hStatus av2h_luma_reconstruct(struct Av2hDecodeDiagnostics *diagnostics,
                                 const Av2hLeafMode *mode, const Av2hCoeffPrefix *prefix,
                                 const Av2hTransformType *transform,
                                 const Av2hCoeffBlock *coefficients, const uint8_t *predictor,
                                 size_t predictor_stride_samples,
                                 Av2hReconstructionScratch *scratch,
                                 Av2hBlockReconstruction *reconstruction);

Av2hStatus av2h_luma_reconstruct_to_plane(struct Av2hDecodeDiagnostics *diagnostics,
                                          const Av2hPartitionNode *leaf, const Av2hLeafMode *mode,
                                          const Av2hCoeffPrefix *prefix,
                                          const Av2hTransformType *transform,
                                          const Av2hCoeffBlock *coefficients,
                                          Av2hReconstructionPlane *plane,
                                          Av2hReconstructionScratch *scratch);

Av2hStatus av2h_luma_reconstruct_to_plane_u16(
    struct Av2hDecodeDiagnostics *diagnostics, const Av2hPartitionNode *leaf,
    const Av2hLeafMode *mode, const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients, Av2hReconstructionPlaneU16 *plane,
    Av2hReconstructionScratchU16 *scratch, const Av2hSampleDomain *domain);

/*
 * `luma_plane` supplies the same-superblock
 * reconstructed luma consumed by CFL_DERIVED_ALPHA (may be NULL for
 * neighbour-only DC/H leaves).  When `predictor_plane` is non-NULL the intra
 * (pre-residual) predictor samples are also written to it, enabling
 * predictor-plane hashing.  `chroma_plane` is 1 for U and 2 for V, selecting
 * the corresponding explicit-CfL alpha.  Both extra planes and diagnostics
 * are optional; callers provide the chroma plane and format explicitly.
 */

Av2hStatus
av2h_chroma_reconstruct_to_plane(struct Av2hDecodeDiagnostics *diagnostics,
                                 const Av2hPartitionNode *leaf, const Av2hChromaLeafMode *mode,
                                 const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
                                 const Av2hCoeffBlock *coefficients, Av2hReconstructionPlane *plane,
                                 const Av2hReconstructionPlane *luma_plane, uint8_t chroma_plane,
                                 Av2hReconstructionPlane *predictor_plane,
                                 uint8_t chroma_format_idc, Av2hReconstructionScratch *scratch);

Av2hStatus av2h_chroma_reconstruct_to_plane_u16(
    struct Av2hDecodeDiagnostics *diagnostics, const Av2hPartitionNode *leaf,
    const Av2hChromaLeafMode *mode, const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform, const Av2hCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane, const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t chroma_plane, uint8_t chroma_format_idc, Av2hReconstructionScratchU16 *scratch,
    const Av2hSampleDomain *domain);
Av2hStatus av2h_chroma_inverse_cross_transform(Av2hCoeffBlock *u_coefficients,
                                               Av2hCoeffBlock *v_coefficients, uint8_t cctx_type,
                                               uint8_t bit_depth);

#endif
