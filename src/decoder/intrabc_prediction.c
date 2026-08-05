#include "internal/reconstruct_modules.h"
#include "internal/decode_diagnostics.h"

static int32_t intrabc_clamp(int32_t value, int32_t minimum,
                             int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static uint8_t intrabc_morph_template_size(uint16_t size) {
    if (size < 3U) return 0U;
    if (size < 7U) return 4U;
    if (size < 13U) return 8U;
    return 16U;
}

static Av2hStatus apply_intrabc_morph_prediction(
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    int32_t source_x,
    int32_t source_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor) {
    uint8_t template_width = intrabc_morph_template_size(
        width < 16U ? width : 16U);
    uint8_t template_height = intrabc_morph_template_size(
        height < 16U ? height : 16U);
    uint8_t above_available = block_y != 0U;
    uint8_t left_available = block_x != 0U;
    uint8_t above_count = 0U;
    uint8_t left_count = 0U;
    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_xy = 0;
    int32_t sum_xx = 0;
    int32_t alpha;
    int32_t beta;
    uint8_t index;
    uint16_t row;
    uint16_t column;
    if (plane == 0 || predictor == 0 || source_x <= 0 || source_y <= 0) {
        return AV2H_INVALID_DATA;
    }
    if (above_available && left_available) {
        if (template_width == 16U && template_height == 16U) {
            above_count = 16U;
            left_count = 16U;
        } else if (template_width > 4U && template_height > 4U) {
            above_count = 8U;
            left_count = 8U;
        } else if (template_width < 16U && template_height < 16U) {
            above_count = 4U;
            left_count = 4U;
        } else if (template_width == 16U) {
            above_count = 16U;
        } else {
            left_count = 16U;
        }
    } else if (above_available) {
        above_count = template_width;
    } else if (left_available) {
        left_count = template_height;
    }
    for (index = 0U; index < above_count; ++index) {
        size_t step = template_width / above_count;
        size_t offset = (step == 1U ? 0U : step >> 1U) +
            (size_t)index * step;
        size_t current_x = (size_t)block_x + offset;
        size_t reference_x = (size_t)source_x + offset;
        uint8_t x;
        uint8_t y;
        if (current_x >= plane->width || reference_x >= plane->width ||
            !av2h_plane_sample_reconstructed(
                plane, current_x, block_y - 1U) ||
            !av2h_plane_sample_reconstructed(
                plane, reference_x, (size_t)source_y - 1U)) {
            return AV2H_UNSUPPORTED;
        }
        x = plane->data[((size_t)source_y - 1U) * plane->stride + reference_x];
        y = plane->data[((size_t)block_y - 1U) * plane->stride + current_x];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    for (index = 0U; index < left_count; ++index) {
        size_t step = template_height / left_count;
        size_t offset = (step == 1U ? 0U : step >> 1U) +
            (size_t)index * step;
        size_t current_y = (size_t)block_y + offset;
        size_t reference_y = (size_t)source_y + offset;
        uint8_t x;
        uint8_t y;
        if (current_y >= plane->height || reference_y >= plane->height ||
            !av2h_plane_sample_reconstructed(
                plane, block_x - 1U, current_y) ||
            !av2h_plane_sample_reconstructed(
                plane, (size_t)source_x - 1U, reference_y)) {
            return AV2H_UNSUPPORTED;
        }
        x = plane->data[reference_y * plane->stride + (size_t)source_x - 1U];
        y = plane->data[current_y * plane->stride + block_x - 1U];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    alpha = av2h_cfl_derive_alpha(
        sum_x, sum_y, sum_xx, sum_xy, above_count + left_count, 8);
    if (alpha == 0) alpha = 256;
    beta = ((sum_y << 8) - sum_x * alpha) /
        (int32_t)(above_count + left_count);
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int32_t value =
                (predictor[(size_t)row * width + column] * alpha + beta) >> 8;
            predictor[(size_t)row * width + column] =
                (uint8_t)intrabc_clamp(value, 0, 255);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_intrabc_predict(
    const Av2hFirstLeafMode *mode,
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor) {
    size_t row;
    size_t column;
    int32_t integer_x = mode->displacement_x / 8;
    int32_t integer_y = mode->displacement_y / 8;
    int32_t source_x;
    int32_t source_y;
    uint32_t fraction_x;
    uint32_t fraction_y;
    Av2hStatus status;
    av2h_reconstruction_diagnostics_set_luma_stage(10U);
    if (mode->displacement_x < 0 && mode->displacement_x % 8 != 0) {
        --integer_x;
    }
    if (mode->displacement_y < 0 && mode->displacement_y % 8 != 0) {
        --integer_y;
    }
    fraction_x = (uint32_t)(mode->displacement_x - integer_x * 8);
    fraction_y = (uint32_t)(mode->displacement_y - integer_y * 8);
    if ((fraction_x & 1U) != 0U || (fraction_y & 1U) != 0U) {
        return AV2H_UNSUPPORTED;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(11U);
    source_x = (int32_t)block_x + integer_x;
    source_y = (int32_t)block_y + integer_y;
    if (source_x < 0 || source_y < 0 ||
        (uint32_t)source_x + width + (fraction_x != 0U) > plane->width ||
        (uint32_t)source_y + height + (fraction_y != 0U) > plane->height) {
        return AV2H_INVALID_DATA;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(12U);
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t sample_x = (size_t)source_x + column;
            size_t sample_y = (size_t)source_y + row;
            uint32_t left_weight = 8U - fraction_x;
            uint32_t top_weight = 8U - fraction_y;
            uint32_t prediction;
            if (!av2h_plane_sample_reconstructed(plane, sample_x, sample_y) ||
                (fraction_x != 0U && !av2h_plane_sample_reconstructed(
                    plane, sample_x + 1U, sample_y)) ||
                (fraction_y != 0U && !av2h_plane_sample_reconstructed(
                    plane, sample_x, sample_y + 1U)) ||
                (fraction_x != 0U && fraction_y != 0U &&
                 !av2h_plane_sample_reconstructed(
                     plane, sample_x + 1U, sample_y + 1U))) {
                av2h_reconstruction_diagnostics_set_intrabc_source(
                    (uint16_t)sample_x, (uint16_t)sample_y,
                    (sample_y / 4U) * plane->reconstructed_stride +
                        sample_x / 4U);
                return AV2H_UNSUPPORTED;
            }
            prediction = plane->data[sample_y * plane->stride + sample_x] *
                left_weight * top_weight;
            if (fraction_x != 0U) {
                prediction += plane->data[
                    sample_y * plane->stride + sample_x + 1U] *
                    fraction_x * top_weight;
            }
            if (fraction_y != 0U) {
                prediction += plane->data[
                    (sample_y + 1U) * plane->stride + sample_x] *
                    left_weight * fraction_y;
            }
            if (fraction_x != 0U && fraction_y != 0U) {
                prediction += plane->data[
                    (sample_y + 1U) * plane->stride + sample_x + 1U] *
                    fraction_x * fraction_y;
            }
            predictor[row * width + column] =
                (uint16_t)((prediction + 32U) >> 6U);
        }
    }
    if (mode->morph_pred) {
        status = apply_intrabc_morph_prediction(
            plane, block_x, block_y, source_x, source_y,
            width, height, predictor);
        if (status != AV2H_OK) return status;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(13U);
    return AV2H_OK;
}