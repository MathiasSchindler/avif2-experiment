static AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    gdf_source_sample,
    gdf_source_sample_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *cdef_luma,
                                 const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked_luma, int x, int y,
                                 int stripe_start, int stripe_end, const GdfTileBounds *bounds) {
    const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source = cdef_luma;
    if (x < bounds->left)
        x = bounds->left;
    if (x >= bounds->right)
        x = bounds->right - 1;
    if (y < bounds->top)
        y = bounds->top;
    if (y >= bounds->bottom)
        y = bounds->bottom - 1;
    if (y < stripe_start) {
        int minimum = stripe_start - 2;
        if (y < minimum)
            y = minimum;
        source = deblocked_luma;
    } else if (y > stripe_end) {
        int maximum = stripe_end + 2;
        if (y > maximum)
            y = maximum;
        source = deblocked_luma;
    }
    return source->data[(size_t)y * source->stride_samples + (size_t)x];
}
static void AV2H_KERNEL_NAME(gdf_filter_block, gdf_filter_block_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination, const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *cdef_luma,
                             const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked_luma, uint16_t block_x,
                             uint16_t block_y, unsigned int qp_index, unsigned int pixel_scale,
                             const GdfTileBounds *bounds
#if AV2H_KERNEL_WIDE
                             ,
                             const Av2hSampleDomain *domain
#endif
) {
    uint16_t gradient[4][6][6];
    unsigned int gradient_sums[4][2][2];
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) samples[16][16];
    uint8_t classes[2][2];
    unsigned int stripe_number = ((unsigned int)block_y + 8U) / 64U;
    int stripe_start = (int)(stripe_number * 64U) - 8;
    int stripe_end = stripe_start + 63;
    unsigned int row;
    if (stripe_start < bounds->top)
        stripe_start = bounds->top;
    if (stripe_end >= bounds->bottom)
        stripe_end = bounds->bottom - 1;
    for (row = 0U; row < 16U; ++row) {
        unsigned int column;
        for (column = 0U; column < 16U; ++column) {
            samples[row][column] = AV2H_KERNEL_NAME(
                gdf_source_sample,
                gdf_source_sample_u16)(cdef_luma, deblocked_luma, (int)block_x - 6 + (int)column,
                                  (int)block_y - 6 + (int)row, stripe_start, stripe_end, bounds);
        }
    }
    for (row = 0U; row < 6U; ++row) {
        unsigned int column;
        for (column = 0U; column < 6U; ++column) {
            unsigned int direction;
            for (direction = 0U; direction < 4U; ++direction) {
                static const int8_t direction_offset[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
                int dx = direction_offset[direction][0];
                int dy = direction_offset[direction][1];
                int x = (int)column + 5;
                int y = (int)row + 5;
                int center = samples[y][x];
                int first = samples[y - dy][x - dx];
                int second = samples[y + dy][x + dx];
                gradient[direction][row][column] =
                    (uint16_t)av2h_sample_abs(center * 2 - first - second);
            }
        }
    }
    for (row = 0U; row < 2U; ++row) {
        unsigned int column;
        for (column = 0U; column < 2U; ++column) {
            unsigned int vertical = gdf_gradient_sum(gradient, 0U, row * 2U, column * 2U);
            unsigned int horizontal = gdf_gradient_sum(gradient, 1U, row * 2U, column * 2U);
            unsigned int diagonal_zero = gdf_gradient_sum(gradient, 2U, row * 2U, column * 2U);
            unsigned int diagonal_one = gdf_gradient_sum(gradient, 3U, row * 2U, column * 2U);
            gradient_sums[0][row][column] = vertical;
            gradient_sums[1][row][column] = horizontal;
            gradient_sums[2][row][column] = diagonal_zero;
            gradient_sums[3][row][column] = diagonal_one;
            classes[row][column] = (uint8_t)((vertical > horizontal ? 0U : 1U) |
                                             (diagonal_zero > diagonal_one ? 0U : 2U));
        }
    }
    for (row = 0U; row < 4U; ++row) {
        unsigned int column;
        for (column = 0U; column < 4U; ++column) {
            unsigned int class_index = classes[row >> 1U][column >> 1U];
            int accumulator[3] = {0, 0, 0};
            unsigned int kernel_index;
            unsigned int output_index;
            size_t destination_index =
                ((size_t)block_y + row) * destination->stride_samples + block_x + column;
            int center = samples[row + 6U][column + 6U];
            for (kernel_index = 0U; kernel_index < 22U; ++kernel_index) {
                unsigned int alpha =
                    gdf_alpha[gdf_alpha_index(qp_index, kernel_index, class_index)];
                int combination;
                if (kernel_index < 18U) {
                    int dy = gdf_coordinates[kernel_index][0];
                    int dx = gdf_coordinates[kernel_index][1];
                    int above = samples[(int)row + 6 - dy][(int)column + 6 - dx] - center;
                    int below = samples[(int)row + 6 + dy][(int)column + 6 + dx] - center;
#if AV2H_KERNEL_WIDE
                    /* Learned spatial inputs use 10-bit units, independently of sample depth. */
                    if (domain->bit_depth < 10U) {
                        above *= 1 << (10U - domain->bit_depth);
                        below *= 1 << (10U - domain->bit_depth);
                    } else if (domain->bit_depth > 10U) {
                        above = av2h_sample_round_pow2_signed(above, domain->bit_depth - 10U);
                        below = av2h_sample_round_pow2_signed(below, domain->bit_depth - 10U);
                    }
#else
                    above *= 4;
                    below *= 4;
#endif
                    above = av2h_sample_clamp(above, -(int)alpha, (int)alpha);
                    below = av2h_sample_clamp(below, -(int)alpha, (int)alpha);
                    combination = av2h_sample_clamp(above + below, -512, 511);
                } else {
#if AV2H_KERNEL_WIDE
                    unsigned int value =
                        gradient_sums[kernel_index - 18U][row >> 1U][column >> 1U] >>
                        (domain->bit_depth - 6U);
#else
                    unsigned int value =
                        gradient_sums[kernel_index - 18U][row >> 1U][column >> 1U] >> 2U;
#endif
                    combination = value < alpha ? (int)value : (int)alpha;
                }
                for (output_index = 0U; output_index < 3U; ++output_index) {
                    accumulator[output_index] +=
                        combination * gdf_weight[gdf_weight_index(qp_index, output_index,
                                                                  kernel_index, class_index)];
                }
            }
            {
                unsigned int position = 0U;
                for (output_index = 0U; output_index < 3U; ++output_index) {
                    int value = av2h_sample_round_pow2_signed(
                        (accumulator[output_index] +
                         gdf_bias[gdf_bias_index(qp_index, output_index)]) *
                            8,
                        15U);
                    position = position * 16U + (unsigned int)(av2h_sample_clamp(value, -8, 7) + 8);
                }
#if AV2H_KERNEL_WIDE
                av2h_sample_clip_store_u16(
                    destination->data, destination->stride_samples, (size_t)block_x + column,
                    (size_t)block_y + row,
                    (int)destination->data[destination_index] +
                        av2h_sample_round_pow2_signed(
                            (int)gdf_intra_error[qp_index * 4096U + position] * (int)pixel_scale,
                            12U - domain->bit_depth),
                    domain);
#else
                av2h_sample_clip_store_u8(
                    destination->data, destination->stride_samples, (size_t)block_x + column,
                    (size_t)block_y + row,
                    (int)destination->data[destination_index] +
                        av2h_sample_round_pow2_signed(
                            (int)gdf_intra_error[qp_index * 4096U + position] * (int)pixel_scale,
                            4U));
#endif
            }
        }
    }
}
Av2hStatus AV2H_KERNEL_NAME(av2h_gdf_validate_frame, av2h_gdf_validate_frame_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination
#if !AV2H_KERNEL_WIDE
                                   ,
                                   const Av2hFilterPlane *baseline_luma
#endif
                                   ,
                                   const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *cdef_luma,
                                   const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked_luma,
                                   const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                   const Av2hGdfControlMap *controls
#if AV2H_KERNEL_WIDE
                                   ,
                                   const Av2hSampleDomain *domain
#endif
) {
    (void)sequence;
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#endif
    if (!AV2H_KERNEL_NAME(gdf_frame_arguments_valid,
                          gdf_frame_arguments_valid_u16)(destination,
#if !AV2H_KERNEL_WIDE
                                   baseline_luma,
#endif
                                   cdef_luma, deblocked_luma, frame)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (frame->gdf_mode == 2U) {
        size_t index;
        if (controls == NULL || controls->flags == NULL ||
            controls->block_size != frame->gdf_block_size ||
            controls->columns != frame->gdf_block_columns ||
            controls->rows != frame->gdf_block_rows ||
            controls->stride_blocks < controls->columns ||
            controls->columns !=
                ((size_t)frame->width + controls->block_size - 1U) / controls->block_size ||
            controls->rows !=
                ((size_t)frame->height + controls->block_size - 1U) / controls->block_size) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < controls->rows; ++index) {
            size_t column;
            for (column = 0U; column < controls->columns; ++column) {
                if (controls->flags[index * controls->stride_blocks + column] > 1U) {
                    return AV2H_INVALID_ARGUMENT;
                }
            }
        }
    }
    return AV2H_OK;
}

/* Geometry, controls, and disjoint writable storage must be validated first.
 * The two immutable source planes may share storage in the stage router. */
void AV2H_KERNEL_NAME(av2h_gdf_apply_frame, av2h_gdf_apply_frame_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination,
#if !AV2H_KERNEL_WIDE
                          const Av2hFilterPlane *baseline_luma,
#endif
                          const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *cdef_luma, const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked_luma,
                          const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                          const Av2hGdfControlMap *controls
#if AV2H_KERNEL_WIDE
                          ,
                          const Av2hSampleDomain *domain
#endif
) {
    uint16_t row;
    unsigned int qp_index;
#if AV2H_KERNEL_WIDE
    qp_index = gdf_training_qp_index(frame, domain->bit_depth);
    av2h_plane_copy_u16(destination->data, destination->stride_samples, cdef_luma->data,
                        cdef_luma->stride_samples, destination->width, destination->height);
#else
    qp_index = gdf_training_qp_index(frame, 8U);
    av2h_plane_copy_u8(destination->data, destination->stride_samples, baseline_luma->data,
                       baseline_luma->stride_samples, destination->width, destination->height);
#endif
    if (frame->gdf_mode == 0U)
        return;
    for (row = 0U; row < destination->height; row = (uint16_t)(row + 4U)) {
        uint16_t column;
        for (column = 0U; column < destination->width; column = (uint16_t)(column + 4U)) {
            GdfTileBounds bounds = gdf_tile_bounds(sequence, frame, column, row);
            if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U ||
                (uint32_t)frame->tile_column_count * frame->tile_row_count <= 1U) {
                bounds.right = cdef_luma->width;
                bounds.bottom = cdef_luma->height;
            }
            if (frame->gdf_mode == 2U) {
                size_t control_row = ((size_t)row + 8U) / controls->block_size;
                if (control_row >= controls->rows) {
                    control_row = controls->rows - 1U;
                }
                if (controls->flags[control_row * controls->stride_blocks +
                                    (size_t)column / controls->block_size] == 0U) {
                    continue;
                }
            }
#if AV2H_KERNEL_WIDE
            gdf_filter_block_u16(destination, cdef_luma, deblocked_luma, column, row, qp_index,
                                 (unsigned int)frame->gdf_scale_index + 1U, &bounds, domain);
#else
            gdf_filter_block(destination, cdef_luma, deblocked_luma, column, row, qp_index,
                             (unsigned int)frame->gdf_scale_index + 1U, &bounds);
#endif
        }
    }
}

Av2hStatus AV2H_KERNEL_NAME(av2h_gdf_filter_frame, av2h_gdf_filter_frame_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination,
#if !AV2H_KERNEL_WIDE
                                 const Av2hFilterPlane *baseline_luma,
#endif
                                 const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *cdef_luma,
                                 const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *deblocked_luma,
                                 const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                 const Av2hGdfControlMap *controls
#if AV2H_KERNEL_WIDE
                                 ,
                                 const Av2hSampleDomain *domain
#endif
) {
    Av2hStatus status = AV2H_KERNEL_NAME(
        av2h_gdf_validate_frame,
        av2h_gdf_validate_frame_u16)(destination,
#if !AV2H_KERNEL_WIDE
                                                baseline_luma,
#endif
                                                cdef_luma, deblocked_luma, sequence, frame, controls
#if AV2H_KERNEL_WIDE
                                                ,
                                                domain
#endif
    );
    if (status != AV2H_OK)
        return status;
    AV2H_KERNEL_NAME(av2h_gdf_apply_frame,
                     av2h_gdf_apply_frame_u16)(destination,
#if !AV2H_KERNEL_WIDE
                         baseline_luma,
#endif
                         cdef_luma, deblocked_luma, sequence, frame, controls
#if AV2H_KERNEL_WIDE
                         ,
                         domain
#endif
    );
    return AV2H_OK;
}