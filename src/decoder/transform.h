#ifndef AV2H_DECODER_TRANSFORM_H
#define AV2H_DECODER_TRANSFORM_H

#include "coeff.h"

#define AV2H_PRIMARY_TX_TYPE_SYMBOLS 7U
#define AV2H_SECONDARY_TX_TYPE_SYMBOLS 4U

typedef enum {
    AV2H_TX_DCT_DCT = 0,
    AV2H_TX_DCT_ADST = 2,
    AV2H_TX_ADST_FLIPADST = 7,
    AV2H_TX_IDTX = 9,
    AV2H_TX_V_DCT = 10,
    AV2H_TX_H_ADST = 13,
    AV2H_TX_TYPE_INVALID = 255
} Av2hFirstTxType;

typedef struct {
    uint16_t primary_size0[AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t primary_size1[AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t secondary_32x32[AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U];
    uint16_t chroma_cross_type[7U + 4U];
} Av2hFirstTransformCdfs;

typedef struct {
    Av2hFirstTxType primary_type;
    uint8_t primary_signaled;
    uint8_t primary_symbol;
    uint8_t secondary_signaled;
    uint8_t secondary_symbol;
    size_t before_primary_bits;
    size_t after_primary_bits;
    size_t after_secondary_bits;
} Av2hFirstTransformType;

Av2hStatus av2h_first_transform_cdfs_init(Av2hFirstTransformCdfs *cdfs);
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

#endif