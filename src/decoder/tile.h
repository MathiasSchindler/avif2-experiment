#ifndef AV2H_DECODER_TILE_H
#define AV2H_DECODER_TILE_H

#include "reconstruct.h"

#define AV2H_MAX_LOSSLESS_CHROMA_TX_NODES 1024U
#define AV2H_MAX_LUMA_PARTITION_NODES 2048U
#define AV2H_MAX_TX_PARTITION_NODES 5U
#define AV2H_TX_PARTITION_GROUPS 9U
#define AV2H_TX_PARTITION_TYPE_GROUPS 14U
#define AV2H_TX_PARTITION_TYPE_SYMBOLS 7U

typedef struct {
    size_t header_bits;
    size_t header_size;
    Av2hSpan entropy;
} Av2hTileGroup;

typedef struct {
    Av2hLumaModeContext context;
    uint8_t mode;
    int8_t angle_delta;
    uint8_t chroma_cfl;
    uint8_t chroma_smooth;
} Av2hTileLumaModeCell;

typedef struct {
    Av2hPartitionNode node;
    uint8_t partition;
} Av2hTileLumaPartitionNode;

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
    uint8_t *restoration_unit_flags;
    size_t restoration_unit_flag_capacity;
    int16_t *restoration_unit_taps;
    size_t restoration_unit_tap_capacity;
} Av2hTileContextStorage;

typedef struct {
    uint16_t restore_cdf[AV2H_BINARY_CDF_SIZE];
    uint16_t pc_wiener_restore_cdf[AV2H_BINARY_CDF_SIZE];
    uint16_t switchable_restore_cdf[2U][AV2H_BINARY_CDF_SIZE];
    uint16_t length_cdf[2U][AV2H_BINARY_CDF_SIZE];
    uint16_t uv_sym_cdf[AV2H_BINARY_CDF_SIZE];
    uint16_t four_part_cdf[8U];
    uint8_t *unit_flags[AV2H_FRAME_PLANE_COUNT];
    int16_t *unit_taps[AV2H_FRAME_PLANE_COUNT];
    size_t unit_capacity[AV2H_FRAME_PLANE_COUNT];
    size_t decoded_count[AV2H_FRAME_PLANE_COUNT];
    uint16_t unit_columns[AV2H_FRAME_PLANE_COUNT];
    uint16_t unit_rows[AV2H_FRAME_PLANE_COUNT];
    uint16_t unit_size[AV2H_FRAME_PLANE_COUNT];
    int16_t bank[AV2H_FRAME_PLANE_COUNT][4U]
                [AV2H_WIENER_NONSEP_MAX_TAPS];
    uint8_t bank_size[AV2H_FRAME_PLANE_COUNT];
    uint8_t bank_pointer[AV2H_FRAME_PLANE_COUNT];
    uint16_t superblock_mi;
    uint8_t active_mask;
} Av2hRestorationEntropyState;

typedef struct {
    uint16_t do_partition[2U][2U][AV2H_TX_PARTITION_GROUPS]
                          [AV2H_BINARY_CDF_SIZE];
    uint16_t two_or_three_way[2U][2U][2U][AV2H_BINARY_CDF_SIZE];
    uint16_t partition_type[2U][2U][AV2H_TX_PARTITION_TYPE_GROUPS]
                            [AV2H_TX_PARTITION_TYPE_SYMBOLS + 4U];
} Av2hTxPartitionCdfs;

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
    uint8_t decoded_plane;
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
    Av2hRestorationEntropyState restoration_entropy;
    Av2hTxPartitionCdfs tx_partition_cdfs;
} Av2hTileEntropyState;

typedef struct {
    Av2hPartitionTraversal luma_traversal;
    Av2hPartitionTraversal chroma_traversal;
    Av2hTileLumaPartitionNode
        luma_partition_nodes[AV2H_MAX_LUMA_PARTITION_NODES];
    size_t luma_partition_node_count;
    Av2hTileLumaModeCell *colocated_luma;
    size_t colocated_luma_count;
    size_t mi_columns;
    size_t mi_rows;
} Av2hTileTraversalState;

typedef struct {
    uint16_t superblock_mi;
    uint16_t sb_row;
    uint16_t sb_column;
    Av2hQIndex current_q_index;
    Av2hQIndex current_block_q_index;
    size_t sb_index;
    size_t sb_count;
} Av2hTileSuperblockState;

typedef struct {
    Av2hBlockSize root_block_size;
    Av2hPartitionType luma_root_partition;
    Av2hPartitionType chroma_root_partition;
    Av2hPartitionType chroma_root_forced_partition;
    Av2hPartitionType square_child_luma_partition;
    uint8_t luma_root_child_count;
    uint8_t luma_root_children_share;
    uint8_t square_child_luma_child_count;
    uint8_t square_child_luma_children_share;
    uint8_t square_root_child_index;
} Av2hTileRootState;

typedef struct {
    Av2hPartitionNode coding_node;
    Av2hFirstLeafMode coding_mode;
    uint16_t tx_index;
    uint16_t tx_count;
    uint8_t tx_partition;
    Av2hPartitionNode tx_nodes[AV2H_MAX_TX_PARTITION_NODES];
    uint8_t selected_tx_count;
} Av2hTileLumaState;

typedef struct {
    Av2hPartitionNode coding_node;
    Av2hFirstChromaMode coding_mode;
    uint16_t tx_index;
    uint16_t tx_count;
    uint16_t tx_unit_start;
    uint8_t tx_plane;
    uint16_t u_eob;
    uint16_t u_eobs[AV2H_MAX_LOSSLESS_CHROMA_TX_NODES];
    uint8_t shared_resume_luma;
    uint8_t shared_complete;
    uint8_t shared_mode_pending;
    uint8_t shared_commit_luma;
} Av2hTileChromaState;

typedef struct {
    Av2hPartitionNode node;
    Av2hFirstChromaMode mode;
} Av2hTileSharedChromaState;

typedef struct {
    Av2hTileDecodePhase phase;
    Av2hTileDecodeEvent event;
    Av2hStatus status;
} Av2hTileControlState;

typedef struct {
    Av2hTileEntropyState entropy_state;
    Av2hTileTraversalState traversal;
    Av2hTileSuperblockState superblock;
    Av2hTileRootState root;
    Av2hTileLumaState luma;
    Av2hTileChromaState chroma;
    Av2hTileSharedChromaState shared_chroma;
    Av2hTileControlState control;
    struct Av2hDecodeDiagnostics *diagnostics;
} Av2hTileDecoder;

Av2hStatus av2h_tile_luma_tx_node_get(
    const Av2hTileDecoder *decoder, uint16_t index,
    Av2hPartitionNode *node);

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
Av2hStatus av2h_tile_decoder_read_shared_chroma_mode(
    Av2hTileDecoder *decoder,
    const Av2hPartitionNode *node);
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