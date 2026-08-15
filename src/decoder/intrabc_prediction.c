#include "internal/reconstruct_modules.h"
#include "internal/decode_diagnostics.h"
#include "internal/sample_math.h"

/*
 * IntraBC displacement vectors are coded in eighth-pel luma units.  The
 * normative predictor converts them to sixteenth-pel plane units, splits the
 * result into an integer reference position and a subpel phase, and runs the
 * two tap "intrabc" bilinear kernel separably with the regular inter-prediction
 * rounding schedule.  Doing the interpolation in one fused step would round
 * differently, so the stages are reproduced exactly here.
 */
#define AV2H_INTRABC_FILTER_BITS 7
#define AV2H_INTRABC_ROUND0_BITS 3

int32_t av2h_intrabc_plane_position(
    uint16_t block_position, int32_t displacement, uint8_t subsampling) {
    return (int32_t)((uint32_t)block_position << AV2H_SUBPEL_BITS) +
        (int32_t)((uint32_t)displacement << (1U - subsampling));
}

int32_t av2h_intrabc_integer_position(int32_t position) {
    return position >= 0 ? (position >> AV2H_SUBPEL_BITS) :
        -(((-position) + ((1 << AV2H_SUBPEL_BITS) - 1)) >> AV2H_SUBPEL_BITS);
}

static int32_t intrabc_round_shift(int32_t value, uint8_t bits) {
    if (bits == 0U) return value;
    return (value + (1 << (bits - 1U))) >> bits;
}

static void intrabc_filter_taps(uint32_t phase, int32_t taps[2]) {
    taps[1] = (int32_t)(phase * 8U);
    taps[0] = 128 - taps[1];
}

static uint32_t intrabc_load(
    const uint8_t *source8,
    const uint16_t *source16,
    size_t stride,
    size_t x,
    size_t y) {
    return source8 != 0 ? source8[y * stride + x] : source16[y * stride + x];
}

static void intrabc_store(
    uint8_t *destination8,
    uint16_t *destination16,
    size_t index,
    int32_t value,
    uint8_t bit_depth) {
    int32_t maximum = (int32_t)((1U << bit_depth) - 1U);
    int32_t clipped = av2h_sample_clamp(value, 0, maximum);
    if (destination8 != 0) {
        destination8[index] = (uint8_t)clipped;
    } else {
        destination16[index] = (uint16_t)clipped;
    }
}

Av2hStatus av2h_intrabc_convolve(
    const uint8_t *source8,
    const uint16_t *source16,
    size_t source_stride,
    int32_t source_x,
    int32_t source_y,
    uint32_t subpel_x,
    uint32_t subpel_y,
    uint16_t width,
    uint16_t height,
    uint8_t bit_depth,
    uint8_t *destination8,
    uint16_t *destination16) {
    int32_t horizontal[2];
    int32_t vertical[2];
    int32_t upper[AV2H_INTRABC_MAX_BLOCK];
    int32_t lower[AV2H_INTRABC_MAX_BLOCK];
    uint8_t round_0 = AV2H_INTRABC_ROUND0_BITS;
    uint8_t round_1 = (uint8_t)(2 * AV2H_INTRABC_FILTER_BITS -
        AV2H_INTRABC_ROUND0_BITS);
    int32_t range = (int32_t)bit_depth + AV2H_INTRABC_FILTER_BITS -
        AV2H_INTRABC_ROUND0_BITS + 2;
    size_t row;
    size_t column;
    if ((source8 == 0) == (source16 == 0) ||
        (destination8 == 0) == (destination16 == 0) ||
        width == 0U || height == 0U || width > AV2H_INTRABC_MAX_BLOCK ||
        source_x < 0 || source_y < 0 || bit_depth < 8U || bit_depth > 12U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (range > 16) {
        round_0 = (uint8_t)(round_0 + (range - 16));
        round_1 = (uint8_t)(round_1 - (range - 16));
    }
    intrabc_filter_taps(subpel_x, horizontal);
    intrabc_filter_taps(subpel_y, vertical);
    if (subpel_x == 0U && subpel_y == 0U) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                intrabc_store(
                    destination8, destination16, row * width + column,
                    (int32_t)intrabc_load(
                        source8, source16, source_stride,
                        (size_t)source_x + column, (size_t)source_y + row),
                    bit_depth);
            }
        }
        return AV2H_OK;
    }
    if (subpel_y == 0U) {
        uint8_t bits = (uint8_t)(AV2H_INTRABC_FILTER_BITS - round_0);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t x = (size_t)source_x + column;
                size_t y = (size_t)source_y + row;
                int32_t sum =
                    horizontal[0] * (int32_t)intrabc_load(
                        source8, source16, source_stride, x, y) +
                    horizontal[1] * (int32_t)intrabc_load(
                        source8, source16, source_stride, x + 1U, y);
                intrabc_store(
                    destination8, destination16, row * width + column,
                    intrabc_round_shift(
                        intrabc_round_shift(sum, round_0), bits),
                    bit_depth);
            }
        }
        return AV2H_OK;
    }
    if (subpel_x == 0U) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t x = (size_t)source_x + column;
                size_t y = (size_t)source_y + row;
                int32_t sum =
                    vertical[0] * (int32_t)intrabc_load(
                        source8, source16, source_stride, x, y) +
                    vertical[1] * (int32_t)intrabc_load(
                        source8, source16, source_stride, x, y + 1U);
                intrabc_store(
                    destination8, destination16, row * width + column,
                    intrabc_round_shift(
                        sum, (uint8_t)AV2H_INTRABC_FILTER_BITS),
                    bit_depth);
            }
        }
        return AV2H_OK;
    }
    {
        int32_t horizontal_offset =
            1 << ((int32_t)bit_depth + AV2H_INTRABC_FILTER_BITS - 1);
        int32_t offset_bits = (int32_t)bit_depth +
            2 * AV2H_INTRABC_FILTER_BITS - round_0;
        int32_t vertical_offset = 1 << offset_bits;
        int32_t bias = (1 << (offset_bits - round_1)) +
            (1 << (offset_bits - round_1 - 1));
        uint8_t bits = (uint8_t)(2 * AV2H_INTRABC_FILTER_BITS -
            round_0 - round_1);
        int32_t *previous = upper;
        int32_t *next = lower;
        for (column = 0U; column < width; ++column) {
            size_t x = (size_t)source_x + column;
            size_t y = (size_t)source_y;
            previous[column] = intrabc_round_shift(
                horizontal_offset +
                    horizontal[0] * (int32_t)intrabc_load(
                        source8, source16, source_stride, x, y) +
                    horizontal[1] * (int32_t)intrabc_load(
                        source8, source16, source_stride, x + 1U, y),
                round_0);
        }
        for (row = 0U; row < height; ++row) {
            int32_t *swap;
            for (column = 0U; column < width; ++column) {
                size_t x = (size_t)source_x + column;
                size_t y = (size_t)source_y + row + 1U;
                next[column] = intrabc_round_shift(
                    horizontal_offset +
                        horizontal[0] * (int32_t)intrabc_load(
                            source8, source16, source_stride, x, y) +
                        horizontal[1] * (int32_t)intrabc_load(
                            source8, source16, source_stride, x + 1U, y),
                    round_0);
            }
            for (column = 0U; column < width; ++column) {
                int32_t sum = vertical_offset +
                    vertical[0] * previous[column] +
                    vertical[1] * next[column];
                int32_t value =
                    intrabc_round_shift(sum, round_1) - bias;
                intrabc_store(
                    destination8, destination16, row * width + column,
                    intrabc_round_shift(value, bits), bit_depth);
            }
            swap = previous;
            previous = next;
            next = swap;
        }
    }
    return AV2H_OK;
}

/* AVM rounds the displacement to full pel with GET_MV_RAWPEL. */
int32_t av2h_intrabc_full_pel(int16_t displacement) {
    int32_t value = displacement;
    return (value + 3 + (value >= 0 ? 1 : 0)) >> 3;
}

#define AV2H_BAWP_MAX_REF 16U
#define AV2H_BAWP_SHIFT 8

static const uint8_t av2h_bawp_size_log2[AV2H_BAWP_MAX_REF + 1U] = {
    0U, 0U, 0U, 2U, 2U, 2U, 2U, 3U, 3U, 3U, 3U, 3U, 3U, 4U, 4U, 4U, 4U
};

static const uint8_t av2h_bawp_log_to_size[5] = { 0U, 2U, 4U, 8U, 16U };

static void intrabc_morph_sample_counts(
    uint8_t above_available,
    uint8_t left_available,
    uint8_t width,
    uint8_t height,
    uint8_t *above_count,
    uint8_t *left_count) {
    above_available = (uint8_t)(width != 0U ? above_available : 0U);
    left_available = (uint8_t)(height != 0U ? left_available : 0U);
    *above_count = 0U;
    *left_count = 0U;
    if (above_available && left_available) {
        if (width == 16U && height == 16U) {
            *above_count = 16U;
            *left_count = 16U;
        } else if (width > 4U && height > 4U) {
            *above_count = 8U;
            *left_count = 8U;
        } else if (width < 16U && height < 16U) {
            *above_count = 4U;
            *left_count = 4U;
        } else if (width == 16U) {
            *above_count = 16U;
        } else {
            *left_count = 16U;
        }
    } else if (above_available) {
        *above_count = width;
    } else if (left_available) {
        *left_count = height;
    }
}

static int32_t intrabc_morph_sample(
    const uint8_t *data8, const uint16_t *data16, size_t index) {
    return data8 != 0 ? (int32_t)data8[index] : (int32_t)data16[index];
}

/*
 * Morph prediction (BAWP) refines the IntraBC predictor with a least squares
 * linear model fitted between the row above / column left of the coding block
 * and the matching lines next to the full-pel reference block.  The model is
 * derived once per coding block, so the coding block geometry is used even
 * when the predictor is produced one transform block at a time.
 */
static Av2hStatus intrabc_morph_parameters(
    const uint8_t *data8,
    const uint16_t *data16,
    size_t stride,
    uint32_t plane_width,
    uint32_t plane_height,
    uint16_t coding_x,
    uint16_t coding_y,
    uint16_t coding_width,
    uint16_t coding_height,
    int32_t reference_x,
    int32_t reference_y,
    int32_t *alpha_out,
    int32_t *beta_out) {
    uint16_t reference_width = coding_width;
    uint16_t reference_height = coding_height;
    uint8_t clamped_width;
    uint8_t clamped_height;
    uint8_t padded_width;
    uint8_t padded_height;
    uint8_t above_count;
    uint8_t left_count;
    int32_t reference_pad[AV2H_BAWP_MAX_REF];
    int32_t recon_pad[AV2H_BAWP_MAX_REF];
    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_xy = 0;
    int32_t sum_xx = 0;
    int32_t count = 0;
    size_t index;
    if (coding_x >= plane_width || coding_y >= plane_height) {
        return AV2H_INVALID_DATA;
    }
    if (coding_width > plane_width - coding_x) {
        reference_width = (uint16_t)(plane_width - coding_x);
    }
    if (coding_height > plane_height - coding_y) {
        reference_height = (uint16_t)(plane_height - coding_y);
    }
    clamped_width = (uint8_t)(reference_width < AV2H_BAWP_MAX_REF ?
        reference_width : AV2H_BAWP_MAX_REF);
    clamped_height = (uint8_t)(reference_height < AV2H_BAWP_MAX_REF ?
        reference_height : AV2H_BAWP_MAX_REF);
    padded_width = av2h_bawp_log_to_size[av2h_bawp_size_log2[clamped_width]];
    padded_height = av2h_bawp_log_to_size[av2h_bawp_size_log2[clamped_height]];
    intrabc_morph_sample_counts(
        (uint8_t)(coding_y != 0U), (uint8_t)(coding_x != 0U),
        padded_width, padded_height, &above_count, &left_count);
    if (above_count != 0U) {
        uint8_t step = (uint8_t)(padded_width / above_count);
        uint8_t start = (uint8_t)(step == 1U ? 0U : step >> 1U);
        uint8_t delta = (uint8_t)(padded_width - clamped_width);
        uint8_t position;
        if (reference_y <= 0 || reference_x < 0 ||
            (uint32_t)reference_x >= plane_width ||
            clamped_width > plane_width - (uint32_t)reference_x) {
            return AV2H_INVALID_DATA;
        }
        for (index = 0U; index < clamped_width; ++index) {
            reference_pad[index] = intrabc_morph_sample(data8, data16,
                ((size_t)reference_y - 1U) * stride +
                (size_t)reference_x + index);
            recon_pad[index] = intrabc_morph_sample(data8, data16,
                ((size_t)coding_y - 1U) * stride + (size_t)coding_x + index);
        }
        for (index = 0U; index < delta; ++index) {
            reference_pad[index + clamped_width] = reference_pad[index];
            recon_pad[index + clamped_width] = recon_pad[index];
        }
        for (position = start; position < padded_width;
             position = (uint8_t)(position + step)) {
            int32_t x = reference_pad[position];
            int32_t y = recon_pad[position];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }
        count += above_count;
    }
    if (left_count != 0U) {
        uint8_t step = (uint8_t)(padded_height / left_count);
        uint8_t start = (uint8_t)(step == 1U ? 0U : step >> 1U);
        uint8_t delta = (uint8_t)(padded_height - clamped_height);
        uint8_t position;
        if (reference_x <= 0 || reference_y < 0 ||
            (uint32_t)reference_y >= plane_height ||
            clamped_height > plane_height - (uint32_t)reference_y) {
            return AV2H_INVALID_DATA;
        }
        for (index = 0U; index < clamped_height; ++index) {
            reference_pad[index] = intrabc_morph_sample(data8, data16,
                ((size_t)reference_y + index) * stride +
                (size_t)reference_x - 1U);
            recon_pad[index] = intrabc_morph_sample(data8, data16,
                ((size_t)coding_y + index) * stride + (size_t)coding_x - 1U);
        }
        for (index = 0U; index < delta; ++index) {
            reference_pad[index + clamped_height] = reference_pad[index];
            recon_pad[index + clamped_height] = recon_pad[index];
        }
        for (position = start; position < padded_height;
             position = (uint8_t)(position + step)) {
            int32_t x = reference_pad[position];
            int32_t y = recon_pad[position];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }
        count += left_count;
    }
    if (count > 0) {
        int32_t alpha = (int16_t)av2h_cfl_derive_alpha(
            sum_x, sum_y, sum_xx, sum_xy, count, AV2H_BAWP_SHIFT);
        if (alpha == 0) alpha = 1 << AV2H_BAWP_SHIFT;
        *alpha_out = alpha;
        *beta_out =
            (((int32_t)sum_y << AV2H_BAWP_SHIFT) - sum_x * alpha) / count;
    } else {
        *alpha_out = 1 << AV2H_BAWP_SHIFT;
        *beta_out = -(1 << (AV2H_BAWP_SHIFT - 1));
    }
    return AV2H_OK;
}

Av2hStatus av2h_intrabc_morph_u16(
    const uint16_t *plane_data,
    size_t stride,
    uint32_t plane_width,
    uint32_t plane_height,
    uint16_t coding_x,
    uint16_t coding_y,
    uint16_t coding_width,
    uint16_t coding_height,
    int32_t reference_x,
    int32_t reference_y,
    uint16_t width,
    uint16_t height,
    uint8_t bit_depth,
    uint16_t *predictor) {
    int32_t alpha;
    int32_t beta;
    int32_t maximum = (int32_t)((1U << bit_depth) - 1U);
    size_t index;
    Av2hStatus status;
    if (plane_data == 0 || predictor == 0) return AV2H_INVALID_DATA;
    status = intrabc_morph_parameters(
        0, plane_data, stride, plane_width, plane_height, coding_x, coding_y,
        coding_width, coding_height, reference_x, reference_y, &alpha, &beta);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < (size_t)width * height; ++index) {
        int32_t value = ((int32_t)predictor[index] * alpha + beta) >>
            AV2H_BAWP_SHIFT;
        if (value < 0) value = 0;
        if (value > maximum) value = maximum;
        predictor[index] = (uint16_t)value;
    }
    return AV2H_OK;
}

static Av2hStatus apply_intrabc_morph_prediction(
    const Av2hReconstructionPlane *plane,
    uint16_t coding_x,
    uint16_t coding_y,
    int32_t reference_x,
    int32_t reference_y,
    uint16_t coding_width,
    uint16_t coding_height,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor) {
    int32_t alpha;
    int32_t beta;
    size_t index;
    Av2hStatus status;
    if (plane == 0 || predictor == 0) return AV2H_INVALID_DATA;
    status = intrabc_morph_parameters(
        plane->data, 0, plane->stride_samples, plane->width, plane->height,
        coding_x, coding_y, coding_width, coding_height,
        reference_x, reference_y, &alpha, &beta);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < (size_t)width * height; ++index) {
        predictor[index] = av2h_sample_clip(
            ((int32_t)predictor[index] * alpha + beta) >> AV2H_BAWP_SHIFT);
    }
    return AV2H_OK;
}

Av2hStatus av2h_intrabc_predict(
    Av2hDecodeDiagnostics *diagnostics,
    const Av2hFirstLeafMode *mode,
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor) {
    size_t row;
    size_t column;
    int32_t position_x;
    int32_t position_y;
    int32_t source_x;
    int32_t source_y;
    uint32_t subpel_x;
    uint32_t subpel_y;
    Av2hStatus status;
    av2h_reconstruction_diagnostics_set_luma_stage(
        diagnostics, 10U);
    position_x = av2h_intrabc_plane_position(
        block_x, mode->displacement_x, 0U);
    position_y = av2h_intrabc_plane_position(
        block_y, mode->displacement_y, 0U);
    source_x = av2h_intrabc_integer_position(position_x);
    source_y = av2h_intrabc_integer_position(position_y);
    subpel_x = (uint32_t)(
        position_x - source_x * (1 << AV2H_SUBPEL_BITS));
    subpel_y = (uint32_t)(
        position_y - source_y * (1 << AV2H_SUBPEL_BITS));
    av2h_reconstruction_diagnostics_set_luma_stage(
        diagnostics, 11U);
    if (source_x < 0 || source_y < 0 ||
        (uint32_t)source_x + width + (subpel_x != 0U) > plane->width ||
        (uint32_t)source_y + height + (subpel_y != 0U) > plane->height) {
        return AV2H_INVALID_DATA;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(
        diagnostics, 12U);
    for (row = 0U; row < (size_t)height + (subpel_y != 0U); ++row) {
        for (column = 0U; column < (size_t)width + (subpel_x != 0U);
             ++column) {
            size_t sample_x = (size_t)source_x + column;
            size_t sample_y = (size_t)source_y + row;
            if (!av2h_plane_sample_reconstructed(plane, sample_x, sample_y)) {
                av2h_reconstruction_diagnostics_set_intrabc_source(
                    diagnostics,
                    (uint16_t)sample_x, (uint16_t)sample_y,
                    (sample_y / 4U) * plane->reconstructed_stride_cells +
                        sample_x / 4U);
                return AV2H_UNSUPPORTED;
            }
        }
    }
    status = av2h_intrabc_convolve(
        plane->data, 0, plane->stride_samples, source_x, source_y,
        subpel_x, subpel_y, width, height, 8U, predictor, 0);
    if (status != AV2H_OK) return status;
    if (mode->morph_pred) {
        uint16_t coding_x = (uint16_t)(mode->block_origin_column * 4U);
        uint16_t coding_y = (uint16_t)(mode->block_origin_row * 4U);
        status = apply_intrabc_morph_prediction(
            plane, coding_x, coding_y,
            (int32_t)coding_x + av2h_intrabc_full_pel(mode->displacement_x),
            (int32_t)coding_y + av2h_intrabc_full_pel(mode->displacement_y),
            mode->block_width, mode->block_height,
            width, height, predictor);
        if (status != AV2H_OK) return status;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(
        diagnostics, 13U);
    return AV2H_OK;
}
