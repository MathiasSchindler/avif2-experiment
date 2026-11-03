static AV2H_KERNEL_TYPE(uint8_t, uint16_t)
    AV2H_KERNEL_NAME(luma_sample, luma_sample_u16)(const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * luma,
                                                   size_t stride_samples, uint16_t width,
                                                   uint16_t height, int x, int y) {
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= (int)width)
        x = (int)width - 1;
    if (y >= (int)height)
        y = (int)height - 1;
    return luma[(size_t)y * stride_samples + (size_t)x];
}
static unsigned int AV2H_KERNEL_NAME(ccso_class,
                                     ccso_class_u16)(AV2H_KERNEL_TYPE(uint8_t, uint16_t) neighbor,
                                                     AV2H_KERNEL_TYPE(uint8_t, uint16_t) center,
                                                     uint16_t threshold, uint8_t edge_classifier) {
    int difference = (int)neighbor - (int)center;
    if (difference < -(int)threshold)
        return 0U;
    if (edge_classifier != 0U)
        return 1U;
    return difference > (int)threshold ? 2U : 1U;
}
static void AV2H_KERNEL_NAME(ccso_filter_plane, ccso_filter_plane_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) * destination,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * luma, size_t luma_stride_samples,
    uint16_t luma_width, uint16_t luma_height, const Av2hCcsoPlane *parameters,
    const uint8_t *controls, size_t control_stride_units, uint16_t luma_unit_size,
    unsigned int subsampling, const Av2hSequenceState *sequence, const Av2hFrameState *frame
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
    unsigned int unit_size = luma_unit_size >> subsampling;
#if AV2H_KERNEL_WIDE
    unsigned int shift = domain->bit_depth - parameters->max_band_log2;
#else
    unsigned int shift = 8U - parameters->max_band_log2;
#endif
    uint16_t threshold = ccso_quant_size[parameters->scale_index][parameters->quant_index];
    int first_x = 0;
    int first_y = 0;
    int second_x = 0;
    int second_y = 0;
    uint16_t y;
    if (parameters->band_only == 0U) {
        ccso_sample_offsets(parameters->extended_filter_support, &first_x, &first_y, &second_x,
                            &second_y);
    }
    for (y = 0U; y < destination->height; ++y) {
        uint16_t x;
        for (x = 0U; x < destination->width; ++x) {
            size_t control_row = y / unit_size;
            size_t control_column = x / unit_size;
            int luma_x;
            int luma_y;
            int tile_left;
            int tile_top;
            int tile_right;
            int tile_bottom;
            AV2H_KERNEL_TYPE(uint8_t, uint16_t) center;
            unsigned int first_class = 0U;
            unsigned int second_class = 0U;
            unsigned int band;
            size_t lut_index;
            size_t destination_index;
            if (controls[control_row * control_stride_units + control_column] == 0U) {
                continue;
            }
            luma_x = (int)((uint32_t)x << subsampling);
            luma_y = (int)((uint32_t)y << subsampling);
            ccso_tile_bounds(sequence, frame, luma_x, luma_y, &tile_left, &tile_top, &tile_right,
                             &tile_bottom);
            if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U ||
                (uint32_t)frame->tile_column_count * frame->tile_row_count <= 1U) {
                tile_left = 0;
                tile_top = 0;
                tile_right = luma_width;
                tile_bottom = luma_height;
            }
            center = AV2H_KERNEL_NAME(luma_sample, luma_sample_u16)(
                luma, luma_stride_samples, luma_width, luma_height, luma_x, luma_y);
            if (parameters->band_only == 0U) {
                first_class = AV2H_KERNEL_NAME(ccso_class, ccso_class_u16)(
                    AV2H_KERNEL_NAME(luma_sample, luma_sample_u16)(
                        luma, luma_stride_samples, luma_width, luma_height,
                        av2h_sample_clamp(luma_x + first_x, tile_left, tile_right - 1),
                        av2h_sample_clamp(luma_y + first_y, tile_top, tile_bottom - 1)),
                    center, threshold, parameters->edge_classifier);
                second_class = AV2H_KERNEL_NAME(ccso_class, ccso_class_u16)(
                    AV2H_KERNEL_NAME(luma_sample, luma_sample_u16)(
                        luma, luma_stride_samples, luma_width, luma_height,
                        av2h_sample_clamp(luma_x + second_x, tile_left, tile_right - 1),
                        av2h_sample_clamp(luma_y + second_y, tile_top, tile_bottom - 1)),
                    center, threshold, parameters->edge_classifier);
            }
            band = parameters->max_band_log2 == 0U ? 0U : (unsigned int)center >> shift;
            lut_index = ((size_t)band << 4U) + ((size_t)first_class << 2U) + second_class;
            destination_index = (size_t)y * destination->stride_samples + x;
#if AV2H_KERNEL_WIDE
            av2h_sample_clip_store_u16(destination->data, destination->stride_samples, x, y,
                                       (int)destination->data[destination_index] +
                                           parameters->filter_offset[lut_index],
                                       domain);
#else
            av2h_sample_clip_store_u8(destination->data, destination->stride_samples, x, y,
                                      (int)destination->data[destination_index] +
                                          parameters->filter_offset[lut_index]);
#endif
        }
    }
}
Av2hStatus AV2H_KERNEL_NAME(av2h_ccso_filter_frame, av2h_ccso_filter_frame_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) * planes,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * pre_cdef_luma, size_t pre_cdef_luma_stride_samples,
    const Av2hSequenceState *sequence, const Av2hFrameState *frame,
    const Av2hCcsoControlMap *controls
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#endif
    unsigned int plane;
#if AV2H_KERNEL_WIDE
    if (planes == NULL || pre_cdef_luma == NULL || frame == NULL || frame->width == 0U ||
        frame->height == 0U ||
        pre_cdef_luma_stride_samples < av2h_filter_coded_dimension(frame->width) ||
        !filter_dimensions_valid_u16(planes, frame) || !av2h_ccso_parameters_valid(frame) ||
        !av2h_ccso_controls_valid(frame, controls)) {
#else
    if (planes == NULL || pre_cdef_luma == NULL || frame == NULL || frame->width == 0U ||
        frame->height == 0U ||
        pre_cdef_luma_stride_samples < av2h_filter_coded_dimension(frame->width) ||
        !filter_dimensions_valid(planes, frame) || !av2h_ccso_parameters_valid(frame) ||
        !av2h_ccso_controls_valid(frame, controls)) {
#endif
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (frame->ccso[plane].enabled == 0U)
            continue;
#if AV2H_KERNEL_WIDE
        ccso_filter_plane_u16(&planes->plane[plane], pre_cdef_luma, pre_cdef_luma_stride_samples,
                              planes->plane[0].width, planes->plane[0].height, &frame->ccso[plane],
                              controls->plane[plane], controls->stride_units[plane],
                              av2h_filter_ccso_unit_size(frame),
                              av2h_chroma_shift(frame->chroma_format_idc, plane), sequence, frame,
                              domain);
#else
        ccso_filter_plane(&planes->plane[plane], pre_cdef_luma, pre_cdef_luma_stride_samples,
                          planes->plane[0].width, planes->plane[0].height, &frame->ccso[plane],
                          controls->plane[plane], controls->stride_units[plane],
                          av2h_filter_ccso_unit_size(frame),
                          av2h_chroma_shift(frame->chroma_format_idc, plane), sequence, frame);
#endif
    }
    return AV2H_OK;
}