#ifndef AV2H_DECODER_INTERNAL_SAMPLE_MATH_H
#define AV2H_DECODER_INTERNAL_SAMPLE_MATH_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t bit_depth;
    uint8_t bytes_per_sample;
    uint16_t midpoint;
    uint16_t maximum;
} Av2hSampleDomain;

static const Av2hSampleDomain av2h_sample_domain_8bit = {
    8U, 1U, 128U, 255U
};

static const Av2hSampleDomain av2h_sample_domain_10bit = {
    10U, 2U, 512U, 1023U
};

static const Av2hSampleDomain av2h_sample_domain_12bit = {12U, 2U, 2048U, 4095U};

static inline const Av2hSampleDomain *av2h_sample_domain_get(uint8_t bit_depth) {
    return bit_depth == 8U    ? &av2h_sample_domain_8bit
           : bit_depth == 10U ? &av2h_sample_domain_10bit
           : bit_depth == 12U ? &av2h_sample_domain_12bit
                              : NULL;
}

static inline const Av2hSampleDomain *av2h_sample_domain_from_idc(uint8_t bit_depth_idc) {
    return bit_depth_idc == 1U   ? &av2h_sample_domain_8bit
           : bit_depth_idc == 0U ? &av2h_sample_domain_10bit
           : bit_depth_idc == 2U ? &av2h_sample_domain_12bit
                                 : NULL;
}

static inline int av2h_sample_domain_valid(const Av2hSampleDomain *domain) {
    const Av2hSampleDomain *canonical;
    if (domain == NULL)
        return 0;
    canonical = av2h_sample_domain_get(domain->bit_depth);
    return canonical != NULL && domain->bytes_per_sample == canonical->bytes_per_sample &&
           domain->midpoint == canonical->midpoint && domain->maximum == canonical->maximum;
}

static inline int32_t av2h_sample_clamp(
    int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static inline int32_t av2h_sample_abs(int32_t value) {
    return value < 0 ? -value : value;
}

/* Prediction/transform dispatch retains six for sizes outside 4/8/16/32. */
static inline uint8_t av2h_sample_block_log2(uint16_t size) {
    return size == 4U ? 2U : size == 8U ? 3U :
        size == 16U ? 4U : size == 32U ? 5U : 6U;
}

static inline uint16_t av2h_sample_clip_to_domain(
    int32_t value, const Av2hSampleDomain *domain) {
    return (uint16_t)av2h_sample_clamp(
        value, 0, (int32_t)domain->maximum);
}

static inline uint16_t av2h_sample_q3_maximum(const Av2hSampleDomain *domain) {
    return (uint16_t)((domain->maximum << 3U) | 7U);
}

static inline uint8_t av2h_sample_midpoint_8bit(void) {
    return (uint8_t)av2h_sample_domain_8bit.midpoint;
}

static inline uint8_t av2h_sample_below_midpoint_8bit(void) {
    return (uint8_t)(av2h_sample_domain_8bit.midpoint - 1U);
}

static inline uint8_t av2h_sample_above_midpoint_8bit(void) {
    return (uint8_t)(av2h_sample_domain_8bit.midpoint + 1U);
}

static inline uint8_t av2h_sample_maximum_8bit(void) {
    return (uint8_t)av2h_sample_domain_8bit.maximum;
}

static inline uint8_t av2h_sample_clip(int32_t value) {
    return (uint8_t)av2h_sample_clip_to_domain(
        value, &av2h_sample_domain_8bit);
}

static inline uint16_t av2h_sample_midpoint_10bit(void) {
    return av2h_sample_domain_10bit.midpoint;
}

static inline uint16_t av2h_sample_below_midpoint_10bit(void) {
    return (uint16_t)(av2h_sample_domain_10bit.midpoint - 1U);
}

static inline uint16_t av2h_sample_above_midpoint_10bit(void) {
    return (uint16_t)(av2h_sample_domain_10bit.midpoint + 1U);
}

static inline uint16_t av2h_sample_maximum_10bit(void) {
    return av2h_sample_domain_10bit.maximum;
}

static inline uint16_t av2h_sample_clip_10bit(int32_t value) {
    return av2h_sample_clip_to_domain(value, &av2h_sample_domain_10bit);
}

/* Floor division by 2^shift without shifting a negative value; shift 0..63. */
static inline int32_t av2h_sample_floor_shift(int64_t value, unsigned int shift) {
    if (value >= 0) return (int32_t)((uint64_t)value >> shift);
    {
        uint64_t magnitude = (uint64_t)0U - (uint64_t)value;
        return (int32_t)(-(int64_t)((magnitude - 1U) >> shift) - 1);
    }
}

/* Half-up for nonnegative inputs; signed ties toward +infinity; shift 0..62. */
static inline int32_t av2h_sample_round_pow2(int64_t value, int shift) {
    return av2h_sample_floor_shift(
        value + (((int64_t)1 << shift) >> 1), (unsigned int)shift);
}

/* Symmetric rounding, with ties away from zero; shift 0..62. */
static inline int32_t av2h_sample_round_pow2_signed(int64_t value, int shift) {
    uint64_t magnitude;
    int64_t rounded;
    if (shift == 0) return (int32_t)value;
    magnitude = value < 0 ? (uint64_t)0U - (uint64_t)value : (uint64_t)value;
    rounded = (int64_t)((magnitude +
        ((uint64_t)1U << (shift - 1))) >> shift);
    return (int32_t)(value < 0 ? -rounded : rounded);
}

/* The reciprocal input is positive; shift receives its fixed-point scale. */
uint16_t av2h_sample_resolve_divisor_32(uint32_t divisor, int *shift);

/*
 * Approximate Q-shift slope clipped to +/-((2 << shift) - 1).
 * Statistics must have nonnegative variance; zero count yields zero.
 */
int32_t av2h_sample_regression_slope(
    int32_t sum_x,
    int32_t sum_y,
    int32_t sum_xx,
    int32_t sum_xy,
    int count,
    int shift);

#endif