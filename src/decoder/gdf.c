#include "decoder/filter.h"

#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

#include <stddef.h>

#include "gdf_data.inc"

static const int8_t gdf_coordinates[18][2] = {
    { 6, 0 }, { 5, 0 }, { 4, 0 }, { 3, 0 },
    { 2, 1 }, { 2, 0 }, { 2, -1 },
    { 1, 2 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 1, -2 },
    { 0, 6 }, { 0, 5 }, { 0, 4 }, { 0, 3 }, { 0, 2 }, { 0, 1 }
};

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} GdfTileBounds;

static uint8_t gdf_source_sample(const Av2hFilterPlane *cdef_luma,
                                 const Av2hFilterPlane *deblocked_luma, int x, int y,
                                 int stripe_start, int stripe_end, const GdfTileBounds *bounds);
static uint16_t gdf_source_sample_u16(const Av2hFilterPlaneU16 *cdef_luma,
                                      const Av2hFilterPlaneU16 *deblocked_luma, int x, int y,
                                      int stripe_start, int stripe_end,
                                      const GdfTileBounds *bounds);
static void gdf_filter_block(Av2hFilterPlane *destination, const Av2hFilterPlane *cdef_luma,
                             const Av2hFilterPlane *deblocked_luma, uint16_t block_x,
                             uint16_t block_y, unsigned int qp_index, unsigned int pixel_scale,
                             const GdfTileBounds *bounds);
static void gdf_filter_block_u16(Av2hFilterPlaneU16 *destination,
                                 const Av2hFilterPlaneU16 *cdef_luma,
                                 const Av2hFilterPlaneU16 *deblocked_luma, uint16_t block_x,
                                 uint16_t block_y, unsigned int qp_index, unsigned int pixel_scale,
                                 const GdfTileBounds *bounds, const Av2hSampleDomain *domain);
Av2hStatus av2h_gdf_filter_frame(Av2hFilterPlane *destination, const Av2hFilterPlane *baseline_luma,
                                 const Av2hFilterPlane *cdef_luma,
                                 const Av2hFilterPlane *deblocked_luma,
                                 const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                 const Av2hGdfControlMap *controls);
Av2hStatus av2h_gdf_filter_frame_u16(Av2hFilterPlaneU16 *destination,
                                     const Av2hFilterPlaneU16 *cdef_luma,
                                     const Av2hFilterPlaneU16 *deblocked_luma,
                                     const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                     const Av2hGdfControlMap *controls,
                                     const Av2hSampleDomain *domain);

static GdfTileBounds gdf_tile_bounds(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint16_t x,
    uint16_t y) {
    GdfTileBounds bounds = { 0, 0, frame->width, frame->height };
    unsigned int column;
    unsigned int row;
    if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U) {
        return bounds;
    }
    for (column = 0U; column < frame->tile_column_count; ++column) {
        int end = (int)frame->tile_column_start_sb[column + 1U] *
            sequence->superblock_size;
        if ((int)x < end || column + 1U == frame->tile_column_count) {
            bounds.left = (int)frame->tile_column_start_sb[column] *
                sequence->superblock_size;
            bounds.right = end < frame->width ? end : frame->width;
            break;
        }
    }
    for (row = 0U; row < frame->tile_row_count; ++row) {
        int end = (int)frame->tile_row_start_sb[row + 1U] *
            sequence->superblock_size;
        if ((int)y < end || row + 1U == frame->tile_row_count) {
            bounds.top = (int)frame->tile_row_start_sb[row] *
                sequence->superblock_size;
            bounds.bottom = end < frame->height ? end : frame->height;
            break;
        }
    }
    return bounds;
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

static unsigned int gdf_training_qp_index(const Av2hFrameState *frame,
                                          unsigned int bit_depth) {
    unsigned int base_index = 0U;
    unsigned int qp_offset = 24U * (bit_depth - 8U);
    if (frame->base_q_index >= 172U + qp_offset) {
        base_index = 2U;
    } else if (frame->base_q_index >= 147U + qp_offset) {
        base_index = 1U;
    }
    return base_index + frame->gdf_qp_index;
}

int16_t av2h_deblock_side_threshold(size_t index) {
    return deblock_side_threshold[index];
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
        if (!av2h_size_multiply(left_row, left->stride_samples, &left_offset) ||
            !av2h_size_multiply(right_row, right->stride_samples, &right_offset) ||
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

#include "internal/sample_kernel_u8.inc"
#include "internal/gdf_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/gdf_kernel.inc"
#include "internal/sample_kernel_end.inc"
