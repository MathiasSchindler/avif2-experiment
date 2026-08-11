#include "decoder/filter.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_diagnostics.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static Av2hFilterDiagnostics filter_diagnostics;
#endif

const Av2hFilterDiagnostics *av2h_filter_diagnostics_get(void) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return &filter_diagnostics;
#else
    return 0;
#endif
}

static void reset_filter_diagnostics(void) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    filter_diagnostics.reason = 0U;
#endif
}

static void set_filter_reason(uint8_t reason) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    filter_diagnostics.reason = reason;
#else
    (void)reason;
#endif
}

Av2hStatus av2h_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFilterWorkspace *workspace) {
    unsigned int plane;
    Av2hStatus status;
    reset_filter_diagnostics();
    if (sequence == 0 || frame == 0 || planes == 0 || workspace == 0) {
        set_filter_reason(1U);
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration != 0U) {
        set_filter_reason(2U);
        return AV2H_UNSUPPORTED;
    }
    if (frame->cdef_strength_count != 0U &&
        (frame->cdef_on_skip_transform == 0U ||
         frame->cdef_damping < 3U)) {
        set_filter_reason(5U);
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
        set_filter_reason(6U);
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (planes->plane[plane].data ==
                workspace->deblocked.plane[plane].data ||
            planes->plane[plane].data == workspace->cdef.plane[plane].data ||
            workspace->deblocked.plane[plane].data ==
                workspace->cdef.plane[plane].data) {
            set_filter_reason(7U);
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        filter_copy_plane(&workspace->deblocked.plane[plane],
                          &planes->plane[plane]);
    }
    status = av2h_deblock_filter_frame(
        &workspace->deblocked, sequence, frame,
        &workspace->deblock_map);
    if (status != AV2H_OK) return status;
    status = av2h_cdef_filter_frame_with_controls(
        &workspace->cdef, &workspace->deblocked, frame,
        &workspace->cdef_controls);
    if (status != AV2H_OK) return status;
    status = av2h_ccso_filter_frame(
        &workspace->cdef, workspace->deblocked.plane[0].data,
        workspace->deblocked.plane[0].stride_samples, frame,
        &workspace->ccso_controls);
    if (status != AV2H_OK) return status;
    status = av2h_gdf_filter_frame_with_controls(
        &planes->plane[0], &workspace->cdef.plane[0],
        &workspace->deblocked.plane[0], frame,
        &workspace->gdf_controls);
    if (status != AV2H_OK) return status;
    for (plane = 1U; plane < av2h_frame_plane_count(frame); ++plane) {
        filter_copy_plane(&planes->plane[plane],
                          &workspace->cdef.plane[plane]);
    }
    return AV2H_OK;
}

Av2hStatus av2h_filter_stage_u16(
    Av2hFilterPlanesU16 *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFilterWorkspace *workspace) {
    Av2hStatus status;
    const Av2hFilterPlaneU16 *stage_planes[AV2H_FRAME_PLANE_COUNT * 3U];
    size_t stage_plane_count = 0U;
    size_t left;
    unsigned int plane;
    if (planes == 0 || sequence == 0 || frame == 0 || workspace == 0 ||
        sequence->info.bit_depth_idc != 0U ||
        (frame->cdef_strength_count != 0U && sequence->enable_cdef == 0U) ||
        ((frame->ccso[0].enabled != 0U ||
          frame->ccso[1].enabled != 0U ||
          frame->ccso[2].enabled != 0U) && sequence->enable_ccso == 0U) ||
            (frame->gdf_mode != 0U && sequence->enable_gdf == 0U) ||
            !filter_dimensions_valid_u16(planes, frame) ||
        !filter_dimensions_valid_u16(&workspace->deblocked_u16, frame) ||
        !filter_dimensions_valid_u16(&workspace->cdef_u16, frame) ||
        !av2h_ccso_parameters_valid(frame) ||
        !av2h_ccso_controls_valid(frame, &workspace->ccso_controls)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration != 0U) return AV2H_UNSUPPORTED;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        stage_planes[stage_plane_count++] = &planes->plane[plane];
        stage_planes[stage_plane_count++] =
            &workspace->deblocked_u16.plane[plane];
        stage_planes[stage_plane_count++] = &workspace->cdef_u16.plane[plane];
    }
    for (left = 0U; left < stage_plane_count; ++left) {
        size_t right;
        for (right = left + 1U; right < stage_plane_count; ++right) {
            if (filter_plane_rows_overlap_u16(
                    stage_planes[left], stage_planes[right])) {
                return AV2H_INVALID_ARGUMENT;
            }
        }
    }
    status = av2h_deblock_stage_u16(
        planes, sequence, frame, workspace);
    if (status != AV2H_OK) return status;
    status = av2h_cdef_filter_frame_u16_with_controls(
        &workspace->cdef_u16, &workspace->deblocked_u16, frame,
        &workspace->cdef_controls);
    if (status != AV2H_OK) return status;
    status = av2h_ccso_filter_frame_u16(
        &workspace->cdef_u16, workspace->deblocked_u16.plane[0].data,
        workspace->deblocked_u16.plane[0].stride_samples, frame,
        &workspace->ccso_controls);
    if (status != AV2H_OK) return status;
    status = av2h_gdf_filter_frame_u16_with_controls(
        &planes->plane[0], &workspace->cdef_u16.plane[0],
        &workspace->deblocked_u16.plane[0], frame,
        &workspace->gdf_controls);
    if (status != AV2H_OK) return status;
    for (plane = 1U; plane < av2h_frame_plane_count(frame); ++plane) {
        filter_copy_plane_u16(
            &planes->plane[plane], &workspace->cdef_u16.plane[plane]);
    }
    return AV2H_OK;
}
