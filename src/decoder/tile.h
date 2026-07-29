#ifndef AV2H_DECODER_TILE_H
#define AV2H_DECODER_TILE_H

#include "reconstruct.h"

extern uint32_t av2h_debug_luma_stage_ranges[5U];
extern uint8_t av2h_debug_luma_tile_stage;
extern uint8_t av2h_debug_chroma_colocated_mode;
extern int8_t av2h_debug_chroma_colocated_delta;

typedef struct {
    size_t header_bits;
    size_t header_size;
    Av2hSpan entropy;
} Av2hTileGroup;

typedef struct {
    uint8_t present;
    uint8_t mode;
    int8_t angle_delta;
    uint8_t chroma_cfl;
} Av2hTileLumaModeCell;

typedef struct {
    uint8_t *luma_partition_above;
    uint8_t *luma_partition_left;
    uint8_t *chroma_partition_above;
    uint8_t *chroma_partition_left;
    Av2hLumaModeContext *luma_mode_above;
    Av2hLumaModeContext *luma_mode_left;
    uint8_t *coefficient_above;
    uint8_t *coefficient_left;
    Av2hTileLumaModeCell *colocated_luma;
    size_t above_count;
    size_t left_count;
    size_t colocated_luma_count;
    uint8_t *gdf_block_flags;
    size_t gdf_block_flag_capacity;
    uint8_t *cdef_unit_strengths;
    size_t cdef_unit_strength_capacity;
    uint8_t *ccso_unit_controls[AV2H_FRAME_PLANE_COUNT];
    size_t ccso_unit_control_capacity;
} Av2hTileContextStorage;

typedef struct {
    Av2hPartitionNode node;
    Av2hFirstLeafMode mode;
    Av2hFirstCoeffPrefix prefix;
    Av2hFirstTransformType transform;
} Av2hTileLumaLeaf;

typedef struct {
    Av2hPartitionNode node;
    Av2hFirstChromaMode mode;
    Av2hFirstCoeffPrefix prefix[2U];
    Av2hFirstTransformType transform[2U];
} Av2hTileChromaLeaf;

typedef enum {
    AV2H_TILE_DECODE_LUMA = 0,
    AV2H_TILE_DECODE_CHROMA = 1,
    AV2H_TILE_DECODE_DONE = 2
} Av2hTileDecodePhase;

typedef enum {
    AV2H_TILE_EVENT_NONE = 0,
    AV2H_TILE_EVENT_LUMA_LEAF = 1,
    AV2H_TILE_EVENT_CHROMA_LEAF = 2,
    /* has_leaf == 0 ended the current SB structure, not the tile. */
    AV2H_TILE_EVENT_ROOT_HANDOFF = 3,
    AV2H_TILE_EVENT_DONE = 4
} Av2hTileDecodeEvent;

typedef struct {
    Av2hSpan entropy;
    Av2hRangeDecoder range;
    const Av2hSequenceState *sequence;
    const Av2hFrameState *frame;
    Av2hPartitionCdfs partition_cdfs;
    Av2hLeafCdfs leaf_cdfs;
    Av2hFirstCoeffCdfs coeff_cdfs;
    Av2hFirstTransformCdfs transform_cdfs;
    Av2hFirstCoeffLevelCdfs level_cdfs;
    Av2hPartitionContextState partition_contexts;
    Av2hLumaModeContextState luma_mode_contexts;
    Av2hCoeffContextState coefficient_contexts;
    Av2hGdfEntropyState gdf_entropy;
    Av2hCdefEntropyState cdef_entropy;
    Av2hCcsoEntropyState ccso_entropy;
    Av2hPartitionTraversal luma_traversal;
    Av2hPartitionTraversal chroma_traversal;
    Av2hTileLumaModeCell *colocated_luma;
    size_t colocated_luma_count;
    size_t mi_columns;
    size_t mi_rows;
    uint16_t superblock_mi;
    uint16_t sb_row;
    uint16_t sb_column;
    size_t sb_index;
    size_t sb_count;
    Av2hBlockSize root_block_size;
    Av2hPartitionType luma_root_partition;
    Av2hPartitionType chroma_root_partition;
    Av2hPartitionType chroma_root_forced_partition;
    uint8_t luma_root_child_count;
    uint8_t luma_root_children_share;
    Av2hTileDecodePhase phase;
    Av2hTileDecodeEvent event;
    Av2hStatus status;
} Av2hTileDecoder;

Av2hStatus av2h_tile_group_single_parse(Av2hSpan payload,
                                         size_t uncompressed_header_bits,
                                         Av2hTileGroup *group);
Av2hStatus av2h_tile_decoder_init(
    Av2hTileDecoder *decoder,
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hTileContextStorage *storage);
Av2hStatus av2h_tile_decoder_next_luma(
    Av2hTileDecoder *decoder,
    Av2hTileLumaLeaf *leaf,
    Av2hFirstCoeffBlock *block,
    uint8_t *has_leaf);
Av2hStatus av2h_tile_decoder_reconstruct_next_luma(
    Av2hTileDecoder *decoder,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch,
    Av2hTileLumaLeaf *leaf,
    uint8_t *has_leaf);
Av2hStatus av2h_tile_decoder_next_chroma(
    Av2hTileDecoder *decoder,
    Av2hTileChromaLeaf *leaf,
    Av2hFirstCoeffBlock *u_block,
    Av2hFirstCoeffBlock *v_block,
    uint8_t *has_leaf);
Av2hStatus av2h_tile_decoder_reconstruct_next_chroma(
    Av2hTileDecoder *decoder,
    Av2hReconstructionPlane *u_plane,
    Av2hReconstructionPlane *v_plane,
    Av2hReconstructionScratch *scratch,
    Av2hTileChromaLeaf *leaf,
    uint8_t *has_leaf);
Av2hStatus av2h_tile_decoder_finish_root(Av2hTileDecoder *decoder);
Av2hStatus av2h_tile_decoder_commit_leaf(
    Av2hTileDecoder *decoder,
    Av2hPartitionNode leaf);

#endif