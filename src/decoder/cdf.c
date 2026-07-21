#include "cdf.h"

#define AV2H_CDF_TOP 32768U
#define AV2H_PARAMETER_COMBINATIONS 125U

static int parameter_adjustment(unsigned int digit) {
    static const int8_t adjustments[5] = { 0, -1, -2, 1, 1 };
    return adjustments[digit];
}

Av2hStatus av2h_binary_cdf_init(
    uint16_t inverse_cdf[AV2H_BINARY_CDF_SIZE],
    Av2hBinaryCdfSpec specification) {
    unsigned int parameter = specification.parameter_index;
    int first;
    int second;
    int third;
    if (inverse_cdf == 0) return AV2H_INVALID_ARGUMENT;
    if (specification.threshold == 0U ||
        specification.threshold >= AV2H_CDF_TOP ||
        parameter >= AV2H_PARAMETER_COMBINATIONS ||
        specification.count > 32U) {
        return AV2H_INVALID_DATA;
    }
    first = parameter_adjustment(parameter / 25U);
    second = parameter_adjustment((parameter / 5U) % 5U);
    third = parameter_adjustment(parameter % 5U);
    inverse_cdf[0] =
        (uint16_t)(AV2H_CDF_TOP - specification.threshold);
    inverse_cdf[1] = 0U;
    inverse_cdf[2] = specification.count;
    inverse_cdf[3] = (uint16_t)(2 + first);
    inverse_cdf[4] = (uint16_t)(3 + second);
    inverse_cdf[5] = (uint16_t)(4 + third);
    return AV2H_OK;
}