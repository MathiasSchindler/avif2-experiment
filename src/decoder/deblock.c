#include "decoder/filter.h"
#include "decoder/coeff_level.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"

#define AV2H_DEBLOCK_MAX_SAMPLES 8U

static const uint8_t deblock_weight[AV2H_DEBLOCK_MAX_SAMPLES] = {
    85U, 51U, 37U, 28U, 23U, 20U, 17U, 15U
};

static const uint8_t deblock_q_multiplier[AV2H_DEBLOCK_MAX_SAMPLES] = {
    32U, 25U, 19U, 19U, 18U, 18U, 17U, 17U
};

static const uint8_t deblock_first_threshold[5] = {
    45U, 43U, 40U, 35U, 32U
};

Av2hStatus av2h_deblock_quantizer_thresholds(
    const Av2hDeblockCell *cell,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    unsigned int plane,
    unsigned int pass,
    uint16_t *q_threshold,
    uint16_t *side_threshold) {
    int delta = 0;
    int level;
    int q_index;
    int side_index;
    int side_bias;
    unsigned int side_shift;
    uint32_t q_step;
    Av2hQIndex q_step_index;
    unsigned int delta_index = plane == 0U ? pass : plane + 1U;
    if (cell == 0 || sequence == 0 || frame == 0 || q_threshold == 0 ||
        side_threshold == 0 || plane >= AV2H_FRAME_PLANE_COUNT || pass > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->info.bit_depth_idc != 0U &&
        sequence->info.bit_depth_idc != 1U) {
        return AV2H_UNSUPPORTED;
    }
    if (plane == 1U) {
        delta = frame->u_ac_delta_q + sequence->base_uv_ac_delta_q;
    } else if (plane == 2U) {
        delta = frame->v_ac_delta_q + sequence->base_uv_ac_delta_q;
    }
    if (cell->q_index == 0U && delta <= 0) {
        q_index = 0;
    } else {
        q_index = av2h_sequence_clamp_delta_q_index(
            sequence, (int)cell->q_index + delta);
    }
    level = q_index + 8 * frame->deblock_delta[delta_index];
    q_step_index = level < 1 ? (Av2hQIndex)(level == 0 ? 0U : 1U) :
        (Av2hQIndex)av2h_sample_clamp(
            level, 1, (int)av2h_sequence_max_q_index(sequence));
    q_step = sequence->info.bit_depth_idc == 0U ?
        av2h_coeff_dequant_step_10bit(q_step_index, 0) :
        av2h_coeff_dequant_step_8bit(q_step_index, 0);
    *q_threshold = (uint16_t)(((q_step + 4U) >> 3U) >> 6U);
    if (sequence->info.bit_depth_idc == 0U) {
        side_index = av2h_sample_clamp(level - 48, 0, 295);
        side_bias = 4;
        side_shift = 3U;
    } else {
        side_index = av2h_sample_clamp(level, 0, 295);
        side_bias = 16;
        side_shift = 5U;
    }
    *side_threshold = (uint16_t)(
        av2h_sample_clamp(
            av2h_deblock_side_threshold(side_index) + side_bias,
            0, 65535) >> side_shift);
    return AV2H_OK;
}

static int deblock_direction_error(const uint8_t *sample,
                                   const uint8_t *parallel_sample,
                                   ptrdiff_t step,
                                   unsigned int distance,
                                   int negative) {
    ptrdiff_t endpoint = negative ?
        -((ptrdiff_t)distance + 1) * step : (ptrdiff_t)distance * step;
    ptrdiff_t edge = negative ? -step : 0;
    ptrdiff_t adjacent = negative ? -2 * step : step;
    int first = (sample[edge] - sample[endpoint]) -
                (int)distance * (sample[edge] - sample[adjacent]);
    int second = (parallel_sample[edge] - parallel_sample[endpoint]) -
                 (int)distance *
                     (parallel_sample[edge] - parallel_sample[adjacent]);
    return (absolute_value(first) + absolute_value(second) + 1) >> 1;
}

static unsigned int deblock_filter_choice(
    const uint8_t *sample,
    ptrdiff_t step,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold,
    const uint8_t *parallel_sample) {
    int second_derivative[4];
    unsigned int negative_max = negative_length / 2U - 1U;
    unsigned int positive_max = positive_length / 2U - 1U;
    int transition;
    int end_threshold;
    unsigned int distance;
    if (q_threshold == 0U || side_threshold == 0U || positive_max < 1U ||
        positive_max < negative_max) {
        return 0U;
    }
    second_derivative[0] = absolute_value(
        sample[-3 * step] - 2 * sample[-2 * step] + sample[-step]);
    second_derivative[3] = absolute_value(
        sample[0] - 2 * sample[step] + sample[2 * step]);
    second_derivative[0] = (second_derivative[0] + absolute_value(
        parallel_sample[-3 * step] - 2 * parallel_sample[-2 * step] +
        parallel_sample[-step]) + 1) >> 1;
    second_derivative[3] = (second_derivative[3] + absolute_value(
        parallel_sample[0] - 2 * parallel_sample[step] +
        parallel_sample[2 * step]) + 1) >> 1;
    if (second_derivative[0] > side_threshold ||
        second_derivative[3] > side_threshold) {
        return 0U;
    }
    if (positive_max == 1U) return 1U;
    second_derivative[1] = absolute_value(
        sample[-2 * step] - 2 * sample[-step] + sample[0]);
    second_derivative[2] = absolute_value(
        sample[-step] - 2 * sample[0] + sample[step]);
    second_derivative[1] = (second_derivative[1] + absolute_value(
        parallel_sample[-2 * step] - 2 * parallel_sample[-step] +
        parallel_sample[0]) + 1) >> 1;
    second_derivative[2] = (second_derivative[2] + absolute_value(
        parallel_sample[-step] - 2 * parallel_sample[0] +
        parallel_sample[step]) + 1) >> 1;
    if (second_derivative[0] > (side_threshold >> 2U) ||
        second_derivative[3] > (side_threshold >> 2U) ||
        second_derivative[1] + second_derivative[2] >
            (int)q_threshold * 4) {
        return 1U;
    }
    if (second_derivative[0] > (side_threshold >> 3U) ||
        second_derivative[3] > (side_threshold >> 3U) ||
        second_derivative[1] + second_derivative[2] >
            (int)q_threshold * 3) {
        return 2U;
    }
    end_threshold = ((int)side_threshold * 3) >> 4;
    if ((negative_max > 2U &&
            deblock_direction_error(sample, parallel_sample, step, 3U, 1) >
               end_threshold) ||
           deblock_direction_error(sample, parallel_sample, step, 3U, 0) >
              end_threshold) {
        return 2U;
    }
    if (positive_max == 3U) return 3U;
    transition = (second_derivative[1] + second_derivative[2]) << 4U;
    for (distance = 4U; distance <= AV2H_DEBLOCK_MAX_SAMPLES;
         distance += 2U) {
        unsigned int tested_distance = distance == 8U ? 7U : distance;
        end_threshold = ((int)side_threshold * (int)distance) >> 4;
        if (transition >
            (int)q_threshold * deblock_first_threshold[distance - 4U] ||
            (negative_max >= tested_distance &&
             deblock_direction_error(sample, parallel_sample, step,
                                     tested_distance, 1) > end_threshold) ||
            deblock_direction_error(sample, parallel_sample, step,
                                    tested_distance, 0) > end_threshold) {
            return distance == 4U ? 3U : distance - 2U;
        }
        if (positive_max <= tested_distance) {
            return (tested_distance >> 1U) << 1U;
        }
    }
    return AV2H_DEBLOCK_MAX_SAMPLES;
}

static void deblock_filter_lane(uint8_t *sample,
                                ptrdiff_t step,
                                unsigned int negative_width,
                                unsigned int positive_width,
                                uint16_t q_threshold,
                                uint8_t negative_lossless,
                                uint8_t positive_lossless) {
    unsigned int width = negative_width > positive_width ?
                         negative_width : positive_width;
    int delta = (3 * (sample[0] - sample[-step]) -
                 (sample[step] - sample[-2 * step])) * 4;
    int limit = (int)q_threshold * deblock_q_multiplier[width - 1U];
    unsigned int index;
    delta = av2h_sample_clamp(delta, -limit, limit);
    if (negative_lossless == 0U) {
        for (index = 0U; index < negative_width; ++index) {
            int adjustment = round_shift_signed(
                delta * deblock_weight[negative_width - 1U] *
                    (int)(negative_width - index),
                11U);
            sample[-((ptrdiff_t)index + 1) * step] = av2h_sample_clip(
                sample[-((ptrdiff_t)index + 1) * step] + adjustment);
        }
    }
    if (positive_lossless == 0U) {
        for (index = 0U; index < positive_width; ++index) {
            int adjustment = round_shift_signed(
                delta * deblock_weight[positive_width - 1U] *
                    (int)(positive_width - index),
                11U);
            sample[(ptrdiff_t)index * step] = av2h_sample_clip(
                sample[(ptrdiff_t)index * step] - adjustment);
        }
    }
}

static int deblock_direction_error_u16(const uint16_t *sample,
                                       const uint16_t *parallel_sample,
                                       ptrdiff_t step,
                                       unsigned int distance,
                                       int negative) {
    ptrdiff_t endpoint = negative ?
        -((ptrdiff_t)distance + 1) * step : (ptrdiff_t)distance * step;
    ptrdiff_t edge = negative ? -step : 0;
    ptrdiff_t adjacent = negative ? -2 * step : step;
    int first = (sample[edge] - sample[endpoint]) -
                (int)distance * (sample[edge] - sample[adjacent]);
    int second = (parallel_sample[edge] - parallel_sample[endpoint]) -
                 (int)distance *
                     (parallel_sample[edge] - parallel_sample[adjacent]);
    return (absolute_value(first) + absolute_value(second) + 1) >> 1;
}

static unsigned int deblock_filter_choice_u16(
    const uint16_t *sample,
    ptrdiff_t step,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold,
    const uint16_t *parallel_sample) {
    int second_derivative[4];
    unsigned int negative_max = negative_length / 2U - 1U;
    unsigned int positive_max = positive_length / 2U - 1U;
    int transition;
    int end_threshold;
    unsigned int distance;
    if (q_threshold == 0U || side_threshold == 0U || positive_max < 1U ||
        positive_max < negative_max) {
        return 0U;
    }
    second_derivative[0] = absolute_value(
        sample[-3 * step] - 2 * sample[-2 * step] + sample[-step]);
    second_derivative[3] = absolute_value(
        sample[0] - 2 * sample[step] + sample[2 * step]);
    second_derivative[0] = (second_derivative[0] + absolute_value(
        parallel_sample[-3 * step] - 2 * parallel_sample[-2 * step] +
        parallel_sample[-step]) + 1) >> 1;
    second_derivative[3] = (second_derivative[3] + absolute_value(
        parallel_sample[0] - 2 * parallel_sample[step] +
        parallel_sample[2 * step]) + 1) >> 1;
    if (second_derivative[0] > side_threshold ||
        second_derivative[3] > side_threshold) {
        return 0U;
    }
    if (positive_max == 1U) return 1U;
    second_derivative[1] = absolute_value(
        sample[-2 * step] - 2 * sample[-step] + sample[0]);
    second_derivative[2] = absolute_value(
        sample[-step] - 2 * sample[0] + sample[step]);
    second_derivative[1] = (second_derivative[1] + absolute_value(
        parallel_sample[-2 * step] - 2 * parallel_sample[-step] +
        parallel_sample[0]) + 1) >> 1;
    second_derivative[2] = (second_derivative[2] + absolute_value(
        parallel_sample[-step] - 2 * parallel_sample[0] +
        parallel_sample[step]) + 1) >> 1;
    if (second_derivative[0] > (side_threshold >> 2U) ||
        second_derivative[3] > (side_threshold >> 2U) ||
        second_derivative[1] + second_derivative[2] >
            (int)q_threshold * 4) {
        return 1U;
    }
    if (second_derivative[0] > (side_threshold >> 3U) ||
        second_derivative[3] > (side_threshold >> 3U) ||
        second_derivative[1] + second_derivative[2] >
            (int)q_threshold * 3) {
        return 2U;
    }
    end_threshold = ((int)side_threshold * 3) >> 4;
    if ((negative_max > 2U &&
         deblock_direction_error_u16(
             sample, parallel_sample, step, 3U, 1) > end_threshold) ||
        deblock_direction_error_u16(
            sample, parallel_sample, step, 3U, 0) > end_threshold) {
        return 2U;
    }
    if (positive_max == 3U) return 3U;
    transition = (second_derivative[1] + second_derivative[2]) << 4U;
    for (distance = 4U; distance <= AV2H_DEBLOCK_MAX_SAMPLES;
         distance += 2U) {
        unsigned int tested_distance = distance == 8U ? 7U : distance;
        end_threshold = ((int)side_threshold * (int)distance) >> 4;
        if (transition >
            (int)q_threshold * deblock_first_threshold[distance - 4U] ||
            (negative_max >= tested_distance &&
             deblock_direction_error_u16(
                 sample, parallel_sample, step,
                 tested_distance, 1) > end_threshold) ||
            deblock_direction_error_u16(
                sample, parallel_sample, step,
                tested_distance, 0) > end_threshold) {
            return distance == 4U ? 3U : distance - 2U;
        }
        if (positive_max <= tested_distance) {
            return (tested_distance >> 1U) << 1U;
        }
    }
    return AV2H_DEBLOCK_MAX_SAMPLES;
}

static void deblock_filter_lane_u16(uint16_t *sample,
                                    ptrdiff_t step,
                                    unsigned int negative_width,
                                    unsigned int positive_width,
                                    uint16_t q_threshold,
                                    uint8_t negative_lossless,
                                    uint8_t positive_lossless) {
    unsigned int width = negative_width > positive_width ?
                         negative_width : positive_width;
    int delta = (3 * (sample[0] - sample[-step]) -
                 (sample[step] - sample[-2 * step])) * 4;
    int limit = (int)q_threshold * deblock_q_multiplier[width - 1U];
    unsigned int index;
    delta = av2h_sample_clamp(delta, -limit, limit);
    if (negative_lossless == 0U) {
        for (index = 0U; index < negative_width; ++index) {
            int adjustment = round_shift_signed(
                delta * deblock_weight[negative_width - 1U] *
                    (int)(negative_width - index),
                11U);
            sample[-((ptrdiff_t)index + 1) * step] = av2h_sample_clip_10bit(
                sample[-((ptrdiff_t)index + 1) * step] + adjustment);
        }
    }
    if (positive_lossless == 0U) {
        for (index = 0U; index < positive_width; ++index) {
            int adjustment = round_shift_signed(
                delta * deblock_weight[positive_width - 1U] *
                    (int)(positive_width - index),
                11U);
            sample[(ptrdiff_t)index * step] = av2h_sample_clip_10bit(
                sample[(ptrdiff_t)index * step] - adjustment);
        }
    }
}

static Av2hStatus deblock_edge(
    Av2hFilterPlane *plane,
    uint16_t x,
    uint16_t y,
    uint8_t vertical,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold,
    uint8_t negative_lossless,
    uint8_t positive_lossless) {
    unsigned int negative_max;
    unsigned int positive_max;
    unsigned int negative_required;
    unsigned int positive_required;
    ptrdiff_t step;
    ptrdiff_t lane_step;
    uint8_t *sample;
    unsigned int filter;
    unsigned int lane;
    if (plane == 0 || plane->data == 0 || plane->stride_samples < plane->width ||
        vertical > 1U || negative_length < 4U ||
        (negative_length & 1U) != 0U || positive_length < negative_length ||
        positive_length > 18U || (positive_length & 1U) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    negative_max = negative_length / 2U - 1U;
    positive_max = positive_length / 2U - 1U;
    negative_required = negative_max + 1U > 3U ? negative_max + 1U : 3U;
    positive_required = positive_max > 3U ? positive_max : 3U;
    if ((vertical != 0U &&
         ((unsigned int)x < negative_required ||
          (unsigned int)plane->width - x <= positive_required ||
          (unsigned int)y + 4U > plane->height)) ||
        (vertical == 0U &&
         ((unsigned int)y < negative_required ||
          (unsigned int)plane->height - y <= positive_required ||
          (unsigned int)x + 4U > plane->width))) {
        return AV2H_INVALID_ARGUMENT;
    }
    step = vertical != 0U ? 1 : (ptrdiff_t)plane->stride_samples;
    lane_step = vertical != 0U ? (ptrdiff_t)plane->stride_samples : 1;
    sample = plane->data + (size_t)y * plane->stride_samples + x;
    filter = deblock_filter_choice(
        sample, step, negative_length, positive_length,
        q_threshold, side_threshold, sample + 3 * lane_step);
    if (filter == 0U) return AV2H_OK;
    if (negative_max > filter) negative_max = filter;
    for (lane = 0U; lane < 4U; ++lane) {
        deblock_filter_lane(sample + (ptrdiff_t)lane * lane_step, step,
                            negative_max, filter, q_threshold,
                            negative_lossless, positive_lossless);
    }
    return AV2H_OK;
}

Av2hStatus av2h_deblock_edge(
    Av2hFilterPlane *plane,
    uint16_t x,
    uint16_t y,
    uint8_t vertical,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold) {
    return deblock_edge(
        plane, x, y, vertical, negative_length, positive_length,
        q_threshold, side_threshold, 0U, 0U);
}

static Av2hStatus deblock_edge_u16(
    Av2hFilterPlaneU16 *plane,
    uint16_t x,
    uint16_t y,
    uint8_t vertical,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold,
    uint8_t negative_lossless,
    uint8_t positive_lossless) {
    unsigned int negative_max;
    unsigned int positive_max;
    unsigned int negative_required;
    unsigned int positive_required;
    ptrdiff_t step;
    ptrdiff_t lane_step;
    uint16_t *sample;
    unsigned int filter;
    unsigned int lane;
    if (plane == 0 || plane->data == 0 || plane->stride_samples < plane->width ||
        vertical > 1U || negative_length < 4U ||
        (negative_length & 1U) != 0U || positive_length < negative_length ||
        positive_length > 18U || (positive_length & 1U) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    negative_max = negative_length / 2U - 1U;
    positive_max = positive_length / 2U - 1U;
    negative_required = negative_max + 1U > 3U ? negative_max + 1U : 3U;
    positive_required = positive_max > 3U ? positive_max : 3U;
    if ((vertical != 0U &&
         ((unsigned int)x < negative_required ||
          (unsigned int)plane->width - x <= positive_required ||
          (unsigned int)y + 4U > plane->height)) ||
        (vertical == 0U &&
         ((unsigned int)y < negative_required ||
          (unsigned int)plane->height - y <= positive_required ||
          (unsigned int)x + 4U > plane->width))) {
        return AV2H_INVALID_ARGUMENT;
    }
    step = vertical != 0U ? 1 : (ptrdiff_t)plane->stride_samples;
    lane_step = vertical != 0U ? (ptrdiff_t)plane->stride_samples : 1;
    sample = plane->data + (size_t)y * plane->stride_samples + x;
    filter = deblock_filter_choice_u16(
        sample, step, negative_length, positive_length,
        q_threshold, side_threshold, sample + 3 * lane_step);
    if (filter == 0U) return AV2H_OK;
    if (negative_max > filter) negative_max = filter;
    for (lane = 0U; lane < 4U; ++lane) {
        deblock_filter_lane_u16(
            sample + (ptrdiff_t)lane * lane_step, step,
            negative_max, filter, q_threshold,
            negative_lossless, positive_lossless);
    }
    return AV2H_OK;
}

Av2hStatus av2h_deblock_edge_u16(
    Av2hFilterPlaneU16 *plane,
    uint16_t x,
    uint16_t y,
    uint8_t vertical,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold) {
    return deblock_edge_u16(
        plane, x, y, vertical, negative_length, positive_length,
        q_threshold, side_threshold, 0U, 0U);
}

static int deblock_transform_dimension_valid(uint8_t value) {
    return value == 4U || value == 8U || value == 16U || value == 32U ||
           value == 64U;
}

int av2h_deblock_map_valid(const Av2hFilterPlanes *planes,
                             const Av2hDeblockMap *map) {
    unsigned int plane;
    if (map == 0) return 0;
    for (plane = 0U;
         plane < AV2H_FRAME_PLANE_COUNT && planes->plane[plane].data != 0;
         ++plane) {
        size_t grid_size = plane == 0U ? 4U : 2U;
        size_t columns = planes->plane[plane].width / grid_size;
        size_t rows = planes->plane[plane].height / grid_size;
        size_t row;
        if (plane != 0U && map->columns[plane] == columns / 2U &&
            map->rows[plane] == rows / 2U) {
            columns /= 2U;
            rows /= 2U;
        }
        if (map->plane[plane] == 0 || map->columns[plane] != columns ||
            map->rows[plane] != rows || map->stride_cells[plane] < columns) {
            return 0;
        }
        for (row = 0U; row < rows; ++row) {
            size_t column;
            for (column = 0U; column < columns; ++column) {
                const Av2hDeblockCell *cell =
                    &map->plane[plane][row * map->stride_cells[plane] + column];
                if (!deblock_transform_dimension_valid(
                        cell->transform_width) ||
                    !deblock_transform_dimension_valid(
                        cell->transform_height) ||
                    cell->block_column >= columns || cell->block_row >= rows ||
                    cell->transform_column >= columns ||
                    cell->transform_row >= rows || cell->skip > 1U ||
                    cell->lossless > 1U) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

int av2h_deblock_map_valid_u16(const Av2hFilterPlanesU16 *planes,
                               const Av2hDeblockMap *map) {
    unsigned int plane;
    if (map == 0) return 0;
    for (plane = 0U;
         plane < AV2H_FRAME_PLANE_COUNT && planes->plane[plane].data != 0;
         ++plane) {
        size_t grid_size = plane == 0U ? 4U : 2U;
        size_t columns = planes->plane[plane].width / grid_size;
        size_t rows = planes->plane[plane].height / grid_size;
        size_t row;
        if (plane != 0U && map->columns[plane] == columns / 2U &&
            map->rows[plane] == rows / 2U) {
            columns /= 2U;
            rows /= 2U;
        }
        if (map->plane[plane] == 0 || map->columns[plane] != columns ||
            map->rows[plane] != rows || map->stride_cells[plane] < columns) {
            return 0;
        }
        for (row = 0U; row < rows; ++row) {
            size_t column;
            for (column = 0U; column < columns; ++column) {
                const Av2hDeblockCell *cell =
                    &map->plane[plane][row * map->stride_cells[plane] + column];
                if (!deblock_transform_dimension_valid(
                        cell->transform_width) ||
                    !deblock_transform_dimension_valid(
                        cell->transform_height) ||
                    cell->block_column >= columns || cell->block_row >= rows ||
                    cell->transform_column >= columns ||
                    cell->transform_row >= rows || cell->skip > 1U ||
                    cell->lossless > 1U) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

static Av2hStatus deblock_frame_edge(
    Av2hFilterPlane *plane_data,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hDeblockMap *map,
    unsigned int plane,
    unsigned int pass,
    size_t row,
    size_t column) {
    const Av2hDeblockCell *cell =
        &map->plane[plane][row * map->stride_cells[plane] + column];
    size_t previous_row = row - (pass != 0U ? 1U : 0U);
    size_t previous_column = column - (pass == 0U ? 1U : 0U);
    const Av2hDeblockCell *previous = &map->plane[plane][
        previous_row * map->stride_cells[plane] + previous_column];
    unsigned int transform_edge = pass == 0U ?
        cell->transform_column != previous->transform_column :
        cell->transform_row != previous->transform_row;
    unsigned int block_edge = pass == 0U ?
        cell->block_column == column : cell->block_row == row;
    unsigned int filter_size;
    unsigned int maximum_positive;
    unsigned int maximum_negative;
    unsigned int superblock_edge;
    unsigned int tile_edge = 0U;
    uint16_t current_q;
    uint16_t previous_q;
    uint16_t current_side;
    uint16_t previous_side;
    uint16_t q_threshold;
    uint16_t side_threshold;
    size_t grid_size = plane_data->width / map->columns[plane];
    size_t x = column * grid_size;
    size_t y = row * grid_size;
    if (pass == 0U) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t tile_index;
        for (tile_index = 1U;
             tile_index < frame->tile_column_count;
             ++tile_index) {
            size_t boundary =
                ((size_t)frame->tile_column_start_sb[tile_index] *
                 sequence->superblock_size) >> shift;
            if (x == boundary) {
                tile_edge = 1U;
                break;
            }
        }
    }
    if (sequence->disable_loopfilters_across_tiles != 0U && tile_edge != 0U) {
        return AV2H_OK;
    }
    if (sequence->disable_loopfilters_across_tiles != 0U && pass != 0U) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t tile_index;
        for (tile_index = 1U; tile_index < frame->tile_row_count;
             ++tile_index) {
            size_t boundary =
                ((size_t)frame->tile_row_start_sb[tile_index] *
                 sequence->superblock_size) >> shift;
            if (y == boundary) return AV2H_OK;
        }
    }
    if (transform_edge == 0U || (block_edge == 0U && cell->skip != 0U)) {
        return AV2H_OK;
    }
    if ((pass == 0U &&
         ((y & 3U) != 0U || x < 3U || plane_data->width - x <= 3U ||
          y + 4U > plane_data->height)) ||
        (pass != 0U &&
         ((x & 3U) != 0U || y < 3U || plane_data->height - y <= 3U ||
          x + 4U > plane_data->width))) {
        return AV2H_OK;
    }
    if (av2h_deblock_quantizer_thresholds(
            cell, sequence, frame, plane, pass,
            &current_q, &current_side) != AV2H_OK ||
        av2h_deblock_quantizer_thresholds(
            previous, sequence, frame, plane, pass,
            &previous_q, &previous_side) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    q_threshold = current_q != 0U && previous_q != 0U ?
        (uint16_t)((current_q + previous_q + 1U) >> 1U) :
        (current_q > previous_q ? current_q : previous_q);
    side_threshold = current_side != 0U && previous_side != 0U ?
        (uint16_t)((current_side + previous_side + 1U) >> 1U) :
        (current_side > previous_side ? current_side : previous_side);
    filter_size = pass == 0U ?
        (cell->transform_width < previous->transform_width ?
         cell->transform_width : previous->transform_width) :
        (cell->transform_height < previous->transform_height ?
         cell->transform_height : previous->transform_height);
    if (plane == 0U) {
        if ((pass == 0U && column * grid_size + 16U > plane_data->width) ||
            (pass != 0U && row * grid_size + 16U > plane_data->height)) {
            if (filter_size > 16U) filter_size = 16U;
        }
    } else if ((pass == 0U && column * grid_size + 8U > plane_data->width) ||
               (pass != 0U && row * grid_size + 8U > plane_data->height)) {
        if (filter_size > 8U) filter_size = 8U;
    }
    if (filter_size <= 4U) {
        maximum_positive = 1U;
    } else if (filter_size == 8U) {
        maximum_positive = 3U;
    } else if (filter_size == 16U) {
        maximum_positive = plane == 0U ? 6U : 4U;
    } else {
        maximum_positive = plane == 0U ? 8U : 4U;
    }
    superblock_edge = tile_edge != 0U ||
        (pass != 0U &&
         (row * grid_size %
          (plane == 0U || frame->chroma_format_idc == 2U ? 64U : 32U)) == 0U);
    maximum_negative = maximum_positive;
    if (superblock_edge != 0U) {
        unsigned int limit = plane == 0U ? 6U : 2U;
        if (maximum_negative > limit) maximum_negative = limit;
    }
    return deblock_edge(
        plane_data, (uint16_t)x, (uint16_t)y,
        (uint8_t)(pass == 0U),
        (uint8_t)((maximum_negative + 1U) * 2U),
        (uint8_t)((maximum_positive + 1U) * 2U),
        q_threshold, side_threshold, previous->lossless, cell->lossless);
}

static Av2hStatus deblock_frame_edge_u16(
    Av2hFilterPlaneU16 *plane_data,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hDeblockMap *map,
    unsigned int plane,
    unsigned int pass,
    size_t row,
    size_t column) {
    const Av2hDeblockCell *cell =
        &map->plane[plane][row * map->stride_cells[plane] + column];
    size_t previous_row = row - (pass != 0U ? 1U : 0U);
    size_t previous_column = column - (pass == 0U ? 1U : 0U);
    const Av2hDeblockCell *previous = &map->plane[plane][
        previous_row * map->stride_cells[plane] + previous_column];
    unsigned int transform_edge = pass == 0U ?
        cell->transform_column != previous->transform_column :
        cell->transform_row != previous->transform_row;
    unsigned int block_edge = pass == 0U ?
        cell->block_column == column : cell->block_row == row;
    unsigned int filter_size;
    unsigned int maximum_positive;
    unsigned int maximum_negative;
    unsigned int superblock_edge;
    unsigned int tile_edge = 0U;
    uint16_t current_q;
    uint16_t previous_q;
    uint16_t current_side;
    uint16_t previous_side;
    uint16_t q_threshold;
    uint16_t side_threshold;
    size_t grid_size = plane_data->width / map->columns[plane];
    size_t x = column * grid_size;
    size_t y = row * grid_size;
    if (pass == 0U) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t tile_index;
        for (tile_index = 1U;
             tile_index < frame->tile_column_count;
             ++tile_index) {
            size_t boundary =
                ((size_t)frame->tile_column_start_sb[tile_index] *
                 sequence->superblock_size) >> shift;
            if (x == boundary) {
                tile_edge = 1U;
                break;
            }
        }
    }
    if (sequence->disable_loopfilters_across_tiles != 0U && tile_edge != 0U) {
        return AV2H_OK;
    }
    if (sequence->disable_loopfilters_across_tiles != 0U && pass != 0U) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t tile_index;
        for (tile_index = 1U; tile_index < frame->tile_row_count;
             ++tile_index) {
            size_t boundary =
                ((size_t)frame->tile_row_start_sb[tile_index] *
                 sequence->superblock_size) >> shift;
            if (y == boundary) return AV2H_OK;
        }
    }
    if (transform_edge == 0U || (block_edge == 0U && cell->skip != 0U)) {
        return AV2H_OK;
    }
    if ((pass == 0U &&
         ((y & 3U) != 0U || x < 3U || plane_data->width - x <= 3U ||
          y + 4U > plane_data->height)) ||
        (pass != 0U &&
         ((x & 3U) != 0U || y < 3U || plane_data->height - y <= 3U ||
          x + 4U > plane_data->width))) {
        return AV2H_OK;
    }
    if (av2h_deblock_quantizer_thresholds(
            cell, sequence, frame, plane, pass,
            &current_q, &current_side) != AV2H_OK ||
        av2h_deblock_quantizer_thresholds(
            previous, sequence, frame, plane, pass,
            &previous_q, &previous_side) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    q_threshold = current_q != 0U && previous_q != 0U ?
        (uint16_t)((current_q + previous_q + 1U) >> 1U) :
        (current_q > previous_q ? current_q : previous_q);
    side_threshold = current_side != 0U && previous_side != 0U ?
        (uint16_t)((current_side + previous_side + 1U) >> 1U) :
        (current_side > previous_side ? current_side : previous_side);
    filter_size = pass == 0U ?
        (cell->transform_width < previous->transform_width ?
         cell->transform_width : previous->transform_width) :
        (cell->transform_height < previous->transform_height ?
         cell->transform_height : previous->transform_height);
    if (plane == 0U) {
        if ((pass == 0U && column * grid_size + 16U > plane_data->width) ||
            (pass != 0U && row * grid_size + 16U > plane_data->height)) {
            if (filter_size > 16U) filter_size = 16U;
        }
    } else if ((pass == 0U && column * grid_size + 8U > plane_data->width) ||
               (pass != 0U && row * grid_size + 8U > plane_data->height)) {
        if (filter_size > 8U) filter_size = 8U;
    }
    if (filter_size <= 4U) {
        maximum_positive = 1U;
    } else if (filter_size == 8U) {
        maximum_positive = 3U;
    } else if (filter_size == 16U) {
        maximum_positive = plane == 0U ? 6U : 4U;
    } else {
        maximum_positive = plane == 0U ? 8U : 4U;
    }
    superblock_edge = tile_edge != 0U ||
        (pass != 0U &&
         (row * grid_size %
          (plane == 0U || frame->chroma_format_idc == 2U ? 64U : 32U)) == 0U);
    maximum_negative = maximum_positive;
    if (superblock_edge != 0U) {
        unsigned int limit = plane == 0U ? 6U : 2U;
        if (maximum_negative > limit) maximum_negative = limit;
    }
    return deblock_edge_u16(
        plane_data, (uint16_t)x, (uint16_t)y,
        (uint8_t)(pass == 0U),
        (uint8_t)((maximum_negative + 1U) * 2U),
        (uint8_t)((maximum_positive + 1U) * 2U),
        q_threshold, side_threshold, previous->lossless, cell->lossless);
}

Av2hStatus av2h_deblock_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hDeblockMap *map) {
    unsigned int plane;
    if (planes == 0 || sequence == 0 || frame == 0 ||
        !filter_dimensions_valid(planes, frame) ||
        !av2h_deblock_map_valid(planes, map)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int pass;
        for (pass = 0U; pass < 2U; ++pass) {
            unsigned int enable_index = plane == 0U ? pass : plane + 1U;
            size_t row;
            if (frame->deblock_enabled[enable_index] == 0U) continue;
            for (row = 0U; row < map->rows[plane]; ++row) {
                size_t column;
                for (column = 0U; column < map->columns[plane]; ++column) {
                    Av2hStatus status;
                    if ((pass == 0U && column == 0U) ||
                        (pass != 0U && row == 0U)) {
                        continue;
                    }
                    status = deblock_frame_edge(
                        &planes->plane[plane], sequence, frame, map,
                        plane, pass, row, column);
                    if (status != AV2H_OK) return status;
                }
            }
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_deblock_filter_frame_u16(
    Av2hFilterPlanesU16 *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hDeblockMap *map) {
    unsigned int plane;
    if (planes == 0 || sequence == 0 || frame == 0 ||
        !filter_dimensions_valid_u16(planes, frame) ||
        !av2h_deblock_map_valid_u16(planes, map)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int pass;
        for (pass = 0U; pass < 2U; ++pass) {
            unsigned int enable_index = plane == 0U ? pass : plane + 1U;
            size_t row;
            if (frame->deblock_enabled[enable_index] == 0U) continue;
            for (row = 0U; row < map->rows[plane]; ++row) {
                size_t column;
                for (column = 0U; column < map->columns[plane]; ++column) {
                    Av2hStatus status;
                    if ((pass == 0U && column == 0U) ||
                        (pass != 0U && row == 0U)) {
                        continue;
                    }
                    status = deblock_frame_edge_u16(
                        &planes->plane[plane], sequence, frame, map,
                        plane, pass, row, column);
                    if (status != AV2H_OK) return status;
                }
            }
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_deblock_stage_u16(
    const Av2hFilterPlanesU16 *reconstructed,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFilterWorkspace *workspace) {
    unsigned int plane;
    if (reconstructed == 0 || sequence == 0 || frame == 0 || workspace == 0 ||
        sequence->info.bit_depth_idc != 0U ||
        !filter_dimensions_valid_u16(reconstructed, frame) ||
        !filter_dimensions_valid_u16(&workspace->deblocked_u16, frame) ||
        !av2h_deblock_map_valid_u16(
            &workspace->deblocked_u16, &workspace->deblock_map)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (reconstructed->plane[plane].data ==
            workspace->deblocked_u16.plane[plane].data) {
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlaneU16 *source = &reconstructed->plane[plane];
        Av2hFilterPlaneU16 *destination =
            &workspace->deblocked_u16.plane[plane];
        av2h_plane_copy_u16(
            destination->data, destination->stride_samples,
            source->data, source->stride_samples,
            source->width, source->height);
    }
    return av2h_deblock_filter_frame_u16(
        &workspace->deblocked_u16, sequence, frame,
        &workspace->deblock_map);
}

