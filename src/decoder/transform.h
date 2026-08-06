#ifndef AV2H_DECODER_TRANSFORM_H
#define AV2H_DECODER_TRANSFORM_H

#include "coeff.h"

#define AV2H_PRIMARY_TX_TYPE_SYMBOLS 7U
#define AV2H_SECONDARY_TX_TYPE_SYMBOLS 4U
#define AV2H_SECONDARY_TX_SET_SYMBOLS 7U
#define AV2H_SECONDARY_ADST_TX_SET_SYMBOLS 4U
#define AV2H_INTER_TX_TYPE_INDEX_SYMBOLS 8U

typedef enum {
    AV2H_TX_DCT_DCT = 0,
    AV2H_TX_ADST_DCT = 1,
    AV2H_TX_DCT_ADST = 2,
    AV2H_TX_ADST_ADST = 3,
    AV2H_TX_FLIPADST_DCT = 4,
    AV2H_TX_DCT_FLIPADST = 5,
    AV2H_TX_FLIPADST_FLIPADST = 6,
    AV2H_TX_ADST_FLIPADST = 7,
    AV2H_TX_FLIPADST_ADST = 8,
    AV2H_TX_IDTX = 9,
    AV2H_TX_V_DCT = 10,
    AV2H_TX_H_DCT = 11,
    AV2H_TX_V_ADST = 12,
    AV2H_TX_H_ADST = 13,
    AV2H_TX_V_FLIPADST = 14,
    AV2H_TX_H_FLIPADST = 15,
    AV2H_TX_TYPE_INVALID = 255
} Av2hFirstTxType;

typedef struct {
    uint16_t primary_size0[AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t primary_size1[AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t primary_short_side_4[4U + 4U];
    uint16_t primary_size2[AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t primary_long_side_32[AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_long_side_32[AV2H_BINARY_CDF_SIZE];
    uint16_t primary_short_side_8[4U + 4U];
    uint16_t primary_short_side_16[4U + 4U];
    uint16_t intrabc_short_side[3U][3U][4U + 4U];
    uint16_t intrabc_type_set[3U][2U][AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_type_set2[3U][AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_type_index
        [3U][AV2H_INTER_TX_TYPE_INDEX_SYMBOLS + 4U];
    uint16_t intrabc_type_index2
        [3U][AV2H_INTER_TX_TYPE_INDEX_SYMBOLS + 4U];
    uint16_t intrabc_type_offset
        [3U][AV2H_INTER_TX_TYPE_INDEX_SYMBOLS + 4U];
    uint16_t intrabc_type_offset2[3U][4U + 4U];
    uint16_t intrabc_32x32_type[AV2H_BINARY_CDF_SIZE];
    uint16_t secondary_4x4[AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t secondary_8x8[AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t secondary_16x16[AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t secondary_32x32[AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t secondary_64x64[AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t intrabc_secondary[5U][AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t secondary_set[AV2H_SECONDARY_TX_SET_SYMBOLS + 4U];
    uint16_t secondary_adst_set[AV2H_SECONDARY_ADST_TX_SET_SYMBOLS + 4U];
    uint16_t chroma_cross_type[7U + 4U];
} Av2hFirstTransformCdfs;

typedef struct {
    Av2hFirstTxType primary_type;
    uint8_t primary_signaled;
    uint8_t primary_symbol;
    uint8_t primary_long_side_symbol;
    uint8_t secondary_signaled;
    uint8_t secondary_symbol;
    uint8_t secondary_set_symbol;
    uint8_t secondary_set;
    uint8_t cctx_type;
    size_t before_primary_bits;
    size_t after_primary_bits;
    size_t after_secondary_type_bits;
    size_t after_secondary_bits;
} Av2hFirstTransformType;

Av2hStatus av2h_first_transform_cdfs_init(Av2hFirstTransformCdfs *cdfs);
Av2hStatus av2h_primary_tx_type_map(
    Av2hFirstTxSize tx_size,
    Av2hFirstLeafIntraMode mode,
    uint8_t long_side_symbol,
    uint8_t primary_symbol,
    Av2hFirstTxType *tx_type);
Av2hStatus av2h_secondary_tx_type_signal(
    const Av2hSequenceState *sequence,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTxType primary_type,
    uint8_t *signaled);
Av2hStatus av2h_first_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform);
Av2hStatus av2h_first_chroma_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hFrameState *frame,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform);
Av2hStatus av2h_chroma_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hFirstChromaMode *mode,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform);

#endif