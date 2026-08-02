#include "decoder/filter.h"

#include "gdf_data.inc"

uint8_t av2h_debug_filter_reason;
uint64_t av2h_debug_filter_stage_hash[
    AV2H_FILTER_STAGE_COUNT][AV2H_FRAME_PLANE_COUNT];
Av2hFilterPlanes av2h_debug_filter_stage_planes[4U];

static uint64_t filter_plane_hash(const Av2hFilterPlane *plane) {
    uint64_t hash = 1469598103934665603ULL;
    uint16_t y;
    for (y = 0U; y < plane->height; ++y) {
        uint16_t x;
        for (x = 0U; x < plane->width; ++x) {
            hash ^= plane->data[(size_t)y * plane->stride + x];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

static void capture_filter_stage(size_t stage,
                                 const Av2hFilterPlanes *planes) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        av2h_debug_filter_stage_hash[stage][plane] =
            filter_plane_hash(&planes->plane[plane]);
    }
}

#define AV2H_CCSO_LEGACY_UNIT_SIZE 64U

static uint16_t ccso_unit_size(const Av2hFrameState *frame) {
    return frame->ccso_unit_size != 0U ?
        frame->ccso_unit_size : AV2H_CCSO_LEGACY_UNIT_SIZE;
}
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

static const uint16_t ccso_quant_size[4][4] = {
    { 16U, 8U, 32U, 0U },
    { 56U, 40U, 64U, 128U },
    { 48U, 24U, 96U, 192U },
    { 80U, 112U, 160U, 256U }
};

static const int8_t gdf_coordinates[18][2] = {
    { 6, 0 }, { 5, 0 }, { 4, 0 }, { 3, 0 },
    { 2, 1 }, { 2, 0 }, { 2, -1 },
    { 1, 2 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 1, -2 },
    { 0, 6 }, { 0, 5 }, { 0, 4 }, { 0, 3 }, { 0, 2 }, { 0, 1 }
};

static int dimensions_valid(const Av2hFilterPlanes *planes,
                            const Av2hFrameState *frame);

static uint8_t clip_sample(int value) {
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static int absolute_value(int value) {
    return value < 0 ? -value : value;
}

static int clamp_value(int value, int minimum, int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int round_shift_signed(int value, unsigned int shift) {
    int adjusted = value + (1 << (shift - 1U));
    if (adjusted >= 0) return adjusted >> shift;
    return -(int)(((unsigned int)(-adjusted) + ((1U << shift) - 1U)) >> shift);
}

static int round_shift_symmetric(int value, unsigned int shift) {
    unsigned int half = 1U << (shift - 1U);
    if (value >= 0) return (value + (int)half) >> shift;
    return -(int)(((unsigned int)(-value) + half) >> shift);
}

static int floor_shift_signed(int value, unsigned int shift) {
    if (value >= 0) return value >> shift;
    return -(int)(((unsigned int)(-value) + ((1U << shift) - 1U)) >> shift);
}

static size_t gdf_alpha_index(unsigned int qp_index,
                              unsigned int kernel_index,
                              unsigned int class_index) {
    return (((size_t)qp_index * 22U + kernel_index) * 4U) + class_index;
}

static size_t gdf_weight_index(unsigned int qp_index,
                               unsigned int output_index,
                               unsigned int kernel_index,
                               unsigned int class_index) {
    return (((((size_t)qp_index * 3U + output_index) * 22U) +
             kernel_index) * 4U) + class_index;
}

static size_t gdf_bias_index(unsigned int qp_index,
                             unsigned int output_index) {
    return (size_t)qp_index * 3U + output_index;
}

static unsigned int gdf_training_qp_index(const Av2hFrameState *frame) {
    unsigned int base_index = 0U;
    if (frame->base_q_index >= 172U) {
        base_index = 2U;
    } else if (frame->base_q_index >= 147U) {
        base_index = 1U;
    }
    return base_index + frame->gdf_qp_index;
}

static int deblock_qlookup(int q_index) {
    static const uint8_t ac_qlookup[25] = {
        64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U, 51U,
        52U, 54U, 55U, 57U, 59U, 60U, 62U, 64U, 66U, 68U,
        70U, 72U, 74U, 76U, 78U
    };
    if (q_index <= 0) return ac_qlookup[0];
    if (q_index > 255) q_index = 255;
    if (q_index < 25) return ac_qlookup[q_index];
    return (int)ac_qlookup[((q_index - 1) % 24) + 1] <<
           ((q_index - 1) / 24);
}

static void deblock_thresholds(const Av2hDeblockCell *cell,
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
    unsigned int delta_index = plane == 0U ? pass : plane + 1U;
    if (plane == 1U) {
        delta = frame->u_ac_delta_q + sequence->base_uv_ac_delta_q;
    } else if (plane == 2U) {
        delta = frame->v_ac_delta_q + sequence->base_uv_ac_delta_q;
    }
    if (cell->q_index == 0U && delta <= 0) {
        q_index = 0;
    } else {
        q_index = clamp_value((int)cell->q_index + delta, 1, 255);
    }
    level = q_index + 8 * frame->deblock_delta[delta_index];
    *q_threshold = (uint16_t)(((deblock_qlookup(level) + 4) >> 3) >> 6);
    side_index = clamp_value(level, 0, 295);
    *side_threshold = (uint16_t)(
        clamp_value(deblock_side_threshold[side_index] + 16, 0, 65535) >> 5);
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
    delta = clamp_value(delta, -limit, limit);
    if (negative_lossless == 0U) {
        for (index = 0U; index < negative_width; ++index) {
            int adjustment = round_shift_signed(
                delta * deblock_weight[negative_width - 1U] *
                    (int)(negative_width - index),
                11U);
            sample[-((ptrdiff_t)index + 1) * step] = clip_sample(
                sample[-((ptrdiff_t)index + 1) * step] + adjustment);
        }
    }
    if (positive_lossless == 0U) {
        for (index = 0U; index < positive_width; ++index) {
            int adjustment = round_shift_signed(
                delta * deblock_weight[positive_width - 1U] *
                    (int)(positive_width - index),
                11U);
            sample[(ptrdiff_t)index * step] = clip_sample(
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
    if (plane == 0 || plane->data == 0 || plane->stride < plane->width ||
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
    step = vertical != 0U ? 1 : (ptrdiff_t)plane->stride;
    lane_step = vertical != 0U ? (ptrdiff_t)plane->stride : 1;
    sample = plane->data + (size_t)y * plane->stride + x;
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

static int deblock_transform_dimension_valid(uint8_t value) {
    return value == 4U || value == 8U || value == 16U || value == 32U ||
           value == 64U;
}

static int deblock_map_valid(const Av2hFilterPlanes *planes,
                             const Av2hDeblockMap *map) {
    unsigned int plane;
    if (map == 0) return 0;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
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
            map->rows[plane] != rows || map->stride[plane] < columns) {
            return 0;
        }
        for (row = 0U; row < rows; ++row) {
            size_t column;
            for (column = 0U; column < columns; ++column) {
                const Av2hDeblockCell *cell =
                    &map->plane[plane][row * map->stride[plane] + column];
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
        &map->plane[plane][row * map->stride[plane] + column];
    size_t previous_row = row - (pass != 0U ? 1U : 0U);
    size_t previous_column = column - (pass == 0U ? 1U : 0U);
    const Av2hDeblockCell *previous = &map->plane[plane][
        previous_row * map->stride[plane] + previous_column];
    unsigned int transform_edge =
        cell->transform_column != previous->transform_column ||
        cell->transform_row != previous->transform_row;
    unsigned int block_edge = pass == 0U ?
        cell->block_column == column : cell->block_row == row;
    unsigned int filter_size;
    unsigned int maximum_positive;
    unsigned int maximum_negative;
    unsigned int superblock_edge;
    uint16_t current_q;
    uint16_t previous_q;
    uint16_t current_side;
    uint16_t previous_side;
    uint16_t q_threshold;
    uint16_t side_threshold;
    size_t grid_size = plane_data->width / map->columns[plane];
    size_t x = column * grid_size;
    size_t y = row * grid_size;
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
    deblock_thresholds(cell, sequence, frame, plane, pass,
                       &current_q, &current_side);
    deblock_thresholds(previous, sequence, frame, plane, pass,
                       &previous_q, &previous_side);
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
    superblock_edge = pass != 0U &&
        (row * grid_size % (plane == 0U ? 64U : 32U)) == 0U;
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

Av2hStatus av2h_deblock_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hDeblockMap *map) {
    unsigned int plane;
    if (planes == 0 || sequence == 0 || frame == 0 ||
        !dimensions_valid(planes, frame) || !deblock_map_valid(planes, map)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
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

static unsigned int floor_log2(unsigned int value) {
    unsigned int result = 0U;
    while (value > 1U) {
        value >>= 1U;
        ++result;
    }
    return result;
}

Av2hStatus av2h_cdef_find_direction(
    const Av2hFilterPlane *luma,
    uint16_t x,
    uint16_t y,
    uint8_t *direction,
    uint32_t *variance) {
    static const unsigned int divisors[9] = {
        0U, 840U, 420U, 280U, 210U, 168U, 140U, 120U, 105U
    };
    int partial[8][15];
    int64_t cost[8];
    int64_t best_cost = 0;
    unsigned int best_direction = 0U;
    unsigned int row;
    unsigned int column;
    unsigned int index;
    if (luma == 0 || luma->data == 0 || luma->stride < luma->width ||
        direction == 0 || variance == 0 ||
        (uint32_t)x + 8U > luma->width ||
        (uint32_t)y + 8U > luma->height) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 8U; ++index) {
        unsigned int line;
        cost[index] = 0;
        for (line = 0U; line < 15U; ++line) partial[index][line] = 0;
    }
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            int value = (int)luma->data[
                ((size_t)y + row) * luma->stride + x + column] - 128;
            partial[0][row + column] += value;
            partial[1][row + column / 2U] += value;
            partial[2][row] += value;
            partial[3][3U + row - column / 2U] += value;
            partial[4][7U + row - column] += value;
            partial[5][3U - row / 2U + column] += value;
            partial[6][column] += value;
            partial[7][row / 2U + column] += value;
            }
    }
    for (index = 0U; index < 8U; ++index) {
        cost[2] += (int64_t)partial[2][index] * partial[2][index];
        cost[6] += (int64_t)partial[6][index] * partial[6][index];
    }
    cost[2] *= divisors[8];
    cost[6] *= divisors[8];
    for (index = 0U; index < 7U; ++index) {
        cost[0] += ((int64_t)partial[0][index] * partial[0][index] +
                    (int64_t)partial[0][14U - index] *
                        partial[0][14U - index]) * divisors[index + 1U];
        cost[4] += ((int64_t)partial[4][index] * partial[4][index] +
                    (int64_t)partial[4][14U - index] *
                        partial[4][14U - index]) * divisors[index + 1U];
    }
    cost[0] += (int64_t)partial[0][7] * partial[0][7] * divisors[8];
    cost[4] += (int64_t)partial[4][7] * partial[4][7] * divisors[8];
    for (index = 1U; index < 8U; index += 2U) {
        unsigned int line;
        for (line = 0U; line < 5U; ++line) {
            cost[index] += (int64_t)partial[index][3U + line] *
                           partial[index][3U + line];
        }
        cost[index] *= divisors[8];
        for (line = 0U; line < 3U; ++line) {
            cost[index] +=
                ((int64_t)partial[index][line] * partial[index][line] +
                 (int64_t)partial[index][10U - line] *
                     partial[index][10U - line]) * divisors[2U * line + 2U];
        }
    }
    for (index = 0U; index < 8U; ++index) {
        if (cost[index] > best_cost) {
            best_cost = cost[index];
            best_direction = index;
        }
    }
    *direction = (uint8_t)best_direction;
    *variance = (uint32_t)((best_cost - cost[(best_direction + 4U) & 7U]) >>
                           10U);
    return AV2H_OK;
}

static int cdef_constrain(int difference,
                          unsigned int threshold,
                          unsigned int damping) {
    unsigned int magnitude;
    unsigned int adjustment;
    int constrained;
    if (threshold == 0U) return 0;
    magnitude = (unsigned int)absolute_value(difference);
    adjustment = floor_log2(threshold);
    adjustment = damping > adjustment ? damping - adjustment : 0U;
    constrained = (int)threshold - (int)(magnitude >> adjustment);
    constrained = clamp_value(constrained, 0, (int)magnitude);
    return difference < 0 ? -constrained : constrained;
}

static int cdef_tap(const Av2hFilterPlane *source,
                    int x,
                    int y,
                    int center,
                    unsigned int strength,
                    unsigned int damping,
                    int *minimum,
                    int *maximum,
                    int clipping_required,
                    int weight) {
    int sample;
    if (x < 0 || y < 0 || x >= (int)source->width ||
        y >= (int)source->height) {
        return 0;
    }
    sample = source->data[(size_t)y * source->stride + (size_t)x];
    if (clipping_required) {
        if (sample < *minimum) *minimum = sample;
        if (sample > *maximum) *maximum = sample;
    }
    return weight * cdef_constrain(sample - center, strength, damping);
}

Av2hStatus av2h_cdef_filter_block(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *source,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    unsigned int plane_index,
    uint8_t primary_strength,
    uint8_t secondary_strength,
    uint8_t damping,
    uint8_t direction,
    uint32_t variance) {
    static const int8_t directions[8][2][2] = {
        { { -1, 1 }, { -2, 2 } }, { { 0, 1 }, { -1, 2 } },
        { { 0, 1 }, { 0, 2 } }, { { 0, 1 }, { 1, 2 } },
        { { 1, 1 }, { 2, 2 } }, { { 1, 0 }, { 2, 1 } },
        { { 1, 0 }, { 2, 0 } }, { { 1, 0 }, { 2, -1 } }
    };
    static const uint8_t primary_taps[2][2] = { { 4U, 2U }, { 3U, 3U } };
    uint8_t adjusted_primary = primary_strength;
    unsigned int tap_set;
    int clipping_required;
    uint16_t row;
    if (destination == 0 || source == 0 || destination->data == 0 ||
        source->data == 0 || destination->stride < destination->width ||
        source->stride < source->width || plane_index >= 3U ||
        primary_strength > 15U ||
        (secondary_strength != 0U && secondary_strength != 1U &&
         secondary_strength != 2U && secondary_strength != 4U) ||
        damping > 6U ||
        direction >= 8U || width == 0U || height == 0U ||
        width > (plane_index == 0U ? 8U : 4U) ||
        height > (plane_index == 0U ? 8U : 4U) ||
        destination->width != source->width ||
        destination->height != source->height ||
        (uint32_t)x + width > source->width ||
        (uint32_t)y + height > source->height) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (plane_index == 0U) {
        if (variance == 0U) {
            adjusted_primary = 0U;
        } else {
            unsigned int strength_scale = variance >> 6U;
            strength_scale = strength_scale == 0U ? 0U :
                floor_log2(strength_scale);
            if (strength_scale > 12U) strength_scale = 12U;
            adjusted_primary = (uint8_t)(
                (primary_strength * (4U + strength_scale) + 8U) >> 4U);
        }
    } else if (damping != 0U) {
        --damping;
    }
    tap_set = adjusted_primary & 1U;
    clipping_required = adjusted_primary != 0U && secondary_strength != 0U;
    for (row = 0U; row < height; ++row) {
        uint16_t column;
        for (column = 0U; column < width; ++column) {
            int sample_x = (int)x + column;
            int sample_y = (int)y + row;
            int center = source->data[(size_t)sample_y * source->stride +
                                      (size_t)sample_x];
            int minimum = center;
            int maximum = center;
            int sum = 0;
            unsigned int distance;
            for (distance = 0U; distance < 2U; ++distance) {
                int sign;
                for (sign = -1; sign <= 1; sign += 2) {
                    unsigned int secondary_index;
                    int primary_y = directions[direction][distance][0];
                    int primary_x = directions[direction][distance][1];
                    sum += cdef_tap(
                        source, sample_x + sign * primary_x,
                        sample_y + sign * primary_y, center,
                        adjusted_primary, damping, &minimum, &maximum,
                        clipping_required, primary_taps[tap_set][distance]);
                    for (secondary_index = 0U; secondary_index < 2U;
                         ++secondary_index) {
                        unsigned int secondary_direction =
                            (direction + (secondary_index == 0U ? 2U : 6U)) &
                            7U;
                        int secondary_y =
                            directions[secondary_direction][distance][0];
                        int secondary_x =
                            directions[secondary_direction][distance][1];
                        sum += cdef_tap(
                            source, sample_x + sign * secondary_x,
                            sample_y + sign * secondary_y, center,
                            secondary_strength, damping, &minimum, &maximum,
                            clipping_required, 2 - (int)distance);
                    }
                }
            }
            center += floor_shift_signed(8 + sum - (sum < 0), 4U);
            if (clipping_required) {
                center = clamp_value(center, minimum, maximum);
            }
            destination->data[(size_t)sample_y * destination->stride +
                              (size_t)sample_x] = clip_sample(center);
        }
    }
    return AV2H_OK;
}

static Av2hStatus cdef_partial_direction(const Av2hFilterPlane *luma,
                                         uint16_t x,
                                         uint16_t y,
                                         uint8_t *direction,
                                         uint32_t *variance) {
    uint8_t samples[64];
    Av2hFilterPlane block = { samples, 8U, 8U, 8U };
    unsigned int row;
    for (row = 0U; row < 8U; ++row) {
        unsigned int column;
        uint16_t source_y = (uint16_t)(y + row);
        if (source_y >= luma->height) source_y = luma->height - 1U;
        for (column = 0U; column < 8U; ++column) {
            uint16_t source_x = (uint16_t)(x + column);
            if (source_x >= luma->width) source_x = luma->width - 1U;
            samples[row * 8U + column] =
                luma->data[(size_t)source_y * luma->stride + source_x];
        }
    }
    return av2h_cdef_find_direction(&block, 0U, 0U, direction, variance);
}

static void copy_filter_plane(Av2hFilterPlane *destination,
                              const Av2hFilterPlane *source) {
    uint16_t row;
    for (row = 0U; row < source->height; ++row) {
        uint16_t column;
        for (column = 0U; column < source->width; ++column) {
            destination->data[(size_t)row * destination->stride + column] =
                source->data[(size_t)row * source->stride + column];
        }
    }
}

Av2hStatus av2h_cdef_filter_frame_with_controls(
    Av2hFilterPlanes *destination,
    const Av2hFilterPlanes *deblocked,
    const Av2hFrameState *frame,
    const Av2hCdefControlMap *controls) {
    uint16_t y;
    unsigned int plane;
    if (destination == 0 || deblocked == 0 || frame == 0 ||
        frame->width == 0U || frame->height == 0U ||
        !dimensions_valid(destination, frame) ||
        !dimensions_valid(deblocked, frame)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (frame->cdef_strength_count != 0U &&
         (frame->cdef_on_skip_transform == 0U ||
          frame->cdef_damping < 3U)) {
        return AV2H_UNSUPPORTED;
    }
    if (frame->cdef_strength_count != 0U) {
        size_t strength_index;
        if (frame->cdef_damping > 6U) return AV2H_INVALID_ARGUMENT;
        for (strength_index = 0U;
             strength_index < frame->cdef_strength_count;
             ++strength_index) {
            if (frame->cdef_y_strength[strength_index] > 63U ||
                frame->cdef_uv_strength[strength_index] > 63U) {
                return AV2H_INVALID_ARGUMENT;
            }
        }
    }
    if (frame->cdef_strength_count > 1U &&
        (controls == 0 || controls->strengths == 0 ||
         controls->columns != ((size_t)frame->width + 63U) / 64U ||
         controls->rows != ((size_t)frame->height + 63U) / 64U)) {
        return AV2H_UNSUPPORTED;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (destination->plane[plane].data == deblocked->plane[plane].data) {
            return AV2H_INVALID_ARGUMENT;
        }
        copy_filter_plane(&destination->plane[plane],
                          &deblocked->plane[plane]);
    }
    if (frame->cdef_strength_count == 0U) return AV2H_OK;
    for (y = 0U; y < deblocked->plane[0].height;
         y = (uint16_t)(y + 8U)) {
        uint16_t x;
        for (x = 0U; x < deblocked->plane[0].width;
             x = (uint16_t)(x + 8U)) {
            size_t strength_index = 0U;
            uint8_t y_primary;
            uint8_t y_secondary;
            uint8_t uv_primary;
            uint8_t uv_secondary;
            uint16_t width = (uint16_t)(deblocked->plane[0].width - x);
            uint16_t height = (uint16_t)(deblocked->plane[0].height - y);
            uint8_t direction;
            uint32_t variance;
            Av2hStatus status;
            if (frame->cdef_strength_count > 1U) {
                strength_index = ((size_t)y / 64U) * controls->columns +
                    (size_t)x / 64U;
                strength_index = controls->strengths[strength_index];
                if (strength_index >= frame->cdef_strength_count) {
                    return AV2H_INVALID_DATA;
                }
            }
            y_primary = frame->cdef_y_strength[strength_index] / 4U;
            y_secondary = frame->cdef_y_strength[strength_index] % 4U;
            if (y_secondary == 3U) ++y_secondary;
            uv_primary = frame->cdef_uv_strength[strength_index] / 4U;
            uv_secondary = frame->cdef_uv_strength[strength_index] % 4U;
            if (uv_secondary == 3U) ++uv_secondary;
            if (width > 8U) width = 8U;
            if (height > 8U) height = 8U;
            if (width == 8U && height == 8U) {
                status = av2h_cdef_find_direction(
                    &deblocked->plane[0], x, y, &direction, &variance);
            } else {
                status = cdef_partial_direction(
                    &deblocked->plane[0], x, y, &direction, &variance);
            }
            if (status != AV2H_OK) return status;
            status = av2h_cdef_filter_block(
                &destination->plane[0], &deblocked->plane[0], x, y,
                width, height, 0U, y_primary, y_secondary,
                frame->cdef_damping,
                y_primary != 0U ? direction : 0U, variance);
            if (status != AV2H_OK) return status;
            for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                uint16_t chroma_x = x >> 1U;
                uint16_t chroma_y = y >> 1U;
                uint16_t chroma_width = width == 8U ? 4U :
                    (uint16_t)(((uint32_t)width + 1U) >> 1U);
                uint16_t chroma_height = height == 8U ? 4U :
                    (uint16_t)(((uint32_t)height + 1U) >> 1U);
                status = av2h_cdef_filter_block(
                    &destination->plane[plane], &deblocked->plane[plane],
                    chroma_x, chroma_y, chroma_width, chroma_height, plane,
                    uv_primary, uv_secondary, frame->cdef_damping,
                    uv_primary != 0U ? direction : 0U, variance);
                if (status != AV2H_OK) return status;
            }
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_cdef_filter_frame(
    Av2hFilterPlanes *destination,
    const Av2hFilterPlanes *deblocked,
    const Av2hFrameState *frame) {
    return av2h_cdef_filter_frame_with_controls(
        destination, deblocked, frame, 0);
}

static uint8_t gdf_source_sample(const Av2hFilterPlane *cdef_luma,
                                 const Av2hFilterPlane *deblocked_luma,
                                 int x,
                                 int y,
                                 int stripe_start,
                                 int stripe_end) {
    const Av2hFilterPlane *source = cdef_luma;
    if (x < 0) x = 0;
    if (x >= (int)cdef_luma->width) x = (int)cdef_luma->width - 1;
    if (y < 0) y = 0;
    if (y >= (int)cdef_luma->height) y = (int)cdef_luma->height - 1;
    if (y < stripe_start) {
        int minimum = stripe_start - 2;
        if (y < minimum) y = minimum;
        source = deblocked_luma;
    } else if (y > stripe_end) {
        int maximum = stripe_end + 2;
        if (y > maximum) y = maximum;
        source = deblocked_luma;
    }
    return source->data[(size_t)y * source->stride + (size_t)x];
}

static int filter_planes_overlap(const Av2hFilterPlane *left,
                                 const Av2hFilterPlane *right) {
    size_t left_row = 0U;
    size_t right_row = 0U;
    uintptr_t pointer_max = (uintptr_t)-1;
    while (left_row < left->height && right_row < right->height) {
        size_t left_offset;
        size_t right_offset;
        uintptr_t left_start;
        uintptr_t left_end;
        uintptr_t right_start;
        uintptr_t right_end;
        if (!av2h_size_multiply(left_row, left->stride, &left_offset) ||
            !av2h_size_multiply(right_row, right->stride, &right_offset) ||
            left_offset > (size_t)pointer_max ||
            right_offset > (size_t)pointer_max ||
            (uintptr_t)left->data > pointer_max - (uintptr_t)left_offset ||
            (uintptr_t)right->data > pointer_max - (uintptr_t)right_offset) {
            return 1;
        }
        left_start = (uintptr_t)left->data + (uintptr_t)left_offset;
        right_start = (uintptr_t)right->data + (uintptr_t)right_offset;
        if ((uintptr_t)left->width > pointer_max - left_start ||
            (uintptr_t)right->width > pointer_max - right_start) {
            return 1;
        }
        left_end = left_start + (uintptr_t)left->width;
        right_end = right_start + (uintptr_t)right->width;
        if (left_start < right_end && right_start < left_end) return 1;
        if (left_end <= right_start) {
            ++left_row;
        } else {
            ++right_row;
        }
    }
    return 0;
}

static unsigned int gdf_gradient_sum(const uint16_t gradient[4][6][6],
                                     unsigned int direction,
                                     unsigned int row,
                                     unsigned int column) {
    unsigned int sum = 0U;
    unsigned int y;
    for (y = 0U; y < 4U; ++y) {
        unsigned int x;
        for (x = 0U; x < 4U; ++x) {
            sum += gradient[direction][row + y][column + x];
        }
    }
    return sum;
}

static void gdf_filter_block(Av2hFilterPlane *destination,
                             const Av2hFilterPlane *cdef_luma,
                             const Av2hFilterPlane *deblocked_luma,
                             uint16_t block_x,
                             uint16_t block_y,
                             unsigned int qp_index,
                             unsigned int pixel_scale) {
    uint16_t gradient[4][6][6];
    uint8_t classes[2][2];
    unsigned int stripe_number = ((unsigned int)block_y + 8U) / 64U;
    int stripe_start = (int)(stripe_number * 64U) - 8;
    int stripe_end = stripe_start + 63;
    unsigned int row;
    if (stripe_start < 0) stripe_start = 0;
    if (stripe_end >= (int)cdef_luma->height) {
        stripe_end = (int)cdef_luma->height - 1;
    }
    for (row = 0U; row < 6U; ++row) {
        unsigned int column;
        for (column = 0U; column < 6U; ++column) {
            unsigned int direction;
            for (direction = 0U; direction < 4U; ++direction) {
                static const int8_t direction_offset[4][2] = {
                    { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 }
                };
                int dx = direction_offset[direction][0];
                int dy = direction_offset[direction][1];
                int x = (int)block_x - 1 + (int)column;
                int y = (int)block_y - 1 + (int)row;
                int center = gdf_source_sample(
                    cdef_luma, deblocked_luma, x, y,
                    stripe_start, stripe_end);
                int first = gdf_source_sample(
                    cdef_luma, deblocked_luma, x - dx, y - dy,
                    stripe_start, stripe_end);
                int second = gdf_source_sample(
                    cdef_luma, deblocked_luma, x + dx, y + dy,
                    stripe_start, stripe_end);
                gradient[direction][row][column] =
                    (uint16_t)absolute_value(center * 2 - first - second);
            }
        }
    }
    for (row = 0U; row < 2U; ++row) {
        unsigned int column;
        for (column = 0U; column < 2U; ++column) {
            unsigned int vertical = gdf_gradient_sum(
                gradient, 0U, row * 2U, column * 2U);
            unsigned int horizontal = gdf_gradient_sum(
                gradient, 1U, row * 2U, column * 2U);
            unsigned int diagonal_zero = gdf_gradient_sum(
                gradient, 2U, row * 2U, column * 2U);
            unsigned int diagonal_one = gdf_gradient_sum(
                gradient, 3U, row * 2U, column * 2U);
            classes[row][column] = (uint8_t)(
                (vertical > horizontal ? 0U : 1U) |
                (diagonal_zero > diagonal_one ? 0U : 2U));
        }
    }
    for (row = 0U; row < 4U; ++row) {
        unsigned int column;
        for (column = 0U; column < 4U; ++column) {
            unsigned int class_index = classes[row >> 1U][column >> 1U];
            int accumulator[3] = { 0, 0, 0 };
            unsigned int kernel_index;
            unsigned int output_index;
            size_t destination_index =
                ((size_t)block_y + row) * destination->stride +
                block_x + column;
            int center = gdf_source_sample(
                cdef_luma, deblocked_luma,
                (int)block_x + (int)column,
                (int)block_y + (int)row, stripe_start, stripe_end);
            for (kernel_index = 0U; kernel_index < 22U; ++kernel_index) {
                unsigned int alpha = gdf_alpha[gdf_alpha_index(
                    qp_index, kernel_index, class_index)];
                int combination;
                if (kernel_index < 18U) {
                    int dy = gdf_coordinates[kernel_index][0];
                    int dx = gdf_coordinates[kernel_index][1];
                    int above = gdf_source_sample(
                        cdef_luma, deblocked_luma,
                        (int)block_x + (int)column - dx,
                        (int)block_y + (int)row - dy,
                        stripe_start, stripe_end) - center;
                    int below = gdf_source_sample(
                        cdef_luma, deblocked_luma,
                        (int)block_x + (int)column + dx,
                        (int)block_y + (int)row + dy,
                        stripe_start, stripe_end) - center;
                    above *= 4;
                    below *= 4;
                    above = clamp_value(above, -(int)alpha, (int)alpha);
                    below = clamp_value(below, -(int)alpha, (int)alpha);
                    combination = clamp_value(above + below, -512, 511);
                } else {
                    unsigned int value = gdf_gradient_sum(
                        gradient, kernel_index - 18U,
                        (row >> 1U) << 1U, (column >> 1U) << 1U) >> 2U;
                    combination = value < alpha ? (int)value : (int)alpha;
                }
                for (output_index = 0U; output_index < 3U; ++output_index) {
                    accumulator[output_index] += combination *
                        gdf_weight[gdf_weight_index(
                            qp_index, output_index, kernel_index,
                            class_index)];
                }
            }
            {
                unsigned int position = 0U;
                int normalized[3] = { 0, 0, 0 };
                for (output_index = 0U; output_index < 3U; ++output_index) {
                    int value = round_shift_symmetric(
                        (accumulator[output_index] +
                         gdf_bias[gdf_bias_index(qp_index, output_index)]) *
                            8,
                        15U);
                    normalized[output_index] = value;
                    position = position * 16U + (unsigned int)
                        (clamp_value(value, -8, 7) + 8);
                }
                destination->data[destination_index] = clip_sample(
                    (int)destination->data[destination_index] +
                    round_shift_symmetric(
                        (int)gdf_intra_error[qp_index * 4096U + position] *
                            (int)pixel_scale,
                        4U));
            }
        }
    }
}

Av2hStatus av2h_gdf_filter_frame_with_controls(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *cdef_luma,
    const Av2hFilterPlane *deblocked_luma,
    const Av2hFrameState *frame,
    const Av2hGdfControlMap *controls) {
    uint16_t row;
    unsigned int qp_index;
    if (destination == 0 || cdef_luma == 0 || deblocked_luma == 0 ||
        frame == 0 || destination->data == 0 || cdef_luma->data == 0 ||
        deblocked_luma->data == 0 || destination->stride < destination->width ||
        cdef_luma->stride < cdef_luma->width ||
        deblocked_luma->stride < deblocked_luma->width ||
        destination->width == 0U || destination->height == 0U ||
        destination->width != cdef_luma->width ||
        destination->height != cdef_luma->height ||
        destination->width != deblocked_luma->width ||
        destination->height != deblocked_luma->height ||
        filter_planes_overlap(destination, cdef_luma) ||
        filter_planes_overlap(destination, deblocked_luma) ||
        filter_planes_overlap(cdef_luma, deblocked_luma) ||
        frame->gdf_mode > 2U ||
        frame->gdf_qp_index >= 4U || frame->gdf_scale_index >= 4U ||
        (destination->width & 3U) != 0U ||
        (destination->height & 3U) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (frame->gdf_mode == 2U) {
        size_t index;
        if (controls == 0 || controls->flags == 0 ||
            controls->block_size != frame->gdf_block_size ||
            controls->columns != frame->gdf_block_columns ||
            controls->rows != frame->gdf_block_rows ||
            controls->stride < controls->columns ||
            controls->columns !=
                ((size_t)frame->width + controls->block_size - 1U) /
                    controls->block_size ||
            controls->rows !=
                ((size_t)frame->height + controls->block_size - 1U) /
                    controls->block_size) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < controls->rows; ++index) {
            size_t column;
            for (column = 0U; column < controls->columns; ++column) {
                if (controls->flags[index * controls->stride + column] > 1U) {
                    return AV2H_INVALID_ARGUMENT;
                }
            }
        }
    }
    qp_index = gdf_training_qp_index(frame);
    for (row = 0U; row < destination->height; ++row) {
        uint16_t column;
        for (column = 0U; column < destination->width; ++column) {
            destination->data[(size_t)row * destination->stride + column] =
                cdef_luma->data[(size_t)row * cdef_luma->stride + column];
        }
    }
    if (frame->gdf_mode == 0U) return AV2H_OK;
    for (row = 0U; row < destination->height; row = (uint16_t)(row + 4U)) {
        uint16_t column;
        for (column = 0U; column < destination->width;
             column = (uint16_t)(column + 4U)) {
            if (frame->gdf_mode == 2U &&
                controls->flags[
                    ((size_t)row / controls->block_size) * controls->stride +
                    (size_t)column / controls->block_size] == 0U) {
                continue;
            }
            gdf_filter_block(destination, cdef_luma, deblocked_luma,
                             column, row, qp_index,
                             (unsigned int)frame->gdf_scale_index + 1U);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_gdf_filter_frame(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *cdef_luma,
    const Av2hFilterPlane *deblocked_luma,
    const Av2hFrameState *frame) {
    return av2h_gdf_filter_frame_with_controls(
        destination, cdef_luma, deblocked_luma, frame, 0);
}

static size_t coded_dimension(uint16_t value) {
    return ((size_t)value + 7U) & ~(size_t)7U;
}

static int workspace_reserve(size_t *size,
                             size_t count,
                             size_t element_size,
                             size_t alignment) {
    size_t bytes;
    size_t aligned;
    if (!av2h_size_multiply(count, element_size, &bytes) ||
        !av2h_size_align(*size, alignment, &aligned) ||
        !av2h_size_add(aligned, bytes, size)) {
        return 0;
    }
    return 1;
}

Av2hStatus av2h_filter_workspace_query(
    const Av2hFrameState *frame,
    Av2hFilterWorkspaceLayout *layout) {
    size_t coded_width;
    size_t coded_height;
    size_t size = 0U;
    unsigned int stage;
    unsigned int plane;
    if (frame == 0 || layout == 0 || frame->width == 0U ||
        frame->height == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (ccso_unit_size(frame) != 64U &&
        ccso_unit_size(frame) != 128U &&
        ccso_unit_size(frame) != 256U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(layout, 0U, sizeof(*layout));
    coded_width = coded_dimension(frame->width);
    coded_height = coded_dimension(frame->height);
    layout->alignment = _Alignof(Av2hDeblockCell);
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        size_t width = coded_width >> (plane == 0U ? 0U : 1U);
        size_t height = coded_height >> (plane == 0U ? 0U : 1U);
        size_t grid_size = plane == 0U ? 4U : 2U;
        if (width > UINT16_MAX || height > UINT16_MAX ||
            !av2h_size_multiply(
                width, height, &layout->stage_plane_bytes[plane]) ||
            !av2h_size_multiply(
                width / grid_size, height / grid_size,
                &layout->deblock_cell_count[plane])) {
            return AV2H_OVERFLOW;
        }
        layout->plane_width[plane] = (uint16_t)width;
        layout->plane_height[plane] = (uint16_t)height;
        layout->deblock_columns[plane] = width / grid_size;
        layout->deblock_rows[plane] = height / grid_size;
    }
    {
        uint16_t unit_size = ccso_unit_size(frame);
        layout->ccso_columns =
            (coded_width + unit_size - 1U) / unit_size;
        layout->ccso_rows =
            (coded_height + unit_size - 1U) / unit_size;
    }
    if (!av2h_size_multiply(
            layout->ccso_columns, layout->ccso_rows,
            &layout->ccso_control_count)) {
        return AV2H_OVERFLOW;
    }
    for (stage = 0U; stage < 2U; ++stage) {
        for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            if (!workspace_reserve(
                    &size, layout->stage_plane_bytes[plane],
                    sizeof(uint8_t), _Alignof(uint8_t))) {
                return AV2H_OVERFLOW;
            }
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (!workspace_reserve(
                &size, layout->deblock_cell_count[plane],
                sizeof(Av2hDeblockCell), _Alignof(Av2hDeblockCell))) {
            return AV2H_OVERFLOW;
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (!workspace_reserve(
                &size, layout->ccso_control_count,
                sizeof(uint8_t), _Alignof(uint8_t))) {
            return AV2H_OVERFLOW;
        }
    }
    layout->size = size;
    return AV2H_OK;
}

Av2hStatus av2h_filter_workspace_init(
    const Av2hFrameState *frame,
    void *memory,
    size_t memory_size,
    Av2hFilterWorkspace *workspace,
    Av2hFilterWorkspaceStorage *storage) {
    Av2hFilterWorkspaceLayout layout;
    Av2hArena arena;
    unsigned int plane;
    Av2hStatus status;
    if (workspace == 0 || storage == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_filter_workspace_query(frame, &layout);
    if (status != AV2H_OK) return status;
    if (memory == 0 ||
        ((uintptr_t)memory & (layout.alignment - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (memory_size < layout.size) return AV2H_OUT_OF_MEMORY;
    av2h_memory_fill(workspace, 0U, sizeof(*workspace));
    av2h_memory_fill(storage, 0U, sizeof(*storage));
    av2h_memory_fill(memory, 0U, layout.size);
    av2h_arena_init(&arena, memory, layout.size);
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        workspace->deblocked.plane[plane] = (Av2hFilterPlane){
            av2h_arena_allocate(
                &arena, layout.stage_plane_bytes[plane],
                _Alignof(uint8_t)),
            layout.plane_width[plane],
            layout.plane_width[plane],
            layout.plane_height[plane]
        };
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        workspace->cdef.plane[plane] = (Av2hFilterPlane){
            av2h_arena_allocate(
                &arena, layout.stage_plane_bytes[plane],
                _Alignof(uint8_t)),
            layout.plane_width[plane],
            layout.plane_width[plane],
            layout.plane_height[plane]
        };
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        storage->deblock_cells[plane] = av2h_arena_allocate(
            &arena,
            layout.deblock_cell_count[plane] * sizeof(Av2hDeblockCell),
            _Alignof(Av2hDeblockCell));
        workspace->deblock_map.plane[plane] =
            storage->deblock_cells[plane];
        workspace->deblock_map.stride[plane] =
            layout.deblock_columns[plane];
        workspace->deblock_map.columns[plane] =
            layout.deblock_columns[plane];
        workspace->deblock_map.rows[plane] =
            layout.deblock_rows[plane];
    }
    workspace->ccso_controls.columns = layout.ccso_columns;
    workspace->ccso_controls.rows = layout.ccso_rows;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        storage->ccso_controls[plane] = av2h_arena_allocate(
            &arena, layout.ccso_control_count, _Alignof(uint8_t));
        workspace->ccso_controls.plane[plane] =
            storage->ccso_controls[plane];
        workspace->ccso_controls.stride[plane] = layout.ccso_columns;
    }
    if (arena.status != AV2H_OK ||
        av2h_arena_required(&arena) > layout.size) {
        return arena.status != AV2H_OK ? arena.status : AV2H_OVERFLOW;
    }
    return AV2H_OK;
}

static int dimensions_valid(const Av2hFilterPlanes *planes,
                            const Av2hFrameState *frame) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        size_t width = coded_dimension(frame->width) >> (plane == 0U ? 0U : 1U);
        size_t height = coded_dimension(frame->height) >> (plane == 0U ? 0U : 1U);
        if (planes->plane[plane].data == 0 ||
            planes->plane[plane].width != width ||
            planes->plane[plane].height != height ||
            planes->plane[plane].stride < width) {
            return 0;
        }
    }
    return 1;
}

static int controls_valid(const Av2hFrameState *frame,
                          const Av2hCcsoControlMap *controls) {
    uint16_t unit_size = ccso_unit_size(frame);
    size_t columns = (coded_dimension(frame->width) +
                      unit_size - 1U) / unit_size;
    size_t rows = (coded_dimension(frame->height) +
                   unit_size - 1U) / unit_size;
    unsigned int plane;
    if (controls == 0 || controls->columns != columns ||
        controls->rows != rows ||
        (unit_size != 64U && unit_size != 128U && unit_size != 256U)) {
        return 0;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (frame->ccso[plane].enabled != 0U &&
            (controls->plane[plane] == 0 ||
             controls->stride[plane] < columns)) {
            return 0;
        }
        if (frame->ccso[plane].enabled != 0U) {
            size_t row;
            for (row = 0U; row < rows; ++row) {
                size_t column;
                for (column = 0U; column < columns; ++column) {
                    if (controls->plane[plane][
                            row * controls->stride[plane] + column] > 1U) {
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

static int ccso_parameters_valid(const Av2hFrameState *frame) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hCcsoPlane *parameters = &frame->ccso[plane];
        unsigned int edge_intervals;
        uint16_t expected_offsets;
        if (parameters->enabled > 1U || parameters->band_only > 1U) {
            return 0;
        }
        if (parameters->enabled == 0U) continue;
        if (parameters->scale_index >= 4U) return 0;
        if (parameters->band_only != 0U) {
            if (parameters->quant_index != 0U ||
                parameters->extended_filter_support != 0U ||
                parameters->edge_classifier != 0U ||
                parameters->max_band_log2 > 6U) {
                return 0;
            }
            edge_intervals = 1U;
        } else {
            if (parameters->quant_index >= 4U ||
                parameters->extended_filter_support >= 7U ||
                parameters->edge_classifier >= 2U ||
                parameters->max_band_log2 > 3U ||
                (parameters->scale_index == 0U &&
                 parameters->quant_index == 3U &&
                 parameters->edge_classifier != 0U)) {
                return 0;
            }
            edge_intervals =
                parameters->edge_classifier != 0U ? 2U : 3U;
        }
        expected_offsets = (uint16_t)(
            ((uint16_t)1U << parameters->max_band_log2) *
            edge_intervals * edge_intervals);
        if (parameters->offset_count != expected_offsets) return 0;
    }
    return 1;
}

static void ccso_sample_offsets(uint8_t support,
                                int *first_x,
                                int *first_y,
                                int *second_x,
                                int *second_y) {
    static const int8_t offsets[7][4] = {
        { 0, -1, 0, 1 },
        { -1, 0, 1, 0 },
        { -1, -1, 1, 1 },
        { 1, -1, -1, 1 },
        { -2, -1, 2, 1 },
        { -2, 1, 2, -1 },
        { 2, 0, -2, 0 }
    };
    *first_x = offsets[support][0];
    *first_y = offsets[support][1];
    *second_x = offsets[support][2];
    *second_y = offsets[support][3];
}

static uint8_t luma_sample(const uint8_t *luma,
                           size_t stride,
                           uint16_t width,
                           uint16_t height,
                           int x,
                           int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= (int)width) x = (int)width - 1;
    if (y >= (int)height) y = (int)height - 1;
    return luma[(size_t)y * stride + (size_t)x];
}

static unsigned int ccso_class(uint8_t neighbor,
                               uint8_t center,
                               uint16_t threshold,
                               uint8_t edge_classifier) {
    int difference = (int)neighbor - (int)center;
    if (difference < -(int)threshold) return 0U;
    if (edge_classifier != 0U) return 1U;
    return difference > (int)threshold ? 2U : 1U;
}

static void ccso_filter_plane(Av2hFilterPlane *destination,
                              const uint8_t *luma,
                              size_t luma_stride,
                              uint16_t luma_width,
                              uint16_t luma_height,
                              const Av2hCcsoPlane *parameters,
                              const uint8_t *controls,
                              size_t control_stride,
                              uint16_t luma_unit_size,
                              unsigned int plane_index) {
    unsigned int subsampling = plane_index == 0U ? 0U : 1U;
    unsigned int unit_size = luma_unit_size >> subsampling;
    unsigned int shift = 8U - parameters->max_band_log2;
    uint16_t threshold = ccso_quant_size[parameters->scale_index]
                                        [parameters->quant_index];
    int first_x = 0;
    int first_y = 0;
    int second_x = 0;
    int second_y = 0;
    uint16_t y;
    if (parameters->band_only == 0U) {
        ccso_sample_offsets(parameters->extended_filter_support,
                            &first_x, &first_y, &second_x, &second_y);
    }
    for (y = 0U; y < destination->height; ++y) {
        uint16_t x;
        for (x = 0U; x < destination->width; ++x) {
            size_t control_row = y / unit_size;
            size_t control_column = x / unit_size;
            int luma_x;
            int luma_y;
            uint8_t center;
            unsigned int first_class = 0U;
            unsigned int second_class = 0U;
            unsigned int band;
            size_t lut_index;
            size_t destination_index;
            if (controls[control_row * control_stride + control_column] == 0U) {
                continue;
            }
            luma_x = (int)((uint32_t)x << subsampling);
            luma_y = (int)((uint32_t)y << subsampling);
            center = luma_sample(luma, luma_stride, luma_width,
                                 luma_height, luma_x, luma_y);
            if (parameters->band_only == 0U) {
                first_class = ccso_class(
                    luma_sample(luma, luma_stride, luma_width, luma_height,
                                luma_x + first_x, luma_y + first_y),
                    center, threshold, parameters->edge_classifier);
                second_class = ccso_class(
                    luma_sample(luma, luma_stride, luma_width, luma_height,
                                luma_x + second_x, luma_y + second_y),
                    center, threshold, parameters->edge_classifier);
            }
            band = parameters->max_band_log2 == 0U ? 0U : center >> shift;
            lut_index = ((size_t)band << 4U) +
                        ((size_t)first_class << 2U) + second_class;
            destination_index = (size_t)y * destination->stride + x;
            destination->data[destination_index] = clip_sample(
                (int)destination->data[destination_index] +
                parameters->filter_offset[lut_index]);
        }
    }
}

Av2hStatus av2h_ccso_filter_frame(
    Av2hFilterPlanes *planes,
    const uint8_t *pre_cdef_luma,
    size_t pre_cdef_luma_stride,
    const Av2hFrameState *frame,
    const Av2hCcsoControlMap *controls) {
    unsigned int plane;
    if (planes == 0 || pre_cdef_luma == 0 || frame == 0 ||
        frame->width == 0U || frame->height == 0U ||
        pre_cdef_luma_stride < coded_dimension(frame->width) ||
        !dimensions_valid(planes, frame) ||
        !ccso_parameters_valid(frame) ||
        !controls_valid(frame, controls)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (frame->ccso[plane].enabled == 0U) continue;
        ccso_filter_plane(&planes->plane[plane], pre_cdef_luma,
                          pre_cdef_luma_stride, planes->plane[0].width,
                          planes->plane[0].height, &frame->ccso[plane],
                          controls->plane[plane], controls->stride[plane],
                          ccso_unit_size(frame), plane);
    }
    return AV2H_OK;
}

Av2hStatus av2h_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFilterWorkspace *workspace) {
    unsigned int plane;
    Av2hStatus status;
    av2h_debug_filter_reason = 0U;
    av2h_memory_fill(
        av2h_debug_filter_stage_hash, 0U,
        sizeof(av2h_debug_filter_stage_hash));
    if (sequence == 0 || frame == 0 || planes == 0 || workspace == 0) {
        av2h_debug_filter_reason = 1U;
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration != 0U) {
        av2h_debug_filter_reason = 2U;
        return AV2H_UNSUPPORTED;
    }
    if (frame->cdef_strength_count != 0U &&
        (frame->cdef_on_skip_transform == 0U ||
         frame->cdef_damping < 3U)) {
        av2h_debug_filter_reason = 5U;
        return AV2H_UNSUPPORTED;
    }
    if ((frame->cdef_strength_count != 0U &&
         sequence->enable_cdef == 0U) ||
        ((frame->ccso[0].enabled != 0U ||
          frame->ccso[1].enabled != 0U ||
          frame->ccso[2].enabled != 0U) &&
         sequence->enable_ccso == 0U) ||
        (frame->gdf_mode != 0U && sequence->enable_gdf == 0U) ||
        (frame->cdef_strength_count != 0U &&
         (frame->cdef_damping > 6U ||
          frame->cdef_y_strength[0] > 63U ||
          frame->cdef_uv_strength[0] > 63U)) ||
        !ccso_parameters_valid(frame) ||
        !dimensions_valid(planes, frame) ||
        !dimensions_valid(&workspace->deblocked, frame) ||
        !dimensions_valid(&workspace->cdef, frame) ||
        !deblock_map_valid(&workspace->deblocked,
                           &workspace->deblock_map) ||
        !controls_valid(frame, &workspace->ccso_controls)) {
        av2h_debug_filter_reason = 6U;
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (planes->plane[plane].data ==
                workspace->deblocked.plane[plane].data ||
            planes->plane[plane].data == workspace->cdef.plane[plane].data ||
            workspace->deblocked.plane[plane].data ==
                workspace->cdef.plane[plane].data) {
            av2h_debug_filter_reason = 7U;
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        copy_filter_plane(&workspace->deblocked.plane[plane],
                          &planes->plane[plane]);
    }
    capture_filter_stage(0U, planes);
    av2h_debug_filter_stage_planes[0] = *planes;
    status = av2h_deblock_filter_frame(
        &workspace->deblocked, sequence, frame,
        &workspace->deblock_map);
    if (status != AV2H_OK) return status;
    capture_filter_stage(1U, &workspace->deblocked);
    av2h_debug_filter_stage_planes[1] = workspace->deblocked;
    status = av2h_cdef_filter_frame_with_controls(
        &workspace->cdef, &workspace->deblocked, frame,
        &workspace->cdef_controls);
    if (status != AV2H_OK) return status;
    capture_filter_stage(2U, &workspace->cdef);
    av2h_debug_filter_stage_planes[2] = workspace->cdef;
    status = av2h_ccso_filter_frame(
        &workspace->cdef, workspace->deblocked.plane[0].data,
        workspace->deblocked.plane[0].stride, frame,
        &workspace->ccso_controls);
    if (status != AV2H_OK) return status;
    capture_filter_stage(3U, &workspace->cdef);
    av2h_debug_filter_stage_planes[3] = workspace->cdef;
    status = av2h_gdf_filter_frame_with_controls(
        &planes->plane[0], &workspace->cdef.plane[0],
        &workspace->deblocked.plane[0], frame,
        &workspace->gdf_controls);
    if (status != AV2H_OK) return status;
    for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        copy_filter_plane(&planes->plane[plane],
                          &workspace->cdef.plane[plane]);
    }
    capture_filter_stage(4U, planes);
    return AV2H_OK;
}