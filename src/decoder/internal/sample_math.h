#ifndef AV2H_DECODER_INTERNAL_SAMPLE_MATH_H
#define AV2H_DECODER_INTERNAL_SAMPLE_MATH_H

#include <stdint.h>

static inline int32_t av2h_sample_clamp(
    int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static inline uint8_t av2h_sample_clip(int32_t value) {
    return (uint8_t)av2h_sample_clamp(value, 0, 255);
}

#endif