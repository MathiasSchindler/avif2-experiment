#include "decoder/internal/sample_math.h"

#include <stdio.h>

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    unsigned int byte;
    for (byte = 0U; byte < 4U; ++byte) {
        hash ^= (value >> (byte * 8U)) & 255U;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t divisor_case(uint64_t hash, uint32_t divisor) {
    int shift;
    uint16_t scale = av2h_sample_resolve_divisor_32(divisor, &shift);
    hash = hash_u32(hash, divisor);
    hash = hash_u32(hash, scale);
    return hash_u32(hash, (uint32_t)shift);
}

int main(void) {
    static const int counts[] = { 0, 1, 2, 8, 32 };
    uint64_t divisor_hash = UINT64_C(14695981039346656037);
    uint64_t regression_hash = UINT64_C(14695981039346656037);
    uint32_t divisor;
    unsigned int bit;
    size_t index;
    for (divisor = 1U; divisor <= 65536U; ++divisor) {
        divisor_hash = divisor_case(divisor_hash, divisor);
    }
    for (bit = 0U; bit < 32U; ++bit) {
        int delta;
        for (delta = -1; delta <= 1; ++delta) {
            int64_t value = ((int64_t)1 << bit) + delta;
            if (value > 0) {
                divisor_hash = divisor_case(divisor_hash, (uint32_t)value);
            }
        }
    }
    divisor_hash = divisor_case(divisor_hash, UINT32_MAX);
    for (index = 0U; index < sizeof(counts) / sizeof(counts[0]); ++index) {
        int slope;
        for (slope = -4; slope <= 4; ++slope) {
            int shift;
            for (shift = 0; shift <= 12; ++shift) {
                int shape;
                for (shape = 0; shape < 8; ++shape) {
                    int32_t sum_x = 0;
                    int32_t sum_y = 0;
                    int32_t sum_xx = 0;
                    int32_t sum_xy = 0;
                    int sample;
                    for (sample = 0; sample < counts[index]; ++sample) {
                        int32_t x = (sample * sample + 3 * sample +
                                     shape * 7) % 1024;
                        int32_t y = slope * x + shape - 4;
                        sum_x += x;
                        sum_y += y;
                        sum_xx += x * x;
                        sum_xy += x * y;
                    }
                    regression_hash = hash_u32(
                        regression_hash, (uint32_t)av2h_sample_regression_slope(
                            sum_x, sum_y, sum_xx, sum_xy,
                            counts[index], shift));
                }
            }
        }
    }
    printf("prediction math: divisor=%016llx regression=%016llx\n",
           (unsigned long long)divisor_hash,
           (unsigned long long)regression_hash);
    if (divisor_hash != UINT64_C(0x3cc8148f050d5c66) ||
        regression_hash != UINT64_C(0xd6f776f1ee629f75)) {
        fputs("prediction math characterization mismatch\n", stderr);
        return 1;
    }
    return 0;
}
