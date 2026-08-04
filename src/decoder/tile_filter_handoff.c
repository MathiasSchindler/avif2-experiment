#include "internal/tile_modules.h"
#include "internal/filter_geometry.h"

static int ccso_entropy_complete(const Av2hCcsoEntropyState *state) {
    size_t plane;
    size_t index;
    if (state == 0) return 0;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (state->unit_controls[plane] == 0) return 0;
        for (index = 0U; index < state->unit_count; ++index) {
            if (state->unit_controls[plane][index] ==
                AV2H_CCSO_UNTRANSMITTED) {
                return 0;
            }
        }
    }
    return 1;
}

Av2hStatus av2h_tile_filter_controls_handoff(
    const Av2hTileDecoder *decoder,
    Av2hTileFilterControls *controls) {
    uint16_t ccso_unit_size;
    size_t plane;
    if (decoder == 0 || controls == 0) return AV2H_INVALID_ARGUMENT;
    av2h_memory_fill(controls, 0U, sizeof(*controls));
    if (decoder->frame->gdf_mode >= 2U &&
        decoder->gdf_entropy.decoded_count !=
            decoder->frame->gdf_block_count) {
        return AV2H_UNSUPPORTED;
    }
    if (!ccso_entropy_complete(&decoder->ccso_entropy)) {
        return AV2H_UNSUPPORTED;
    }
    controls->cdef.strengths = decoder->cdef_entropy.unit_strengths;
    controls->cdef.columns = decoder->cdef_entropy.unit_columns;
    controls->cdef.rows = decoder->cdef_entropy.unit_rows;
    controls->gdf.flags = decoder->gdf_entropy.block_flags;
    controls->gdf.stride = decoder->gdf_entropy.block_columns;
    controls->gdf.columns = decoder->gdf_entropy.block_columns;
    controls->gdf.rows = decoder->gdf_entropy.block_rows;
    controls->gdf.block_size = decoder->gdf_entropy.block_size;
    ccso_unit_size = av2h_filter_ccso_unit_size(decoder->frame);
    controls->ccso.columns =
        (av2h_filter_coded_dimension(decoder->frame->width) +
         ccso_unit_size - 1U) / ccso_unit_size;
    controls->ccso.rows =
        (av2h_filter_coded_dimension(decoder->frame->height) +
         ccso_unit_size - 1U) / ccso_unit_size;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        controls->ccso.plane[plane] =
            decoder->ccso_entropy.unit_controls[plane];
        controls->ccso.stride[plane] = controls->ccso.columns;
    }
    return AV2H_OK;
}