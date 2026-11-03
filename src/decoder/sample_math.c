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

uint16_t av2h_sample_resolve_divisor_32(uint32_t d, int *shift) {
    int32_t f;
    int s = (int)av2h_floor_log2_u32(d);
    int32_t e = (int32_t)(d - ((uint32_t)1 << s));
    if (s > AV2H_DIV_LUT_BITS) {
        f = av2h_sample_round_pow2(e, s - AV2H_DIV_LUT_BITS);
    } else {
        f = e << (AV2H_DIV_LUT_BITS - s);
    }
    *shift = s + AV2H_DIV_LUT_PREC_BITS;
    return reciprocal_lut[f];
}

static int32_t resolve_fixed_ratio(int32_t n, int32_t d, int shift) {
    int sign_n = n < 0;
    int32_t f_n;
    int32_t f_d;
    int shift_n;
    int shift_d;
    int shift_add;
    int32_t ret;
    if (sign_n)
        n = -n;
    if (n == 0 || d == 0)
        return 0;
    shift_n = (int)av2h_floor_log2_u32((uint32_t)n);
    shift_d = (int)av2h_floor_log2_u32((uint32_t)d);
    {
        int32_t e_d = (int32_t)(d - ((uint32_t)1 << shift_d));
        if (shift_d > AV2H_DIV_LUT_BITS) {
            f_d = av2h_sample_round_pow2(e_d, shift_d - AV2H_DIV_LUT_BITS);
        } else {
            f_d = e_d << (AV2H_DIV_LUT_BITS - shift_d);
        }
    }
    if (shift_n > AV2H_DIV_LUT_BITS) {
        f_n = av2h_sample_round_pow2(n, shift_n - AV2H_DIV_LUT_BITS);
    } else {
        f_n = n << (AV2H_DIV_LUT_BITS - shift_n);
    }
    shift_add = shift_d - shift_n - shift;
    if (shift_add <= 1) {
        int shift0 = AV2H_DIV_LUT_PREC_BITS + AV2H_DIV_LUT_BITS + shift_add;
        ret = shift0 >= 0 ? (reciprocal_lut[f_d] * f_n) >> shift0 : (2 << shift) - 1;
    } else {
        ret = 0;
    }
    if (ret >= (2 << shift) - 1)
        ret = (2 << shift) - 1;
    return sign_n ? -ret : ret;
}

int32_t av2h_sample_regression_slope(int32_t sum_x, int32_t sum_y, int32_t sum_xx, int32_t sum_xy,
                                     int count, int shift) {
    int32_t der;
    int32_t nor;
    if (count == 0)
        return 0;
    der = sum_xx - (int32_t)((int64_t)sum_x * sum_x / count);
    nor = sum_xy - (int32_t)((int64_t)sum_x * sum_y / count);
    if (der == 0 || nor == 0)
        return 0;
    return resolve_fixed_ratio(nor, der, shift);
}
