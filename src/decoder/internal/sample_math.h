#ifndef AV2H_DECODER_INTERNAL_SAMPLE_MATH_H
#define AV2H_DECODER_INTERNAL_SAMPLE_MATH_H

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

static inline int32_t av2h_sample_clamp(
    int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static inline uint16_t av2h_sample_clip_to_domain(
    int32_t value, const Av2hSampleDomain *domain) {
    return (uint16_t)av2h_sample_clamp(
        value, 0, (int32_t)domain->maximum);
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

#endif