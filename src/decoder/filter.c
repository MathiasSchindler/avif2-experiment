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
        workspace->deblocked.plane[0].stride, frame,
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