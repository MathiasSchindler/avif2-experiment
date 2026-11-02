#include "internal/intra_prediction.h"
#include "internal/sample_math.h"

static const uint8_t smooth8_blend[8U] = {
    32U, 16U, 8U, 4U, 2U, 1U, 0U, 0U
};

static const uint8_t ibp_weights[5U][16U] = {
    { 96U },
    { 86U, 107U },
    { 77U, 90U, 102U, 115U },
    { 71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U },
    { 68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U,
      98U, 102U, 106U, 109U, 113U, 117U, 121U, 124U }
};

static uint8_t smooth_scale(uint16_t width, uint16_t height) {
    unsigned int sum =
        (unsigned int)av2h_intra_block_log2(width) - 2U +
        (unsigned int)av2h_intra_block_log2(height) - 2U;
    return (uint8_t)((sum + 2U) >> 2U);
}

static uint8_t smooth_blend(uint16_t position, uint8_t scale) {
    unsigned int distance = ((unsigned int)position << 1U) >> scale;
    return distance >= 6U ? 0U : (uint8_t)(64U >> distance);
}

static uint8_t dc_predictor_average(unsigned int sum, unsigned int count,
                                    uint16_t width, uint16_t height,
                                    uint8_t have_top, uint8_t have_left) {
    if (count == 0U) return av2h_sample_midpoint_8bit();
    if (have_top && have_left && width != height) {
        int shift;
        uint16_t scale = av2h_sample_resolve_divisor_32(count, &shift);
        return (uint8_t)(((uint64_t)sum * scale +
                          ((uint64_t)1U << (shift - 1))) >> shift);
    }
    return (uint8_t)((sum + count / 2U) / count);
}

Av2hStatus av2h_predict_dc(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    unsigned int sum = 0U;
    unsigned int count = 0U;
    uint8_t value;
    if (references->above != 0 && references->above_count >= width) {
        for (index = 0U; index < width; ++index) {
            sum += references->above[index];
        }
        count += width;
    }
    if (references->left != 0 && references->left_count >= height) {
        for (index = 0U; index < height; ++index) {
            sum += references->left[index];
        }
        count += height;
    }
    value = dc_predictor_average(
        sum, count, width, height,
        (uint8_t)(references->above != 0 &&
                  references->above_count >= width),
        (uint8_t)(references->left != 0 &&
                  references->left_count >= height));
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] = value;
        }
    }
    if (mode->enable_ibp && !(width == 4U && height == 4U)) {
        if (references->above != 0 &&
            references->above_count >= width) {
            uint16_t row_limit = (uint16_t)(height >> 2U);
            uint16_t column_start =
                width < height && references->left != 0 &&
                    references->left_count >= height ?
                    (uint16_t)(width >> 2U) : 0U;
            uint8_t weight_row = (uint8_t)(av2h_intra_block_log2(height) - 2U);
            for (row = 0U; row < row_limit; ++row) {
                unsigned int weight = ibp_weights[weight_row][row];
                for (column = column_start; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride_samples + column] =
                        (uint8_t)(
                            (references->above[column] *
                                 (128U - weight) +
                             predictor[
                                 (size_t)row * predictor_stride_samples + column] *
                                 weight +
                             64U) >>
                            7U);
                }
            }
        }
        if (references->left != 0 &&
            references->left_count >= height) {
            uint16_t row_start =
                width >= height && references->above != 0 &&
                    references->above_count >= width ?
                    (uint16_t)(height >> 2U) : 0U;
            uint16_t column_limit = (uint16_t)(width >> 2U);
            uint8_t weight_row = (uint8_t)(av2h_intra_block_log2(width) - 2U);
            for (row = row_start; row < height; ++row) {
                for (column = 0U; column < column_limit; ++column) {
                    unsigned int weight =
                        ibp_weights[weight_row][column];
                    predictor[(size_t)row * predictor_stride_samples + column] =
                        (uint8_t)(
                            (references->left[row] *
                                 (128U - weight) +
                             predictor[
                                 (size_t)row * predictor_stride_samples + column] *
                                 weight +
                             64U) >>
                            7U);
                }
            }
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_smooth_vertical(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    uint8_t scale = smooth_scale(width, height);
    uint8_t bottom_left;
    int have_above = references->above != 0 &&
        references->above_count != 0U;
    int have_left = references->left != 0 &&
        references->left_count != 0U;
    if (have_left && references->left_count < height) {
        return AV2H_INVALID_ARGUMENT;
    }
    bottom_left = !have_left ? have_above ? references->above[0] :
        av2h_sample_above_midpoint_8bit() :
        references->left_count > height ?
        references->left[height] : references->left[height - 1U];
    for (row = 0U; row < height; ++row) {
        uint8_t blend = smooth_blend(row, scale);
        for (column = 0U; column < width; ++column) {
            uint8_t top = have_above ? references->above[
                column < references->above_count ? column :
                references->above_count - 1U] :
                have_left ? references->left[0] :
                av2h_sample_below_midpoint_8bit();
            int32_t value = bottom_left + av2h_intra_round_shift(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                av2h_intra_block_log2(height));
            value += av2h_intra_round_shift(
                ((int64_t)top - value) * blend, 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)value;
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_smooth_horizontal(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    uint8_t scale = smooth_scale(width, height);
    uint8_t top_right;
    int have_above = references->above != 0 &&
        references->above_count != 0U;
    int have_left = references->left != 0 &&
        references->left_count != 0U;
    top_right = !have_above ? have_left ? references->left[0] :
        av2h_sample_below_midpoint_8bit() :
        references->above_count > width ?
        references->above[width] :
        references->above[references->above_count - 1U];
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            uint8_t left = have_left ? references->left[
                row < references->left_count ? row :
                references->left_count - 1U] :
                have_above ? references->above[0] :
                av2h_sample_above_midpoint_8bit();
            uint8_t blend = smooth_blend(column, scale);
            int32_t value = top_right + av2h_intra_round_shift(
                ((int64_t)left - top_right) *
                    (width - 1U - column),
                av2h_intra_block_log2(width));
            value += av2h_intra_round_shift(
                ((int64_t)left - value) * blend, 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)value;
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_smooth(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    uint8_t scale = smooth_scale(width, height);
    uint8_t bottom_left;
    uint8_t top_right;
    int have_above = references->above != 0 &&
        references->above_count != 0U;
    int have_left = references->left != 0 &&
        references->left_count != 0U;
    if ((have_above && references->above_count < width) ||
        (have_left && references->left_count < height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    bottom_left = !have_left ? have_above ? references->above[0] :
        av2h_sample_above_midpoint_8bit() :
        references->left_count > height ?
        references->left[height] : references->left[height - 1U];
    top_right = !have_above ? have_left ? references->left[0] :
        av2h_sample_below_midpoint_8bit() :
        references->above_count > width ?
        references->above[width] : references->above[width - 1U];
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            uint8_t top = !have_above ?
                have_left ? references->left[0] :
                av2h_sample_below_midpoint_8bit() :
                references->above[column];
            uint8_t left = !have_left ?
                have_above ? references->above[0] :
                av2h_sample_above_midpoint_8bit() :
                references->left[row];
            uint8_t top_blend = smooth_blend(row, scale);
            uint8_t left_blend = smooth_blend(column, scale);
            int32_t vertical = bottom_left + av2h_intra_round_shift(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                av2h_intra_block_log2(height));
            int32_t horizontal = top_right + av2h_intra_round_shift(
                ((int64_t)left - top_right) *
                    (width - 1U - column),
                av2h_intra_block_log2(width));
            vertical += av2h_intra_round_shift(
                ((int64_t)top - vertical) *
                    top_blend, 7U);
            horizontal += av2h_intra_round_shift(
                ((int64_t)left - horizontal) *
                    left_blend, 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)av2h_intra_round_shift(vertical + horizontal, 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_smooth_16x8(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    uint8_t scale = smooth_scale(width, height);
    uint8_t bottom_left;
    uint8_t top_right;
    if (references->above == 0 || references->above_count < width ||
        references->left == 0 || references->left_count < height) {
        return AV2H_INVALID_ARGUMENT;
    }
    bottom_left = references->left_count > height ?
        references->left[height] : references->left[height - 1U];
    top_right = references->above_count > width ?
        references->above[width] : references->above[width - 1U];
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            uint8_t top = references->above[column];
            uint8_t left = references->left[row];
            int32_t vertical = bottom_left + av2h_intra_round_shift(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                av2h_intra_block_log2(height));
            int32_t horizontal = top_right + av2h_intra_round_shift(
                ((int64_t)left - top_right) *
                    (width - 1U - column),
                av2h_intra_block_log2(width));
            vertical += av2h_intra_round_shift(
                ((int64_t)top - vertical) *
                    smooth_blend(row, scale), 7U);
            horizontal += av2h_intra_round_shift(
                ((int64_t)left - horizontal) *
                    smooth_blend(column, scale), 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)av2h_intra_round_shift(vertical + horizontal, 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_smooth_8x32(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
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
            int32_t vertical = bottom_left + av2h_intra_round_shift(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                5U);
            int32_t horizontal = top_right + av2h_intra_round_shift(
                ((int64_t)references->left[row] - top_right) *
                    (width - 1U - column),
                3U);
            vertical += av2h_intra_round_shift(
                ((int64_t)top - vertical) * top_blend, 6U);
            horizontal += av2h_intra_round_shift(
                ((int64_t)references->left[row] - horizontal) *
                    smooth8_blend[column],
                6U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)av2h_intra_round_shift(vertical + horizontal, 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_paeth(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (references->above != 0 && references->above_count >= width &&
        (references->left == 0 || references->left_count == 0U)) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    references->above[column];
            }
        }
        return AV2H_OK;
    }
    if (references->left != 0 && references->left_count >= height &&
        (references->above == 0 || references->above_count == 0U)) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    references->left[row];
            }
        }
        return AV2H_OK;
    }
    if ((references->above == 0 || references->above_count == 0U) &&
        (references->left == 0 || references->left_count == 0U)) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    av2h_sample_midpoint_8bit();
            }
        }
        return AV2H_OK;
    }
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
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)value;
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_smooth_u16(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferencesU16 *references,
    uint16_t *predictor,
    size_t predictor_stride_samples) {
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t row;
    uint16_t column;
    int have_above;
    int have_left;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    have_above = above != 0 && above_count != 0U;
    have_left = left != 0 && left_count != 0U;
    uint8_t scale = smooth_scale(width, height);
    uint16_t bottom_left = !have_left ? have_above ? above[0] : 513U :
        left_count > height ? left[height] : left[left_count - 1U];
    uint16_t top_right = !have_above ? have_left ? left[0] : 511U :
        above_count > width ? above[width] : above[above_count - 1U];
    if ((have_above && above_count < width) ||
        (have_left && left_count < height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            uint16_t top = have_above ? above[column] :
                have_left ? left[0] : 511U;
            uint16_t side = have_left ? left[row] :
                have_above ? above[0] : 513U;
            int32_t vertical = bottom_left + av2h_intra_round_shift(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                av2h_intra_block_log2(height));
            int32_t horizontal = top_right + av2h_intra_round_shift(
                ((int64_t)side - top_right) *
                    (width - 1U - column),
                av2h_intra_block_log2(width));
            vertical += av2h_intra_round_shift(
                ((int64_t)top - vertical) * smooth_blend(row, scale),
                7U);
            horizontal += av2h_intra_round_shift(
                ((int64_t)side - horizontal) *
                    smooth_blend(column, scale),
                7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ?
                    (uint16_t)vertical :
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ?
                    (uint16_t)horizontal :
                    (uint16_t)av2h_intra_round_shift(vertical + horizontal, 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_paeth_u16(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferencesU16 *references,
    uint16_t *predictor,
    size_t predictor_stride_samples) {
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t above_left;
    uint8_t has_above_left;
    uint16_t row;
    uint16_t column;
    int have_above;
    int have_left;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    above_left = references->above_left;
    has_above_left = references->has_above_left;
    have_above = above != 0 && above_count != 0U;
    have_left = left != 0 && left_count != 0U;
    if (!have_above && !have_left) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples +
                          column] = 512U;
            }
        }
        return AV2H_OK;
    }
    if (!have_left || !have_above) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples +
                          column] = have_above ?
                    above[column < above_count ? column :
                          above_count - 1U] :
                    left[row < left_count ? row : left_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (above_count < width || left_count < height ||
        has_above_left == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int32_t top = above[column];
            int32_t side = left[row];
            int32_t corner = above_left;
            int32_t base = top + side - corner;
            int32_t side_distance = base > side ?
                base - side : side - base;
            int32_t top_distance = base > top ? base - top : top - base;
            int32_t corner_distance = base > corner ?
                base - corner : corner - base;
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint16_t)(side_distance <= top_distance &&
                    side_distance <= corner_distance ? side :
                    top_distance <= corner_distance ? top : corner);
        }
    }
    return AV2H_OK;

}
