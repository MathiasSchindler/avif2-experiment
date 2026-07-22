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
    size_t after_bits;
} Av2hPartitionDecision;

typedef struct {
    Av2hPartitionDecision decisions[AV2H_MAX_FIRST_LEAF_PARTITIONS];
    size_t decision_count;
    Av2hPartitionNode leaf;
} Av2hFirstLeafPath;

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

#endif