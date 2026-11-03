#ifndef AV2H_DECODER_CDF_H
#define AV2H_DECODER_CDF_H

#include "base.h"

#define AV2H_BINARY_CDF_SIZE 6U
#define AV2H_MAX_CDF_SYMBOLS 16U
#define AV2H_MAX_CDF_SIZE (AV2H_MAX_CDF_SYMBOLS + 4U)

typedef struct {
    uint16_t threshold;
    uint8_t parameter_index;
    uint8_t count;
} Av2hBinaryCdfSpec;

Av2hStatus av2h_cdf_init(uint16_t *inverse_cdf, size_t inverse_cdf_size, const uint16_t *thresholds,
                         size_t symbol_count, uint8_t parameter_index, uint8_t count);

Av2hStatus av2h_binary_cdf_init(uint16_t inverse_cdf[AV2H_BINARY_CDF_SIZE],
                                Av2hBinaryCdfSpec specification);

#endif