#include "internal/intra_prediction.h"

#include "internal/sample_math.h"

#include <stddef.h>

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
        (unsigned int)av2h_sample_block_log2(width) - 2U +
        (unsigned int)av2h_sample_block_log2(height) - 2U;
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
    const Av2hLeafMode *mode,
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
    if (references->above != NULL && references->above_count >= width) {
        for (index = 0U; index < width; ++index) {
            sum += references->above[index];
        }
        count += width;
    }
    if (references->left != NULL && references->left_count >= height) {
        for (index = 0U; index < height; ++index) {
            sum += references->left[index];
        }
        count += height;
    }
    value = dc_predictor_average(
        sum, count, width, height,
        (uint8_t)(references->above != NULL &&
                  references->above_count >= width),
        (uint8_t)(references->left != NULL &&
                  references->left_count >= height));
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] = value;
        }
    }
    if (mode->enable_ibp && !(width == 4U && height == 4U)) {
        if (references->above != NULL &&
            references->above_count >= width) {
            uint16_t row_limit = (uint16_t)(height >> 2U);
            uint16_t column_start =
                width < height && references->left != NULL &&
                    references->left_count >= height ?
                    (uint16_t)(width >> 2U) : 0U;
            uint8_t weight_row = (uint8_t)(av2h_sample_block_log2(height) - 2U);
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
        if (references->left != NULL &&
            references->left_count >= height) {
            uint16_t row_start =
                width >= height && references->above != NULL &&
                    references->above_count >= width ?
                    (uint16_t)(height >> 2U) : 0U;
            uint16_t column_limit = (uint16_t)(width >> 2U);
            uint8_t weight_row = (uint8_t)(av2h_sample_block_log2(width) - 2U);
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
    int have_above = references->above != NULL &&
        references->above_count != 0U;
    int have_left = references->left != NULL &&
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
            int32_t value = bottom_left + av2h_sample_round_pow2(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                av2h_sample_block_log2(height));
            value += av2h_sample_round_pow2(
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
    int have_above = references->above != NULL &&
        references->above_count != 0U;
    int have_left = references->left != NULL &&
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
            int32_t value = top_right + av2h_sample_round_pow2(
                ((int64_t)left - top_right) *
                    (width - 1U - column),
                av2h_sample_block_log2(width));
            value += av2h_sample_round_pow2(
                ((int64_t)left - value) * blend, 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)value;
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
    if (references->above == NULL || references->above_count < width ||
        references->left == NULL || references->left_count < height) {
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
            int32_t vertical = bottom_left + av2h_sample_round_pow2(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                av2h_sample_block_log2(height));
            int32_t horizontal = top_right + av2h_sample_round_pow2(
                ((int64_t)left - top_right) *
                    (width - 1U - column),
                av2h_sample_block_log2(width));
            vertical += av2h_sample_round_pow2(
                ((int64_t)top - vertical) *
                    smooth_blend(row, scale), 7U);
            horizontal += av2h_sample_round_pow2(
                ((int64_t)left - horizontal) *
                    smooth_blend(column, scale), 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)av2h_sample_round_pow2(vertical + horizontal, 1U);
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
    if (references->left == NULL || references->left_count < height) {
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
            int32_t vertical = bottom_left + av2h_sample_round_pow2(
                ((int64_t)top - bottom_left) *
                    (height - 1U - row),
                5U);
            int32_t horizontal = top_right + av2h_sample_round_pow2(
                ((int64_t)references->left[row] - top_right) *
                    (width - 1U - column),
                3U);
            vertical += av2h_sample_round_pow2(
                ((int64_t)top - vertical) * top_blend, 6U);
            horizontal += av2h_sample_round_pow2(
                ((int64_t)references->left[row] - horizontal) *
                    smooth8_blend[column],
                6U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)av2h_sample_round_pow2(vertical + horizontal, 1U);
        }
    }
    return AV2H_OK;

}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/intra_prediction_basic_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/intra_prediction_basic_kernel.inc"
#include "internal/sample_kernel_end.inc"
