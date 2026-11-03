#include "decoder/filter.h"

#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_diagnostics.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/filter_stage.h"
#include "decoder/internal/sample_math.h"

#include <stddef.h>

const Av2hFilterDiagnostics *av2h_filter_diagnostics_get(const Av2hDecodeDiagnostics *diagnostics) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return diagnostics != NULL ? &diagnostics->filter : NULL;
#else
    (void)diagnostics;
    return NULL;
#endif
}

static void reset_filter_diagnostics(Av2hFilterWorkspace *workspace) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (workspace != NULL && workspace->diagnostics != NULL) {
        workspace->diagnostics->filter.reason = 0U;
    }
#else
    (void)workspace;
#endif
}

static void set_filter_reason(Av2hFilterWorkspace *workspace, uint8_t reason) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (workspace != NULL && workspace->diagnostics != NULL) {
        workspace->diagnostics->filter.reason = reason;
    }
#else
    (void)workspace;
    (void)reason;
#endif
}

static int restoration_controls_valid(const Av2hFrameState *frame,
                                      const Av2hRestorationControlMap *controls) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t unit_count;
        size_t unit_index;
        uint8_t frame_type = frame->restoration_type[plane];
        if (frame_type == AV2H_RESTORE_NONE)
            continue;
        if (controls->unit_flags[plane] == NULL || controls->unit_taps[plane] == NULL ||
            controls->unit_columns[plane] != frame->restoration_unit_columns[plane] ||
            controls->unit_rows[plane] != frame->restoration_unit_rows[plane] ||
            controls->unit_size[plane] != frame->restoration_unit_size[plane] ||
            !av2h_size_multiply(controls->unit_columns[plane], controls->unit_rows[plane],
                                &unit_count) ||
            unit_count != frame->restoration_unit_count[plane]) {
            return 0;
        }
        for (unit_index = 0U; unit_index < unit_count; ++unit_index) {
            uint8_t unit_type = controls->unit_flags[plane][unit_index];
            if (unit_type == AV2H_RESTORE_NONE)
                continue;
            if (frame_type == AV2H_RESTORE_SWITCHABLE) {
                if (plane != 0U || (unit_type != AV2H_RESTORE_PC_WIENER &&
                                    unit_type != AV2H_RESTORE_WIENER_NONSEP)) {
                    return 0;
                }
            } else if (unit_type != frame_type) {
                return 0;
            }
        }
    }
    return 1;
}

static int filter_enables_valid(const Av2hSequenceState *sequence, const Av2hFrameState *frame) {
    return (frame->cdef_strength_count == 0U || sequence->enable_cdef != 0U) &&
           ((frame->ccso[0].enabled == 0U && frame->ccso[1].enabled == 0U &&
             frame->ccso[2].enabled == 0U) ||
            sequence->enable_ccso != 0U) &&
           (frame->gdf_mode == 0U || sequence->enable_gdf != 0U);
}

static int deblock_plane_enabled(const Av2hFrameState *frame, unsigned int plane) {
    return plane == 0U ? frame->deblock_enabled[0] != 0U || frame->deblock_enabled[1] != 0U
                       : frame->deblock_enabled[plane + 1U] != 0U;
}

Av2hStatus av2h_filter_frame(Av2hFilterPlanes *planes, const Av2hSequenceState *sequence,
                             const Av2hFrameState *frame, Av2hFilterWorkspace *workspace,
                             const Av2hExecution *execution) {
    unsigned int plane;
    Av2hStatus status;
    const Av2hFilterPlane *gdf_baseline;
    Av2hFilterPlanes deblocked = {0};
    Av2hFilterPlanes active;
    Av2hFilterPlanes gdf_output = {0};
    Av2hFilterPlanes restoration_planes = {0};
    int preserve_stages;
    reset_filter_diagnostics(workspace);
    if (sequence == NULL || frame == NULL || planes == NULL || workspace == NULL) {
        set_filter_reason(workspace, 1U);
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration == 0U && (frame->restoration_type[0] != AV2H_RESTORE_NONE ||
                                               frame->restoration_type[1] != AV2H_RESTORE_NONE ||
                                               frame->restoration_type[2] != AV2H_RESTORE_NONE)) {
        set_filter_reason(workspace, 2U);
        return AV2H_UNSUPPORTED;
    }
    if (sequence->enable_restoration != 0U &&
        ((frame->restoration_type[0] != AV2H_RESTORE_NONE &&
          frame->restoration_type[0] != AV2H_RESTORE_PC_WIENER &&
          frame->restoration_type[0] != AV2H_RESTORE_WIENER_NONSEP &&
          frame->restoration_type[0] != AV2H_RESTORE_SWITCHABLE) ||
         (frame->restoration_type[1] != AV2H_RESTORE_NONE &&
          frame->restoration_type[1] != AV2H_RESTORE_WIENER_NONSEP) ||
         (frame->restoration_type[2] != AV2H_RESTORE_NONE &&
          frame->restoration_type[2] != AV2H_RESTORE_WIENER_NONSEP))) {
        set_filter_reason(workspace, 2U);
        return AV2H_UNSUPPORTED;
    }
    if (frame->cdef_strength_count != 0U &&
        (frame->cdef_on_skip_transform == 0U || frame->cdef_damping < 3U)) {
        set_filter_reason(workspace, 5U);
        return AV2H_UNSUPPORTED;
    }
    if (!filter_enables_valid(sequence, frame) ||
        (frame->cdef_strength_count != 0U &&
         (frame->cdef_damping > 6U || frame->cdef_y_strength[0] > 63U ||
          frame->cdef_uv_strength[0] > 63U)) ||
        !av2h_ccso_parameters_valid(frame) || !filter_dimensions_valid(planes, frame) ||
        !filter_dimensions_valid(&workspace->deblocked, frame) ||
        !filter_dimensions_valid(&workspace->cdef, frame) ||
        !av2h_deblock_map_valid(&workspace->deblocked, &workspace->deblock_map) ||
        !av2h_ccso_controls_valid(frame, &workspace->ccso_controls)) {
        set_filter_reason(workspace, 6U);
        return AV2H_INVALID_ARGUMENT;
    }
    if (!restoration_controls_valid(frame, &workspace->restoration_controls) ||
        (frame->restoration_type[0] != AV2H_RESTORE_NONE &&
         (workspace->restoration.data == NULL ||
          planes->plane[0].data == workspace->restoration.data ||
          workspace->deblocked.plane[0].data == workspace->restoration.data ||
          workspace->cdef.plane[0].data == workspace->restoration.data))) {
        set_filter_reason(workspace, 6U);
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (planes->plane[plane].data == workspace->deblocked.plane[plane].data ||
            planes->plane[plane].data == workspace->cdef.plane[plane].data ||
            workspace->deblocked.plane[plane].data == workspace->cdef.plane[plane].data) {
            set_filter_reason(workspace, 7U);
            return AV2H_INVALID_ARGUMENT;
        }
    }
    /* Accepted U8 aliases require the original snapshot and commit order. */
    preserve_stages = filter_stages_overlap(planes, &workspace->deblocked, &workspace->cdef, frame);
    if (preserve_stages)
        execution = NULL;
    if (workspace->restoration.data != NULL) {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            if (filter_plane_rows_overlap(&workspace->restoration, &planes->plane[plane]) ||
                filter_plane_rows_overlap(&workspace->restoration,
                                          &workspace->deblocked.plane[plane]) ||
                filter_plane_rows_overlap(&workspace->restoration, &workspace->cdef.plane[plane]))
                execution = NULL;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        deblocked.plane[plane] = planes->plane[plane];
        if (preserve_stages || deblock_plane_enabled(frame, plane)) {
            filter_copy_plane(&workspace->deblocked.plane[plane], &planes->plane[plane]);
            deblocked.plane[plane] = workspace->deblocked.plane[plane];
        }
    }
    status = av2h_deblock_filter_frame(&deblocked, sequence, frame, &workspace->deblock_map);
    if (status != AV2H_OK)
        return status;
    active = deblocked;
    if (preserve_stages || frame->cdef_strength_count != 0U) {
        status =
            filter_dispatch_stage(AV2H_FILTER_STAGE_CDEF, &workspace->cdef, &deblocked, &deblocked,
                                  NULL, sequence, frame, workspace, NULL, execution);
        if (status != AV2H_OK)
            return status;
        active = workspace->cdef;
    } else {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            if (frame->ccso[plane].enabled != 0U) {
                filter_copy_plane(&workspace->cdef.plane[plane], &active.plane[plane]);
                active.plane[plane] = workspace->cdef.plane[plane];
            }
        }
    }
    status = filter_dispatch_stage(AV2H_FILTER_STAGE_CCSO, &active, &active, &deblocked, NULL,
                                   sequence, frame, workspace, NULL, execution);
    if (status != AV2H_OK)
        return status;
    if (frame->restoration_type[0] == AV2H_RESTORE_PC_WIENER ||
        frame->restoration_type[0] == AV2H_RESTORE_SWITCHABLE) {
        if (workspace->restoration_controls.tx_skip == NULL) {
            return AV2H_INVALID_ARGUMENT;
        }
        restoration_planes.plane[0] = workspace->restoration;
        status =
            filter_dispatch_stage(AV2H_FILTER_STAGE_PC_WIENER, &restoration_planes, &active,
                                  &deblocked, NULL, sequence, frame, workspace, NULL, execution);
        if (status != AV2H_OK)
            return status;
    }
    if (frame->restoration_type[0] == AV2H_RESTORE_WIENER_NONSEP ||
        frame->restoration_type[0] == AV2H_RESTORE_SWITCHABLE ||
        frame->restoration_type[1] == AV2H_RESTORE_WIENER_NONSEP ||
        frame->restoration_type[2] == AV2H_RESTORE_WIENER_NONSEP) {
        restoration_planes.plane[0] = workspace->restoration;
        for (plane = 1U; plane < av2h_frame_plane_count(frame); ++plane) {
            restoration_planes.plane[plane] =
                deblocked.plane[plane].data == planes->plane[plane].data
                    ? workspace->deblocked.plane[plane]
                    : planes->plane[plane];
        }
        status =
            filter_dispatch_stage(AV2H_FILTER_STAGE_WIENER_NONSEP, &restoration_planes, &active,
                                  &deblocked, NULL, sequence, frame, workspace, NULL, execution);
        if (status != AV2H_OK)
            return status;
        for (plane = 1U; plane < av2h_frame_plane_count(frame); ++plane) {
            if (frame->restoration_type[plane] != AV2H_RESTORE_NONE)
                active.plane[plane] = restoration_planes.plane[plane];
        }
    }
    gdf_baseline = frame->restoration_type[0] != AV2H_RESTORE_NONE ? &workspace->restoration
                                                                   : &workspace->cdef.plane[0];
    status = av2h_gdf_validate_frame(&planes->plane[0], gdf_baseline, &workspace->cdef.plane[0],
                                     &workspace->deblocked.plane[0], sequence, frame,
                                     &workspace->gdf_controls);
    if (status != AV2H_OK)
        return status;
    gdf_baseline = frame->restoration_type[0] != AV2H_RESTORE_NONE ? &workspace->restoration
                                                                   : &active.plane[0];
    if (frame->gdf_mode != 0U) {
        gdf_output.plane[0] = deblocked.plane[0].data == planes->plane[0].data
                                  ? workspace->deblocked.plane[0]
                                  : planes->plane[0];
        status = filter_dispatch_stage(AV2H_FILTER_STAGE_GDF, &gdf_output, &active, &deblocked,
                                       gdf_baseline, sequence, frame, workspace, NULL, execution);
        if (status != AV2H_OK)
            return status;
        active.plane[0] = gdf_output.plane[0];
    } else {
        active.plane[0] = *gdf_baseline;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (planes->plane[plane].data != active.plane[plane].data)
            filter_copy_plane(&planes->plane[plane], &active.plane[plane]);
    }
    return AV2H_OK;
}

Av2hStatus av2h_filter_stage_u16(Av2hFilterPlanesU16 *planes, const Av2hSequenceState *sequence,
                                 const Av2hFrameState *frame, Av2hFilterWorkspace *workspace,
                                 const Av2hSampleDomain *domain, const Av2hExecution *execution) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    Av2hStatus status;
    unsigned int plane;
    Av2hFilterPlanesU16 deblocked = {0};
    Av2hFilterPlanesU16 active;
    Av2hFilterPlanesU16 gdf_output = {0};
    if (planes == NULL || sequence == NULL || frame == NULL || workspace == NULL ||
        sequence->info.bit_depth_idc != 0U || domain->bit_depth != 10U ||
        !filter_enables_valid(sequence, frame) || !filter_dimensions_valid_u16(planes, frame) ||
        !filter_dimensions_valid_u16(&workspace->deblocked_u16, frame) ||
        !filter_dimensions_valid_u16(&workspace->cdef_u16, frame) ||
        !av2h_ccso_parameters_valid(frame) ||
        !av2h_ccso_controls_valid(frame, &workspace->ccso_controls)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration != 0U)
        return AV2H_UNSUPPORTED;
    if (filter_stages_overlap_u16(planes, &workspace->deblocked_u16, &workspace->cdef_u16, frame))
        return AV2H_INVALID_ARGUMENT;
    if (!av2h_deblock_map_valid_u16(&workspace->deblocked_u16, &workspace->deblock_map))
        return AV2H_INVALID_ARGUMENT;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        deblocked.plane[plane] = planes->plane[plane];
        if (deblock_plane_enabled(frame, plane)) {
            filter_copy_plane_u16(&workspace->deblocked_u16.plane[plane], &planes->plane[plane]);
            deblocked.plane[plane] = workspace->deblocked_u16.plane[plane];
        }
    }
    status =
        av2h_deblock_filter_frame_u16(&deblocked, sequence, frame, &workspace->deblock_map, domain);
    if (status != AV2H_OK)
        return status;
    active = deblocked;
    if (frame->cdef_strength_count != 0U) {
        status = filter_dispatch_stage_u16(AV2H_FILTER_STAGE_CDEF, &workspace->cdef_u16, &deblocked,
                                           &deblocked, NULL, sequence, frame, workspace, domain,
                                           execution);
        if (status != AV2H_OK)
            return status;
        active = workspace->cdef_u16;
    } else {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            if (frame->ccso[plane].enabled != 0U) {
                filter_copy_plane_u16(&workspace->cdef_u16.plane[plane], &active.plane[plane]);
                active.plane[plane] = workspace->cdef_u16.plane[plane];
            }
        }
    }
    status = filter_dispatch_stage_u16(AV2H_FILTER_STAGE_CCSO, &active, &active, &deblocked, NULL,
                                       sequence, frame, workspace, domain, execution);
    if (status != AV2H_OK)
        return status;
    status = av2h_gdf_validate_frame_u16(&planes->plane[0], &workspace->cdef_u16.plane[0],
                                         &workspace->deblocked_u16.plane[0], sequence, frame,
                                         &workspace->gdf_controls, domain);
    if (status != AV2H_OK)
        return status;
    if (frame->gdf_mode != 0U) {
        gdf_output.plane[0] = deblocked.plane[0].data == planes->plane[0].data
                                  ? workspace->deblocked_u16.plane[0]
                                  : planes->plane[0];
        status = filter_dispatch_stage_u16(AV2H_FILTER_STAGE_GDF, &gdf_output, &active, &deblocked,
                                           NULL, sequence, frame, workspace, domain, execution);
        if (status != AV2H_OK)
            return status;
        active.plane[0] = gdf_output.plane[0];
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (planes->plane[plane].data != active.plane[plane].data)
            filter_copy_plane_u16(&planes->plane[plane], &active.plane[plane]);
    }
    return AV2H_OK;
}
