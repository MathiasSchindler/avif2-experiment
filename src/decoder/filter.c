#include "decoder/filter.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

uint8_t av2h_debug_filter_reason;
uint64_t av2h_debug_filter_stage_hash[
    AV2H_FILTER_STAGE_COUNT][AV2H_FRAME_PLANE_COUNT];
Av2hFilterPlanes av2h_debug_filter_stage_planes[4U];

static uint64_t filter_plane_hash(const Av2hFilterPlane *plane) {
    uint64_t hash = 1469598103934665603ULL;
    uint16_t y;
    for (y = 0U; y < plane->height; ++y) {
        uint16_t x;
        for (x = 0U; x < plane->width; ++x) {
            hash ^= plane->data[(size_t)y * plane->stride + x];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

static void capture_filter_stage(size_t stage,
                                 const Av2hFilterPlanes *planes) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        av2h_debug_filter_stage_hash[stage][plane] =
            filter_plane_hash(&planes->plane[plane]);
    }
}

Av2hStatus av2h_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFilterWorkspace *workspace) {
    unsigned int plane;
    Av2hStatus status;
    av2h_debug_filter_reason = 0U;
    av2h_memory_fill(
        av2h_debug_filter_stage_hash, 0U,
        sizeof(av2h_debug_filter_stage_hash));
    if (sequence == 0 || frame == 0 || planes == 0 || workspace == 0) {
        av2h_debug_filter_reason = 1U;
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration != 0U) {
        av2h_debug_filter_reason = 2U;
        return AV2H_UNSUPPORTED;
    }
    if (frame->cdef_strength_count != 0U &&
        (frame->cdef_on_skip_transform == 0U ||
         frame->cdef_damping < 3U)) {
        av2h_debug_filter_reason = 5U;
        return AV2H_UNSUPPORTED;
    }
    if ((frame->cdef_strength_count != 0U &&
         sequence->enable_cdef == 0U) ||
        ((frame->ccso[0].enabled != 0U ||
          frame->ccso[1].enabled != 0U ||
          frame->ccso[2].enabled != 0U) &&
         sequence->enable_ccso == 0U) ||
        (frame->gdf_mode != 0U && sequence->enable_gdf == 0U) ||
        (frame->cdef_strength_count != 0U &&
         (frame->cdef_damping > 6U ||
          frame->cdef_y_strength[0] > 63U ||
          frame->cdef_uv_strength[0] > 63U)) ||
        !av2h_ccso_parameters_valid(frame) ||
        !filter_dimensions_valid(planes, frame) ||
        !filter_dimensions_valid(&workspace->deblocked, frame) ||
        !filter_dimensions_valid(&workspace->cdef, frame) ||
        !av2h_deblock_map_valid(&workspace->deblocked,
                           &workspace->deblock_map) ||
        !av2h_ccso_controls_valid(frame, &workspace->ccso_controls)) {
        av2h_debug_filter_reason = 6U;
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (planes->plane[plane].data ==
                workspace->deblocked.plane[plane].data ||
            planes->plane[plane].data == workspace->cdef.plane[plane].data ||
            workspace->deblocked.plane[plane].data ==
                workspace->cdef.plane[plane].data) {
            av2h_debug_filter_reason = 7U;
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        filter_copy_plane(&workspace->deblocked.plane[plane],
                          &planes->plane[plane]);
    }
    capture_filter_stage(0U, planes);
    av2h_debug_filter_stage_planes[0] = *planes;
    status = av2h_deblock_filter_frame(
        &workspace->deblocked, sequence, frame,
        &workspace->deblock_map);
    if (status != AV2H_OK) return status;
    capture_filter_stage(1U, &workspace->deblocked);
    av2h_debug_filter_stage_planes[1] = workspace->deblocked;
    status = av2h_cdef_filter_frame_with_controls(
        &workspace->cdef, &workspace->deblocked, frame,
        &workspace->cdef_controls);
    if (status != AV2H_OK) return status;
    capture_filter_stage(2U, &workspace->cdef);
    av2h_debug_filter_stage_planes[2] = workspace->cdef;
    status = av2h_ccso_filter_frame(
        &workspace->cdef, workspace->deblocked.plane[0].data,
        workspace->deblocked.plane[0].stride, frame,
        &workspace->ccso_controls);
    if (status != AV2H_OK) return status;
    capture_filter_stage(3U, &workspace->cdef);
    av2h_debug_filter_stage_planes[3] = workspace->cdef;
    status = av2h_gdf_filter_frame_with_controls(
        &planes->plane[0], &workspace->cdef.plane[0],
        &workspace->deblocked.plane[0], frame,
        &workspace->gdf_controls);
    if (status != AV2H_OK) return status;
    for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        filter_copy_plane(&planes->plane[plane],
                          &workspace->cdef.plane[plane]);
    }
    capture_filter_stage(4U, planes);
    return AV2H_OK;
}