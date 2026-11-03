#include "decoder/internal/sample_math.h"

#include <stdio.h>

static int failures;

#define CHECK(expression)                                      \
    do {                                                       \
        if (!(expression)) {                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n",     \
                    __FILE__, __LINE__, #expression);           \
            ++failures;                                        \
        }                                                      \
    } while (0)

static int32_t reference_floor(int64_t value, unsigned int shift) {
    int64_t divisor = INT64_C(1) << shift;
    return (int32_t)(value / divisor - (value % divisor < 0));
}

static void check_shifts(int64_t value, unsigned int shift) {
    int64_t half = shift == 0U ? 0 : INT64_C(1) << (shift - 1U);
    int64_t magnitude = value < 0 ? -value : value;
    int64_t symmetric = reference_floor(magnitude + half, shift);
    CHECK(av2h_sample_floor_shift(value, shift) == reference_floor(value, shift));
    CHECK(av2h_sample_round_pow2(value, (int)shift) ==
          reference_floor(value + half, shift));
    CHECK(av2h_sample_round_pow2_signed(value, (int)shift) ==
          (int32_t)(value < 0 ? -symmetric : symmetric));
}

static void test_rounding(void) {
    unsigned int shift;
    CHECK(av2h_sample_round_pow2(-1, 1) == 0);
    CHECK(av2h_sample_round_pow2_signed(-1, 1) == -1);
    CHECK(av2h_sample_floor_shift(-1, 1) == -1);
    CHECK(av2h_sample_round_pow2(UINT32_MAX, 2) == INT32_C(1073741824));
    for (shift = 0U; shift < 16U; ++shift) {
        int value;
        for (value = -8192; value <= 8192; ++value) {
            check_shifts(value, shift);
        }
    }
    for (shift = 0U; shift < 31U; ++shift) {
        int quotient;
        int64_t divisor = INT64_C(1) << shift;
        int64_t half = divisor >> 1U;
        for (quotient = -64; quotient <= 64; ++quotient) {
            int delta;
            for (delta = -1; delta <= 1; ++delta) {
                check_shifts(quotient * divisor + delta, shift);
                check_shifts(quotient * divisor + half + delta, shift);
            }
        }
        check_shifts(INT32_MIN, shift);
        check_shifts(INT32_MAX, shift);
    }
    CHECK(av2h_sample_floor_shift(INT64_MIN, 32U) == INT32_MIN);
    CHECK(av2h_sample_round_pow2(INT64_MIN, 32) == INT32_MIN);
    CHECK(av2h_sample_round_pow2_signed(INT64_MIN, 32) == INT32_MIN);
    CHECK(av2h_sample_floor_shift(INT64_MIN, 63U) == -1);
    CHECK(av2h_sample_floor_shift(INT64_MAX, 63U) == 0);
}

static void test_block_log2(void) {
    uint32_t size;
    for (size = 0U; size <= UINT16_MAX; ++size) {
        uint8_t expected = size == 4U ? 2U : size == 8U ? 3U :
            size == 16U ? 4U : size == 32U ? 5U : 6U;
        CHECK(av2h_sample_block_log2((uint16_t)size) == expected);
    }
}

int main(void) {
    test_rounding();
    test_block_log2();
    CHECK(av2h_sample_domain_8bit.bit_depth == 8U);
    CHECK(av2h_sample_domain_8bit.bytes_per_sample == 1U);
    CHECK(av2h_sample_domain_10bit.bit_depth == 10U);
    CHECK(av2h_sample_domain_10bit.bytes_per_sample == 2U);
    CHECK(av2h_sample_midpoint_8bit() == 128U);
    CHECK(av2h_sample_below_midpoint_8bit() == 127U);
    CHECK(av2h_sample_above_midpoint_8bit() == 129U);
    CHECK(av2h_sample_maximum_8bit() == 255U);
    CHECK(av2h_sample_clamp(-12, -8, 7) == -8);
    CHECK(av2h_sample_clamp(-8, -8, 7) == -8);
    CHECK(av2h_sample_clamp(3, -8, 7) == 3);
    CHECK(av2h_sample_clamp(7, -8, 7) == 7);
    CHECK(av2h_sample_clamp(12, -8, 7) == 7);
    CHECK(av2h_sample_clamp(-1, 0, -1) == 0);
    CHECK(av2h_sample_clamp(0, 0, -1) == -1);
    CHECK(av2h_sample_abs(-INT32_MAX) == INT32_MAX);
    CHECK(av2h_sample_abs(0) == 0);
    CHECK(av2h_sample_abs(INT32_MAX) == INT32_MAX);
    CHECK(av2h_sample_clip(-1) == 0U);
    CHECK(av2h_sample_clip(0) == 0U);
    CHECK(av2h_sample_clip(128) == 128U);
    CHECK(av2h_sample_clip(255) == 255U);
    CHECK(av2h_sample_clip(256) == 255U);
    CHECK(av2h_sample_clip(INT32_MIN) == 0U);
    CHECK(av2h_sample_clip(INT32_MAX) == 255U);
    CHECK(av2h_sample_midpoint_10bit() == 512U);
    CHECK(av2h_sample_below_midpoint_10bit() == 511U);
    CHECK(av2h_sample_above_midpoint_10bit() == 513U);
    CHECK(av2h_sample_maximum_10bit() == 1023U);
    CHECK(av2h_sample_clip_10bit(255) == 255U);
    CHECK(av2h_sample_clip_10bit(256) == 256U);
    CHECK(av2h_sample_clip_10bit(511) == 511U);
    CHECK(av2h_sample_clip_10bit(512) == 512U);
    CHECK(av2h_sample_clip_10bit(1022) == 1022U);
    CHECK(av2h_sample_clip_10bit(1023) == 1023U);
    CHECK(av2h_sample_clip_10bit(-1) == 0U);
    CHECK(av2h_sample_clip_10bit(1024) == 1023U);
    CHECK(av2h_sample_clip_10bit(INT32_MIN) == 0U);
    CHECK(av2h_sample_clip_10bit(INT32_MAX) == 1023U);
    if (failures != 0) return 1;
    puts("sample math unit: pass");
    return 0;
}