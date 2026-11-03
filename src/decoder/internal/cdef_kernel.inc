static AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) AV2H_KERNEL_NAME(
    cdef_plane_view,
    cdef_plane_view_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *plane, const CdefTileBounds *bounds,
                                       unsigned int shift) {
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) view = *plane;
    size_t left = bounds->left >> shift;
    size_t top = bounds->top >> shift;
    view.data += top * view.stride_samples + left;
    view.width = (uint16_t)((bounds->right >> shift) - left);
    view.height = (uint16_t)((bounds->bottom >> shift) - top);
    return view;
}
Av2hStatus AV2H_KERNEL_NAME(
    av2h_cdef_find_direction,
    av2h_cdef_find_direction_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *luma, uint16_t x, uint16_t y,
                                    uint8_t *direction, uint32_t *variance
#if AV2H_KERNEL_WIDE
                                    ,
                                    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    static const unsigned int divisors[9] = {0U, 840U, 420U, 280U, 210U, 168U, 140U, 120U, 105U};
    int partial[8][15];
    int64_t cost[8];
    int64_t best_cost = 0;
    unsigned int best_direction = 0U;
    unsigned int row;
    unsigned int column;
    unsigned int index;
    if (luma == NULL || luma->data == NULL || luma->stride_samples < luma->width ||
        direction == NULL || variance == NULL || (uint32_t)x + 8U > luma->width ||
        (uint32_t)y + 8U > luma->height) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 8U; ++index) {
        unsigned int line;
        cost[index] = 0;
        for (line = 0U; line < 15U; ++line)
            partial[index][line] = 0;
    }
    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
#if AV2H_KERNEL_WIDE
            int value = (int)(luma->data[((size_t)y + row) * luma->stride_samples + x + column] >>
                              (domain->bit_depth - 8U)) -
                        av2h_sample_midpoint_8bit();
#else
            int value = (int)luma->data[((size_t)y + row) * luma->stride_samples + x + column] -
                        domain->midpoint;
#endif
            partial[0][row + column] += value;
            partial[1][row + column / 2U] += value;
            partial[2][row] += value;
            partial[3][3U + row - column / 2U] += value;
            partial[4][7U + row - column] += value;
            partial[5][3U - row / 2U + column] += value;
            partial[6][column] += value;
            partial[7][row / 2U + column] += value;
        }
    }
    for (index = 0U; index < 8U; ++index) {
        cost[2] += (int64_t)partial[2][index] * partial[2][index];
        cost[6] += (int64_t)partial[6][index] * partial[6][index];
    }
    cost[2] *= divisors[8];
    cost[6] *= divisors[8];
    for (index = 0U; index < 7U; ++index) {
        cost[0] += ((int64_t)partial[0][index] * partial[0][index] +
                    (int64_t)partial[0][14U - index] * partial[0][14U - index]) *
                   divisors[index + 1U];
        cost[4] += ((int64_t)partial[4][index] * partial[4][index] +
                    (int64_t)partial[4][14U - index] * partial[4][14U - index]) *
                   divisors[index + 1U];
    }
    cost[0] += (int64_t)partial[0][7] * partial[0][7] * divisors[8];
    cost[4] += (int64_t)partial[4][7] * partial[4][7] * divisors[8];
    for (index = 1U; index < 8U; index += 2U) {
        unsigned int line;
        for (line = 0U; line < 5U; ++line) {
            cost[index] += (int64_t)partial[index][3U + line] * partial[index][3U + line];
        }
        cost[index] *= divisors[8];
        for (line = 0U; line < 3U; ++line) {
            cost[index] += ((int64_t)partial[index][line] * partial[index][line] +
                            (int64_t)partial[index][10U - line] * partial[index][10U - line]) *
                           divisors[2U * line + 2U];
        }
    }
    for (index = 0U; index < 8U; ++index) {
        if (cost[index] > best_cost) {
            best_cost = cost[index];
            best_direction = index;
        }
    }
    *direction = (uint8_t)best_direction;
    *variance = (uint32_t)((best_cost - cost[(best_direction + 4U) & 7U]) >> 10U);
    return AV2H_OK;
}
static int AV2H_KERNEL_NAME(
    cdef_tap,
    cdef_tap_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source, int x, int y, int center, unsigned int strength,
                    unsigned int damping, int *minimum, int *maximum, int clipping_required,
                    int weight) {
    int sample;
    if (x < 0 || y < 0 || x >= (int)source->width || y >= (int)source->height) {
        return 0;
    }
    sample = source->data[(size_t)y * source->stride_samples + (size_t)x];
    if (clipping_required) {
        if (sample < *minimum)
            *minimum = sample;
        if (sample > *maximum)
            *maximum = sample;
    }
    return weight * cdef_constrain(sample - center, strength, damping);
}
Av2hStatus AV2H_KERNEL_NAME(av2h_cdef_filter_block, av2h_cdef_filter_block_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *destination, const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *source,
                                  uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                  unsigned int plane_index, uint8_t primary_strength,
                                  uint8_t secondary_strength, uint8_t damping, uint8_t direction,
                                  uint32_t variance
#if AV2H_KERNEL_WIDE
                                  ,
                                  const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#endif
    static const int8_t directions[8][2][2] = {
        {{-1, 1}, {-2, 2}}, {{0, 1}, {-1, 2}}, {{0, 1}, {0, 2}}, {{0, 1}, {1, 2}},
        {{1, 1}, {2, 2}},   {{1, 0}, {2, 1}},  {{1, 0}, {2, 0}}, {{1, 0}, {2, -1}}};
    static const uint8_t primary_taps[2][2] = {{4U, 2U}, {3U, 3U}};
#if AV2H_KERNEL_WIDE
    unsigned int adjusted_primary = (unsigned int)primary_strength << (domain->bit_depth - 8U);
    unsigned int adjusted_secondary = (unsigned int)secondary_strength << (domain->bit_depth - 8U);
#else
    uint8_t adjusted_primary = primary_strength;
#endif
    unsigned int tap_set;
    int clipping_required;
    uint16_t row;
    if (destination == NULL || source == NULL || destination->data == NULL ||
        source->data == NULL || destination->stride_samples < destination->width ||
        source->stride_samples < source->width || plane_index >= 3U || primary_strength > 15U ||
        (secondary_strength != 0U && secondary_strength != 1U && secondary_strength != 2U &&
         secondary_strength != 4U) ||
        damping > 6U || direction >= 8U || width == 0U || height == 0U || width > 8U ||
        height > 8U || destination->width != source->width ||
        destination->height != source->height || (uint32_t)x + width > source->width ||
        (uint32_t)y + height > source->height) {
        return AV2H_INVALID_ARGUMENT;
    }
#if AV2H_KERNEL_WIDE
    damping = (uint8_t)(damping + domain->bit_depth - 8U - (plane_index == 0U ? 0U : 1U));
#endif
    if (plane_index == 0U) {
        if (variance == 0U) {
            adjusted_primary = 0U;
        } else {
            unsigned int strength_scale = variance >> 6U;
            strength_scale = strength_scale == 0U ? 0U : av2h_floor_log2_u32(strength_scale);
            if (strength_scale > 12U)
                strength_scale = 12U;
#if AV2H_KERNEL_WIDE
            adjusted_primary = (adjusted_primary * (4U + strength_scale) + 8U) >> 4U;
#else
            adjusted_primary = (uint8_t)((primary_strength * (4U + strength_scale) + 8U) >> 4U);
#endif
        }
    }
#if !AV2H_KERNEL_WIDE
    if (plane_index != 0U && damping != 0U) {
        --damping;
    }
#endif
#if AV2H_KERNEL_WIDE
    tap_set = (adjusted_primary >> (domain->bit_depth - 8U)) & 1U;
    clipping_required = adjusted_primary != 0U && adjusted_secondary != 0U;
#else
    tap_set = adjusted_primary & 1U;
    clipping_required = adjusted_primary != 0U && secondary_strength != 0U;
#endif
    for (row = 0U; row < height; ++row) {
        uint16_t column;
        for (column = 0U; column < width; ++column) {
            int sample_x = (int)x + column;
            int sample_y = (int)y + row;
            int center = source->data[(size_t)sample_y * source->stride_samples + (size_t)sample_x];
            int minimum = center;
            int maximum = center;
            int sum = 0;
            unsigned int distance;
            for (distance = 0U; distance < 2U; ++distance) {
                int sign;
                for (sign = -1; sign <= 1; sign += 2) {
                    unsigned int secondary_index;
                    int primary_y = directions[direction][distance][0];
                    int primary_x = directions[direction][distance][1];
                    sum += AV2H_KERNEL_NAME(
                        cdef_tap,
                        cdef_tap_u16)(source, sample_x + sign * primary_x, sample_y + sign * primary_y,
                                 center, adjusted_primary, damping, &minimum, &maximum,
                                 clipping_required, primary_taps[tap_set][distance]);
                    for (secondary_index = 0U; secondary_index < 2U; ++secondary_index) {
                        unsigned int secondary_direction =
                            (direction + (secondary_index == 0U ? 2U : 6U)) & 7U;
                        int secondary_y = directions[secondary_direction][distance][0];
                        int secondary_x = directions[secondary_direction][distance][1];
#if AV2H_KERNEL_WIDE
                        sum += cdef_tap_u16(source, sample_x + sign * secondary_x,
                                            sample_y + sign * secondary_y, center,
                                            adjusted_secondary, damping, &minimum, &maximum,
                                            clipping_required, 2 - (int)distance);
#else
                        sum += cdef_tap(source, sample_x + sign * secondary_x,
                                        sample_y + sign * secondary_y, center, secondary_strength,
                                        damping, &minimum, &maximum, clipping_required,
                                        2 - (int)distance);
#endif
                    }
                }
            }
            center += av2h_sample_floor_shift(8 + sum - (sum < 0), 4U);
            if (clipping_required) {
                center = av2h_sample_clamp(center, minimum, maximum);
            }
#if AV2H_KERNEL_WIDE
            av2h_sample_clip_store_u16(destination->data, destination->stride_samples,
                                       (size_t)sample_x, (size_t)sample_y, center, domain);
#else
            av2h_sample_clip_store_u8(destination->data, destination->stride_samples,
                                      (size_t)sample_x, (size_t)sample_y, center);
#endif
        }
    }
    return AV2H_OK;
}
static Av2hStatus AV2H_KERNEL_NAME(
    cdef_partial_direction,
    cdef_partial_direction_u16)(const AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) *luma, uint16_t x, uint16_t y,
                                         uint8_t *direction, uint32_t *variance
#if AV2H_KERNEL_WIDE
                                         ,
                                         const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#endif
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) samples[64];
    AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) block = {samples, 8U, 8U, 8U};
    unsigned int row;
    for (row = 0U; row < 8U; ++row) {
        unsigned int column;
        uint16_t source_y = (uint16_t)(y + row);
        if (source_y >= luma->height)
            source_y = luma->height - 1U;
        for (column = 0U; column < 8U; ++column) {
            uint16_t source_x = (uint16_t)(x + column);
            if (source_x >= luma->width)
                source_x = luma->width - 1U;
            samples[row * 8U + column] =
                luma->data[(size_t)source_y * luma->stride_samples + source_x];
        }
    }
#if AV2H_KERNEL_WIDE
    return av2h_cdef_find_direction_u16(&block, 0U, 0U, direction, variance, domain);
#else
    return av2h_cdef_find_direction(&block, 0U, 0U, direction, variance);
#endif
}
Av2hStatus AV2H_KERNEL_NAME(av2h_cdef_filter_rows, av2h_cdef_filter_rows_u16)(
    AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *destination, const AV2H_KERNEL_TYPE(Av2hFilterPlanes, Av2hFilterPlanesU16) *deblocked,
                                 const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                 const Av2hCdefControlMap *controls, size_t row_begin,
                                 size_t row_end
#if AV2H_KERNEL_WIDE
                                 ,
                                 const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#endif
    uint16_t y;
    unsigned int plane;
    if (!filter_rows_valid(row_begin, row_end) || destination == NULL || deblocked == NULL ||
        frame == NULL || frame->width == 0U || frame->height == 0U ||
        !AV2H_KERNEL_NAME(filter_dimensions_valid,
                          filter_dimensions_valid_u16)(destination, frame) ||
        !AV2H_KERNEL_NAME(filter_dimensions_valid, filter_dimensions_valid_u16)(deblocked, frame)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (frame->cdef_strength_count != 0U &&
        (frame->cdef_on_skip_transform == 0U || frame->cdef_damping < 3U)) {
        return AV2H_UNSUPPORTED;
    }
    if (frame->cdef_strength_count != 0U) {
        size_t strength_index;
        if (frame->cdef_damping > 6U)
            return AV2H_INVALID_ARGUMENT;
        for (strength_index = 0U; strength_index < frame->cdef_strength_count; ++strength_index) {
            if (frame->cdef_y_strength[strength_index] > 63U ||
                frame->cdef_uv_strength[strength_index] > 63U) {
                return AV2H_INVALID_ARGUMENT;
            }
        }
    }
    if (frame->cdef_strength_count > 1U &&
        (controls == NULL || controls->strengths == NULL ||
         controls->columns != ((size_t)frame->width + 63U) / 64U ||
         controls->rows != ((size_t)frame->height + 63U) / 64U)) {
        return AV2H_UNSUPPORTED;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (destination->plane[plane].data == deblocked->plane[plane].data) {
            return AV2H_INVALID_ARGUMENT;
        }
#if AV2H_KERNEL_WIDE
        filter_copy_rows_u16(&destination->plane[plane], &deblocked->plane[plane],
                             row_begin >> av2h_chroma_shift(frame->chroma_format_idc, plane),
                             row_end >> av2h_chroma_shift(frame->chroma_format_idc, plane));
#else
        filter_copy_rows(&destination->plane[plane], &deblocked->plane[plane],
                         row_begin >> av2h_chroma_shift(frame->chroma_format_idc, plane),
                         row_end >> av2h_chroma_shift(frame->chroma_format_idc, plane));
#endif
    }
    if (frame->cdef_strength_count == 0U)
        return AV2H_OK;
    for (y = (uint16_t)row_begin; y < deblocked->plane[0].height && y < row_end;
         y = (uint16_t)(y + 8U)) {
        uint16_t x;
        for (x = 0U; x < deblocked->plane[0].width; x = (uint16_t)(x + 8U)) {
            size_t strength_index = 0U;
            uint8_t y_primary;
            uint8_t y_secondary;
            uint8_t uv_primary;
            uint8_t uv_secondary;
            uint16_t width = (uint16_t)(deblocked->plane[0].width - x);
            uint16_t height = (uint16_t)(deblocked->plane[0].height - y);
            uint8_t direction;
            uint32_t variance;
            Av2hStatus status;
            CdefTileBounds bounds = cdef_tile_bounds(sequence, frame, x, y);
            if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U ||
                (uint32_t)frame->tile_column_count * frame->tile_row_count <= 1U) {
                bounds.right = deblocked->plane[0].width;
                bounds.bottom = deblocked->plane[0].height;
            }
            AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) luma_destination = AV2H_KERNEL_NAME(cdef_plane_view, cdef_plane_view_u16)(&destination->plane[0], &bounds, 0U);
            AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) luma_source = AV2H_KERNEL_NAME(cdef_plane_view,
                                           cdef_plane_view_u16)(&deblocked->plane[0], &bounds, 0U);
            uint16_t tile_x = (uint16_t)(x - bounds.left);
            uint16_t tile_y = (uint16_t)(y - bounds.top);
            if (frame->cdef_strength_count > 1U) {
                strength_index = ((size_t)y / 64U) * controls->columns + (size_t)x / 64U;
                strength_index = controls->strengths[strength_index];
                if (strength_index >= frame->cdef_strength_count) {
                    return AV2H_INVALID_DATA;
                }
            }
            y_primary = frame->cdef_y_strength[strength_index] / 4U;
            y_secondary = frame->cdef_y_strength[strength_index] % 4U;
            if (y_secondary == 3U)
                ++y_secondary;
            uv_primary = frame->cdef_uv_strength[strength_index] / 4U;
            uv_secondary = frame->cdef_uv_strength[strength_index] % 4U;
            if (uv_secondary == 3U)
                ++uv_secondary;
            if (width > 8U)
                width = 8U;
            if (height > 8U)
                height = 8U;
            if (width == 8U && height == 8U) {
#if AV2H_KERNEL_WIDE
                status = av2h_cdef_find_direction_u16(&luma_source, tile_x, tile_y, &direction,
                                                      &variance, domain);
#else
                status =
                    av2h_cdef_find_direction(&luma_source, tile_x, tile_y, &direction, &variance);
#endif
            } else {
#if AV2H_KERNEL_WIDE
                status = cdef_partial_direction_u16(&luma_source, tile_x, tile_y, &direction,
                                                    &variance, domain);
#else
                status =
                    cdef_partial_direction(&luma_source, tile_x, tile_y, &direction, &variance);
#endif
            }
            if (status != AV2H_OK)
                return status;
#if AV2H_KERNEL_WIDE
            status =
                av2h_cdef_filter_block_u16(&luma_destination, &luma_source, tile_x, tile_y, width,
                                           height, 0U, y_primary, y_secondary, frame->cdef_damping,
                                           y_primary != 0U ? direction : 0U, variance, domain);
#else
            status = av2h_cdef_filter_block(&luma_destination, &luma_source, tile_x, tile_y, width,
                                            height, 0U, y_primary, y_secondary, frame->cdef_damping,
                                            y_primary != 0U ? direction : 0U, variance);
#endif
            if (status != AV2H_OK)
                return status;
            for (plane = 1U; plane < av2h_frame_plane_count(frame); ++plane) {
                unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
                uint16_t chroma_x = (uint16_t)(x >> shift);
                uint16_t chroma_y = (uint16_t)(y >> shift);
                uint16_t chroma_width =
                    (uint16_t)(((uint32_t)width + ((uint32_t)1U << shift) - 1U) >> shift);
                uint16_t chroma_height =
                    (uint16_t)(((uint32_t)height + ((uint32_t)1U << shift) - 1U) >> shift);
                AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) chroma_destination = AV2H_KERNEL_NAME(cdef_plane_view, cdef_plane_view_u16)(&destination->plane[plane], &bounds, shift);
                AV2H_KERNEL_TYPE(Av2hFilterPlane, Av2hFilterPlaneU16) chroma_source = AV2H_KERNEL_NAME(cdef_plane_view, cdef_plane_view_u16)(&deblocked->plane[plane], &bounds, shift);
#if AV2H_KERNEL_WIDE
                status = av2h_cdef_filter_block_u16(
                    &chroma_destination, &chroma_source,
                    (uint16_t)(chroma_x - (bounds.left >> shift)),
                    (uint16_t)(chroma_y - (bounds.top >> shift)), chroma_width, chroma_height,
                    plane, uv_primary, uv_secondary, frame->cdef_damping,
                    uv_primary != 0U ? direction : 0U, variance, domain);
#else
                status = av2h_cdef_filter_block(&chroma_destination, &chroma_source,
                                                (uint16_t)(chroma_x - (bounds.left >> shift)),
                                                (uint16_t)(chroma_y - (bounds.top >> shift)),
                                                chroma_width, chroma_height, plane, uv_primary,
                                                uv_secondary, frame->cdef_damping,
                                                uv_primary != 0U ? direction : 0U, variance);
#endif
                if (status != AV2H_OK)
                    return status;
            }
        }
    }
    return AV2H_OK;
}