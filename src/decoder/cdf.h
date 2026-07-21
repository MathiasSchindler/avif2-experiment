#ifndef AV2H_DECODER_CDF_H
#define AV2H_DECODER_CDF_H

#include "base.h"

#define AV2H_BINARY_CDF_SIZE 6U

typedef struct {
    uint16_t threshold;
    uint8_t parameter_index;
    uint8_t count;
} Av2hBinaryCdfSpec;

Av2hStatus av2h_binary_cdf_init(
    uint16_t inverse_cdf[AV2H_BINARY_CDF_SIZE],
    Av2hBinaryCdfSpec specification);

#endif