#ifndef AV2H_DECODER_TX_GEOMETRY_H
#define AV2H_DECODER_TX_GEOMETRY_H

#include "block_geometry.h"

typedef enum {
    AV2H_TX_4X4 = 0,
    AV2H_TX_8X8 = 1,
    AV2H_TX_16X16 = 2,
    AV2H_TX_32X32 = 3,
    AV2H_TX_64X64 = 4,
    AV2H_TX_4X8 = 5,
    AV2H_TX_8X4 = 6,
    AV2H_TX_8X16 = 7,
    AV2H_TX_16X8 = 8,
    AV2H_TX_16X32 = 9,
    AV2H_TX_32X16 = 10,
    AV2H_TX_32X64 = 11,
    AV2H_TX_64X32 = 12,
    AV2H_TX_4X16 = 13,
    AV2H_TX_16X4 = 14,
    AV2H_TX_8X32 = 15,
    AV2H_TX_32X8 = 16,
    AV2H_TX_16X64 = 17,
    AV2H_TX_64X16 = 18,
    AV2H_TX_4X32 = 19,
    AV2H_TX_32X4 = 20,
    AV2H_TX_8X64 = 21,
    AV2H_TX_64X8 = 22,
    AV2H_TX_4X64 = 23,
    AV2H_TX_64X4 = 24,
    AV2H_TX_SIZE_COUNT = 25,
    AV2H_TX_INVALID = 255
} Av2hTxSize;

typedef enum {
    AV2H_TX_SQUARE_4 = 0,
    AV2H_TX_SQUARE_8 = 1,
    AV2H_TX_SQUARE_16 = 2,
    AV2H_TX_SQUARE_32 = 3,
    AV2H_TX_SQUARE_64 = 4
} Av2hTxSquareClass;

typedef enum {
    AV2H_TX_EOB_16 = 0,
    AV2H_TX_EOB_32 = 1,
    AV2H_TX_EOB_64 = 2,
    AV2H_TX_EOB_128 = 3,
    AV2H_TX_EOB_256 = 4,
    AV2H_TX_EOB_512 = 5,
    AV2H_TX_EOB_1024 = 6
} Av2hTxEobClass;

typedef enum {
    AV2H_TX_SIGNAL_INTRA_4,
    AV2H_TX_SIGNAL_INTRA_8,
    AV2H_TX_SIGNAL_INTRA_16,
    AV2H_TX_SIGNAL_DCT_ONLY,
    AV2H_TX_SIGNAL_WIDE_32,
    AV2H_TX_SIGNAL_HIGH_32,
    AV2H_TX_SIGNAL_WIDE_64,
    AV2H_TX_SIGNAL_HIGH_64
} Av2hTxSignalingClass;

typedef struct {
    Av2hTxSize tx_size;
    uint8_t output_width;
    uint8_t output_height;
    uint8_t coefficient_width;
    uint8_t coefficient_height;
    uint8_t tx_size_context;
    Av2hTxSquareClass square_class;
    Av2hTxSquareClass square_up_class;
    Av2hTxEobClass eob_class;
    uint16_t eob_capacity;
    Av2hTxSignalingClass signaling_class;
} Av2hTxDescriptor;

Av2hStatus av2h_tx_descriptor_get(
    Av2hTxSize tx_size,
    const Av2hTxDescriptor **descriptor);
Av2hStatus av2h_luma_tx_descriptor_get(
    Av2hBlockSize block_size,
    const Av2hTxDescriptor **descriptor);
Av2hStatus av2h_luma_tx_node_count(
    Av2hBlockSize block_size,
    uint16_t *count);
Av2hStatus av2h_luma_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint16_t index,
    Av2hPartitionNode *tx_node);
Av2hStatus av2h_luma_lossless_tx_node_count(
    const Av2hPartitionNode *coding_node,
    uint16_t mi_columns,
    uint16_t mi_rows,
    uint16_t *count);
Av2hStatus av2h_luma_lossless_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint16_t mi_columns,
    uint16_t mi_rows,
    uint16_t index,
    Av2hPartitionNode *tx_node);
Av2hStatus av2h_chroma_tx_descriptor_get(
    Av2hBlockSize block_size,
    uint8_t chroma_format_idc,
    const Av2hTxDescriptor **descriptor);
Av2hStatus av2h_chroma_tx_node_count(
    Av2hBlockSize block_size,
    uint8_t chroma_format_idc,
    uint16_t *count);
Av2hStatus av2h_chroma_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint8_t chroma_format_idc,
    uint16_t index,
    Av2hPartitionNode *tx_node);
Av2hStatus av2h_chroma_lossless_tx_node_count(
    const Av2hPartitionNode *coding_node,
    uint16_t mi_columns,
    uint16_t mi_rows,
    uint8_t chroma_format_idc,
    uint16_t *count);
Av2hStatus av2h_chroma_lossless_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint16_t mi_columns,
    uint16_t mi_rows,
    uint8_t chroma_format_idc,
    uint16_t index,
    Av2hPartitionNode *tx_node);

#endif
