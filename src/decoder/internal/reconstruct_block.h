#ifndef AV2H_DECODER_INTERNAL_RECONSTRUCT_BLOCK_H
#define AV2H_DECODER_INTERNAL_RECONSTRUCT_BLOCK_H

#include "decoder/reconstruct.h"

/* These predictors are contiguous, with the full output geometry, not the
 * capped coefficient geometry. No picture-plane state is read or committed. */
Av2hStatus av2h_luma_reconstruct_u16(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hLeafMode *mode,
    const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients,
    const uint16_t *predictor,
    Av2hReconstructionScratchU16 *scratch,
    Av2hBlockReconstructionU16 *reconstruction);

Av2hStatus av2h_chroma_reconstruct(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hChromaLeafMode *mode,
    const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients,
    const uint8_t *predictor,
    uint16_t width,
    uint16_t height,
    Av2hReconstructionScratch *scratch,
    Av2hBlockReconstruction *reconstruction);

Av2hStatus av2h_chroma_reconstruct_u16(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hChromaLeafMode *mode,
    const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients,
    const uint16_t *predictor,
    Av2hReconstructionScratchU16 *scratch,
    Av2hBlockReconstructionU16 *reconstruction);

#endif
