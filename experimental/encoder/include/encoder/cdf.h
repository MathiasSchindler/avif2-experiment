#ifndef EXPERIMENTAL_ENCODER_CDF_H
#define EXPERIMENTAL_ENCODER_CDF_H

#include "encoder/base.h"

#define ENC_CDF_MAX_SYMBOLS 16U
#define ENC_CDF_MAX_SIZE (ENC_CDF_MAX_SYMBOLS + 4U)

EncStatus enc_cdf_materialize(uint16_t *inverse_cdf, size_t capacity,
                              const uint16_t *thresholds,
                              size_t symbol_count, uint8_t parameter_index,
                              uint8_t count);

#endif
