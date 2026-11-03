static AV2H_KERNEL_TYPE(uint8_t, uint16_t)
    AV2H_KERNEL_NAME(directional_reference, directional_reference_u16)(
        const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * reference, size_t reference_count,
        AV2H_KERNEL_TYPE(uint8_t, uint16_t) above_left, int index) {
    if (index < 0)
        return above_left;
    if (reference == NULL || reference_count == 0U)
        return above_left;
    if ((size_t)index >= reference_count) {
        return reference[reference_count - 1U];
    }
    return reference[index];
}
static void AV2H_KERNEL_NAME(directional_project_zone1_u8, directional_project_zone1_u16)(
    const Av2hLeafMode *mode, int angle, uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * above_edge,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * left_edge,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples,
    uint8_t use_idif, const Av2hSampleDomain *domain) {
    uint16_t row;
    uint16_t column;
    int dx = directional_derivative[angle];
    int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
    int x = dx * (1 + mode->mrl_index);
    for (row = 0U; row < height; ++row, x += dx) {
        int base = x >> 6;
        int shift = (x & 0x3f) >> 1;
        for (column = 0U; column < width; ++column, ++base) {
            int32_t sum = 0;
#if AV2H_KERNEL_WIDE
            uint16_t primary;
#endif
            size_t tap;
#if AV2H_KERNEL_WIDE
            if (base > max_base || (mode->edge_ibp != 0U && base == max_base)) {
#else
            uint8_t primary;
            if (base > max_base || (!use_idif && base == max_base)) {
#endif
                primary = above_edge[max_base];
#if AV2H_KERNEL_WIDE
            } else if (mode->edge_ibp != 0U || use_idif == 0U) {
                primary = av2h_sample_clip_to_domain(
                    (above_edge[base] * (32 - shift) + above_edge[base + 1] * shift + 16) >> 5U,
                    domain);
#else
            } else if (!use_idif) {
                primary = (uint8_t)((above_edge[base] * (32 - shift) +
                                     above_edge[base + 1] * shift + 16) >>
                                    5U);
#endif
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] * above_edge[base + (int)tap - 1];
                }
                primary = av2h_sample_clip_to_domain((sum + 64) >> 7U, domain);
            }
            if (mode->mrl_index == 0U && mode->enable_ibp && mode->angle_delta % 2 == 0) {
                int secondary_dy = directional_derivative[90 - angle];
                int weight_dy = directional_ibp_canonical_derivative(angle);
                int secondary_y = secondary_dy * ((int)column + 1);
                int secondary_base = (secondary_y >> 6) + row;
                int secondary_shift = (secondary_y & 0x3f) >> 1;
                AV2H_KERNEL_TYPE(uint8_t, uint16_t) secondary;
                uint8_t weight;
                if (secondary_base > max_base) {
                    secondary = left_edge[max_base];
#if AV2H_KERNEL_WIDE
                } else if (use_idif == 0U) {
                    secondary = av2h_sample_clip_to_domain(
                        (left_edge[secondary_base] * (32 - secondary_shift) +
                         left_edge[secondary_base + 1] * secondary_shift + 16) >>
                            5U,
                        domain);
#endif
                } else {
                    sum = 0;
                    for (tap = 0U; tap < 4U; ++tap) {
                        sum += dr_interp_filter[secondary_shift][tap] *
                               left_edge[secondary_base + (int)tap - 1];
                    }
                    secondary = av2h_sample_clip_to_domain((sum + 64) >> 7U, domain);
                }
                weight = directional_ibp_weight(row >> (height >> 5U), column >> (width >> 5U),
                                                weight_dy);
                primary = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))(
                    (primary * weight + secondary * (128U - weight) + 64U) >> 7U);
            }
            predictor[(size_t)row * predictor_stride_samples + column] = primary;
        }
    }
}
static void AV2H_KERNEL_NAME(directional_project_zone2_u8, directional_project_zone2_u16)(
    const Av2hLeafMode *mode, int angle, uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * above_edge,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * left_edge, size_t edge_count,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples,
    uint8_t use_idif, const Av2hSampleDomain *domain) {
    uint16_t row;
    uint16_t column;
    const int dx = directional_derivative[180 - angle];
    const int dy = directional_derivative[angle - 90];
    const int min_base = -1 - mode->mrl_index;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
#if AV2H_KERNEL_WIDE
            int position = ((int)column << 6) - ((int)row + 1 + mode->mrl_index) * dx;
            int base = position >> 6;
            int shift = (position & 0x3f) >> 1;
            const uint16_t *reference = above_edge;
#else
            int x = ((int)column << 6) - ((int)row + 1 + mode->mrl_index) * dx;
            int base = x >> 6;
            int shift = (x & 0x3f) >> 1;
            const uint8_t *reference;
#endif
            int32_t sum = 0;
            size_t tap;
#if AV2H_KERNEL_WIDE
            if (base < min_base) {
                position = ((int)row << 6) - ((int)column + 1 + mode->mrl_index) * dy;
                base = position >> 6;
                shift = (position & 0x3f) >> 1;
#else
            if (base >= min_base) {
                reference = above_edge;
            } else {
                int y = ((int)row << 6) - ((int)column + 1 + mode->mrl_index) * dy;
                base = y >> 6;
                shift = (y & 0x3f) >> 1;
#endif
                reference = left_edge;
            }
#if AV2H_KERNEL_WIDE
            if (mode->edge_ibp != 0U || use_idif == 0U) {
#else
            if (!use_idif) {
#endif
                sum = reference[base] * (32 - shift) + reference[base + 1] * shift;
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
                    int reference_index = base + (int)tap - 1;
                    AV2H_KERNEL_TYPE(uint8_t, uint16_t)
                    sample =
                        mode->mrl_index != 0U
                            ? reference[reference_index]
                            : AV2H_KERNEL_NAME(directional_reference, directional_reference_u16)(
                                  reference, edge_count, reference[-1], reference_index);
                    sum += dr_interp_filter[shift][tap] * sample;
                }
            }
#if AV2H_KERNEL_WIDE
            predictor[(size_t)row * predictor_stride_samples + column] = av2h_sample_clip_to_domain(
                mode->edge_ibp != 0U || use_idif == 0U ? (sum + 16) >> 5U : (sum + 64) >> 7U,
                domain);
#else
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip_to_domain(use_idif ? (sum + 64) >> 7U : (sum + 16) >> 5U, domain);
#endif
        }
    }
}
static void AV2H_KERNEL_NAME(directional_project_zone3_u8, directional_project_zone3_u16)(
    const Av2hLeafMode *mode, int angle, uint16_t width, uint16_t height,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * above_edge,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * left_edge, size_t edge_count,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples,
    uint8_t use_idif, const Av2hSampleDomain *domain) {
    uint16_t row;
    uint16_t column;
    int dy = directional_derivative[270 - angle];
    int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
    int y = dy * (1 + mode->mrl_index);
    for (column = 0U; column < width; ++column, y += dy) {
        int base = y >> 6;
        int shift = (y & 0x3f) >> 1;
        for (row = 0U; row < height; ++row, ++base) {
            int32_t sum = 0;
#if AV2H_KERNEL_WIDE
            uint16_t primary;
#endif
            size_t tap;
#if AV2H_KERNEL_WIDE
            if (base > max_base || (mode->edge_ibp != 0U && base == max_base)) {
                primary = left_edge[max_base];
            } else if (mode->edge_ibp != 0U || use_idif == 0U) {
                primary = av2h_sample_clip_to_domain(
                    (left_edge[base] * (32 - shift) + left_edge[base + 1] * shift + 16) >> 5U,
                    domain);
#else
            if (base > max_base || (!use_idif && base == max_base)) {
                sum = (int32_t)left_edge[max_base] << (use_idif ? 7U : 5U);
            } else if (!use_idif) {
                sum = left_edge[base] * (32 - shift) + left_edge[base + 1] * shift;
#endif
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
#if AV2H_KERNEL_WIDE
                    int reference_index = base + (int)tap - 1;
                    uint16_t sample = mode->mrl_index != 0U ? left_edge[reference_index]
                                      : reference_index < 0 ? left_edge[-1]
                                      : (size_t)reference_index >= edge_count
                                          ? left_edge[edge_count - 1U]
                                          : left_edge[reference_index];
                    sum += dr_interp_filter[shift][tap] * sample;
#else
                    sum += dr_interp_filter[shift][tap] *
                           directional_reference(left_edge, edge_count, left_edge[-1],
                                                 base + (int)tap - 1);
#endif
                }
#if AV2H_KERNEL_WIDE
                primary = av2h_sample_clip_to_domain((sum + 64) >> 7U, domain);
#endif
            }
#if AV2H_KERNEL_WIDE
            if (mode->mrl_index == 0U && mode->enable_ibp != 0U && mode->angle_delta % 2 == 0) {
#else
            {
                uint8_t primary = av2h_sample_clip_to_domain(
                    use_idif ? (sum + 64) >> 7U : (sum + 16) >> 5U, domain);
                if (mode->mrl_index == 0U && mode->enable_ibp && mode->angle_delta % 2 == 0) {
#endif
                int secondary_dx = directional_derivative[angle - 180];
                int weight_dy = directional_ibp_canonical_derivative(270 - angle);
                int secondary_x = secondary_dx * ((int)row + 1);
                int secondary_base = (secondary_x >> 6) + (int)column;
                int secondary_shift = (secondary_x & 0x3f) >> 1;
                int32_t secondary_sum = 0;
                AV2H_KERNEL_TYPE(uint8_t, uint16_t) secondary;
                uint8_t weight;
                if (secondary_base > max_base) {
                    secondary = above_edge[max_base];
#if AV2H_KERNEL_WIDE
                } else if (use_idif == 0U) {
                    secondary = av2h_sample_clip_to_domain(
                        (above_edge[secondary_base] * (32 - secondary_shift) +
                         above_edge[secondary_base + 1] * secondary_shift + 16) >>
                            5U,
                        domain);
#endif
                } else {
                    for (tap = 0U; tap < 4U; ++tap) {
                        secondary_sum +=
                            dr_interp_filter[secondary_shift][tap] *
                            AV2H_KERNEL_NAME(directional_reference, directional_reference_u16)(
                                above_edge, edge_count, above_edge[-1],
                                secondary_base + (int)tap - 1);
                    }
                    secondary = av2h_sample_clip_to_domain((secondary_sum + 64) >> 7U, domain);
                }
                weight = directional_ibp_weight(column >> (width >> 5U), row >> (height >> 5U),
                                                weight_dy);
                primary = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))(
                    (primary * weight + secondary * (128U - weight) + 64U) >> 7U);
            }
            predictor[(size_t)row * predictor_stride_samples + column] = primary;
        }
    }
}
#if !AV2H_KERNEL_WIDE
}
#endif
