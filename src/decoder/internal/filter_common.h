#ifndef AV2H_DECODER_INTERNAL_FILTER_COMMON_H
#define AV2H_DECODER_INTERNAL_FILTER_COMMON_H

#include "decoder/filter.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/sample_math.h"
#include "decoder/internal/sample_ops.h"

#include <stddef.h>

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

/* Instantiate typed kernels after the shared helpers. */
#include "sample_kernel_u8.inc"
#include "filter_common_kernel.inc"
#include "sample_kernel_end.inc"
#include "sample_kernel_u16.inc"
#include "filter_common_kernel.inc"
#include "sample_kernel_end.inc"

#endif