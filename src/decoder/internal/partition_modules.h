#ifndef AV2H_DECODER_INTERNAL_PARTITION_MODULES_H
#define AV2H_DECODER_INTERNAL_PARTITION_MODULES_H

#include "decoder/partition.h"

typedef struct {
    uint8_t mi_width;
    uint8_t mi_height;
    uint8_t split_size_adjust;
    uint8_t rect_size_adjust;
} Av2hBlockGeometry;

extern const Av2hBlockGeometry av2h_block_geometry[AV2H_BLOCK_SIZE_COUNT];

#endif