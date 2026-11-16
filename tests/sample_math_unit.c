#include "decoder/internal/sample_math.h"
#include "shared/bit_math.h"

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

static void test_extreme_ub_boundaries(void) {
    static const int64_t extreme_values[] = {
        INT64_MIN,
        INT64_MIN + 1,
        (int64_t)INT32_MIN - 1,
        (int64_t)INT32_MIN,
        (int64_t)INT32_MIN + 1,
        -1,
        0,
        1,
        (int64_t)INT32_MAX - 1,
        (int64_t)INT32_MAX,
        (int64_t)INT32_MAX + 1,
        INT64_MAX - 1,
        INT64_MAX
    };
    static const int extreme_shifts[] = {
        INT32_MIN, -65, -1, 0, 1, 30, 31, 32, 62, 63, 64, 65, INT32_MAX
    };
    static const unsigned int floor_shifts[] = {
        0U, 1U, 30U, 31U, 32U, 62U, 63U, 64U, 65U, 128U
    };
    const unsigned char bits_input[4] = { 0xa5U, 0x5aU, 0xffU, 0x00U };
    size_t v_idx;
    size_t s_idx;

    CHECK(av2h_sample_abs(INT32_MIN) == INT32_MAX);
    CHECK(av2h_sample_abs_u32(INT32_MIN) == UINT32_C(0x80000000));
    CHECK(av2h_sample_abs_u32(INT32_MAX) == (uint32_t)INT32_MAX);
    CHECK(av2h_sample_abs_u32(0) == 0U);
    CHECK(av2h_sample_abs_u64(INT64_MIN) == (UINT64_C(1) << 63U));
    CHECK(av2h_sample_abs_u64(-1) == 1U);
    CHECK(av2h_sample_abs_u64(INT64_MAX) == (uint64_t)INT64_MAX);
    CHECK(av2h_sample_clamp_i64(INT64_MIN) == INT32_MIN);
    CHECK(av2h_sample_clamp_i64(INT64_MAX) == INT32_MAX);

    CHECK(av2h_sample_floor_shift(INT64_MIN, 64U) == -1);
    CHECK(av2h_sample_floor_shift(-1, 64U) == -1);
    CHECK(av2h_sample_floor_shift(0, 64U) == 0);
    CHECK(av2h_sample_floor_shift(INT64_MAX, 64U) == 0);
    CHECK(av2h_sample_round_pow2(INT64_MAX, 1) == INT32_MAX);
    CHECK(av2h_sample_round_pow2(INT64_MAX, 63) == 1);
    CHECK(av2h_sample_round_pow2(INT64_MIN, 63) == -1);
    CHECK(av2h_sample_round_pow2(INT64_MIN, 64) == 0);
    CHECK(av2h_sample_round_pow2(INT64_MAX, 64) == 0);
    CHECK(av2h_sample_round_pow2(-5, -3) == -5);
    CHECK(av2h_sample_round_pow2_signed(INT64_MIN, 64) == -1);
    CHECK(av2h_sample_round_pow2_signed(INT64_MAX, 64) == 0);
    CHECK(av2h_sample_round_pow2_signed(INT64_MIN, 65) == 0);
    CHECK(av2h_sample_round_pow2_signed(-5, -3) == -5);

    for (v_idx = 0U; v_idx < sizeof(extreme_values) / sizeof(extreme_values[0]); ++v_idx) {
        int64_t value = extreme_values[v_idx];
        for (s_idx = 0U; s_idx < sizeof(extreme_shifts) / sizeof(extreme_shifts[0]); ++s_idx) {
            int shift = extreme_shifts[s_idx];
            int32_t r1 = av2h_sample_round_pow2(value, shift);
            int32_t r2 = av2h_sample_round_pow2_signed(value, shift);
            CHECK(r1 >= INT32_MIN && r1 <= INT32_MAX);
            CHECK(r2 >= INT32_MIN && r2 <= INT32_MAX);
        }
        for (s_idx = 0U; s_idx < sizeof(floor_shifts) / sizeof(floor_shifts[0]); ++s_idx) {
            int32_t f = av2h_sample_floor_shift(value, floor_shifts[s_idx]);
            CHECK(f >= INT32_MIN && f <= INT32_MAX);
        }
    }

    CHECK(av2h_ceil_log2_ratio_u32(0U, 0U) == 0U);
    CHECK(av2h_ceil_log2_ratio_u32(0U, UINT32_MAX) == 0U);
    CHECK(av2h_read_bits_msb(bits_input, 8U, 4U) == 0U);
    CHECK(av2h_read_bits_msb(bits_input, 0U, 33U) == 0U);
    CHECK(av2h_read_bits_msb(bits_input, 0U, 32U) == UINT32_C(0xa55aff00));
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
    test_extreme_ub_boundaries();
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