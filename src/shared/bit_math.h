#ifndef AV2H_SHARED_BIT_MATH_H
#define AV2H_SHARED_BIT_MATH_H

#include <stdint.h>

/* Zero and one need no bits to distinguish one possible value. */
static inline unsigned int av2h_ceil_log2_u32(uint32_t value) {
    unsigned int bits = 0U;
    uint32_t probe = value > 0U ? value - 1U : 0U;
    while (probe != 0U) {
        ++bits;
        probe >>= 1U;
    }
    return bits;
}

/* Zero retains the decoder's existing zero-result convention. */
static inline unsigned int av2h_floor_log2_u32(uint32_t value) {
    unsigned int bits = 0U;
    while (value > 1U) {
        value >>= 1U;
        ++bits;
    }
    return bits;
}

/* Smallest shift with unit * 2^shift >= value; unit must be positive. */
static inline unsigned int av2h_ceil_log2_ratio_u32(
    uint32_t unit, uint32_t value) {
    unsigned int bits = 0U;
    uint64_t limit = unit;
    while (limit < value) {
        limit <<= 1U;
        ++bits;
    }
    return bits;
}

#endif
