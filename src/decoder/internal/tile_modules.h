#ifndef AV2H_DECODER_INTERNAL_TILE_MODULES_H
#define AV2H_DECODER_INTERNAL_TILE_MODULES_H

#include "../filter.h"
#include "../tile.h"

typedef struct {
    Av2hCdefControlMap cdef;
    Av2hGdfControlMap gdf;
    Av2hCcsoControlMap ccso;
} Av2hTileFilterControls;

Av2hStatus av2h_tile_filter_controls_handoff(const Av2hTileDecoder *decoder,
                                             Av2hTileFilterControls *controls);

Av2hStatus av2h_tile_decoder_fail(Av2hTileDecoder *decoder, Av2hStatus status);
void av2h_tile_capture_root_partition(Av2hTileDecoder *decoder, const Av2hLeafPath *path,
                                      Av2hPartitionType *root_partition);
uint8_t av2h_tile_partition_direction(Av2hPartitionType partition);

#endif