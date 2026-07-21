#ifndef AV2H_DECODER_TILE_H
#define AV2H_DECODER_TILE_H

#include "base.h"

typedef struct {
    size_t header_bits;
    size_t header_size;
    Av2hSpan entropy;
} Av2hTileGroup;

Av2hStatus av2h_tile_group_single_parse(Av2hSpan payload,
                                         size_t uncompressed_header_bits,
                                         Av2hTileGroup *group);

#endif