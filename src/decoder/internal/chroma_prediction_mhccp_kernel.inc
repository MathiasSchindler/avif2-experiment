static uint16_t AV2H_KERNEL_NAME(mhccp_luma_q3, mhccp_luma_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t x0 = (size_t)chroma_x * 2U;
    size_t y0 = (size_t)chroma_y * 2U;
    size_t x1 = x0 + 1U < luma_plane->width ? x0 + 1U : x0;
    size_t y1 = y0 + 1U < luma_plane->height ? y0 + 1U : y0;
    return (uint16_t)((luma_plane->data[y0 * luma_plane->stride_samples + x0] +
                       luma_plane->data[y0 * luma_plane->stride_samples + x1] +
                       luma_plane->data[y1 * luma_plane->stride_samples + x0] +
                       luma_plane->data[y1 * luma_plane->stride_samples + x1])
                      << 1U);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter2_q3, mhccp_luma_filter2_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ? center_y + 1U : center_y;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      4U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
                      luma_plane->data[top_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter2_top_clamped_q3,
                                 mhccp_luma_filter2_top_clamped_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ? center_y + 1U : center_y;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      5U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter1_q3, mhccp_luma_filter1_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ? center_y + 1U : center_y;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      2U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + left_x] +
                      2U * luma_plane->data[bottom_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + right_x]);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter1_sb_top_q3, mhccp_luma_filter1_sb_top_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    return (uint16_t)(2U *
                      (luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                       2U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                       luma_plane->data[center_y * luma_plane->stride_samples + right_x]));
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter2_sb_top_q3, mhccp_luma_filter2_sb_top_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      6U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x]);
}
Av2hStatus AV2H_KERNEL_NAME(av2h_mhccp_predict_observed, av2h_mhccp_predict_observed_u16)(
#if !AV2H_KERNEL_WIDE
    Av2hDecodeDiagnostics *diagnostics,
#endif
    const Av2hChromaLeafMode *mode,
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * chroma_plane,
    uint16_t block_x, uint16_t block_y, uint16_t width, uint16_t height,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples
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
    AV2H_KERNEL_TYPE(int32_t, int64_t) matrix[3U][3U] = {{0}};
    AV2H_KERNEL_TYPE(int32_t, int64_t) target[3U] = {0};
    int32_t parameters[3U];
    uint16_t prediction_width;
    uint16_t prediction_height;
#if AV2H_KERNEL_WIDE
    uint16_t top_count;
    uint16_t left_count;
    uint16_t top_start_x;
    uint16_t sample;
    uint16_t count = 0U;
#endif
    uint16_t row;
#if AV2H_KERNEL_WIDE
    uint16_t column;
    uint8_t have_top;
    uint8_t have_left;
    uint8_t is_top_sb_boundary;
    int32_t top_left_center = 0;
    if (mode != NULL && mode->chroma_format_idc == 2U) {
        return mhccp_predict_u16_444(mode, luma_plane, chroma_plane, block_x, block_y, width,
                                     height, predictor, predictor_stride_samples, domain);
#else
    if (mode->mhccp_direction <= 2U && av2h_chroma_geometry_supported(width, height) &&
        luma_plane != NULL && luma_plane->data != NULL && luma_plane->reconstructed != NULL &&
        luma_plane->stride_samples >= luma_plane->width) {
        return mhccp_predict_direction01(
            diagnostics, luma_plane, chroma_plane, mode->cfl_downsample_filter_index,
            mode->mhccp_direction, block_x, block_y, width, height, mode->superblock_size,
            (uint8_t)(mode->chroma_format_idc == 2U), predictor, predictor_stride_samples);
#endif
    }
#if AV2H_KERNEL_WIDE
    if (mode == NULL || luma_plane == NULL || chroma_plane == NULL || predictor == NULL ||
        luma_plane->data == NULL || chroma_plane->data == NULL ||
        mode->cfl_kind != AV2H_CFL_MULTI_PARAM || mode->mhccp_direction > 2U ||
        mode->cfl_downsample_filter_index > 2U || mode->chroma_format_idc != 0U ||
        !av2h_chroma_geometry_supported(width, height) || predictor_stride_samples < width ||
        block_x >= chroma_plane->width || block_y >= chroma_plane->height) {
#else
    if (mode->mhccp_direction != 2U || block_y != 0U || block_x < 2U || width != 16U ||
        height != 32U || luma_plane == NULL || luma_plane->data == NULL ||
        luma_plane->reconstructed == NULL || luma_plane->stride_samples < luma_plane->width ||
        (size_t)block_x * 2U >= luma_plane->width ||
        (size_t)(block_y + height) * 2U > luma_plane->height ||
        !av2h_chroma_plane_region_reconstructed(luma_plane, (uint16_t)(block_x * 2U - 4U), 0U,
                                                (uint16_t)(luma_plane->width - (block_x * 2U - 4U)),
                                                (uint16_t)(height * 2U)) ||
        !av2h_chroma_plane_region_reconstructed(chroma_plane, (uint16_t)(block_x - 2U), 0U, 2U,
                                                height)) {
#endif
        return AV2H_UNSUPPORTED;
    }
    prediction_width =
        width < chroma_plane->width - block_x ? width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y
                            ? height
                            : (uint16_t)(chroma_plane->height - block_y);
#if AV2H_KERNEL_WIDE
    if (!av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, (uint16_t)(block_x * 2U), (uint16_t)(block_y * 2U),
            (uint16_t)(prediction_width * 2U), (uint16_t)(prediction_height * 2U))) {
#else
    if ((size_t)(block_x + prediction_width) * 2U > luma_plane->width ||
        (size_t)(block_y + prediction_height) * 2U > luma_plane->height) {
#endif
        return AV2H_UNSUPPORTED;
    }
#if AV2H_KERNEL_WIDE
    have_top = (uint8_t)(block_y >= 2U);
    have_left = (uint8_t)(block_x >= 2U);
    is_top_sb_boundary = (uint8_t)(mode->superblock_size != 0U &&
                                   ((uint32_t)block_y * 2U) % mode->superblock_size == 0U);
    if (!have_top && !have_left) {
        for (row = 0U; row < prediction_height; ++row) {
            for (column = 0U; column < prediction_width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] = domain->midpoint;
            }
        }
        return AV2H_OK;
    }
    top_count = have_top ? (uint16_t)(width * 2U) : 0U;
    if (!have_left && top_count >= 2U)
        top_count = (uint16_t)(top_count - 2U);
    if (top_count > 62U)
        top_count = 62U;
    left_count = have_left ? (uint16_t)(height * 2U - 1U) : 0U;
    if (have_top && left_count > 61U)
        left_count = 61U;
    if (left_count >= chroma_plane->height - block_y) {
        left_count = (uint16_t)(chroma_plane->height - block_y - 1U);
    }
    if (have_left) {
        uint16_t available_left = 0U;
        while (available_left < left_count &&
               av2h_chroma_plane_sample_reconstructed_u16(chroma_plane, block_x - 1U,
                                                          (size_t)block_y + available_left)) {
            ++available_left;
        }
        if (available_left > height - 1U) {
            available_left = (uint16_t)(height - 1U + ((available_left - (height - 1U)) & ~1U));
        }
        left_count = available_left;
    }
    have_left = (uint8_t)(left_count != 0U);
    top_start_x = (uint16_t)(have_left ? block_x - 1U : block_x + 1U);
    if (have_top) {
        uint16_t available = 0U;
        uint16_t limit = chroma_plane->reference_width > top_start_x + 1U
                             ? (uint16_t)(chroma_plane->reference_width - top_start_x - 1U)
                             : 0U;
        if (top_count > limit)
            top_count = limit;
        while (available < top_count &&
               av2h_chroma_plane_sample_reconstructed_u16(
                   chroma_plane, (size_t)top_start_x + available, block_y - 1U)) {
            ++available;
        }
        if (!have_left && available != 0U && available <= width)
            --available;
        if (available > width) {
            available = (uint16_t)(width + ((available - width) & ~1U));
        }
        top_count = available;
        have_top = (uint8_t)(top_count != 0U);
    }
    for (sample = 0U; sample < top_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(top_start_x + sample);
        uint16_t chroma_y = (uint16_t)(block_y - 1U);
        int32_t center =
            (int32_t)((is_top_sb_boundary
                           ? av2h_chroma_luma_sb_top_sample_q3_u16(
                                 luma_plane, mode->cfl_downsample_filter_index, chroma_x, block_y)
                           : av2h_chroma_luma_sample_q3_u16(luma_plane,
                                                            mode->cfl_downsample_filter_index,
                                                            chroma_x, chroma_y)) >>
                      3U);
#else
    for (row = 1U; row + 1U < height; ++row) {
#endif
        int32_t vector[3U];
#if !AV2H_KERNEL_WIDE
        uint16_t center =
            (uint16_t)(mhccp_luma_q3(luma_plane, (uint16_t)(block_x - 1U), row) >> 3U);
#endif
        int first;
        int second;
#if AV2H_KERNEL_WIDE
        if (sample == 0U)
            top_left_center = center;
        vector[0] =
            mode->mhccp_direction == 0U ? center
            : mode->mhccp_direction == 1U
                ? (is_top_sb_boundary
                       ? center
                       : (int32_t)((mode->cfl_downsample_filter_index == 2U
                                        ? mhccp_luma_filter2_top_clamped_q3_u16(
                                              luma_plane, chroma_x, (uint16_t)(chroma_y - 1U))
                                        : av2h_chroma_luma_sample_q3_u16(
                                              luma_plane, mode->cfl_downsample_filter_index,
                                              chroma_x, (uint16_t)(chroma_y - 1U))) >>
                                   3U))
                : (int32_t)((is_top_sb_boundary
                                 ? (sample == 0U && have_left
                                        ? av2h_chroma_luma_sb_top_left_clamped_q3_u16(
                                              luma_plane, mode->cfl_downsample_filter_index,
                                              chroma_x == 0U ? 0U : (uint16_t)(chroma_x - 1U),
                                              block_y)
                                        : av2h_chroma_luma_sb_top_sample_q3_u16(
                                              luma_plane, mode->cfl_downsample_filter_index,
                                              chroma_x == 0U ? 0U : (uint16_t)(chroma_x - 1U),
                                              block_y))
                                 : (sample == 0U && have_left
                                        ? av2h_chroma_luma_left_clamped_q3_u16(
                                              luma_plane, mode->cfl_downsample_filter_index,
                                              (uint16_t)(chroma_x - 1U), chroma_y)
                                        : av2h_chroma_luma_sample_q3_u16(
                                              luma_plane, mode->cfl_downsample_filter_index,
                                              chroma_x == 0U ? 0U : (uint16_t)(chroma_x - 1U),
                                              chroma_y))) >>
                            3U);
        vector[1] = (center * center + domain->midpoint) >> domain->bit_depth;
        vector[2] = domain->midpoint;
#else
        vector[0] = (int32_t)(mhccp_luma_q3(luma_plane, (uint16_t)(block_x - 2U), row) >> 3U);
        vector[1] = (center * center + 128) >> 8U;
        vector[2] = 128;
#endif
        for (first = 0; first < 3; ++first) {
#if AV2H_KERNEL_WIDE
            target[first] +=
                vector[first] *
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride_samples + chroma_x];
#else
            target[first] +=
                vector[first] *
                chroma_plane->data[(size_t)row * chroma_plane->stride_samples + block_x - 1U];
#endif
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
#if AV2H_KERNEL_WIDE
        ++count;
#endif
    }
#if AV2H_KERNEL_WIDE
    for (sample = have_top ? 0U : 1U; sample < left_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(block_x - 1U);
        uint16_t chroma_y = (uint16_t)(block_y + sample);
        int32_t center =
            (int32_t)(av2h_chroma_luma_sample_q3_u16(luma_plane, mode->cfl_downsample_filter_index,
                                                     chroma_x, chroma_y) >>
                      3U);
        int32_t vector[3U];
        int first;
        int second;
        vector[0] =
            mode->mhccp_direction == 0U ? center
            : mode->mhccp_direction == 1U
                ? (sample == 0U && have_top
                       ? top_left_center
                       : (int32_t)(av2h_chroma_luma_sample_q3_u16(
                                       luma_plane, mode->cfl_downsample_filter_index, chroma_x,
                                       (uint16_t)(chroma_y - 1U)) >>
                                   3U))
                : (int32_t)(av2h_chroma_luma_left_clamped_q3_u16(
                                luma_plane, mode->cfl_downsample_filter_index,
                                chroma_x == 0U ? 0U : (uint16_t)(chroma_x - 1U), chroma_y) >>
                            3U);
        vector[1] = (center * center + domain->midpoint) >> domain->bit_depth;
        vector[2] = domain->midpoint;
        for (first = 0; first < 3; ++first) {
            target[first] +=
                vector[first] *
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride_samples + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
#else
    for (row = 0U; row < 3U; ++row) {
        uint16_t column;
        target[row] <<= 1U;
        for (column = row; column < 3U; ++column) {
            matrix[row][column] <<= 1U;
#endif
            }
        }
#if AV2H_KERNEL_WIDE
        ++count;
    }
    if (count == 0U)
        return AV2H_UNSUPPORTED;
    mhccp_solve_samples(matrix, target, count, domain, parameters);
#else
    mhccp_solve(matrix, target, parameters, 8U);
#endif
    for (row = 0U; row < prediction_height; ++row) {
#if !AV2H_KERNEL_WIDE
        uint16_t column;
#endif
        for (column = 0U; column < prediction_width; ++column) {
#if AV2H_KERNEL_WIDE
            int32_t center =
                (int32_t)(av2h_chroma_luma_sample_q3_u16(
                              luma_plane, mode->cfl_downsample_filter_index,
                              (uint16_t)(block_x + column), (uint16_t)(block_y + row)) >>
                          3U);
#endif
            int32_t vector[3U];
#if !AV2H_KERNEL_WIDE
            uint16_t center =
                (uint16_t)(mhccp_luma_q3(luma_plane, (uint16_t)(block_x + column), row) >> 3U);
#endif
            int32_t sum = 0;
            int parameter;
#if AV2H_KERNEL_WIDE
            vector[0] =
                mode->mhccp_direction == 0U ? center
                : mode->mhccp_direction == 1U
                    ? (row == 0U && !have_top ? center
                       : row == 0U && is_top_sb_boundary
                           ? (int32_t)(av2h_chroma_luma_sb_top_sample_q3_u16(
                                           luma_plane, mode->cfl_downsample_filter_index,
                                           (uint16_t)(block_x + column), block_y) >>
                                       3U)
                           : (int32_t)(av2h_chroma_luma_sample_q3_u16(
                                           luma_plane, mode->cfl_downsample_filter_index,
                                           (uint16_t)(block_x + column),
                                           (uint16_t)(block_y + row - 1U)) >>
                                       3U))
                    : (int32_t)(av2h_chroma_luma_sample_q3_u16(
                                    luma_plane, mode->cfl_downsample_filter_index,
                                    column == 0U && !have_left ? block_x
                                                               : (uint16_t)(block_x + column - 1U),
                                    (uint16_t)(block_y + row)) >>
                                3U);
            vector[1] = (center * center + domain->midpoint) >> domain->bit_depth;
            vector[2] = domain->midpoint;
#else
            vector[0] =
                (int32_t)(mhccp_luma_q3(luma_plane, (uint16_t)(block_x + column - 1U), row) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
            vector[2] = 128;
#endif
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(parameters[parameter], vector[parameter],
                                       AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip_to_domain(sum, domain);
        }
    }
    return AV2H_OK;
}