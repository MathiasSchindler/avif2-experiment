#ifndef AV2H_DECODER_PARTITION_H
#define AV2H_DECODER_PARTITION_H

#include "partition_cdf.h"
#include "range.h"

typedef enum {
    AV2H_BLOCK_4X4,
    AV2H_BLOCK_4X8,
    AV2H_BLOCK_8X4,
    AV2H_BLOCK_8X8,
    AV2H_BLOCK_8X16,
    AV2H_BLOCK_16X8,
    AV2H_BLOCK_16X16,
    AV2H_BLOCK_16X32,
    AV2H_BLOCK_32X16,
    AV2H_BLOCK_32X32,
    AV2H_BLOCK_32X64,
    AV2H_BLOCK_64X32,
    AV2H_BLOCK_64X64,
    AV2H_BLOCK_64X128,
    AV2H_BLOCK_128X64,
    AV2H_BLOCK_128X128,
    AV2H_BLOCK_128X256,
    AV2H_BLOCK_256X128,
    AV2H_BLOCK_256X256,
    AV2H_BLOCK_4X16,
    AV2H_BLOCK_16X4,
    AV2H_BLOCK_8X32,
    AV2H_BLOCK_32X8,
    AV2H_BLOCK_16X64,
    AV2H_BLOCK_64X16,
    AV2H_BLOCK_4X32,
    AV2H_BLOCK_32X4,
    AV2H_BLOCK_8X64,
    AV2H_BLOCK_64X8,
    AV2H_BLOCK_SIZE_COUNT,
    AV2H_BLOCK_INVALID = 255
} Av2hBlockSize;

typedef enum {
    AV2H_PARTITION_NONE,
    AV2H_PARTITION_HORZ,
    AV2H_PARTITION_VERT,
    AV2H_PARTITION_HORZ_3,
    AV2H_PARTITION_VERT_3,
    AV2H_PARTITION_HORZ_4A,
    AV2H_PARTITION_HORZ_4B,
    AV2H_PARTITION_VERT_4A,
    AV2H_PARTITION_VERT_4B,
    AV2H_PARTITION_SPLIT,
    AV2H_PARTITION_INVALID = 255
} Av2hPartitionType;

typedef enum {
    AV2H_PARTITION_HORIZONTAL = 0,
    AV2H_PARTITION_VERTICAL = 1
} Av2hPartitionAxis;

typedef struct {
    uint16_t row;
    uint16_t column;
    Av2hBlockSize block_size;
} Av2hPartitionNode;

#define AV2H_MAX_FORCED_PARTITION_ANCESTORS 8U
#define AV2H_MAX_FIRST_LEAF_PARTITIONS 16U
#define AV2H_MAX_PARTITION_TRAVERSAL_NODES 32U

typedef struct {
    Av2hPartitionNode node;
    Av2hPartitionType partition;
} Av2hForcedPartitionStep;

typedef struct {
    Av2hForcedPartitionStep steps[AV2H_MAX_FORCED_PARTITION_ANCESTORS];
    size_t step_count;
    Av2hPartitionNode next;
} Av2hForcedPartitionPath;

typedef struct {
    Av2hPartitionAxis axis;
    size_t after_split_bits;
    size_t after_axis_bits;
    size_t after_extension_bits;
} Av2hOrdinaryPartitionPrefix;

typedef struct {
    Av2hForcedPartitionPath forced;
    size_t split_context;
    size_t rect_type_context;
    size_t extension_context;
    Av2hOrdinaryPartitionPrefix ordinary;
} Av2hFirstPartitionPrefix;

typedef struct {
    Av2hPartitionNode node;
    Av2hPartitionType partition;
    uint8_t forced;
    uint8_t axis_implied;
    uint8_t extension_implied;
    size_t split_context;
    size_t rect_type_context;
    size_t extension_context;
    size_t after_bits;
} Av2hPartitionDecision;

typedef struct {
    Av2hPartitionDecision decisions[AV2H_MAX_FIRST_LEAF_PARTITIONS];
    size_t decision_count;
    Av2hPartitionNode leaf;
} Av2hFirstLeafPath;

typedef struct {
    uint8_t *above[AV2H_PARTITION_STRUCTURE_COUNT];
    uint8_t *left[AV2H_PARTITION_STRUCTURE_COUNT];
    size_t above_count;
    size_t left_count;
} Av2hPartitionContextState;

typedef struct {
    Av2hPartitionNode stack[AV2H_MAX_PARTITION_TRAVERSAL_NODES];
    Av2hPartitionNode pending_leaf;
    size_t stack_count;
    uint16_t mi_rows;
    uint16_t mi_columns;
    uint8_t structure;
    uint8_t enable_ext_partitions;
    uint8_t enable_uneven_4way_partitions;
    uint8_t has_pending_leaf;
    Av2hPartitionNode diagnostic_node;
    size_t diagnostic_before_bits;
} Av2hPartitionTraversal;

Av2hStatus av2h_block_dimensions_mi(Av2hBlockSize block_size,
                                     size_t *width,
                                     size_t *height);

Av2hStatus av2h_partition_forced_path(uint16_t mi_rows,
                                       uint16_t mi_columns,
                                       Av2hPartitionNode root,
                                       Av2hForcedPartitionPath *path);
Av2hStatus av2h_partition_contexts(Av2hBlockSize block_size,
                                    unsigned int left_smaller,
                                    unsigned int above_smaller,
                                    unsigned int first_half_smaller,
                                    unsigned int second_half_smaller,
                                    size_t *split_context,
                                    size_t *rect_type_context,
                                    size_t *extension_context);
Av2hStatus av2h_partition_context_state_init(
    Av2hPartitionContextState *state,
    uint8_t *luma_above,
    uint8_t *luma_left,
    uint8_t *chroma_above,
    uint8_t *chroma_left,
    size_t above_count,
    size_t left_count);
Av2hStatus av2h_partition_contexts_from_state(
    const Av2hPartitionContextState *state,
    size_t structure,
    Av2hPartitionNode node,
    Av2hPartitionAxis axis,
    size_t *split_context,
    size_t *rect_type_context,
    size_t *extension_context);
Av2hStatus av2h_partition_context_state_update(
    Av2hPartitionContextState *state,
    size_t structure,
    Av2hPartitionNode leaf);
Av2hStatus av2h_partition_traversal_init(
    Av2hPartitionTraversal *traversal,
    uint16_t mi_rows,
    uint16_t mi_columns,
    Av2hPartitionNode root,
    size_t structure,
    unsigned int enable_ext_partitions,
    unsigned int enable_uneven_4way_partitions);
Av2hStatus av2h_partition_traversal_reset_root(
    Av2hPartitionTraversal *traversal,
    Av2hPartitionNode root);
Av2hStatus av2h_partition_child_nodes(
    Av2hPartitionNode node,
    Av2hPartitionType partition,
    Av2hPartitionNode children[4U],
    size_t *child_count);
Av2hStatus av2h_partition_traversal_force(
    Av2hPartitionTraversal *traversal,
    Av2hPartitionType partition,
    uint8_t *has_leaf);
Av2hStatus av2h_partition_traversal_next(
    Av2hPartitionTraversal *traversal,
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    const Av2hPartitionContextState *state,
    Av2hFirstLeafPath *path,
    uint8_t *has_leaf);
Av2hStatus av2h_partition_traversal_commit_leaf(
    Av2hPartitionTraversal *traversal,
    Av2hPartitionContextState *state,
    Av2hPartitionNode leaf);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
typedef struct {
    uint8_t stage;
    uint8_t mode;
    uint16_t eob;
    size_t tell;
} Av2hPartitionLeafDiagnostic;
const Av2hPartitionDecision *av2h_partition_diagnostic_trace(size_t *count);
const uint8_t *av2h_partition_diagnostic_trace_structure(void);
void av2h_partition_diagnostic_forced_decision(
    Av2hPartitionNode node,
    Av2hPartitionType partition,
    uint8_t structure);
void av2h_partition_diagnostic_leaf_stage(
    Av2hPartitionNode node, uint8_t stage, uint8_t mode, uint16_t eob,
    size_t tell);
const Av2hPartitionLeafDiagnostic *av2h_partition_diagnostic_leaf_trace(
    size_t *count);
#endif
Av2hStatus av2h_partition_first_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    uint16_t frame_width,
    uint16_t frame_height,
    Av2hFirstPartitionPrefix *prefix);
Av2hStatus av2h_partition_first_leaf_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    uint16_t frame_width,
    uint16_t frame_height,
    unsigned int enable_ext_partitions,
    Av2hFirstLeafPath *path);

Av2hStatus av2h_partition_ordinary_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    Av2hOrdinaryPartitionPrefix *prefix);
Av2hStatus av2h_partition_node_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    Av2hPartitionNode node,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    unsigned int enable_ext_partitions,
    Av2hPartitionDecision *decision,
    Av2hPartitionNode *child);
Av2hStatus av2h_partition_node_read_for_structure(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    size_t structure,
    Av2hPartitionNode node,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    unsigned int enable_ext_partitions,
    Av2hPartitionDecision *decision,
    Av2hPartitionNode *child);
Av2hStatus av2h_partition_node_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    const Av2hPartitionContextState *state,
    size_t structure,
    Av2hPartitionNode node,
    uint16_t mi_rows,
    uint16_t mi_columns,
    unsigned int enable_ext_partitions,
    unsigned int enable_uneven_4way_partitions,
    Av2hPartitionDecision *decision,
    Av2hPartitionNode *child);

#endif