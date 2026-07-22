#ifndef AV2H_DECODER_COEFF_H
#define AV2H_DECODER_COEFF_H

#include "leaf.h"

typedef enum {
    AV2H_TX_4X4 = 0,
    AV2H_TX_8X8 = 1,
    AV2H_TX_32X32 = 3,
    AV2H_TX_8X4 = 6,
    AV2H_TX_INVALID = 255
} Av2hFirstTxSize;

typedef struct {
    uint16_t txb_skip_txs0[AV2H_BINARY_CDF_SIZE];
    uint16_t txb_skip_chroma_txs0_ctx6[AV2H_BINARY_CDF_SIZE];
    uint16_t txb_skip_chroma_v_ctx9[AV2H_BINARY_CDF_SIZE];
    uint16_t txb_skip_fsc_txs0_ctx9[AV2H_BINARY_CDF_SIZE];
    uint16_t txb_skip_txs1[AV2H_BINARY_CDF_SIZE];
    uint16_t txb_skip_txs3[AV2H_BINARY_CDF_SIZE];
    uint16_t eob_pt_16[5U + 4U];
    uint16_t eob_pt_16_chroma[5U + 4U];
    uint16_t eob_pt_32[6U + 4U];
    uint16_t eob_pt_64[7U + 4U];
    uint16_t eob_pt_1024[8U + 4U];
    uint16_t eob_extra[AV2H_BINARY_CDF_SIZE];
} Av2hFirstCoeffCdfs;

typedef struct {
    Av2hFirstTxSize tx_size;
    uint8_t tx_size_context;
    uint8_t txb_skip_context;
    uint8_t all_zero;
    uint8_t eob_point;
    uint8_t eob_offset_bits;
    uint16_t eob_extra;
    uint16_t eob;
    size_t before_all_zero_bits;
    size_t after_all_zero_bits;
    size_t after_eob_point_bits;
    size_t after_eob_extra_bit;
    size_t after_eob_bits;
} Av2hFirstCoeffPrefix;

Av2hStatus av2h_first_coeff_cdfs_init(Av2hFirstCoeffCdfs *cdfs);
Av2hStatus av2h_first_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    Av2hFirstCoeffPrefix *prefix);
Av2hStatus av2h_first_chroma_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    Av2hFirstCoeffPrefix *prefix);
Av2hStatus av2h_coeff_prefix_read(Av2hRangeDecoder *decoder,
                                  Av2hFirstCoeffCdfs *cdfs,
                                  const Av2hFrameState *frame,
                                  const Av2hPartitionNode *leaf,
                                  const Av2hFirstLeafMode *mode,
                                  uint8_t txb_skip_context,
                                  Av2hFirstCoeffPrefix *prefix);

#endif