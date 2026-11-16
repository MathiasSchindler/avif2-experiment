#include "internal/sample_math.h"

#include "shared/bit_math.h"

#define AV2H_DIV_LUT_BITS 7
#define AV2H_DIV_LUT_PREC_BITS 9

static const uint16_t reciprocal_lut[129U] = {
    512, 508, 504, 500, 496, 493, 489, 485, 482, 478, 475, 471, 468, 465, 462, 458, 455, 452, 449,
    446, 443, 440, 437, 434, 431, 428, 426, 423, 420, 417, 415, 412, 410, 407, 405, 402, 400, 397,
    395, 392, 390, 388, 386, 383, 381, 379, 377, 374, 372, 370, 368, 366, 364, 362, 360, 358, 356,
    354, 352, 350, 349, 347, 345, 343, 341, 340, 338, 336, 334, 333, 331, 329, 328, 326, 324, 323,
    321, 320, 318, 317, 315, 314, 312, 311, 309, 308, 306, 305, 303, 302, 301, 299, 298, 297, 295,
    294, 293, 291, 290, 289, 287, 286, 285, 284, 282, 281, 280, 279, 278, 277, 275, 274, 273, 272,
    271, 270, 269, 267, 266, 265, 264, 263, 262, 261, 260, 259, 258, 257, 256};

static unsigned int floor_log2_u64(uint64_t value) {
    unsigned int high = (unsigned int)(value >> 32U);
    if (high != 0U)
        return 32U + av2h_floor_log2_u32(high);
    return av2h_floor_log2_u32((uint32_t)value);
}

uint16_t av2h_sample_resolve_divisor_32(uint32_t d, int *shift) {
    int32_t f;
    int s;
    uint32_t e;
    if (d == 0U) {
        *shift = AV2H_DIV_LUT_PREC_BITS;
        return reciprocal_lut[0];
    }
    s = (int)av2h_floor_log2_u32(d);
    e = d - ((uint32_t)1U << (unsigned int)s);
    if (s > AV2H_DIV_LUT_BITS) {
        f = av2h_sample_round_pow2((int64_t)e, s - AV2H_DIV_LUT_BITS);
    } else {
        f = (int32_t)(e << (unsigned int)(AV2H_DIV_LUT_BITS - s));
    }
    *shift = s + AV2H_DIV_LUT_PREC_BITS;
    return reciprocal_lut[f];
}

static int32_t resolve_fixed_ratio(int64_t n, int64_t d, int shift) {
    int sign_n = n < 0;
    uint64_t abs_n = av2h_sample_abs_u64(n);
    uint64_t abs_d;
    int32_t f_n;
    int32_t f_d;
    int shift_n;
    int shift_d;
    int shift_add;
    int32_t limit = (int32_t)(((uint32_t)2U << (unsigned int)shift) - 1U);
    int32_t ret;
    if (abs_n == 0U || d <= 0)
        return 0;
    abs_d = (uint64_t)d;
    shift_n = (int)floor_log2_u64(abs_n);
    shift_d = (int)floor_log2_u64(abs_d);
    {
        uint64_t e_d = abs_d - ((uint64_t)1U << (unsigned int)shift_d);
        if (shift_d > AV2H_DIV_LUT_BITS) {
            f_d = av2h_sample_round_pow2((int64_t)e_d, shift_d - AV2H_DIV_LUT_BITS);
        } else {
            f_d = (int32_t)(e_d << (unsigned int)(AV2H_DIV_LUT_BITS - shift_d));
        }
    }
    if (shift_n > AV2H_DIV_LUT_BITS) {
        f_n = av2h_sample_round_pow2((int64_t)abs_n, shift_n - AV2H_DIV_LUT_BITS);
    } else {
        f_n = (int32_t)(abs_n << (unsigned int)(AV2H_DIV_LUT_BITS - shift_n));
    }
    shift_add = shift_d - shift_n - shift;
    if (shift_add <= 1) {
        int shift0 = AV2H_DIV_LUT_PREC_BITS + AV2H_DIV_LUT_BITS + shift_add;
        ret =
            shift0 >= 0
                ? (int32_t)(((uint32_t)reciprocal_lut[f_d] * (uint32_t)f_n) >> (unsigned int)shift0)
                : limit;
    } else {
        ret = 0;
    }
    if (ret >= limit)
        ret = limit;
    return sign_n ? -ret : ret;
}

int32_t av2h_sample_regression_slope(int32_t sum_x, int32_t sum_y, int32_t sum_xx, int32_t sum_xy,
                                     int count, int shift) {
    int64_t der;
    int64_t nor;
    if (count <= 0 || shift < 0 || shift >= 30)
        return 0;
    der = (int64_t)sum_xx - (int64_t)sum_x * sum_x / count;
    nor = (int64_t)sum_xy - (int64_t)sum_x * sum_y / count;
    if (der <= 0 || nor == 0)
        return 0;
    return resolve_fixed_ratio(nor, der, shift);
}
