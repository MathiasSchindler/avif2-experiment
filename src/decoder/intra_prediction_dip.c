#include "internal/intra_prediction.h"

#include "internal/sample_math.h"

#include <stddef.h>

#include "intra_dip_data.inc"

static void intra_dip_matrix_predict(const uint16_t input[11U],
                                     unsigned int mode,
                                     uint16_t output[64U]) {
    unsigned int position;
    for (position = 0U; position < 64U; ++position) {
        int64_t sum = 0;
        unsigned int feature;
        size_t base = ((size_t)mode * 64U + position) * 11U;
        for (feature = 0U; feature < 11U; ++feature) {
            sum += (int64_t)av2h_intra_dip_weights[base + feature] *
                   input[feature];
        }
        output[position] = (uint16_t)av2h_sample_clamp(
            av2h_sample_round_pow2(sum, 10U), 0, 255);
    }
}

static uint8_t intra_dip_edge_sample(const uint8_t *edge,
                                     size_t count,
                                     size_t index,
                                     uint8_t fallback) {
    if (edge == NULL || count == 0U) return fallback;
    return edge[index < count ? index : count - 1U];
}

Av2hStatus av2h_predict_intra_dip(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t input[11U];
    uint16_t output[64U];
    const uint8_t *edge[2U];
    size_t edge_count[2U];
    unsigned int width_log2;
    unsigned int height_log2;
    unsigned int transpose;
    unsigned int mode_index;
    unsigned int down_log2[2U];
    unsigned int feature = 0U;
    unsigned int side;
    uint8_t fallback;
    uint8_t side_fallback[2U];
    uint8_t above_fallback;
    uint8_t left_fallback;
    unsigned int up_x_log2;
    unsigned int up_y_log2;
    unsigned int down_x_log2 = 0U;
    unsigned int down_y_log2 = 0U;
    unsigned int multiplier_x;
    unsigned int multiplier_y;
    unsigned int divisor_x;
    unsigned int divisor_y;
    unsigned int row;
    unsigned int column;
    if (mode == NULL || references == NULL || predictor == NULL ||
        predictor_stride_samples < width ||
        !av2h_luma_prediction_dimension_supported(width) ||
        !av2h_luma_prediction_dimension_supported(height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    transpose = mode->intra_dip_mode >> 4U;
    mode_index = mode->intra_dip_mode & 15U;
    if (transpose > 1U || mode_index >= AV2H_INTRA_DIP_MODES) {
        return AV2H_INVALID_DATA;
    }
    width_log2 = av2h_sample_block_log2(width);
    height_log2 = av2h_sample_block_log2(height);
    down_log2[0] = (transpose != 0U ? height_log2 : width_log2) - 2U;
    down_log2[1] = (transpose != 0U ? width_log2 : height_log2) - 2U;
    edge[0] = transpose != 0U ? references->left : references->above;
    edge[1] = transpose != 0U ? references->above : references->left;
    edge_count[0] = transpose != 0U ?
        references->left_count : references->above_count;
    edge_count[1] = transpose != 0U ?
        references->above_count : references->left_count;
    fallback = references->has_above_left ? references->above_left :
        references->above_count != 0U ? references->above[0] :
        references->left_count != 0U ? references->left[0] :
        av2h_sample_midpoint_8bit();
    /* The above-left corner sample (feature 0) and the per-edge samples
     * (features 1..10) use different fallbacks when a neighbor is fully
     * unavailable. Per AV2's reference-sample construction (matching
     * predict_directional's angle==90/180 fallback rule elsewhere in this
     * file), an absent above row defaults to the below-midpoint value and
     * an absent left column defaults to the above-midpoint value for the
     * edge samples themselves; only the corner uses the plain bit-depth
     * midpoint when neither neighbor is available at all (e.g. the very
     * first block of a frame). */
    {
        above_fallback = references->above_count != 0U ?
            references->above[0] : references->left_count != 0U ?
                references->left[0] : av2h_sample_below_midpoint_8bit();
        left_fallback = references->left_count != 0U ?
            references->left[0] : references->above_count != 0U ?
                references->above[0] : av2h_sample_above_midpoint_8bit();
        side_fallback[0] = transpose != 0U ? left_fallback : above_fallback;
        side_fallback[1] = transpose != 0U ? above_fallback : left_fallback;
    }
    input[feature++] = references->has_above_left ?
        references->above_left : fallback;
    for (side = 0U; side < 2U; ++side) {
        unsigned int group;
        size_t edge_index = 0U;
        unsigned int down = 1U << down_log2[side];
        unsigned int rounding = down >> 1U;
        for (group = 0U; group < 4U; ++group) {
            unsigned int sample;
            unsigned int sum = 0U;
            for (sample = 0U; sample < down; ++sample) {
                sum += intra_dip_edge_sample(
                    edge[side], edge_count[side], edge_index++,
                    side_fallback[side]);
            }
            input[feature++] = (uint16_t)(
                (sum + rounding) >> down_log2[side]);
        }
    }
    for (side = 0U; side < 2U; ++side) {
        size_t edge_index = (size_t)4U << down_log2[side];
        unsigned int down = 1U << down_log2[side];
        unsigned int sample;
        unsigned int sum = 0U;
        for (sample = 0U; sample < down; ++sample) {
            sum += intra_dip_edge_sample(
                edge[side], edge_count[side], edge_index++,
                side_fallback[side]);
        }
        input[feature++] = (uint16_t)(
            (sum + (down >> 1U)) >> down_log2[side]);
    }
    intra_dip_matrix_predict(input, mode_index, output);
    up_x_log2 = width_log2 > 3U ? width_log2 - 3U : 0U;
    up_y_log2 = height_log2 > 3U ? height_log2 - 3U : 0U;
    if (width_log2 < 3U) down_x_log2 = 3U - width_log2;
    if (height_log2 < 3U) down_y_log2 = 3U - height_log2;
    multiplier_x = 1U << up_x_log2;
    multiplier_y = 1U << up_y_log2;
    divisor_x = 1U << down_x_log2;
    divisor_y = 1U << down_y_log2;
    for (row = 0U; row < 8U / divisor_y; ++row) {
        for (column = 0U; column < 8U / divisor_x; ++column) {
            unsigned int source_row = row * divisor_y;
            unsigned int source_column = column * divisor_x;
            unsigned int output_row = row * multiplier_y + multiplier_y - 1U;
            unsigned int output_column =
                column * multiplier_x + multiplier_x - 1U;
            unsigned int matrix_row = transpose != 0U ?
                source_column : source_row;
            unsigned int matrix_column = transpose != 0U ?
                source_row : source_column;
            predictor[(size_t)output_row * predictor_stride_samples +
                      output_column] =
                (uint8_t)output[matrix_row * 8U + matrix_column];
        }
    }
    for (row = 0U; row < 8U / divisor_y; ++row) {
        unsigned int output_row = row * multiplier_y + multiplier_y - 1U;
        unsigned int previous = intra_dip_edge_sample(
            references->left, references->left_count, output_row,
            left_fallback);
        for (column = 0U; column < 8U / divisor_x; ++column) {
            unsigned int output_column = column * multiplier_x;
            unsigned int next = predictor[
                (size_t)output_row * predictor_stride_samples +
                output_column + multiplier_x - 1U];
            unsigned int offset;
            for (offset = 0U; offset + 1U < multiplier_x; ++offset) {
                unsigned int position = offset + 1U;
                predictor[(size_t)output_row * predictor_stride_samples +
                          output_column + offset] = (uint8_t)(
                    (previous * (multiplier_x - position) +
                     next * position) >> up_x_log2);
            }
            previous = next;
        }
    }
    for (column = 0U; column < width; ++column) {
        unsigned int previous = intra_dip_edge_sample(
            references->above, references->above_count, column,
            above_fallback);
        for (row = 0U; row < 8U / divisor_y; ++row) {
            unsigned int output_row = row * multiplier_y;
            unsigned int next = predictor[
                (size_t)(output_row + multiplier_y - 1U) *
                    predictor_stride_samples + column];
            unsigned int offset;
            for (offset = 0U; offset + 1U < multiplier_y; ++offset) {
                unsigned int position = offset + 1U;
                predictor[(size_t)(output_row + offset) *
                          predictor_stride_samples + column] = (uint8_t)(
                    (previous * (multiplier_y - position) +
                     next * position) >> up_y_log2);
            }
            previous = next;
        }
    }
    return AV2H_OK;
}
