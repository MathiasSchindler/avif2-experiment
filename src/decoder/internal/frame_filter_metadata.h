#ifndef AV2H_DECODER_INTERNAL_FRAME_FILTER_METADATA_H
#define AV2H_DECODER_INTERNAL_FRAME_FILTER_METADATA_H

#include "decoder/coeff.h"
#include "decoder/filter.h"

Av2hStatus av2h_deblock_map_commit_transform(Av2hDeblockMap *map, uint8_t chroma_format_idc,
                                             unsigned int plane, const Av2hPartitionNode *leaf,
                                             const Av2hCoeffPrefix *prefix, uint8_t skip,
                                             uint8_t all_zero, uint8_t lossless);

Av2hStatus av2h_restoration_tx_skip_commit(Av2hFilterWorkspace *workspace,
                                           const Av2hPartitionNode *leaf,
                                           const Av2hCoeffPrefix *prefix);

#endif
