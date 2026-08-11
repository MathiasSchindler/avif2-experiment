#ifndef AV2H_DECODER_INTERNAL_FILTER_COMMON_H
#define AV2H_DECODER_INTERNAL_FILTER_COMMON_H

#include "decoder/filter.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/sample_math.h"
#include "decoder/internal/sample_ops.h"

static inline int absolute_value(int value) {
    return value < 0 ? -value : value;
}

static inline int round_shift_signed(int value, unsigned int shift) {
    int adjusted = value + (1 << (shift - 1U));
    if (adjusted >= 0) return adjusted >> shift;
    return -(int)(((unsigned int)(-adjusted) +
                  ((1U << shift) - 1U)) >> shift);
}

static inline int round_shift_symmetric(int value, unsigned int shift) {
    unsigned int half = 1U << (shift - 1U);
    if (value >= 0) return (value + (int)half) >> shift;
    return -(int)(((unsigned int)(-value) + half) >> shift);
}

static inline int floor_shift_signed(int value, unsigned int shift) {
    if (value >= 0) return value >> shift;
    return -(int)(((unsigned int)(-value) +
                  ((1U << shift) - 1U)) >> shift);
}

static inline int filter_dimensions_valid(
    const Av2hFilterPlanes *planes, const Av2hFrameState *frame) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t width = av2h_frame_plane_dimension(
            frame, plane, av2h_filter_coded_dimension(frame->width));
        size_t height = av2h_frame_plane_dimension(
            frame, plane, av2h_filter_coded_dimension(frame->height));
        if (planes->plane[plane].data == 0 ||
            planes->plane[plane].width != width ||
            planes->plane[plane].height != height ||
            planes->plane[plane].stride_samples < width) {
            return 0;
        }
    }
    return 1;
}

static inline int filter_dimensions_valid_u16(
    const Av2hFilterPlanesU16 *planes, const Av2hFrameState *frame) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t width = av2h_frame_plane_dimension(
            frame, plane, av2h_filter_coded_dimension(frame->width));
        size_t height = av2h_frame_plane_dimension(
            frame, plane, av2h_filter_coded_dimension(frame->height));
        if (planes->plane[plane].data == 0 ||
            planes->plane[plane].width != width ||
            planes->plane[plane].height != height ||
            planes->plane[plane].stride_samples < width) {
            return 0;
        }
    }
    return 1;
}

static inline void filter_copy_plane(
    Av2hFilterPlane *destination, const Av2hFilterPlane *source) {
    av2h_plane_copy_u8(
        destination->data, destination->stride_samples,
        source->data, source->stride_samples,
        source->width, source->height);
}

static inline void filter_copy_plane_u16(
    Av2hFilterPlaneU16 *destination, const Av2hFilterPlaneU16 *source) {
    av2h_plane_copy_u16(
        destination->data, destination->stride_samples,
        source->data, source->stride_samples,
        source->width, source->height);
}

static inline int filter_plane_rows_overlap_u16(
    const Av2hFilterPlaneU16 *left,
    const Av2hFilterPlaneU16 *right) {
    size_t left_row = 0U;
    size_t right_row = 0U;
    uintptr_t pointer_max = (uintptr_t)-1;
    while (left_row < left->height && right_row < right->height) {
        size_t left_offset;
        size_t right_offset;
        size_t left_width;
        size_t right_width;
        uintptr_t left_start;
        uintptr_t left_end;
        uintptr_t right_start;
        uintptr_t right_end;
        if (!av2h_size_multiply(
                left_row, left->stride_samples, &left_offset) ||
            !av2h_size_multiply(left_offset, sizeof(uint16_t), &left_offset) ||
            !av2h_size_multiply(
                right_row, right->stride_samples, &right_offset) ||
            !av2h_size_multiply(
                right_offset, sizeof(uint16_t), &right_offset) ||
            !av2h_size_multiply(
                left->width, sizeof(uint16_t), &left_width) ||
            !av2h_size_multiply(
                right->width, sizeof(uint16_t), &right_width) ||
            left_offset > (size_t)pointer_max ||
            right_offset > (size_t)pointer_max ||
            (uintptr_t)left->data > pointer_max - (uintptr_t)left_offset ||
            (uintptr_t)right->data > pointer_max - (uintptr_t)right_offset) {
            return 1;
        }
        left_start = (uintptr_t)left->data + (uintptr_t)left_offset;
        right_start = (uintptr_t)right->data + (uintptr_t)right_offset;
        if ((uintptr_t)left_width > pointer_max - left_start ||
            (uintptr_t)right_width > pointer_max - right_start) {
            return 1;
        }
        left_end = left_start + (uintptr_t)left_width;
        right_end = right_start + (uintptr_t)right_width;
        if (left_start < right_end && right_start < left_end) return 1;
        if (left_end <= right_start) {
            ++left_row;
        } else {
            ++right_row;
        }
    }
    return 0;
}

#endif