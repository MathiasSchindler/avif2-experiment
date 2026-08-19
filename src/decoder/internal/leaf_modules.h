#ifndef AV2H_DECODER_INTERNAL_LEAF_MODULES_H
#define AV2H_DECODER_INTERNAL_LEAF_MODULES_H

#include "decoder/leaf.h"

uint8_t av2h_leaf_block_allows_palette(Av2hBlockSize block_size);

Av2hStatus av2h_leaf_segment_id_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    uint8_t extended_enabled,
    uint8_t context,
    uint8_t predictor,
    uint8_t maximum,
    uint8_t *segment_id);

Av2hQIndex av2h_leaf_segment_q_index(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hQIndex base_q_index,
    uint8_t segment_id);

Av2hStatus av2h_leaf_palette_read_colors(
    Av2hRangeDecoder *decoder,
    const Av2hSequenceState *sequence,
    const Av2hLumaModeNeighbors *neighbors,
    Av2hFirstLeafMode *mode);

Av2hStatus av2h_leaf_read_gdf_entropy(
    Av2hRangeDecoder *decoder,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hGdfEntropyState *state,
    Av2hFirstLeafMode *mode);

Av2hStatus av2h_leaf_read_cdef_entropy(
    Av2hRangeDecoder *decoder,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hCdefEntropyState *state,
    Av2hFirstLeafMode *mode);

Av2hStatus av2h_leaf_read_ccso_entropy(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hCcsoEntropyState *state,
    Av2hFirstLeafMode *mode);

Av2hStatus av2h_leaf_read_first_ccso_without_state(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hFirstLeafMode *mode);

Av2hStatus av2h_leaf_read_first_intrabc(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    const Av2hLumaModeNeighbors *neighbors,
    Av2hFirstLeafMode *mode);

uint8_t av2h_intrabc_bank_origin_valid(
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    size_t width_mi,
    size_t height_mi,
    int16_t displacement_x,
    int16_t displacement_y);

#endif