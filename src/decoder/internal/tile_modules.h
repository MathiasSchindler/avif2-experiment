#ifndef AV2H_DECODER_INTERNAL_TILE_MODULES_H
#define AV2H_DECODER_INTERNAL_TILE_MODULES_H

#include "../tile.h"
#include "../filter.h"

typedef struct {
    Av2hCdefControlMap cdef;
    Av2hGdfControlMap gdf;
    Av2hCcsoControlMap ccso;
} Av2hTileFilterControls;

Av2hStatus av2h_tile_filter_controls_handoff(
    const Av2hTileDecoder *decoder,
    Av2hTileFilterControls *controls);

void av2h_tile_debug_trace_leaf(Av2hDebugLeafTraceEntry *trace,
                                size_t *count,
                                Av2hPartitionNode node,
                                uint8_t mode,
                                int8_t angle_delta,
                                uint8_t mode_set,
                                uint8_t mode_index,
                                size_t before_mode_bits,
                                size_t after_mode_index_bits,
                                const Av2hRangeDecoder *decoder);
Av2hStatus av2h_tile_decoder_fail(Av2hTileDecoder *decoder,
                                  Av2hStatus status);
void av2h_tile_capture_root_partition(
    Av2hTileDecoder *decoder,
    const Av2hFirstLeafPath *path,
    Av2hPartitionType *root_partition);
uint8_t av2h_tile_partition_direction(Av2hPartitionType partition);

#endif