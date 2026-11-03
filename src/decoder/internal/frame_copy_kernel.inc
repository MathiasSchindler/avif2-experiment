static int AV2H_KERNEL_NAME(copy_rows_u8, copy_rows_u16)(size_t begin, size_t end,
                                                         unsigned int worker_index,
                                                         void *argument) {
#if AV2H_KERNEL_WIDE
    Av2hCopyRowsU16 *copy = (Av2hCopyRowsU16 *)argument;
#else
    Av2hCopyRowsU8 *copy = (Av2hCopyRowsU8 *)argument;
#endif
    size_t row;
    (void)worker_index;
    for (row = begin; row < end; ++row) {
#if AV2H_KERNEL_WIDE
        size_t column;
        uint8_t *destination = copy->destination + row * copy->width * sizeof(uint16_t);
        const uint16_t *source = copy->source + row * copy->source_stride;
        for (column = 0U; column < copy->width; ++column) {
            uint16_t sample = source[column];
            destination[column * 2U] = (uint8_t)sample;
            destination[column * 2U + 1U] = (uint8_t)(sample >> 8U);
        }
#else
        av2h_memory_copy(copy->destination + row * copy->row_bytes,
                         copy->source + row * copy->source_stride, copy->row_bytes);
#endif
    }
    return 0;
}
Av2hStatus AV2H_KERNEL_NAME(av2h_copy_visible_planar_private, av2h_copy_visible_planar_u16_private)(
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
    size_t output_size_bytes, const Av2hExecution *execution) {
    size_t offset = 0U;
    unsigned int plane;
    Av2hStatus status =
        AV2H_KERNEL_NAME(av2h_frame_output_validate_u8, av2h_frame_output_validate_u16)(
            frame, coded, coded_data_size_bytes, output, output_size_bytes);
    if (status != AV2H_OK)
        return status;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
#if AV2H_KERNEL_WIDE
        const Av2hReconstructionPlaneU16 *source = &coded[plane];
#else
        const Av2hFilterPlane *source = &coded->plane[plane];
#endif
        size_t width = av2h_frame_plane_dimension(frame, plane, frame->width);
        size_t height = av2h_frame_plane_dimension(frame, plane, frame->height);
#if AV2H_KERNEL_WIDE
        size_t row_bytes = width * sizeof(uint16_t);
        Av2hCopyRowsU16 copy = {source->data, output + offset, source->stride_samples, width};
        if (copy_rows_dispatch(execution, height, row_bytes, copy_rows_u16, &copy) != 0) {
#else
        Av2hCopyRowsU8 copy = {source->data, output + offset, source->stride_samples, width};
        if (copy_rows_dispatch(execution, height, width, copy_rows_u8, &copy) != 0) {
#endif
            return AV2H_INVALID_DATA;
        }
#if AV2H_KERNEL_WIDE
        offset += row_bytes * height;
#else
        offset += width * height;
#endif
    }
    return AV2H_OK;
}