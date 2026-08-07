#ifndef AV2H_DECODER_INTERNAL_FILTER_COMMON_H
#define AV2H_DECODER_INTERNAL_FILTER_COMMON_H

#include "decoder/filter.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/sample_math.h"

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
            planes->plane[plane].stride < width) {
            return 0;
        }
    }
    return 1;
}

static inline void filter_copy_plane(
    Av2hFilterPlane *destination, const Av2hFilterPlane *source) {
    uint16_t row;
    for (row = 0U; row < source->height; ++row) {
        uint16_t column;
        for (column = 0U; column < source->width; ++column) {
            destination->data[(size_t)row * destination->stride + column] =
                source->data[(size_t)row * source->stride + column];
        }
    }
}

#endif