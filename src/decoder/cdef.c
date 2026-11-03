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

static Av2hFilterPlane cdef_plane_view(const Av2hFilterPlane *plane, const CdefTileBounds *bounds,
                                       unsigned int shift);
static Av2hFilterPlaneU16 cdef_plane_view_u16(const Av2hFilterPlaneU16 *plane,
                                              const CdefTileBounds *bounds, unsigned int shift);
Av2hStatus av2h_cdef_find_direction(const Av2hFilterPlane *luma, uint16_t x, uint16_t y,
                                    uint8_t *direction, uint32_t *variance);
Av2hStatus av2h_cdef_find_direction_u16(const Av2hFilterPlaneU16 *luma, uint16_t x, uint16_t y,
                                        uint8_t *direction, uint32_t *variance,
                                        const Av2hSampleDomain *domain);
static int cdef_tap(const Av2hFilterPlane *source, int x, int y, int center, unsigned int strength,
                    unsigned int damping, int *minimum, int *maximum, int clipping_required,
                    int weight);
static int cdef_tap_u16(const Av2hFilterPlaneU16 *source, int x, int y, int center,
                        unsigned int strength, unsigned int damping, int *minimum, int *maximum,
                        int clipping_required, int weight);
Av2hStatus av2h_cdef_filter_block(Av2hFilterPlane *destination, const Av2hFilterPlane *source,
                                  uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                  unsigned int plane_index, uint8_t primary_strength,
                                  uint8_t secondary_strength, uint8_t damping, uint8_t direction,
                                  uint32_t variance);
Av2hStatus av2h_cdef_filter_block_u16(Av2hFilterPlaneU16 *destination,
                                      const Av2hFilterPlaneU16 *source, uint16_t x, uint16_t y,
                                      uint16_t width, uint16_t height, unsigned int plane_index,
                                      uint8_t primary_strength, uint8_t secondary_strength,
                                      uint8_t damping, uint8_t direction, uint32_t variance,
                                      const Av2hSampleDomain *domain);
static Av2hStatus cdef_partial_direction(const Av2hFilterPlane *luma, uint16_t x, uint16_t y,
                                         uint8_t *direction, uint32_t *variance);
static Av2hStatus cdef_partial_direction_u16(const Av2hFilterPlaneU16 *luma, uint16_t x, uint16_t y,
                                             uint8_t *direction, uint32_t *variance,
                                             const Av2hSampleDomain *domain);
Av2hStatus av2h_cdef_filter_frame(Av2hFilterPlanes *destination, const Av2hFilterPlanes *deblocked,
                                  const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                  const Av2hCdefControlMap *controls);
Av2hStatus
av2h_cdef_filter_frame_u16(Av2hFilterPlanesU16 *destination, const Av2hFilterPlanesU16 *deblocked,
                           const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                           const Av2hCdefControlMap *controls, const Av2hSampleDomain *domain);

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

#include "internal/sample_kernel_u8.inc"
#include "internal/cdef_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/cdef_kernel.inc"
#include "internal/sample_kernel_end.inc"
