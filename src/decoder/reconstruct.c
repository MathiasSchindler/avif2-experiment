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

static const int8_t d203_filter[4U][4U] = {
    { -12, 94, 56, -10 },
    { -8, 37, 109, -10 },
    { -6, 121, 17, -4 },
    { -12, 76, 76, -12 }
};

static const int8_t d45_filter[4U][4U] = {
    { -9, 46, 102, -11 },
    { -12, 94, 56, -10 },
    { -3, 125, 8, -2 },
    { -8, 37, 109, -10 }
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

static const int8_t d67_mrl3_filter[4U][4U] = {
    { -12, 94, 56, -10 },
    { -1, 4, 127, -2 },
    { -11, 66, 85, -12 },
    { -7, 118, 22, -5 }
};

static const uint8_t smooth4_blend[4U] = {
    32U, 8U, 2U, 0U
};

static const int8_t adst8_frequency0[8U] = {
    11, 28, 44, 58, 70, 79, 86, 89
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
    return size == 4U ? 128 : 181;
}

Av2hStatus av2h_luma_predict(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    uint8_t left_storage[11U];
    uint8_t *left = &left_storage[1U];
    uint8_t above_storage[15U];
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
                        width == 4U && height == 4U) ||
                     (mode->mrl_index == 3U && mode->angle_delta == -3 &&
                        width == 4U && height == 4U)))) {
        return AV2H_UNSUPPORTED;
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
        width == 4U && height == 4U) {
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
        if (references->above == 0 || references->above_count == 0U ||
            !references->has_above_left) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[-1] = references->above_left;
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
    if (mode->mode != AV2H_INTRA_D203 || mode->angle_delta != 0 ||
        width != 4U || height != 4U) {
        return AV2H_UNSUPPORTED;
    }
    if (references->left == 0 || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    left[-1] = references->left[0];
    for (index = 0U; index < 8U; ++index) {
        size_t source = index < references->left_count ?
            index : references->left_count - 1U;
        left[index] = references->left[source];
    }
    left[8] = left[7];
    left[9] = left[7];
    for (column = 0U; column < width; ++column) {
        uint16_t offset = (uint16_t)((column + 1U) * 24U);
        uint16_t base = offset >> 6U;
        for (row = 0U; row < height; ++row, ++base) {
            int32_t value;
            if (base <= 7U) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d203_filter[column][tap] *
                           left[(int32_t)base + (int32_t)tap - 1];
                }
                value = (sum + 64) >> 7U;
            } else {
                value = left[7];
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
    if (width != 4U || height != 4U ||
        !((mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0) ||
          (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -1) ||
          (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0))) {
        return AV2H_UNSUPPORTED;
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
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            uint16_t offset = (uint16_t)((row + 1U) * 28U);
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

static Av2hStatus flip_adst_matrix_value(uint16_t size, uint16_t frequency,
                                         uint16_t output, int32_t *value) {
    size_t index;
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
                    if (input[(size_t)row * width] != 0) {
                        sum = (int64_t)input[(size_t)row * width] *
                              adst8_frequency0[output];
                    }
                    for (frequency = 1U; frequency < width; ++frequency) {
                        if (input[(size_t)row * width + frequency] != 0) {
                            return AV2H_UNSUPPORTED;
                        }
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
                    if (height != 4U) return AV2H_UNSUPPORTED;
                    matrix = adst4[frequency][output];
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
            mode->mode != AV2H_INTRA_D67 &&
            mode->mode != AV2H_INTRA_D45 && mode->mode != AV2H_INTRA_D203) ||
        coefficients->width == 0U || coefficients->height == 0U ||
        coefficients->coefficient_count !=
            coefficients->width * coefficients->height) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        (prefix->tx_size != AV2H_TX_4X4 ||
         transform->primary_type != AV2H_TX_DCT_ADST)) {
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
               (mode->mode == AV2H_INTRA_D45 ||
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