#ifndef AV2H_DECODER_TX_GEOMETRY_H
#define AV2H_DECODER_TX_GEOMETRY_H

#include "coeff.h"

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
    Av2hFirstTxSize tx_size;
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
    Av2hFirstTxSize tx_size,
    const Av2hTxDescriptor **descriptor);
Av2hStatus av2h_luma_tx_descriptor_get(
    Av2hBlockSize block_size,
    const Av2hTxDescriptor **descriptor);
Av2hStatus av2h_luma_tx_node_count(
    Av2hBlockSize block_size,
    uint8_t *count);
Av2hStatus av2h_luma_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint8_t index,
    Av2hPartitionNode *tx_node);
Av2hStatus av2h_luma_lossless_tx_node_count(
    Av2hBlockSize block_size,
    uint8_t *count);
Av2hStatus av2h_luma_lossless_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint8_t index,
    Av2hPartitionNode *tx_node);
Av2hStatus av2h_chroma_tx_descriptor_get(
    Av2hBlockSize block_size,
    uint8_t chroma_format_idc,
    const Av2hTxDescriptor **descriptor);
Av2hStatus av2h_chroma_tx_node_count(
    Av2hBlockSize block_size,
    uint8_t chroma_format_idc,
    uint8_t *count);
Av2hStatus av2h_chroma_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint8_t chroma_format_idc,
    uint8_t index,
    Av2hPartitionNode *tx_node);
Av2hStatus av2h_chroma_lossless_tx_node_count(
    Av2hBlockSize block_size,
    uint8_t chroma_format_idc,
    uint8_t *count);
Av2hStatus av2h_chroma_lossless_tx_node_get(
    const Av2hPartitionNode *coding_node,
    uint8_t chroma_format_idc,
    uint8_t index,
    Av2hPartitionNode *tx_node);

#endif
