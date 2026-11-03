static inline int AV2H_KERNEL_NAME(
    filter_plane_rows_overlap,
    filter_plane_rows_overlap_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *left,
                                            const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *right) {
    size_t left_row = 0U;
    size_t right_row = 0U;
    uintptr_t pointer_max = (uintptr_t)-1;
    uintptr_t left_bound;
    uintptr_t right_bound;
    const size_t sample_bytes = sizeof(AV2H_KERNEL_TYPE(uint8_t, uint16_t));
    if (left->height == 0U || right->height == 0U)
        return 0;
    if (filter_plane_extent(left->data, left->stride_samples, left->width, left->height,
                            sample_bytes, &left_bound) &&
        filter_plane_extent(right->data, right->stride_samples, right->width, right->height,
                            sample_bytes, &right_bound) &&
        (left_bound <= (uintptr_t)right->data || right_bound <= (uintptr_t)left->data)) {
        return 0;
    }
    /* Preserve exact row-strided and overflowing-layout behavior. */
    while (left_row < left->height && right_row < right->height) {
        size_t left_offset;
        size_t right_offset;
        size_t left_width;
        size_t right_width;
        uintptr_t left_start;
        uintptr_t left_end;
        uintptr_t right_start;
        uintptr_t right_end;
        if (!av2h_size_multiply(left_row, left->stride_samples, &left_offset) ||
            !av2h_size_multiply(left_offset, sample_bytes, &left_offset) ||
            !av2h_size_multiply(right_row, right->stride_samples, &right_offset) ||
            !av2h_size_multiply(right_offset, sample_bytes, &right_offset) ||
            !av2h_size_multiply(left->width, sample_bytes, &left_width) ||
            !av2h_size_multiply(right->width, sample_bytes, &right_width) ||
            left_offset > (size_t)pointer_max || right_offset > (size_t)pointer_max ||
            (uintptr_t)left->data > pointer_max - (uintptr_t)left_offset ||
            (uintptr_t)right->data > pointer_max - (uintptr_t)right_offset) {
            return 1;
        }
        left_start = (uintptr_t)left->data + (uintptr_t)left_offset;
        right_start = (uintptr_t)right->data + (uintptr_t)right_offset;
        if ((uintptr_t)left_width > pointer_max - left_start ||
            (uintptr_t)right_width > pointer_max - right_start) {
            return 1;
        }
        left_end = left_start + (uintptr_t)left_width;
        right_end = right_start + (uintptr_t)right_width;
        if (left_start < right_end && right_start < left_end)
            return 1;
        if (left_end <= right_start) {
            ++left_row;
        } else {
            ++right_row;
        }
    }
    return 0;
}

static inline int AV2H_KERNEL_NAME(
    filter_stages_overlap,
    filter_stages_overlap_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *planes,
                                        const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *deblocked,
                                        const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *cdef, const Av2hFrameState *frame) {
    const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *stages[AV2H_FRAME_PLANE_COUNT * 3U];
    size_t count = 0U;
    size_t left;
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        stages[count++] = &planes->plane[plane];
        stages[count++] = &deblocked->plane[plane];
        stages[count++] = &cdef->plane[plane];
    }
    for (left = 0U; left < count; ++left) {
        size_t right;
        for (right = left + 1U; right < count; ++right) {
            if (AV2H_KERNEL_NAME(filter_plane_rows_overlap,
                                 filter_plane_rows_overlap_u16)(stages[left], stages[right]))
                return 1;
        }
    }
    return 0;
}

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

static inline void AV2H_KERNEL_NAME(filter_copy_rows, filter_copy_rows_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination, const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source,
                                    size_t begin, size_t end) {
    if (end > source->height)
        end = source->height;
    if (begin >= end)
        return;
    AV2H_KERNEL_NAME(
        av2h_plane_copy_u8,
        av2h_plane_copy_u16)(destination->data + begin * destination->stride_samples,
                       destination->stride_samples, source->data + begin * source->stride_samples,
                       source->stride_samples, source->width, end - begin);
}
