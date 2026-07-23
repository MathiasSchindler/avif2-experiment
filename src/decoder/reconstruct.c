#include "reconstruct.h"

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

static const uint8_t flip_adst8_frequency[] = { 0U, 2U };
static const int8_t flip_adst8[2U][8U] = {
    { 89, 86, 79, 70, 58, 44, 28, 11 },
    { 79, 12, -66, -87, -34, 48, 89, 54 }
};

static const int8_t d45_filter[4U][4U] = {
    { -9, 46, 102, -11 },
    { -12, 94, 56, -10 },
    { -3, 125, 8, -2 },
    { -8, 37, 109, -10 }
};

static const int16_t dr_interp_filter[32U][4U] = {
    { 0, 128, 0, 0 }, { -2, 127, 4, -1 }, { -3, 125, 8, -2 },
    { -5, 123, 13, -3 }, { -6, 121, 17, -4 }, { -7, 118, 22, -5 },
    { -9, 116, 27, -6 }, { -9, 112, 32, -7 }, { -10, 109, 37, -8 },
    { -11, 106, 41, -8 }, { -11, 102, 46, -9 }, { -12, 98, 52, -10 },
    { -12, 94, 56, -10 }, { -12, 90, 61, -11 },
    { -12, 85, 66, -11 }, { -12, 81, 71, -12 },
    { -12, 76, 76, -12 }, { -12, 71, 81, -12 },
    { -11, 66, 85, -12 }, { -11, 61, 90, -12 },
    { -10, 56, 94, -12 }, { -10, 52, 98, -12 },
    { -9, 46, 102, -11 }, { -8, 41, 106, -11 },
    { -8, 37, 109, -10 }, { -7, 32, 112, -9 },
    { -6, 27, 116, -9 }, { -5, 22, 118, -7 },
    { -4, 17, 121, -6 }, { -3, 13, 123, -5 },
    { -2, 8, 125, -3 }, { -1, 4, 127, -2 }
};

static const int8_t d67_filter[8U][4U] = {
    { -5, 22, 118, -7 },
    { -10, 109, 37, -8 },
    { -9, 46, 102, -11 },
    { -5, 123, 13, -3 },
    { -12, 71, 81, -12 },
    { -2, 8, 125, -3 },
    { -12, 94, 56, -10 },
    { -7, 32, 112, -9 }
};

static const int8_t d67_mrl4_filter[4U][4U] = {
    { -1, 4, 127, -2 },
    { -12, 85, 66, -11 },
    { -2, 8, 125, -3 },
    { -12, 90, 61, -11 }
};

static const int8_t d67_mrl8_filter[8U][4U] = {
    { -1, 4, 127, -2 },
    { -12, 85, 66, -11 },
    { -2, 8, 125, -3 },
    { -12, 90, 61, -11 },
    { -3, 13, 123, -5 },
    { -12, 94, 56, -10 },
    { -4, 17, 121, -6 },
    { -12, 98, 52, -10 }
};

static const int8_t d67_mrl3_filter[4U][4U] = {
    { -12, 94, 56, -10 },
    { -1, 4, 127, -2 },
    { -11, 66, 85, -12 },
    { -7, 118, 22, -5 }
};

static const uint8_t smooth4_blend[4U] = {
    32U, 8U, 2U, 0U
};

static const uint8_t smooth8_blend[8U] = {
    32U, 16U, 8U, 4U, 2U, 1U, 0U, 0U
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
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U
};
static const int8_t dct16[13U][16U] = {
        { 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64 },
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
            35, -83, 83, -35, -35, 83, -83, 35 }
};

static const uint8_t dct32_frequency[] = { 0U, 1U, 3U, 5U, 7U };
static const int8_t dct32[5U][32U] = {
    {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    },
    {
        90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 47, 39, 30, 22, 13, 4,
        -4, -13, -22, -30, -39, -47, -54, -61, -67, -73, -78, -82,
        -85, -88, -90, -90
    },
    {
        90, 82, 67, 47, 22, -4, -30, -54, -73, -85, -90, -88, -78, -61,
        -39, -13, 13, 39, 61, 78, 88, 90, 85, 73, 54, 30, 4, -22, -47,
        -67, -82, -90
    },
    {
        88, 67, 30, -13, -54, -82, -90, -78, -47, -4, 39, 73, 90, 85,
        61, 22, -22, -61, -85, -90, -73, -39, 4, 47, 78, 90, 82, 54,
        13, -30, -67, -88
    },
    {
        85, 47, -13, -67, -90, -73, -22, 39, 82, 88, 54, -4, -61, -90,
        -78, -30, 30, 78, 90, 61, 4, -54, -88, -82, -39, 22, 73, 90,
        67, 13, -47, -85
    }
};

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t round_shift(int64_t value, uint8_t shift) {
    int64_t adjusted = value + ((int64_t)1 << (shift - 1U));
    if (adjusted >= 0) return (int32_t)(adjusted >> shift);
    return (int32_t)-(((-adjusted) + (((int64_t)1 << shift) - 1)) >> shift);
}

static int32_t identity_scale(uint16_t size) {
    return size == 4U ? 128 : size == 8U ? 181 :
        size == 16U ? 256 : 362;
}

Av2hStatus av2h_luma_predict(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    uint8_t left_storage[44U];
    uint8_t *left = &left_storage[1U];
    uint8_t above_storage[29U];
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    if (mode == 0 || references == 0 || predictor == 0 ||
        predictor_stride < width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((mode->mrl_index != 0U || mode->multi_line_mrl != 0U) &&
                !(mode->mode == AV2H_INTRA_D67 &&
                    mode->multi_line_mrl == 0U &&
                    ((mode->mrl_index == 1U && mode->angle_delta == -1 &&
                        width == 8U && height == 8U) ||
                     (mode->mrl_index == 1U && mode->angle_delta == -2 &&
                        ((width == 4U && height == 4U) ||
                         (width == 8U && height == 8U))) ||
                            (mode->mrl_index == 3U && mode->angle_delta == -3 &&
                                width == 4U && height == 4U))) &&
                        !(mode->mode == AV2H_INTRA_D45 &&
                          mode->multi_line_mrl == 0U && mode->mrl_index == 1U &&
                          mode->angle_delta == 3 && width == 8U && height == 16U)) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 &&
        width == 4U && height == 4U) {
        unsigned int sum = 0U;
        uint8_t value;
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < width; ++index) {
            sum += references->above[index];
        }
        for (index = 0U; index < height; ++index) {
            sum += references->left[index];
        }
        value = (uint8_t)((sum + 4U) >> 3U);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
        mode->angle_delta == 0 &&
        ((width == 4U && height == 4U) ||
         (width == 8U && height == 8U))) {
        const uint8_t *blend = height == 4U ? smooth4_blend : smooth8_blend;
        uint8_t bottom_left;
        if (references->left == 0 || references->left_count < height ||
            (references->above != 0 && references->above_count < width)) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = references->left[height - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t top = references->above == 0 ? references->left[0] :
                    references->above[column];
                int32_t value = bottom_left + round_shift(
                    ((int64_t)top - bottom_left) *
                        (height - 1U - row),
                    height == 4U ? 2U : 3U);
                value += round_shift(
                    ((int64_t)top - value) * blend[row],
                    6U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
        mode->angle_delta == 0 && width == 4U && height == 8U) {
        static const uint8_t weight[4U] = { 255U, 149U, 85U, 64U };
        uint8_t top_right;
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        top_right = references->above[width - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint32_t value =
                    (uint32_t)weight[column] * references->left[row] +
                    (uint32_t)(256U - weight[column]) * top_right;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 128U) >> 8U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        width == 4U && height == 4U) {
        uint8_t bottom_left;
        uint8_t top_right;
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = references->left[height - 1U];
        top_right = references->above[width - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t vertical = bottom_left + round_shift(
                    ((int64_t)references->above[column] - bottom_left) *
                        (height - 1U - row),
                    2U);
                int32_t horizontal = top_right + round_shift(
                    ((int64_t)references->left[row] - top_right) *
                        (width - 1U - column),
                    2U);
                vertical += round_shift(
                    ((int64_t)references->above[column] - vertical) *
                        smooth4_blend[row],
                    6U);
                horizontal += round_shift(
                    ((int64_t)references->left[row] - horizontal) *
                        smooth4_blend[column],
                    6U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)round_shift(vertical + horizontal, 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        width == 8U && height == 32U) {
        uint8_t bottom_left;
        uint8_t top_right;
        if (references->left == 0 || references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = references->left_count > height ?
            references->left[height] : references->left[height - 1U];
        top_right = references->above_count > width ?
            references->above[width] :
            references->above_count != 0U ?
                references->above[references->above_count - 1U] :
                references->left[0];
        for (row = 0U; row < height; ++row) {
            uint8_t top_blend = row < 6U ? (uint8_t)(32U >> row) : 0U;
            for (column = 0U; column < width; ++column) {
                uint8_t top = references->above_count != 0U ?
                    references->above[column] : references->left[0];
                int32_t vertical = bottom_left + round_shift(
                    ((int64_t)top - bottom_left) *
                        (height - 1U - row),
                    5U);
                int32_t horizontal = top_right + round_shift(
                    ((int64_t)references->left[row] - top_right) *
                        (width - 1U - column),
                    3U);
                vertical += round_shift(
                    ((int64_t)top - vertical) * top_blend, 6U);
                horizontal += round_shift(
                    ((int64_t)references->left[row] - horizontal) *
                        smooth8_blend[column],
                    6U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)round_shift(vertical + horizontal, 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_PAETH && mode->angle_delta == 0 &&
        width == 4U && height == 4U) {
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height ||
            !references->has_above_left) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t top = references->above[column];
                int32_t side = references->left[row];
                int32_t corner = references->above_left;
                int32_t base = top + side - corner;
                int32_t side_distance = base > side ? base - side : side - base;
                int32_t top_distance = base > top ? base - top : top - base;
                int32_t corner_distance = base > corner ?
                    base - corner : corner - base;
                int32_t value = side_distance <= top_distance &&
                    side_distance <= corner_distance ? side :
                    top_distance <= corner_distance ? top : corner;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        ((width == 4U && height == 4U) ||
         (width == 16U && height == 8U))) {
        size_t edge_count = (size_t)width + height;
        if (references->above_count == 0U && references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < edge_count; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - (references->above_count != 0U);
            above[index] = references->above_count != 0U ?
                references->above[source] : references->left[0];
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t source = (size_t)row + column + 1U;
                predictor[(size_t)row * predictor_stride + column] =
                    above[source];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -1 &&
        mode->mrl_index == 1U && width == 8U && height == 8U) {
        uint8_t storage[21U];
        uint8_t *edge = &storage[1U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < 20U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        edge[-1] = edge[0];
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((54U + row * 27U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_filter[row][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
        mode->mrl_index == 1U && width == 4U && height == 4U) {
        uint8_t storage[13U];
        uint8_t *edge = &storage[1U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[-1] = references->has_above_left ? references->above_left :
            references->above[0];
        for (index = 0U; index < 12U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((62U + row * 31U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_mrl4_filter[row][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
        mode->mrl_index == 1U && width == 8U && height == 8U) {
        uint8_t storage[21U];
        uint8_t *edge = &storage[1U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[-1] = references->has_above_left ? references->above_left :
            references->above[0];
        for (index = 0U; index < 20U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((62U + row * 31U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_mrl8_filter[row][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -3 &&
        mode->mrl_index == 3U && width == 4U && height == 4U) {
        uint8_t edge[12U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < 12U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((152U + row * 38U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_mrl3_filter[row][tap] *
                           edge[base + tap - 1U];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 &&
        width == 8U && height == 4U) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        above[-1] = references->above[0];
        for (index = 0U; index < 12U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            above[index] = references->above[source];
        }
        above[12] = above[11];
        above[13] = above[11];
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)(((row + 1U) * 44U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                int32_t value;
                if (base <= 11U) {
                    size_t tap;
                    int32_t sum = 0;
                    for (tap = 0U; tap < 4U; ++tap) {
                        sum += d45_filter[row][tap] *
                               above[(int32_t)base + (int32_t)tap - 1];
                    }
                    value = (sum + 64) >> 7U;
                } else {
                    value = above[11];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp(value, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        width == 8U && height == 16U) {
        const uint16_t derivative = 97U;
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (column = 0U; column < width; ++column) {
            uint16_t position = (uint16_t)((column + 2U) * derivative);
            uint16_t base = position >> 6U;
            uint8_t shift = (uint8_t)((position >> 1U) & 0x1fU);
            for (row = 0U; row < height; ++row, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    size_t source = base + tap - 1U;
                    if (source >= references->left_count) {
                        source = references->left_count - 1U;
                    }
                    sum += dr_interp_filter[shift][tap] *
                           references->left[source];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
        if (mode->mode != AV2H_INTRA_D203 ||
                (mode->angle_delta != 0 && mode->angle_delta != -3) ||
        !((width == 4U && height == 4U) ||
                    (width == 8U && height == 16U) ||
          (width == 8U && height == 32U))) {
        return AV2H_UNSUPPORTED;
    }
    if (references->left == 0 || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    left[-1] = references->left[0];
    for (index = 0U; index < (size_t)width + height; ++index) {
        size_t source = index < references->left_count ?
            index : references->left_count - 1U;
        left[index] = references->left[source];
    }
    left[width + height] = left[width + height - 1U];
    left[width + height + 1U] = left[width + height - 1U];
    for (column = 0U; column < width; ++column) {
        uint16_t derivative = mode->angle_delta == 0 ? 24U : 12U;
        uint16_t offset = (uint16_t)((column + 1U) * derivative);
        uint16_t base = offset >> 6U;
        uint8_t shift = (uint8_t)((offset >> 1U) & 0x1fU);
        for (row = 0U; row < height; ++row, ++base) {
            int32_t value;
            if (base <= width + height - 1U) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                           left[(int32_t)base + (int32_t)tap - 1];
                }
                value = (sum + 64) >> 7U;
            } else {
                value = left[width + height - 1U];
            }
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(value, 0, 255);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_chroma_predict(
    const Av2hFirstChromaMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    uint16_t row;
    uint16_t column;
    if (mode == 0 || references == 0 || predictor == 0 ||
        predictor_stride < width) {
        return AV2H_INVALID_ARGUMENT;
    }
        if (!((width == 4U && height == 4U &&
                    ((mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0) ||
          (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0) ||
          (mode->mode == AV2H_INTRA_D67 &&
           (mode->angle_delta == -1 || mode->angle_delta == -2)) ||
                     (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0))) ||
                    (width == 4U && height == 8U &&
                     mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3))) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_DC) {
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t value;
        if (references->above != 0) {
            for (column = 0U; column < width; ++column) {
                sum += references->above[column];
            }
            count += width;
        }
        if (references->left != 0) {
            for (row = 0U; row < height; ++row) {
                sum += references->left[row];
            }
            count += height;
        }
        value = count == 0U ? 128U : (uint8_t)((sum + count / 2U) / count);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) {
        const uint16_t derivative = 93U;
        uint8_t edge[13U];
        uint8_t filtered[13U];
        size_t edge_index;
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[0] = references->left[0];
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t source = edge_index - 1U;
            edge[edge_index] = source < references->left_count ?
                references->left[source] :
                references->left[references->left_count - 1U];
        }
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t previous = edge_index == 1U ? 0U : edge_index - 1U;
            size_t next = edge_index + 1U < sizeof(edge) ?
                edge_index + 1U : edge_index;
            int32_t value = edge[previous] + 2 * edge[edge_index] +
                edge[next];
            filtered[edge_index] = (uint8_t)((value + 2) >> 2U);
        }
        filtered[0] = edge[0];
        for (column = 0U; column < width; ++column) {
            uint16_t offset = (uint16_t)((column + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (row = 0U; row < height; ++row, ++base) {
                size_t first = base + 1U < sizeof(edge) ?
                    base + 1U : sizeof(edge) - 1U;
                size_t second = first + 1U < sizeof(edge) ?
                    first + 1U : sizeof(edge) - 1U;
                int32_t value = filtered[first] * (32 - shift) +
                    filtered[second] * shift;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t source = (size_t)row + column + 1U;
                if (source >= references->above_count) {
                    source = references->above_count - 1U;
                }
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[source];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67) {
        uint16_t derivative = mode->angle_delta == -1 ? 28U : 32U;
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            uint16_t offset = (uint16_t)((row + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (column = 0U; column < width; ++column, ++base) {
                size_t first = base < references->above_count ?
                    base : references->above_count - 1U;
                size_t second = first + 1U < references->above_count ?
                    first + 1U : references->above_count - 1U;
                int32_t value = references->above[first] * (32 - shift) +
                    references->above[second] * shift;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (references->left == 0 || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (column = 0U; column < width; ++column) {
        uint16_t offset = (uint16_t)((column + 1U) * 24U);
        uint16_t base = offset >> 6U;
        uint16_t shift = (offset & 63U) >> 1U;
        for (row = 0U; row < height; ++row, ++base) {
            size_t first = base < references->left_count ?
                base : references->left_count - 1U;
            size_t second = first + 1U < references->left_count ?
                first + 1U : references->left_count - 1U;
            int32_t value = references->left[first] * (32 - shift) +
                references->left[second] * shift;
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)((value + 16) >> 5U);
        }
    }
    return AV2H_OK;
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
        for (index = 0U; index < 5U; ++index) {
            if (dct32_frequency[index] == frequency) {
                *value = dct32[index][output];
                return AV2H_OK;
            }
        }
        return AV2H_UNSUPPORTED;
    }
    return AV2H_UNSUPPORTED;
}

static uint8_t plane_sample_reconstructed(const Av2hReconstructionPlane *plane,
                                          size_t x, size_t y) {
    size_t index = (y / 4U) * plane->reconstructed_stride + x / 4U;
    return index < plane->reconstructed_count && plane->reconstructed[index];
}

static void plane_mark_reconstructed(Av2hReconstructionPlane *plane,
                                     uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height) {
    size_t first_column = x / 4U;
    size_t first_row = y / 4U;
    size_t last_column = ((size_t)x + width - 1U) / 4U;
    size_t last_row = ((size_t)y + height - 1U) / 4U;
    size_t row;
    size_t column;
    for (row = first_row; row <= last_row; ++row) {
        for (column = first_column; column <= last_column; ++column) {
            plane->reconstructed[row * plane->reconstructed_stride + column] =
                1U;
        }
    }
}

static Av2hStatus flip_adst_matrix_value(uint16_t size, uint16_t frequency,
                                         uint16_t output, int32_t *value) {
    size_t index;
    if (size == 4U) {
        *value = adst4[frequency][size - output - 1U];
        if ((frequency & 1U) != 0U) *value = -*value;
        return AV2H_OK;
    }
    if (size != 8U) return AV2H_UNSUPPORTED;
    for (index = 0U; index < 2U; ++index) {
        if (flip_adst8_frequency[index] == frequency) {
            *value = flip_adst8[index][output];
            return AV2H_OK;
        }
    }
    return AV2H_UNSUPPORTED;
}

static Av2hStatus inverse_rows(const int32_t *input, int32_t *transposed,
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

static Av2hStatus inverse_columns(const int32_t *transposed,
                                  int32_t *output_block,
                                  uint16_t width, uint16_t height,
                                  uint8_t shift, uint8_t identity,
                                  uint8_t adst) {
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
                    } else {
                        return AV2H_UNSUPPORTED;
                    }
                    status = AV2H_OK;
                } else {
                    status = matrix_value(height, frequency, output, &matrix);
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

Av2hStatus av2h_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    size_t index;
    size_t count;
    uint8_t first_shift;
    uint8_t second_shift;
    uint8_t rectangular;
    uint8_t identity_rows;
    uint8_t adst_rows;
    uint8_t flip_adst_rows;
    uint8_t identity_columns;
    uint8_t adst_columns;
    Av2hStatus status;
    if (mode == 0 || prefix == 0 || transform == 0 || coefficients == 0 ||
        scratch == 0 || reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
        if ((mode->mode != AV2H_INTRA_DC && mode->mode != AV2H_INTRA_PAETH &&
            mode->mode != AV2H_INTRA_SMOOTH &&
            mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
            mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
            mode->mode != AV2H_INTRA_D67 &&
            mode->mode != AV2H_INTRA_D45 && mode->mode != AV2H_INTRA_D203) ||
        coefficients->width == 0U || coefficients->height == 0U ||
        coefficients->coefficient_count !=
            coefficients->width * coefficients->height) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        !((prefix->tx_size == AV2H_TX_4X4 &&
           transform->primary_type == AV2H_TX_DCT_ADST) ||
          (prefix->tx_size == AV2H_TX_8X32 &&
           transform->primary_type == AV2H_TX_H_DCT))) {
        return AV2H_UNSUPPORTED;
    }
    if (prefix->tx_size == AV2H_TX_8X4 &&
        transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               (mode->use_intrabc || mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_H_ADST &&
               mode->mode == AV2H_INTRA_D203) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_H_FLIPADST &&
               mode->use_intrabc) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_D45) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X16 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->mode == AV2H_INTRA_D45 && mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_D45) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X16 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_D203) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 &&
               transform->primary_type == AV2H_TX_H_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    reconstruction->width = coefficients->width;
    reconstruction->height = coefficients->height;
    count = coefficients->coefficient_count;
    for (index = 0U; index < count; ++index) {
        scratch->scaled[index] = rectangular ?
            round_shift((int64_t)coefficients->dequantized[index] * 2896, 12U) :
            coefficients->dequantized[index];
        scratch->scaled[index] = clamp(scratch->scaled[index], -32768, 32767);
    }
    status = inverse_rows(scratch->scaled, scratch->transposed,
                          coefficients->width, coefficients->height,
                          first_shift, identity_rows, adst_rows,
                          flip_adst_rows);
    if (status != AV2H_OK) return status;
    status = inverse_columns(scratch->transposed, scratch->residual,
                             coefficients->width, coefficients->height,
                             second_shift, identity_columns, adst_columns);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < count; ++index) {
        int32_t predicted = predictor == 0 ? 128 :
            predictor[(index / coefficients->width) * predictor_stride +
                      index % coefficients->width];
        int32_t pixel = clamp(predicted + scratch->residual[index], 0, 255);
        reconstruction->residual[index] = (int16_t)scratch->residual[index];
        reconstruction->pixels[index] = (uint8_t)pixel;
    }
    return AV2H_OK;
}

Av2hStatus av2h_first_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    return av2h_luma_reconstruct(mode, prefix, transform, coefficients,
                                 0, 0U, scratch, reconstruction);
}

Av2hStatus av2h_luma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch) {
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references;
    uint16_t block_x;
    uint16_t block_y;
    size_t above_count;
    size_t left_count;
    size_t index;
    int32_t source_x;
    int32_t source_y;
    Av2hStatus status;
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        plane->stride < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == 0 ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count < plane->reconstructed_stride *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    if ((uint32_t)block_x + coefficients->width > plane->width ||
        (uint32_t)block_y + coefficients->height > plane->height ||
        coefficients->width > 32U || coefficients->height > 32U) {
        return AV2H_INVALID_ARGUMENT;
    }
    above_count = block_y <= mode->mrl_index ||
        block_x >= plane->reference_width ? 0U :
        (size_t)plane->reference_width - block_x;
    if (above_count > sizeof(scratch->above)) {
        above_count = sizeof(scratch->above);
    }
    for (index = 0U; index < above_count; ++index) {
        if (!plane_sample_reconstructed(
                plane, block_x + index,
                (size_t)block_y - 1U - mode->mrl_index)) {
            above_count = index;
            break;
        }
    }
    for (index = 0U; index < above_count; ++index) {
        size_t source_x = block_x + index;
        if (source_x >= plane->reference_width) {
            source_x = plane->reference_width - 1U;
        }
        scratch->above[index] = plane->data[
            ((size_t)block_y - 1U - mode->mrl_index) * plane->stride +
            source_x];
    }
    left_count = block_x <= mode->mrl_index ? 0U : coefficients->height;
    if (left_count != 0U && mode->mode == AV2H_INTRA_SMOOTH &&
        block_y + left_count < plane->height) {
        ++left_count;
    }
    if (left_count > sizeof(scratch->left)) {
        left_count = sizeof(scratch->left);
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->reference_height) {
            source_y = plane->reference_height - 1U;
        }
        if (!plane_sample_reconstructed(
                plane, (size_t)block_x - 1U - mode->mrl_index, source_y)) {
            left_count = index;
            break;
        }
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->reference_height) {
            source_y = plane->reference_height - 1U;
        }
        scratch->left[index] = plane->data[
            source_y * plane->stride + block_x - 1U -
            mode->mrl_index];
    }
    references.left = left_count == 0U ? 0 : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? 0 : scratch->above;
    references.above_count = above_count;
    references.has_above_left = block_x > mode->mrl_index &&
        block_y > mode->mrl_index;
    references.above_left = references.has_above_left ?
        plane->data[((size_t)block_y - 1U - mode->mrl_index) * plane->stride +
                    block_x - 1U - mode->mrl_index] :
        0U;
    if (mode->use_intrabc) {
        size_t row;
        size_t column;
        if (mode->displacement_x % 8 != 0 ||
            mode->displacement_y % 8 != 0) {
            return AV2H_UNSUPPORTED;
        }
        source_x = (int32_t)block_x + mode->displacement_x / 8;
        source_y = (int32_t)block_y + mode->displacement_y / 8;
        if (source_x < 0 || source_y < 0 ||
            (uint32_t)source_x + coefficients->width > plane->width ||
            (uint32_t)source_y + coefficients->height > plane->height) {
            return AV2H_INVALID_DATA;
        }
        if ((uint32_t)source_y + coefficients->height > block_y) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < coefficients->height; ++row) {
            for (column = 0U; column < coefficients->width; ++column) {
                scratch->predictor[row * coefficients->width + column] =
                    plane->data[((size_t)source_y + row) * plane->stride +
                                (size_t)source_x + column];
            }
        }
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            coefficients->width, scratch, &reconstruction);
    } else if (mode->mode == AV2H_INTRA_DC && above_count == 0U &&
               left_count == 0U) {
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, 0, 0U,
            scratch, &reconstruction);
    } else {
        status = av2h_luma_predict(
            mode, coefficients->width, coefficients->height, &references,
            scratch->predictor, coefficients->width);
        if (status != AV2H_OK) return status;
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            coefficients->width, scratch, &reconstruction);
    }
    if (status != AV2H_OK) return status;
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        size_t row = index / coefficients->width;
        size_t column = index % coefficients->width;
        plane->data[((size_t)block_y + row) * plane->stride +
                    block_x + column] = reconstruction.pixels[index];
    }
    plane_mark_reconstructed(plane, block_x, block_y,
                             coefficients->width, coefficients->height);
    return AV2H_OK;
}

Av2hStatus av2h_first_chroma_reconstruct(
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    Av2hFirstLeafMode mode = { 0 };
    if (prefix == 0 || transform == 0 || coefficients == 0 || scratch == 0 ||
        reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->tx_size != AV2H_TX_4X4 ||
        transform->primary_type != AV2H_TX_DCT_DCT ||
        coefficients->width != 4U || coefficients->height != 4U) {
        return AV2H_UNSUPPORTED;
    }
    mode.mode = AV2H_INTRA_DC;
    return av2h_luma_reconstruct(&mode, prefix, transform, coefficients,
                                 0, 0U, scratch, reconstruction);
}

Av2hStatus av2h_chroma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch) {
    Av2hFirstLeafMode luma_mode = { 0 };
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references;
    uint16_t block_x;
    uint16_t block_y;
    size_t above_count;
    size_t left_count;
    size_t index;
    Av2hStatus status;
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        plane->stride < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == 0 ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count < plane->reconstructed_stride *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    block_x = (uint16_t)((uint32_t)leaf->column * 2U);
    block_y = (uint16_t)((uint32_t)leaf->row * 2U);
    if ((uint32_t)block_x + coefficients->width > plane->width ||
        (uint32_t)block_y + coefficients->height > plane->height ||
        coefficients->width != 4U ||
        (coefficients->height != 4U && coefficients->height != 8U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    above_count = block_y == 0U || block_x >= plane->reference_width ? 0U :
        (size_t)plane->reference_width - block_x;
    if (above_count > sizeof(scratch->above)) {
        above_count = sizeof(scratch->above);
    }
    for (index = 0U; index < above_count; ++index) {
        if (!plane_sample_reconstructed(
                plane, block_x + index, (size_t)block_y - 1U)) {
            above_count = index;
            break;
        }
    }
    for (index = 0U; index < above_count; ++index) {
        scratch->above[index] = plane->data[
            ((size_t)block_y - 1U) * plane->stride + block_x + index];
    }
    left_count = block_x == 0U ? 0U : coefficients->height;
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->reference_height) {
            source_y = plane->reference_height - 1U;
        }
        if (!plane_sample_reconstructed(
                plane, (size_t)block_x - 1U, source_y)) {
            left_count = index;
            break;
        }
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->reference_height) {
            source_y = plane->reference_height - 1U;
        }
        scratch->left[index] = plane->data[
            source_y * plane->stride + block_x - 1U];
    }
    references.left = left_count == 0U ? 0 : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? 0 : scratch->above;
    references.above_count = above_count;
    references.above_left = 0U;
    references.has_above_left = 0U;
    status = av2h_chroma_predict(
        mode, coefficients->width, coefficients->height, &references,
        scratch->predictor, coefficients->width);
    if (status != AV2H_OK) return status;
    luma_mode.mode = mode->mode;
    luma_mode.angle_delta = mode->angle_delta;
    status = av2h_luma_reconstruct(
        &luma_mode, prefix, transform, coefficients, scratch->predictor,
        coefficients->width, scratch, &reconstruction);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        size_t row = index / coefficients->width;
        size_t column = index % coefficients->width;
        plane->data[((size_t)block_y + row) * plane->stride +
                    block_x + column] = reconstruction.pixels[index];
    }
    plane_mark_reconstructed(plane, block_x, block_y,
                             coefficients->width, coefficients->height);
    return AV2H_OK;
}