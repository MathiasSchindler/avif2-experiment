static AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    directional_reference,
    directional_reference_u16)(const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *reference, size_t reference_count,
                               AV2H_KERNEL_TYPE(uint8_t, uint16_t) above_left, int index) {
    if (index < 0 || reference == NULL || reference_count == 0U)
        return above_left;
    if ((size_t)index >= reference_count)
        return reference[reference_count - 1U];
    return reference[index];
}

static void AV2H_KERNEL_NAME(
    directional_project_zone1_u8,
    directional_project_zone1_u16)(const Av2hLeafMode *mode, int angle, uint16_t width,
                                         uint16_t height, const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *above_edge,
                                         const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *left_edge,
                                   AV2H_KERNEL_TYPE(uint8_t, uint16_t) *predictor,
                                         size_t predictor_stride_samples,
                                         Av2hDirectionalProjectionPolicy policy,
                                         const Av2hSampleDomain *domain) {
    uint16_t row;
    int dx = directional_derivative[angle];
    int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
    int x = dx * (1 + mode->mrl_index);
    for (row = 0U; row < height; ++row, x += dx) {
        uint16_t column;
        int base = x >> 6;
        int shift = (x & 0x3f) >> 1;
        for (column = 0U; column < width; ++column, ++base) {
            int32_t sum = 0;
            AV2H_KERNEL_TYPE(uint8_t, uint16_t) primary;
            size_t tap;
            if (base > max_base || (policy.clamp_last && base == max_base)) {
                primary = above_edge[max_base];
            } else if (policy.primary_linear) {
                primary = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                    (above_edge[base] * (32 - shift) + above_edge[base + 1] * shift + 16) >> 5U,
                    domain);
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] * above_edge[base + (int)tap - 1];
                }
                primary = (AV2H_KERNEL_TYPE(
                    uint8_t, uint16_t))av2h_sample_clip_to_domain((sum + 64) >> 7U, domain);
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
                } else if (policy.secondary_linear) {
                    secondary = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                        (left_edge[secondary_base] * (32 - secondary_shift) +
                         left_edge[secondary_base + 1] * secondary_shift + 16) >>
                            5U,
                        domain);
                } else {
                    sum = 0;
                    for (tap = 0U; tap < 4U; ++tap) {
                        sum += dr_interp_filter[secondary_shift][tap] *
                               left_edge[secondary_base + (int)tap - 1];
                    }
                    secondary = (AV2H_KERNEL_TYPE(
                        uint8_t, uint16_t))av2h_sample_clip_to_domain((sum + 64) >> 7U, domain);
                }
                weight = directional_ibp_weight(row >> (height >> 5U), column >> (width >> 5U),
                                                weight_dy);
                primary = (AV2H_KERNEL_TYPE(
                    uint8_t,
                    uint16_t))((primary * weight + secondary * (128U - weight) + 64U) >> 7U);
            }
            predictor[(size_t)row * predictor_stride_samples + column] = primary;
        }
    }
}

static void AV2H_KERNEL_NAME(
    directional_project_zone2_u8,
    directional_project_zone2_u16)(const Av2hLeafMode *mode, int angle, uint16_t width,
                                         uint16_t height, const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *above_edge,
                                         const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *left_edge, size_t edge_count,
                                   AV2H_KERNEL_TYPE(uint8_t, uint16_t) *predictor, size_t predictor_stride_samples,
                                         Av2hDirectionalProjectionPolicy policy,
                                         const Av2hSampleDomain *domain) {
    uint16_t row;
    const int dx = directional_derivative[180 - angle];
    const int dy = directional_derivative[angle - 90];
    const int min_base = -1 - mode->mrl_index;
    for (row = 0U; row < height; ++row) {
        uint16_t column;
        for (column = 0U; column < width; ++column) {
            int position = ((int)column << 6) - ((int)row + 1 + mode->mrl_index) * dx;
            int base = position >> 6;
            int shift = (position & 0x3f) >> 1;
            const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *reference = above_edge;
            int32_t sum = 0;
            size_t tap;
            if (base < min_base) {
                position = ((int)row << 6) - ((int)column + 1 + mode->mrl_index) * dy;
                base = position >> 6;
                shift = (position & 0x3f) >> 1;
                reference = left_edge;
            }
            if (policy.primary_linear) {
                sum = reference[base] * (32 - shift) + reference[base + 1] * shift;
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
                    int reference_index = base + (int)tap - 1;
                    AV2H_KERNEL_TYPE(uint8_t, uint16_t) sample = mode->mrl_index != 0U
                                         ? reference[reference_index]
                                         : AV2H_KERNEL_NAME(directional_reference, directional_reference_u16)(reference, edge_count,
                                                                 reference[-1], reference_index);
                    sum += dr_interp_filter[shift][tap] * sample;
                }
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                    policy.primary_linear ? (sum + 16) >> 5U : (sum + 64) >> 7U, domain);
        }
    }
}

static void AV2H_KERNEL_NAME(
    directional_project_zone3_u8,
    directional_project_zone3_u16)(const Av2hLeafMode *mode, int angle, uint16_t width,
                                         uint16_t height, const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *above_edge,
                                         const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *left_edge, size_t edge_count,
                                   AV2H_KERNEL_TYPE(uint8_t, uint16_t) *predictor, size_t predictor_stride_samples,
                                         Av2hDirectionalProjectionPolicy policy,
                                         const Av2hSampleDomain *domain) {
    uint16_t column;
    int dy = directional_derivative[270 - angle];
    int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
    int y = dy * (1 + mode->mrl_index);
    for (column = 0U; column < width; ++column, y += dy) {
        uint16_t row;
        int base = y >> 6;
        int shift = (y & 0x3f) >> 1;
        for (row = 0U; row < height; ++row, ++base) {
            int32_t sum = 0;
            AV2H_KERNEL_TYPE(uint8_t, uint16_t) primary;
            size_t tap;
            if (base > max_base || (policy.clamp_last && base == max_base)) {
                primary = left_edge[max_base];
            } else if (policy.primary_linear) {
                primary = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                    (left_edge[base] * (32 - shift) + left_edge[base + 1] * shift + 16) >> 5U,
                    domain);
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
                    int reference_index = base + (int)tap - 1;
                    AV2H_KERNEL_TYPE(uint8_t, uint16_t) sample = policy.unclamped_mrl && mode->mrl_index != 0U
                                         ? left_edge[reference_index]
                                         : AV2H_KERNEL_NAME(directional_reference, directional_reference_u16)(left_edge, edge_count,
                                                                 left_edge[-1], reference_index);
                    sum += dr_interp_filter[shift][tap] * sample;
                }
                primary = (AV2H_KERNEL_TYPE(
                    uint8_t, uint16_t))av2h_sample_clip_to_domain((sum + 64) >> 7U, domain);
            }
            if (mode->mrl_index == 0U && mode->enable_ibp && mode->angle_delta % 2 == 0) {
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
                } else if (policy.secondary_linear) {
                    secondary = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                        (above_edge[secondary_base] * (32 - secondary_shift) +
                         above_edge[secondary_base + 1] * secondary_shift + 16) >>
                            5U,
                        domain);
                } else {
                    for (tap = 0U; tap < 4U; ++tap) {
                        secondary_sum +=
                            dr_interp_filter[secondary_shift][tap] *
                            AV2H_KERNEL_NAME(
                                directional_reference,
                                directional_reference_u16)(above_edge, edge_count, above_edge[-1],
                                                  secondary_base + (int)tap - 1);
                    }
                    secondary =
                        (AV2H_KERNEL_TYPE(
                            uint8_t,
                            uint16_t))av2h_sample_clip_to_domain((secondary_sum + 64) >> 7U, domain);
                }
                weight = directional_ibp_weight(column >> (width >> 5U), row >> (height >> 5U),
                                                weight_dy);
                primary = (AV2H_KERNEL_TYPE(
                    uint8_t,
                    uint16_t))((primary * weight + secondary * (128U - weight) + 64U) >> 7U);
            }
            predictor[(size_t)row * predictor_stride_samples + column] = primary;
        }
    }
}
