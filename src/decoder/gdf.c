#include "decoder/filter.h"

#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

#include <stddef.h>

#include "gdf_data.inc"

static const int8_t gdf_coordinates[18][2] = {{6, 0},  {5, 0}, {4, 0}, {3, 0}, {2, 1},  {2, 0},
                                              {2, -1}, {1, 2}, {1, 1}, {1, 0}, {1, -1}, {1, -2},
                                              {0, 6},  {0, 5}, {0, 4}, {0, 3}, {0, 2},  {0, 1}};

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} GdfTileBounds;

static GdfTileBounds gdf_tile_bounds(const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                     uint16_t x, uint16_t y) {
    GdfTileBounds bounds = {0, 0, frame->width, frame->height};
    unsigned int column;
    unsigned int row;
    if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U) {
        return bounds;
    }
    for (column = 0U; column < frame->tile_column_count; ++column) {
        int end = (int)frame->tile_column_start_sb[column + 1U] * sequence->superblock_size;
        if ((int)x < end || column + 1U == frame->tile_column_count) {
            bounds.left = (int)frame->tile_column_start_sb[column] * sequence->superblock_size;
            bounds.right = end < frame->width ? end : frame->width;
            break;
        }
    }
    for (row = 0U; row < frame->tile_row_count; ++row) {
        int end = (int)frame->tile_row_start_sb[row + 1U] * sequence->superblock_size;
        if ((int)y < end || row + 1U == frame->tile_row_count) {
            bounds.top = (int)frame->tile_row_start_sb[row] * sequence->superblock_size;
            bounds.bottom = end < frame->height ? end : frame->height;
            break;
        }
    }
    return bounds;
}

static size_t gdf_alpha_index(unsigned int qp_index, unsigned int kernel_index,
                              unsigned int class_index) {
    return (((size_t)qp_index * 22U + kernel_index) * 4U) + class_index;
}

static size_t gdf_weight_index(unsigned int qp_index, unsigned int output_index,
                               unsigned int kernel_index, unsigned int class_index) {
    return (((((size_t)qp_index * 3U + output_index) * 22U) + kernel_index) * 4U) + class_index;
}

static size_t gdf_bias_index(unsigned int qp_index, unsigned int output_index) {
    return (size_t)qp_index * 3U + output_index;
}

static unsigned int gdf_training_qp_index(const Av2hFrameState *frame, unsigned int bit_depth) {
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

static unsigned int gdf_gradient_sum(const uint16_t gradient[4][6][6], unsigned int direction,
                                     unsigned int row, unsigned int column) {
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

static int gdf_frame_arguments_valid_u16(const Av2hFilterPlaneU16 *destination,
                                         const Av2hFilterPlaneU16 *cdef_luma,
                                         const Av2hFilterPlaneU16 *deblocked_luma,
                                         const Av2hFrameState *frame) {
    return !(destination == NULL || cdef_luma == NULL || deblocked_luma == NULL || frame == NULL ||
             destination->data == NULL || cdef_luma->data == NULL || deblocked_luma->data == NULL ||
             destination->stride_samples < destination->width ||
             cdef_luma->stride_samples < cdef_luma->width ||
             deblocked_luma->stride_samples < deblocked_luma->width || destination->width == 0U ||
             destination->height == 0U || destination->width != cdef_luma->width ||
             destination->height != cdef_luma->height ||
             destination->width != deblocked_luma->width ||
             destination->height != deblocked_luma->height ||
             filter_plane_rows_overlap_u16(destination, cdef_luma) ||
             filter_plane_rows_overlap_u16(destination, deblocked_luma) ||
             filter_plane_rows_overlap_u16(cdef_luma, deblocked_luma) || frame->gdf_mode > 2U ||
             frame->gdf_qp_index >= 4U || frame->gdf_scale_index >= 4U ||
             (destination->width & 3U) != 0U || (destination->height & 3U) != 0U);
}

static int gdf_frame_arguments_valid(const Av2hFilterPlane *destination,
                                     const Av2hFilterPlane *baseline_luma,
                                     const Av2hFilterPlane *cdef_luma,
                                     const Av2hFilterPlane *deblocked_luma,
                                     const Av2hFrameState *frame) {
    return !(destination == NULL || baseline_luma == NULL || cdef_luma == NULL ||
             deblocked_luma == NULL || frame == NULL || destination->data == NULL ||
             baseline_luma->data == NULL || cdef_luma->data == NULL ||
             deblocked_luma->data == NULL || destination->stride_samples < destination->width ||
             baseline_luma->stride_samples < baseline_luma->width ||
             cdef_luma->stride_samples < cdef_luma->width ||
             deblocked_luma->stride_samples < deblocked_luma->width || destination->width == 0U ||
             destination->height == 0U || destination->width != baseline_luma->width ||
             destination->height != baseline_luma->height ||
             destination->width != cdef_luma->width || destination->height != cdef_luma->height ||
             destination->width != deblocked_luma->width ||
             destination->height != deblocked_luma->height ||
             filter_plane_rows_overlap(destination, baseline_luma) ||
             filter_plane_rows_overlap(destination, cdef_luma) ||
             filter_plane_rows_overlap(destination, deblocked_luma) ||
             (baseline_luma->data != cdef_luma->data &&
              filter_plane_rows_overlap(baseline_luma, cdef_luma)) ||
             filter_plane_rows_overlap(baseline_luma, deblocked_luma) ||
             filter_plane_rows_overlap(cdef_luma, deblocked_luma) || frame->gdf_mode > 2U ||
             frame->gdf_qp_index >= 4U || frame->gdf_scale_index >= 4U ||
             (destination->width & 3U) != 0U || (destination->height & 3U) != 0U);
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/gdf_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/gdf_kernel.inc"
#include "internal/sample_kernel_end.inc"
