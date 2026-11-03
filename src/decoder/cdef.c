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
    if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U) {
        return bounds;
    }
    for (column = 0U; column < frame->tile_column_count; ++column) {
        uint32_t end =
            (uint32_t)frame->tile_column_start_sb[column + 1U] * sequence->superblock_size;
        if (x < end || column + 1U == frame->tile_column_count) {
            bounds.left =
                (uint16_t)(frame->tile_column_start_sb[column] * sequence->superblock_size);
            bounds.right = (uint16_t)(end < frame->width ? end : frame->width);
            break;
        }
    }
    for (row = 0U; row < frame->tile_row_count; ++row) {
        uint32_t end = (uint32_t)frame->tile_row_start_sb[row + 1U] * sequence->superblock_size;
        if (y < end || row + 1U == frame->tile_row_count) {
            bounds.top = (uint16_t)(frame->tile_row_start_sb[row] * sequence->superblock_size);
            bounds.bottom = (uint16_t)(end < frame->height ? end : frame->height);
            break;
        }
    }
    return bounds;
}

static int cdef_constrain(int difference, unsigned int threshold, unsigned int damping) {
    unsigned int magnitude;
    unsigned int adjustment;
    int constrained;
    if (threshold == 0U)
        return 0;
    magnitude = (unsigned int)av2h_sample_abs(difference);
    adjustment = av2h_floor_log2_u32(threshold);
    adjustment = damping > adjustment ? damping - adjustment : 0U;
    constrained = (int)threshold - (int)(magnitude >> adjustment);
    constrained = av2h_sample_clamp(constrained, 0, (int)magnitude);
    return difference < 0 ? -constrained : constrained;
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/cdef_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/cdef_kernel.inc"
#include "internal/sample_kernel_end.inc"
