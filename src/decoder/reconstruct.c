#include "reconstruct.h"
#include "internal/decode_diagnostics.h"
static Av2hReconstructionDiagnostics reconstruction_diagnostics;

const Av2hReconstructionDiagnostics *av2h_reconstruction_diagnostics_get(
    void) {
    return &reconstruction_diagnostics;
}
void av2h_reconstruction_diagnostics_set_luma_stage(uint8_t value) {
    reconstruction_diagnostics.luma_stage = value;
}

void av2h_reconstruction_diagnostics_set_intrabc_source(
    uint16_t x, uint16_t y, size_t map_index) {
    reconstruction_diagnostics.intrabc_source_x = x;
    reconstruction_diagnostics.intrabc_source_y = y;
    reconstruction_diagnostics.intrabc_map_index = map_index;
}
void av2h_reconstruction_diagnostics_set_chroma_stage(uint8_t value) {
    reconstruction_diagnostics.chroma_stage = value;
}

void av2h_reconstruction_diagnostics_set_inverse_transform_stage(
    uint8_t value) {
    reconstruction_diagnostics.inverse_transform_stage = value;
}
#include "internal/reconstruct_modules.h"
#include "internal/sample_math.h"
#include "internal/sample_ops.h"
#include "tx_geometry.h"

#define directional_base_angle av2h_directional_base_angle
#define luma_wide_angle av2h_luma_wide_angle
#define wide_angle_map av2h_luma_wide_angle

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    return av2h_sample_clamp(value, minimum, maximum);
}

static int32_t round_shift(int64_t value, uint8_t shift) {
    int64_t adjusted = value + ((int64_t)1 << (shift - 1U));
    if (adjusted >= 0) return (int32_t)(adjusted >> shift);
    return (int32_t)-(((-adjusted) + (((int64_t)1 << shift) - 1)) >> shift);
}

static int32_t round_shift_signed(int64_t value, uint8_t shift) {
    int64_t magnitude = value < 0 ? -value : value;
    int32_t rounded = (int32_t)((magnitude +
        ((int64_t)1 << (shift - 1U))) >> shift);
    return value < 0 ? -rounded : rounded;
}

static int32_t floor_shift_i32(int32_t value, uint8_t shift) {
    if (value >= 0) return value >> shift;
    return -(int32_t)(((uint32_t)(-value) +
        (((uint32_t)1U << shift) - 1U)) >> shift);
}

static Av2hStatus reconstruct_lossless_4x4_u16(
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    uint8_t use_dpcm,
    uint8_t dpcm_horizontal,
    uint8_t identity_transform,
    uint8_t direct_dpcm,
    Av2hFirstReconstructionU16 *reconstruction) {
    int32_t intermediate[16U];
    int32_t residual[16U];
    size_t row;
    size_t column;
    if (coefficients == 0 || predictor == 0 || reconstruction == 0 ||
        coefficients->width != 4U || coefficients->height != 4U ||
        coefficients->coefficient_count != 16U || use_dpcm > 1U ||
        dpcm_horizontal > 1U || identity_transform > 1U ||
        direct_dpcm > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (identity_transform != 0U || direct_dpcm != 0U) {
        for (row = 0U; row < 16U; ++row) {
            residual[row] = floor_shift_i32(
                coefficients->dequantized[row], 3U);
        }
        if (use_dpcm != 0U && dpcm_horizontal != 0U) {
            for (row = 0U; row < 4U; ++row) {
                for (column = 1U; column < 4U; ++column) {
                    residual[row * 4U + column] +=
                        residual[row * 4U + column - 1U];
                }
            }
        } else if (use_dpcm != 0U) {
            for (column = 0U; column < 4U; ++column) {
                for (row = 1U; row < 4U; ++row) {
                    residual[row * 4U + column] +=
                        residual[(row - 1U) * 4U + column];
                }
            }
        }
        reconstruction->width = 4U;
        reconstruction->height = 4U;
        for (row = 0U; row < 16U; ++row) {
            reconstruction->residual[row] = residual[row];
            reconstruction->pixels[row] = av2h_sample_clip_10bit(
                (int32_t)predictor[row] + residual[row]);
        }
        return AV2H_OK;
    }
    for (row = 0U; row < 4U; ++row) {
        int32_t a = floor_shift_i32(
            coefficients->dequantized[row * 4U], 3U);
        int32_t c = floor_shift_i32(
            coefficients->dequantized[row * 4U + 1U], 3U);
        int32_t d = floor_shift_i32(
            coefficients->dequantized[row * 4U + 2U], 3U);
        int32_t b = floor_shift_i32(
            coefficients->dequantized[row * 4U + 3U], 3U);
        int32_t e;
        a += c;
        d -= b;
        e = floor_shift_i32(a - d, 1U);
        b = e - b;
        c = e - c;
        a -= b;
        d += c;
        intermediate[row * 4U] = a;
        intermediate[row * 4U + 1U] = b;
        intermediate[row * 4U + 2U] = c;
        intermediate[row * 4U + 3U] = d;
    }
    for (column = 0U; column < 4U; ++column) {
        int32_t a = intermediate[column];
        int32_t c = intermediate[4U + column];
        int32_t d = intermediate[8U + column];
        int32_t b = intermediate[12U + column];
        int32_t e;
        a += c;
        d -= b;
        e = floor_shift_i32(a - d, 1U);
        b = e - b;
        c = e - c;
        a -= b;
        d += c;
        residual[column] = a;
        residual[4U + column] = b;
        residual[8U + column] = c;
        residual[12U + column] = d;
    }
    if (use_dpcm != 0U && dpcm_horizontal != 0U) {
        for (row = 0U; row < 4U; ++row) {
            for (column = 1U; column < 4U; ++column) {
                residual[row * 4U + column] +=
                    residual[row * 4U + column - 1U];
            }
        }
    } else if (use_dpcm != 0U) {
        for (column = 0U; column < 4U; ++column) {
            for (row = 1U; row < 4U; ++row) {
                residual[row * 4U + column] +=
                    residual[(row - 1U) * 4U + column];
            }
        }
    }
    reconstruction->width = 4U;
    reconstruction->height = 4U;
    for (row = 0U; row < 16U; ++row) {
        reconstruction->residual[row] = residual[row];
        reconstruction->pixels[row] = av2h_sample_clip_10bit(
            (int32_t)predictor[row] + residual[row]);
    }
    return AV2H_OK;
}

uint8_t av2h_plane_sample_reconstructed(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    size_t index = (y / 4U) * plane->reconstructed_stride_cells + x / 4U;
    return index < plane->reconstructed_count_cells && plane->reconstructed[index];
}

static void plane_mark_reconstructed(Av2hReconstructionPlane *plane,
                                     uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height) {
    size_t first_column = x / 4U;
    size_t first_row = y / 4U;
    size_t last_column = ((size_t)x + width - 1U) / 4U;
    size_t last_row = ((size_t)y + height - 1U) / 4U;
    size_t row;
    size_t column;
    for (row = first_row; row <= last_row; ++row) {
        for (column = first_column; column <= last_column; ++column) {
            plane->reconstructed[row * plane->reconstructed_stride_cells + column] =
                1U;
        }
    }
}

static uint8_t plane_sample_reconstructed_u16(
    const Av2hReconstructionPlaneU16 *plane, size_t x, size_t y) {
    size_t index = (y / 4U) * plane->reconstructed_stride_cells + x / 4U;
    return index < plane->reconstructed_count_cells &&
        plane->reconstructed[index];
}

static void plane_mark_reconstructed_u16(
    Av2hReconstructionPlaneU16 *plane,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    size_t first_column = x / 4U;
    size_t first_row = y / 4U;
    size_t last_column = ((size_t)x + width - 1U) / 4U;
    size_t last_row = ((size_t)y + height - 1U) / 4U;
    size_t row;
    size_t column;
    for (row = first_row; row <= last_row; ++row) {
        for (column = first_column; column <= last_column; ++column) {
            plane->reconstructed[
                row * plane->reconstructed_stride_cells + column] = 1U;
        }
    }
}

static uint16_t dc_average_rectangular_u16(uint64_t sum, size_t count) {
    unsigned int divisor_log2 = 0U;
    unsigned int value = (unsigned int)count;
    unsigned int shift;
    uint32_t scale;
    while (value > 1U) {
        value >>= 1U;
        ++divisor_log2;
    }
    shift = divisor_log2 + 9U;
    scale = (uint32_t)(((1U << shift) + count / 2U) / count);
    return (uint16_t)((sum * scale + (1U << (shift - 1U))) >> shift);
}

static Av2hStatus intrabc_predict_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hReconstructionPlaneU16 *plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint16_t *predictor) {
    int32_t position_x = av2h_intrabc_plane_position(
        block_x, mode->displacement_x, 0U);
    int32_t position_y = av2h_intrabc_plane_position(
        block_y, mode->displacement_y, 0U);
    int32_t source_x = av2h_intrabc_integer_position(position_x);
    int32_t source_y = av2h_intrabc_integer_position(position_y);
    uint32_t subpel_x =
        (uint32_t)(position_x - source_x * (1 << AV2H_SUBPEL_BITS));
    uint32_t subpel_y =
        (uint32_t)(position_y - source_y * (1 << AV2H_SUBPEL_BITS));
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    if (source_x < 0 || source_y < 0 ||
        (uint32_t)source_x + width + (subpel_x != 0U) > plane->width ||
        (uint32_t)source_y + height + (subpel_y != 0U) > plane->height) {
        return AV2H_INVALID_DATA;
    }
    for (row = 0U; row < height + (subpel_y != 0U); ++row) {
        for (column = 0U; column < width + (subpel_x != 0U); ++column) {
            size_t sample_x = (size_t)source_x + column;
            size_t sample_y = (size_t)source_y + row;
            if (!plane_sample_reconstructed_u16(plane, sample_x, sample_y)) {
                av2h_reconstruction_diagnostics_set_intrabc_source(
                    (uint16_t)sample_x, (uint16_t)sample_y,
                    (sample_y / 4U) * plane->reconstructed_stride_cells +
                        sample_x / 4U);
                return AV2H_UNSUPPORTED;
            }
        }
    }
    status = av2h_intrabc_convolve(
        0, plane->data, plane->stride_samples, source_x, source_y,
        subpel_x, subpel_y, width, height, 10U, 0, predictor);
    if (status != AV2H_OK) return status;
    if (mode->morph_pred != 0U) {
        uint16_t coding_x = (uint16_t)(mode->block_origin_column * 4U);
        uint16_t coding_y = (uint16_t)(mode->block_origin_row * 4U);
        status = av2h_intrabc_morph_u16(
            plane->data, plane->stride_samples, plane->width, plane->height,
            coding_x, coding_y, mode->block_width, mode->block_height,
            (int32_t)coding_x + av2h_intrabc_full_pel(mode->displacement_x),
            (int32_t)coding_y + av2h_intrabc_full_pel(mode->displacement_y),
            width, height, 10U, predictor);
        if (status != AV2H_OK) return status;
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    size_t index;
    size_t count;
    uint8_t first_shift;
    uint8_t second_shift;
    uint8_t rectangular;
    uint8_t identity_rows;
    uint8_t adst_rows;
    uint8_t flip_adst_rows;
    uint8_t identity_columns;
    uint8_t adst_columns;
    uint8_t flip_adst_columns = 0U;
    Av2hStatus status;
    if (mode == 0 || prefix == 0 || transform == 0 || coefficients == 0 ||
        scratch == 0 || reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 &&
        (transform->primary_type == AV2H_TX_DCT_DCT ||
         transform->primary_type == AV2H_TX_IDTX)) {
        return av2h_reconstruct_lossless_4x4(
            mode, transform, coefficients, predictor,
            predictor_stride_samples, reconstruction);
    }
    if (prefix->all_zero) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        Av2hStatus descriptor_status;
        if (prefix->eob != 0U || coefficients->nonzero_count != 0U ||
            predictor == 0) {
            return AV2H_UNSUPPORTED;
        }
        descriptor_status = av2h_tx_descriptor_get(
            prefix->tx_size, &descriptor);
        if (descriptor_status != AV2H_OK) return descriptor_status;
        if (predictor_stride_samples < descriptor->output_width ||
            coefficients->coefficient_count !=
                coefficients->width * coefficients->height) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        output_count = (size_t)descriptor->output_width *
            descriptor->output_height;
        if (output_count > AV2H_RECON_SAMPLE_CAPACITY) {
            return AV2H_LIMIT_EXCEEDED;
        }
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (prefix->q_index == 0U &&
        transform->primary_type == AV2H_TX_IDTX &&
        mode->use_dpcm_y != 0U) {
        const Av2hTxDescriptor *descriptor;
        size_t width;
        size_t height;
        unsigned int scale_bits;
        size_t row;
        size_t column;
        if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK) {
            return AV2H_UNSUPPORTED;
        }
        width = descriptor->output_width;
        height = descriptor->output_height;
        if (coefficients->width != width || coefficients->height != height ||
            coefficients->coefficient_count != width * height ||
            predictor == 0 || predictor_stride_samples < width) {
            return AV2H_UNSUPPORTED;
        }
        scale_bits = 3U - (coefficients->coefficient_count > 256U ? 1U : 0U) -
            (coefficients->coefficient_count > 1024U ? 1U : 0U);
        reconstruction->width = (uint16_t)width;
        reconstruction->height = (uint16_t)height;
        if (mode->dpcm_mode_y == 0U) {
            for (column = 0U; column < width; ++column) {
                int32_t delay = 0;
                for (row = 0U; row < height; ++row) {
                    size_t sample = row * width + column;
                    int32_t current =
                        coefficients->dequantized[sample] >> scale_bits;
                    int32_t residual = current + delay;
                    reconstruction->residual[sample] = (int16_t)residual;
                    reconstruction->pixels[sample] = av2h_sample_clip(
                        (int32_t)predictor[
                            row * predictor_stride_samples + column] +
                        residual);
                    delay += current;
                }
            }
        } else {
            for (row = 0U; row < height; ++row) {
                int32_t delay = 0;
                for (column = 0U; column < width; ++column) {
                    size_t sample = row * width + column;
                    int32_t current =
                        coefficients->dequantized[sample] >> scale_bits;
                    int32_t residual = current + delay;
                    reconstruction->residual[sample] = (int16_t)residual;
                    reconstruction->pixels[sample] = av2h_sample_clip(
                        (int32_t)predictor[
                            row * predictor_stride_samples + column] +
                        residual);
                    delay += current;
                }
            }
        }
        return AV2H_OK;
    }
    if (av2h_reconstruct_generic_selected(mode, transform, prefix->tx_size)) {
        return av2h_reconstruct_transform_generic(
            mode, prefix, transform, coefficients, predictor, predictor_stride_samples,
            scratch, reconstruction);
    }
        if ((mode->mode != AV2H_INTRA_DC &&
            mode->mode != AV2H_INTRA_VERTICAL &&
            mode->mode != AV2H_INTRA_HORIZONTAL &&
            mode->mode != AV2H_INTRA_PAETH &&
            mode->mode != AV2H_INTRA_SMOOTH &&
            mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
            mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
            mode->mode != AV2H_INTRA_D67 &&
            mode->mode != AV2H_INTRA_D113 &&
            mode->mode != AV2H_INTRA_D135 &&
            mode->mode != AV2H_INTRA_D157 &&
            mode->mode != AV2H_INTRA_D45 && mode->mode != AV2H_INTRA_D203) ||
        coefficients->width == 0U || coefficients->height == 0U ||
        coefficients->coefficient_count !=
            coefficients->width * coefficients->height) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->use_intrabc && mode->skip_transform) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK ||
            descriptor == 0 || mode->intrabc_mode > 1U ||
            mode->mv_precision != 3U || !prefix->all_zero ||
            prefix->eob != 0U ||
            transform->primary_type != AV2H_TX_DCT_DCT ||
            coefficients->width != descriptor->coefficient_width ||
            coefficients->height != descriptor->coefficient_height ||
            coefficients->coefficient_count != descriptor->eob_capacity ||
            coefficients->nonzero_count != 0U || predictor == 0 ||
            predictor_stride_samples < descriptor->output_width) {
            return AV2H_UNSUPPORTED;
        }
        output_count = (size_t)descriptor->output_width *
            descriptor->output_height;
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->enable_ibp == 1U &&
        mode->use_intrabc == 0U && mode->skip_transform == 0U &&
        mode->fsc_mode == 0U &&
        prefix->tx_size == AV2H_TX_16X16 && prefix->all_zero &&
        prefix->eob == 0U &&
        transform->primary_type == AV2H_TX_DCT_DCT &&
        coefficients->width == 16U && coefficients->height == 16U &&
        coefficients->coefficient_count == 256U &&
        coefficients->nonzero_count == 0U && predictor != 0 &&
        predictor_stride_samples >= 16U) {
        reconstruction->width = 16U;
        reconstruction->height = 16U;
        for (index = 0U; index < 256U; ++index) {
            size_t row = index >> 4U;
            size_t column = index & 15U;
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->enable_ibp == 1U &&
        mode->use_intrabc == 0U && mode->skip_transform == 0U &&
        mode->fsc_mode == 0U &&
        prefix->tx_size == AV2H_TX_64X16 && prefix->all_zero &&
        prefix->eob == 0U &&
        transform->primary_type == AV2H_TX_DCT_DCT &&
        coefficients->width == 32U && coefficients->height == 16U &&
        coefficients->coefficient_count == 512U &&
        coefficients->nonzero_count == 0U && predictor != 0 &&
        predictor_stride_samples >= 64U) {
        reconstruction->width = 64U;
        reconstruction->height = 16U;
        for (index = 0U; index < 512U; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < 1024U; ++index) {
            size_t row = index >> 6U;
            size_t column = index & 63U;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        !((prefix->tx_size == AV2H_TX_4X4 &&
           (transform->primary_type == AV2H_TX_DCT_ADST ||
            transform->primary_type == AV2H_TX_DCT_DCT)) ||
                    (prefix->tx_size == AV2H_TX_4X8 &&
                     (transform->primary_type == AV2H_TX_ADST_DCT ||
                        transform->primary_type == AV2H_TX_ADST_ADST)) ||
          (prefix->tx_size == AV2H_TX_8X8 &&
                     (transform->primary_type == AV2H_TX_DCT_DCT ||
                        transform->primary_type == AV2H_TX_ADST_ADST)) ||
          (prefix->tx_size == AV2H_TX_16X8 &&
                     (transform->primary_type == AV2H_TX_ADST_ADST ||
                        transform->primary_type == AV2H_TX_DCT_DCT)) ||
          (prefix->tx_size == AV2H_TX_16X16 &&
           transform->primary_type == AV2H_TX_ADST_ADST) ||
          (prefix->tx_size == AV2H_TX_16X32 &&
           transform->primary_type == AV2H_TX_DCT_DCT) ||
          (prefix->tx_size == AV2H_TX_8X32 &&
           transform->primary_type == AV2H_TX_H_DCT))) {
        return AV2H_UNSUPPORTED;
    }
    if (prefix->tx_size == AV2H_TX_8X4 &&
        transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if ((prefix->tx_size == AV2H_TX_4X8 ||
                prefix->tx_size == AV2H_TX_8X4) &&
               transform->primary_type == AV2H_TX_H_ADST &&
               (mode->mode == AV2H_INTRA_HORIZONTAL ||
                (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                 prefix->tx_size == AV2H_TX_8X4))) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_D157)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D135)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_FLIPADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
        flip_adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_VERTICAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X16 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_PAETH)) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X16 &&
               transform->primary_type == AV2H_TX_DCT_ADST) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               (mode->use_intrabc || mode->fsc_mode ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_H_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_HORIZONTAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_H_FLIPADST &&
               mode->use_intrabc) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_D45) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D45)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_HORIZONTAL) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X16 &&
               transform->primary_type == AV2H_TX_IDTX &&
               (mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D135) && mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D113 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D157 ||
                mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D113 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
             (mode->mode == AV2H_INTRA_SMOOTH ||
              mode->mode == AV2H_INTRA_D157)) {
        first_shift = 6U;
         second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X16 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_DC)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                mode->mode == AV2H_INTRA_PAETH ||
                mode->mode == AV2H_INTRA_HORIZONTAL)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_DC)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 &&
               transform->primary_type == AV2H_TX_H_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_DC) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->dequantized[index] != 0 &&
                index != 0U && index != 16U &&
                index != 17U && index != 32U) {
                return AV2H_UNSUPPORTED;
            }
        }
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->dequantized[index] != 0 &&
                index != 0U && index != 1U) {
                return AV2H_UNSUPPORTED;
            }
        }
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D113)) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_H_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->mode == AV2H_INTRA_D45 && mode->fsc_mode) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X4 &&
               transform->primary_type == AV2H_TX_H_DCT &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X4 &&
               transform->primary_type == AV2H_TX_FLIPADST_DCT &&
               (mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D157)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
        flip_adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_32X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D45)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_32X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    reconstruction->width = coefficients->width;
    reconstruction->height = coefficients->height;
    count = coefficients->coefficient_count;
    if (transform->secondary_symbol != 0U) {
        for (index = 0U; index < count; ++index) {
            scratch->scaled[index] = coefficients->dequantized[index];
        }
        status = av2h_inverse_secondary_transform(
            mode, prefix, transform, coefficients, scratch->scaled);
        if (status != AV2H_OK) return status;
        if (rectangular) {
            for (index = 0U; index < count; ++index) {
                scratch->scaled[index] = clamp(round_shift(
                    (int64_t)scratch->scaled[index] * 2896, 12U),
                    -32768, 32767);
            }
        }
    } else {
        for (index = 0U; index < count; ++index) {
            scratch->scaled[index] = rectangular ? round_shift(
                (int64_t)coefficients->dequantized[index] * 2896, 12U) :
                coefficients->dequantized[index];
            scratch->scaled[index] =
                clamp(scratch->scaled[index], -32768, 32767);
        }
    }
    status = av2h_inverse_rows(scratch->scaled, scratch->transposed,
                          coefficients->width, coefficients->height,
                          first_shift, identity_rows, adst_rows,
                          flip_adst_rows);
    if (status != AV2H_OK) return status;
    status = av2h_inverse_columns(scratch->transposed, scratch->residual,
                             coefficients->width, coefficients->height,
                             second_shift, identity_columns, adst_columns,
                             flip_adst_columns);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < count; ++index) {
        int32_t predicted = predictor == 0 ?
            av2h_sample_midpoint_8bit() :
            predictor[(index / coefficients->width) * predictor_stride_samples +
                      index % coefficients->width];
        int32_t pixel = av2h_sample_clip(
            predicted + scratch->residual[index]);
        reconstruction->residual[index] = (int16_t)scratch->residual[index];
        reconstruction->pixels[index] = (uint8_t)pixel;
    }
    return AV2H_OK;
}

Av2hStatus av2h_first_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    return av2h_luma_reconstruct(mode, prefix, transform, coefficients,
                                 0, 0U, scratch, reconstruction);
}

static Av2hStatus palette_map_origin(
    const Av2hFirstLeafMode *mode,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t output_width,
    uint16_t output_height,
    size_t *offset_x,
    size_t *offset_y) {
    size_t origin_x = (size_t)mode->palette_origin_column * 4U;
    size_t origin_y = (size_t)mode->palette_origin_row * 4U;
    if (mode->palette_size > AV2H_PALETTE_MAX_SIZE ||
        (size_t)block_x < origin_x || (size_t)block_y < origin_y) {
        return AV2H_UNSUPPORTED;
    }
    *offset_x = (size_t)block_x - origin_x;
    *offset_y = (size_t)block_y - origin_y;
    if (*offset_x + output_width > mode->palette_width ||
        *offset_y + output_height > mode->palette_height) {
        return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

static Av2hStatus palette_fill_predictor(
    const Av2hFirstLeafMode *mode,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t output_width,
    uint16_t output_height,
    uint8_t *predictor) {
    size_t offset_x;
    size_t offset_y;
    size_t row;
    size_t column;
    Av2hStatus status = palette_map_origin(
        mode, block_x, block_y, output_width, output_height, &offset_x,
        &offset_y);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < output_height; ++row) {
        for (column = 0U; column < output_width; ++column) {
            uint8_t color_index = av2h_palette_map_get(
                mode,
                (offset_y + row) * mode->palette_width + offset_x + column);
            if (color_index >= mode->palette_size) return AV2H_INVALID_DATA;
            predictor[row * (size_t)output_width + column] =
                (uint8_t)mode->palette_colors[color_index];
        }
    }
    return AV2H_OK;
}

static Av2hStatus palette_fill_predictor_u16(
    const Av2hFirstLeafMode *mode,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t output_width,
    uint16_t output_height,
    uint16_t *predictor) {
    size_t offset_x;
    size_t offset_y;
    size_t row;
    size_t column;
    Av2hStatus status = palette_map_origin(
        mode, block_x, block_y, output_width, output_height, &offset_x,
        &offset_y);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < output_height; ++row) {
        for (column = 0U; column < output_width; ++column) {
            uint8_t color_index = av2h_palette_map_get(
                mode,
                (offset_y + row) * mode->palette_width + offset_x + column);
            if (color_index >= mode->palette_size) return AV2H_INVALID_DATA;
            predictor[row * (size_t)output_width + column] =
                mode->palette_colors[color_index];
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch) {
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references;
    const Av2hTxDescriptor *descriptor;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t output_width;
    uint16_t output_height;
    uint16_t visible_width;
    uint16_t visible_height;
    size_t above_count;
    size_t left_count;
    size_t secondary_above_count = 0U;
    size_t secondary_left_count = 0U;
    size_t above_reference_y;
    uint8_t is_top_superblock_boundary;
    size_t index;
    Av2hStatus status;
    Av2hFirstLeafMode effective_mode;
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        plane->stride_samples < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == 0 ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells < plane->reconstructed_stride_cells *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(0U);
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    output_width = descriptor->output_width;
    output_height = descriptor->output_height;
    is_top_superblock_boundary = (uint8_t)(mode->mrl_index != 0U &&
        mode->superblock_size != 0U &&
        block_y % mode->superblock_size == 0U);
    above_reference_y = (size_t)block_y - 1U -
        (is_top_superblock_boundary ? 0U : mode->mrl_index);
    if (block_x >= plane->width || block_y >= plane->height ||
        coefficients->width > 32U || coefficients->height > 32U) {
        return AV2H_INVALID_ARGUMENT;
    }
    visible_width = output_width < plane->width - block_x ?
        output_width : (uint16_t)(plane->width - block_x);
    visible_height = output_height < plane->height - block_y ?
        output_height : (uint16_t)(plane->height - block_y);
    above_count = block_y <= mode->mrl_index ||
        block_x >= plane->width ? 0U :
        (size_t)plane->width - block_x;
    if (mode->tx_extended_edges_unavailable != 0U &&
        above_count > output_width) {
        above_count = output_width;
    }
    if (above_count > 2U * output_width) {
        above_count = 2U * output_width;
    }
    if (mode->mode == AV2H_INTRA_PAETH && above_count != 0U &&
        above_count < output_width) {
        above_count = output_width;
    }
    if (above_count > sizeof(scratch->above)) {
        above_count = sizeof(scratch->above);
    }
    for (index = 0U; index < above_count; ++index) {
        size_t source_x = block_x + index;
        if (source_x >= plane->width) {
            source_x = plane->width - 1U;
        }
        if (!av2h_plane_sample_reconstructed(
            plane, source_x, above_reference_y)) {
            above_count = index;
            break;
        }
    }
    for (index = 0U; index < above_count; ++index) {
        size_t source_x = block_x + index;
        if (source_x >= plane->width) {
            source_x = plane->width - 1U;
        }
        scratch->above[index] = av2h_reconstructed_sample_load_u8(
            plane->data, plane->stride_samples, source_x, above_reference_y);
    }
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_DC) &&
        above_count != 0U &&
        above_count < output_width) {
        uint8_t edge = scratch->above[above_count - 1U];
        for (index = above_count; index < output_width; ++index) {
            scratch->above[index] = edge;
        }
        above_count = output_width;
    }
    left_count = block_x <= mode->mrl_index ? 0U : output_height;
    if (left_count != 0U && mode->use_intra_dip != 0U &&
        mode->tx_extended_edges_unavailable == 0U) {
        left_count += output_height >> 2U;
    } else if (left_count != 0U &&
        mode->tx_extended_edges_unavailable == 0U &&
        (mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
        block_y + left_count < plane->height) {
        ++left_count;
    } else if (left_count != 0U && mode->mrl_index == 0U &&
               directional_base_angle(mode->mode) != 0) {
        int angle = luma_wide_angle(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            output_width, output_height);
        if (mode->tx_extended_edges_unavailable == 0U &&
            (angle > 180 ||
             (mode->enable_ibp != 0U && angle > 0 && angle < 90))) {
            left_count += output_width;
            if (left_count > 2U * output_height) {
                left_count = 2U * output_height;
            }
        }
    } else if (left_count != 0U && mode->mrl_index != 0U &&
               mode->tx_extended_edges_unavailable == 0U &&
               directional_base_angle(mode->mode) != 0 &&
               (luma_wide_angle(
                   directional_base_angle(mode->mode) +
                       mode->angle_delta * 3 +
                       (mode->mrl_index == 1U ? 1 :
                        mode->mrl_index == 2U ? -1 : 0),
                   output_width, output_height) > 180 ||
                (mode->enable_ibp != 0U && luma_wide_angle(
                    directional_base_angle(mode->mode) +
                        mode->angle_delta * 3 +
                        (mode->mrl_index == 1U ? 1 :
                         mode->mrl_index == 2U ? -1 : 0),
                    output_width, output_height) < 90))) {
        left_count += output_width + 2U * mode->mrl_index;
    } else if (left_count != 0U &&
               mode->mode == AV2H_INTRA_HORIZONTAL &&
               mode->angle_delta == 1 && mode->mrl_index == 0U &&
               output_width == 16U && output_height == 16U) {
        left_count += output_width;
    }
    if (left_count > sizeof(scratch->left)) {
        left_count = sizeof(scratch->left);
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->height) {
            source_y = plane->height - 1U;
        }
        if (!av2h_plane_sample_reconstructed(
            plane, (size_t)block_x - 1U - mode->mrl_index, source_y)) {
            left_count = index;
            break;
        }
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->height) {
            source_y = plane->height - 1U;
        }
        scratch->left[index] = av2h_reconstructed_sample_load_u8(
            plane->data, plane->stride_samples,
            block_x - 1U - mode->mrl_index, source_y);
    }
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
            mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
            mode->mode == AV2H_INTRA_PAETH) &&
        left_count != 0U && left_count < output_height) {
        uint8_t edge = scratch->left[left_count - 1U];
        for (index = left_count; index < output_height; ++index) {
            scratch->left[index] = edge;
        }
        left_count = output_height;
    }
    if (mode->mrl_index != 0U && mode->multi_line_mrl != 0U) {
        if (block_y != 0U) {
            secondary_above_count =
                (size_t)plane->width - block_x;
            if (mode->tx_extended_edges_unavailable != 0U &&
                secondary_above_count > output_width) {
                secondary_above_count = output_width;
            }
            if (secondary_above_count > 2U * output_width) {
                secondary_above_count = 2U * output_width;
            }
            if (secondary_above_count > sizeof(scratch->secondary_above)) {
                secondary_above_count = sizeof(scratch->secondary_above);
            }
            for (index = 0U; index < secondary_above_count; ++index) {
                if (!av2h_plane_sample_reconstructed(
                        plane, block_x + index, (size_t)block_y - 1U)) {
                    secondary_above_count = index;
                    break;
                }
                scratch->secondary_above[index] =
                    av2h_reconstructed_sample_load_u8(
                        plane->data, plane->stride_samples,
                        block_x + index, (size_t)block_y - 1U);
            }
        }
        if (block_x != 0U) {
            secondary_left_count = output_height;
            if (mode->tx_extended_edges_unavailable == 0U &&
                directional_base_angle(mode->mode) != 0 &&
                luma_wide_angle(
                    directional_base_angle(mode->mode) +
                        mode->angle_delta * 3 +
                        (mode->mrl_index == 1U ? 1 :
                         mode->mrl_index == 2U ? -1 : 0),
                    output_width, output_height) > 180) {
                secondary_left_count += output_width;
                if (secondary_left_count > 2U * output_height) {
                    secondary_left_count = 2U * output_height;
                }
            }
            if (secondary_left_count > sizeof(scratch->secondary_left)) {
                secondary_left_count = sizeof(scratch->secondary_left);
            }
            for (index = 0U; index < secondary_left_count; ++index) {
                size_t source_y = (size_t)block_y + index;
                if (source_y >= plane->height) {
                    source_y = plane->height - 1U;
                }
                if (!av2h_plane_sample_reconstructed(
                        plane, (size_t)block_x - 1U, source_y)) {
                    secondary_left_count = index;
                    break;
                }
                scratch->secondary_left[index] =
                    av2h_reconstructed_sample_load_u8(
                        plane->data, plane->stride_samples,
                        block_x - 1U, source_y);
            }
        }
    }
    references.left = left_count == 0U ? 0 : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? 0 : scratch->above;
    references.above_count = above_count;
    references.secondary_left = secondary_left_count == 0U ?
        0 : scratch->secondary_left;
    references.secondary_left_count = secondary_left_count;
    references.secondary_above = secondary_above_count == 0U ?
        0 : scratch->secondary_above;
    references.secondary_above_count = secondary_above_count;
    references.has_secondary_above_left =
        secondary_above_count != 0U && secondary_left_count != 0U;
    references.secondary_above_left =
        references.has_secondary_above_left ?
            plane->data[((size_t)block_y - 1U) * plane->stride_samples +
                        block_x - 1U] : 0U;
    references.has_above_left = block_x > mode->mrl_index &&
        block_y > mode->mrl_index;
    references.above_left = references.has_above_left ?
        plane->data[((size_t)block_y - 1U - mode->mrl_index) * plane->stride_samples +
                    block_x - 1U - mode->mrl_index] :
        0U;
    references.negative_count = 0U;
    if (mode->mrl_index != 0U && references.has_above_left) {
        size_t negative_index;
        references.negative_count = (uint8_t)(mode->mrl_index + 1U);
        for (negative_index = 1U;
             negative_index <= references.negative_count;
             ++negative_index) {
            references.above_negative[negative_index] = plane->data[
                above_reference_y * plane->stride_samples + block_x - negative_index];
            references.left_negative[negative_index] = plane->data[
                ((size_t)block_y - (is_top_superblock_boundary ? 1U :
                    negative_index)) * plane->stride_samples +
                block_x - 1U - mode->mrl_index];
        }
    }
    if (mode->palette_size != 0U) {
        status = palette_fill_predictor(
            mode, block_x, block_y, output_width, output_height,
            scratch->predictor);
        if (status != AV2H_OK) return status;
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width, scratch, &reconstruction);
    } else if (mode->use_intrabc) {
        status = av2h_intrabc_predict(
            mode, plane, block_x, block_y, output_width, output_height,
            scratch->predictor);
        if (status != AV2H_OK) return status;
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width, scratch, &reconstruction);
    } else if (mode->mode == AV2H_INTRA_DC && above_count == 0U &&
               left_count == 0U && mode->use_intra_dip == 0U) {
        for (index = 0U; index < (size_t)output_width * output_height;
             ++index) {
            scratch->predictor[index] = av2h_sample_midpoint_8bit();
        }
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width,
            scratch, &reconstruction);
    } else {
        av2h_reconstruction_diagnostics_set_luma_stage(1U);
        av2h_memory_copy(&effective_mode, mode, sizeof(effective_mode));
        if (prefix->tx_size == AV2H_TX_4X4) {
            effective_mode.enable_ibp = 0U;
        }
        status = av2h_luma_predict(
            &effective_mode, output_width, output_height, &references,
            scratch->predictor, output_width);
        if (status != AV2H_OK) return status;
        av2h_reconstruction_diagnostics_set_luma_stage(2U);
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width, scratch, &reconstruction);
    }
    if (status != AV2H_OK) return status;
    av2h_reconstruction_diagnostics_set_luma_stage(3U);
    if (reconstruction.width != output_width ||
        reconstruction.height != output_height) {
        return AV2H_UNSUPPORTED;
    }
    av2h_predictor_commit_u8(
        plane->data + (size_t)block_y * plane->stride_samples + block_x,
        plane->stride_samples, reconstruction.pixels, output_width,
        visible_width, visible_height);
    plane_mark_reconstructed(
        plane, block_x, block_y, visible_width, visible_height);
    return AV2H_OK;
}

Av2hStatus av2h_luma_reconstruct_to_plane_u16(
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane,
    Av2hReconstructionScratchU16 *scratch) {
    Av2hFirstReconstructionU16 reconstruction;
    const Av2hTxDescriptor *descriptor;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t visible_width;
    uint16_t visible_height;
    size_t index;
    uint64_t sum = 0U;
    size_t count = 0U;
    size_t above_count = 0U;
    size_t above_target;
    size_t left_count = 0U;
    size_t left_target;
    size_t secondary_above_count = 0U;
    size_t secondary_left_count = 0U;
    size_t above_reference_y;
    Av2hLumaReferencesU16 references = { 0 };
    uint16_t dc_value;
    uint8_t is_top_superblock_boundary;
    uint8_t top_vertical_multi_line_mrl;
    uint8_t zone_one_mrl;
    uint8_t directional_mrl;
    uint8_t directional_multi_line_mrl;
    int prediction_angle;
    Av2hStatus status;
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        plane->reconstructed == 0 || plane->stride_samples < plane->width ||
        plane->reference_width == 0U || plane->reference_height == 0U ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells < plane->reconstructed_stride_cells *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(0U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    if (block_x >= plane->width || block_y >= plane->height) {
        av2h_reconstruction_diagnostics_set_luma_stage(20U);
        return AV2H_UNSUPPORTED;
    }
    prediction_angle = luma_wide_angle(
        av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
            (mode->mrl_index == 1U ? 1 :
             mode->mrl_index == 2U ? -1 : 0),
        descriptor->output_width, descriptor->output_height);
    if (mode->mode != AV2H_INTRA_DC &&
        mode->mode != AV2H_INTRA_VERTICAL &&
        mode->mode != AV2H_INTRA_HORIZONTAL &&
        mode->mode != AV2H_INTRA_SMOOTH &&
        mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
        mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
        mode->mode != AV2H_INTRA_PAETH &&
        mode->mode != AV2H_INTRA_D45 &&
        mode->mode != AV2H_INTRA_D135 &&
        mode->mode != AV2H_INTRA_D113 &&
        mode->mode != AV2H_INTRA_D67 &&
        mode->mode != AV2H_INTRA_D157 &&
        mode->mode != AV2H_INTRA_D203) {
        av2h_reconstruction_diagnostics_set_luma_stage(21U);
        return AV2H_UNSUPPORTED;
    }
    if (mode->angle_delta != 0 &&
        av2h_directional_base_angle(mode->mode) == 0) {
        av2h_reconstruction_diagnostics_set_luma_stage(22U);
        return AV2H_UNSUPPORTED;
    }
    top_vertical_multi_line_mrl = (uint8_t)(
        mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 1U &&
        mode->enable_ibp <= 1U && block_y > mode->mrl_index &&
        descriptor->output_width == 8U &&
        descriptor->output_height == 8U);
    zone_one_mrl = (uint8_t)(
        mode->mrl_index <= 2U && mode->multi_line_mrl == 0U &&
        prediction_angle > 0 && prediction_angle < 90);
    directional_mrl = (uint8_t)(
        av2h_directional_base_angle(mode->mode) != 0 &&
        mode->mrl_index <= 3U && mode->multi_line_mrl == 0U);
    directional_multi_line_mrl = (uint8_t)(
        av2h_directional_base_angle(mode->mode) != 0 &&
        mode->mrl_index > 0U && mode->mrl_index <= 3U &&
        mode->multi_line_mrl == 1U);
    if ((mode->mrl_index != 0U || mode->multi_line_mrl != 0U) &&
        top_vertical_multi_line_mrl == 0U && zone_one_mrl == 0U &&
        directional_mrl == 0U && directional_multi_line_mrl == 0U) {
        av2h_reconstruction_diagnostics_set_luma_stage(23U);
        return AV2H_UNSUPPORTED;
    }
    visible_width = descriptor->output_width < plane->width - block_x ?
        descriptor->output_width : (uint16_t)(plane->width - block_x);
    visible_height = descriptor->output_height < plane->height - block_y ?
        descriptor->output_height : (uint16_t)(plane->height - block_y);
    above_target = descriptor->output_width;
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) &&
        block_x + above_target < plane->width) {
        ++above_target;
    }
    if (prediction_angle > 0 && prediction_angle < 90) {
        above_target += descriptor->output_width;
        if (above_target > 2U * descriptor->output_width) {
            above_target = 2U * descriptor->output_width;
        }
    }
    if (mode->enable_ibp != 0U && prediction_angle > 180) {
        above_target += descriptor->output_height;
        if (above_target > 2U * descriptor->output_width) {
            above_target = 2U * descriptor->output_width;
        }
    }
    if (above_target > sizeof(scratch->above) /
                           sizeof(scratch->above[0])) {
        above_target = sizeof(scratch->above) /
            sizeof(scratch->above[0]);
    }
    if ((mode->mode == AV2H_INTRA_D67 && mode->angle_delta == 0) ||
        (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == -2) ||
        zone_one_mrl != 0U) {
        above_target = (size_t)plane->width - block_x;
        if (above_target > sizeof(scratch->above) /
                               sizeof(scratch->above[0])) {
            above_target = sizeof(scratch->above) /
                sizeof(scratch->above[0]);
        }
    }
    is_top_superblock_boundary = (uint8_t)(mode->mrl_index != 0U &&
        mode->superblock_size != 0U &&
        block_y % mode->superblock_size == 0U);
    above_reference_y = (size_t)block_y - 1U -
        (is_top_superblock_boundary ? 0U : mode->mrl_index);
    if (block_y != 0U) {
        for (index = 0U; index < above_target; ++index) {
            size_t sample_x = block_x + index;
            if (sample_x >= plane->width) {
                if (av2h_directional_base_angle(mode->mode) != 0) break;
                sample_x = plane->width - 1U;
            }
            if (!plane_sample_reconstructed_u16(
                    plane, sample_x, above_reference_y)) {
                break;
            }
            scratch->above[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples,
                sample_x, above_reference_y);
            sum += scratch->above[index];
            ++count;
            ++above_count;
        }
    }
    if (top_vertical_multi_line_mrl != 0U) {
        for (index = 0U; index < descriptor->output_width; ++index) {
            if (!plane_sample_reconstructed_u16(
                    plane, block_x + index, block_y - 1U)) {
                break;
            }
            scratch->secondary_above[index] =
                av2h_reconstructed_sample_load_u16(
                    plane->data, plane->stride_samples,
                    block_x + index, block_y - 1U);
            ++secondary_above_count;
        }
    }
    left_target = descriptor->output_height;
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
        block_y + left_target < plane->height) {
        ++left_target;
    }
    if (av2h_directional_base_angle(mode->mode) != 0) {
        if (prediction_angle > 180 ||
            (mode->enable_ibp != 0U && prediction_angle < 90)) {
            left_target += descriptor->output_width +
                2U * mode->mrl_index;
            if (left_target > 2U * descriptor->output_height) {
                left_target = 2U * descriptor->output_height;
            }
        }
    }
    if (left_target > sizeof(scratch->left) / sizeof(scratch->left[0])) {
        left_target = sizeof(scratch->left) / sizeof(scratch->left[0]);
    }
    if (block_x > mode->mrl_index) {
        for (index = 0U; index < left_target; ++index) {
            size_t sample_y = block_y + index;
            if (sample_y >= plane->height) {
                if (av2h_directional_base_angle(mode->mode) != 0) break;
                sample_y = plane->height - 1U;
            }
            if (!plane_sample_reconstructed_u16(
                    plane, block_x - 1U - mode->mrl_index, sample_y)) {
                break;
            }
            scratch->left[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples,
                block_x - 1U - mode->mrl_index, sample_y);
            sum += scratch->left[index];
            ++count;
            ++left_count;
        }
    }
    if (directional_multi_line_mrl != 0U) {
        if (block_y != 0U) {
            secondary_above_count = (size_t)plane->width - block_x;
            if (secondary_above_count >
                sizeof(scratch->secondary_above) /
                    sizeof(scratch->secondary_above[0])) {
                secondary_above_count =
                    sizeof(scratch->secondary_above) /
                    sizeof(scratch->secondary_above[0]);
            }
            for (index = 0U; index < secondary_above_count; ++index) {
                if (!plane_sample_reconstructed_u16(
                        plane, block_x + index, block_y - 1U)) {
                    secondary_above_count = index;
                    break;
                }
                scratch->secondary_above[index] =
                    av2h_reconstructed_sample_load_u16(
                        plane->data, plane->stride_samples,
                        block_x + index, block_y - 1U);
            }
        }
        if (block_x != 0U) {
            secondary_left_count = descriptor->output_height;
            if (luma_wide_angle(
                    av2h_directional_base_angle(mode->mode) +
                        mode->angle_delta * 3 +
                        (mode->mrl_index == 1U ? 1 :
                         mode->mrl_index == 2U ? -1 : 0),
                    descriptor->output_width,
                    descriptor->output_height) > 180) {
                secondary_left_count += descriptor->output_width;
                if (secondary_left_count >
                    2U * descriptor->output_height) {
                    secondary_left_count =
                        2U * descriptor->output_height;
                }
            }
            if (secondary_left_count >
                sizeof(scratch->secondary_left) /
                    sizeof(scratch->secondary_left[0])) {
                secondary_left_count =
                    sizeof(scratch->secondary_left) /
                    sizeof(scratch->secondary_left[0]);
            }
            for (index = 0U; index < secondary_left_count; ++index) {
                size_t sample_y = block_y + index;
                if (sample_y >= plane->height) break;
                if (!plane_sample_reconstructed_u16(
                        plane, block_x - 1U, sample_y)) {
                    secondary_left_count = index;
                    break;
                }
                scratch->secondary_left[index] =
                    av2h_reconstructed_sample_load_u16(
                        plane->data, plane->stride_samples,
                        block_x - 1U, sample_y);
            }
        }
    }
    dc_value = count == 0U ? av2h_sample_midpoint_10bit() :
        descriptor->output_width != descriptor->output_height &&
        above_count != 0U && left_count != 0U ?
            dc_average_rectangular_u16(sum, count) :
            (uint16_t)((sum + count / 2U) / count);
    references.left = left_count == 0U ? 0 : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? 0 : scratch->above;
    references.above_count = above_count;
    references.secondary_left = secondary_left_count == 0U ?
        0 : scratch->secondary_left;
    references.secondary_left_count = secondary_left_count;
    references.secondary_above = secondary_above_count == 0U ?
        0 : scratch->secondary_above;
    references.secondary_above_count = secondary_above_count;
    references.has_secondary_above_left = (uint8_t)(
        secondary_above_count != 0U && secondary_left_count != 0U);
    references.secondary_above_left =
        references.has_secondary_above_left ?
            av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples,
                block_x - 1U, block_y - 1U) : 0U;
    references.has_above_left = (uint8_t)(
        block_x > mode->mrl_index && block_y > mode->mrl_index);
    references.above_left = references.has_above_left ?
        av2h_reconstructed_sample_load_u16(
            plane->data, plane->stride_samples,
            block_x - 1U - mode->mrl_index,
            block_y - 1U - mode->mrl_index) : 0U;
    if (mode->mrl_index != 0U && references.has_above_left) {
        size_t negative_index;
        references.negative_count = (uint8_t)(mode->mrl_index + 1U);
        for (negative_index = 1U;
             negative_index <= references.negative_count;
             ++negative_index) {
            references.above_negative[negative_index] =
                av2h_reconstructed_sample_load_u16(
                    plane->data, plane->stride_samples,
                    block_x - negative_index, above_reference_y);
            references.left_negative[negative_index] =
                av2h_reconstructed_sample_load_u16(
                    plane->data, plane->stride_samples,
                    block_x - 1U - mode->mrl_index,
                    block_y - (is_top_superblock_boundary ? 1U :
                        negative_index));
        }
    }
    for (index = 0U;
         index < (size_t)descriptor->output_width * descriptor->output_height;
         ++index) {
        scratch->predictor[index] = dc_value;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->enable_ibp != 0U &&
        mode->palette_size == 0U &&
        !(descriptor->output_width == 4U &&
          descriptor->output_height == 4U)) {
        static const uint8_t weights[5U][16U] = {
            { 96U },
            { 86U, 107U },
            { 77U, 90U, 102U, 115U },
            { 71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U },
            { 68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U,
              98U, 102U, 106U, 109U, 113U, 117U, 121U, 124U }
        };
        uint16_t row;
        uint16_t column;
        if (above_count >= descriptor->output_width) {
            uint16_t row_limit =
                (uint16_t)(descriptor->output_height >> 2U);
            uint16_t column_start =
                descriptor->output_width < descriptor->output_height &&
                left_count >= descriptor->output_height ?
                    (uint16_t)(descriptor->output_width >> 2U) : 0U;
            uint8_t weight_row = (uint8_t)(
                (descriptor->output_height == 4U ? 2U :
                 descriptor->output_height == 8U ? 3U :
                 descriptor->output_height == 16U ? 4U :
                 descriptor->output_height == 32U ? 5U : 6U) - 2U);
            for (row = 0U; row < row_limit; ++row) {
                unsigned int weight = weights[weight_row][row];
                for (column = column_start;
                     column < descriptor->output_width; ++column) {
                    size_t predictor_index =
                        (size_t)row * descriptor->output_width + column;
                    scratch->predictor[predictor_index] = (uint16_t)(
                        (scratch->above[column] * (128U - weight) +
                         scratch->predictor[predictor_index] * weight +
                         64U) >> 7U);
                }
            }
        }
        if (left_count >= descriptor->output_height) {
            uint16_t row_start =
                descriptor->output_width >= descriptor->output_height &&
                above_count >= descriptor->output_width ?
                    (uint16_t)(descriptor->output_height >> 2U) : 0U;
            uint16_t column_limit =
                (uint16_t)(descriptor->output_width >> 2U);
            uint8_t weight_row = (uint8_t)(
                (descriptor->output_width == 4U ? 2U :
                 descriptor->output_width == 8U ? 3U :
                 descriptor->output_width == 16U ? 4U :
                 descriptor->output_width == 32U ? 5U : 6U) - 2U);
            for (row = row_start; row < descriptor->output_height; ++row) {
                for (column = 0U; column < column_limit; ++column) {
                    unsigned int weight = weights[weight_row][column];
                    size_t predictor_index =
                        (size_t)row * descriptor->output_width + column;
                    scratch->predictor[predictor_index] = (uint16_t)(
                        (scratch->left[row] * (128U - weight) +
                         scratch->predictor[predictor_index] * weight +
                         64U) >> 7U);
                }
            }
        }
    }
    if (mode->use_intrabc != 0U) {
        status = intrabc_predict_u16(
            mode, plane, block_x, block_y,
            descriptor->output_width, descriptor->output_height,
            scratch->predictor);
        if (status != AV2H_OK) {
            av2h_reconstruction_diagnostics_set_luma_stage(24U);
            return status;
        }
    } else if (mode->palette_size != 0U) {
        status = palette_fill_predictor_u16(
            mode, block_x, block_y, descriptor->output_width,
            descriptor->output_height, scratch->predictor);
        if (status != AV2H_OK) {
            av2h_reconstruction_diagnostics_set_luma_stage(25U);
            return status;
        }
    } else if (top_vertical_multi_line_mrl != 0U) {
        uint16_t row;
        uint16_t column;
        for (row = 0U; row < descriptor->output_height; ++row) {
            for (column = 0U; column < descriptor->output_width; ++column) {
                scratch->predictor[
                    (size_t)row * descriptor->output_width + column] =
                    above_count != 0U && secondary_above_count != 0U ?
                        (uint16_t)((scratch->above[
                            column < above_count ? column : above_count - 1U] +
                            scratch->secondary_above[
                                column < secondary_above_count ? column :
                                secondary_above_count - 1U] + 1U) >> 1U) :
                        left_count != 0U ? scratch->left[0] : 511U;
            }
        }
    } else if (mode->mode != AV2H_INTRA_DC) {
        Av2hFirstLeafMode prediction_mode;
        av2h_memory_copy(&prediction_mode, mode, sizeof(prediction_mode));
        if (descriptor->output_width == 4U &&
            descriptor->output_height == 4U) {
            prediction_mode.enable_ibp = 0U;
        }
        status = av2h_luma_predict_u16(
            &prediction_mode,
            descriptor->output_width, descriptor->output_height,
            &references, scratch->predictor,
            descriptor->output_width);
        if (status != AV2H_OK) return status;
    }
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 &&
        mode->fsc_mode == 0U) {
        status = reconstruct_lossless_4x4_u16(
            coefficients, scratch->predictor, mode->use_dpcm_y,
            mode->dpcm_mode_y, 0U, 0U, &reconstruction);
    } else if (mode->use_dpcm_y != 0U && mode->fsc_mode != 0U) {
        uint16_t row;
        uint16_t column;
        if (coefficients->coefficient_count !=
                (size_t)descriptor->output_width *
                    descriptor->output_height ||
            coefficients->width != descriptor->output_width ||
            coefficients->height != descriptor->output_height) {
            return AV2H_UNSUPPORTED;
        }
        reconstruction.width = descriptor->output_width;
        reconstruction.height = descriptor->output_height;
        if (mode->dpcm_mode_y == 0U) {
            for (column = 0U; column < descriptor->output_width; ++column) {
                int32_t delay = 0;
                for (row = 0U; row < descriptor->output_height; ++row) {
                    size_t sample =
                        (size_t)row * descriptor->output_width + column;
                    int32_t current =
                        coefficients->dequantized[sample] / 8;
                    int32_t residual = current + delay;
                    reconstruction.residual[sample] = residual;
                    reconstruction.pixels[sample] = av2h_sample_clip_10bit(
                        (int32_t)scratch->predictor[sample] + residual);
                    delay += current;
                }
            }
        } else {
            for (row = 0U; row < descriptor->output_height; ++row) {
                int32_t delay = 0;
                for (column = 0U; column < descriptor->output_width;
                     ++column) {
                    size_t sample =
                        (size_t)row * descriptor->output_width + column;
                    int32_t current =
                        coefficients->dequantized[sample] / 8;
                    int32_t residual = current + delay;
                    reconstruction.residual[sample] = residual;
                    reconstruction.pixels[sample] = av2h_sample_clip_10bit(
                        (int32_t)scratch->predictor[sample] + residual);
                    delay += current;
                }
            }
        }
        status = AV2H_OK;
    } else {
    av2h_reconstruction_diagnostics_set_luma_stage(2U);
    status = av2h_reconstruct_transform_generic_u16(
        mode, prefix, transform, coefficients, scratch->predictor,
        descriptor->output_width,
        scratch, &reconstruction);
    }
    if (status != AV2H_OK) return status;
    av2h_reconstruction_diagnostics_set_luma_stage(3U);
    av2h_predictor_commit_u16(
        plane->data + (size_t)block_y * plane->stride_samples + block_x,
        plane->stride_samples, reconstruction.pixels,
        descriptor->output_width,
        visible_width, visible_height);
    plane_mark_reconstructed_u16(
        plane, block_x, block_y, visible_width, visible_height);
    return AV2H_OK;
}
Av2hStatus av2h_first_chroma_reconstruct(
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    Av2hFirstLeafMode mode;
    if (prefix == 0 || transform == 0 || coefficients == 0 || scratch == 0 ||
        reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->tx_size != AV2H_TX_4X4 ||
        transform->primary_type != AV2H_TX_DCT_DCT ||
        coefficients->width != 4U || coefficients->height != 4U) {
        return AV2H_UNSUPPORTED;
    }
    av2h_memory_fill(&mode, 0U, sizeof(mode));
    mode.mode = AV2H_INTRA_DC;
    return av2h_luma_reconstruct(&mode, prefix, transform, coefficients,
                                 0, 0U, scratch, reconstruction);
}

Av2hStatus av2h_chroma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch) {
    return av2h_chroma_reconstruct_to_plane_ex(
        leaf, mode, prefix, transform, coefficients, plane, 0, 1U, 0,
        scratch);
}

Av2hStatus av2h_chroma_reconstruct_to_plane_ex(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    uint8_t chroma_plane,
    Av2hReconstructionPlane *predictor_plane,
    Av2hReconstructionScratch *scratch) {
    return av2h_chroma_reconstruct_to_plane_format(
        leaf, mode, prefix, transform, coefficients, plane, luma_plane,
        chroma_plane, predictor_plane, 0U, scratch);
}

Av2hStatus av2h_chroma_inverse_cross_transform(
    Av2hFirstCoeffBlock *u_coefficients,
    Av2hFirstCoeffBlock *v_coefficients,
    uint8_t cctx_type,
    uint8_t bit_depth) {
    static const int32_t matrix[6U][2U] = {
        { 181, 181 }, { 222, 128 }, { 128, 222 },
        { 181, -181 }, { 222, -128 }, { 128, -222 }
    };
    size_t index;
    int32_t coefficient_limit;
    if (u_coefficients == 0 || v_coefficients == 0 || cctx_type > 6U ||
        (bit_depth != 8U && bit_depth != 10U) ||
        u_coefficients->coefficient_count !=
            v_coefficients->coefficient_count) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cctx_type == 0U) return AV2H_OK;
    coefficient_limit = 1 << (7U + bit_depth);
    u_coefficients->nonzero_count = 0U;
    v_coefficients->nonzero_count = 0U;
    for (index = 0U; index < u_coefficients->coefficient_count; ++index) {
        int32_t u = u_coefficients->dequantized[index];
        int32_t v = v_coefficients->dequantized[index];
        int32_t first = round_shift_signed(
            (int64_t)matrix[cctx_type - 1U][0U] * u -
                (int64_t)matrix[cctx_type - 1U][1U] * v,
            8U);
        int32_t second = round_shift_signed(
            (int64_t)matrix[cctx_type - 1U][1U] * u +
                (int64_t)matrix[cctx_type - 1U][0U] * v,
            8U);
        u_coefficients->dequantized[index] =
            clamp(first, -coefficient_limit, coefficient_limit - 1);
        v_coefficients->dequantized[index] =
            clamp(second, -coefficient_limit, coefficient_limit - 1);
        if (u_coefficients->dequantized[index] != 0) {
            ++u_coefficients->nonzero_count;
        }
        if (v_coefficients->dequantized[index] != 0) {
            ++v_coefficients->nonzero_count;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_chroma_reconstruct_to_plane_format(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    uint8_t chroma_plane,
    Av2hReconstructionPlane *predictor_plane,
    uint8_t chroma_format_idc,
    Av2hReconstructionScratch *scratch) {
    Av2hFirstLeafMode luma_mode;
    Av2hFirstCoeffPrefix effective_prefix;
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references;
    const Av2hTxDescriptor *descriptor;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t width;
    uint16_t height;
    uint16_t visible_width;
    uint16_t visible_height;
    uint8_t have_top;
    uint8_t have_left;
    size_t index;
    Av2hStatus status;
    av2h_reconstruction_diagnostics_set_chroma_stage(0U);
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        chroma_plane < 1U || chroma_plane > 2U ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        plane->stride_samples < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == 0 ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells < plane->reconstructed_stride_cells *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
    av2h_reconstruction_diagnostics_set_chroma_stage(1U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    width = descriptor->output_width > 32U ?
        descriptor->output_width : coefficients->width;
    height = descriptor->output_height > 32U ?
        descriptor->output_height : coefficients->height;
    block_x = (uint16_t)((uint32_t)leaf->column *
        (chroma_format_idc == 2U ? 4U : 2U));
    block_y = (uint16_t)((uint32_t)leaf->row *
        (chroma_format_idc == 2U ? 4U : 2U));
    if (block_x >= plane->width || block_y >= plane->height ||
        !av2h_chroma_geometry_supported(width, height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    visible_width = width < plane->width - block_x ?
        width : (uint16_t)(plane->width - block_x);
    visible_height = height < plane->height - block_y ?
        height : (uint16_t)(plane->height - block_y);
    if (predictor_plane != 0 &&
        (predictor_plane->data == 0 ||
         predictor_plane->stride_samples < predictor_plane->width ||
         block_x >= predictor_plane->width ||
         block_y >= predictor_plane->height ||
         visible_width > predictor_plane->width - block_x ||
         visible_height > predictor_plane->height - block_y)) {
        return AV2H_INVALID_ARGUMENT;
    }
    /*
     * Gather the reconstructed neighbour references.  Above extends across the
     * available reference width so directional chroma predictors can read
     * their diagonal; left spans the block height.  Readiness truncates at the
     * first sample not yet reconstructed (edge/crop and same-SB ordering).
     */
    {
        size_t above_count = block_y == 0U ||
            block_x >= plane->width ? 0U :
            (size_t)plane->width - block_x;
        size_t left_count = block_x == 0U ? 0U : height;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            width, height);
        if (width > 32U && above_count > width) above_count = width;
        if (left_count != 0U &&
            (mode->mode == AV2H_INTRA_SMOOTH ||
             mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
            ++left_count;
        }
        if (left_count != 0U && height <= 32U &&
            angle > 180 && angle < 270) {
            left_count += height;
        }
        if (above_count > sizeof(scratch->above)) {
            above_count = sizeof(scratch->above);
        }
        if (left_count > sizeof(scratch->left)) {
            left_count = sizeof(scratch->left);
        }
        for (index = 0U; index < above_count; ++index) {
                if (!av2h_plane_sample_reconstructed(
                    plane, block_x + index, (size_t)block_y - 1U)) {
                above_count = index;
                break;
            }
        }
        for (index = 0U; index < above_count; ++index) {
            scratch->above[index] = av2h_reconstructed_sample_load_u8(
                plane->data, plane->stride_samples,
                block_x + index, (size_t)block_y - 1U);
        }
        if (above_count != 0U && above_count < width) {
            uint8_t edge = scratch->above[above_count - 1U];
            while (above_count < width) {
                scratch->above[above_count++] = edge;
            }
        }
        for (index = 0U; index < left_count; ++index) {
            size_t sy = (size_t)block_y + index;
            if (sy >= plane->height) {
                sy = plane->height - 1U;
            }
            if (!av2h_plane_sample_reconstructed(
                    plane, (size_t)block_x - 1U, sy)) {
                left_count = index;
                break;
            }
        }
        for (index = 0U; index < left_count; ++index) {
            size_t sy = (size_t)block_y + index;
            if (sy >= plane->height) {
                sy = plane->height - 1U;
            }
            scratch->left[index] = av2h_reconstructed_sample_load_u8(
                plane->data, plane->stride_samples, block_x - 1U, sy);
        }
        references.left = left_count == 0U ? 0 : scratch->left;
        references.left_count = left_count;
        references.above = above_count == 0U ? 0 : scratch->above;
        references.above_count = above_count;
        have_top = above_count >= width ? 1U : 0U;
        have_left = left_count >= height ? 1U : 0U;
    }
    references.above_left = 0U;
    references.has_above_left = 0U;
    if (block_x != 0U && block_y != 0U &&
        av2h_plane_sample_reconstructed(
            plane, (size_t)block_x - 1U, (size_t)block_y - 1U)) {
        references.above_left = plane->data[
            ((size_t)block_y - 1U) * plane->stride_samples + block_x - 1U];
        references.has_above_left = 1U;
    }
    if (!mode->is_cfl && mode->mode != AV2H_INTRA_DC &&
        (references.above_count == 0U || references.left_count == 0U)) {
        size_t above_count = (size_t)width + height;
        size_t left_count = (size_t)height + width;
        uint8_t had_above = references.above_count != 0U;
        uint8_t had_left = references.left_count != 0U;
        uint8_t above_value = had_left ? references.left[0] :
            av2h_sample_below_midpoint_8bit();
        uint8_t left_value = had_above ? references.above[0] :
            av2h_sample_above_midpoint_8bit();
        if (above_count > sizeof(scratch->above)) {
            above_count = sizeof(scratch->above);
        }
        if (left_count > sizeof(scratch->left)) {
            left_count = sizeof(scratch->left);
        }
        if (!had_above) {
            av2h_memory_fill(scratch->above, above_value, above_count);
            references.above = scratch->above;
            references.above_count = above_count;
        }
        if (!had_left) {
            av2h_memory_fill(scratch->left, left_value, left_count);
            references.left = scratch->left;
            references.left_count = left_count;
        }
        references.above_left = had_above ? left_value :
            had_left ? above_value : av2h_sample_midpoint_8bit();
        references.has_above_left = 1U;
    }
    if (mode->is_cfl) {
        /* CfL bypasses IBP and starts from a flat chroma DC base. */
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t dc_value;
        size_t i;
        if (have_top) {
            size_t step = width > 32U ? 2U : 1U;
            for (i = 0U; i < width; i += step) sum += scratch->above[i];
            count += (unsigned int)((width + step - 1U) / step);
        }
        if (have_left) {
            size_t step = height > 32U ? 2U : 1U;
            for (i = 0U; i < height; i += step) sum += scratch->left[i];
            count += (unsigned int)((height + step - 1U) / step);
        }
        dc_value = av2h_chroma_dc_predictor_average(
            sum, count, width, height, have_top, have_left);
        for (index = 0U; index < (size_t)width * height; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->cfl_kind == AV2H_CFL_MULTI_PARAM) {
            status = av2h_mhccp_predict_observed(
                mode, luma_plane, plane, block_x, block_y,
                width, height,
                scratch->predictor, width);
        } else {
            uint8_t derive_alpha = (uint8_t)(
                mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA);
            int32_t alpha_q3 = 0;
            if (!derive_alpha) {
                uint8_t sign = chroma_plane == 1U ?
                    (uint8_t)(((mode->cfl_alpha_signs + 1U) * 11U) >> 5U) :
                    (uint8_t)(mode->cfl_alpha_signs + 1U - 3U *
                        (((mode->cfl_alpha_signs + 1U) * 11U) >> 5U));
                uint8_t magnitude = chroma_plane == 1U ?
                    (uint8_t)(mode->cfl_alpha_idx >> 3U) :
                    (uint8_t)(mode->cfl_alpha_idx & 7U);
                if (mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA ||
                    mode->cfl_alpha_signs >= 8U || sign > 2U) {
                    return AV2H_UNSUPPORTED;
                }
                alpha_q3 = sign == 0U ? 0 :
                    sign == 2U ? (int32_t)magnitude + 1 :
                    -(int32_t)magnitude - 1;
            }
            status = av2h_cfl_derived_predict(
                luma_plane, plane, mode->cfl_downsample_filter_index,
                derive_alpha, alpha_q3, mode->superblock_size,
                mode->chroma_format_idc, block_x,
                block_y, width, height,
                have_top, have_left,
                scratch->predictor, width);
        }
        if (status != AV2H_OK) return status;
        luma_mode.mode = AV2H_INTRA_DC;
    } else {
        status = av2h_chroma_predict(
            mode, width, height, &references, scratch->predictor, width);
        if (status != AV2H_OK) return status;
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
    }
    luma_mode.use_dpcm_y = mode->use_dpcm_uv;
    luma_mode.dpcm_mode_y = mode->dpcm_mode_uv;
    luma_mode.fsc_mode = transform->primary_type == AV2H_TX_IDTX ? 1U : 0U;
    av2h_reconstruction_diagnostics_set_chroma_stage(2U);
    if (predictor_plane != 0) {
        av2h_predictor_commit_u8(
            predictor_plane->data +
                (size_t)block_y * predictor_plane->stride_samples + block_x,
            predictor_plane->stride_samples,
            scratch->predictor, width, visible_width, visible_height);
    }
    /*
     * Predictor-only path: every all-zero chroma transform block (any
     * geometry, including TX_16X8 / TX_32X4 whose coefficient DCT is owned by
     * the parallel geometry work) reconstructs to its predictor with a zero
     * residual.  A stray coefficient on an all-zero block is rejected without
     * touching the plane.
     */
    if (prefix->all_zero && coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        av2h_predictor_commit_u8(
            plane->data + (size_t)block_y * plane->stride_samples + block_x,
            plane->stride_samples, scratch->predictor, width,
            visible_width, visible_height);
        plane_mark_reconstructed(
            plane, block_x, block_y, visible_width, visible_height);
        av2h_reconstruction_diagnostics_set_chroma_stage(3U);
        return AV2H_OK;
    }
    effective_prefix = *prefix;
    if (coefficients->nonzero_count != 0U) {
        effective_prefix.all_zero = 0U;
    }
    status = av2h_luma_reconstruct(
        &luma_mode, &effective_prefix, transform, coefficients,
        scratch->predictor,
        width, scratch, &reconstruction);
    if (status != AV2H_OK) {
        const Av2hReconstructionDiagnostics *diagnostics =
            av2h_reconstruction_diagnostics_get();
        av2h_reconstruction_diagnostics_set_chroma_stage((uint8_t)(
            10U + (diagnostics != 0 ?
                diagnostics->inverse_transform_stage : 0U)));
        return status;
    }
    av2h_predictor_commit_u8(
        plane->data + (size_t)block_y * plane->stride_samples + block_x,
        plane->stride_samples, reconstruction.pixels, width,
        visible_width, visible_height);
    plane_mark_reconstructed(
        plane, block_x, block_y, visible_width, visible_height);
    return AV2H_OK;
}

Av2hStatus av2h_chroma_reconstruct_to_plane_u16(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane,
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t chroma_plane,
    uint8_t chroma_format_idc,
    Av2hReconstructionScratchU16 *scratch) {
    Av2hFirstLeafMode luma_mode;
    Av2hFirstReconstructionU16 reconstruction;
    const Av2hTxDescriptor *descriptor;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t width;
    uint16_t height;
    uint16_t visible_width;
    uint16_t visible_height;
    size_t above_count = 0U;
    size_t above_target;
    size_t left_count = 0U;
    size_t left_target;
    size_t index;
    int prediction_angle;
    uint8_t apply_edge_ibp;
    Av2hStatus status;
    Av2hLumaReferencesU16 references = { 0 };
    av2h_reconstruction_diagnostics_set_chroma_stage(0U);
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        plane->reconstructed == 0 ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        chroma_plane < 1U || chroma_plane > 2U ||
        plane->stride_samples < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    width = descriptor->output_width;
    height = descriptor->output_height;
    block_x = (uint16_t)((uint32_t)leaf->column *
        (chroma_format_idc == 2U ? 4U : 2U));
    block_y = (uint16_t)((uint32_t)leaf->row *
        (chroma_format_idc == 2U ? 4U : 2U));
            if ((chroma_format_idc == 2U && mode->is_cfl != 0U &&
                mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA &&
                mode->cfl_kind != AV2H_CFL_DERIVED_ALPHA &&
                mode->cfl_kind != AV2H_CFL_MULTI_PARAM) ||
        (mode->is_cfl != 0U &&
                mode->cfl_kind != AV2H_CFL_MULTI_PARAM &&
                mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA &&
                mode->cfl_kind != AV2H_CFL_DERIVED_ALPHA) ||
        (mode->mode != AV2H_INTRA_DC &&
         mode->mode != AV2H_INTRA_VERTICAL &&
         mode->mode != AV2H_INTRA_HORIZONTAL &&
         mode->mode != AV2H_INTRA_SMOOTH &&
         mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
         mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
         mode->mode != AV2H_INTRA_PAETH &&
         mode->mode != AV2H_INTRA_D45 &&
         mode->mode != AV2H_INTRA_D67 &&
         mode->mode != AV2H_INTRA_D113 &&
         mode->mode != AV2H_INTRA_D135 &&
         mode->mode != AV2H_INTRA_D157 &&
         mode->mode != AV2H_INTRA_D203) ||
        !av2h_chroma_geometry_supported(width, height) ||
        block_x >= plane->width || block_y >= plane->height) {
        return AV2H_UNSUPPORTED;
    }
    visible_width = width < plane->width - block_x ?
        width : (uint16_t)(plane->width - block_x);
    visible_height = height < plane->height - block_y ?
        height : (uint16_t)(plane->height - block_y);
    prediction_angle = av2h_directional_base_angle(mode->mode) == 0 ? 0 :
        luma_wide_angle(
            av2h_directional_base_angle(mode->mode) +
                mode->angle_delta * 3,
            width, height);
    apply_edge_ibp = (uint8_t)(mode->edge_ibp != 0U &&
        (width != 4U || height != 4U));
    above_target = width;
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) &&
        block_x + above_target < plane->width) {
        ++above_target;
    }
    if (width <= 32U &&
        ((prediction_angle > 0 && prediction_angle < 90) ||
        (apply_edge_ibp != 0U &&
         prediction_angle > 180 && prediction_angle < 270))) {
        above_target += height;
        if (above_target > sizeof(scratch->above) /
                               sizeof(scratch->above[0])) {
            above_target = sizeof(scratch->above) /
                sizeof(scratch->above[0]);
        }
    }
    if (block_y != 0U) {
        for (index = 0U; index < above_target; ++index) {
            size_t sample_x = block_x + index;
            if (sample_x >= plane->width) sample_x = plane->width - 1U;
            if (!plane_sample_reconstructed_u16(
                    plane, sample_x, block_y - 1U)) {
                break;
            }
            scratch->above[above_count++] =
                av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples,
                sample_x, block_y - 1U);
        }
    }
    left_target = height;
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
        block_y + left_target < plane->height) {
        ++left_target;
    }
    if (height <= 32U &&
        ((prediction_angle > 180 && prediction_angle < 270) ||
        (apply_edge_ibp != 0U &&
         prediction_angle > 0 && prediction_angle < 90))) {
        left_target += width;
        if (left_target > sizeof(scratch->left) /
                              sizeof(scratch->left[0])) {
            left_target = sizeof(scratch->left) /
                sizeof(scratch->left[0]);
        }
    }
    if (block_x != 0U) {
        for (index = 0U; index < left_target; ++index) {
            size_t sample_y = block_y + index;
            if (sample_y >= plane->height) sample_y = plane->height - 1U;
            if (!plane_sample_reconstructed_u16(
                    plane, block_x - 1U, sample_y)) {
                break;
            }
            scratch->left[left_count++] =
                av2h_reconstructed_sample_load_u16(
                    plane->data, plane->stride_samples,
                    block_x - 1U, sample_y);
        }
    }
    av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
    luma_mode.mode = mode->mode;
    luma_mode.angle_delta = mode->angle_delta;
    luma_mode.edge_ibp = mode->edge_ibp;
    luma_mode.above_smooth = mode->above_smooth;
    luma_mode.left_smooth = mode->left_smooth;
    if (mode->is_cfl != 0U) {
        uint64_t sum = 0U;
        size_t count = 0U;
        uint8_t have_top = above_count >= width ? 1U : 0U;
        uint8_t have_left = left_count >= height ? 1U : 0U;
        uint16_t dc_value;
        if (have_top != 0U) {
            size_t step = width > 32U ? 2U : 1U;
            for (index = 0U; index < width; index += step) {
                sum += scratch->above[index];
                ++count;
            }
        }
        if (have_left != 0U) {
            size_t step = height > 32U ? 2U : 1U;
            for (index = 0U; index < height; index += step) {
                sum += scratch->left[index];
                ++count;
            }
        }
        dc_value = count == 0U ? av2h_sample_midpoint_10bit() :
            width != height && have_top != 0U && have_left != 0U ?
                dc_average_rectangular_u16(sum, count) :
                (uint16_t)((sum + count / 2U) / count);
        for (index = 0U; index < (size_t)width * height; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->cfl_kind == AV2H_CFL_MULTI_PARAM) {
            status = av2h_mhccp_predict_observed_u16(
                mode, luma_plane, plane, block_x, block_y, width, height,
                scratch->predictor, width);
        } else if (mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA) {
            status = chroma_format_idc == 2U ?
                av2h_cfl_derived_predict_u16_444(
                    luma_plane, plane, block_x, block_y, width, height,
                    have_top, have_left, scratch->predictor, width) :
                av2h_cfl_derived_predict_u16(
                    luma_plane, plane, mode->cfl_downsample_filter_index,
                    mode->superblock_size, block_x, block_y, width, height,
                    have_top, have_left, scratch->predictor, width);
        } else {
            uint8_t sign = chroma_plane == 1U ?
                (uint8_t)(((mode->cfl_alpha_signs + 1U) * 11U) >> 5U) :
                (uint8_t)(mode->cfl_alpha_signs + 1U - 3U *
                    (((mode->cfl_alpha_signs + 1U) * 11U) >> 5U));
            uint8_t magnitude = chroma_plane == 1U ?
                (uint8_t)(mode->cfl_alpha_idx >> 3U) :
                (uint8_t)(mode->cfl_alpha_idx & 7U);
            int32_t alpha_q3;
            if (mode->cfl_alpha_signs >= 8U || sign > 2U) {
                return AV2H_UNSUPPORTED;
            }
            alpha_q3 = sign == 0U ? 0 :
                sign == 2U ? (int32_t)magnitude + 1 :
                -(int32_t)magnitude - 1;
            status = chroma_format_idc == 2U ?
                av2h_cfl_explicit_predict_u16_444(
                    luma_plane, alpha_q3, block_x, block_y, width, height,
                    have_top, have_left, scratch->predictor, width) :
                av2h_cfl_explicit_predict_u16(
                    luma_plane, mode->cfl_downsample_filter_index,
                    alpha_q3, mode->superblock_size,
                    block_x, block_y, width, height,
                    have_top, have_left,
                    scratch->predictor, width);
        }
        if (status != AV2H_OK) return status;
    } else if (mode->mode == AV2H_INTRA_DC) {
        static const uint8_t weights[5U][16U] = {
            { 96U }, { 86U, 107U }, { 77U, 90U, 102U, 115U },
            { 71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U },
            { 68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U,
              98U, 102U, 106U, 109U, 113U, 117U, 121U, 124U }
        };
        uint64_t sum = 0U;
        size_t count = 0U;
        uint16_t row;
        uint16_t column;
        uint16_t dc_value;
        for (index = 0U; index < above_count; ++index) {
            sum += scratch->above[index];
            ++count;
        }
        for (index = 0U; index < left_count; ++index) {
            sum += scratch->left[index];
            ++count;
        }
        dc_value = count == 0U ? 512U :
            width != height && above_count != 0U && left_count != 0U ?
                dc_average_rectangular_u16(sum, count) :
                (uint16_t)((sum + count / 2U) / count);
        for (index = 0U; index < (size_t)width * height; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->edge_ibp != 0U &&
            !(width == 4U && height == 4U)) {
            if (above_count >= width) {
                uint16_t row_limit = (uint16_t)(height >> 2U);
                uint16_t column_start =
                    width < height && left_count >= height ?
                        (uint16_t)(width >> 2U) : 0U;
                uint8_t weight_row = (uint8_t)(
                    (height == 4U ? 2U :
                     height == 8U ? 3U :
                     height == 16U ? 4U :
                     height == 32U ? 5U : 6U) - 2U);
                for (row = 0U; row < row_limit; ++row) {
                    unsigned int weight = weights[weight_row][row];
                    for (column = column_start; column < width; ++column) {
                        size_t output = (size_t)row * width + column;
                        scratch->predictor[output] = (uint16_t)(
                            (scratch->above[column] * (128U - weight) +
                             scratch->predictor[output] * weight + 64U) >>
                            7U);
                    }
                }
            }
            if (left_count >= height) {
                uint16_t row_start = width >= height && above_count >= width ?
                    (uint16_t)(height >> 2U) : 0U;
                uint16_t column_limit = (uint16_t)(width >> 2U);
                uint8_t weight_row = (uint8_t)(
                    (width == 4U ? 2U :
                     width == 8U ? 3U :
                     width == 16U ? 4U :
                     width == 32U ? 5U : 6U) - 2U);
                for (row = row_start; row < height; ++row) {
                    for (column = 0U; column < column_limit; ++column) {
                        unsigned int weight = weights[weight_row][column];
                        size_t output = (size_t)row * width + column;
                        scratch->predictor[output] = (uint16_t)(
                            (scratch->left[row] * (128U - weight) +
                             scratch->predictor[output] * weight + 64U) >>
                            7U);
                    }
                }
            }
        }
    } else {
        references.has_above_left = (uint8_t)(
            block_x != 0U && block_y != 0U &&
            plane_sample_reconstructed_u16(
                plane, block_x - 1U, block_y - 1U));
        references.above_left = references.has_above_left ?
            av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples,
                block_x - 1U, block_y - 1U) : 0U;
        references.above = above_count == 0U ? 0 : scratch->above;
        references.above_count = above_count;
        references.left = left_count == 0U ? 0 : scratch->left;
        references.left_count = left_count;
        status = av2h_chroma_predict_u16(
            &luma_mode, width, height, &references,
            scratch->predictor, width);
        if (status != AV2H_OK) return status;
    }
    av2h_reconstruction_diagnostics_set_chroma_stage(2U);
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4) {
        status = reconstruct_lossless_4x4_u16(
            coefficients, scratch->predictor, mode->use_dpcm_uv,
            mode->dpcm_mode_uv,
            (uint8_t)(transform->primary_type == AV2H_TX_IDTX),
            mode->use_dpcm_uv,
            &reconstruction);
    } else {
        status = av2h_reconstruct_transform_generic_u16(
            &luma_mode, prefix, transform, coefficients,
            scratch->predictor, width, scratch, &reconstruction);
    }
    if (status != AV2H_OK) return status;
    av2h_predictor_commit_u16(
        plane->data + (size_t)block_y * plane->stride_samples + block_x,
        plane->stride_samples, reconstruction.pixels, width,
        visible_width, visible_height);
    plane_mark_reconstructed_u16(
        plane, block_x, block_y, visible_width, visible_height);
    av2h_reconstruction_diagnostics_set_chroma_stage(3U);
    return AV2H_OK;
}
