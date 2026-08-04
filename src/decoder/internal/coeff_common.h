#ifndef AV2H_DECODER_INTERNAL_COEFF_COMMON_H
#define AV2H_DECODER_INTERNAL_COEFF_COMMON_H

static inline unsigned int av2h_coeff_minimum(
    unsigned int left, unsigned int right) {
    return left < right ? left : right;
}

#endif