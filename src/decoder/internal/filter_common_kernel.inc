static inline int AV2H_KERNEL_NAME(
    filter_dimensions_valid,
    filter_dimensions_valid_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *planes,
                                          const Av2hFrameState *frame) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t width =
            av2h_frame_plane_dimension(frame, plane, av2h_filter_coded_dimension(frame->width));
        size_t height =
            av2h_frame_plane_dimension(frame, plane, av2h_filter_coded_dimension(frame->height));
        if (planes->plane[plane].data == NULL || planes->plane[plane].width != width ||
            planes->plane[plane].height != height || planes->plane[plane].stride_samples < width) {
            return 0;
        }
    }
    return 1;
}

static inline void AV2H_KERNEL_NAME(filter_copy_plane, filter_copy_plane_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination, const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source) {
    AV2H_KERNEL_NAME(
        av2h_plane_copy_u8,
        av2h_plane_copy_u16)(destination->data, destination->stride_samples, source->data,
                       source->stride_samples, source->width, source->height);
}
