#include "internal/reconstruct_modules.h"
#include "internal/decode_diagnostics.h"
#include "internal/sample_math.h"

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    return av2h_sample_clamp(value, minimum, maximum);
}
static int32_t round_shift(int64_t value, uint8_t shift) {
    int64_t adjusted = value + ((int64_t)1 << (shift - 1U));
    if (adjusted >= 0) return (int32_t)(adjusted >> shift);
    return (int32_t)-(((-adjusted) + (((int64_t)1 << shift) - 1)) >> shift);
}
static const int8_t dct4[4U][4U] = {
    { 64, 64, 64, 64 },
    { 83, 35, -35, -83 },
    { 64, -64, -64, 64 },
    { 35, -83, 83, -35 }
};

static const int8_t adst4[4U][4U] = {
    { 18, 50, 75, 89 },
    { 50, 89, 18, -75 },
    { 75, 18, -89, 50 },
    { 89, -75, 50, -18 }
};

static const int8_t adst8[8U][8U] = {
    { 11, 28, 44, 58, 70, 79, 86, 89 },
    { 34, 74, 89, 76, 39, -12, -58, -86 },
    { 54, 89, 48, -34, -87, -66, 12, 79 },
    { 71, 68, -41, -86, 1, 87, 38, -70 },
    { 84, 17, -89, 10, 86, -35, -75, 58 },
    { 88, -44, -44, 88, -44, -44, 88, -44 },
    { 79, -83, 50, 6, -59, 86, -74, 29 },
    { 50, -69, 81, -84, 78, -62, 40, -14 }
};

static const int8_t dct8[8U][8U] = {
    { 64, 64, 64, 64, 64, 64, 64, 64 },
    { 89, 75, 50, 18, -18, -50, -75, -89 },
    { 83, 35, -35, -83, -83, -35, 35, 83 },
    { 75, -18, -89, -50, 50, 89, 18, -75 },
    { 64, -64, -64, 64, 64, -64, -64, 64 },
    { 50, -89, 18, 75, -75, -18, 89, -50 },
    { 35, -83, 83, -35, -35, 83, -83, 35 },
    { 18, -50, 75, -89, 89, -75, 50, -18 }
};

static const uint8_t dct16_frequency[] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
    8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U
};
static const int8_t dct16[16][16] = {
        { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 },
        { 90, 87, 80, 70, 57, 43, 26, 9,
            -9, -26, -43, -57, -70, -80, -87, -90 },
        { 89, 75, 50, 18, -18, -50, -75, -89,
            -89, -75, -50, -18, 18, 50, 75, 89 },
        { 87, 57, 9, -43, -80, -90, -70, -26,
            26, 70, 90, 80, 43, -9, -57, -87 },
        { 83, 35, -35, -83, -83, -35, 35, 83,
            83, 35, -35, -83, -83, -35, 35, 83 },
        { 80, 9, -70, -87, -26, 57, 90, 43,
            -43, -90, -57, 26, 87, 70, -9, -80 },
        { 75, -18, -89, -50, 50, 89, 18, -75,
            -75, 18, 89, 50, -50, -89, -18, 75 },
        { 70, -43, -87, 9, 90, 26, -80, -57,
            57, 80, -26, -90, -9, 87, 43, -70 },
        { 64, -64, -64, 64, 64, -64, -64, 64,
            64, -64, -64, 64, 64, -64, -64, 64 },
        { 57, -80, -26, 90, -9, -87, 43, 70,
            -70, -43, 87, 9, -90, 26, 80, -57 },
        { 50, -89, 18, 75, -75, -18, 89, -50,
            -50, 89, -18, -75, 75, 18, -89, 50 },
        { 43, -90, 57, 26, -87, 70, 9, -80,
            80, -9, -70, 87, -26, -57, 90, -43 },
        { 35, -83, 83, -35, -35, 83, -83, 35,
            35, -83, 83, -35, -35, 83, -83, 35 },
        { 26, -70, 90, -80, 43, 9, -57, 87,
            -87, 57, -9, -43, 80, -90, 70, -26 },
        { 18, -50, 75, -89, 89, -75, 50, -18,
            -18, 50, -75, 89, -89, 75, -50, 18 },
        { 9, -26, 43, -57, 70, -80, 87, -90,
            90, -87, 80, -70, 57, -43, 26, -9 }
};

static const int8_t adst16[16][16] = {
        { 8, 17, 25, 33, 41, 48, 55, 62,
            67, 73, 77, 81, 84, 87, 88, 89 },
        { 25, 48, 67, 81, 88, 88, 81, 67,
            48, 25, 0, -25, -48, -67, -81, -88 },
        { 41, 73, 88, 84, 62, 25, -17, -55,
            -81, -89, -77, -48, -8, 33, 67, 87 },
        { 55, 87, 81, 41, -17, -67, -89, -73,
            -25, 33, 77, 88, 62, 8, -48, -84 },
        { 67, 88, 48, -25, -81, -81, -25, 48,
            88, 67, 0, -67, -88, -48, 25, 81 },
        { 77, 77, 0, -77, -77, 0, 77, 77,
            0, -77, -77, 0, 77, 77, 0, -77 },
        { 84, 55, -48, -87, -8, 81, 62, -41,
            -88, -17, 77, 67, -33, -89, -25, 73 },
        { 88, 25, -81, -48, 67, 67, -48, -81,
            25, 88, 0, -88, -25, 81, 48, -67 },
        { 89, -8, -88, 17, 87, -25, -84, 33,
            81, -41, -77, 48, 73, -55, -67, 62 },
        { 87, -41, -67, 73, 33, -88, 8, 84,
            -48, -62, 77, 25, -89, 17, 81, -55 },
        { 81, -67, -25, 88, -48, -48, 88, -25,
            -67, 81, 0, -81, 67, 25, -88, 48 },
        { 73, -84, 25, 55, -89, 48, 33, -87,
            67, 8, -77, 81, -17, -62, 88, -41 },
        { 62, -89, 67, -8, -55, 88, -73, 17,
            48, -87, 77, -25, -41, 84, -81, 33 },
        { 48, -81, 88, -67, 25, 25, -67, 88,
            -81, 48, 0, -48, 81, -88, 67, -25 },
        { 33, -62, 81, -89, 84, -67, 41, -8,
            -25, 55, -77, 88, -87, 73, -48, 17 },
        { 17, -33, 48, -62, 73, -81, 87, -89,
            88, -84, 77, -67, 55, -41, 25, -8 }
};

static const uint8_t dct32_frequency[] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U,
    12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U,
    24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U
};
static const int8_t dct32[32][32] = {
    {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    },
    {
        90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 47, 39, 30, 22, 13, 4,
        -4, -13, -22, -30, -39, -47, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90
    },
    {
        90, 87, 80, 70, 57, 43, 26, 9, -9, -26, -43, -57, -70, -80, -87, -90,
        -90, -87, -80, -70, -57, -43, -26, -9, 9, 26, 43, 57, 70, 80, 87, 90
    },
    {
        90, 82, 67, 47, 22, -4, -30, -54, -73, -85, -90, -88, -78, -61, -39, -13,
        13, 39, 61, 78, 88, 90, 85, 73, 54, 30, 4, -22, -47, -67, -82, -90
    },
    {
        89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89,
        89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89
    },
    {
        88, 67, 30, -13, -54, -82, -90, -78, -47, -4, 39, 73, 90, 85, 61, 22,
        -22, -61, -85, -90, -73, -39, 4, 47, 78, 90, 82, 54, 13, -30, -67, -88
    },
    {
        87, 57, 9, -43, -80, -90, -70, -26, 26, 70, 90, 80, 43, -9, -57, -87,
        -87, -57, -9, 43, 80, 90, 70, 26, -26, -70, -90, -80, -43, 9, 57, 87
    },
    {
        85, 47, -13, -67, -90, -73, -22, 39, 82, 88, 54, -4, -61, -90, -78, -30,
        30, 78, 90, 61, 4, -54, -88, -82, -39, 22, 73, 90, 67, 13, -47, -85
    },
    {
        83, 35, -35, -83, -83, -35, 35, 83, 83, 35, -35, -83, -83, -35, 35, 83,
        83, 35, -35, -83, -83, -35, 35, 83, 83, 35, -35, -83, -83, -35, 35, 83
    },
    {
        82, 22, -54, -90, -61, 13, 78, 85, 30, -47, -90, -67, 4, 73, 88, 39,
        -39, -88, -73, -4, 67, 90, 47, -30, -85, -78, -13, 61, 90, 54, -22, -82
    },
    {
        80, 9, -70, -87, -26, 57, 90, 43, -43, -90, -57, 26, 87, 70, -9, -80,
        -80, -9, 70, 87, 26, -57, -90, -43, 43, 90, 57, -26, -87, -70, 9, 80
    },
    {
        78, -4, -82, -73, 13, 85, 67, -22, -88, -61, 30, 90, 54, -39, -90, -47,
        47, 90, 39, -54, -90, -30, 61, 88, 22, -67, -85, -13, 73, 82, 4, -78
    },
    {
        75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75,
        75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75
    },
    {
        73, -30, -90, -22, 78, 67, -39, -90, -13, 82, 61, -47, -88, -4, 85, 54,
        -54, -85, 4, 88, 47, -61, -82, 13, 90, 39, -67, -78, 22, 90, 30, -73
    },
    {
        70, -43, -87, 9, 90, 26, -80, -57, 57, 80, -26, -90, -9, 87, 43, -70,
        -70, 43, 87, -9, -90, -26, 80, 57, -57, -80, 26, 90, 9, -87, -43, 70
    },
    {
        67, -54, -78, 39, 85, -22, -90, 4, 90, 13, -88, -30, 82, 47, -73, -61,
        61, 73, -47, -82, 30, 88, -13, -90, -4, 90, 22, -85, -39, 78, 54, -67
    },
    {
        64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64,
        64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64
    },
    {
        61, -73, -47, 82, 30, -88, -13, 90, -4, -90, 22, 85, -39, -78, 54, 67,
        -67, -54, 78, 39, -85, -22, 90, 4, -90, 13, 88, -30, -82, 47, 73, -61
    },
    {
        57, -80, -26, 90, -9, -87, 43, 70, -70, -43, 87, 9, -90, 26, 80, -57,
        -57, 80, 26, -90, 9, 87, -43, -70, 70, 43, -87, -9, 90, -26, -80, 57
    },
    {
        54, -85, -4, 88, -47, -61, 82, 13, -90, 39, 67, -78, -22, 90, -30, -73,
        73, 30, -90, 22, 78, -67, -39, 90, -13, -82, 61, 47, -88, 4, 85, -54
    },
    {
        50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50,
        50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50
    },
    {
        47, -90, 39, 54, -90, 30, 61, -88, 22, 67, -85, 13, 73, -82, 4, 78,
        -78, -4, 82, -73, -13, 85, -67, -22, 88, -61, -30, 90, -54, -39, 90, -47
    },
    {
        43, -90, 57, 26, -87, 70, 9, -80, 80, -9, -70, 87, -26, -57, 90, -43,
        -43, 90, -57, -26, 87, -70, -9, 80, -80, 9, 70, -87, 26, 57, -90, 43
    },
    {
        39, -88, 73, -4, -67, 90, -47, -30, 85, -78, 13, 61, -90, 54, 22, -82,
        82, -22, -54, 90, -61, -13, 78, -85, 30, 47, -90, 67, 4, -73, 88, -39
    },
    {
        35, -83, 83, -35, -35, 83, -83, 35, 35, -83, 83, -35, -35, 83, -83, 35,
        35, -83, 83, -35, -35, 83, -83, 35, 35, -83, 83, -35, -35, 83, -83, 35
    },
    {
        30, -78, 90, -61, 4, 54, -88, 82, -39, -22, 73, -90, 67, -13, -47, 85,
        -85, 47, 13, -67, 90, -73, 22, 39, -82, 88, -54, -4, 61, -90, 78, -30
    },
    {
        26, -70, 90, -80, 43, 9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -26,
        -26, 70, -90, 80, -43, -9, 57, -87, 87, -57, 9, 43, -80, 90, -70, 26
    },
    {
        22, -61, 85, -90, 73, -39, -4, 47, -78, 90, -82, 54, -13, -30, 67, -88,
        88, -67, 30, 13, -54, 82, -90, 78, -47, 4, 39, -73, 90, -85, 61, -22
    },
    {
        18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18,
        18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18
    },
    {
        13, -39, 61, -78, 88, -90, 85, -73, 54, -30, 4, 22, -47, 67, -82, 90,
        -90, 82, -67, 47, -22, -4, 30, -54, 73, -85, 90, -88, 78, -61, 39, -13
    },
    {
        9, -26, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 26, -9,
        -9, 26, -43, 57, -70, 80, -87, 90, -90, 87, -80, 70, -57, 43, -26, 9
    },
    {
        4, -13, 22, -30, 39, -47, 54, -61, 67, -73, 78, -82, 85, -88, 90, -90,
        90, -90, 88, -85, 82, -78, 73, -67, 61, -54, 47, -39, 30, -22, 13, -4
    }
};

static int32_t identity_scale(uint16_t size) {
    return size == 4U ? 128 : size == 8U ? 181 :
        size == 16U ? 256 : 362;
}

static Av2hStatus matrix_value(uint16_t size, uint16_t frequency,
                               uint16_t output, int32_t *value) {
    size_t index;
    if (size == 4U) {
        *value = dct4[frequency][output];
        return AV2H_OK;
    }
    if (size == 8U) {
        *value = dct8[frequency][output];
        return AV2H_OK;
    }
    if (size == 16U) {
        for (index = 0U;
             index < sizeof(dct16_frequency) / sizeof(dct16_frequency[0]);
             ++index) {
            if (dct16_frequency[index] == frequency) {
                *value = dct16[index][output];
                return AV2H_OK;
            }
        }
        return AV2H_UNSUPPORTED;
    }
    if (size == 32U) {
        for (index = 0U;
             index < sizeof(dct32_frequency) / sizeof(dct32_frequency[0]);
             ++index) {
            if (dct32_frequency[index] == frequency) {
                *value = dct32[index][output];
                return AV2H_OK;
            }
        }
        return AV2H_UNSUPPORTED;
    }
    return AV2H_UNSUPPORTED;
}

static Av2hStatus flip_adst_matrix_value(uint16_t size, uint16_t frequency,
                                         uint16_t output, int32_t *value) {
    if (size == 4U) {
        *value = adst4[frequency][size - output - 1U];
        if ((frequency & 1U) != 0U) *value = -*value;
        return AV2H_OK;
    }
    if (size == 8U) {
        *value = adst8[frequency][size - output - 1U];
        return AV2H_OK;
    }
    if (size == 16U) {
        *value = adst16[frequency][size - output - 1U];
        if ((frequency & 1U) != 0U) *value = -*value;
        return AV2H_OK;
    }
    return AV2H_UNSUPPORTED;
}

Av2hStatus av2h_inverse_rows(const int32_t *input, int32_t *transposed,
                               uint16_t width, uint16_t height,
                               uint8_t shift, uint8_t identity,
                               uint8_t adst, uint8_t flip_adst) {
    uint16_t row;
    uint16_t output;
    uint16_t frequency;
    for (row = 0U; row < height; ++row) {
        for (output = 0U; output < width; ++output) {
            int64_t sum = 0;
            if (identity) {
                sum = (int64_t)input[(size_t)row * width + output] *
                      identity_scale(width);
            } else if (adst) {
                if (width == 4U) {
                    for (frequency = 0U; frequency < width; ++frequency) {
                        sum +=
                            (int64_t)input[(size_t)row * width + frequency] *
                            adst4[frequency][output];
                    }
                } else if (width == 8U) {
                    for (frequency = 0U; frequency < width; ++frequency) {
                        sum += (int64_t)input[
                            (size_t)row * width + frequency] *
                            adst8[frequency][output];
                    }
                } else if (width == 16U) {
                    for (frequency = 0U; frequency < width; ++frequency) {
                        sum += (int64_t)input[
                            (size_t)row * width + frequency] *
                            adst16[frequency][output];
                    }
                } else {
                    return AV2H_UNSUPPORTED;
                }
            } else {
                for (frequency = 0U; frequency < width; ++frequency) {
                    int32_t source = input[(size_t)row * width + frequency];
                    int32_t matrix;
                    Av2hStatus status;
                    if (source == 0) continue;
                    status = flip_adst ?
                        flip_adst_matrix_value(width, frequency, output,
                                               &matrix) :
                        matrix_value(width, frequency, output, &matrix);
                    if (status != AV2H_OK) return status;
                    sum += (int64_t)source * matrix;
                }
            }
            transposed[(size_t)output * height + row] =
                clamp(round_shift(sum, shift), -32768, 32767);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_inverse_columns(const int32_t *transposed,
                                  int32_t *output_block,
                                  uint16_t width, uint16_t height,
                                  uint8_t shift, uint8_t identity,
                                  uint8_t adst, uint8_t flip_adst) {
    uint16_t column;
    uint16_t output;
    uint16_t frequency;
    for (column = 0U; column < width; ++column) {
        for (output = 0U; output < height; ++output) {
            int64_t sum = 0;
            if (identity) {
                sum = (int64_t)transposed[(size_t)column * height + output] *
                      identity_scale(height);
            }
            for (frequency = 0U; !identity && frequency < height; ++frequency) {
                int32_t source = transposed[(size_t)column * height + frequency];
                int32_t matrix;
                Av2hStatus status;
                if (source == 0) continue;
                if (adst) {
                    if (height == 4U) {
                        matrix = adst4[frequency][output];
                    } else if (height == 8U) {
                        matrix = adst8[frequency][output];
                    } else if (height == 16U) {
                        matrix = adst16[frequency][output];
                    } else {
                        return AV2H_UNSUPPORTED;
                    }
                    status = AV2H_OK;
                } else {
                    status = flip_adst ?
                        flip_adst_matrix_value(height, frequency, output,
                                               &matrix) :
                        matrix_value(height, frequency, output, &matrix);
                }
                if (status != AV2H_OK) return status;
                sum += (int64_t)source * matrix;
            }
            output_block[(size_t)output * width + column] =
                clamp(round_shift(sum, shift), -256, 255);
        }
    }
    return AV2H_OK;
}

/*
 * Dimension-parametrized reconstruction kernel shared by the Emerald Q208
 * luma blocks.  It implements the AV2 2D inverse transform (spec 7.15.4)
 * directly on adjusted coefficient dimensions (each clamped to 32), then
 * expands the residual to the full output geometry by sample duplication
 * for the 64-wide/64-tall transforms.  Row/column transform kinds are
 * derived from the transform type and shifts from the transform size, so
 * this branch never depends on transform-size enum names that are still
 * being introduced by the parallel geometry work.
 */

/*
 * Transform size integer values fixed by the AV2 bitstream format.  Some
 * are not yet named in coeff.h; use the raw values so the kernel stays
 * independent of the parallel geometry branch.
 */
#define RECON_TX_4X4 0
#define RECON_TX_8X8 1
#define RECON_TX_16X16 2
#define RECON_TX_32X32 3
#define RECON_TX_64X64 4
#define RECON_TX_4X8 5
#define RECON_TX_8X4 6
#define RECON_TX_8X16 7
#define RECON_TX_16X8 8
#define RECON_TX_16X32 9
#define RECON_TX_32X64 11
#define RECON_TX_32X16 10
#define RECON_TX_64X32 12
#define RECON_TX_4X16 13
#define RECON_TX_16X4 14
#define RECON_TX_16X64 17
#define RECON_TX_32X8 16
#define RECON_TX_8X32 15
#define RECON_TX_64X16 18
#define RECON_TX_4X32 19
#define RECON_TX_32X4 20
#define RECON_TX_8X64 21
#define RECON_TX_64X8 22
#define RECON_TX_4X64 23
#define RECON_TX_64X4 24

typedef enum {
    RECON_1D_DCT,
    RECON_1D_ADST,
    RECON_1D_FLIPADST,
    RECON_1D_IDENTITY,
    RECON_1D_NONE
} Av2hRecon1dKind;

static uint8_t recon_size_log2(uint16_t size) {
    return size == 4U ? 2U : size == 8U ? 3U : size == 16U ? 4U :
        size == 32U ? 5U : 6U;
}

static Av2hStatus recon_geometry(uint8_t tx_size,
                                 uint16_t *output_width,
                                 uint16_t *output_height,
                                 uint8_t *row_shift,
                                 uint8_t *column_shift) {
    switch (tx_size) {
        case RECON_TX_4X4:
            *output_width = 4U; *output_height = 4U;
            *row_shift = 7U; *column_shift = 10U; return AV2H_OK;
        case RECON_TX_4X8:
            *output_width = 4U; *output_height = 8U;
            *row_shift = 7U; *column_shift = 10U; return AV2H_OK;
        case RECON_TX_8X8:
            *output_width = 8U; *output_height = 8U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_8X4:
            *output_width = 8U; *output_height = 4U;
            *row_shift = 7U; *column_shift = 10U; return AV2H_OK;
        case RECON_TX_16X16:
            *output_width = 16U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_32X32:
            *output_width = 32U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_64X64:
            *output_width = 64U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_8X16:
            *output_width = 8U; *output_height = 16U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_16X8:
            *output_width = 16U; *output_height = 8U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_16X4:
            *output_width = 16U; *output_height = 4U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_4X16:
            *output_width = 4U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_32X8:
            *output_width = 32U; *output_height = 8U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_32X4:
            *output_width = 32U; *output_height = 4U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_64X16:
            *output_width = 64U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_4X32:
            *output_width = 4U; *output_height = 32U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_32X16:
            *output_width = 32U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_16X32:
            *output_width = 16U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_64X32:
            *output_width = 64U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_16X64:
            *output_width = 16U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_8X64:
            *output_width = 8U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_4X64:
            *output_width = 4U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_64X4:
            *output_width = 64U; *output_height = 4U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_64X8:
            *output_width = 64U; *output_height = 8U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_8X32:
            *output_width = 8U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_32X64:
            *output_width = 32U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        default:
            return AV2H_UNSUPPORTED;
    }
}

static Av2hStatus recon_transform_kinds(uint8_t primary_type,
                                        Av2hRecon1dKind *row_kind,
                                        Av2hRecon1dKind *column_kind) {
    switch (primary_type) {
        case AV2H_TX_DCT_DCT:
            *row_kind = RECON_1D_DCT; *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_ADST_DCT:
            *row_kind = RECON_1D_DCT; *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_DCT_ADST:
            *row_kind = RECON_1D_ADST; *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_DCT_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_FLIPADST_DCT:
            *row_kind = RECON_1D_DCT;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_ADST_ADST:
            *row_kind = RECON_1D_ADST; *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_FLIPADST_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_FLIPADST_ADST:
            *row_kind = RECON_1D_ADST;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_ADST_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_H_ADST:
            *row_kind = RECON_1D_ADST;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        case AV2H_TX_V_ADST:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_V_DCT:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_H_DCT:
            *row_kind = RECON_1D_DCT;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        case AV2H_TX_V_FLIPADST:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_H_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        case AV2H_TX_IDTX:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        default:
            *row_kind = RECON_1D_NONE; *column_kind = RECON_1D_NONE;
            return AV2H_UNSUPPORTED;
    }
}

static Av2hStatus recon_kind_supported(Av2hRecon1dKind kind, uint16_t size) {
    if (kind == RECON_1D_DCT) {
        return (size == 4U || size == 8U || size == 16U || size == 32U) ?
            AV2H_OK : AV2H_UNSUPPORTED;
    }
    if (kind == RECON_1D_ADST || kind == RECON_1D_FLIPADST) {
        return (size == 4U || size == 8U || size == 16U) ?
            AV2H_OK : AV2H_UNSUPPORTED;
    }
    if (kind == RECON_1D_IDENTITY) {
        return (size == 4U || size == 8U || size == 16U || size == 32U) ?
            AV2H_OK : AV2H_UNSUPPORTED;
    }
    return AV2H_UNSUPPORTED;
}

static int32_t recon_kernel_value(Av2hRecon1dKind kind, uint16_t size,
                                  uint16_t frequency, uint16_t output) {
    if (kind == RECON_1D_DCT) {
        if (size == 4U) return dct4[frequency][output];
        if (size == 8U) return dct8[frequency][output];
        if (size == 16U) return dct16[frequency][output];
        return dct32[frequency][output];
    }
    if (kind == RECON_1D_FLIPADST) {
        int32_t value = size == 4U ?
            adst4[frequency][size - output - 1U] : size == 8U ?
            adst8[frequency][size - output - 1U] :
            adst16[frequency][size - output - 1U];
        return size != 8U && (frequency & 1U) != 0U ? -value : value;
    }
    if (size == 4U) return adst4[frequency][output];
    if (size == 8U) return adst8[frequency][output];
    return adst16[frequency][output];
}

static void recon_transform_1d(const int32_t *input, int32_t *output,
                               Av2hRecon1dKind kind, uint16_t size,
                               uint8_t shift, uint8_t column_transform,
                               uint8_t bit_depth) {
    int32_t magnitude_bits =
        (int32_t)bit_depth + (column_transform ? 0 : 7);
    int32_t minimum = -((int32_t)1 << magnitude_bits);
    int32_t maximum = ((int32_t)1 << magnitude_bits) - 1;
    uint16_t out;
    uint16_t frequency;
    if (kind == RECON_1D_IDENTITY) {
        for (out = 0U; out < size; ++out) {
            output[out] = clamp(round_shift(
                (int64_t)input[out] * identity_scale(size), shift),
                minimum, maximum);
        }
        return;
    }
    for (out = 0U; out < size; ++out) {
        int64_t sum = 0;
        for (frequency = 0U; frequency < size; ++frequency) {
            sum += (int64_t)recon_kernel_value(kind, size, frequency, out) *
                   input[frequency];
        }
        output[out] = clamp(round_shift(sum, shift), minimum, maximum);
    }
}

static int32_t lossless_floor_shift(int32_t value, uint8_t shift) {
    if (value >= 0) return value >> shift;
    return -(int32_t)(((uint32_t)(-value) +
        (((uint32_t)1U << shift) - 1U)) >> shift);
}

static int32_t lossless_half(int32_t value) {
    return lossless_floor_shift(value, 1U);
}

static void lossless_iwht4(int32_t *values) {
    int32_t first = values[0] + values[1];
    int32_t fourth = values[2] - values[3];
    int32_t half = lossless_half(first - fourth);
    int32_t second = half - values[3];
    int32_t third = half - values[1];
    values[0] = first - second;
    values[1] = second;
    values[2] = third;
    values[3] = fourth + third;
}

Av2hStatus av2h_reconstruct_lossless_4x4(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hFirstReconstruction *reconstruction) {
    int32_t residual[16U];
    size_t row;
    size_t column;
    if (coefficients->width != 4U || coefficients->height != 4U ||
        coefficients->coefficient_count != 16U ||
        (transform->primary_type != AV2H_TX_DCT_DCT &&
         transform->primary_type != AV2H_TX_IDTX) ||
        (predictor != 0 && predictor_stride_samples < 4U)) {
        return AV2H_UNSUPPORTED;
    }
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            residual[row * 4U + column] =
                lossless_floor_shift(
                    coefficients->dequantized[row * 4U + column], 3U);
        }
        if (transform->primary_type == AV2H_TX_DCT_DCT &&
            !(mode->use_dpcm_y != 0U && mode->fsc_mode != 0U)) {
            lossless_iwht4(&residual[row * 4U]);
        }
    }
    if (transform->primary_type == AV2H_TX_DCT_DCT &&
        !(mode->use_dpcm_y != 0U && mode->fsc_mode != 0U)) {
        for (column = 0U; column < 4U; ++column) {
            int32_t values[4U];
            for (row = 0U; row < 4U; ++row) {
                values[row] = residual[row * 4U + column];
            }
            lossless_iwht4(values);
            for (row = 0U; row < 4U; ++row) {
                residual[row * 4U + column] = values[row];
            }
        }
    }
    if (mode->use_dpcm_y != 0U) {
        if (mode->dpcm_mode_y == 0U) {
            for (column = 0U; column < 4U; ++column) {
                for (row = 1U; row < 4U; ++row) {
                    residual[row * 4U + column] +=
                        residual[(row - 1U) * 4U + column];
                }
            }
        } else {
            for (row = 0U; row < 4U; ++row) {
                for (column = 1U; column < 4U; ++column) {
                    residual[row * 4U + column] +=
                        residual[row * 4U + column - 1U];
                }
            }
        }
    }
    reconstruction->width = 4U;
    reconstruction->height = 4U;
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            size_t index = row * 4U + column;
            int32_t predicted = predictor == 0 ?
                av2h_sample_midpoint_8bit() :
                predictor[row * predictor_stride_samples + column];
            reconstruction->residual[index] = (int16_t)residual[index];
            reconstruction->pixels[index] =
                av2h_sample_clip(predicted + residual[index]);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_reconstruct_transform_generic(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    uint16_t output_width;
    uint16_t output_height;
    uint8_t row_shift;
    uint8_t column_shift;
    uint16_t coeff_width;
    uint16_t coeff_height;
    uint8_t rectangular;
    Av2hRecon1dKind row_kind;
    Av2hRecon1dKind column_kind;
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    size_t index;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(1U);
    if (recon_geometry(prefix->tx_size, &output_width, &output_height,
                       &row_shift, &column_shift) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    coeff_width = output_width > 32U ? 32U : output_width;
    coeff_height = output_height > 32U ? 32U : output_height;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(2U);
    if (coefficients->width != coeff_width ||
        coefficients->height != coeff_height ||
        coefficients->coefficient_count !=
            (uint32_t)coeff_width * coeff_height) {
        return AV2H_UNSUPPORTED;
    }
    if (predictor != 0 && predictor_stride_samples < output_width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 &&
        (transform->primary_type == AV2H_TX_DCT_DCT ||
         transform->primary_type == AV2H_TX_IDTX)) {
        return av2h_reconstruct_lossless_4x4(
            mode, transform, coefficients, predictor,
            predictor_stride_samples, reconstruction);
    }
    if (prefix->all_zero && prefix->eob == 0U &&
        coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        reconstruction->width = output_width;
        reconstruction->height = output_height;
        for (row = 0U; row < output_height; ++row) {
            for (column = 0U; column < output_width; ++column) {
                size_t out_index = (size_t)row * output_width + column;
                int32_t sample = predictor == 0 ?
                    av2h_sample_midpoint_8bit() :
                    predictor[(size_t)row * predictor_stride_samples + column];
                reconstruction->residual[out_index] = 0;
                reconstruction->pixels[out_index] = (uint8_t)sample;
            }
        }
        return AV2H_OK;
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(3U);
    if (recon_transform_kinds(transform->primary_type, &row_kind,
                              &column_kind) != AV2H_OK ||
        recon_kind_supported(row_kind, coeff_width) != AV2H_OK ||
        recon_kind_supported(column_kind, coeff_height) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    if (transform->primary_type == AV2H_TX_IDTX &&
        prefix->tx_size == RECON_TX_32X4) {
        row_shift = 7U;
        column_shift = 11U;
    }
    reconstruction->width = output_width;
    reconstruction->height = output_height;
    rectangular = (uint8_t)((recon_size_log2(output_width) ^
                             recon_size_log2(output_height)) & 1U);
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        scratch->scaled[index] = coefficients->dequantized[index];
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(4U);
    status = av2h_inverse_secondary_transform(
        mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK) return status;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(5U);
    for (row = 0U; row < coeff_height; ++row) {
        int32_t row_input[32U];
        int32_t row_output[32U];
        for (column = 0U; column < coeff_width; ++column) {
            int32_t value =
                scratch->scaled[(size_t)row * coeff_width + column];
            row_input[column] = rectangular ?
                round_shift((int64_t)value * 2896, 12U) : value;
        }
        recon_transform_1d(row_input, row_output, row_kind, coeff_width,
                           row_shift, 0U, 8U);
        for (column = 0U; column < coeff_width; ++column) {
            scratch->transposed[(size_t)row * coeff_width + column] =
                row_output[column];
        }
    }
    for (column = 0U; column < coeff_width; ++column) {
        int32_t column_input[32U];
        int32_t column_output[32U];
        for (row = 0U; row < coeff_height; ++row) {
            column_input[row] =
                scratch->transposed[(size_t)row * coeff_width + column];
        }
        recon_transform_1d(column_input, column_output, column_kind,
                           coeff_height, column_shift, 1U, 8U);
        for (row = 0U; row < coeff_height; ++row) {
            scratch->residual[(size_t)row * coeff_width + column] =
                column_output[row];
        }
    }
    for (row = 0U; row < output_height; ++row) {
        uint16_t source_row =
            coeff_height != output_height ? (uint16_t)(row >> 1U) : row;
        for (column = 0U; column < output_width; ++column) {
            uint16_t source_column =
                coeff_width != output_width ? (uint16_t)(column >> 1U) : column;
            int32_t residual = scratch->residual[
                (size_t)source_row * coeff_width + source_column];
            size_t out_index = (size_t)row * output_width + column;
            int32_t sample = predictor == 0 ?
                av2h_sample_midpoint_8bit() :
                predictor[(size_t)row * predictor_stride_samples + column];
            reconstruction->residual[out_index] = (int16_t)residual;
            reconstruction->pixels[out_index] =
                av2h_sample_clip(sample + residual);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_reconstruct_transform_generic_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction) {
    uint16_t output_width;
    uint16_t output_height;
    uint8_t row_shift;
    uint8_t column_shift;
    uint16_t coeff_width;
    uint16_t coeff_height;
    uint8_t rectangular;
    Av2hRecon1dKind row_kind;
    Av2hRecon1dKind column_kind;
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    size_t index;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(1U);
    if (mode == 0 || prefix == 0 || transform == 0 || coefficients == 0 ||
        predictor == 0 || scratch == 0 || reconstruction == 0 ||
        recon_geometry(prefix->tx_size, &output_width, &output_height,
                       &row_shift, &column_shift) != AV2H_OK) {
        return AV2H_INVALID_ARGUMENT;
    }
    coeff_width = output_width > 32U ? 32U : output_width;
    coeff_height = output_height > 32U ? 32U : output_height;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(10U);
    if (coefficients->width != coeff_width ||
        coefficients->height != coeff_height ||
        coefficients->coefficient_count !=
            (uint32_t)coeff_width * coeff_height) {
        return AV2H_UNSUPPORTED;
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(11U);
    if (predictor_stride_samples < output_width ||
        (size_t)output_width * output_height > AV2H_RECON_SAMPLE_CAPACITY) {
        return AV2H_UNSUPPORTED;
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(12U);
    if (mode->skip_transform != 0U &&
        !(prefix->all_zero && prefix->eob == 0U &&
          coefficients->nonzero_count == 0U)) {
        return AV2H_UNSUPPORTED;
    }
        av2h_reconstruction_diagnostics_set_inverse_transform_stage(13U);
    reconstruction->width = output_width;
    reconstruction->height = output_height;
    if (prefix->all_zero && prefix->eob == 0U &&
        coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_INVALID_DATA;
            }
        }
        for (row = 0U; row < output_height; ++row) {
            for (column = 0U; column < output_width; ++column) {
                index = (size_t)row * output_width + column;
                reconstruction->residual[index] = 0;
                reconstruction->pixels[index] = predictor[
                    (size_t)row * predictor_stride_samples + column];
            }
        }
        return AV2H_OK;
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(2U);
    if (recon_transform_kinds(transform->primary_type, &row_kind,
                              &column_kind) != AV2H_OK ||
        recon_kind_supported(row_kind, coeff_width) != AV2H_OK ||
        recon_kind_supported(column_kind, coeff_height) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    rectangular = (uint8_t)((recon_size_log2(output_width) ^
                             recon_size_log2(output_height)) & 1U);
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        scratch->scaled[index] = coefficients->dequantized[index];
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(3U);
    status = av2h_inverse_secondary_transform(
        mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK) return status;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(4U);
    for (row = 0U; row < coeff_height; ++row) {
        int32_t row_input[32U];
        int32_t row_output[32U];
        for (column = 0U; column < coeff_width; ++column) {
            int32_t value =
                scratch->scaled[(size_t)row * coeff_width + column];
            row_input[column] = rectangular ?
                round_shift((int64_t)value * 2896, 12U) : value;
        }
        recon_transform_1d(row_input, row_output, row_kind, coeff_width,
                           row_shift, 0U, 10U);
        for (column = 0U; column < coeff_width; ++column) {
            scratch->transposed[(size_t)row * coeff_width + column] =
                row_output[column];
        }
    }
    for (column = 0U; column < coeff_width; ++column) {
        int32_t column_input[32U];
        int32_t column_output[32U];
        for (row = 0U; row < coeff_height; ++row) {
            column_input[row] =
                scratch->transposed[(size_t)row * coeff_width + column];
        }
        recon_transform_1d(column_input, column_output, column_kind,
                           coeff_height, column_shift, 1U, 10U);
        for (row = 0U; row < coeff_height; ++row) {
            scratch->residual[(size_t)row * coeff_width + column] =
                column_output[row];
        }
    }
    for (row = 0U; row < output_height; ++row) {
        uint16_t source_row =
            coeff_height != output_height ? (uint16_t)(row >> 1U) : row;
        for (column = 0U; column < output_width; ++column) {
            uint16_t source_column = coeff_width != output_width ?
                (uint16_t)(column >> 1U) : column;
            int32_t residual = scratch->residual[
                (size_t)source_row * coeff_width + source_column];
            index = (size_t)row * output_width + column;
            reconstruction->residual[index] = residual;
            reconstruction->pixels[index] = av2h_sample_clip_10bit(
                (int32_t)predictor[
                    (size_t)row * predictor_stride_samples + column] +
                residual);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_reconstruct_transform_8x16_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction) {
    uint16_t row;
    uint16_t column;
    size_t index;
    Av2hStatus status;
    if (mode == 0 || prefix == 0 || transform == 0 || coefficients == 0 ||
        predictor == 0 || scratch == 0 || reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (mode->mode != AV2H_INTRA_DC || mode->angle_delta != 0 ||
        mode->mrl_index != 0U || mode->enable_ibp != 0U ||
        mode->use_intrabc != 0U || mode->fsc_mode != 0U ||
        mode->palette_size != 0U || prefix->tx_size != RECON_TX_8X16 ||
        transform->primary_type != AV2H_TX_DCT_DCT ||
        transform->secondary_signaled != 0U || coefficients->width != 8U ||
        coefficients->height != 16U ||
        coefficients->coefficient_count != 128U ||
        predictor_stride_samples < 8U) {
        return AV2H_UNSUPPORTED;
    }
    reconstruction->width = 8U;
    reconstruction->height = 16U;
    if (prefix->all_zero) {
        if (prefix->eob != 0U || coefficients->nonzero_count != 0U) {
            return AV2H_INVALID_DATA;
        }
        for (index = 0U; index < 128U; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_INVALID_DATA;
            }
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[
                (index / 8U) * predictor_stride_samples + index % 8U];
        }
        return AV2H_OK;
    }
    for (index = 0U; index < 128U; ++index) {
        scratch->scaled[index] = clamp(round_shift(
            (int64_t)coefficients->dequantized[index] * 2896, 12U),
            -131072, 131071);
    }
    status = av2h_inverse_secondary_transform(
        mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < 16U; ++row) {
        int32_t row_output[8U];
        recon_transform_1d(
            scratch->scaled + (size_t)row * 8U, row_output,
            RECON_1D_DCT, 8U, 7U, 0U, 10U);
        for (column = 0U; column < 8U; ++column) {
            scratch->transposed[(size_t)row * 8U + column] =
                row_output[column];
        }
    }
    for (column = 0U; column < 8U; ++column) {
        int32_t column_input[16U];
        int32_t column_output[16U];
        for (row = 0U; row < 16U; ++row) {
            column_input[row] =
                scratch->transposed[(size_t)row * 8U + column];
        }
        recon_transform_1d(
            column_input, column_output, RECON_1D_DCT,
            16U, 11U, 1U, 10U);
        for (row = 0U; row < 16U; ++row) {
            scratch->residual[(size_t)row * 8U + column] =
                column_output[row];
        }
    }
    for (index = 0U; index < 128U; ++index) {
        int32_t residual = scratch->residual[index];
        int32_t predicted = predictor[
            (index / 8U) * predictor_stride_samples + index % 8U];
        reconstruction->residual[index] = residual;
        reconstruction->pixels[index] =
            av2h_sample_clip_10bit(predicted + residual);
    }
    return AV2H_OK;
}

Av2hStatus av2h_reconstruct_transform_8x4_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction) {
    uint16_t row;
    uint16_t column;
    size_t index;
    Av2hStatus status;
    if (mode == 0 || prefix == 0 || transform == 0 || coefficients == 0 ||
        predictor == 0 || scratch == 0 || reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (mode->mode != AV2H_INTRA_DC || mode->angle_delta != 0 ||
        mode->enable_ibp != 0U || prefix->tx_size != RECON_TX_8X4 ||
        transform->primary_type != AV2H_TX_DCT_DCT ||
        transform->secondary_signaled != 0U || coefficients->width != 8U ||
        coefficients->height != 4U || coefficients->coefficient_count != 32U ||
        predictor_stride_samples < 8U) {
        return AV2H_UNSUPPORTED;
    }
    reconstruction->width = 8U;
    reconstruction->height = 4U;
    for (index = 0U; index < 32U; ++index) {
        scratch->scaled[index] = clamp(round_shift(
            (int64_t)coefficients->dequantized[index] * 2896, 12U),
            -131072, 131071);
    }
    status = av2h_inverse_secondary_transform(
        mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < 4U; ++row) {
        int32_t row_output[8U];
        recon_transform_1d(
            scratch->scaled + (size_t)row * 8U, row_output,
            RECON_1D_DCT, 8U, 7U, 0U, 10U);
        for (column = 0U; column < 8U; ++column) {
            scratch->transposed[(size_t)row * 8U + column] =
                row_output[column];
        }
    }
    for (column = 0U; column < 8U; ++column) {
        int32_t column_input[4U];
        int32_t column_output[4U];
        for (row = 0U; row < 4U; ++row) {
            column_input[row] =
                scratch->transposed[(size_t)row * 8U + column];
        }
        recon_transform_1d(
            column_input, column_output, RECON_1D_DCT,
            4U, 10U, 1U, 10U);
        for (row = 0U; row < 4U; ++row) {
            scratch->residual[(size_t)row * 8U + column] =
                column_output[row];
        }
    }
    for (index = 0U; index < 32U; ++index) {
        int32_t residual = scratch->residual[index];
        int32_t predicted = predictor[
            (index / 8U) * predictor_stride_samples + index % 8U];
        reconstruction->residual[index] = residual;
        reconstruction->pixels[index] =
            av2h_sample_clip_10bit(predicted + residual);
    }
    return AV2H_OK;
}

int av2h_reconstruct_generic_selected(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    uint8_t tx_size) {
    uint16_t output_width;
    uint16_t output_height;
    uint8_t row_shift;
    uint8_t column_shift;
    Av2hRecon1dKind row_kind;
    Av2hRecon1dKind column_kind;
    int identity = transform->primary_type == AV2H_TX_IDTX;
    if (identity) {
        int legal_identity =
                        mode->fsc_mode ||
            mode->use_intrabc ||
            (!mode->fsc_mode && !mode->use_intrabc &&
             (tx_size == RECON_TX_4X32 || tx_size == RECON_TX_32X4 ||
              tx_size == RECON_TX_8X32 || tx_size == RECON_TX_32X8 ||
              tx_size == RECON_TX_16X32 || tx_size == RECON_TX_32X16));
        if (mode->skip_transform || !legal_identity) return 0;
    } else if (mode->skip_transform || mode->fsc_mode) {
        return 0;
    }
    if (recon_geometry(tx_size, &output_width, &output_height,
                       &row_shift, &column_shift) != AV2H_OK) {
        return 0;
    }
    if (!identity && !mode->use_intrabc && tx_size == RECON_TX_4X4 &&
                transform->primary_type != AV2H_TX_FLIPADST_FLIPADST &&
                transform->primary_type != AV2H_TX_ADST_DCT &&
                transform->primary_type != AV2H_TX_DCT_ADST &&
                transform->primary_type != AV2H_TX_DCT_FLIPADST &&
                transform->primary_type != AV2H_TX_FLIPADST_DCT &&
                transform->primary_type != AV2H_TX_ADST_FLIPADST &&
                transform->primary_type != AV2H_TX_FLIPADST_ADST &&
                transform->primary_type != AV2H_TX_ADST_ADST &&
                transform->primary_type != AV2H_TX_H_ADST &&
                transform->primary_type != AV2H_TX_V_ADST &&
                transform->primary_type != AV2H_TX_V_DCT &&
                transform->primary_type != AV2H_TX_H_DCT &&
                transform->primary_type != AV2H_TX_V_FLIPADST &&
                transform->primary_type != AV2H_TX_H_FLIPADST &&
                !((mode->mode == AV2H_INTRA_D157 ||
                   mode->mode == AV2H_INTRA_VERTICAL) &&
                    transform->primary_type == AV2H_TX_DCT_ADST)) {
        return 0;
    }
    if (!identity && !mode->use_intrabc && tx_size == RECON_TX_8X4 &&
        transform->primary_type != AV2H_TX_DCT_DCT &&
        transform->primary_type != AV2H_TX_FLIPADST_FLIPADST &&
        transform->primary_type != AV2H_TX_ADST_DCT &&
        transform->primary_type != AV2H_TX_DCT_ADST &&
        transform->primary_type != AV2H_TX_DCT_FLIPADST &&
        transform->primary_type != AV2H_TX_FLIPADST_DCT &&
        transform->primary_type != AV2H_TX_ADST_FLIPADST &&
        transform->primary_type != AV2H_TX_FLIPADST_ADST &&
        transform->primary_type != AV2H_TX_ADST_ADST &&
        transform->primary_type != AV2H_TX_H_ADST &&
        transform->primary_type != AV2H_TX_V_ADST &&
        transform->primary_type != AV2H_TX_V_DCT &&
        transform->primary_type != AV2H_TX_H_DCT &&
        transform->primary_type != AV2H_TX_V_FLIPADST &&
        transform->primary_type != AV2H_TX_H_FLIPADST) {
        return 0;
    }
    if (recon_transform_kinds(transform->primary_type, &row_kind,
                              &column_kind) != AV2H_OK) {
        return 0;
    }
    return 1;
}
