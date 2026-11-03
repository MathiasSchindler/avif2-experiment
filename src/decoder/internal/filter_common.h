#ifndef AV2H_DECODER_INTERNAL_FILTER_COMMON_H
#define AV2H_DECODER_INTERNAL_FILTER_COMMON_H

#include "decoder/filter.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/sample_math.h"
#include "decoder/internal/sample_ops.h"

#include <stddef.h>

static inline int filter_rows_valid(size_t begin, size_t end) {
    return begin <= UINT16_MAX && begin % 64U == 0U && begin <= end &&
           (end == SIZE_MAX || end % 64U == 0U);
}

static inline int filter_plane_extent(const void *data, size_t stride, size_t width, size_t height,
                                      size_t sample_bytes, uintptr_t *end) {
    size_t bytes;
    size_t row_bytes;
    uintptr_t start = (uintptr_t)data;
    if (!av2h_size_multiply(height - 1U, stride, &bytes) ||
        !av2h_size_multiply(bytes, sample_bytes, &bytes) ||
        !av2h_size_multiply(width, sample_bytes, &row_bytes) ||
        !av2h_size_add(bytes, row_bytes, &bytes) || bytes > (uintptr_t)-1 ||
        start > (uintptr_t)-1 - bytes) {
        return 0;
    }
    *end = start + bytes;
    return 1;
}

/* Instantiate typed kernels after the shared helpers. */
#include "sample_kernel_u8.inc"
#include "filter_common_kernel.inc"
#include "sample_kernel_end.inc"
#include "sample_kernel_u16.inc"
#include "filter_common_kernel.inc"
#include "sample_kernel_end.inc"

#endif