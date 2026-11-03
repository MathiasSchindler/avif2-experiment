#include "internal/chroma_prediction.h"

#include "internal/intra_prediction.h"
#include "internal/sample_math.h"

#include <stddef.h>

static int wide_angle_map(int angle, uint16_t width, uint16_t height) {
    if ((height == 2U * width && angle < 61) || (height == 4U * width && angle < 73) ||
        (height == 8U * width && angle < 82) || (height == 16U * width && angle < 86)) {
        return angle + 180;
    }
    if ((width == 2U * height && angle > 209) || (width == 4U * height && angle > 197) ||
        (width == 8U * height && angle > 188) || (width == 16U * height && angle > 184)) {
        return angle - 180;
    }
    return angle;
}
static const uint8_t ibp_weights[5U][16U] = {
    {96U},
    {86U, 107U},
    {77U, 90U, 102U, 115U},
    {71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U},
    {68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U, 98U, 102U, 106U, 109U, 113U, 117U, 121U, 124U}};

static uint8_t smooth_scale(uint16_t width, uint16_t height) {
    unsigned int sum = (unsigned int)av2h_sample_block_log2(width) - 2U +
                       (unsigned int)av2h_sample_block_log2(height) - 2U;
    return (uint8_t)((sum + 2U) >> 2U);
}

static uint8_t smooth_blend(uint16_t position, uint8_t scale) {
    unsigned int shift = ((unsigned int)position << 1U) >> scale;
    return shift >= 6U ? 0U : (uint8_t)(64U >> shift);
}

/*
 * Intra bi-prediction (IBP) refinement of a DC predictor (enable_ibp=1).
 * The flat DC surface in `dst` is blended toward the reconstructed above and
 * left neighbours near the block edges, matching the AVM highbd_ibp_dc*
 * predictors (avm_dsp/intrapred.c).  Applied for every chroma transform
 * except TX_4X4, where IBP is disallowed.
 */
static const uint8_t ibp_size_to_weights_index[9U] = {0U, 1U, 2U, 0U, 3U, 0U, 0U, 0U, 4U};

uint8_t av2h_chroma_dc_predictor_average(unsigned int sum, unsigned int count, uint16_t width,
                                         uint16_t height, uint8_t have_top, uint8_t have_left) {
    int shift;
    uint16_t scale;
    (void)width;
    (void)height;
    (void)have_top;
    (void)have_left;
    if (count == 0U)
        return av2h_sample_midpoint_8bit();
    scale = av2h_sample_resolve_divisor_32(count, &shift);
    return (uint8_t)(((uint64_t)sum * scale + ((uint64_t)1U << (shift - 1))) >> shift);
}

static void apply_ibp_dc(uint8_t *dst, size_t stride_samples, uint16_t width, uint16_t height,
                         const uint8_t *above, const uint8_t *left) {
    uint16_t row;
    uint16_t column;
    if (above != NULL && left != NULL) {
        uint16_t len_h = (uint16_t)(height >> 2U);
        uint16_t len_w = (uint16_t)(width >> 2U);
        uint16_t row_start = width >= height ? len_h : 0U;
        uint16_t col_start = width >= height ? 0U : len_w;
        const uint8_t *weights = ibp_weights[ibp_size_to_weights_index[height >> 3U]];
        for (row = 0U; row < len_h; ++row) {
            for (column = col_start; column < width; ++column) {
                int32_t value = (above[column] * (128 - weights[row]) +
                                 dst[(size_t)row * stride_samples + column] * weights[row] + 64) >>
                                7U;
                dst[(size_t)row * stride_samples + column] = (uint8_t)value;
            }
        }
        weights = ibp_weights[ibp_size_to_weights_index[width >> 3U]];
        for (row = row_start; row < height; ++row) {
            for (column = 0U; column < len_w; ++column) {
                int32_t value =
                    (left[row] * (128 - weights[column]) +
                     dst[(size_t)row * stride_samples + column] * weights[column] + 64) >>
                    7U;
                dst[(size_t)row * stride_samples + column] = (uint8_t)value;
            }
        }
    } else if (above != NULL) {
        uint16_t len_h = (uint16_t)(height >> 2U);
        const uint8_t *weights = ibp_weights[ibp_size_to_weights_index[height >> 3U]];
        for (row = 0U; row < len_h; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t value = (above[column] * (128 - weights[row]) +
                                 dst[(size_t)row * stride_samples + column] * weights[row] + 64) >>
                                7U;
                dst[(size_t)row * stride_samples + column] = (uint8_t)value;
            }
        }
    } else if (left != NULL) {
        uint16_t len_w = (uint16_t)(width >> 2U);
        const uint8_t *weights = ibp_weights[ibp_size_to_weights_index[width >> 3U]];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < len_w; ++column) {
                int32_t value =
                    (left[row] * (128 - weights[column]) +
                     dst[(size_t)row * stride_samples + column] * weights[column] + 64) >>
                    7U;
                dst[(size_t)row * stride_samples + column] = (uint8_t)value;
            }
        }
    }
}

Av2hStatus av2h_chroma_predict(const Av2hChromaLeafMode *mode, uint16_t width, uint16_t height,
                               const Av2hLumaReferences *references, uint8_t *predictor,
                               size_t predictor_stride_samples) {
    uint16_t row;
    uint16_t column;
    if (mode == NULL || references == NULL || predictor == NULL ||
        predictor_stride_samples < width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((mode->angle_delta == -2 || mode->angle_delta == 2) &&
        av2h_directional_base_angle(mode->mode) != 0 &&
        av2h_luma_prediction_dimension_supported(width) &&
        av2h_luma_prediction_dimension_supported(height)) {
        Av2hLeafMode luma_mode;
        int angle = wide_angle_map(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3,
                                   width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.edge_ibp = (uint8_t)((width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return av2h_predict_directional(&luma_mode, angle, width, height, references, predictor,
                                        predictor_stride_samples, 0U, 1U);
    }
    if ((mode->angle_delta == -3 || mode->angle_delta == -1 || mode->angle_delta == 1 ||
         mode->angle_delta == 3) &&
        av2h_directional_base_angle(mode->mode) != 0 &&
        av2h_luma_prediction_dimension_supported(width) &&
        av2h_luma_prediction_dimension_supported(height)) {
        Av2hLeafMode luma_mode;
        int angle = wide_angle_map(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3,
                                   width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.edge_ibp = (uint8_t)((width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return av2h_predict_directional(&luma_mode, angle, width, height, references, predictor,
                                        predictor_stride_samples, 0U, 1U);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        av2h_luma_prediction_dimension_supported(width) &&
        av2h_luma_prediction_dimension_supported(height)) {
        Av2hLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        return av2h_luma_predict(&luma_mode, width, height, references, predictor,
                                 predictor_stride_samples);
    }
    if (mode->angle_delta == 0 && av2h_directional_base_angle(mode->mode) != 0 &&
        av2h_luma_prediction_dimension_supported(width) &&
        av2h_luma_prediction_dimension_supported(height)) {
        Av2hLeafMode luma_mode;
        int angle = wide_angle_map(av2h_directional_base_angle(mode->mode), width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.edge_ibp = (uint8_t)((width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return av2h_predict_directional(&luma_mode, angle, width, height, references, predictor,
                                        predictor_stride_samples, 0U, 1U);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL && mode->angle_delta == 0 &&
        av2h_luma_prediction_dimension_supported(width) &&
        av2h_luma_prediction_dimension_supported(height)) {
        Av2hLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        return av2h_luma_predict(&luma_mode, width, height, references, predictor,
                                 predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
        (mode->angle_delta == -3 || mode->angle_delta == -2) && width == 32U && height == 32U) {
        Av2hLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(&luma_mode, width, height, references, predictor,
                                 predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
        (mode->angle_delta == -3 || mode->angle_delta == -2) && width == 32U && height == 16U) {
        Av2hLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(&luma_mode, width, height, references, predictor,
                                 predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 1 && width == 16U &&
        height == 32U) {
        Av2hLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(&luma_mode, width, height, references, predictor,
                                 predictor_stride_samples);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 && width == 32U &&
        height == 16U) {
        if (references->above == NULL || references->above_count == 0U) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    references
                        ->above[column < references->above_count ? column
                                                                 : references->above_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 && width == 32U &&
        height == 32U) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    references->above != NULL && references->above_count != 0U
                        ? references->above[column < references->above_count
                                                ? column
                                                : references->above_count - 1U]
                        : av2h_sample_below_midpoint_8bit();
            }
        }
        return AV2H_OK;
    }
    if (!((width == 4U && height == 4U &&
           ((mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0) ||
            (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0) ||
            (mode->mode == AV2H_INTRA_D67 &&
             (mode->angle_delta == -1 || mode->angle_delta == -2)) ||
            (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0))) ||
          (width == 32U && (height == 16U || height == 32U) && mode->mode == AV2H_INTRA_D67 &&
           mode->angle_delta == 0) ||
          (width == 16U && (height == 4U || height == 16U) && mode->mode == AV2H_INTRA_D203 &&
           mode->angle_delta == 0) ||
          (width == 4U && height == 8U && mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) ||
          (mode->angle_delta == 0 &&
           ((mode->mode == AV2H_INTRA_DC && av2h_chroma_geometry_supported(width, height)) ||
            ((mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL || mode->mode == AV2H_INTRA_PAETH) &&
             av2h_luma_prediction_dimension_supported(width) &&
             av2h_luma_prediction_dimension_supported(height)))))) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL) {
        uint8_t fallback;
        if (references->left != NULL && references->left_count != 0U) {
            for (row = 0U; row < height; ++row) {
                size_t source = row < references->left_count ? row : references->left_count - 1U;
                uint8_t value = references->left[source];
                for (column = 0U; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride_samples + column] = value;
                }
            }
            return AV2H_OK;
        }
        /* No left neighbour: AV2 fills the left edge with the first above
         * sample when the block has an above neighbour, else the mid value. */
        fallback = (references->above != NULL && references->above_count != 0U)
                       ? references->above[0]
                       : av2h_sample_midpoint_8bit();
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] = fallback;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC) {
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t value;
        if (references->above != NULL) {
            for (column = 0U; column < width; ++column) {
                sum += references->above[column];
            }
            count += width;
        }
        if (references->left != NULL) {
            for (row = 0U; row < height; ++row) {
                sum += references->left[row];
            }
            count += height;
        }
        value = av2h_chroma_dc_predictor_average(
            sum, count, width, height, references->above != NULL, references->left != NULL);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] = value;
            }
        }
        if ((mode->ibp_configured == 0U || mode->edge_ibp != 0U) && (width != 4U || height != 4U)) {
            apply_ibp_dc(predictor, predictor_stride_samples, width, height, references->above,
                         references->left);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t top_right;
        if (references->above == NULL || references->above_count < width) {
            return AV2H_INVALID_ARGUMENT;
        }
        top_right = references->above_count > width ? references->above[width]
                                                    : references->above[width - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t left =
                    references->left == NULL ? references->above[0] : references->left[row];
                uint8_t blend = smooth_blend(column, scale);
                int32_t value = top_right + av2h_sample_round_pow2(((int64_t)left - top_right) *
                                                                       (width - 1U - column),
                                                                   av2h_sample_block_log2(width));
                value += av2h_sample_round_pow2(((int64_t)left - value) * blend, 7U);
                predictor[(size_t)row * predictor_stride_samples + column] = (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_PAETH) {
        if (references->above == NULL || references->above_count < width ||
            references->left == NULL || references->left_count < height ||
            !references->has_above_left) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t top = references->above[column];
                int32_t side = references->left[row];
                int32_t corner = references->above_left;
                int32_t base = top + side - corner;
                int32_t side_distance = base > side ? base - side : side - base;
                int32_t top_distance = base > top ? base - top : top - base;
                int32_t corner_distance = base > corner ? base - corner : corner - base;
                predictor[(size_t)row * predictor_stride_samples + column] =
                    (uint8_t)(side_distance <= top_distance && side_distance <= corner_distance
                                  ? side
                              : top_distance <= corner_distance ? top
                                                                : corner);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) {
        const uint16_t derivative = 93U;
        uint8_t edge[13U];
        uint8_t filtered[13U];
        size_t edge_index;
        if (references->left == NULL || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[0] = references->left[0];
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t source = edge_index - 1U;
            edge[edge_index] = source < references->left_count
                                   ? references->left[source]
                                   : references->left[references->left_count - 1U];
        }
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t previous = edge_index == 1U ? 0U : edge_index - 1U;
            size_t next = edge_index + 1U < sizeof(edge) ? edge_index + 1U : edge_index;
            int32_t value = edge[previous] + 2 * edge[edge_index] + edge[next];
            filtered[edge_index] = (uint8_t)((value + 2) >> 2U);
        }
        filtered[0] = edge[0];
        for (column = 0U; column < width; ++column) {
            uint16_t offset = (uint16_t)((column + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (row = 0U; row < height; ++row, ++base) {
                size_t first = base + 1U < sizeof(edge) ? base + 1U : sizeof(edge) - 1U;
                size_t second = first + 1U < sizeof(edge) ? first + 1U : sizeof(edge) - 1U;
                int32_t value = filtered[first] * (32 - shift) + filtered[second] * shift;
                predictor[(size_t)row * predictor_stride_samples + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45) {
        if (references->above == NULL || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t source = (size_t)row + column + 1U;
                if (source >= references->above_count) {
                    source = references->above_count - 1U;
                }
                predictor[(size_t)row * predictor_stride_samples + column] =
                    references->above[source];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67) {
        uint16_t derivative = mode->angle_delta == -1 ? 28U : mode->angle_delta == -2 ? 32U : 24U;
        if (references->above == NULL || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            uint16_t offset = (uint16_t)((row + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (column = 0U; column < width; ++column, ++base) {
                size_t first = base < references->above_count ? base : references->above_count - 1U;
                size_t second = first + 1U < references->above_count ? first + 1U
                                                                     : references->above_count - 1U;
                int32_t value =
                    references->above[first] * (32 - shift) + references->above[second] * shift;
                predictor[(size_t)row * predictor_stride_samples + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (references->left == NULL || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (column = 0U; column < width; ++column) {
        uint16_t offset = (uint16_t)((column + 1U) * 24U);
        uint16_t base = offset >> 6U;
        uint16_t shift = (offset & 63U) >> 1U;
        for (row = 0U; row < height; ++row, ++base) {
            size_t first = base < references->left_count ? base : references->left_count - 1U;
            size_t second =
                first + 1U < references->left_count ? first + 1U : references->left_count - 1U;
            int32_t value =
                references->left[first] * (32 - shift) + references->left[second] * shift;
            predictor[(size_t)row * predictor_stride_samples + column] =
                (uint8_t)((value + 16) >> 5U);
        }
    }
    return AV2H_OK;
}
