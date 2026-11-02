#ifndef AV2H_DECODER_TRANSFORM_TYPE_H
#define AV2H_DECODER_TRANSFORM_TYPE_H

#include <stddef.h>
#include <stdint.h>

#define AV2H_PRIMARY_TX_TYPE_SYMBOLS 7U
#define AV2H_SECONDARY_TX_TYPE_SYMBOLS 4U
#define AV2H_SECONDARY_TX_SET_SYMBOLS 7U
#define AV2H_SECONDARY_ADST_TX_SET_SYMBOLS 4U
#define AV2H_INTER_TX_TYPE_INDEX_SYMBOLS 8U

typedef int32_t Av2hTransformIntermediate;

typedef int64_t Av2hTransformSum;

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

#endif
