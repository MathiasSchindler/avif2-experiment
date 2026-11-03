Av2hStatus AV2H_KERNEL_NAME(av2h_cfl_derived_predict, av2h_cfl_derived_predict_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * chroma_plane,
    uint8_t filter_index,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t)
#if !AV2H_KERNEL_WIDE
        derive_alpha,
    int32_t supplied_alpha_q3,
    uint16_t
#endif
        superblock_size
#if !AV2H_KERNEL_WIDE
    ,
    uint8_t chroma_format_idc
#endif
    ,
    uint16_t block_x, uint16_t block_y, uint16_t width, uint16_t height, uint8_t have_top,
    uint8_t have_left, AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor,
    size_t predictor_stride_samples
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    int numb_up = 0;
    int numb_left = 0;
    int count = 0;
    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_xx = 0;
    int32_t sum_xy = 0;
    int32_t alpha;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
    const int bit_depth = av2h_sample_domain_8bit.bit_depth;
    uint8_t full_resolution = (uint8_t)(chroma_format_idc == 2U);
    unsigned int scale = full_resolution ? 1U : 2U;
    uint16_t luma_x = (uint16_t)((uint32_t)block_x * scale);
    uint16_t luma_y = (uint16_t)((uint32_t)block_y * scale);
    uint16_t luma_width = (uint16_t)((uint32_t)width * scale);
    uint16_t luma_height = (uint16_t)((uint32_t)height * scale);
    int32_t luma_left_ds[64];
    int32_t luma_above_ds[64];
    int32_t chroma_left[64];
    int32_t chroma_above[64];
#endif
    uint8_t is_top_sb_boundary;
#if AV2H_KERNEL_WIDE
    if (luma_plane == NULL || chroma_plane == NULL || predictor == NULL ||
        luma_plane->data == NULL || chroma_plane->data == NULL ||
        luma_plane->reconstructed == NULL || filter_index > 2U || width == 0U || height == 0U ||
        width > 64U || height > 64U || predictor_stride_samples < width ||
        (have_top && block_y == 0U) || (have_left && block_x == 0U)) {
#else
    int top_offset;
    int bottom_offset;
    uint16_t row;
    uint16_t column;
    int32_t avg_l;
    int32_t alpha;
    int numb_up;
    int numb_left;
    int count;
    int32_t sum_x;
    int32_t sum_y;
    int32_t sum_xx;
    int32_t sum_xy;
    int shift;
    uint16_t visible_luma_width;
    uint16_t visible_luma_height;
    uint16_t effective_luma_width;
    uint16_t effective_luma_height;
    uint16_t effective_chroma_width;
    uint16_t effective_chroma_height;
    if (luma_plane == NULL || luma_plane->data == NULL ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) || filter_index > 2U || width > 64U ||
        height > 64U || width == 0U || height == 0U) {
#endif
        return AV2H_UNSUPPORTED;
    }
#if !AV2H_KERNEL_WIDE
    if (luma_x >= luma_plane->width || luma_y >= luma_plane->height ||
        luma_plane->stride_samples < luma_plane->width || luma_plane->reconstructed == NULL) {
        return AV2H_UNSUPPORTED;
    }
    effective_luma_width = (uint16_t)((luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_luma_height = (uint16_t)((luma_plane->reference_height + 7U) & ~(size_t)7U);
    effective_chroma_width =
        (uint16_t)((chroma_plane->reference_width + (full_resolution ? 7U : 3U)) &
                   ~(size_t)(full_resolution ? 7U : 3U));
    effective_chroma_height =
        (uint16_t)((chroma_plane->reference_height + (full_resolution ? 7U : 3U)) &
                   ~(size_t)(full_resolution ? 7U : 3U));
    if (effective_luma_width > luma_plane->width) {
        effective_luma_width = luma_plane->width;
    }
    if (effective_luma_height > luma_plane->height) {
        effective_luma_height = luma_plane->height;
    }
    if (effective_chroma_width > chroma_plane->width) {
        effective_chroma_width = chroma_plane->width;
    }
    if (effective_chroma_height > chroma_plane->height) {
        effective_chroma_height = chroma_plane->height;
    }
    visible_luma_width = luma_x >= effective_luma_width ? 0U
                         : luma_width < effective_luma_width - luma_x
                             ? luma_width
                             : (uint16_t)(effective_luma_width - luma_x);
    visible_luma_height = luma_y >= effective_luma_height ? 0U
                          : luma_height < effective_luma_height - luma_y
                              ? luma_height
                              : (uint16_t)(effective_luma_height - luma_y);
    if (!av2h_chroma_plane_region_reconstructed(luma_plane, luma_x, luma_y, visible_luma_width,
                                                visible_luma_height)) {
        return AV2H_UNSUPPORTED;
    }
    if (have_left) {
        uint16_t reference_x = full_resolution      ? (uint16_t)(luma_x - 1U)
                               : filter_index == 0U ? (uint16_t)(luma_x - 2U)
                                                    : (uint16_t)(luma_x - 3U);
        uint16_t reference_width = full_resolution ? 1U : (filter_index == 0U ? 2U : 3U);
        if (luma_x < reference_width ||
            !av2h_chroma_plane_region_reconstructed(luma_plane, reference_x, luma_y,
                                                    reference_width, visible_luma_height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            if (full_resolution) {
                luma_left_ds[row] = (int32_t)av2h_chroma_plane_coded_sample(luma_plane, luma_x - 1U,
                                                                            (size_t)luma_y + row)
                                    << 3U;
            } else if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                luma_left_ds[row] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, center_x - 1U, center_y) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, center_x, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x - 1U,
                                                             center_y + 1U) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, center_x,
                                                                  center_y + 1U) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U,
                                                             center_y + 1U));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                size_t top_y = row == 0U ? center_y : center_y - 1U;
                luma_left_ds[row] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, center_x - 1U, center_y) +
                              4U * av2h_chroma_plane_coded_sample(luma_plane, center_x, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x, top_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x, center_y + 1U));
            } else {
                size_t r0 = (size_t)luma_y + (size_t)row * 2U;
                const uint8_t *base =
                    &luma_plane->data[r0 * luma_plane->stride_samples + luma_x - 2U];
                luma_left_ds[row] = (int32_t)(base[0] + base[1] + base[luma_plane->stride_samples] +
                                              base[luma_plane->stride_samples + 1U])
                                    << 1;
            }
            chroma_left[row] =
                av2h_chroma_plane_coded_sample(chroma_plane, block_x - 1U, (size_t)block_y + row);
            if (luma_y >= effective_luma_height) {
                luma_left_ds[row] = (int32_t)av2h_sample_domain_8bit.midpoint << 3;
            } else if (row >= visible_luma_height / scale && row != 0U) {
                luma_left_ds[row] = luma_left_ds[row - 1U];
            }
            if (block_y >= effective_chroma_height) {
                chroma_left[row] = domain->midpoint;
            } else if (row >= effective_chroma_height - block_y && row != 0U) {
                chroma_left[row] = chroma_left[row - 1U];
            }
        }
    }
    if (have_top) {
        is_top_sb_boundary = (uint8_t)(superblock_size != 0U && (luma_y % superblock_size) == 0U);
        top_offset = full_resolution ? 1 : 2 - is_top_sb_boundary;
        bottom_offset = full_resolution ? 0 : 1 - is_top_sb_boundary;
        if (luma_y < (uint16_t)top_offset ||
            !av2h_chroma_plane_region_reconstructed(
                luma_plane, luma_x, (uint16_t)(luma_y - top_offset),
                luma_width < luma_plane->width - luma_x ? luma_width
                                                        : (uint16_t)(luma_plane->width - luma_x),
                full_resolution ? 1U : 4U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            if (full_resolution) {
                luma_above_ds[column] = (int32_t)av2h_chroma_plane_coded_sample(
                                            luma_plane, (size_t)luma_x + column, luma_y - 1U)
                                        << 3U;
            } else if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, left_x, top_row) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, center_x, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, left_x, bottom_row) +
                              2U *
                                  av2h_chroma_plane_coded_sample(luma_plane, center_x, bottom_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U,
                                                             bottom_row));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t center_y = (size_t)luma_y - (size_t)top_offset;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_y = is_top_sb_boundary ? center_y : center_y - 1U;
                size_t bottom_y = center_y + (size_t)bottom_offset;
                luma_above_ds[column] =
                    (int32_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                              4U * luma_plane
                                       ->data[center_y * luma_plane->stride_samples + center_x] +
                              luma_plane
                                  ->data[center_y * luma_plane->stride_samples + center_x + 1U] +
                              luma_plane->data[top_y * luma_plane->stride_samples + center_x] +
                              luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
            } else {
                size_t c0 = (size_t)luma_x + (size_t)column * 2U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, c0, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, c0 + 1U, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, c0, bottom_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, c0 + 1U, bottom_row))
                    << 1;
            }
            chroma_above[column] = av2h_chroma_plane_coded_sample(
                chroma_plane, (size_t)block_x + column, (size_t)block_y - 1U);
            if (luma_x >= effective_luma_width) {
                luma_above_ds[column] = (int32_t)av2h_sample_domain_8bit.midpoint << 3;
            } else if (column >= visible_luma_width / scale && column != 0U) {
                luma_above_ds[column] = luma_above_ds[column - 1U];
            }
            if (block_x >= effective_chroma_width) {
                chroma_above[column] = domain->midpoint;
            } else if (column >= effective_chroma_width - block_x && column != 0U) {
                chroma_above[column] = chroma_above[column - 1U];
            }
        }
    }
    {
        int ss_hor = width > 32U ? 2 : 1;
        int ss_ver = height > 32U ? 2 : 1;
        int32_t sum = 0;
        int dc_count = 0;
        if (have_top) {
            for (column = 0U; column < width; column += (uint16_t)ss_hor) {
                sum += luma_above_ds[column];
                ++dc_count;
            }
        }
        if (have_left) {
            for (row = 0U; row < height; row += (uint16_t)ss_ver) {
                sum += luma_left_ds[row];
                ++dc_count;
            }
        }
        if (dc_count > 0) {
            int div_shift = 0;
            uint16_t scale = av2h_sample_resolve_divisor_32((uint32_t)dc_count, &div_shift);
            int32_t rounding = (1 << div_shift) >> 1;
            int32_t val = (int32_t)(((int64_t)sum * scale + rounding) >> div_shift);
            int32_t max_v = (1 << (bit_depth + 3)) - 1;
            avg_l = val > max_v ? max_v : val;
        } else {
            avg_l = (int32_t)av2h_sample_domain_8bit.midpoint << 3;
        }
    }
    numb_up = 0;
    numb_left = 0;
#endif
    if (have_top && have_left) {
        if (width > (uint16_t)(height * 2U)) {
            numb_up = AV2H_CFL_NUM_REF_SAM;
        } else if (height > (uint16_t)(width * 2U)) {
            numb_left = AV2H_CFL_NUM_REF_SAM;
        } else {
            numb_up = AV2H_CFL_NUM_REF_SAM >> 1;
            numb_left = AV2H_CFL_NUM_REF_SAM >> 1;
        }
    } else {
        numb_up = have_top ? AV2H_CFL_NUM_REF_SAM : 0;
        numb_left = have_left ? AV2H_CFL_NUM_REF_SAM : 0;
    }
    if (numb_up > width)
        numb_up = width;
    if (numb_left > height)
        numb_left = height;
#if AV2H_KERNEL_WIDE
    is_top_sb_boundary =
        (uint8_t)(superblock_size != 0U && ((uint32_t)block_y * 2U) % superblock_size == 0U);
    if (numb_up != 0) {
#else
    count = 0;
    sum_x = 0;
    sum_y = 0;
    sum_xx = 0;
    sum_xy = 0;
    if (numb_up > 0) {
#endif
        int step = (int)width / numb_up;
#if AV2H_KERNEL_WIDE
        int sample = step == 1 ? 0 : step >> 1;
#else
        int start;
        int i;
#endif
        if (step < 1)
            step = 1;
#if AV2H_KERNEL_WIDE
        for (; sample < (int)width; sample += step) {
            uint16_t chroma_x = (uint16_t)(block_x + sample);
            int32_t luma;
            int32_t chroma;
            if (chroma_x >= chroma_plane->width) {
                chroma_x = (uint16_t)(chroma_plane->width - 1U);
            }
            if (is_top_sb_boundary) {
                luma = (int32_t)((sample == 0 ? av2h_chroma_luma_sb_top_left_clamped_q3_u16(
                                                    luma_plane, filter_index, chroma_x, block_y)
                                              : av2h_chroma_luma_sb_top_sample_q3_u16(
                                                    luma_plane, filter_index, chroma_x, block_y)) >>
                                 3U);
            } else {
                luma = (int32_t)((sample == 0 ? av2h_chroma_luma_left_clamped_q3_u16(
                                                    luma_plane, filter_index, chroma_x,
                                                    (uint16_t)(block_y - 1U))
                                              : av2h_chroma_luma_sample_q3_u16(
                                                    luma_plane, filter_index, chroma_x,
                                                    (uint16_t)(block_y - 1U))) >>
                                 3U);
            }
            chroma = chroma_plane
                         ->data[((size_t)block_y - 1U) * chroma_plane->stride_samples + chroma_x];
            sum_x += luma;
            sum_y += chroma;
            sum_xx += luma * luma;
            sum_xy += luma * chroma;
#else
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)width; i += step) {
            int32_t lx = luma_above_ds[i] >> 3;
            int32_t cy = chroma_above[i];
            sum_x += lx;
            sum_y += cy;
            sum_xy += lx * cy;
            sum_xx += lx * lx;
#endif
            ++count;
        }
    }
#if AV2H_KERNEL_WIDE
    if (numb_left != 0) {
#else
    if (numb_left > 0) {
#endif
        int step = (int)height / numb_left;
#if AV2H_KERNEL_WIDE
        int sample = step == 1 ? 0 : step >> 1;
#else
        int start;
        int i;
#endif
        if (step < 1)
            step = 1;
#if AV2H_KERNEL_WIDE
        for (; sample < (int)height; sample += step) {
            uint16_t chroma_y = (uint16_t)(block_y + sample);
            int32_t luma =
                (int32_t)(av2h_chroma_luma_sample_q3_u16(luma_plane, filter_index,
                                                         (uint16_t)(block_x - 1U), chroma_y) >>
                          3U);
            int32_t chroma =
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride_samples + block_x - 1U];
            sum_x += luma;
            sum_y += chroma;
            sum_xx += luma * luma;
            sum_xy += luma * chroma;
#else
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)height; i += step) {
            int32_t lx = luma_left_ds[i] >> 3;
            int32_t cy = chroma_left[i];
            sum_x += lx;
            sum_y += cy;
            sum_xy += lx * cy;
            sum_xx += lx * lx;
#endif
            ++count;
        }
    }
#if AV2H_KERNEL_WIDE
    alpha = av2h_sample_regression_slope(sum_x, sum_y, sum_xx, sum_xy, count,
                                         3 + AV2H_CFL_ADD_BITS_ALPHA);
    return cfl_predict_u16_scaled(luma_plane, filter_index, alpha, AV2H_CFL_ADD_BITS_ALPHA,
                                  superblock_size, block_x, block_y, width, height, have_top,
                                  have_left, predictor, predictor_stride_samples, domain);
#else
    shift = 3 + AV2H_CFL_ADD_BITS_ALPHA;
    alpha = derive_alpha ? av2h_sample_regression_slope(sum_x, sum_y, sum_xx, sum_xy, count, shift)
                         : supplied_alpha_q3 * (1 << AV2H_CFL_ADD_BITS_ALPHA);
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int32_t recon_q3;
            if (full_resolution) {
                recon_q3 = (int32_t)av2h_chroma_plane_coded_sample(
                               luma_plane, (size_t)luma_x + column, (size_t)luma_y + row)
                           << 3U;
            } else if (filter_index == 1U) {
                size_t sample_x = (size_t)luma_x + (size_t)column * 2U;
                size_t sample_y = (size_t)luma_y + (size_t)row * 2U;
                size_t left_x = (column & 31U) == 0U ? sample_x : sample_x - 1U;
                recon_q3 =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, left_x, sample_y) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, sample_x, sample_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, sample_x + 1U, sample_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, left_x, sample_y + 1U) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, sample_x,
                                                                  sample_y + 1U) +
                              av2h_chroma_plane_coded_sample(luma_plane, sample_x + 1U,
                                                             sample_y + 1U));
            } else if (filter_index == 2U) {
                recon_q3 = cfl_luma_filter2_current_q3(luma_plane, (uint16_t)(block_x + column),
                                                       (uint16_t)(block_y + row), block_x, block_y);
            } else {
                size_t sample_x = (size_t)luma_x + (size_t)column * 2U;
                size_t sample_y = (size_t)luma_y + (size_t)row * 2U;
                recon_q3 =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, sample_x, sample_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, sample_x + 1U, sample_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, sample_x, sample_y + 1U) +
                              av2h_chroma_plane_coded_sample(luma_plane, sample_x + 1U,
                                                             sample_y + 1U))
                    << 1;
#endif
}
#if !AV2H_KERNEL_WIDE
int32_t ac = recon_q3 - avg_l;
int32_t scaled = av2h_sample_round_pow2_signed((int64_t)alpha * ac, 6 + AV2H_CFL_ADD_BITS_ALPHA);
int32_t base_dc = predictor[(size_t)row * predictor_stride_samples + column];
predictor[(size_t)row * predictor_stride_samples + column] =
    av2h_sample_clip_to_domain(scaled + base_dc, domain);
}
}
return AV2H_OK;
}
#endif
