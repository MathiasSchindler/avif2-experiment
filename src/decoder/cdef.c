#include "decoder/filter.h"

#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"
#include "shared/bit_math.h"

#include <stddef.h>

typedef struct {
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
} CdefTileBounds;

static CdefTileBounds cdef_tile_bounds(const Av2hSequenceState *sequence,
                                       const Av2hFrameState *frame, uint16_t x, uint16_t y) {
    CdefTileBounds bounds = {0U, 0U, frame->width, frame->height};
    unsigned int column;
    unsigned int row;
    unsigned int max_columns = (unsigned int)(sizeof(frame->tile_column_start_sb) /
                                                  sizeof(frame->tile_column_start_sb[0]) -
                                              1U);
    unsigned int max_rows =
        (unsigned int)(sizeof(frame->tile_row_start_sb) / sizeof(frame->tile_row_start_sb[0]) - 1U);
    unsigned int column_count =
        frame->tile_column_count <= max_columns ? frame->tile_column_count : max_columns;
    unsigned int row_count = frame->tile_row_count <= max_rows ? frame->tile_row_count : max_rows;
    if (sequence != NULL && sequence->disable_loopfilters_across_tiles != 0U) {
        for (column = 0U; column < column_count; ++column) {
            uint32_t start =
                (uint32_t)frame->tile_column_start_sb[column] * sequence->superblock_size;
            uint32_t end =
                (uint32_t)frame->tile_column_start_sb[column + 1U] * sequence->superblock_size;
            if (x < end || column + 1U == column_count) {
                bounds.left = (uint16_t)(start <= x ? start : x);
                bounds.right = (uint16_t)(end < frame->width ? end : frame->width);
                break;
            }
        }
        for (row = 0U; row < row_count; ++row) {
            uint32_t start = (uint32_t)frame->tile_row_start_sb[row] * sequence->superblock_size;
            uint32_t end = (uint32_t)frame->tile_row_start_sb[row + 1U] * sequence->superblock_size;
            if (y < end || row + 1U == row_count) {
                bounds.top = (uint16_t)(start <= y ? start : y);
                bounds.bottom = (uint16_t)(end < frame->height ? end : frame->height);
                break;
            }
        }
    }
    if (bounds.right <= x)
        bounds.right = frame->width;
    if (bounds.left > x)
        bounds.left = 0U;
    if (bounds.bottom <= y)
        bounds.bottom = frame->height;
    if (bounds.top > y)
        bounds.top = 0U;
    return bounds;
}

static int cdef_constrain(int difference, unsigned int threshold, unsigned int damping) {
    uint32_t magnitude;
    unsigned int adjustment;
    uint32_t shifted;
    uint32_t constrained;
    if (threshold == 0U)
        return 0;
    magnitude = av2h_sample_abs_u32(difference);
    adjustment = av2h_floor_log2_u32(threshold);
    adjustment = damping > adjustment ? damping - adjustment : 0U;
    shifted = adjustment >= 32U ? 0U : (magnitude >> adjustment);
    if (shifted >= threshold)
        return 0;
    constrained = threshold - shifted;
    if (constrained > magnitude)
        constrained = magnitude;
    if (constrained > (uint32_t)INT32_MAX)
        constrained = (uint32_t)INT32_MAX;
    return difference < 0 ? -(int)constrained : (int)constrained;
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/cdef_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/cdef_kernel.inc"
#include "internal/sample_kernel_end.inc"
