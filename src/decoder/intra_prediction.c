#include "internal/intra_prediction.h"

#include <stddef.h>

Av2hStatus av2h_luma_predict(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                             const Av2hLumaReferences *references, uint8_t *predictor,
                             size_t predictor_stride_samples);
Av2hStatus av2h_luma_predict_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                 const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                 size_t predictor_stride_samples, const Av2hSampleDomain *domain);

int av2h_luma_prediction_dimension_supported(uint16_t dimension) {
    return dimension == 4U || dimension == 8U || dimension == 16U || dimension == 32U ||
           dimension == 64U;
}

static int prediction_geometry_supported(uint16_t width, uint16_t height) {
    return av2h_luma_prediction_dimension_supported(width) &&
           av2h_luma_prediction_dimension_supported(height);
}

/* Preserve direct-helper behavior outside the bitstream angle-delta range. */
static int legacy_angle_geometry_supported(Av2hIntraMode mode, uint16_t width, uint16_t height) {
    switch (mode) {
    case AV2H_INTRA_D113:
        return (width == 4U && height == 32U) ||
               ((width == 16U || width == 32U) &&
                (height == 8U || height == 16U || height == 32U || height == 64U)) ||
               (width == 64U && (height == 16U || height == 32U || height == 64U));
    case AV2H_INTRA_D135:
        return (width == 16U && height == 64U) || (width == 32U && height == 16U);
    case AV2H_INTRA_D157:
        return width == 16U && (height == 32U || height == 64U);
    case AV2H_INTRA_D203:
        return (width == 64U && height == 32U) || (width == 32U && height == 16U);
    default:
        return 0;
    }
}

Av2hStatus av2h_luma_predict_u16_adjusted(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                          const Av2hLumaReferencesU16 *references,
                                          uint16_t *predictor, size_t predictor_stride_samples,
                                          int angle_adjust, uint8_t disable_edge_filter,
                                          uint8_t use_idif, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;

    int angle;
    if (mode == NULL || references == NULL || predictor == NULL ||
        predictor_stride_samples < width || !prediction_geometry_supported(width, height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (mode->multi_line_mrl == 1U && mode->mrl_index != 0U && mode->mrl_index <= 3U) {
        return av2h_predict_multiline_u16(mode, width, height, references, predictor,
                                          predictor_stride_samples, use_idif, domain);
    }
    if ((mode->angle_delta != 0 && av2h_directional_base_angle(mode->mode) == 0) ||
        mode->mrl_index > 3U || mode->multi_line_mrl != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    angle = av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
                                     av2h_intra_mrl_angle_delta(mode->mrl_index) + angle_adjust,
                                 width, height);

    if (angle > 90 && angle < 180) {
        return av2h_predict_zone2_u16(mode, width, height, references, predictor,
                                      predictor_stride_samples, disable_edge_filter, use_idif,
                                      angle, domain);
    }
    if (angle > 0 && angle < 90 && mode->enable_ibp <= 1U) {
        return av2h_predict_zone1_u16(mode, width, height, references, predictor,
                                      predictor_stride_samples, disable_edge_filter, use_idif,
                                      angle, domain);
    }
    if (angle > 180 && angle < 270 && mode->enable_ibp <= 1U) {
        return av2h_predict_zone3_u16(mode, width, height, references, predictor,
                                      predictor_stride_samples, disable_edge_filter, use_idif,
                                      angle, domain);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL || mode->mode == AV2H_INTRA_HORIZONTAL) {
        return av2h_predict_cardinal_u16(mode, width, height, references, predictor,
                                         predictor_stride_samples, domain);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL || mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
        mode->mode == AV2H_INTRA_SMOOTH) {
        return av2h_predict_smooth_u16(mode, width, height, references, predictor,
                                       predictor_stride_samples, domain);
    }
    if (mode->mode == AV2H_INTRA_PAETH) {
        return av2h_predict_paeth_u16(width, height, references, predictor,
                                      predictor_stride_samples, domain);
    }
    return AV2H_UNSUPPORTED;
}

Av2hStatus av2h_chroma_predict_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                   const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                   size_t predictor_stride_samples,
                                   const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    return av2h_luma_predict_u16_adjusted(mode, width, height, references, predictor,
                                          predictor_stride_samples, 0, 0U, 0U, domain);
}

/* Compatibility for direct-helper fields outside canonical bitstream admission. */
static Av2hStatus predict_legacy_directional(const Av2hLeafMode *mode, uint16_t width,
                                             uint16_t height, const Av2hLumaReferences *references,
                                             uint8_t *predictor, size_t predictor_stride_samples) {
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 && mode->mrl_index == 3U &&
        mode->multi_line_mrl == 1U &&
        ((width == 8U && height == 4U) || (width == 32U && height == 16U))) {
        return av2h_predict_vertical_multiline(width, height, references, predictor,
                                               predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 && mode->mrl_index == 3U &&
        mode->multi_line_mrl == 0U &&
        ((width == 4U && height == 16U) || (width == 16U && height == 16U) ||
         (width == 32U && height == 16U))) {
        return av2h_predict_horizontal_mrl(width, height, references, predictor,
                                           predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == 0 &&
        ((width == 4U && height == 4U) || (width == 4U && height == 8U) ||
         (width == 8U && height == 16U) || (width == 16U && height == 8U) ||
         (width == 4U && height == 32U) || (width == 16U && height == 4U) ||
         (width == 32U && height == 4U))) {
        return av2h_predict_directional_idif(mode, 67, width, height, references, predictor,
                                             predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 3 && mode->mrl_index == 0U &&
        mode->multi_line_mrl == 0U && width == 64U && (height == 16U || height == 64U)) {
        return av2h_predict_directional_idif(mode, 212, width, height, references, predictor,
                                             predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 3 && mode->mrl_index == 0U &&
        mode->multi_line_mrl == 0U &&
        ((width == 4U && height == 4U) || (width == 8U && height == 4U) ||
         (width == 8U && height == 16U) || (width == 8U && height == 32U) ||
         (width == 16U && height == 8U) || (width == 32U && height == 4U))) {
        return av2h_predict_directional_idif(mode, 166, width, height, references, predictor,
                                             predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -2 && mode->mrl_index == 3U &&
        mode->multi_line_mrl == 0U &&
        ((width == 16U && height == 16U) || (width == 32U && height == 16U))) {
        return av2h_predict_directional_idif(mode, 174, width, height, references, predictor,
                                             predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -1 && mode->mrl_index == 0U &&
        mode->multi_line_mrl == 0U && width == 64U && height == 8U) {
        return av2h_predict_directional_idif(mode, 177, width, height, references, predictor,
                                             predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 && mode->mrl_index == 2U &&
        mode->multi_line_mrl == 1U && width == 8U && height == 16U) {
        return av2h_predict_horizontal_multiline(width, height, references, predictor,
                                                 predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->multi_line_mrl == 1U &&
        ((mode->angle_delta == -2 && ((mode->mrl_index == 2U && width == 16U && height == 8U) ||
                                      (mode->mrl_index == 3U && width == 32U && height == 16U))) ||
         (mode->angle_delta == 2 && ((mode->mrl_index == 3U && width == 8U && height == 8U) ||
                                     (mode->mrl_index == 1U && width == 32U && height == 16U))) ||
         (mode->angle_delta == -3 && mode->mrl_index == 3U && width == 8U && height == 16U))) {
        return av2h_predict_horizontal_multiline_angle(mode, width, height, references, predictor,
                                                       predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 && mode->multi_line_mrl == 0U &&
        (((mode->mrl_index == 0U) &&
          ((width == 4U && height == 4U) || (width == 8U && height == 4U) ||
           (width == 8U && height == 8U) || (width == 16U && height == 16U) ||
           (width == 16U && height == 32U) || (width == 32U && height == 8U) ||
           (width == 32U && height == 32U))) ||
         (mode->mrl_index == 2U && width == 8U && height == 8U) ||
         (mode->mrl_index == 3U && width == 4U && height == 4U))) {
        return av2h_predict_vertical_reference(width, height, references, predictor,
                                               predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 2 && mode->multi_line_mrl == 0U &&
        ((mode->mrl_index == 1U && width == 16U && height == 16U) ||
         (mode->mrl_index == 0U && width == 8U && height == 16U) ||
         (mode->enable_ibp == 1U && width == 64U && height == 64U))) {
        return av2h_predict_directional_idif(mode, 96, width, height, references, predictor,
                                             predictor_stride_samples);
    }
    if (mode->mrl_index != 0U || mode->multi_line_mrl != 0U) {
        if (mode->multi_line_mrl == 0U) {
            if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
                mode->mrl_index == 2U && width == 8U && height == 8U) {
                return av2h_predict_horizontal(width, height, references, predictor,
                                               predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 && mode->mrl_index == 1U &&
                mode->multi_line_mrl == 0U && width == 32U && height == 32U) {
                return av2h_predict_d45_mrl1(width, height, references, predictor,
                                             predictor_stride_samples);
            }
            if (((mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 0 && width == 16U &&
                  height == 32U) ||
                 (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 2 && width == 16U &&
                  height == 16U)) &&
                mode->mrl_index == 2U && mode->multi_line_mrl == 0U) {
                return av2h_predict_zone2_mrl2(mode, width, height, references, predictor,
                                               predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -1 && mode->mrl_index == 1U &&
                width == 8U && height == 8U) {
                return av2h_predict_d67_mrl1_minus_one(width, height, references, predictor,
                                                       predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 && mode->mrl_index == 1U &&
                width == 4U && height == 4U) {
                return av2h_predict_d67_4x4_mrl1_minus_two(width, height, references, predictor,
                                                           predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 && mode->mrl_index == 1U &&
                width == 8U && height == 8U) {
                return av2h_predict_d67_8x8_mrl1_minus_two(width, height, references, predictor,
                                                           predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -3 && mode->mrl_index == 3U &&
                width == 4U && height == 4U) {
                return av2h_predict_d67_4x4_mrl3_minus_three(width, height, references, predictor,
                                                             predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 && mode->mrl_index == 1U &&
                mode->multi_line_mrl == 0U && width == 8U && height == 16U) {
                return av2h_predict_d45_mrl1_plus_three(width, height, references, predictor,
                                                        predictor_stride_samples);
            }
            if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 3 && mode->mrl_index == 1U &&
                mode->multi_line_mrl == 0U && width == 8U && (height == 8U || height == 16U)) {
                return av2h_predict_d203_mrl1_plus_three(mode, width, height, references, predictor,
                                                         predictor_stride_samples);
            }
        }
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 && width <= 64U &&
        height <= 64U) {
        return av2h_predict_horizontal(width, height, references, predictor,
                                       predictor_stride_samples);
    }
    if (mode->enable_ibp == 1U && (mode->angle_delta < -3 || mode->angle_delta > 3) &&
        legacy_angle_geometry_supported(mode->mode, width, height)) {
        return av2h_predict_directional_idif(
            mode,
            av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3,
                                 width, height),
            width, height, references, predictor, predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -1 && mode->mrl_index == 0U &&
        ((width == 16U && height == 4U) || (width == 32U && height == 8U))) {
        return av2h_predict_d45_minus_one(width, height, references, predictor,
                                          predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 && width == 8U && height == 4U) {
        return av2h_predict_d45_plus_three(width, height, references, predictor,
                                           predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 3 && width == 8U && height == 4U) {
        return av2h_predict_d203_plus_three(width, height, references, predictor,
                                            predictor_stride_samples);
    }
    return av2h_predict_d203_fallback(mode, width, height, references, predictor,
                                      predictor_stride_samples);
}

Av2hStatus av2h_luma_predict(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                             const Av2hLumaReferences *references, uint8_t *predictor,
                             size_t predictor_stride_samples) {
    if (mode == NULL || references == NULL || predictor == NULL ||
        predictor_stride_samples < width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (mode->use_intra_dip != 0U) {
        return av2h_predict_intra_dip(mode, width, height, references, predictor,
                                      predictor_stride_samples);
    }
    int base_angle = av2h_directional_base_angle(mode->mode);
    if (base_angle != 0) {
        int plain = mode->angle_delta == 0 && mode->mrl_index == 0U && mode->multi_line_mrl == 0U;
        if (prediction_geometry_supported(width, height) &&
            (plain || (mode->angle_delta >= -3 && mode->angle_delta <= 3 &&
                       mode->mrl_index < AV2H_MRL_INDEX_SYMBOLS && mode->multi_line_mrl <= 1U &&
                       mode->enable_ibp <= 1U))) {
            return av2h_predict_luma_directional_feature(mode, width, height, references, predictor,
                                                         predictor_stride_samples);
        }
        return predict_legacy_directional(mode, width, height, references, predictor,
                                          predictor_stride_samples);
    }
    if (mode->angle_delta != 0 || mode->mrl_index != 0U || mode->multi_line_mrl != 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (prediction_geometry_supported(width, height)) {
        switch (mode->mode) {
        case AV2H_INTRA_DC:
            return av2h_predict_dc(mode, width, height, references, predictor,
                                   predictor_stride_samples);
        case AV2H_INTRA_SMOOTH:
            return av2h_predict_smooth(width, height, references, predictor,
                                       predictor_stride_samples);
        case AV2H_INTRA_SMOOTH_VERTICAL:
            return av2h_predict_smooth_vertical(width, height, references, predictor,
                                                predictor_stride_samples);
        case AV2H_INTRA_SMOOTH_HORIZONTAL:
            return av2h_predict_smooth_horizontal(width, height, references, predictor,
                                                  predictor_stride_samples);
        default:
            break;
        }
    }
    if (mode->mode == AV2H_INTRA_PAETH && width <= 64U && height <= 64U) {
        return av2h_predict_paeth(width, height, references, predictor, predictor_stride_samples);
    }
    return AV2H_UNSUPPORTED;
}

Av2hStatus av2h_luma_predict_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                 const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                 size_t predictor_stride_samples, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    return av2h_luma_predict_u16_adjusted(mode, width, height, references, predictor,
                                          predictor_stride_samples, 0, 0U, 1U, domain);
}
