static int AV2H_KERNEL_NAME(sized_u8_plane_bytes, sized_u16_plane_bytes)(size_t stride_samples,
                                                                         size_t height,
                                                                         size_t *size_bytes) {
#if AV2H_KERNEL_WIDE
    size_t sample_count;
    return av2h_size_multiply(stride_samples, height, &sample_count) &&
           av2h_size_multiply(sample_count, sizeof(uint16_t), size_bytes);
#else
    return av2h_size_multiply(stride_samples, height, size_bytes);
#endif
}
Av2hStatus AV2H_KERNEL_NAME(av2h_frame_output_validate_u8, av2h_frame_output_validate_u16)(
    const Av2hFrameState *frame,
    const
#if AV2H_KERNEL_WIDE
    Av2hReconstructionPlaneU16
#else
    Av2hFilterPlanes *
#endif
        coded
#if AV2H_KERNEL_WIDE
            [AV2H_FRAME_PLANE_COUNT]
#endif
    ,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT], uint8_t *output,
    size_t output_size_bytes) {
    Av2hFilterWorkspaceLayout layout;
    size_t required_output_bytes;
    unsigned int plane;
    Av2hStatus status;
    if (frame == NULL || coded == NULL || coded_data_size_bytes == NULL || output == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
#if AV2H_KERNEL_WIDE
    status = av2h_planar_output_size(frame, sizeof(uint16_t), &required_output_bytes);
#else
    status = av2h_planar_output_size(frame, 1U, &required_output_bytes);
#endif
    if (status != AV2H_OK)
        return status;
    if (output_size_bytes < required_output_bytes)
        return AV2H_OUT_OF_MEMORY;
#if AV2H_KERNEL_WIDE
    status = av2h_filter_workspace_query(frame, sizeof(uint16_t), &layout);
#else
    status = av2h_filter_workspace_query(frame, 1U, &layout);
#endif
    if (status != AV2H_OK)
        return status;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
#if AV2H_KERNEL_WIDE
        const Av2hReconstructionPlaneU16 *source = &coded[plane];
#else
        const Av2hFilterPlane *source = &coded->plane[plane];
#endif
        size_t source_size_bytes;
        if (source->data == NULL || source->width != layout.plane_width[plane] ||
            source->height != layout.plane_height[plane] ||
            source->stride_samples < source->width ||
            !AV2H_KERNEL_NAME(sized_u8_plane_bytes, sized_u16_plane_bytes)(
                source->stride_samples, source->height, &source_size_bytes) ||
            source_size_bytes > coded_data_size_bytes[plane] ||
            output_overlaps(output, required_output_bytes, source->data, source_size_bytes)) {
            return AV2H_INVALID_ARGUMENT;
        }
    }
    return AV2H_OK;
}