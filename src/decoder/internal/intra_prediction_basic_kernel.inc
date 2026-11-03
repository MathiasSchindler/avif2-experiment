static void AV2H_KERNEL_NAME(predict_smooth_samples, predict_smooth_samples_u16)(
    uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(Av2hLumaReferences, Av2hLumaReferencesU16) * references,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples,
    Av2hIntraMode mode, uint16_t bottom_left, uint16_t top_right, const Av2hSampleDomain *domain) {
    uint16_t row;
    uint8_t scale = smooth_scale(width, height);
    int have_above = references->above != NULL && references->above_count != 0U;
    int have_left = references->left != NULL && references->left_count != 0U;
    for (row = 0U; row < height; ++row) {
        uint16_t column;
        for (column = 0U; column < width; ++column) {
            uint16_t top = have_above  ? references->above[column]
                           : have_left ? references->left[0]
                                       : domain->midpoint - 1U;
            uint16_t side = have_left    ? references->left[row]
                            : have_above ? references->above[0]
                                         : domain->midpoint + 1U;
            int32_t vertical = bottom_left + av2h_sample_round_pow2(((int64_t)top - bottom_left) *
                                                                        (height - 1U - row),
                                                                    av2h_sample_block_log2(height));
            int32_t horizontal = top_right + av2h_sample_round_pow2(((int64_t)side - top_right) *
                                                                        (width - 1U - column),
                                                                    av2h_sample_block_log2(width));
            vertical +=
                av2h_sample_round_pow2(((int64_t)top - vertical) * smooth_blend(row, scale), 7U);
            horizontal += av2h_sample_round_pow2(
                ((int64_t)side - horizontal) * smooth_blend(column, scale), 7U);
            predictor[(size_t)row * predictor_stride_samples + column] = (AV2H_KERNEL_TYPE(
                uint8_t, uint16_t))(mode == AV2H_INTRA_SMOOTH_VERTICAL ? vertical
                                    : mode == AV2H_INTRA_SMOOTH_HORIZONTAL
                                        ? horizontal
                                        : av2h_sample_round_pow2(vertical + horizontal, 1U));
        }
    }
}

static void AV2H_KERNEL_NAME(predict_paeth_samples, predict_paeth_samples_u16)(
    uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(Av2hLumaReferences, Av2hLumaReferencesU16) * references,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples) {
    uint16_t row;
    for (row = 0U; row < height; ++row) {
        uint16_t column;
        for (column = 0U; column < width; ++column) {
            int32_t top = references->above[column];
            int32_t side = references->left[row];
            int32_t corner = references->above_left;
            int32_t base = top + side - corner;
            int32_t side_distance = base > side ? base - side : side - base;
            int32_t top_distance = base > top ? base - top : top - base;
            int32_t corner_distance = base > corner ? base - corner : corner - base;
            predictor[(size_t)row * predictor_stride_samples + column] =
                (AV2H_KERNEL_TYPE(uint8_t, uint16_t))(
                    side_distance <= top_distance && side_distance <= corner_distance ? side
                    : top_distance <= corner_distance                                 ? top
                                                                                      : corner);
        }
    }
}
