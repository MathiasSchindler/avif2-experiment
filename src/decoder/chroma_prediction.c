#include "reconstruct.h"
#include "internal/decode_diagnostics.h"
#include "internal/reconstruct_modules.h"
#include "internal/sample_math.h"

#define directional_base_angle av2h_directional_base_angle
#define luma_prediction_dimension_supported \
    av2h_luma_prediction_dimension_supported

static int wide_angle_map(int angle, uint16_t width, uint16_t height) {
    if ((height == 2U * width && angle < 61) ||
        (height == 4U * width && angle < 73) ||
        (height == 8U * width && angle < 82) ||
        (height == 16U * width && angle < 86)) {
        return angle + 180;
    }
    if ((width == 2U * height && angle > 209) ||
        (width == 4U * height && angle > 197) ||
        (width == 8U * height && angle > 188) ||
        (width == 16U * height && angle > 184)) {
        return angle - 180;
    }
    return angle;
}
static const uint8_t ibp_weights[5U][16U] = {
    { 96U },
    { 86U, 107U },
    { 77U, 90U, 102U, 115U },
    { 71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U },
    { 68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U,
      98U, 102U, 106U, 109U, 113U, 117U, 121U, 124U }
};

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    return av2h_sample_clamp(value, minimum, maximum);
}
static int32_t round_shift(int64_t value, uint8_t shift) {
    int64_t adjusted = value + ((int64_t)1 << (shift - 1U));
    if (adjusted >= 0) return (int32_t)(adjusted >> shift);
    return (int32_t)-(((-adjusted) + (((int64_t)1 << shift) - 1)) >> shift);
}

static uint8_t block_log2(uint16_t size) {
    return size == 4U ? 2U : size == 8U ? 3U :
        size == 16U ? 4U : size == 32U ? 5U : 6U;
}

static uint8_t smooth_scale(uint16_t width, uint16_t height) {
    unsigned int sum =
        (unsigned int)block_log2(width) - 2U +
        (unsigned int)block_log2(height) - 2U;
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
static const uint8_t ibp_size_to_weights_index[9U] = {
    0U, 1U, 2U, 0U, 3U, 0U, 0U, 0U, 4U
};

uint8_t av2h_chroma_dc_predictor_average(
    unsigned int sum, unsigned int count,
    uint16_t width, uint16_t height,
    uint8_t have_top, uint8_t have_left) {
    if (count == 0U) return 128U;
    if (have_top && have_left && width != height) {
        int shift;
        uint16_t scale = av2h_cfl_resolve_divisor_32(count, &shift);
        return (uint8_t)(((uint64_t)sum * scale +
                          ((uint64_t)1U << (shift - 1))) >> shift);
    }
    return (uint8_t)((sum + count / 2U) / count);
}

static void apply_ibp_dc(uint8_t *dst, size_t stride, uint16_t width,
                         uint16_t height, const uint8_t *above,
                         const uint8_t *left) {
    uint16_t row;
    uint16_t column;
    if (above != 0 && left != 0) {
        uint16_t len_h = (uint16_t)(height >> 2U);
        uint16_t len_w = (uint16_t)(width >> 2U);
        uint16_t row_start = width >= height ? len_h : 0U;
        uint16_t col_start = width >= height ? 0U : len_w;
        const uint8_t *weights =
            ibp_weights[ibp_size_to_weights_index[height >> 3U]];
        for (row = 0U; row < len_h; ++row) {
            for (column = col_start; column < width; ++column) {
                int32_t value = (above[column] * (128 - weights[row]) +
                    dst[(size_t)row * stride + column] * weights[row] + 64) >>
                    7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
        weights = ibp_weights[ibp_size_to_weights_index[width >> 3U]];
        for (row = row_start; row < height; ++row) {
            for (column = 0U; column < len_w; ++column) {
                int32_t value = (left[row] * (128 - weights[column]) +
                    dst[(size_t)row * stride + column] * weights[column] +
                    64) >> 7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
    } else if (above != 0) {
        uint16_t len_h = (uint16_t)(height >> 2U);
        const uint8_t *weights =
            ibp_weights[ibp_size_to_weights_index[height >> 3U]];
        for (row = 0U; row < len_h; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t value = (above[column] * (128 - weights[row]) +
                    dst[(size_t)row * stride + column] * weights[row] + 64) >>
                    7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
    } else if (left != 0) {
        uint16_t len_w = (uint16_t)(width >> 2U);
        const uint8_t *weights =
            ibp_weights[ibp_size_to_weights_index[width >> 3U]];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < len_w; ++column) {
                int32_t value = (left[row] * (128 - weights[column]) +
                    dst[(size_t)row * stride + column] * weights[column] +
                    64) >> 7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
    }
}

Av2hStatus av2h_chroma_predict(
    const Av2hFirstChromaMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    uint16_t row;
    uint16_t column;
    if (mode == 0 || references == 0 || predictor == 0 ||
        predictor_stride < width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((mode->angle_delta == -2 || mode->angle_delta == 2) &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        Av2hFirstLeafMode luma_mode;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.edge_ibp = (uint8_t)(
            (width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return av2h_predict_directional(
            &luma_mode, angle, width, height, references,
            predictor, predictor_stride, 0U, 1U);
    }
    if ((mode->angle_delta == -3 || mode->angle_delta == -1 ||
         mode->angle_delta == 1 || mode->angle_delta == 3) &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        Av2hFirstLeafMode luma_mode;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.edge_ibp = (uint8_t)(
            (width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return av2h_predict_directional(
            &luma_mode, angle, width, height, references,
            predictor, predictor_stride, 0U, 1U);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->angle_delta == 0 &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        Av2hFirstLeafMode luma_mode;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode), width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.edge_ibp = (uint8_t)(
            (width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return av2h_predict_directional(
            &luma_mode, angle, width, height, references,
            predictor, predictor_stride, 0U, 1U);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
        mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
        (mode->angle_delta == -3 || mode->angle_delta == -2) &&
        width == 32U && height == 32U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
        (mode->angle_delta == -3 || mode->angle_delta == -2) &&
        width == 32U && height == 16U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 1 &&
        width == 16U && height == 32U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        width == 32U && height == 16U) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[column < references->above_count ?
                        column : references->above_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        width == 32U && height == 32U) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    references->above != 0U &&
                    references->above_count != 0U ?
                        references->above[
                            column < references->above_count ? column :
                            references->above_count - 1U] : 127U;
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
                    (width == 32U &&
                     (height == 16U || height == 32U) &&
                     mode->mode == AV2H_INTRA_D67 &&
                     mode->angle_delta == 0) ||
                    (width == 16U &&
                     (height == 4U || height == 16U) &&
                     mode->mode == AV2H_INTRA_D203 &&
                     mode->angle_delta == 0) ||
                    (width == 4U && height == 8U &&
                     mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) ||
                    (mode->angle_delta == 0 &&
                                         ((mode->mode == AV2H_INTRA_DC &&
                                             av2h_chroma_geometry_supported(width, height)) ||
                                            ((mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
                                                mode->mode == AV2H_INTRA_PAETH) &&
                                             luma_prediction_dimension_supported(width) &&
                                                            luma_prediction_dimension_supported(height)))))) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL) {
        uint8_t fallback;
        if (references->left != 0 && references->left_count != 0U) {
            for (row = 0U; row < height; ++row) {
                size_t source = row < references->left_count ?
                    row : references->left_count - 1U;
                uint8_t value = references->left[source];
                for (column = 0U; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride + column] = value;
                }
            }
            return AV2H_OK;
        }
        /* No left neighbour: AV2 fills the left edge with the first above
         * sample when the block has an above neighbour, else the mid value. */
        fallback = (references->above != 0 && references->above_count != 0U) ?
            references->above[0] : 128U;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = fallback;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC) {
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t value;
        if (references->above != 0) {
            for (column = 0U; column < width; ++column) {
                sum += references->above[column];
            }
            count += width;
        }
        if (references->left != 0) {
            for (row = 0U; row < height; ++row) {
                sum += references->left[row];
            }
            count += height;
        }
        value = av2h_chroma_dc_predictor_average(
            sum, count, width, height,
            references->above != 0, references->left != 0);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        if (width != 4U || height != 4U) {
            apply_ibp_dc(predictor, predictor_stride, width, height,
                         references->above, references->left);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t top_right;
        if (references->above == 0 || references->above_count < width) {
            return AV2H_INVALID_ARGUMENT;
        }
        top_right = references->above_count > width ?
            references->above[width] : references->above[width - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t left = references->left == 0 ?
                    references->above[0] : references->left[row];
                uint8_t blend = smooth_blend(column, scale);
                int32_t value = top_right + round_shift(
                    ((int64_t)left - top_right) *
                        (width - 1U - column),
                    block_log2(width));
                value += round_shift(
                    ((int64_t)left - value) * blend, 7U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_PAETH) {
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height ||
            !references->has_above_left) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t top = references->above[column];
                int32_t side = references->left[row];
                int32_t corner = references->above_left;
                int32_t base = top + side - corner;
                int32_t side_distance =
                    base > side ? base - side : side - base;
                int32_t top_distance =
                    base > top ? base - top : top - base;
                int32_t corner_distance =
                    base > corner ? base - corner : corner - base;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)(side_distance <= top_distance &&
                                      side_distance <= corner_distance ?
                                  side :
                                  top_distance <= corner_distance ?
                                      top : corner);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) {
        const uint16_t derivative = 93U;
        uint8_t edge[13U];
        uint8_t filtered[13U];
        size_t edge_index;
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[0] = references->left[0];
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t source = edge_index - 1U;
            edge[edge_index] = source < references->left_count ?
                references->left[source] :
                references->left[references->left_count - 1U];
        }
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t previous = edge_index == 1U ? 0U : edge_index - 1U;
            size_t next = edge_index + 1U < sizeof(edge) ?
                edge_index + 1U : edge_index;
            int32_t value = edge[previous] + 2 * edge[edge_index] +
                edge[next];
            filtered[edge_index] = (uint8_t)((value + 2) >> 2U);
        }
        filtered[0] = edge[0];
        for (column = 0U; column < width; ++column) {
            uint16_t offset = (uint16_t)((column + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (row = 0U; row < height; ++row, ++base) {
                size_t first = base + 1U < sizeof(edge) ?
                    base + 1U : sizeof(edge) - 1U;
                size_t second = first + 1U < sizeof(edge) ?
                    first + 1U : sizeof(edge) - 1U;
                int32_t value = filtered[first] * (32 - shift) +
                    filtered[second] * shift;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t source = (size_t)row + column + 1U;
                if (source >= references->above_count) {
                    source = references->above_count - 1U;
                }
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[source];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67) {
        uint16_t derivative = mode->angle_delta == -1 ? 28U :
            mode->angle_delta == -2 ? 32U : 24U;
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            uint16_t offset = (uint16_t)((row + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (column = 0U; column < width; ++column, ++base) {
                size_t first = base < references->above_count ?
                    base : references->above_count - 1U;
                size_t second = first + 1U < references->above_count ?
                    first + 1U : references->above_count - 1U;
                int32_t value = references->above[first] * (32 - shift) +
                    references->above[second] * shift;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (references->left == 0 || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (column = 0U; column < width; ++column) {
        uint16_t offset = (uint16_t)((column + 1U) * 24U);
        uint16_t base = offset >> 6U;
        uint16_t shift = (offset & 63U) >> 1U;
        for (row = 0U; row < height; ++row, ++base) {
            size_t first = base < references->left_count ?
                base : references->left_count - 1U;
            size_t second = first + 1U < references->left_count ?
                first + 1U : references->left_count - 1U;
            int32_t value = references->left[first] * (32 - shift) +
                references->left[second] * shift;
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)((value + 16) >> 5U);
        }
    }
    return AV2H_OK;
}

/*
 * CfL scaling and downsampling for derived and explicit alpha prediction.
 * The fixed-point derivation and all three 4:2:0 downsampling filters match
 * the pinned AVM reference (av2/common/cfl.c, warped_motion.h divisor LUT).
 */
#define AV2H_CFL_ADD_BITS_ALPHA 5
#define AV2H_CFL_NUM_REF_SAM 8
#define AV2H_DIV_LUT_BITS 7
#define AV2H_DIV_LUT_PREC_BITS 9

static const uint16_t cfl_div_lut[129U] = {
    512, 508, 504, 500, 496, 493, 489, 485, 482, 478, 475, 471, 468, 465, 462,
    458, 455, 452, 449, 446, 443, 440, 437, 434, 431, 428, 426, 423, 420, 417,
    415, 412, 410, 407, 405, 402, 400, 397, 395, 392, 390, 388, 386, 383, 381,
    379, 377, 374, 372, 370, 368, 366, 364, 362, 360, 358, 356, 354, 352, 350,
    349, 347, 345, 343, 341, 340, 338, 336, 334, 333, 331, 329, 328, 326, 324,
    323, 321, 320, 318, 317, 315, 314, 312, 311, 309, 308, 306, 305, 303, 302,
    301, 299, 298, 297, 295, 294, 293, 291, 290, 289, 287, 286, 285, 284, 282,
    281, 280, 279, 278, 277, 275, 274, 273, 272, 271, 270, 269, 267, 266, 265,
    264, 263, 262, 261, 260, 259, 258, 257, 256
};

static int cfl_get_msb(uint32_t value) {
    int log = 0;
    while (value > 1U) {
        value >>= 1U;
        ++log;
    }
    return log;
}

static int32_t cfl_round_pow2(int64_t value, int shift) {
    return (int32_t)((value + (((int64_t)1 << shift) >> 1)) >> shift);
}

static int32_t cfl_round_pow2_signed(int64_t value, int shift) {
    return value < 0 ? -cfl_round_pow2(-value, shift) :
        cfl_round_pow2(value, shift);
}

uint16_t av2h_cfl_resolve_divisor_32(uint32_t d, int *shift) {
    int32_t f;
    int s = cfl_get_msb(d);
    int32_t e = (int32_t)(d - ((uint32_t)1 << s));
    if (s > AV2H_DIV_LUT_BITS) {
        f = cfl_round_pow2(e, s - AV2H_DIV_LUT_BITS);
    } else {
        f = e << (AV2H_DIV_LUT_BITS - s);
    }
    *shift = s + AV2H_DIV_LUT_PREC_BITS;
    return cfl_div_lut[f];
}

static int32_t cfl_resolve_divisor_cfl(int32_t n, int32_t d, int shift) {
    int sign_n = n < 0;
    int32_t f_n;
    int32_t f_d;
    int shift_n;
    int shift_d;
    int shift_add;
    int32_t ret;
    if (sign_n) n = -n;
    if (n == 0 || d == 0) return 0;
    shift_n = cfl_get_msb((uint32_t)n);
    shift_d = cfl_get_msb((uint32_t)d);
    {
        int32_t e_d = (int32_t)(d - ((uint32_t)1 << shift_d));
        if (shift_d > AV2H_DIV_LUT_BITS) {
            f_d = cfl_round_pow2(e_d, shift_d - AV2H_DIV_LUT_BITS);
        } else {
            f_d = e_d << (AV2H_DIV_LUT_BITS - shift_d);
        }
    }
    if (shift_n > AV2H_DIV_LUT_BITS) {
        f_n = cfl_round_pow2(n, shift_n - AV2H_DIV_LUT_BITS);
    } else {
        f_n = n << (AV2H_DIV_LUT_BITS - shift_n);
    }
    shift_add = shift_d - shift_n - shift;
    if (shift_add <= 1) {
        int shift0 = AV2H_DIV_LUT_PREC_BITS + AV2H_DIV_LUT_BITS + shift_add;
        ret = shift0 >= 0 ? (cfl_div_lut[f_d] * f_n) >> shift0 :
            (2 << shift) - 1;
    } else {
        ret = 0;
    }
    if (ret >= (2 << shift) - 1) ret = (2 << shift) - 1;
    return sign_n ? -ret : ret;
}

int32_t av2h_cfl_derive_alpha(int32_t sum_x, int32_t sum_y, int32_t sum_xx,
                              int32_t sum_xy, int count, int shift) {
    int32_t der;
    int32_t nor;
    if (count == 0) return 0;
    der = sum_xx - (int32_t)((int64_t)sum_x * sum_x / count);
    nor = sum_xy - (int32_t)((int64_t)sum_x * sum_y / count);
    if (der == 0 || nor == 0) return 0;
    return cfl_resolve_divisor_cfl(nor, der, shift);
}

/*
 * Verify a rectangular region of the plane is marked reconstructed at 4x4
 * granularity (the readiness map resolution).
 */
static int plane_region_reconstructed(const Av2hReconstructionPlane *plane,
                                      uint16_t x, uint16_t y,
                                      uint16_t width, uint16_t height) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < height; row += 4U) {
        for (column = 0U; column < width; column += 4U) {
                if (!av2h_plane_sample_reconstructed(
                    plane, (size_t)x + column, (size_t)y + row)) {
                return 0;
            }
        }
    }
    return 1;
}

static uint8_t plane_clamped_sample(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    if (x >= plane->reference_width) x = plane->reference_width - 1U;
    if (y >= plane->reference_height) y = plane->reference_height - 1U;
    return plane->data[y * plane->stride + x];
}

static uint8_t plane_coded_sample(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    if (x >= plane->width) x = plane->width - 1U;
    if (y >= plane->height) y = plane->height - 1U;
    return plane->data[y * plane->stride + x];
}

static uint16_t mhccp_luma_filter2_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y);

static uint16_t cfl_luma_filter2_current_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y,
    uint16_t block_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = chroma_x == block_x ? center_x : center_x - 1U;
    size_t top_y = chroma_y == block_y ? center_y : center_y - 1U;
    return (uint16_t)(
        plane_coded_sample(luma_plane, left_x, center_y) +
        4U * plane_coded_sample(luma_plane, center_x, center_y) +
        plane_coded_sample(luma_plane, center_x + 1U, center_y) +
        plane_coded_sample(luma_plane, center_x, top_y) +
        plane_coded_sample(luma_plane, center_x, center_y + 1U));
}

/*
 * Derive the CFL_DERIVED_ALPHA predictor for one chroma transform block.
 * `predictor` enters holding the DC base prediction (built from the chroma
 * neighbours) and leaves holding predictor + scaled luma-AC.  Fails closed
 * (leaving the DC base untouched) whenever the required reconstructed luma is
 * unavailable or the geometry is unsupported.
 */
Av2hStatus av2h_cfl_derived_predict(
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t filter_index,
    uint8_t derive_alpha,
    int32_t supplied_alpha_q3,
    uint16_t superblock_size,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint8_t *predictor,
    size_t predictor_stride) {
    const int bit_depth = 8;
    uint16_t luma_x = (uint16_t)((uint32_t)block_x * 2U);
    uint16_t luma_y = (uint16_t)((uint32_t)block_y * 2U);
    uint16_t luma_width = (uint16_t)((uint32_t)width * 2U);
    uint16_t luma_height = (uint16_t)((uint32_t)height * 2U);
    int32_t luma_left_ds[64];
    int32_t luma_above_ds[64];
    int32_t chroma_left[64];
    int32_t chroma_above[64];
    uint8_t is_top_sb_boundary;
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
    if (luma_plane == 0 || luma_plane->data == 0 ||
        filter_index > 2U ||
        width > 64U ||
        height > 64U || width == 0U || height == 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (luma_x >= luma_plane->width ||
        luma_y >= luma_plane->height ||
        luma_plane->stride < luma_plane->width || luma_plane->reconstructed == 0) {
        return AV2H_UNSUPPORTED;
    }
    effective_luma_width = (uint16_t)(
        (luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_luma_height = (uint16_t)(
        (luma_plane->reference_height + 7U) & ~(size_t)7U);
    effective_chroma_width = (uint16_t)(
        (chroma_plane->reference_width + 3U) & ~(size_t)3U);
    effective_chroma_height = (uint16_t)(
        (chroma_plane->reference_height + 3U) & ~(size_t)3U);
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
    visible_luma_width = luma_x >= effective_luma_width ? 0U :
        luma_width < effective_luma_width - luma_x ?
            luma_width : (uint16_t)(effective_luma_width - luma_x);
    visible_luma_height = luma_y >= effective_luma_height ? 0U :
        luma_height < effective_luma_height - luma_y ?
            luma_height : (uint16_t)(effective_luma_height - luma_y);
    if (!plane_region_reconstructed(luma_plane, luma_x, luma_y,
                                    visible_luma_width,
                                    visible_luma_height)) {
        return AV2H_UNSUPPORTED;
    }
    /* Left neighbour luma (box downsampled) and chroma. */
    if (have_left) {
        uint16_t reference_x = filter_index == 0U ?
            (uint16_t)(luma_x - 2U) : (uint16_t)(luma_x - 3U);
        uint16_t reference_width = filter_index == 0U ? 2U : 3U;
        if (luma_x < (filter_index == 0U ? 2U : 3U) ||
            !plane_region_reconstructed(luma_plane, reference_x, luma_y,
                                        reference_width,
                                        visible_luma_height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                luma_left_ds[row] = (int32_t)(
                    plane_coded_sample(luma_plane, center_x - 1U, center_y) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, center_y) +
                    plane_coded_sample(luma_plane, center_x + 1U, center_y) +
                    plane_coded_sample(
                        luma_plane, center_x - 1U, center_y + 1U) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, center_y + 1U) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, center_y + 1U));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                size_t top_y = row == 0U ? center_y : center_y - 1U;
                luma_left_ds[row] = (int32_t)(
                    plane_coded_sample(
                        luma_plane, center_x - 1U, center_y) +
                    4U * plane_coded_sample(
                        luma_plane, center_x, center_y) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, center_y) +
                    plane_coded_sample(luma_plane, center_x, top_y) +
                    plane_coded_sample(
                        luma_plane, center_x, center_y + 1U));
            } else {
                size_t r0 = (size_t)luma_y + (size_t)row * 2U;
                const uint8_t *base = &luma_plane->data[
                    r0 * luma_plane->stride + luma_x - 2U];
                luma_left_ds[row] = (int32_t)(base[0] + base[1] +
                    base[luma_plane->stride] +
                    base[luma_plane->stride + 1U]) << 1;
            }
            chroma_left[row] = plane_coded_sample(
                chroma_plane, block_x - 1U, (size_t)block_y + row);
            if (luma_y >= effective_luma_height) {
                luma_left_ds[row] = 128 << 3;
            } else if (row >= (visible_luma_height >> 1U) && row != 0U) {
                luma_left_ds[row] = luma_left_ds[row - 1U];
            }
            if (block_y >= effective_chroma_height) {
                chroma_left[row] = 128;
            } else if (row >= effective_chroma_height - block_y && row != 0U) {
                chroma_left[row] = chroma_left[row - 1U];
            }
        }
    }
    /* Above neighbour luma (box downsampled) and chroma. */
    if (have_top) {
        is_top_sb_boundary = (uint8_t)(superblock_size != 0U &&
            (luma_y % superblock_size) == 0U);
        top_offset = 2 - is_top_sb_boundary;
        bottom_offset = 1 - is_top_sb_boundary;
        if (luma_y < (uint16_t)top_offset ||
            !plane_region_reconstructed(
                luma_plane, luma_x, (uint16_t)(luma_y - top_offset),
                luma_width < luma_plane->width - luma_x ?
                    luma_width :
                    (uint16_t)(luma_plane->width - luma_x),
                4U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] = (int32_t)(
                    plane_coded_sample(luma_plane, left_x, top_row) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, top_row) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, top_row) +
                    plane_coded_sample(luma_plane, left_x, bottom_row) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, bottom_row) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, bottom_row));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t center_y =
                    (size_t)luma_y - (size_t)top_offset;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_y = is_top_sb_boundary ?
                    center_y : center_y - 1U;
                size_t bottom_y = center_y + (size_t)bottom_offset;
                luma_above_ds[column] = (int32_t)(
                    luma_plane->data[center_y * luma_plane->stride + left_x] +
                    4U * luma_plane->data[center_y * luma_plane->stride +
                                          center_x] +
                    luma_plane->data[center_y * luma_plane->stride +
                                     center_x + 1U] +
                    luma_plane->data[top_y * luma_plane->stride + center_x] +
                    luma_plane->data[bottom_y * luma_plane->stride +
                                     center_x]);
            } else {
                size_t c0 = (size_t)luma_x + (size_t)column * 2U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] = (int32_t)(
                    plane_clamped_sample(luma_plane, c0, top_row) +
                    plane_clamped_sample(luma_plane, c0 + 1U, top_row) +
                    plane_clamped_sample(luma_plane, c0, bottom_row) +
                    plane_clamped_sample(
                        luma_plane, c0 + 1U, bottom_row)) << 1;
            }
            chroma_above[column] = plane_coded_sample(
                chroma_plane, (size_t)block_x + column,
                (size_t)block_y - 1U);
            if (luma_x >= effective_luma_width) {
                luma_above_ds[column] = 128 << 3;
            } else if (column >= (visible_luma_width >> 1U) && column != 0U) {
                luma_above_ds[column] = luma_above_ds[column - 1U];
            }
            if (block_x >= effective_chroma_width) {
                chroma_above[column] = 128;
            } else if (column >= effective_chroma_width - block_x &&
                       column != 0U) {
                chroma_above[column] = chroma_above[column - 1U];
            }
        }
    }
    /* Neighbour luma DC (avg_l) subtracted from current-block luma. */
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
            uint16_t scale = av2h_cfl_resolve_divisor_32(
                (uint32_t)dc_count, &div_shift);
            int32_t rounding = (1 << div_shift) >> 1;
            int32_t val = (int32_t)(((int64_t)sum * scale + rounding) >>
                                    div_shift);
            int32_t max_v = (1 << (bit_depth + 3)) - 1;
            avg_l = val > max_v ? max_v : val;
        } else {
            avg_l = 8 << (bit_depth - 1);
        }
    }
    /* Distribute reference samples between the available sides. */
    numb_up = 0;
    numb_left = 0;
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
    if (numb_up > width) numb_up = width;
    if (numb_left > height) numb_left = height;
    count = 0;
    sum_x = 0;
    sum_y = 0;
    sum_xx = 0;
    sum_xy = 0;
    if (numb_up > 0) {
        int step = (int)width / numb_up;
        int start;
        int i;
        if (step < 1) step = 1;
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)width; i += step) {
            int32_t lx = luma_above_ds[i] >> 3;
            int32_t cy = chroma_above[i];
            sum_x += lx;
            sum_y += cy;
            sum_xy += lx * cy;
            sum_xx += lx * lx;
            ++count;
        }
    }
    if (numb_left > 0) {
        int step = (int)height / numb_left;
        int start;
        int i;
        if (step < 1) step = 1;
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)height; i += step) {
            int32_t lx = luma_left_ds[i] >> 3;
            int32_t cy = chroma_left[i];
            sum_x += lx;
            sum_y += cy;
            sum_xy += lx * cy;
            sum_xx += lx * lx;
            ++count;
        }
    }
    shift = 3 + AV2H_CFL_ADD_BITS_ALPHA;
    alpha = derive_alpha ?
        av2h_cfl_derive_alpha(
            sum_x, sum_y, sum_xx, sum_xy, count, shift) :
        supplied_alpha_q3 * (1 << AV2H_CFL_ADD_BITS_ALPHA);
    /* Predict: DC base + scaled luma AC. */
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int32_t recon_q3;
            if (filter_index == 1U) {
                size_t sample_x =
                    (size_t)luma_x + (size_t)column * 2U;
                size_t sample_y =
                    (size_t)luma_y + (size_t)row * 2U;
                size_t left_x = column == 0U ? sample_x : sample_x - 1U;
                recon_q3 = (int32_t)(
                    plane_coded_sample(luma_plane, left_x, sample_y) +
                    2U * plane_coded_sample(
                        luma_plane, sample_x, sample_y) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y) +
                    plane_coded_sample(
                        luma_plane, left_x, sample_y + 1U) +
                    2U * plane_coded_sample(
                        luma_plane, sample_x, sample_y + 1U) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y + 1U));
            } else if (filter_index == 2U) {
                recon_q3 = cfl_luma_filter2_current_q3(
                    luma_plane, (uint16_t)(block_x + column),
                    (uint16_t)(block_y + row), block_x, block_y);
            } else {
                size_t sample_x =
                    (size_t)luma_x + (size_t)column * 2U;
                size_t sample_y =
                    (size_t)luma_y + (size_t)row * 2U;
                recon_q3 = (int32_t)(
                    plane_coded_sample(luma_plane, sample_x, sample_y) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y) +
                    plane_coded_sample(
                        luma_plane, sample_x, sample_y + 1U) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y + 1U)) << 1;
            }
            int32_t ac = recon_q3 - avg_l;
            int32_t scaled = cfl_round_pow2_signed(
                (int64_t)alpha * ac, 6 + AV2H_CFL_ADD_BITS_ALPHA);
            int32_t base_dc = predictor[(size_t)row * predictor_stride + column];
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(scaled + base_dc, 0, 255);
        }
    }
    return AV2H_OK;
}

#define AV2H_MHCCP_DECIM_BITS 16
#define AV2H_MHCCP_DIV_PREC_BITS 14
#define AV2H_MHCCP_DIV_SLOT_BITS 3

static int mhccp_log2(uint32_t value) {
    int result = 0;
    while (value > 1U) {
        value >>= 1U;
        ++result;
    }
    return result;
}

static int32_t mhccp_mul_fixed(int32_t left, int32_t right, int shift) {
    uint32_t abs_left = (uint32_t)(left < 0 ? -left : left);
    uint32_t abs_right = (uint32_t)(right < 0 ? -right : right);
    int left_bits = mhccp_log2(abs_left) + 1;
    int right_bits = mhccp_log2(abs_right) + 1;
    int drop = left_bits + right_bits - 29;
    int left_drop;
    int right_drop;
    int adjusted_shift;
    int32_t product;
    uint32_t bias;
    if (drop < 0) drop = 0;
    left_drop = drop >> 1;
    right_drop = drop - left_drop;
    adjusted_shift = shift - left_drop - right_drop;
    product = (left_drop != 0 ? left >> left_drop : left) *
              (right_drop != 0 ? right >> right_drop : right);
    if (adjusted_shift <= 0) return product;
    if (adjusted_shift > 29) return 0;
    bias = (uint32_t)1U << (adjusted_shift - 1);
    return product >= 0 ?
        (int32_t)(((uint32_t)product + bias) >> adjusted_shift) :
        -(int32_t)(((uint32_t)(-product) + bias) >> adjusted_shift);
}

static void mhccp_division_scale(uint32_t denominator, int32_t *scale,
                                  int *shift) {
    static const int32_t square_weight[8U] = {
        214, 153, 113, 86, 67, 53, 43, 35
    };
    static const int32_t offset[8U] = {
        4822, 5952, 6624, 6792, 6408, 5424, 3792, 1466
    };
    static const int32_t bias[8U] = {
        12784, 12054, 11670, 11583, 11764, 12195, 12870, 13782
    };
    int delta;
    int32_t normalized;
    int32_t fraction;
    int32_t centered;
    int32_t index;
    *shift = mhccp_log2(denominator);
    delta = *shift - AV2H_MHCCP_DIV_PREC_BITS;
    if (delta >= 0) {
        int32_t rounding = delta > 0 ?
            (int32_t)((uint32_t)1U << (delta - 1)) : 0;
        normalized = ((int32_t)denominator + rounding) >> delta;
    } else {
        normalized = (int32_t)(denominator << -delta);
    }
    if (normalized < 1) normalized = 1;
    if (normalized > (1 << (AV2H_MHCCP_DIV_PREC_BITS + 1)) - 1) {
        normalized = (1 << (AV2H_MHCCP_DIV_PREC_BITS + 1)) - 1;
    }
    fraction = normalized & ((1 << AV2H_MHCCP_DIV_PREC_BITS) - 1);
    index = fraction >>
        (AV2H_MHCCP_DIV_PREC_BITS - AV2H_MHCCP_DIV_SLOT_BITS);
    centered = fraction - offset[index];
    *scale = ((square_weight[index] *
               ((centered * centered) >> AV2H_MHCCP_DIV_PREC_BITS)) >> 8) -
             (centered >> 1) + bias[index];
    *scale <<= AV2H_MHCCP_DECIM_BITS - AV2H_MHCCP_DIV_PREC_BITS;
}

static void mhccp_solve(int32_t matrix[3U][3U], int32_t target[3U],
                         int32_t result[3U]) {
    int32_t work[3U][4U] = { { 0 } };
    int row;
    for (row = 0; row < 3; ++row) {
        int column;
        for (column = 0; column < 3; ++column) {
            work[row][column] = column >= row ?
                matrix[row][column] : matrix[column][row];
        }
        work[row][row] += 2;
        work[row][3] = target[row];
    }
    for (row = 0; row < 3; ++row) {
        uint32_t diagonal = (uint32_t)(work[row][row] < 0 ?
            -work[row][row] : work[row][row]);
        int32_t scale;
        int shift;
        int lower_row;
        if (diagonal < 1U) diagonal = 1U;
        mhccp_division_scale(diagonal, &scale, &shift);
        for (lower_row = row + 1; lower_row < 4; ++lower_row) {
            work[row][lower_row] = mhccp_mul_fixed(
                work[row][lower_row], scale, shift);
        }
        for (lower_row = row + 1; lower_row < 3; ++lower_row) {
            int32_t factor = work[lower_row][row];
            int column;
            for (column = row + 1; column < 4; ++column) {
                work[lower_row][column] -= mhccp_mul_fixed(
                    factor, work[row][column], AV2H_MHCCP_DECIM_BITS);
            }
        }
    }
    result[2] = work[2][3];
    for (row = 1; row >= 0; --row) {
        int column;
        result[row] = work[row][3];
        for (column = row + 1; column < 3; ++column) {
            result[row] -= mhccp_mul_fixed(
                work[row][column], result[column],
                AV2H_MHCCP_DECIM_BITS);
        }
    }
}

static uint16_t mhccp_luma_q3(const Av2hReconstructionPlane *luma_plane,
                               uint16_t chroma_x, uint16_t chroma_y) {
    size_t x0 = (size_t)chroma_x * 2U;
    size_t y0 = (size_t)chroma_y * 2U;
    size_t x1 = x0 + 1U < luma_plane->width ? x0 + 1U : x0;
    size_t y1 = y0 + 1U < luma_plane->height ? y0 + 1U : y0;
    return (uint16_t)((luma_plane->data[y0 * luma_plane->stride + x0] +
        luma_plane->data[y0 * luma_plane->stride + x1] +
        luma_plane->data[y1 * luma_plane->stride + x0] +
        luma_plane->data[y1 * luma_plane->stride + x1]) << 1U);
}

static uint16_t mhccp_luma_filter2_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        4U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[top_y * luma_plane->stride + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride + center_x]);
}

static uint16_t mhccp_luma_filter2_top_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        5U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[bottom_y * luma_plane->stride + center_x]);
}

static uint16_t mhccp_luma_filter2_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        6U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x]);
}

static uint16_t mhccp_luma_filter1_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        2U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[bottom_y * luma_plane->stride + left_x] +
        2U * luma_plane->data[bottom_y * luma_plane->stride + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride + right_x]);
}

static uint16_t mhccp_luma_left_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    if (filter_index == 0U) {
        return mhccp_luma_q3(luma_plane, chroma_x, chroma_y);
    }
    if (filter_index == 1U) {
        return (uint16_t)(
            3U * luma_plane->data[
                center_y * luma_plane->stride + center_x] +
            luma_plane->data[center_y * luma_plane->stride + right_x] +
            3U * luma_plane->data[
                bottom_y * luma_plane->stride + center_x] +
            luma_plane->data[bottom_y * luma_plane->stride + right_x]);
    }
    return (uint16_t)(
        5U * luma_plane->data[
            center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[top_y * luma_plane->stride + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride + center_x]);
}

static uint16_t mhccp_luma_filter1_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(2U * (
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        2U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x]));
}

static uint16_t mhccp_luma_filter0_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(4U * (
        luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x]));
}

static uint16_t mhccp_luma_sb_top_left_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    uint16_t center = luma_plane->data[
        center_y * luma_plane->stride + center_x];
    uint16_t right = luma_plane->data[
        center_y * luma_plane->stride + right_x];
    if (filter_index == 0U) return (uint16_t)(4U * (center + right));
    if (filter_index == 1U) return (uint16_t)(2U * (3U * center + right));
    return (uint16_t)(7U * center + right);
}

static uint16_t mhccp_luma_sb_top_sample_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y) {
    return filter_index == 0U ?
        mhccp_luma_filter0_sb_top_q3(luma_plane, chroma_x, block_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_sb_top_q3(luma_plane, chroma_x, block_y) :
        mhccp_luma_filter2_sb_top_q3(luma_plane, chroma_x, block_y);
}

static uint16_t mhccp_luma_sample_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    return filter_index == 0U ?
        mhccp_luma_q3(luma_plane, chroma_x, chroma_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_q3(luma_plane, chroma_x, chroma_y) :
        mhccp_luma_filter2_q3(luma_plane, chroma_x, chroma_y);
}

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static Av2hPredictionDiagnostics prediction_diagnostics;
#endif

const Av2hPredictionDiagnostics *av2h_prediction_diagnostics_get(void) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return &prediction_diagnostics;
#else
    return 0;
#endif
}

void av2h_prediction_diagnostics_set_mhccp_reason(uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    prediction_diagnostics.mhccp_reason = value;
#else
    (void)value;
#endif
}
int av2h_chroma_geometry_supported(uint16_t width, uint16_t height);

static Av2hStatus mhccp_predict_direction01(
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t filter_index,
    uint8_t direction,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint16_t superblock_size,
    uint8_t *predictor,
    size_t predictor_stride) {
    int32_t matrix[3U][3U] = { { 0 } };
    int32_t target[3U] = { 0 };
    int32_t parameters[3U];
    uint16_t sample;
    uint16_t row;
    uint16_t column;
    uint16_t count = 0U;
    uint16_t top_count;
    uint16_t left_count;
    uint16_t top_start_x;
    uint16_t top_chroma_width;
    uint16_t top_luma_width;
    uint16_t prediction_width;
    uint16_t prediction_height;
    int32_t top_left_center = 0;
    uint8_t have_top;
    uint8_t have_left;
    uint8_t is_top_sb_boundary;
    int matrix_shift;
    uint32_t count_ceiling;
    av2h_prediction_diagnostics_set_mhccp_reason(0U);
    have_top = (uint8_t)(block_y >= 2U);
    have_left = (uint8_t)(block_x >= 2U);
    is_top_sb_boundary = (uint8_t)(superblock_size != 0U &&
        ((uint32_t)block_y * 2U) % superblock_size == 0U);
    if (filter_index > 2U || direction > 2U ||
        width == 0U || height == 0U ||
                block_x >= chroma_plane->reference_width ||
                block_y >= chroma_plane->height ||
        (have_left &&
                 (block_x - 1U >= chroma_plane->reference_width ||
                    block_x * 2U - 2U >= luma_plane->reference_width))) {
                av2h_prediction_diagnostics_set_mhccp_reason(6U);
        return AV2H_UNSUPPORTED;
    }
    prediction_width = width < chroma_plane->width - block_x ?
        width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y ?
        height : (uint16_t)(chroma_plane->height - block_y);
    if (!have_top && !have_left) {
        if (!plane_region_reconstructed(
                luma_plane, (uint16_t)(block_x * 2U),
                (uint16_t)(block_y * 2U),
                (uint16_t)(prediction_width * 2U),
                (uint16_t)(prediction_height * 2U))) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < prediction_height; ++row) {
            for (column = 0U; column < prediction_width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = 128U;
            }
        }
        return AV2H_OK;
    }
    top_count = have_top ? (uint16_t)(width * 2U) : 0U;
    if (!have_left && top_count >= 2U) top_count = (uint16_t)(top_count - 2U);
    if (is_top_sb_boundary && top_count > 62U) top_count = 62U;
    left_count = have_left ? (uint16_t)(height * 2U - 1U) : 0U;
    if (left_count >= chroma_plane->height - block_y) {
        left_count = (uint16_t)(chroma_plane->height - block_y - 1U);
    }
    if (have_left) {
        uint16_t available_left = 0U;
         while (available_left < left_count &&
             av2h_plane_sample_reconstructed(
                   chroma_plane, (size_t)block_x - 1U,
                   (size_t)block_y + available_left)) {
            ++available_left;
        }
        if (available_left > height - 1U) {
            available_left = (uint16_t)(height - 1U +
                ((available_left - (height - 1U)) & ~1U));
        }
        left_count = available_left;
        have_left = (uint8_t)(available_left != 0U);
    }
    top_start_x = (uint16_t)(have_left ? block_x - 1U : block_x + 1U);
    top_chroma_width = 0U;
    top_luma_width = 0U;
    if (have_top) {
        uint16_t available_top = 0U;
        uint16_t effective_chroma_width = (uint16_t)(
            (chroma_plane->reference_width + 3U) & ~(size_t)3U);
        uint16_t effective_luma_width = (uint16_t)(
            (luma_plane->reference_width + 7U) & ~(size_t)7U);
        if (effective_chroma_width > chroma_plane->width) {
            effective_chroma_width = chroma_plane->width;
        }
        if (effective_luma_width > luma_plane->width) {
            effective_luma_width = luma_plane->width;
        }
        top_chroma_width = effective_chroma_width > top_start_x + 1U ?
            (uint16_t)(effective_chroma_width - top_start_x - 1U) : 0U;
        if (top_chroma_width > top_count) top_chroma_width = top_count;
        top_luma_width = effective_luma_width > top_start_x * 2U ?
            (uint16_t)(effective_luma_width - top_start_x * 2U) : 0U;
        if (top_luma_width > top_count * 2U) {
            top_luma_width = (uint16_t)(top_count * 2U);
        }
        if (top_count > top_chroma_width) top_count = top_chroma_width;
        if (top_count > (top_luma_width + 1U) / 2U) {
            top_count = (uint16_t)((top_luma_width + 1U) / 2U);
        }
         while (available_top < top_count &&
             av2h_plane_sample_reconstructed(
                   chroma_plane, (size_t)top_start_x + available_top,
                   (size_t)block_y - 1U)) {
            ++available_top;
        }
        if (!have_left && available_top != 0U && available_top <= width) {
            --available_top;
        }
        if (available_top > width) {
            available_top = (uint16_t)(width +
                ((available_top - width) & ~1U));
        }
        top_count = available_top;
        top_chroma_width = available_top;
        top_luma_width = (uint16_t)(available_top * 2U);
        have_top = (uint8_t)(available_top != 0U);
    }
    if (have_top &&
        (top_chroma_width == 0U || top_luma_width == 0U ||
         !plane_region_reconstructed(
             chroma_plane, top_start_x, (uint16_t)(block_y - 1U),
             top_chroma_width, 1U))) {
        av2h_prediction_diagnostics_set_mhccp_reason(1U);
        return AV2H_UNSUPPORTED;
    }
    if (have_top && !plane_region_reconstructed(
            luma_plane, (uint16_t)(top_start_x * 2U),
            (uint16_t)(block_y * 2U - 2U), top_luma_width, 2U)) {
        av2h_prediction_diagnostics_set_mhccp_reason(2U);
        return AV2H_UNSUPPORTED;
    }
    if (have_left && !plane_region_reconstructed(
            chroma_plane, (uint16_t)(block_x - 1U), block_y, 1U,
            left_count)) {
        av2h_prediction_diagnostics_set_mhccp_reason(3U);
        return AV2H_UNSUPPORTED;
    }
    if (have_left && !plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * 2U - 2U),
            (uint16_t)(block_y * 2U), 2U,
            (uint16_t)(left_count * 2U))) {
        av2h_prediction_diagnostics_set_mhccp_reason(4U);
        return AV2H_UNSUPPORTED;
    }
    if (!plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * 2U),
            (uint16_t)(block_y * 2U),
            (uint16_t)(prediction_width * 2U),
            (uint16_t)(prediction_height * 2U))) {
        av2h_prediction_diagnostics_set_mhccp_reason(5U);
        return AV2H_UNSUPPORTED;
    }
    if (direction == 1U && have_top &&
        !plane_region_reconstructed(
            luma_plane, (uint16_t)(top_start_x * 2U),
            (uint16_t)(block_y * 2U - 4U),
            top_luma_width, 2U)) {
        av2h_prediction_diagnostics_set_mhccp_reason(8U);
        return AV2H_UNSUPPORTED;
    }
    for (sample = 0U; sample < top_count; ++sample) {
        uint16_t luma_chroma_x = (uint16_t)(top_start_x + sample);
        uint16_t chroma_x = luma_chroma_x;
        uint16_t chroma_y = (uint16_t)(block_y - 1U);
        int32_t vector[3U];
        int first;
        int second;
        {
            int32_t center = (int32_t)((is_top_sb_boundary ?
                    mhccp_luma_sb_top_sample_q3(
                        luma_plane, filter_index, luma_chroma_x, block_y) :
                    mhccp_luma_sample_q3(
                        luma_plane, filter_index, luma_chroma_x,
                        chroma_y)) >> 3U);
            if (sample == 0U) top_left_center = center;
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (is_top_sb_boundary ? center :
                    (int32_t)((filter_index == 0U ?
                        mhccp_luma_q3(
                            luma_plane, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U)) :
                     filter_index == 1U ?
                        mhccp_luma_filter1_q3(
                            luma_plane, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U)) :
                        mhccp_luma_filter2_top_clamped_q3(
                            luma_plane, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U))) >> 3U)) :
                    (int32_t)((is_top_sb_boundary ?
                        (sample == 0U && have_left ?
                            mhccp_luma_sb_top_left_clamped_q3(
                                luma_plane, filter_index,
                                luma_chroma_x == 0U ? 0U :
                                    (uint16_t)(luma_chroma_x - 1U), block_y) :
                            mhccp_luma_sb_top_sample_q3(
                                luma_plane, filter_index,
                                luma_chroma_x == 0U ? 0U :
                                    (uint16_t)(luma_chroma_x - 1U), block_y)) :
                        sample == 0U && have_left ?
                            mhccp_luma_left_clamped_q3(
                                luma_plane, filter_index,
                                (uint16_t)(luma_chroma_x - 1U), chroma_y) :
                            mhccp_luma_sample_q3(
                                luma_plane, filter_index,
                                luma_chroma_x == 0U ? 0U :
                                    (uint16_t)(luma_chroma_x - 1U),
                                chroma_y)) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
        }
        vector[2] = 128;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    for (sample = have_top ? 0U : 1U; sample < left_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(block_x - 1U);
        uint16_t chroma_y = (uint16_t)(block_y + sample);
        int32_t vector[3U];
        int first;
        int second;
        {
            int32_t center = (int32_t)(mhccp_luma_sample_q3(
                luma_plane, filter_index, chroma_x, chroma_y) >> 3U);
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (sample == 0U && have_top ? top_left_center :
                     (int32_t)(mhccp_luma_sample_q3(
                         luma_plane, filter_index, chroma_x,
                         (uint16_t)(chroma_y - 1U)) >> 3U)) :
                    (int32_t)(mhccp_luma_left_clamped_q3(
                        luma_plane, filter_index,
                        (uint16_t)(chroma_x - 1U), chroma_y) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
        }
        vector[2] = 128;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    count_ceiling = count - 1U;
    matrix_shift = 0;
    while (count_ceiling != 0U) {
        ++matrix_shift;
        count_ceiling >>= 1U;
    }
    matrix_shift = 6 - matrix_shift;
    for (row = 0U; row < 3U; ++row) {
        for (column = row; column < 3U; ++column) {
            matrix[row][column] = matrix_shift > 0 ?
                matrix[row][column] << matrix_shift :
                matrix[row][column] >> -matrix_shift;
        }
        target[row] = matrix_shift > 0 ?
            target[row] << matrix_shift :
            target[row] >> -matrix_shift;
    }
    mhccp_solve(matrix, target, parameters);
    for (row = 0U; row < prediction_height; ++row) {
        for (column = 0U; column < prediction_width; ++column) {
            int32_t center = (int32_t)(mhccp_luma_sample_q3(
                luma_plane, filter_index,
                (uint16_t)(block_x + column),
                (uint16_t)(block_y + row)) >> 3U);
            int32_t vector[3U];
            int32_t sum = 0;
            int parameter;
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (row == 0U && !have_top ? center :
                     row == 0U && is_top_sb_boundary ?
                        (int32_t)(mhccp_luma_sb_top_sample_q3(
                            luma_plane, filter_index,
                            (uint16_t)(block_x + column), block_y) >> 3U) :
                        (int32_t)(mhccp_luma_sample_q3(
                            luma_plane, filter_index,
                            (uint16_t)(block_x + column),
                            (uint16_t)(block_y + row - 1U)) >> 3U)) :
                    (int32_t)(mhccp_luma_sample_q3(
                        luma_plane, filter_index,
                        column == 0U && !have_left ? block_x :
                            (uint16_t)(block_x + column - 1U),
                        (uint16_t)(block_y + row)) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
            vector[2] = 128;
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(
                    parameters[parameter], vector[parameter],
                    AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(sum, 0, 255);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_mhccp_predict_observed(
    const Av2hFirstChromaMode *mode,
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor,
    size_t predictor_stride) {
    int32_t matrix[3U][3U] = { { 0 } };
    int32_t target[3U] = { 0 };
    int32_t parameters[3U];
    uint16_t prediction_width;
    uint16_t prediction_height;
    uint16_t row;
    if (mode->mhccp_direction <= 2U &&
        av2h_chroma_geometry_supported(width, height) &&
        luma_plane != 0 && luma_plane->data != 0 &&
        luma_plane->reconstructed != 0 &&
        luma_plane->stride >= luma_plane->width) {
        return mhccp_predict_direction01(
            luma_plane, chroma_plane,
            mode->cfl_downsample_filter_index,
            mode->mhccp_direction, block_x, block_y,
            width, height, mode->superblock_size,
            predictor, predictor_stride);
    }
    if (mode->mhccp_direction != 2U || block_y != 0U || block_x < 2U ||
        width != 16U || height != 32U || luma_plane == 0 ||
        luma_plane->data == 0 || luma_plane->reconstructed == 0 ||
        luma_plane->stride < luma_plane->width ||
        (size_t)block_x * 2U >= luma_plane->width ||
        (size_t)(block_y + height) * 2U > luma_plane->height ||
        !plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * 2U - 4U), 0U,
            (uint16_t)(luma_plane->width - (block_x * 2U - 4U)),
            (uint16_t)(height * 2U)) ||
        !plane_region_reconstructed(
            chroma_plane, (uint16_t)(block_x - 2U), 0U, 2U, height)) {
        return AV2H_UNSUPPORTED;
    }
    prediction_width = width < chroma_plane->width - block_x ?
        width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y ?
        height : (uint16_t)(chroma_plane->height - block_y);
    if ((size_t)(block_x + prediction_width) * 2U > luma_plane->width ||
        (size_t)(block_y + prediction_height) * 2U > luma_plane->height) {
        return AV2H_UNSUPPORTED;
    }
    for (row = 1U; row + 1U < height; ++row) {
        int32_t vector[3U];
        uint16_t center = (uint16_t)(
            mhccp_luma_q3(luma_plane, (uint16_t)(block_x - 1U), row) >> 3U);
        int first;
        int second;
        vector[0] = (int32_t)(
            mhccp_luma_q3(luma_plane, (uint16_t)(block_x - 2U), row) >> 3U);
        vector[1] = (center * center + 128) >> 8U;
        vector[2] = 128;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)row * chroma_plane->stride + block_x - 1U];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
    }
    for (row = 0U; row < 3U; ++row) {
        uint16_t column;
        target[row] <<= 1U;
        for (column = row; column < 3U; ++column) {
            matrix[row][column] <<= 1U;
        }
    }
    mhccp_solve(matrix, target, parameters);
    for (row = 0U; row < prediction_height; ++row) {
        uint16_t column;
        for (column = 0U; column < prediction_width; ++column) {
            int32_t vector[3U];
            uint16_t center = (uint16_t)(mhccp_luma_q3(
                luma_plane, (uint16_t)(block_x + column), row) >> 3U);
            int32_t sum = 0;
            int parameter;
            vector[0] = (int32_t)(mhccp_luma_q3(
                luma_plane, (uint16_t)(block_x + column - 1U), row) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
            vector[2] = 128;
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(
                    parameters[parameter], vector[parameter],
                    AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(sum, 0, 255);
        }
    }
    return AV2H_OK;
}

int av2h_chroma_geometry_supported(uint16_t width, uint16_t height) {
    return (width == 4U && (height == 4U || height == 8U ||
             height == 16U || height == 32U || height == 64U)) ||
           (width == 8U &&
                        (height == 4U || height == 8U || height == 16U ||
             height == 32U || height == 64U)) ||
           (width == 16U &&
                        (height == 4U || height == 8U || height == 16U ||
             height == 32U || height == 64U)) ||
       (width == 32U &&
            (height == 4U || height == 8U || height == 16U ||
             height == 32U || height == 64U)) ||
       (width == 64U &&
            (height == 4U || height == 8U || height == 16U ||
             height == 32U || height == 64U));
}
