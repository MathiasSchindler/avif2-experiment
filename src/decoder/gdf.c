#include "decoder/filter.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

#include "gdf_data.inc"

static const int8_t gdf_coordinates[18][2] = {
    { 6, 0 }, { 5, 0 }, { 4, 0 }, { 3, 0 },
    { 2, 1 }, { 2, 0 }, { 2, -1 },
    { 1, 2 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 1, -2 },
    { 0, 6 }, { 0, 5 }, { 0, 4 }, { 0, 3 }, { 0, 2 }, { 0, 1 }
};

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

int16_t av2h_deblock_side_threshold(size_t index) {
    return deblock_side_threshold[index];
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
                    above = av2h_sample_clamp(
                        above, -(int)alpha, (int)alpha);
                    below = av2h_sample_clamp(
                        below, -(int)alpha, (int)alpha);
                    combination = av2h_sample_clamp(
                        above + below, -512, 511);
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
                        (av2h_sample_clamp(value, -8, 7) + 8);
                }
                destination->data[destination_index] = av2h_sample_clip(
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
            if (frame->gdf_mode == 2U) {
                size_t control_row =
                    ((size_t)row + 8U) / controls->block_size;
                if (control_row >= controls->rows) {
                    control_row = controls->rows - 1U;
                }
                if (controls->flags[
                        control_row * controls->stride +
                        (size_t)column / controls->block_size] == 0U) {
                    continue;
                }
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

