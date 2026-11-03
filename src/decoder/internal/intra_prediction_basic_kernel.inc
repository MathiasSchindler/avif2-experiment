Av2hStatus AV2H_KERNEL_NAME(av2h_predict_smooth, av2h_predict_smooth_u16)(
#if AV2H_KERNEL_WIDE
    const Av2hLeafMode *mode,
#endif
    uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(Av2hLumaReferences, Av2hLumaReferencesU16) * references,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    uint16_t row;
    uint16_t column;
#if AV2H_KERNEL_WIDE
    int have_above;
    int have_left;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    have_above = above != NULL && above_count != 0U;
    have_left = left != NULL && left_count != 0U;
#endif
    uint8_t scale = smooth_scale(width, height);
#if AV2H_KERNEL_WIDE
    uint16_t bottom_left = !have_left            ? have_above ? above[0] : (domain->midpoint + 1U)
                           : left_count > height ? left[height]
                                                 : left[left_count - 1U];
    uint16_t top_right = !have_above           ? have_left ? left[0] : (domain->midpoint - 1U)
                         : above_count > width ? above[width]
                                               : above[above_count - 1U];
    if ((have_above && above_count < width) || (have_left && left_count < height)) {
#else
    uint8_t bottom_left;
    uint8_t top_right;
    int have_above = references->above != NULL && references->above_count != 0U;
    int have_left = references->left != NULL && references->left_count != 0U;
    if ((have_above && references->above_count < width) ||
        (have_left && references->left_count < height)) {
#endif
        return AV2H_INVALID_ARGUMENT;
    }
#if !AV2H_KERNEL_WIDE
    bottom_left = !have_left ? have_above ? references->above[0] : (domain->midpoint + 1U)
                  : references->left_count > height ? references->left[height]
                                                    : references->left[height - 1U];
    top_right = !have_above ? have_left ? references->left[0] : (domain->midpoint - 1U)
                : references->above_count > width ? references->above[width]
                                                  : references->above[width - 1U];
#endif
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
#if AV2H_KERNEL_WIDE
            uint16_t top = have_above  ? above[column]
                           : have_left ? left[0]
                                       : (domain->midpoint - 1U);
            uint16_t side = have_left ? left[row] : have_above ? above[0] : (domain->midpoint + 1U);
#else
            uint8_t top = !have_above ? have_left ? references->left[0] : (domain->midpoint - 1U)
                                      : references->above[column];
            uint8_t left = !have_left ? have_above ? references->above[0] : (domain->midpoint + 1U)
                                      : references->left[row];
            uint8_t top_blend = smooth_blend(row, scale);
            uint8_t left_blend = smooth_blend(column, scale);
#endif
            int32_t vertical = bottom_left + av2h_sample_round_pow2(((int64_t)top - bottom_left) *
                                                                        (height - 1U - row),
                                                                    av2h_sample_block_log2(height));
#if AV2H_KERNEL_WIDE
            int32_t horizontal = top_right + av2h_sample_round_pow2(((int64_t)side - top_right) *
                                                                        (width - 1U - column),
                                                                    av2h_sample_block_log2(width));
            vertical +=
                av2h_sample_round_pow2(((int64_t)top - vertical) * smooth_blend(row, scale), 7U);
            horizontal += av2h_sample_round_pow2(
                ((int64_t)side - horizontal) * smooth_blend(column, scale), 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ? (uint16_t)vertical
                : mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL
                    ? (uint16_t)horizontal
                    : (uint16_t)av2h_sample_round_pow2(vertical + horizontal, 1U);
#else
            int32_t horizontal = top_right + av2h_sample_round_pow2(((int64_t)left - top_right) *
                                                                        (width - 1U - column),
                                                                    av2h_sample_block_log2(width));
            vertical += av2h_sample_round_pow2(((int64_t)top - vertical) * top_blend, 7U);
            horizontal += av2h_sample_round_pow2(((int64_t)left - horizontal) * left_blend, 7U);
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)av2h_sample_round_pow2(vertical + horizontal, 1U);
#endif
        }
    }
    return AV2H_OK;
}
Av2hStatus AV2H_KERNEL_NAME(av2h_predict_paeth, av2h_predict_paeth_u16)(
    uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(Av2hLumaReferences, Av2hLumaReferencesU16) * references,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t above_left;
    uint8_t has_above_left;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    uint16_t row;
    uint16_t column;
#if AV2H_KERNEL_WIDE
    int have_above;
    int have_left;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    above_left = references->above_left;
    has_above_left = references->has_above_left;
    have_above = above != NULL && above_count != 0U;
    have_left = left != NULL && left_count != 0U;
    if (!have_above && !have_left) {
#else
    if (references->above != NULL && references->above_count >= width &&
        (references->left == NULL || references->left_count == 0U)) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    references->above[column];
            }
        }
        return AV2H_OK;
    }
    if (references->left != NULL && references->left_count >= height &&
        (references->above == NULL || references->above_count == 0U)) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] = references->left[row];
            }
        }
        return AV2H_OK;
    }
    if ((references->above == NULL || references->above_count == 0U) &&
        (references->left == NULL || references->left_count == 0U)) {
#endif
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] = domain->midpoint;
            }
        }
        return AV2H_OK;
    }
#if AV2H_KERNEL_WIDE
    if (!have_left || !have_above) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    have_above ? above[column < above_count ? column : above_count - 1U]
                               : left[row < left_count ? row : left_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (above_count < width || left_count < height || has_above_left == 0U) {
#else
    if (references->above == NULL || references->above_count < width || references->left == NULL ||
        references->left_count < height || !references->has_above_left) {
#endif
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
#if AV2H_KERNEL_WIDE
            int32_t top = above[column];
            int32_t side = left[row];
            int32_t corner = above_left;
#else
            int32_t top = references->above[column];
            int32_t side = references->left[row];
            int32_t corner = references->above_left;
#endif
            int32_t base = top + side - corner;
            int32_t side_distance = base > side ? base - side : side - base;
            int32_t top_distance = base > top ? base - top : top - base;
            int32_t corner_distance = base > corner ? base - corner : corner - base;
#if AV2H_KERNEL_WIDE
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint16_t)(side_distance <= top_distance && side_distance <= corner_distance ? side
                           : top_distance <= corner_distance ? top
                                                             : corner);
#else
            int32_t value = side_distance <= top_distance && side_distance <= corner_distance ? side
                            : top_distance <= corner_distance ? top
                                                              : corner;
            predictor[(size_t)row * predictor_stride_samples + column] = (uint8_t)value;
#endif
        }
    }
    return AV2H_OK;
}