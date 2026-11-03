#include "internal/intra_prediction.h"

#include "internal/sample_math.h"

#include <stddef.h>

static const int8_t d45_filter[4U][4U] = {
    { -9, 46, 102, -11 },
    { -12, 94, 56, -10 },
    { -3, 125, 8, -2 },
    { -8, 37, 109, -10 }
};

static const int16_t dr_interp_filter[32U][4U] = {
    { 0, 128, 0, 0 }, { -2, 127, 4, -1 }, { -3, 125, 8, -2 },
    { -5, 123, 13, -3 }, { -6, 121, 17, -4 }, { -7, 118, 22, -5 },
    { -9, 116, 27, -6 }, { -9, 112, 32, -7 }, { -10, 109, 37, -8 },
    { -11, 106, 41, -8 }, { -11, 102, 46, -9 }, { -12, 98, 52, -10 },
    { -12, 94, 56, -10 }, { -12, 90, 61, -11 },
    { -12, 85, 66, -11 }, { -12, 81, 71, -12 },
    { -12, 76, 76, -12 }, { -12, 71, 81, -12 },
    { -11, 66, 85, -12 }, { -11, 61, 90, -12 },
    { -10, 56, 94, -12 }, { -10, 52, 98, -12 },
    { -9, 46, 102, -11 }, { -8, 41, 106, -11 },
    { -8, 37, 109, -10 }, { -7, 32, 112, -9 },
    { -6, 27, 116, -9 }, { -5, 22, 118, -7 },
    { -4, 17, 121, -6 }, { -3, 13, 123, -5 },
    { -2, 8, 125, -3 }, { -1, 4, 127, -2 }
};

static const int8_t d67_filter[8U][4U] = {
    { -5, 22, 118, -7 },
    { -10, 109, 37, -8 },
    { -9, 46, 102, -11 },
    { -5, 123, 13, -3 },
    { -12, 71, 81, -12 },
    { -2, 8, 125, -3 },
    { -12, 94, 56, -10 },
    { -7, 32, 112, -9 }
};

static const int8_t d67_mrl4_filter[4U][4U] = {
    { -1, 4, 127, -2 },
    { -12, 85, 66, -11 },
    { -2, 8, 125, -3 },
    { -12, 90, 61, -11 }
};

static const int8_t d67_mrl8_filter[8U][4U] = {
    { -1, 4, 127, -2 },
    { -12, 85, 66, -11 },
    { -2, 8, 125, -3 },
    { -12, 90, 61, -11 },
    { -3, 13, 123, -5 },
    { -12, 94, 56, -10 },
    { -4, 17, 121, -6 },
    { -12, 98, 52, -10 }
};

static const int8_t d67_mrl3_filter[4U][4U] = {
    { -12, 94, 56, -10 },
    { -1, 4, 127, -2 },
    { -11, 66, 85, -12 },
    { -7, 118, 22, -5 }
};

static const int16_t directional_derivative[90U] = {
    0, 4096, 2048, 1365, 1024, 819, 682, 585, 512, 455, 409, 409, 409, 372,
    341, 292, 273, 256, 227, 215, 204, 186, 178, 170, 157, 151, 146, 136,
    132, 128, 117, 110, 107, 99, 97, 97, 93, 87, 83, 81, 77, 74, 73, 69,
    66, 64, 62, 59, 56, 55, 53, 50, 49, 47, 44, 42, 42, 41, 38, 37, 35,
    32, 31, 30, 28, 27, 26, 24, 23, 22, 20, 19, 18, 16, 15, 14, 12, 11,
    10, 10, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
};

static uint8_t directional_reference(const uint8_t *reference, size_t reference_count,
                                     uint8_t above_left, int index);
static uint16_t directional_reference_u16(const uint16_t *reference, size_t reference_count,
                                          uint16_t above_left, int index);
static void directional_project_zone1_u8(const Av2hLeafMode *mode, int angle, uint16_t width,
                                         uint16_t height, const uint8_t *above_edge,
                                         const uint8_t *left_edge, uint8_t *predictor,
                                         size_t predictor_stride_samples, uint8_t use_idif,
                                         const Av2hSampleDomain *domain);
static void directional_project_zone1_u16(const Av2hLeafMode *mode, int angle, uint16_t width,
                                          uint16_t height, const uint16_t *above_edge,
                                          const uint16_t *left_edge, uint16_t *predictor,
                                          size_t predictor_stride_samples, uint8_t use_idif,
                                          const Av2hSampleDomain *domain);
static void directional_project_zone2_u8(const Av2hLeafMode *mode, int angle, uint16_t width,
                                         uint16_t height, const uint8_t *above_edge,
                                         const uint8_t *left_edge, size_t edge_count,
                                         uint8_t *predictor, size_t predictor_stride_samples,
                                         uint8_t use_idif, const Av2hSampleDomain *domain);
static void directional_project_zone2_u16(const Av2hLeafMode *mode, int angle, uint16_t width,
                                          uint16_t height, const uint16_t *above_edge,
                                          const uint16_t *left_edge, size_t edge_count,
                                          uint16_t *predictor, size_t predictor_stride_samples,
                                          uint8_t use_idif, const Av2hSampleDomain *domain);
static void directional_project_zone3_u8(const Av2hLeafMode *mode, int angle, uint16_t width,
                                         uint16_t height, const uint8_t *above_edge,
                                         const uint8_t *left_edge, size_t edge_count,
                                         uint8_t *predictor, size_t predictor_stride_samples,
                                         uint8_t use_idif, const Av2hSampleDomain *domain);
static void directional_project_zone3_u16(const Av2hLeafMode *mode, int angle, uint16_t width,
                                          uint16_t height, const uint16_t *above_edge,
                                          const uint16_t *left_edge, size_t edge_count,
                                          uint16_t *predictor, size_t predictor_stride_samples,
                                          uint8_t use_idif, const Av2hSampleDomain *domain);

static uint8_t directional_ibp_weight(unsigned int row,
                                      unsigned int column,
                                      int dy) {
    uint32_t y = (uint32_t)dy * (column + 1U);
    uint32_t distance = ((row + 1U) << 6U) + y;
    int shift;
    uint16_t divisor = av2h_sample_resolve_divisor_32(distance, &shift);
    shift -= 7;
    return (uint8_t)((y * divisor + ((uint32_t)1U << (shift - 1))) >> shift);
}

static int directional_ibp_canonical_derivative(int angle) {
    if (angle <= 39) return directional_derivative[51];
    if (angle == 45) return directional_derivative[45];
    if (angle == 51) return directional_derivative[39];
    if (angle == 61) return directional_derivative[29];
    if (angle == 67) return directional_derivative[23];
    if (angle == 73) return directional_derivative[17];
    if (angle == 84) return directional_derivative[6];
    return 0;
}

static Av2hStatus predict_directional(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples,
    uint8_t use_idif,
    uint8_t filter_edges) {
    uint8_t above_storage[141U];
    uint8_t left_storage[141U];
    uint8_t *above = &above_storage[5U];
    uint8_t *left = &left_storage[5U];
    size_t edge_count =
        (size_t)width + height + 2U + 2U * mode->mrl_index;

    uint16_t row;
    uint16_t column;
    int missing_left_ibp;
    uint8_t edge_ibp = (uint8_t)(mode->enable_ibp || mode->edge_ibp);
    if (mode->mrl_index == 1U) {
        ++angle;
    } else if (mode->mrl_index == 2U) {
        --angle;
    }
    if (angle <= 0 || angle >= 270 || mode->multi_line_mrl != 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (angle == 90 || angle == 180) {
        const uint8_t *edge = angle == 90 ? references->above :
            references->left;
        size_t edge_count_available = angle == 90 ? references->above_count :
            references->left_count;
        uint8_t fallback = angle == 90 ?
            (references->left != NULL && references->left_count != 0U ?
                references->left[0] : av2h_sample_below_midpoint_8bit()) :
            (references->above != NULL && references->above_count != 0U ?
                references->above[0] : av2h_sample_above_midpoint_8bit());
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t edge_index = angle == 90 ? column : row;
                predictor[(size_t)row * predictor_stride_samples + column] =
                    edge != NULL && edge_count_available != 0U ?
                        edge[edge_index < edge_count_available ? edge_index :
                             edge_count_available - 1U] : fallback;
            }
        }
        return AV2H_OK;
    }
    missing_left_ibp =
        ((mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0 &&
          width == 32U && height == 16U) ||
         (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
          width == 64U && height == 16U)) &&
        mode->mrl_index == 0U && mode->enable_ibp == 1U &&
        (references->left == NULL || references->left_count == 0U) &&
        references->above != NULL && references->above_count != 0U;
    if (edge_count > 136U) return AV2H_UNSUPPORTED;
    av2h_prepare_directional_edges(
        mode, angle, width, height, references, filter_edges, above, left, edge_count, missing_left_ibp, edge_ibp);
    if (angle < 90) {
        directional_project_zone1_u8(mode, angle, width, height, above, left, predictor,
                                     predictor_stride_samples, use_idif, &av2h_sample_domain_8bit);
        return AV2H_OK;
    }
    if (angle < 180) {
        directional_project_zone2_u8(mode, angle, width, height, above, left, edge_count, predictor,
                                     predictor_stride_samples, use_idif, &av2h_sample_domain_8bit);
        return AV2H_OK;
    }
    {
        directional_project_zone3_u8(mode, angle, width, height, above, left, edge_count, predictor,
                                     predictor_stride_samples, use_idif, &av2h_sample_domain_8bit);
        return AV2H_OK;
    }
    return AV2H_OK;
}

int av2h_directional_base_angle(Av2hIntraMode mode) {
    return mode == AV2H_INTRA_VERTICAL ? 90 :
           mode == AV2H_INTRA_HORIZONTAL ? 180 :
           mode == AV2H_INTRA_D45 ? 45 :
           mode == AV2H_INTRA_D67 ? 67 :
           mode == AV2H_INTRA_D113 ? 113 :
           mode == AV2H_INTRA_D135 ? 135 :
           mode == AV2H_INTRA_D157 ? 157 :
           mode == AV2H_INTRA_D203 ? 203 : 0;
}

int av2h_luma_wide_angle(int angle, uint16_t width, uint16_t height) {
    int threshold = height == 2U * width ? 61 :
                    height == 4U * width ? 73 :
                    height == 8U * width ? 82 :
                    height == 16U * width ? 86 : 0;
    if (threshold != 0 && angle < threshold) return angle + 180;
    threshold = width == 2U * height ? 61 :
                width == 4U * height ? 73 :
                width == 8U * height ? 82 :
                width == 16U * height ? 86 : 0;
    return threshold != 0 && angle > 270 - threshold ? angle - 180 : angle;
}

Av2hStatus av2h_predict_directional(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples,
    uint8_t use_idif,
    uint8_t filter_edges) {
    return predict_directional(
        mode, angle, width, height, references, predictor, predictor_stride_samples,
        use_idif, filter_edges);
}

Av2hStatus av2h_predict_directional_idif(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    return predict_directional(
        mode, angle, width, height, references,
        predictor, predictor_stride_samples, 1U, 1U);
}

Av2hStatus av2h_predict_directional_idif_unfiltered(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    return predict_directional(
        mode, angle, width, height, references,
        predictor, predictor_stride_samples, 1U, 0U);
}

Av2hStatus av2h_predict_d45_8x4_ibp(
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    static const uint8_t weights[4U][8U] = {
        { 64U, 85U, 96U, 103U, 107U, 110U, 112U, 114U },
        { 43U, 64U, 77U, 85U, 92U, 96U, 100U, 103U },
        { 32U, 51U, 64U, 73U, 80U, 85U, 90U, 93U },
        { 26U, 43U, 55U, 64U, 71U, 77U, 81U, 85U }
    };
    uint8_t above_edge[13U];
    uint8_t left_edge[13U];
    uint8_t source[13U];
    uint8_t base;
    size_t index;
    uint16_t row;
    uint16_t column;
    base = references->above_count != 0U ? references->above[0] :
        references->left_count != 0U ? references->left[0] :
        av2h_sample_midpoint_8bit();
    above_edge[0] = references->has_above_left ?
        references->above_left : base;
    left_edge[0] = above_edge[0];
    for (index = 0U; index < 12U; ++index) {
        if (references->above_count != 0U) {
            size_t above_index = index < references->above_count ?
                index : references->above_count - 1U;
            above_edge[index + 1U] = references->above[above_index];
        } else {
            above_edge[index + 1U] = base;
        }
        if (references->left_count != 0U) {
            size_t left_index = index < references->left_count ?
                index : references->left_count - 1U;
            left_edge[index + 1U] = references->left[left_index];
        } else {
            left_edge[index + 1U] = base;
        }
    }
    for (index = 0U; index < 13U; ++index) {
        source[index] = above_edge[index];
    }
    for (index = 1U; index < 13U; ++index) {
        size_t next = index < 12U ? index + 1U : index;
        above_edge[index] = (uint8_t)(
            (source[index - 1U] + 2U * source[index] +
             source[next] + 2U) >> 2U);
    }
    for (index = 0U; index < 13U; ++index) {
        source[index] = left_edge[index];
    }
    for (index = 1U; index < 13U; ++index) {
        size_t next = index < 12U ? index + 1U : index;
        left_edge[index] = (uint8_t)(
            (source[index - 1U] + 2U * source[index] +
             source[next] + 2U) >> 2U);
    }
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            unsigned int weight = weights[row][column];
            unsigned int primary = above_edge[row + column + 2U];
            unsigned int secondary = left_edge[row + column + 2U];
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)((primary * weight +
                           secondary * (128U - weight) + 64U) >> 7U);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_predict_luma_directional_feature(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    Av2hLeafMode primary_mode;
    Av2hLeafMode secondary_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[AV2H_RECON_SAMPLE_CAPACITY];
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    int mrl_angle_delta = av2h_intra_mrl_angle_delta(mode->mrl_index);
    int secondary_angle = av2h_luma_wide_angle(
        av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
            mrl_angle_delta,
        width, height);
    int primary_angle = secondary_angle - mrl_angle_delta;
    if (mode->multi_line_mrl == 0U) {
        return av2h_predict_directional_idif(
            mode, primary_angle, width, height, references,
            predictor, predictor_stride_samples);
    }
    if (mode->multi_line_mrl != 1U || mode->mrl_index == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((width != 4U || height != 4U) &&
        (references->secondary_above == NULL ||
         references->secondary_above_count == 0U) &&
        (references->secondary_left == NULL ||
         references->secondary_left_count == 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    primary_mode.enable_ibp = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    secondary_mode.enable_ibp = 0U;
    status = av2h_predict_directional_idif(
        &primary_mode, primary_angle, width, height, references,
        predictor, predictor_stride_samples);
    if (status != AV2H_OK) return status;
    if (width == 4U && height == 4U) return AV2H_OK;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count = references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count = references->secondary_left_count;
    secondary_references.above_left = references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_predict_directional_idif_unfiltered(
        &secondary_mode, secondary_angle, width, height,
        &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] + 1U) >>
                1U);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_predict_d203_fallback(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint8_t left_storage[44U];
    uint8_t *left = &left_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    if (mode->mode != AV2H_INTRA_D203 ||
                (mode->angle_delta != 0 && mode->angle_delta != -3 &&
                 mode->angle_delta != 3) ||
        !((width == 4U && height == 4U) ||
                    (width == 4U && height == 16U) ||
                    (width == 8U && height == 16U) ||
                    (width == 8U && height == 32U) ||
                    (width == 16U && height == 16U)) ||
        (mode->angle_delta == 3 &&
         !((width == 4U && height == 4U) ||
           (width == 8U && height == 4U)))) {
        return AV2H_UNSUPPORTED;
    }
    if (references->left == NULL || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    left[-1] = references->left[0];
    for (index = 0U; index < (size_t)width + height; ++index) {
        size_t source = index < references->left_count ?
            index : references->left_count - 1U;
        left[index] = references->left[source];
    }
    if (mode->angle_delta == -3 && width == 8U && height == 32U) {
        static const uint8_t kernel[5U] = { 2U, 4U, 4U, 4U, 2U };
        uint8_t source[41U];
        uint8_t *filtered = &left[-1];
        size_t filtered_count = (size_t)width + height + 1U;
        for (index = 0U; index < filtered_count; ++index) {
            source[index] = filtered[index];
        }
        for (index = 1U; index < filtered_count; ++index) {
            size_t tap;
            unsigned int sum = 0U;
            for (tap = 0U; tap < 5U; ++tap) {
                int32_t source_index = (int32_t)index - 2 + (int32_t)tap;
                if (source_index < 0) source_index = 0;
                if (source_index >= (int32_t)filtered_count) {
                    source_index = (int32_t)filtered_count - 1;
                }
                sum += kernel[tap] * source[source_index];
            }
            filtered[index] = (uint8_t)((sum + 8U) >> 4U);
        }
    }
    left[width + height] = left[width + height - 1U];
    left[width + height + 1U] = left[width + height - 1U];
    for (column = 0U; column < width; ++column) {
        uint16_t derivative =
            mode->angle_delta == 0 ? 24U :
            mode->angle_delta == -3 ? 12U : 38U;
        uint16_t offset = (uint16_t)((column + 1U) * derivative);
        uint16_t base = offset >> 6U;
        uint8_t shift = (uint8_t)((offset >> 1U) & 0x1fU);
        for (row = 0U; row < height; ++row, ++base) {
            int32_t value;
            if (base <= width + height - 1U) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                           left[(int32_t)base + (int32_t)tap - 1];
                }
                value = (sum + 64) >> 7U;
            } else {
                value = left[width + height - 1U];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip(value);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_predict_vertical_multiline(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (references->above == NULL || references->above_count == 0U ||
        references->secondary_above == NULL ||
        references->secondary_above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t primary = column < references->above_count ?
                column : references->above_count - 1U;
            size_t secondary =
                column < references->secondary_above_count ?
                    column : references->secondary_above_count - 1U;
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)((references->above[primary] +
                           references->secondary_above[secondary] + 1U) >>
                          1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_horizontal_mrl(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (references->left == NULL || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        uint8_t value = references->left[
            row < references->left_count ?
                row : references->left_count - 1U];
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] = value;
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_horizontal_multiline(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (references->left == NULL || references->left_count == 0U ||
        references->secondary_left == NULL ||
        references->secondary_left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        size_t primary = row < references->left_count ?
            row : references->left_count - 1U;
        size_t secondary = row < references->secondary_left_count ?
            row : references->secondary_left_count - 1U;
        uint8_t value = (uint8_t)(
            (references->left[primary] +
             references->secondary_left[secondary] + 1U) >> 1U);
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] = value;
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_directional_multiline_ibp(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    Av2hLeafMode primary_mode;
    Av2hLeafMode secondary_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[16U * 32U];
    Av2hStatus status;
    if (references->secondary_above == NULL ||
        references->secondary_above_count == 0U ||
        references->secondary_left == NULL ||
        references->secondary_left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    int angle = mode->mode == AV2H_INTRA_D135 ? 138 :
        mode->mode == AV2H_INTRA_D157 ? 157 :
        mode->mode == AV2H_INTRA_VERTICAL ? 99 : 180;
    status = av2h_predict_directional_idif(
        &primary_mode, angle, width, height, references,
        predictor, predictor_stride_samples);
    if (status != AV2H_OK) return status;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count =
        references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count =
        references->secondary_left_count;
    secondary_references.above_left =
        references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_predict_directional_idif_unfiltered(
        &secondary_mode, angle, width, height, &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] +
                 1U) >> 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_horizontal_multiline_angle(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    Av2hLeafMode primary_mode;
    Av2hLeafMode secondary_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[32U * 16U];
            int angle = 180 + mode->angle_delta * 3;
    Av2hStatus status;
    if (references->secondary_above == NULL ||
        references->secondary_above_count == 0U ||
        references->secondary_left == NULL ||
        references->secondary_left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    status = av2h_predict_directional_idif(
        &primary_mode, angle, width, height, references,
        predictor, predictor_stride_samples);
    if (status != AV2H_OK) return status;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count =
        references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count =
        references->secondary_left_count;
    secondary_references.above_left =
        references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_predict_directional_idif_unfiltered(
        &secondary_mode, angle, width, height, &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] +
                 1U) >> 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_vertical_reference(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] =
                references->above[column < references->above_count ?
                    column : references->above_count - 1U];
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_horizontal_multiline_tall(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    Av2hLeafMode primary_mode;
    Av2hLeafMode secondary_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[16U * 64U];
    Av2hStatus status;
    if (references->secondary_above == NULL ||
        references->secondary_above_count == 0U ||
        references->secondary_left == NULL ||
        references->secondary_left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    status = av2h_predict_directional_idif(
        &primary_mode, 186, width, height, references,
        predictor, predictor_stride_samples);
    if (status != AV2H_OK) return status;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count =
        references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count =
        references->secondary_left_count;
    secondary_references.above_left =
        references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_predict_directional_idif_unfiltered(
        &secondary_mode, 186, width, height, &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] +
                 1U) >> 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_vertical_multiline_tall(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    Av2hLeafMode single_line_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[8U * 32U];
    Av2hStatus status;
    if (references->secondary_above == NULL ||
        references->secondary_above_count == 0U ||
        references->secondary_left == NULL ||
        references->secondary_left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&single_line_mode, mode, sizeof(single_line_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    single_line_mode.mrl_index = 0U;
    single_line_mode.multi_line_mrl = 0U;
    status = av2h_predict_directional_idif(
        &single_line_mode, 91, width, height, references,
        predictor, predictor_stride_samples);
    if (status != AV2H_OK) return status;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count =
        references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count = references->secondary_left_count;
    secondary_references.above_left =
        references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_predict_directional_idif(
        &single_line_mode, 91, width, height, &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] +
                 1U) >> 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d203_multiline(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    Av2hLeafMode primary_mode;
    Av2hLeafMode secondary_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[16U * 64U];
    Av2hStatus status;
    if (references->secondary_left == NULL ||
        references->secondary_left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    status = av2h_predict_directional_idif(
        &primary_mode, 206, width, height, references,
        predictor, predictor_stride_samples);
    if (status != AV2H_OK) return status;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count =
        references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count =
        references->secondary_left_count;
    secondary_references.above_left =
        references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_predict_directional_idif_unfiltered(
        &secondary_mode, 206, width, height, &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] +
                 1U) >> 1U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_vertical_small(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_UNSUPPORTED;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] =
                references->above[column < references->above_count ?
                    column : references->above_count - 1U];
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_horizontal(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < height; ++row) {
        uint8_t value = references->left != NULL &&
            references->left_count != 0U ?
            references->left[row < references->left_count ?
                             row : references->left_count - 1U] :
            references->above != NULL &&
                references->above_count != 0U ?
                references->above[0] :
                av2h_sample_above_midpoint_8bit();
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] =
                value;
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d45_missing_edges_ibp(
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < 32U; ++row) {
        uint16_t count_129 = row < 4U ? 0U :
            (uint16_t)(2U * (((row - 4U) / 6U) + 1U));
        uint16_t count_not_127 =
            (uint16_t)(6U * (row / 2U + 1U));
        if (count_not_127 > 32U) count_not_127 = 32U;
        for (column = 0U; column < 32U; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] =
                column < count_129 ?
                    av2h_sample_above_midpoint_8bit() :
                column < count_not_127 ?
                    av2h_sample_midpoint_8bit() :
                    av2h_sample_below_midpoint_8bit();
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d45_mrl1(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t edge[68U];
    int x = directional_derivative[46U] * 2;
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index <= 65U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        edge[index] = references->above[source];
    }
    edge[66U] = edge[65U];
    edge[67U] = edge[65U];
    for (row = 0U; row < height; ++row, x += directional_derivative[46U]) {
        int base = x >> 6;
        int shift = (x & 0x3f) >> 1;
        for (column = 0U; column < width; ++column, ++base) {
            int32_t sum;
            if (base > 65) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    edge[65U];
                continue;
            }
            sum = dr_interp_filter[shift][0U] * edge[base - 1] +
                  dr_interp_filter[shift][1U] * edge[base] +
                  dr_interp_filter[shift][2U] * edge[base + 1] +
                  dr_interp_filter[shift][3U] * edge[base + 2];
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_zone2_mrl2(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t above_storage[53U];
    uint8_t left_storage[69U];
    uint8_t *mrl_above = &above_storage[4U];
    uint8_t *mrl_left = &left_storage[4U];
    const int angle = av2h_directional_base_angle(mode->mode) +
              mode->angle_delta * 3 - 1;
    const int dx = directional_derivative[180 - angle];
    const int dy = directional_derivative[angle - 90];
    if (references->above == NULL || references->left == NULL ||
        references->above_count == 0U ||
        references->left_count == 0U ||
        references->negative_count < 3U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 49U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        mrl_above[index] = references->above[source];
    }
    for (index = 0U; index < 65U; ++index) {
        size_t source = index < references->left_count ?
            index : references->left_count - 1U;
        mrl_left[index] = references->left[source];
    }
    for (index = 1U; index <= 3U; ++index) {
        mrl_above[-(int)index] = references->above_negative[index];
        mrl_left[-(int)index] = references->left_negative[index];
    }
    mrl_above[-4] = mrl_above[-3];
    mrl_left[-4] = mrl_left[-3];
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int x = ((int)column << 6) - ((int)row + 3) * dx;
            int base = x >> 6;
            int shift = (x & 0x3f) >> 1;
            const uint8_t *edge = mrl_above;
            int32_t sum;
            if (base < -3) {
                int y = ((int)row << 6) -
                    ((int)column + 3) * dy;
                base = y >> 6;
                shift = (y & 0x3f) >> 1;
                edge = mrl_left;
            }
            sum = dr_interp_filter[shift][0U] * edge[base - 1] +
                  dr_interp_filter[shift][1U] * edge[base] +
                  dr_interp_filter[shift][2U] * edge[base + 1] +
                  dr_interp_filter[shift][3U] * edge[base + 2];
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d45_diagonal(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint8_t above_storage[45U];
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    size_t edge_count = (size_t)width + height;
    if (references->above_count == 0U && references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < edge_count; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - (references->above_count != 0U);
        above[index] = references->above_count != 0U ?
            references->above[source] : references->left[0];
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t source = (size_t)row + column + 1U;
            predictor[(size_t)row * predictor_stride_samples + column] =
                above[source];
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d67_mrl1_minus_one(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t storage[21U];
    uint8_t *edge = &storage[1U];
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 20U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        edge[index] = references->above[source];
    }
    edge[-1] = edge[0];
    for (row = 0U; row < height; ++row) {
        uint16_t base = (uint16_t)((54U + row * 27U) >> 6U);
        for (column = 0U; column < width; ++column, ++base) {
            size_t tap;
            int32_t sum = 0;
            for (tap = 0U; tap < 4U; ++tap) {
                sum += d67_filter[row][tap] *
                       edge[(int32_t)base + (int32_t)tap - 1];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d67_4x4_mrl1_minus_two(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t storage[13U];
    uint8_t *edge = &storage[1U];
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    edge[-1] = references->has_above_left ? references->above_left :
        references->above[0];
    for (index = 0U; index < 12U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        edge[index] = references->above[source];
    }
    for (row = 0U; row < height; ++row) {
        uint16_t base = (uint16_t)((62U + row * 31U) >> 6U);
        for (column = 0U; column < width; ++column, ++base) {
            size_t tap;
            int32_t sum = 0;
            for (tap = 0U; tap < 4U; ++tap) {
                sum += d67_mrl4_filter[row][tap] *
                       edge[(int32_t)base + (int32_t)tap - 1];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d67_8x8_mrl1_minus_two(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t storage[21U];
    uint8_t *edge = &storage[1U];
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    edge[-1] = references->has_above_left ? references->above_left :
        references->above[0];
    for (index = 0U; index < 20U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        edge[index] = references->above[source];
    }
    for (row = 0U; row < height; ++row) {
        uint16_t base = (uint16_t)((62U + row * 31U) >> 6U);
        for (column = 0U; column < width; ++column, ++base) {
            size_t tap;
            int32_t sum = 0;
            for (tap = 0U; tap < 4U; ++tap) {
                sum += d67_mrl8_filter[row][tap] *
                       edge[(int32_t)base + (int32_t)tap - 1];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d67_4x4_mrl3_minus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t edge[12U];
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 12U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        edge[index] = references->above[source];
    }
    for (row = 0U; row < height; ++row) {
        uint16_t base = (uint16_t)((152U + row * 38U) >> 6U);
        for (column = 0U; column < width; ++column, ++base) {
            size_t tap;
            int32_t sum = 0;
            for (tap = 0U; tap < 4U; ++tap) {
                sum += d67_mrl3_filter[row][tap] *
                       edge[base + tap - 1U];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d45_minus_one(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint8_t above_storage[45U];
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t source[41U];
    uint8_t *edge = above;
    uint8_t *filtered = &edge[-1];
    size_t edge_count = (size_t)width + height;
    size_t filtered_count = edge_count + 1U;
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    edge[-1] = references->has_above_left ? references->above_left :
                                                references->above[0];
    for (index = 0U; index < edge_count; ++index) {
        size_t reference = index < references->above_count ?
            index : references->above_count - 1U;
        edge[index] = references->above[reference];
    }
    for (index = 0U; index < filtered_count; ++index) {
        source[index] = filtered[index];
    }
    for (index = 1U; index < filtered_count; ++index) {
        static const uint8_t kernel[5U] = { 2U, 4U, 4U, 4U, 2U };
        size_t tap;
        unsigned int sum = 0U;
        for (tap = 0U; tap < 5U; ++tap) {
            int32_t source_index = (int32_t)index - 2 + (int32_t)tap;
            if (source_index < 0) source_index = 0;
            if (source_index >= (int32_t)filtered_count) {
                source_index = (int32_t)filtered_count - 1;
            }
            sum += kernel[tap] * source[source_index];
        }
        filtered[index] = (uint8_t)((sum + 8U) >> 4U);
    }
    edge[edge_count] = edge[edge_count - 1U];
    edge[edge_count + 1U] = edge[edge_count - 1U];
    for (row = 0U; row < height; ++row) {
        uint16_t position = (uint16_t)((row + 1U) * 73U);
        uint16_t base = position >> 6U;
        uint8_t shift = (uint8_t)((position & 63U) >> 1U);
        for (column = 0U; column < width; ++column, ++base) {
            size_t tap;
            int32_t sum = 0;
            if (base >= edge_count) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    edge[edge_count - 1U];
                continue;
            }
            for (tap = 0U; tap < 4U; ++tap) {
                sum += dr_interp_filter[shift][tap] *
                       edge[(int32_t)base + (int32_t)tap - 1];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d45_plus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint8_t above_storage[45U];
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    above[-1] = references->above[0];
    for (index = 0U; index < 12U; ++index) {
        size_t source = index < references->above_count ?
            index : references->above_count - 1U;
        above[index] = references->above[source];
    }
    above[12] = above[11];
    above[13] = above[11];
    for (row = 0U; row < height; ++row) {
        uint16_t base = (uint16_t)(((row + 1U) * 44U) >> 6U);
        for (column = 0U; column < width; ++column, ++base) {
            int32_t value;
            if (base <= 11U) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d45_filter[row][tap] *
                           above[(int32_t)base + (int32_t)tap - 1];
                }
                value = (sum + 64) >> 7U;
            } else {
                value = above[11];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip(value);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d45_mrl1_plus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    const uint16_t derivative = 97U;
    if (references->left == NULL || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (column = 0U; column < width; ++column) {
        uint16_t position = (uint16_t)((column + 2U) * derivative);
        uint16_t base = position >> 6U;
        uint8_t shift = (uint8_t)((position >> 1U) & 0x1fU);
        for (row = 0U; row < height; ++row, ++base) {
            size_t tap;
            int32_t sum = 0;
            for (tap = 0U; tap < 4U; ++tap) {
                size_t source = base + tap - 1U;
                if (source >= references->left_count) {
                    source = references->left_count - 1U;
                }
                sum += dr_interp_filter[shift][tap] *
                       references->left[source];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d203_mrl1_plus_three(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint8_t left_storage[44U];
    uint8_t *left = &left_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    const uint16_t derivative = 41U;
    size_t edge_count = (size_t)width + height + 2U;
    if (references->left == NULL || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    left[-1] = references->has_above_left ? references->above_left :
                                               references->left[0];
    for (index = 0U; index < edge_count; ++index) {
        size_t source = index < references->left_count ?
            index : references->left_count - 1U;
        left[index] = references->left[source];
    }
    left[edge_count] = left[edge_count - 1U];
    left[edge_count + 1U] = left[edge_count - 1U];
    for (column = 0U; column < width; ++column) {
        uint16_t position =
            (uint16_t)((column + 1U + mode->mrl_index) * derivative);
        uint16_t base = position >> 6U;
        uint8_t shift = (uint8_t)((position & 63U) >> 1U);
        for (row = 0U; row < height; ++row, ++base) {
            size_t tap;
            int32_t sum = 0;
            for (tap = 0U; tap < 4U; ++tap) {
                sum += dr_interp_filter[shift][tap] *
                       left[(int32_t)base + (int32_t)tap - 1];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_d203_plus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    uint8_t above_storage[45U];
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    const uint16_t derivative = 107U;
    size_t edge_count = (size_t)width + height;
    uint8_t source[13U];
    uint8_t *filtered = &above[-1];
    if (references->above == NULL || references->above_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    above[-1] = references->has_above_left ? references->above_left :
                                               references->above[0];
    for (index = 0U; index < edge_count; ++index) {
        size_t reference = index < references->above_count ?
            index : references->above_count - 1U;
        above[index] = references->above[reference];
    }
    for (index = 0U; index <= edge_count; ++index) {
        source[index] = filtered[index];
    }
    for (index = 1U; index <= edge_count; ++index) {
        size_t previous = index - 1U;
        size_t next = index < edge_count ? index + 1U : index;
        filtered[index] = (uint8_t)(
            (source[previous] + 2U * source[index] + source[next] + 2U) >>
            2U);
    }
    above[edge_count] = above[edge_count - 1U];
    above[edge_count + 1U] = above[edge_count - 1U];
    for (row = 0U; row < height; ++row) {
        uint16_t position = (uint16_t)((row + 1U) * derivative);
        uint16_t base = position >> 6U;
        uint8_t shift = (uint8_t)((position & 63U) >> 1U);
        for (column = 0U; column < width; ++column, ++base) {
            size_t tap;
            int32_t sum = 0;
            if (base > edge_count - 1U) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    above[edge_count - 1U];
                continue;
            }
            for (tap = 0U; tap < 4U; ++tap) {
                sum += dr_interp_filter[shift][tap] *
                       above[(int32_t)base + (int32_t)tap - 1];
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip((sum + 64) >> 7U);
        }
    }
    return AV2H_OK;

}

Av2hStatus av2h_predict_multiline_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                      const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                      size_t predictor_stride_samples, uint8_t use_idif,
                                      const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    uint16_t row;
    uint16_t column;
    Av2hLeafMode primary_mode;
    Av2hLeafMode secondary_mode;
    Av2hLumaReferencesU16 secondary_references;
    uint16_t secondary_predictor[AV2H_RECON_SAMPLE_CAPACITY];
    Av2hStatus status;
    if ((width != 4U || height != 4U) &&
        (references->secondary_above == NULL ||
         references->secondary_above_count == 0U) &&
        (references->secondary_left == NULL ||
         references->secondary_left_count == 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references,
        sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    primary_mode.enable_ibp = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    secondary_mode.enable_ibp = 0U;
    status = av2h_luma_predict_u16_adjusted(&primary_mode, width, height, references, predictor,
                                            predictor_stride_samples, 0, 1U, use_idif, domain);
    if (status != AV2H_OK) return status;
    if (width == 4U && height == 4U) return AV2H_OK;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count =
        references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count =
        references->secondary_left_count;
    secondary_references.above_left =
        references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = av2h_luma_predict_u16_adjusted(
        &secondary_mode, width, height, &secondary_references, secondary_predictor, width,
        av2h_intra_mrl_angle_delta(mode->mrl_index), 1U, use_idif, domain);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output =
                (size_t)row * predictor_stride_samples + column;
            predictor[output] = (uint16_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] +
                 1U) >> 1U);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_predict_zone2_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, uint8_t disable_edge_filter,
                                  uint8_t use_idif, int angle, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;

    uint16_t above_storage[141U];
    uint16_t left_storage[141U];
    uint16_t *above_edge = &above_storage[5U];
    uint16_t *left_edge = &left_storage[5U];
    size_t edge_count = (size_t)width + height + 2U + 2U * mode->mrl_index;

    Av2hStatus edge_status =
        av2h_prepare_directional_zone2_u16(mode, width, height, references, disable_edge_filter,
                                           angle, above_edge, left_edge, edge_count, domain);
    if (edge_status != AV2H_OK)
        return edge_status;
    directional_project_zone2_u16(mode, angle, width, height, above_edge, left_edge, edge_count,
                                  predictor, predictor_stride_samples, use_idif, domain);
    return AV2H_OK;
}

Av2hStatus av2h_predict_zone1_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, uint8_t disable_edge_filter,
                                  uint8_t use_idif, int angle, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;

    uint16_t above_storage[141U];
    uint16_t left_storage[141U];
    uint16_t *above_edge = &above_storage[5U];
    uint16_t *left_edge = &left_storage[5U];
    int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
    size_t edge_count = (size_t)width + height + 2U + 2U * mode->mrl_index;

    Av2hStatus edge_status = av2h_prepare_directional_zone1_u16(
        mode, width, height, references, disable_edge_filter, angle, above_edge, left_edge,
        max_base, edge_count, domain);
    if (edge_status != AV2H_OK)
        return edge_status;
    directional_project_zone1_u16(mode, angle, width, height, above_edge, left_edge, predictor,
                                  predictor_stride_samples, use_idif, domain);
    return AV2H_OK;
}

Av2hStatus av2h_predict_zone3_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, uint8_t disable_edge_filter,
                                  uint8_t use_idif, int angle, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;

    uint16_t above_storage[141U];
    uint16_t left_storage[141U];
    uint16_t *above_edge = &above_storage[5U];
    uint16_t *left_edge = &left_storage[5U];
    int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
    size_t edge_count = (size_t)width + height + 2U + 2U * mode->mrl_index;

    Av2hStatus edge_status = av2h_prepare_directional_zone3_u16(
        mode, width, height, references, disable_edge_filter, angle, above_edge, left_edge,
        max_base, edge_count, domain);
    if (edge_status != AV2H_OK)
        return edge_status;
    directional_project_zone3_u16(mode, angle, width, height, above_edge, left_edge, edge_count,
                                  predictor, predictor_stride_samples, use_idif, domain);
    return AV2H_OK;
}

Av2hStatus av2h_predict_cardinal_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                     const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                     size_t predictor_stride_samples,
                                     const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t row;
    uint16_t column;
    int have_above;
    int have_left;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    have_above = above != NULL && above_count != 0U;
    have_left = left != NULL && left_count != 0U;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            predictor[(size_t)row * predictor_stride_samples + column] =
                mode->mode == AV2H_INTRA_VERTICAL
                    ? have_above  ? above[column < above_count ? column : above_count - 1U]
                      : have_left ? left[0]
                                  : (domain->midpoint - 1U)
                : have_left  ? left[row < left_count ? row : left_count - 1U]
                : have_above ? above[0]
                             : (domain->midpoint + 1U);
        }
    }
    return AV2H_OK;
}

#include "internal/sample_kernel_u8.inc"
#include "internal/intra_prediction_directional_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/intra_prediction_directional_kernel.inc"
#include "internal/sample_kernel_end.inc"
