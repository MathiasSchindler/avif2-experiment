#ifndef AV2H_DECODER_TRANSFORM_H
#define AV2H_DECODER_TRANSFORM_H

#include "cdf.h"
#include "coeff_block.h"
#include "frame.h"
#include "leaf_mode.h"
#include "range.h"
#include "transform_type.h"

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
    uint8_t cctx_allowed,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform);

#endif