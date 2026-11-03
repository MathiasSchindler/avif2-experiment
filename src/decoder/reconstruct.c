#include "reconstruct.h"

#include "internal/chroma_prediction.h"
#include "internal/decode_diagnostics.h"
#include "internal/intra_prediction.h"
#include "internal/intrabc_prediction.h"
#include "internal/reconstruct_block.h"
#include "internal/reconstruct_modules.h"
#include "internal/sample_math.h"
#include "internal/sample_ops.h"
#include "prediction.h"
#include "tx_geometry.h"

#include <stddef.h>

static Av2hStatus cfl_explicit_alpha_q3(const Av2hChromaLeafMode *mode, uint8_t chroma_plane,
                                        int32_t *alpha_q3) {
    uint8_t sign = chroma_plane == 1U
                       ? (uint8_t)(((mode->cfl_alpha_signs + 1U) * 11U) >> 5U)
                       : (uint8_t)(mode->cfl_alpha_signs + 1U -
                                   3U * (((mode->cfl_alpha_signs + 1U) * 11U) >> 5U));
    uint8_t magnitude = chroma_plane == 1U ? (uint8_t)(mode->cfl_alpha_idx >> 3U)
                                           : (uint8_t)(mode->cfl_alpha_idx & 7U);
    if (mode->cfl_alpha_signs >= 8U || sign > 2U)
        return AV2H_UNSUPPORTED;
    *alpha_q3 = sign == 0U ? 0 : sign == 2U ? (int32_t)magnitude + 1 : -(int32_t)magnitude - 1;
    return AV2H_OK;
}

const Av2hReconstructionDiagnostics *av2h_reconstruction_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return diagnostics != NULL ? &diagnostics->reconstruction : NULL;
#else
    (void)diagnostics;
    return NULL;
#endif
}

void av2h_reconstruction_diagnostics_set_luma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) diagnostics->reconstruction.luma_stage = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}

void av2h_reconstruction_diagnostics_set_intrabc_source(
    Av2hDecodeDiagnostics *diagnostics,
    uint16_t x, uint16_t y, size_t map_index) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) {
        diagnostics->reconstruction.intrabc_source_x = x;
        diagnostics->reconstruction.intrabc_source_y = y;
        diagnostics->reconstruction.intrabc_map_index = map_index;
    }
#else
    (void)diagnostics;
    (void)x;
    (void)y;
    (void)map_index;
#endif
}
void av2h_reconstruction_diagnostics_set_chroma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) diagnostics->reconstruction.chroma_stage = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}

void av2h_reconstruction_diagnostics_set_inverse_transform_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) {
        diagnostics->reconstruction.inverse_transform_stage = value;
    }
#else
    (void)diagnostics;
    (void)value;
#endif
}
static int plane_sample_reconstructed_cells(
    const uint8_t *reconstructed, size_t stride, size_t count,
    size_t x, size_t y) {
    size_t index = (y / 4U) * stride + x / 4U;
    return index < count &&
        reconstructed[index] != 0U && reconstructed[index] != 2U;
}

static int plane_sample_inside_active_tile_cells(
    const uint8_t *reconstructed, size_t stride, size_t count,
    size_t x, size_t y) {
    size_t index = (y / 4U) * stride + x / 4U;
    return index < count && reconstructed[index] != 2U;
}

int av2h_plane_sample_reconstructed(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    return plane_sample_reconstructed_cells(
        plane->reconstructed, plane->reconstructed_stride_cells,
        plane->reconstructed_count_cells, x, y);
}

static void plane_mark_reconstructed_cells(
    uint8_t *reconstructed, size_t stride, uint16_t x, uint16_t y,
    uint16_t width, uint16_t height) {
    size_t first_column = x / 4U;
    size_t first_row = y / 4U;
    size_t last_column = ((size_t)x + width - 1U) / 4U;
    size_t last_row = ((size_t)y + height - 1U) / 4U;
    size_t row;
    size_t column;
    for (row = first_row; row <= last_row; ++row) {
        for (column = first_column; column <= last_column; ++column) {
            reconstructed[row * stride + column] = 1U;
        }
    }
}

static int plane_sample_reconstructed_u16(
    const Av2hReconstructionPlaneU16 *plane, size_t x, size_t y) {
    return plane_sample_reconstructed_cells(
        plane->reconstructed, plane->reconstructed_stride_cells,
        plane->reconstructed_count_cells, x, y);
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

static Av2hStatus intrabc_predict_u16(Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode,
                                      const Av2hReconstructionPlaneU16 *plane, uint16_t block_x,
                                      uint16_t block_y, uint16_t width, uint16_t height,
                                      uint16_t *predictor, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
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
                    diagnostics,
                    (uint16_t)sample_x, (uint16_t)sample_y,
                    (sample_y / 4U) * plane->reconstructed_stride_cells +
                        sample_x / 4U);
                return AV2H_UNSUPPORTED;
            }
        }
    }
    status = av2h_intrabc_convolve(NULL, plane->data, plane->stride_samples, source_x, source_y,
                                   subpel_x, subpel_y, width, height, domain->bit_depth, NULL,
                                   predictor);
    if (status != AV2H_OK) return status;
    if (mode->morph_pred != 0U) {
        uint16_t coding_x = (uint16_t)(mode->block_origin_column * 4U);
        uint16_t coding_y = (uint16_t)(mode->block_origin_row * 4U);
        status =
            av2h_intrabc_morph_u16(plane->data, plane->stride_samples, plane->width, plane->height,
                                   coding_x, coding_y, mode->block_width, mode->block_height,
                                   (int32_t)coding_x + av2h_intrabc_full_pel(mode->displacement_x),
                                   (int32_t)coding_y + av2h_intrabc_full_pel(mode->displacement_y),
                                   width, height, domain->bit_depth, predictor);
        if (status != AV2H_OK) return status;
    }
    return AV2H_OK;
}

static Av2hStatus palette_map_origin(
    const Av2hLeafMode *mode,
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

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/reconstruct_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/reconstruct_kernel.inc"
#include "internal/sample_kernel_end.inc"
