#include "internal/tile_modules.h"

#include "internal/filter_geometry.h"

#include <stddef.h>

static int ccso_entropy_complete(const Av2hCcsoEntropyState *state) {
    size_t plane;
    size_t index;
    if (state == NULL) return 0;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (state->unit_controls[plane] == NULL) return 0;
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
    if (decoder == NULL || controls == NULL) return AV2H_INVALID_ARGUMENT;
    av2h_memory_fill(controls, 0U, sizeof(*controls));
    if (decoder->entropy_state.frame->gdf_mode >= 2U &&
        decoder->entropy_state.gdf_entropy.decoded_count !=
            decoder->entropy_state.frame->gdf_block_count) {
        return AV2H_UNSUPPORTED;
    }
    if (decoder->entropy_state.restoration_entropy.active_mask != 0U &&
        decoder->entropy_state.restoration_entropy.decoded_count[0] !=
            decoder->entropy_state.frame->restoration_unit_count[0]) {
        return AV2H_UNSUPPORTED;
    }
    if (!ccso_entropy_complete(&decoder->entropy_state.ccso_entropy)) {
        return AV2H_UNSUPPORTED;
    }
    controls->cdef.strengths = decoder->entropy_state.cdef_entropy.unit_strengths;
    controls->cdef.columns = decoder->entropy_state.cdef_entropy.unit_columns;
    controls->cdef.rows = decoder->entropy_state.cdef_entropy.unit_rows;
    controls->gdf.flags = decoder->entropy_state.gdf_entropy.block_flags;
    controls->gdf.stride_blocks = decoder->entropy_state.gdf_entropy.block_columns;
    controls->gdf.columns = decoder->entropy_state.gdf_entropy.block_columns;
    controls->gdf.rows = decoder->entropy_state.gdf_entropy.block_rows;
    controls->gdf.block_size = decoder->entropy_state.gdf_entropy.block_size;
    ccso_unit_size = av2h_filter_ccso_unit_size(decoder->entropy_state.frame);
    controls->ccso.columns =
        (av2h_filter_coded_dimension(decoder->entropy_state.frame->width) +
         ccso_unit_size - 1U) / ccso_unit_size;
    controls->ccso.rows =
        (av2h_filter_coded_dimension(decoder->entropy_state.frame->height) +
         ccso_unit_size - 1U) / ccso_unit_size;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        controls->ccso.plane[plane] =
            decoder->entropy_state.ccso_entropy.unit_controls[plane];
        controls->ccso.stride_units[plane] = controls->ccso.columns;
    }
    return AV2H_OK;
}