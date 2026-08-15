#include "encoder/cdf.h"

#define ENC_CDF_TOP 32768U
#define ENC_PARAMETER_COMBINATIONS 125U

static int parameter_adjustment(unsigned int digit) {
    static const int8_t adjustments[5] = {0, -1, -2, 1, 1};
    return adjustments[digit];
}

EncStatus enc_cdf_materialize(uint16_t *inverse_cdf, size_t capacity,
                              const uint16_t *thresholds,
                              size_t symbol_count, uint8_t parameter_index,
                              uint8_t count) {
    size_t index;
    unsigned int parameter = parameter_index;
    unsigned int rate_base;
    int first;
    int second;
    int third;

    if (inverse_cdf == 0 || thresholds == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (symbol_count < 2U || symbol_count > ENC_CDF_MAX_SYMBOLS ||
        capacity < symbol_count + 4U ||
        parameter >= ENC_PARAMETER_COMBINATIONS || count > 32U) {
        return ENC_INVALID_ARGUMENT;
    }
    for (index = 0U; index + 1U < symbol_count; ++index) {
        if (thresholds[index] == 0U || thresholds[index] >= ENC_CDF_TOP ||
            (index != 0U && thresholds[index - 1U] >= thresholds[index])) {
            return ENC_INVALID_ARGUMENT;
        }
    }
    first = parameter_adjustment(parameter / 25U);
    second = parameter_adjustment((parameter / 5U) % 5U);
    third = parameter_adjustment(parameter % 5U);
    rate_base = symbol_count < 4U ? 2U : 3U;
    for (index = 0U; index + 1U < symbol_count; ++index) {
        inverse_cdf[index] =
            (uint16_t)(ENC_CDF_TOP - thresholds[index]);
    }
    inverse_cdf[symbol_count - 1U] = 0U;
    inverse_cdf[symbol_count] = count;
    inverse_cdf[symbol_count + 1U] = (uint16_t)(rate_base + first);
    inverse_cdf[symbol_count + 2U] =
        (uint16_t)(rate_base + 1U + second);
    inverse_cdf[symbol_count + 3U] =
        (uint16_t)(rate_base + 2U + third);
    return ENC_OK;
}
