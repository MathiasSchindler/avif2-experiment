#ifndef AV2H_SHARED_BIT_MATH_H
#define AV2H_SHARED_BIT_MATH_H

#include <stdint.h>

/* The input covers bit_offset + bit_count bits; offset < 8 and count <= 32. */
static inline uint32_t av2h_read_bits_msb(const unsigned char *bytes, unsigned int bit_offset,
                                          unsigned int bit_count) {
    uint32_t value = 0U;
    unsigned int available = 8U - bit_offset;
    if (bit_count == 0U)
        return 0U;
    if (bit_count <= available)
        return ((uint32_t)*bytes >> (available - bit_count)) & ((1U << bit_count) - 1U);
    while (bit_count != 0U) {
        unsigned int count = bit_count < available ? bit_count : available;
        value =
            (value << count) | (((uint32_t)*bytes++ >> (available - count)) & ((1U << count) - 1U));
        bit_count -= count;
        available = 8U;
    }
    return value;
}

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
#if defined(__GNUC__) || defined(__clang__)
    return value == 0U ? 0U
                       : (unsigned int)__builtin_clzll(1ULL) - (unsigned int)__builtin_clzll(value);
#else
    unsigned int bits = 0U;
    while (value > 1U) {
        value >>= 1U;
        ++bits;
    }
    return bits;
#endif
}

/* Smallest shift with unit * 2^shift >= value; unit must be positive. */
static inline unsigned int av2h_ceil_log2_ratio_u32(uint32_t unit, uint32_t value) {
    unsigned int bits = 0U;
    uint64_t limit = unit;
    while (limit < value) {
        limit <<= 1U;
        ++bits;
    }
    return bits;
}

#endif
