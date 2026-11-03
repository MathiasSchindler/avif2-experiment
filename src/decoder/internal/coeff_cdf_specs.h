#ifndef AV2H_DECODER_INTERNAL_COEFF_CDF_SPECS_H
#define AV2H_DECODER_INTERNAL_COEFF_CDF_SPECS_H

#include <stdint.h>

typedef struct {
    uint8_t tx_context;
    uint8_t context;
} Av2hCoeffKey;

typedef struct {
    uint8_t tx_context;
    uint8_t context;
    uint16_t threshold[3U];
    uint8_t parameter;
} Av2hBaseSpec;

typedef struct {
    uint8_t tx_context;
    uint8_t context;
    uint16_t threshold[5U];
    uint8_t parameter;
} Av2hLfSpec;

typedef struct {
    uint8_t context;
    uint16_t threshold[3U];
    uint8_t parameter;
} Av2hBrSpec;

typedef struct {
    uint16_t threshold[3U];
    uint8_t parameter;
} Av2hFscCdfSpec;

#endif
