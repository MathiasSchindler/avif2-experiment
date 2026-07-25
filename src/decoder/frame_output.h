#ifndef AV2H_DECODER_FRAME_OUTPUT_H
#define AV2H_DECODER_FRAME_OUTPUT_H

#include "filter.h"
#include "tile.h"

typedef struct {
    Av2hReconstructionPlane coded[AV2H_FRAME_PLANE_COUNT];
    size_t coded_data_size[AV2H_FRAME_PLANE_COUNT];
    Av2hTileContextStorage tile_context;
    Av2hReconstructionScratch *reconstruction_scratch;
    Av2hFilterWorkspace *filter_workspace;
} Av2hFrameDecodeWorkspace;

typedef struct {
    size_t luma_leaf_count;
    size_t chroma_leaf_count;
    size_t entropy_tell;
} Av2hFrameDecodeResult;

Av2hStatus av2h_i420_output_size(
    const Av2hFrameState *frame,
    size_t *size);

Av2hStatus av2h_deblock_map_commit_transform(
    Av2hDeblockMap *map,
    unsigned int plane,
    const Av2hPartitionNode *leaf,
    const Av2hFirstCoeffPrefix *prefix,
    uint8_t q_index,
    uint8_t skip,
    uint8_t all_zero,
    uint8_t lossless);

Av2hStatus av2h_decode_frame_i420_private(
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size,
    Av2hFrameDecodeResult *result);

#endif
