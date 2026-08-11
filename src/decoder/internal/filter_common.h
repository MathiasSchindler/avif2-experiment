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

#endif