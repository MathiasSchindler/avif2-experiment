typedef struct {
    AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *destination;
    const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *source;
    const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *pre_cdef;
    const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *baseline;
    const Av2hSequenceState *sequence;
    const Av2hFrameState *frame;
    const Av2hFilterWorkspace *workspace;
    const Av2hSampleDomain *domain;
    Av2hFilterStage stage;
} AV2H_KERNEL_NAME(Av2hFilterStageWork, Av2hFilterStageWorkU16);

static inline int AV2H_KERNEL_NAME(
    filter_stage_rows, filter_stage_rows_u16)(size_t begin, size_t end, unsigned int worker_index,
                                    void *argument) {
    AV2H_KERNEL_NAME(Av2hFilterStageWork, Av2hFilterStageWorkU16) *work = argument;
    size_t first = begin * 64U;
    size_t last = end * 64U;
    (void)worker_index;
    switch (work->stage) {
    case AV2H_FILTER_STAGE_CDEF:
        return AV2H_KERNEL_NAME(
            av2h_cdef_filter_rows,
            av2h_cdef_filter_rows_u16)(work->destination, work->source, work->sequence, work->frame,
                                     &work->workspace->cdef_controls, first, last
#if AV2H_KERNEL_WIDE
                                     ,
                                     work->domain
#endif
        );
    case AV2H_FILTER_STAGE_CCSO:
        return AV2H_KERNEL_NAME(
            av2h_ccso_filter_rows,
            av2h_ccso_filter_rows_u16)(work->destination, work->pre_cdef->plane[0].data,
                                     work->pre_cdef->plane[0].stride_samples, work->sequence,
                                     work->frame, &work->workspace->ccso_controls, first, last
#if AV2H_KERNEL_WIDE
                                     ,
                                     work->domain
#endif
        );
#if !AV2H_KERNEL_WIDE
    case AV2H_FILTER_STAGE_PC_WIENER:
        return av2h_pc_wiener_filter_rows(&work->destination->plane[0], &work->source->plane[0],
                                          &work->pre_cdef->plane[0], work->frame,
                                          &work->workspace->restoration_controls, first, last);
    case AV2H_FILTER_STAGE_WIENER_NONSEP:
        return av2h_wiener_nonsep_filter_rows(work->destination, work->source, work->pre_cdef,
                                              work->sequence, work->frame,
                                              &work->workspace->restoration_controls, first, last);
#endif
    case AV2H_FILTER_STAGE_GDF:
        AV2H_KERNEL_NAME(
            av2h_gdf_apply_rows,
            av2h_gdf_apply_rows_u16)(&work->destination->plane[0],
#if !AV2H_KERNEL_WIDE
                            work->baseline,
#endif
                            &work->source->plane[0], &work->pre_cdef->plane[0], work->sequence,
                            work->frame, &work->workspace->gdf_controls, first, last
#if AV2H_KERNEL_WIDE
                            ,
                            work->domain
#endif
        );
        return AV2H_OK;
    default:
        return AV2H_INVALID_ARGUMENT;
    }
}

static inline Av2hStatus AV2H_KERNEL_NAME(
    filter_dispatch_stage,
    filter_dispatch_stage_u16)(Av2hFilterStage stage,
                               AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *destination,
                      const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *source, const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *pre_cdef,
                      const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *baseline, const Av2hSequenceState *sequence,
                      const Av2hFrameState *frame, const Av2hFilterWorkspace *workspace,
                      const Av2hSampleDomain *domain, const Av2hExecution *execution) {
    AV2H_KERNEL_NAME(Av2hFilterStageWork, Av2hFilterStageWorkU16) work = {destination, source,    pre_cdef, baseline, sequence,
                                frame,       workspace, domain,   stage};
    if (stage == AV2H_FILTER_STAGE_CCSO && frame->ccso[0].enabled == 0U &&
        frame->ccso[1].enabled == 0U && frame->ccso[2].enabled == 0U)
        execution = NULL;
    int status = av2h_parallel_ranges(execution, ((size_t)source->plane[0].height + 63U) / 64U,
                                      AV2H_KERNEL_NAME(filter_stage_rows, filter_stage_rows_u16), &work);
    return status < 0 ? AV2H_INVALID_DATA : (Av2hStatus)status;
}
