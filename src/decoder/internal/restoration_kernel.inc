typedef struct {
    const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source;
    const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked;
    int stripe_start;
    int stripe_end;
} AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16);

static AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    pc_wiener_source_sample,
    pc_wiener_source_sample_u16)(const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *context, int row, int column) {
    int source_row = row;
    int source_column = av2h_sample_clamp(column, 0, context->source->width - 1);
    if (row < context->stripe_start) {
        if (context->stripe_start == 0) {
            source_row = 0;
        } else {
            source_row = row < context->stripe_start - 1 ? context->stripe_start - 2
                                                         : context->stripe_start - 1;
            source_row = av2h_sample_clamp(source_row, 0, context->deblocked->height - 1);
            return context->deblocked
                ->data[(size_t)source_row * context->deblocked->stride_samples +
                       (size_t)source_column];
        }
    } else if (row >= context->stripe_end) {
        if (context->stripe_end >= context->source->height) {
            source_row = context->source->height - 1;
        } else {
            source_row = row == context->stripe_end ? context->stripe_end : context->stripe_end + 1;
            source_row = av2h_sample_clamp(source_row, 0, context->deblocked->height - 1);
            return context->deblocked
                ->data[(size_t)source_row * context->deblocked->stride_samples +
                       (size_t)source_column];
        }
    }
    source_row = av2h_sample_clamp(source_row, 0, context->source->height - 1);
    return context->source
        ->data[(size_t)source_row * context->source->stride_samples + (size_t)source_column];
}

static uint8_t AV2H_KERNEL_NAME(
    pc_wiener_classify,
    pc_wiener_classify_u16)(const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *context,
                                  const Av2hRestorationControlMap *controls, unsigned int qindex,
                                  int chunk_start, int chunk_width, int block_row, int block_column,
                                  const Av2hSampleDomain *domain) {
    int32_t feature[4] = {0, 0, 0, 0};
    unsigned int tskip = pc_wiener_tx_skip_sum(controls, context->stripe_start,
                                               context->stripe_end - context->stripe_start,
                                               chunk_start, chunk_width, block_row, block_column);
    int row;
    int lut_index = 0;
    for (row = -AV2H_PC_WIENER_FEATURE_LEAD; row <= AV2H_PC_WIENER_FEATURE_LAG; ++row) {
        int sample_row = context->stripe_start + block_row + row;
        int column;
        for (column = -AV2H_PC_WIENER_FEATURE_LEAD; column <= AV2H_PC_WIENER_FEATURE_LAG;
             ++column) {
            int sample_column = chunk_start + block_column + column;
            int center = 2 * AV2H_KERNEL_NAME(
                                 pc_wiener_source_sample,
                                 pc_wiener_source_sample_u16)(context, sample_row, sample_column);
            feature[1] += av2h_sample_abs(
                (int)AV2H_KERNEL_NAME(
                    pc_wiener_source_sample,
                    pc_wiener_source_sample_u16)(context, sample_row - 1, sample_column) +
                AV2H_KERNEL_NAME(pc_wiener_source_sample, pc_wiener_source_sample_u16)(context, sample_row + 1, sample_column) - center);
            feature[2] += av2h_sample_abs(
                (int)AV2H_KERNEL_NAME(
                    pc_wiener_source_sample,
                    pc_wiener_source_sample_u16)(context, sample_row - 1, sample_column + 1) +
                AV2H_KERNEL_NAME(pc_wiener_source_sample, pc_wiener_source_sample_u16)(context, sample_row + 1, sample_column - 1) - center);
            feature[3] += av2h_sample_abs(
                (int)AV2H_KERNEL_NAME(
                    pc_wiener_source_sample,
                    pc_wiener_source_sample_u16)(context, sample_row - 1, sample_column - 1) +
                AV2H_KERNEL_NAME(pc_wiener_source_sample, pc_wiener_source_sample_u16)(context, sample_row + 1, sample_column + 1) - center);
        }
    }
    const int depth_shift = domain->bit_depth - 8;
    for (row = 0; row < 4; ++row) {
        int value = av2h_sample_round_pow2_signed(
            (int64_t)av2h_sample_round_pow2_signed(
                (int64_t)feature[row] * pc_wiener_feature_normalizers[row], depth_shift) +
                pc_wiener_q_adjustment(qindex, tskip, (unsigned int)row, depth_shift),
            14);
        value = av2h_sample_clamp(value, 0, 255) >> 5U;
        lut_index += pc_wiener_lut_weights[row] * value;
    }
    return av2h_pc_wiener_lut_to_class[lut_index];
}

static void AV2H_KERNEL_NAME(pc_wiener_filter_block, pc_wiener_filter_block_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination, const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *context,
                                   int block_y, int block_x, int block_height, int block_width,
                                   const int16_t *filter, const Av2hSampleDomain *domain) {
    static const int8_t tap_row[12] = {1, 0, 2, 0, 1, -1, 2, 2, 1, 1, 3, 0};
    static const int8_t tap_column[12] = {0, 1, 0, 2, 1, 1, 1, -1, 2, -2, 0, 3};
    int row;
    for (row = 0; row < block_height; ++row) {
        int column;
        for (column = 0; column < block_width; ++column) {
            int y = block_y + row;
            int x = block_x + column;
            int64_t value = ((int64_t)filter[12] + 128) *
                            AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                             pc_wiener_source_sample_u16)(context, y, x);
            unsigned int tap;
            for (tap = 0U; tap < 12U; ++tap) {
                value +=
                    (int64_t)filter[tap] *
                    ((int)AV2H_KERNEL_NAME(
                         pc_wiener_source_sample,
                         pc_wiener_source_sample_u16)(context, y + tap_row[tap], x + tap_column[tap]) +
                     AV2H_KERNEL_NAME(
                         pc_wiener_source_sample,
                         pc_wiener_source_sample_u16)(context, y - tap_row[tap], x - tap_column[tap]));
            }
            destination->data[(size_t)y * destination->stride_samples + (size_t)x] =
                (AV2H_KERNEL_TYPE(
                    uint8_t,
                    uint16_t))av2h_sample_clip_to_domain(av2h_sample_round_pow2_signed(value, 7),
                                                    domain);
        }
    }
}

Av2hStatus AV2H_KERNEL_NAME(av2h_pc_wiener_filter_rows, av2h_pc_wiener_filter_rows_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination, const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source,
                                      const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked, const Av2hFrameState *frame,
                                      const Av2hRestorationControlMap *controls, size_t row_begin,
                                      size_t row_end
#if AV2H_KERNEL_WIDE
                                      ,
                                      const Av2hSampleDomain *domain
#endif
) {
#if !AV2H_KERNEL_WIDE
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    size_t unit_row;
    const int16_t *filters;
    const uint8_t *selector;
    unsigned int filter_set;
    if (!av2h_sample_domain_valid(domain) || !filter_rows_valid(row_begin, row_end) ||
        destination == NULL || source == NULL || deblocked == NULL || frame == NULL ||
        controls == NULL || destination->data == NULL || source->data == NULL ||
        deblocked->data == NULL || controls->unit_flags[0] == NULL || controls->tx_skip == NULL ||
        controls->unit_size[0] == 0U || controls->unit_columns[0] == 0U ||
        controls->unit_rows[0] == 0U ||
        controls->tx_skip_stride < ((size_t)source->width + 3U) / 4U ||
        destination->width != source->width || destination->height != source->height ||
        deblocked->width != source->width || deblocked->height != source->height ||
        destination->stride_samples < destination->width ||
        source->stride_samples < source->width || deblocked->stride_samples < deblocked->width ||
        (frame->restoration_type[0] != AV2H_RESTORE_PC_WIENER &&
         frame->restoration_type[0] != AV2H_RESTORE_SWITCHABLE)) {
        return AV2H_INVALID_ARGUMENT;
    }
    filter_set = pc_wiener_filter_set_index(frame->base_q_index);
    filters = pc_wiener_filter(filter_set, 0U);
    selector = pc_wiener_filter_selector(filter_set);
    AV2H_KERNEL_NAME(filter_copy_rows,
                     filter_copy_rows_u16)(destination, source, row_begin, row_end);
    for (unit_row = 0U; unit_row < controls->unit_rows[0]; ++unit_row) {
        int unit_y0 = pc_wiener_unit_row_start(controls, unit_row);
        int unit_y1 = unit_row + 1U == controls->unit_rows[0]
                          ? source->height
                          : pc_wiener_unit_row_start(controls, unit_row + 1U);
        size_t unit_column;
        for (unit_column = 0U; unit_column < controls->unit_columns[0]; ++unit_column) {
            int unit_x0 = (int)(unit_column * controls->unit_size[0]);
            int unit_x1 = unit_column + 1U == controls->unit_columns[0]
                              ? source->width
                              : unit_x0 + controls->unit_size[0];
            int stripe_start;
            if (controls->unit_flags[0][unit_row * controls->unit_columns[0] + unit_column] !=
                AV2H_RESTORE_PC_WIENER) {
                continue;
            }
            for (stripe_start = unit_y0; stripe_start < unit_y1;) {
                int relative_stripe =
                    (stripe_start + AV2H_PC_WIENER_UNIT_OFFSET) / RESTORATION_PROC_UNIT_SIZE;
                int stripe_height = relative_stripe == 0
                                        ? RESTORATION_PROC_UNIT_SIZE - AV2H_PC_WIENER_UNIT_OFFSET
                                        : RESTORATION_PROC_UNIT_SIZE;
                AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) context;
                int chunk_start;
                if (stripe_height > unit_y1 - stripe_start) {
                    stripe_height = unit_y1 - stripe_start;
                }
                context = (AV2H_KERNEL_NAME(Av2hPcWienerSource,
                                            Av2hPcWienerSourceU16)){source, deblocked, stripe_start,
                                               stripe_start + stripe_height};
                for (chunk_start = unit_x0; chunk_start < unit_x1;
                     chunk_start += RESTORATION_PROC_UNIT_SIZE) {
                    int chunk_width = unit_x1 - chunk_start;
                    int block_row;
                    if (chunk_width > RESTORATION_PROC_UNIT_SIZE) {
                        chunk_width = RESTORATION_PROC_UNIT_SIZE;
                    }
                    for (block_row = 0; block_row < stripe_height;
                         block_row += AV2H_PC_WIENER_BLOCK_SIZE) {
                        int block_column;
                        if ((size_t)(stripe_start + block_row) < row_begin ||
                            (size_t)(stripe_start + block_row) >= row_end)
                            continue;
                        for (block_column = 0; block_column < chunk_width;
                             block_column += AV2H_PC_WIENER_BLOCK_SIZE) {
                            uint8_t class_index = AV2H_KERNEL_NAME(pc_wiener_classify,
                                                                   pc_wiener_classify_u16)(
                                &context, controls, frame->base_q_index, chunk_start, chunk_width,
                                block_row, block_column, domain);
                            int block_height = stripe_height - block_row;
                            int block_width = chunk_width - block_column;
                            if (block_height > AV2H_PC_WIENER_BLOCK_SIZE) {
                                block_height = AV2H_PC_WIENER_BLOCK_SIZE;
                            }
                            if (block_width > AV2H_PC_WIENER_BLOCK_SIZE) {
                                block_width = AV2H_PC_WIENER_BLOCK_SIZE;
                            }
                            AV2H_KERNEL_NAME(pc_wiener_filter_block, pc_wiener_filter_block_u16)(
                                destination, &context, stripe_start + block_row,
                                chunk_start + block_column, block_height, block_width,
                                filters + (size_t)selector[class_index] * NUM_PC_WIENER_TAPS_LUMA,
                                domain);
                        }
                    }
                }
                stripe_start += stripe_height;
            }
        }
    }
    return AV2H_OK;
}

static AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    wiener_nonsep_luma_sample,
    wiener_nonsep_luma_sample_u16)(const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *luma, uint8_t filter_index,
                                         unsigned int chroma_shift, int row, int column) {
    int chroma_height = (luma->source->height + ((int)1 << chroma_shift) - 1) >> chroma_shift;
    int chroma_width = (luma->source->width + ((int)1 << chroma_shift) - 1) >> chroma_shift;
    row = av2h_sample_clamp(row, 0, chroma_height - 1);
    column = av2h_sample_clamp(column, 0, chroma_width - 1);
    if (chroma_shift == 0U) {
        return AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                pc_wiener_source_sample_u16)(luma, row, column);
    }
    if (filter_index == 1U) {
        return (AV2H_KERNEL_TYPE(
            uint8_t, uint16_t))(((unsigned int)AV2H_KERNEL_NAME(
                                     pc_wiener_source_sample,
                                     pc_wiener_source_sample_u16)(luma, row * 2, column * 2) +
                                 AV2H_KERNEL_NAME(
                                     pc_wiener_source_sample,
                                     pc_wiener_source_sample_u16)(luma, row * 2 + 1, column * 2)) >>
                         1U);
    }
    if (filter_index == 2U) {
        return AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                pc_wiener_source_sample_u16)(luma, row * 2, column * 2);
    }
    return (AV2H_KERNEL_TYPE(
        uint8_t,
        uint16_t))(((unsigned int)AV2H_KERNEL_NAME(
                        pc_wiener_source_sample,
                        pc_wiener_source_sample_u16)(luma, row * 2, column * 2) +
                    AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                     pc_wiener_source_sample_u16)(luma, row * 2, column * 2 + 1) +
                    AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                     pc_wiener_source_sample_u16)(luma, row * 2 + 1, column * 2) +
                    AV2H_KERNEL_NAME(
                        pc_wiener_source_sample,
                        pc_wiener_source_sample_u16)(luma, row * 2 + 1, column * 2 + 1)) >>
                     2U);
}

static void AV2H_KERNEL_NAME(wiener_nonsep_filter_luma_region,
                             wiener_nonsep_filter_luma_region_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination,
                                             const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *source, int y0, int y1,
                                             int x0, int x1, const int16_t *taps,
                                             const Av2hSampleDomain *domain) {
    static const int8_t tap_row[16] = {1, 0, 2, 0, 1, -1, 2, 2, 1, 1, 3, 0, 4, 0, 3, 3};
    static const int8_t tap_column[16] = {0, 1, 0, 2, 1, 1, 1, -1, 2, -2, 0, 3, 0, 4, 3, -3};
    int row;
    for (row = y0; row < y1; ++row) {
        int column;
        for (column = x0; column < x1; ++column) {
            int center = AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                          pc_wiener_source_sample_u16)(source, row, column);
            int64_t value = (int64_t)center << 7U;
            unsigned int tap;
            for (tap = 0U; tap < 16U; ++tap) {
                value +=
                    (int64_t)taps[tap] *
                    ((int)AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                           pc_wiener_source_sample_u16)(source, row + tap_row[tap],
                                                  column + tap_column[tap]) +
                     AV2H_KERNEL_NAME(
                         pc_wiener_source_sample,
                         pc_wiener_source_sample_u16)(source, row - tap_row[tap], column - tap_column[tap]) -
                     2 * center);
            }
            destination->data[(size_t)row * destination->stride_samples + (size_t)column] =
                (AV2H_KERNEL_TYPE(
                    uint8_t,
                    uint16_t))av2h_sample_clip_to_domain(av2h_sample_round_pow2_signed(value, 7),
                                                    domain);
        }
    }
}

static void AV2H_KERNEL_NAME(wiener_nonsep_filter_chroma_region,
                             wiener_nonsep_filter_chroma_region_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination,
                                               const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *source,
                                               const AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) *luma,
                                               uint8_t luma_filter_index, unsigned int chroma_shift,
                                               int y0, int y1, int x0, int x1, const int16_t *taps,
                                               const Av2hSampleDomain *domain) {
    static const int8_t uv_row[6] = {1, 0, 1, -1, 2, 0};
    static const int8_t uv_column[6] = {0, 1, 1, 1, 0, 2};
    static const int8_t luma_row[12] = {1, -1, 0, 0, 1, -1, -1, 1, 2, -2, 0, 0};
    static const int8_t luma_column[12] = {0, 0, 1, -1, 1, -1, 1, -1, 0, 0, 2, -2};
    int row;
    for (row = y0; row < y1; ++row) {
        int column;
        for (column = x0; column < x1; ++column) {
            int center = AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                          pc_wiener_source_sample_u16)(source, row, column);
            int luma_center = AV2H_KERNEL_NAME(
                wiener_nonsep_luma_sample,
                wiener_nonsep_luma_sample_u16)(luma, luma_filter_index, chroma_shift, row, column);
            int64_t value = (int64_t)center << 7U;
            unsigned int tap;
            for (tap = 0U; tap < 6U; ++tap) {
                value +=
                    (int64_t)taps[tap] *
                    ((int)AV2H_KERNEL_NAME(pc_wiener_source_sample,
                                           pc_wiener_source_sample_u16)(source, row + uv_row[tap],
                                                  column + uv_column[tap]) +
                     AV2H_KERNEL_NAME(
                         pc_wiener_source_sample,
                         pc_wiener_source_sample_u16)(source, row - uv_row[tap], column - uv_column[tap]) -
                     2 * center);
            }
            for (tap = 0U; tap < 12U; ++tap) {
                value += (int64_t)taps[tap + 6U] *
                         ((int)AV2H_KERNEL_NAME(
                              wiener_nonsep_luma_sample,
                              wiener_nonsep_luma_sample_u16)(luma, luma_filter_index, chroma_shift,
                                                         row + luma_row[tap],
                                                         column + luma_column[tap]) -
                          luma_center);
            }
            destination->data[(size_t)row * destination->stride_samples + (size_t)column] =
                (AV2H_KERNEL_TYPE(
                    uint8_t,
                    uint16_t))av2h_sample_clip_to_domain(av2h_sample_round_pow2_signed(value, 7),
                                                    domain);
        }
    }
}

Av2hStatus AV2H_KERNEL_NAME(av2h_wiener_nonsep_filter_rows, av2h_wiener_nonsep_filter_rows_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *destination,
                                          const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *source,
                                          const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *deblocked,
                                          const Av2hSequenceState *sequence,
                                          const Av2hFrameState *frame,
                                          const Av2hRestorationControlMap *controls,
                                          size_t row_begin, size_t row_end
#if AV2H_KERNEL_WIDE
                                          ,
                                          const Av2hSampleDomain *domain
#endif
) {
#if !AV2H_KERNEL_WIDE
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    unsigned int plane;
    if (!av2h_sample_domain_valid(domain) || !filter_rows_valid(row_begin, row_end) ||
        destination == NULL || source == NULL || deblocked == NULL || sequence == NULL ||
        frame == NULL || controls == NULL || frame->chroma_format_idc > 2U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *input = &source->plane[plane];
        const AV2H_KERNEL_TYPE(Av2hFilterPlane,
                               Av2hFilterPlaneU16) *deblocked_plane = &deblocked->plane[plane];
        AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *output = &destination->plane[plane];
        size_t unit_row;
        unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
        int stripe_size = RESTORATION_PROC_UNIT_SIZE >> shift;
        int unit_offset = AV2H_PC_WIENER_UNIT_OFFSET >> shift;
        unsigned int class_count = frame->restoration_num_filter_classes[plane];
        const uint8_t *class_map = NULL;
        if (frame->restoration_type[plane] != AV2H_RESTORE_WIENER_NONSEP &&
            frame->restoration_type[plane] != AV2H_RESTORE_SWITCHABLE) {
            continue;
        }
        if (input->data == NULL || deblocked_plane->data == NULL || output->data == NULL ||
            input->width == 0U || input->height == 0U || input->width != output->width ||
            input->height != output->height || input->width != deblocked_plane->width ||
            input->height != deblocked_plane->height || input->stride_samples < input->width ||
            output->stride_samples < output->width ||
            deblocked_plane->stride_samples < deblocked_plane->width) {
            return AV2H_UNSUPPORTED;
        }
        if (plane != 0U && (source->plane[0].data == NULL || deblocked->plane[0].data == NULL ||
                            source->plane[0].width == 0U || source->plane[0].height == 0U ||
                            source->plane[0].width != deblocked->plane[0].width ||
                            source->plane[0].height != deblocked->plane[0].height ||
                            source->plane[0].stride_samples < source->plane[0].width ||
                            deblocked->plane[0].stride_samples < deblocked->plane[0].width)) {
            return AV2H_UNSUPPORTED;
        }
        if (frame->restoration_type[plane] != AV2H_RESTORE_SWITCHABLE) {
            AV2H_KERNEL_NAME(
                filter_copy_rows,
                filter_copy_rows_u16)(output, input, row_begin >> shift, row_end >> shift);
        }
        if (class_count == 0U || class_count > AV2H_WIENER_NONSEP_MAX_CLASSES ||
            controls->unit_flags[plane] == NULL || controls->unit_taps[plane] == NULL ||
            controls->unit_columns[plane] == 0U || controls->unit_rows[plane] == 0U ||
            controls->unit_size[plane] == 0U) {
            return AV2H_UNSUPPORTED;
        }
        if (class_count > 1U) {
            if (plane != 0U || frame->restoration_frame_filters_on[plane] == 0U ||
                frame->restoration_frame_filter == NULL || controls->tx_skip == NULL ||
                controls->tx_skip_stride < ((size_t)input->width + 3U) / 4U) {
                return AV2H_UNSUPPORTED;
            }
            class_map = av2h_wienerns_class_map(class_count,
                                                pc_wiener_filter_set_index(frame->base_q_index));
        }
        for (unit_row = 0U; unit_row < controls->unit_rows[plane]; ++unit_row) {
            int unit_y0 =
                unit_row == 0U ? 0 : (int)(unit_row * controls->unit_size[plane]) - unit_offset;
            int unit_y1 = unit_row + 1U == controls->unit_rows[plane]
                              ? input->height
                              : (int)((unit_row + 1U) * controls->unit_size[plane]) - unit_offset;
            size_t unit_column;
            for (unit_column = 0U; unit_column < controls->unit_columns[plane]; ++unit_column) {
                size_t unit_index = unit_row * controls->unit_columns[plane] + unit_column;
                int unit_x0 = (int)(unit_column * controls->unit_size[plane]);
                int unit_x1 = unit_column + 1U == controls->unit_columns[plane]
                                  ? input->width
                                  : unit_x0 + controls->unit_size[plane];
                int stripe_start;
                const int16_t *taps =
                    controls->unit_taps[plane] + unit_index * AV2H_WIENER_NONSEP_MAX_TAPS;
                if (controls->unit_flags[plane][unit_index] != AV2H_RESTORE_WIENER_NONSEP)
                    continue;
                for (stripe_start = unit_y0; stripe_start < unit_y1;) {
                    int relative_stripe = (stripe_start + unit_offset) / stripe_size;
                    int stripe_height = stripe_size - (relative_stripe == 0 ? unit_offset : 0);
                    AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) context;
                    int output_y0;
                    int output_y1;
                    if (stripe_height > unit_y1 - stripe_start) {
                        stripe_height = unit_y1 - stripe_start;
                    }
                    context = (AV2H_KERNEL_NAME(
                        Av2hPcWienerSource,
                        Av2hPcWienerSourceU16)){input, deblocked_plane, stripe_start,
                                                   stripe_start + stripe_height};
                    output_y0 = stripe_start;
                    output_y1 = stripe_start + stripe_height;
                    if ((size_t)output_y0 < (row_begin >> shift))
                        output_y0 = (int)(row_begin >> shift);
                    if ((size_t)output_y1 > (row_end >> shift))
                        output_y1 = (int)(row_end >> shift);
                    if (output_y0 >= output_y1) {
                        stripe_start += stripe_height;
                        continue;
                    }
                    if (class_count > 1U) {
                        int chunk_start;
                        for (chunk_start = unit_x0; chunk_start < unit_x1;
                             chunk_start += RESTORATION_PROC_UNIT_SIZE) {
                            int chunk_width = unit_x1 - chunk_start;
                            int block_row;
                            if (chunk_width > RESTORATION_PROC_UNIT_SIZE) {
                                chunk_width = RESTORATION_PROC_UNIT_SIZE;
                            }
                            for (block_row = 0; block_row < stripe_height;
                                 block_row += AV2H_PC_WIENER_BLOCK_SIZE) {
                                int block_column;
                                if (stripe_start + block_row < output_y0 ||
                                    stripe_start + block_row >= output_y1)
                                    continue;
                                for (block_column = 0; block_column < chunk_width;
                                     block_column += AV2H_PC_WIENER_BLOCK_SIZE) {
                                    uint8_t full_class = AV2H_KERNEL_NAME(pc_wiener_classify,
                                                                          pc_wiener_classify_u16)(
                                        &context, controls, frame->base_q_index, chunk_start,
                                        chunk_width, block_row, block_column, domain);
                                    unsigned int sub_class = class_map[full_class];
                                    int block_y = stripe_start + block_row;
                                    int block_x = chunk_start + block_column;
                                    int block_y1 = block_y + AV2H_PC_WIENER_BLOCK_SIZE;
                                    int block_x1 = block_x + AV2H_PC_WIENER_BLOCK_SIZE;
                                    if (sub_class >= class_count) {
                                        return AV2H_INVALID_DATA;
                                    }
                                    if (block_y1 > stripe_start + stripe_height) {
                                        block_y1 = stripe_start + stripe_height;
                                    }
                                    if (block_x1 > chunk_start + chunk_width) {
                                        block_x1 = chunk_start + chunk_width;
                                    }
                                    AV2H_KERNEL_NAME(wiener_nonsep_filter_luma_region,
                                                     wiener_nonsep_filter_luma_region_u16)(
                                        output, &context, block_y, block_y1, block_x, block_x1,
                                        frame->restoration_frame_filter[plane][sub_class], domain);
                                }
                            }
                        }
                    } else if (plane == 0U) {
                        AV2H_KERNEL_NAME(
                            wiener_nonsep_filter_luma_region,
                            wiener_nonsep_filter_luma_region_u16)(output, &context, output_y0, output_y1,
                                                         unit_x0, unit_x1, taps, domain);
                    } else {
                        AV2H_KERNEL_NAME(Av2hPcWienerSource, Av2hPcWienerSourceU16) luma = {&source->plane[0], &deblocked->plane[0],
                                                   stripe_start << shift,
                                                   (stripe_start + stripe_height) << shift};
                        AV2H_KERNEL_NAME(wiener_nonsep_filter_chroma_region,
                                         wiener_nonsep_filter_chroma_region_u16)(
                            output, &context, &luma, sequence->cfl_downsample_filter_index, shift,
                            output_y0, output_y1, unit_x0, unit_x1, taps, domain);
                    }
                    stripe_start += stripe_height;
                }
            }
        }
    }
    return AV2H_OK;
}