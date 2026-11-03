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

typedef struct {
    uint16_t width;
    uint16_t height;
} Av2hVisibleReconstruction;

static int plane_sample_inside_active_tile(const Av2hReconstructionPlane *plane, size_t x,
                                           size_t y);

static void plane_mark_reconstructed(Av2hReconstructionPlane *plane, uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height);

static Av2hStatus palette_fill_predictor(const Av2hLeafMode *mode, uint16_t block_x,
                                         uint16_t block_y, uint16_t output_width,
                                         uint16_t output_height, uint8_t *predictor);

static int plane_sample_inside_active_tile_u16(const Av2hReconstructionPlaneU16 *plane, size_t x,
                                               size_t y);

static void plane_mark_reconstructed_u16(Av2hReconstructionPlaneU16 *plane, uint16_t x, uint16_t y,
                                         uint16_t width, uint16_t height);

static Av2hStatus palette_fill_predictor_u16(const Av2hLeafMode *mode, uint16_t block_x,
                                             uint16_t block_y, uint16_t output_width,
                                             uint16_t output_height, uint16_t *predictor);

static Av2hVisibleReconstruction reconstruction_visible_block(uint16_t plane_width,
                                                              uint16_t plane_height,
                                                              uint16_t block_x, uint16_t block_y,
                                                              uint16_t width, uint16_t height) {
    Av2hVisibleReconstruction visible = {
        width < plane_width - block_x ? width : (uint16_t)(plane_width - block_x),
        height < plane_height - block_y ? height : (uint16_t)(plane_height - block_y)};
    return visible;
}

static int luma_top_superblock_boundary(const Av2hLeafMode *mode, uint16_t block_y) {
    return mode->mrl_index != 0U && mode->superblock_size != 0U &&
        block_y % mode->superblock_size == 0U;
}

static int luma_prediction_angle(const Av2hLeafMode *mode, uint16_t width, uint16_t height) {
    return av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
                                    (mode->mrl_index == 1U   ? 1
                                     : mode->mrl_index == 2U ? -1
                                                             : 0),
                                width, height);
}

static size_t reconstruction_reference_target(size_t base, uint16_t remaining,
                                              uint8_t extend_smooth, size_t directional_extension,
                                              size_t directional_limit) {
    size_t target = base;
    if (extend_smooth && target < remaining)
        ++target;
    if (directional_extension != 0U) {
        target += directional_extension;
        if (target > directional_limit)
            target = directional_limit;
    }
    return target;
}

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

Av2hStatus av2h_luma_reconstruct_to_plane(Av2hDecodeDiagnostics *diagnostics,
                                          const Av2hPartitionNode *leaf, const Av2hLeafMode *mode,
                                          const Av2hCoeffPrefix *prefix,
                                          const Av2hTransformType *transform,
                                          const Av2hCoeffBlock *coefficients,
                                          Av2hReconstructionPlane *plane,
                                          Av2hReconstructionScratch *scratch) {
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
    Av2hBlockReconstruction reconstruction;
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
    Av2hLeafMode effective_mode;
    if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
        coefficients == NULL || plane == NULL || scratch == NULL || plane->data == NULL ||
        plane->stride_samples < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == NULL ||
        plane->reference_width > plane->width || plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells <
            plane->reconstructed_stride_cells * (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 0U);
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    output_width = descriptor->output_width;
    output_height = descriptor->output_height;
    is_top_superblock_boundary = luma_top_superblock_boundary(mode, block_y);
    above_reference_y = (size_t)block_y - 1U - (is_top_superblock_boundary ? 0U : mode->mrl_index);
    if (block_x >= plane->width || block_y >= plane->height || coefficients->width > 32U ||
        coefficients->height > 32U) {
        return AV2H_INVALID_ARGUMENT;
    }
    {
        Av2hVisibleReconstruction visible = reconstruction_visible_block(
            plane->width, plane->height, block_x, block_y, output_width, output_height);
        visible_width = visible.width;
        visible_height = visible.height;
    }
    above_count =
        block_y <= mode->mrl_index || block_x >= plane->width ? 0U : (size_t)plane->width - block_x;
    if (mode->tx_extended_edges_unavailable != 0U && above_count > output_width) {
        above_count = output_width;
    }
    if (above_count > 2U * output_width) {
        above_count = 2U * output_width;
    }
    if (mode->mode == AV2H_INTRA_PAETH && above_count != 0U && above_count < output_width) {
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
        if (!av2h_plane_sample_reconstructed(plane, source_x, above_reference_y)) {
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
    if ((mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_DC) && above_count != 0U &&
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
    } else if (left_count != 0U && mode->tx_extended_edges_unavailable == 0U &&
               (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
               block_y + left_count < plane->height) {
        ++left_count;
    } else if (left_count != 0U && mode->mrl_index == 0U &&
               av2h_directional_base_angle(mode->mode) != 0) {
        int angle =
            av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3,
                                 output_width, output_height);
        if (mode->tx_extended_edges_unavailable == 0U &&
            (angle > 180 || (mode->enable_ibp != 0U && angle > 0 && angle < 90))) {
            left_count += output_width;
            if (left_count > 2U * output_height) {
                left_count = 2U * output_height;
            }
        }
    } else if (left_count != 0U && mode->mrl_index != 0U &&
               mode->tx_extended_edges_unavailable == 0U &&
               av2h_directional_base_angle(mode->mode) != 0 &&
               (luma_prediction_angle(mode, output_width, output_height) > 180 ||
                (mode->enable_ibp != 0U &&
                 luma_prediction_angle(mode, output_width, output_height) < 90))) {
        left_count += output_width + 2U * mode->mrl_index;
    } else if (left_count != 0U && mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 1 &&
               mode->mrl_index == 0U && output_width == 16U && output_height == 16U) {
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
        if (!av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U - mode->mrl_index,
                                             source_y)) {
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
            plane->data, plane->stride_samples, block_x - 1U - mode->mrl_index, source_y);
    }
    if ((mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
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
            secondary_above_count = (size_t)plane->width - block_x;
            if (mode->tx_extended_edges_unavailable != 0U && secondary_above_count > output_width) {
                secondary_above_count = output_width;
            }
            if (secondary_above_count > 2U * output_width) {
                secondary_above_count = 2U * output_width;
            }
            if (secondary_above_count > sizeof(scratch->secondary_above)) {
                secondary_above_count = sizeof(scratch->secondary_above);
            }
            for (index = 0U; index < secondary_above_count; ++index) {
                if (!av2h_plane_sample_reconstructed(plane, block_x + index,
                                                     (size_t)block_y - 1U)) {
                    secondary_above_count = index;
                    break;
                }
                scratch->secondary_above[index] = av2h_reconstructed_sample_load_u8(
                    plane->data, plane->stride_samples, block_x + index, (size_t)block_y - 1U);
            }
        }
        if (block_x != 0U) {
            secondary_left_count = output_height;
            if (mode->tx_extended_edges_unavailable == 0U &&
                av2h_directional_base_angle(mode->mode) != 0 &&
                luma_prediction_angle(mode, output_width, output_height) > 180) {
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
                if (!av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U, source_y)) {
                    secondary_left_count = index;
                    break;
                }
                scratch->secondary_left[index] = av2h_reconstructed_sample_load_u8(
                    plane->data, plane->stride_samples, block_x - 1U, source_y);
            }
        }
    }
    references.left = left_count == 0U ? NULL : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? NULL : scratch->above;
    references.above_count = above_count;
    references.secondary_left = secondary_left_count == 0U ? NULL : scratch->secondary_left;
    references.secondary_left_count = secondary_left_count;
    references.secondary_above = secondary_above_count == 0U ? NULL : scratch->secondary_above;
    references.secondary_above_count = secondary_above_count;
    references.has_secondary_above_left = secondary_above_count != 0U && secondary_left_count != 0U;
    references.secondary_above_left =
        references.has_secondary_above_left
            ? plane->data[((size_t)block_y - 1U) * plane->stride_samples + block_x - 1U]
            : 0U;
    references.has_above_left =
        block_x > mode->mrl_index && block_y > mode->mrl_index &&
        plane_sample_inside_active_tile(plane, (size_t)block_x - 1U - mode->mrl_index,
                                        (size_t)block_y - 1U - mode->mrl_index);
    references.above_left =
        references.has_above_left
            ? plane->data[((size_t)block_y - 1U - mode->mrl_index) * plane->stride_samples +
                          block_x - 1U - mode->mrl_index]
            : 0U;
    references.negative_count = 0U;
    if (mode->mrl_index != 0U && references.has_above_left) {
        size_t negative_index;
        references.negative_count = (uint8_t)(mode->mrl_index + 1U);
        for (negative_index = 1U; negative_index <= references.negative_count; ++negative_index) {
            references.above_negative[negative_index] =
                plane->data[above_reference_y * plane->stride_samples + block_x - negative_index];
            references.left_negative[negative_index] =
                plane->data[((size_t)block_y - (is_top_superblock_boundary ? 1U : negative_index)) *
                                plane->stride_samples +
                            block_x - 1U - mode->mrl_index];
        }
    }
    if (mode->palette_size != 0U) {
        status = palette_fill_predictor(mode, block_x, block_y, output_width, output_height,
                                        scratch->predictor);
        if (status != AV2H_OK)
            return status;
        status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                       scratch->predictor, output_width, scratch, &reconstruction);
    } else if (mode->use_intrabc) {
        status = av2h_intrabc_predict(diagnostics, mode, plane, block_x, block_y, output_width,
                                      output_height, scratch->predictor);
        if (status != AV2H_OK)
            return status;
        status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                       scratch->predictor, output_width, scratch, &reconstruction);
    } else if (mode->mode == AV2H_INTRA_DC && above_count == 0U && left_count == 0U &&
               mode->use_intra_dip == 0U) {
        for (index = 0U; index < (size_t)output_width * output_height; ++index) {
            scratch->predictor[index] = domain->midpoint;
        }
        status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                       scratch->predictor, output_width, scratch, &reconstruction);
    } else {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 1U);
        av2h_memory_copy(&effective_mode, mode, sizeof(effective_mode));
        if (prefix->tx_size == AV2H_TX_4X4) {
            effective_mode.enable_ibp = 0U;
        }
        status = av2h_luma_predict(&effective_mode, output_width, output_height, &references,
                                   scratch->predictor, output_width);
        if (status != AV2H_OK)
            return status;
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 2U);
        status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                       scratch->predictor, output_width, scratch, &reconstruction);
    }
    if (status != AV2H_OK)
        return status;
    av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 3U);
    if (reconstruction.width != output_width || reconstruction.height != output_height) {
        return AV2H_UNSUPPORTED;
    }
    av2h_predictor_commit_u8(plane->data + (size_t)block_y * plane->stride_samples + block_x,
                             plane->stride_samples, reconstruction.pixels, output_width,
                             visible_width, visible_height);
    plane_mark_reconstructed(plane, block_x, block_y, visible_width, visible_height);
    return AV2H_OK;
}
Av2hStatus
av2h_chroma_reconstruct_to_plane(Av2hDecodeDiagnostics *diagnostics, const Av2hPartitionNode *leaf,
                                 const Av2hChromaLeafMode *mode, const Av2hCoeffPrefix *prefix,
                                 const Av2hTransformType *transform,
                                 const Av2hCoeffBlock *coefficients, Av2hReconstructionPlane *plane,
                                 const Av2hReconstructionPlane *luma_plane, uint8_t chroma_plane,
                                 Av2hReconstructionPlane *predictor_plane,
                                 uint8_t chroma_format_idc, Av2hReconstructionScratch *scratch) {
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
    Av2hBlockReconstruction reconstruction;
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
    av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 0U);
    if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
        coefficients == NULL || plane == NULL || scratch == NULL || plane->data == NULL ||
        chroma_plane < 1U || chroma_plane > 2U ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        plane->stride_samples < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == NULL ||
        plane->reference_width > plane->width || plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells <
            plane->reconstructed_stride_cells * (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 1U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    width = descriptor->output_width > 32U ? descriptor->output_width : coefficients->width;
    height = descriptor->output_height > 32U ? descriptor->output_height : coefficients->height;
    block_x = (uint16_t)((uint32_t)leaf->column * (chroma_format_idc == 2U ? 4U : 2U));
    block_y = (uint16_t)((uint32_t)leaf->row * (chroma_format_idc == 2U ? 4U : 2U));
    if (block_x >= plane->width || block_y >= plane->height ||
        !av2h_chroma_geometry_supported(width, height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    {
        Av2hVisibleReconstruction visible = reconstruction_visible_block(
            plane->width, plane->height, block_x, block_y, width, height);
        visible_width = visible.width;
        visible_height = visible.height;
    }
    if (predictor_plane != NULL &&
        (predictor_plane->data == NULL ||
         predictor_plane->stride_samples < predictor_plane->width ||
         block_x >= predictor_plane->width || block_y >= predictor_plane->height ||
         visible_width > predictor_plane->width - block_x ||
         visible_height > predictor_plane->height - block_y)) {
        return AV2H_INVALID_ARGUMENT;
    }
    {
        size_t above_count =
            block_y == 0U || block_x >= plane->width ? 0U : (size_t)plane->width - block_x;
        size_t left_count = block_x == 0U ? 0U : height;
        int angle = av2h_luma_wide_angle(
            av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3, width, height);
        if (width > 32U && above_count > width)
            above_count = width;
        if (left_count != 0U && height <= 32U &&
            (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
            ++left_count;
        }
        if (left_count != 0U && height <= 32U && angle > 180 && angle < 270) {
            left_count += height;
        }
        if (above_count > sizeof(scratch->above)) {
            above_count = sizeof(scratch->above);
        }
        if (left_count > sizeof(scratch->left)) {
            left_count = sizeof(scratch->left);
        }
        for (index = 0U; index < above_count; ++index) {
            if (!av2h_plane_sample_reconstructed(plane, block_x + index, (size_t)block_y - 1U)) {
                above_count = index;
                break;
            }
        }
        for (index = 0U; index < above_count; ++index) {
            scratch->above[index] = av2h_reconstructed_sample_load_u8(
                plane->data, plane->stride_samples, block_x + index, (size_t)block_y - 1U);
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
            if (!av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U, sy)) {
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
        references.left = left_count == 0U ? NULL : scratch->left;
        references.left_count = left_count;
        references.above = above_count == 0U ? NULL : scratch->above;
        references.above_count = above_count;
        have_top = above_count >= width ? 1U : 0U;
        have_left = left_count >= height ? 1U : 0U;
    }
    references.above_left = 0U;
    references.has_above_left = 0U;
    if (block_x != 0U && block_y != 0U &&
        av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U, (size_t)block_y - 1U)) {
        references.above_left =
            plane->data[((size_t)block_y - 1U) * plane->stride_samples + block_x - 1U];
        references.has_above_left = 1U;
    }
    if (!mode->is_cfl && mode->mode != AV2H_INTRA_DC &&
        (references.above_count == 0U || references.left_count == 0U)) {
        size_t above_count = (size_t)width + height;
        size_t left_count = (size_t)height + width;
        uint8_t had_above = references.above_count != 0U;
        uint8_t had_left = references.left_count != 0U;
        uint8_t above_value = had_left ? references.left[0] : (domain->midpoint - 1U);
        uint8_t left_value = had_above ? references.above[0] : (domain->midpoint + 1U);
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
        references.above_left = had_above ? left_value : had_left ? above_value : domain->midpoint;
        references.has_above_left = 1U;
    }
    if (mode->is_cfl) {
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t dc_value;
        size_t i;
        if (have_top) {
            size_t step = width > 32U ? 2U : 1U;
            for (i = 0U; i < width; i += step)
                sum += scratch->above[i];
            count += (unsigned int)((width + step - 1U) / step);
        }
        if (have_left) {
            size_t step = height > 32U ? 2U : 1U;
            for (i = 0U; i < height; i += step)
                sum += scratch->left[i];
            count += (unsigned int)((height + step - 1U) / step);
        }
        dc_value = av2h_chroma_dc_predictor_average(sum, count, width, height, have_top, have_left);
        for (index = 0U; index < (size_t)width * height; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->cfl_kind == AV2H_CFL_MULTI_PARAM) {
            status = av2h_mhccp_predict_observed(diagnostics, mode, luma_plane, plane, block_x,
                                                 block_y, width, height, scratch->predictor, width);
        } else {
            int32_t alpha_q3 = 0;
            uint8_t derive_alpha = (uint8_t)(mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA);
            if (!derive_alpha) {
                if (mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA)
                    return AV2H_UNSUPPORTED;
                status = cfl_explicit_alpha_q3(mode, chroma_plane, &alpha_q3);
                if (status != AV2H_OK)
                    return status;
            }
            status = av2h_cfl_derived_predict(
                luma_plane, plane, mode->cfl_downsample_filter_index, derive_alpha, alpha_q3,
                mode->superblock_size, mode->chroma_format_idc, block_x, block_y, width, height,
                have_top, have_left, scratch->predictor, width);
        }
        if (status != AV2H_OK)
            return status;
    } else {
        status = av2h_chroma_predict(mode, width, height, &references, scratch->predictor, width);
        if (status != AV2H_OK)
            return status;
    }
    av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 2U);
    if (predictor_plane != NULL) {
        av2h_predictor_commit_u8(predictor_plane->data +
                                     (size_t)block_y * predictor_plane->stride_samples + block_x,
                                 predictor_plane->stride_samples, scratch->predictor, width,
                                 visible_width, visible_height);
    }
    status = av2h_chroma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                     scratch->predictor, width, height, scratch, &reconstruction);
    if (status != AV2H_OK) {
        if (prefix->all_zero && coefficients->nonzero_count == 0U) {
            return status;
        }
        const Av2hReconstructionDiagnostics *snapshot =
            av2h_reconstruction_diagnostics_get(diagnostics);
        av2h_reconstruction_diagnostics_set_chroma_stage(
            diagnostics,
            (uint8_t)(10U + (snapshot != NULL ? snapshot->inverse_transform_stage : 0U)));
        return status;
    }
    av2h_predictor_commit_u8(plane->data + (size_t)block_y * plane->stride_samples + block_x,
                             plane->stride_samples, reconstruction.pixels, width, visible_width,
                             visible_height);
    plane_mark_reconstructed(plane, block_x, block_y, visible_width, visible_height);
    if (prefix->all_zero && coefficients->nonzero_count == 0U) {
        av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 3U);
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_reconstruct_to_plane_u16(
    Av2hDecodeDiagnostics *diagnostics, const Av2hPartitionNode *leaf, const Av2hLeafMode *mode,
    const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients, Av2hReconstructionPlaneU16 *plane,
    Av2hReconstructionScratchU16 *scratch, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    Av2hBlockReconstructionU16 reconstruction;
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
    Av2hLumaReferencesU16 references = {.left = NULL};
    uint16_t dc_value;
    uint8_t is_top_superblock_boundary;
    uint8_t top_vertical_multi_line_mrl;
    uint8_t zone_one_mrl;
    uint8_t directional_mrl;
    uint8_t directional_multi_line_mrl;
    int prediction_angle;
    Av2hStatus status;
    if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
        coefficients == NULL || plane == NULL || scratch == NULL || plane->data == NULL ||
        plane->reconstructed == NULL || plane->stride_samples < plane->width ||
        plane->reference_width == 0U || plane->reference_height == 0U ||
        plane->reference_width > plane->width || plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells <
            plane->reconstructed_stride_cells * (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 0U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    if (block_x >= plane->width || block_y >= plane->height) {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 20U);
        return AV2H_UNSUPPORTED;
    }
    prediction_angle =
        luma_prediction_angle(mode, descriptor->output_width, descriptor->output_height);
    if (mode->mode != AV2H_INTRA_DC && mode->mode != AV2H_INTRA_VERTICAL &&
        mode->mode != AV2H_INTRA_HORIZONTAL && mode->mode != AV2H_INTRA_SMOOTH &&
        mode->mode != AV2H_INTRA_SMOOTH_VERTICAL && mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
        mode->mode != AV2H_INTRA_PAETH && mode->mode != AV2H_INTRA_D45 &&
        mode->mode != AV2H_INTRA_D135 && mode->mode != AV2H_INTRA_D113 &&
        mode->mode != AV2H_INTRA_D67 && mode->mode != AV2H_INTRA_D157 &&
        mode->mode != AV2H_INTRA_D203) {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 21U);
        return AV2H_UNSUPPORTED;
    }
    if (mode->angle_delta != 0 && av2h_directional_base_angle(mode->mode) == 0) {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 22U);
        return AV2H_UNSUPPORTED;
    }
    top_vertical_multi_line_mrl =
        (uint8_t)(mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
                  mode->mrl_index == 3U && mode->multi_line_mrl == 1U && mode->enable_ibp <= 1U &&
                  block_y > mode->mrl_index && descriptor->output_width == 8U &&
                  descriptor->output_height == 8U);
    zone_one_mrl = (uint8_t)(mode->mrl_index <= 2U && mode->multi_line_mrl == 0U &&
                             prediction_angle > 0 && prediction_angle < 90);
    directional_mrl = (uint8_t)(av2h_directional_base_angle(mode->mode) != 0 &&
                                mode->mrl_index <= 3U && mode->multi_line_mrl == 0U);
    directional_multi_line_mrl =
        (uint8_t)(av2h_directional_base_angle(mode->mode) != 0 && mode->mrl_index > 0U &&
                  mode->mrl_index <= 3U && mode->multi_line_mrl == 1U);
    if ((mode->mrl_index != 0U || mode->multi_line_mrl != 0U) &&
        top_vertical_multi_line_mrl == 0U && zone_one_mrl == 0U && directional_mrl == 0U &&
        directional_multi_line_mrl == 0U) {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 23U);
        return AV2H_UNSUPPORTED;
    }
    {
        Av2hVisibleReconstruction visible =
            reconstruction_visible_block(plane->width, plane->height, block_x, block_y,
                                         descriptor->output_width, descriptor->output_height);
        visible_width = visible.width;
        visible_height = visible.height;
    }
    above_target = reconstruction_reference_target(
        descriptor->output_width, (uint16_t)(plane->width - block_x),
        mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL,
        prediction_angle > 0 && prediction_angle < 90      ? descriptor->output_width
        : mode->enable_ibp != 0U && prediction_angle > 180 ? descriptor->output_height
                                                           : 0U,
        2U * descriptor->output_width);
    if (above_target > sizeof(scratch->above) / sizeof(scratch->above[0])) {
        above_target = sizeof(scratch->above) / sizeof(scratch->above[0]);
    }
    if ((mode->mode == AV2H_INTRA_D67 && mode->angle_delta == 0) ||
        (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == -2) || zone_one_mrl != 0U) {
        above_target = (size_t)plane->width - block_x;
        if (above_target > sizeof(scratch->above) / sizeof(scratch->above[0])) {
            above_target = sizeof(scratch->above) / sizeof(scratch->above[0]);
        }
    }
    is_top_superblock_boundary = luma_top_superblock_boundary(mode, block_y);
    above_reference_y = (size_t)block_y - 1U - (is_top_superblock_boundary ? 0U : mode->mrl_index);
    if (block_y != 0U) {
        for (index = 0U; index < above_target; ++index) {
            size_t sample_x = block_x + index;
            if (sample_x >= plane->width) {
                if (av2h_directional_base_angle(mode->mode) != 0)
                    break;
                sample_x = plane->width - 1U;
            }
            if (!plane_sample_reconstructed_u16(plane, sample_x, above_reference_y)) {
                break;
            }
            scratch->above[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, sample_x, above_reference_y);
            sum += scratch->above[index];
            ++count;
            ++above_count;
        }
    }
    if (top_vertical_multi_line_mrl != 0U) {
        for (index = 0U; index < descriptor->output_width; ++index) {
            if (!plane_sample_reconstructed_u16(plane, block_x + index, block_y - 1U)) {
                break;
            }
            scratch->secondary_above[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, block_x + index, block_y - 1U);
            ++secondary_above_count;
        }
    }
    left_target = reconstruction_reference_target(
        descriptor->output_height, (uint16_t)(plane->height - block_y),
        mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL,
        av2h_directional_base_angle(mode->mode) != 0 &&
                (prediction_angle > 180 || (mode->enable_ibp != 0U && prediction_angle < 90))
            ? descriptor->output_width + 2U * mode->mrl_index
            : 0U,
        2U * descriptor->output_height);
    if (left_target > sizeof(scratch->left) / sizeof(scratch->left[0])) {
        left_target = sizeof(scratch->left) / sizeof(scratch->left[0]);
    }
    if (block_x > mode->mrl_index) {
        for (index = 0U; index < left_target; ++index) {
            size_t sample_y = block_y + index;
            if (sample_y >= plane->height) {
                if (av2h_directional_base_angle(mode->mode) != 0)
                    break;
                sample_y = plane->height - 1U;
            }
            if (!plane_sample_reconstructed_u16(plane, block_x - 1U - mode->mrl_index, sample_y)) {
                break;
            }
            scratch->left[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, block_x - 1U - mode->mrl_index, sample_y);
            sum += scratch->left[index];
            ++count;
            ++left_count;
        }
    }
    if (directional_multi_line_mrl != 0U) {
        if (block_y != 0U) {
            secondary_above_count = (size_t)plane->width - block_x;
            if (secondary_above_count >
                sizeof(scratch->secondary_above) / sizeof(scratch->secondary_above[0])) {
                secondary_above_count =
                    sizeof(scratch->secondary_above) / sizeof(scratch->secondary_above[0]);
            }
            for (index = 0U; index < secondary_above_count; ++index) {
                if (!plane_sample_reconstructed_u16(plane, block_x + index, block_y - 1U)) {
                    secondary_above_count = index;
                    break;
                }
                scratch->secondary_above[index] = av2h_reconstructed_sample_load_u16(
                    plane->data, plane->stride_samples, block_x + index, block_y - 1U);
            }
        }
        if (block_x != 0U) {
            secondary_left_count = descriptor->output_height;
            if (luma_prediction_angle(mode, descriptor->output_width, descriptor->output_height) >
                180) {
                secondary_left_count += descriptor->output_width;
                if (secondary_left_count > 2U * descriptor->output_height) {
                    secondary_left_count = 2U * descriptor->output_height;
                }
            }
            if (secondary_left_count >
                sizeof(scratch->secondary_left) / sizeof(scratch->secondary_left[0])) {
                secondary_left_count =
                    sizeof(scratch->secondary_left) / sizeof(scratch->secondary_left[0]);
            }
            for (index = 0U; index < secondary_left_count; ++index) {
                size_t sample_y = block_y + index;
                if (sample_y >= plane->height)
                    break;
                if (!plane_sample_reconstructed_u16(plane, block_x - 1U, sample_y)) {
                    secondary_left_count = index;
                    break;
                }
                scratch->secondary_left[index] = av2h_reconstructed_sample_load_u16(
                    plane->data, plane->stride_samples, block_x - 1U, sample_y);
            }
        }
    }
    dc_value = count == 0U ? domain->midpoint
               : descriptor->output_width != descriptor->output_height && above_count != 0U &&
                       left_count != 0U
                   ? dc_average_rectangular_u16(sum, count)
                   : (uint16_t)((sum + count / 2U) / count);
    references.left = left_count == 0U ? NULL : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? NULL : scratch->above;
    references.above_count = above_count;
    references.secondary_left = secondary_left_count == 0U ? NULL : scratch->secondary_left;
    references.secondary_left_count = secondary_left_count;
    references.secondary_above = secondary_above_count == 0U ? NULL : scratch->secondary_above;
    references.secondary_above_count = secondary_above_count;
    references.has_secondary_above_left =
        (uint8_t)(secondary_above_count != 0U && secondary_left_count != 0U);
    references.secondary_above_left =
        references.has_secondary_above_left
            ? av2h_reconstructed_sample_load_u16(plane->data, plane->stride_samples, block_x - 1U,
                                                 block_y - 1U)
            : 0U;
    references.has_above_left =
        (uint8_t)(block_x > mode->mrl_index && block_y > mode->mrl_index &&
                  plane_sample_inside_active_tile_u16(plane, (size_t)block_x - 1U - mode->mrl_index,
                                                      (size_t)block_y - 1U - mode->mrl_index));
    references.above_left =
        references.has_above_left
            ? av2h_reconstructed_sample_load_u16(plane->data, plane->stride_samples,
                                                 block_x - 1U - mode->mrl_index,
                                                 block_y - 1U - mode->mrl_index)
            : 0U;
    if (mode->mrl_index != 0U && references.has_above_left) {
        size_t negative_index;
        references.negative_count = (uint8_t)(mode->mrl_index + 1U);
        for (negative_index = 1U; negative_index <= references.negative_count; ++negative_index) {
            references.above_negative[negative_index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, block_x - negative_index, above_reference_y);
            references.left_negative[negative_index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, block_x - 1U - mode->mrl_index,
                block_y - (is_top_superblock_boundary ? 1U : negative_index));
        }
    }
    for (index = 0U; index < (size_t)descriptor->output_width * descriptor->output_height;
         ++index) {
        scratch->predictor[index] = dc_value;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->enable_ibp != 0U && mode->palette_size == 0U &&
        !(descriptor->output_width == 4U && descriptor->output_height == 4U)) {
        static const uint8_t weights[5U][16U] = {{96U},
                                                 {86U, 107U},
                                                 {77U, 90U, 102U, 115U},
                                                 {71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U},
                                                 {68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U, 98U, 102U,
                                                  106U, 109U, 113U, 117U, 121U, 124U}};
        uint16_t row;
        uint16_t column;
        if (above_count >= descriptor->output_width) {
            uint16_t row_limit = (uint16_t)(descriptor->output_height >> 2U);
            uint16_t column_start = descriptor->output_width < descriptor->output_height &&
                                            left_count >= descriptor->output_height
                                        ? (uint16_t)(descriptor->output_width >> 2U)
                                        : 0U;
            uint8_t weight_row = (uint8_t)((descriptor->output_height == 4U    ? 2U
                                            : descriptor->output_height == 8U  ? 3U
                                            : descriptor->output_height == 16U ? 4U
                                            : descriptor->output_height == 32U ? 5U
                                                                               : 6U) -
                                           2U);
            for (row = 0U; row < row_limit; ++row) {
                unsigned int weight = weights[weight_row][row];
                for (column = column_start; column < descriptor->output_width; ++column) {
                    size_t predictor_index = (size_t)row * descriptor->output_width + column;
                    scratch->predictor[predictor_index] =
                        (uint16_t)((scratch->above[column] * (128U - weight) +
                                    scratch->predictor[predictor_index] * weight + 64U) >>
                                   7U);
                }
            }
        }
        if (left_count >= descriptor->output_height) {
            uint16_t row_start = descriptor->output_width >= descriptor->output_height &&
                                         above_count >= descriptor->output_width
                                     ? (uint16_t)(descriptor->output_height >> 2U)
                                     : 0U;
            uint16_t column_limit = (uint16_t)(descriptor->output_width >> 2U);
            uint8_t weight_row = (uint8_t)((descriptor->output_width == 4U    ? 2U
                                            : descriptor->output_width == 8U  ? 3U
                                            : descriptor->output_width == 16U ? 4U
                                            : descriptor->output_width == 32U ? 5U
                                                                              : 6U) -
                                           2U);
            for (row = row_start; row < descriptor->output_height; ++row) {
                for (column = 0U; column < column_limit; ++column) {
                    unsigned int weight = weights[weight_row][column];
                    size_t predictor_index = (size_t)row * descriptor->output_width + column;
                    scratch->predictor[predictor_index] =
                        (uint16_t)((scratch->left[row] * (128U - weight) +
                                    scratch->predictor[predictor_index] * weight + 64U) >>
                                   7U);
                }
            }
        }
    }
    if (mode->use_intrabc != 0U) {
        status = intrabc_predict_u16(diagnostics, mode, plane, block_x, block_y,
                                     descriptor->output_width, descriptor->output_height,
                                     scratch->predictor, domain);
        if (status != AV2H_OK) {
            av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 24U);
            return status;
        }
    } else if (mode->palette_size != 0U) {
        status = palette_fill_predictor_u16(mode, block_x, block_y, descriptor->output_width,
                                            descriptor->output_height, scratch->predictor);
        if (status != AV2H_OK) {
            av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 25U);
            return status;
        }
    } else if (top_vertical_multi_line_mrl != 0U) {
        uint16_t row;
        uint16_t column;
        for (row = 0U; row < descriptor->output_height; ++row) {
            for (column = 0U; column < descriptor->output_width; ++column) {
                scratch->predictor[(size_t)row * descriptor->output_width + column] =
                    above_count != 0U && secondary_above_count != 0U
                        ? (uint16_t)((scratch->above[column < above_count ? column
                                                                          : above_count - 1U] +
                                      scratch->secondary_above[column < secondary_above_count
                                                                   ? column
                                                                   : secondary_above_count - 1U] +
                                      1U) >>
                                     1U)
                    : left_count != 0U ? scratch->left[0]
                                       : (domain->midpoint - 1U);
            }
        }
    } else if (mode->mode != AV2H_INTRA_DC) {
        Av2hLeafMode prediction_mode;
        av2h_memory_copy(&prediction_mode, mode, sizeof(prediction_mode));
        if (descriptor->output_width == 4U && descriptor->output_height == 4U) {
            prediction_mode.enable_ibp = 0U;
        }
        status = av2h_luma_predict_u16(&prediction_mode, descriptor->output_width,
                                       descriptor->output_height, &references, scratch->predictor,
                                       descriptor->output_width, domain);
        if (status != AV2H_OK)
            return status;
    }
    status = av2h_luma_reconstruct_u16(diagnostics, mode, prefix, transform, coefficients,
                                       scratch->predictor, scratch, &reconstruction, domain);
    if (status != AV2H_OK)
        return status;
    av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 3U);
    av2h_predictor_commit_u16(plane->data + (size_t)block_y * plane->stride_samples + block_x,
                              plane->stride_samples, reconstruction.pixels,
                              descriptor->output_width, visible_width, visible_height);
    plane_mark_reconstructed_u16(plane, block_x, block_y, visible_width, visible_height);
    return AV2H_OK;
}
Av2hStatus av2h_chroma_reconstruct_to_plane_u16(
    Av2hDecodeDiagnostics *diagnostics, const Av2hPartitionNode *leaf,
    const Av2hChromaLeafMode *mode, const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform, const Av2hCoeffBlock *coefficients,
    Av2hReconstructionPlaneU16 *plane, const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t chroma_plane, uint8_t chroma_format_idc, Av2hReconstructionScratchU16 *scratch,
    const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    Av2hLeafMode luma_mode;
    Av2hBlockReconstructionU16 reconstruction;
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
    Av2hLumaReferencesU16 references = {.left = NULL};
    av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 0U);
    if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
        coefficients == NULL || plane == NULL || scratch == NULL || plane->data == NULL ||
        plane->reconstructed == NULL || (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        chroma_plane < 1U || chroma_plane > 2U || plane->stride_samples < plane->width ||
        plane->reference_width == 0U || plane->reference_height == 0U ||
        plane->reference_width > plane->width || plane->reference_height > plane->height) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    width = descriptor->output_width;
    height = descriptor->output_height;
    block_x = (uint16_t)((uint32_t)leaf->column * (chroma_format_idc == 2U ? 4U : 2U));
    block_y = (uint16_t)((uint32_t)leaf->row * (chroma_format_idc == 2U ? 4U : 2U));
    if ((chroma_format_idc == 2U && mode->is_cfl != 0U &&
         mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA && mode->cfl_kind != AV2H_CFL_DERIVED_ALPHA &&
         mode->cfl_kind != AV2H_CFL_MULTI_PARAM) ||
        (mode->is_cfl != 0U && mode->cfl_kind != AV2H_CFL_MULTI_PARAM &&
         mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA && mode->cfl_kind != AV2H_CFL_DERIVED_ALPHA) ||
        (mode->mode != AV2H_INTRA_DC && mode->mode != AV2H_INTRA_VERTICAL &&
         mode->mode != AV2H_INTRA_HORIZONTAL && mode->mode != AV2H_INTRA_SMOOTH &&
         mode->mode != AV2H_INTRA_SMOOTH_VERTICAL && mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
         mode->mode != AV2H_INTRA_PAETH && mode->mode != AV2H_INTRA_D45 &&
         mode->mode != AV2H_INTRA_D67 && mode->mode != AV2H_INTRA_D113 &&
         mode->mode != AV2H_INTRA_D135 && mode->mode != AV2H_INTRA_D157 &&
         mode->mode != AV2H_INTRA_D203) ||
        !av2h_chroma_geometry_supported(width, height) || block_x >= plane->width ||
        block_y >= plane->height) {
        return AV2H_UNSUPPORTED;
    }
    {
        Av2hVisibleReconstruction visible = reconstruction_visible_block(
            plane->width, plane->height, block_x, block_y, width, height);
        visible_width = visible.width;
        visible_height = visible.height;
    }
    prediction_angle =
        av2h_directional_base_angle(mode->mode) == 0
            ? 0
            : av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3,
                                   width, height);
    apply_edge_ibp = (uint8_t)(mode->edge_ibp != 0U && (width != 4U || height != 4U));
    above_target = reconstruction_reference_target(
        width, (uint16_t)(plane->width - block_x),
        width <= 32U &&
            (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL),
        width <= 32U && ((prediction_angle > 0 && prediction_angle < 90) ||
                         (apply_edge_ibp != 0U && prediction_angle > 180 && prediction_angle < 270))
            ? height
            : 0U,
        sizeof(scratch->above) / sizeof(scratch->above[0]));
    if (block_y != 0U) {
        for (index = 0U; index < above_target; ++index) {
            size_t sample_x = block_x + index;
            if (sample_x >= plane->width)
                sample_x = plane->width - 1U;
            if (!plane_sample_reconstructed_u16(plane, sample_x, block_y - 1U)) {
                break;
            }
            scratch->above[above_count++] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, sample_x, block_y - 1U);
        }
    }
    left_target = reconstruction_reference_target(
        height, (uint16_t)(plane->height - block_y),
        height <= 32U &&
            (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL),
        height <= 32U && ((prediction_angle > 180 && prediction_angle < 270) ||
                          (apply_edge_ibp != 0U && prediction_angle > 0 && prediction_angle < 90))
            ? width
            : 0U,
        sizeof(scratch->left) / sizeof(scratch->left[0]));
    if (block_x != 0U) {
        for (index = 0U; index < left_target; ++index) {
            size_t sample_y = block_y + index;
            if (sample_y >= plane->height)
                sample_y = plane->height - 1U;
            if (!plane_sample_reconstructed_u16(plane, block_x - 1U, sample_y)) {
                break;
            }
            scratch->left[left_count++] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, block_x - 1U, sample_y);
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
        dc_value = count == 0U ? domain->midpoint
                   : width != height && have_top != 0U && have_left != 0U
                       ? dc_average_rectangular_u16(sum, count)
                       : (uint16_t)((sum + count / 2U) / count);
        for (index = 0U; index < (size_t)width * height; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->cfl_kind == AV2H_CFL_MULTI_PARAM) {
            status =
                av2h_mhccp_predict_observed_u16(mode, luma_plane, plane, block_x, block_y, width,
                                                height, scratch->predictor, width, domain);
        } else if (mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA) {
            status = chroma_format_idc == 2U
                         ? av2h_cfl_derived_predict_u16_444(luma_plane, plane, block_x, block_y,
                                                            width, height, have_top, have_left,
                                                            scratch->predictor, width, domain)
                         : av2h_cfl_derived_predict_u16(
                               luma_plane, plane, mode->cfl_downsample_filter_index,
                               mode->superblock_size, block_x, block_y, width, height, have_top,
                               have_left, scratch->predictor, width, domain);
        } else {
            int32_t alpha_q3 = 0;
            status = cfl_explicit_alpha_q3(mode, chroma_plane, &alpha_q3);
            if (status != AV2H_OK)
                return status;
            status = chroma_format_idc == 2U
                         ? av2h_cfl_explicit_predict_u16_444(luma_plane, alpha_q3, block_x, block_y,
                                                             width, height, have_top, have_left,
                                                             scratch->predictor, width, domain)
                         : av2h_cfl_explicit_predict_u16(
                               luma_plane, mode->cfl_downsample_filter_index, alpha_q3,
                               mode->superblock_size, block_x, block_y, width, height, have_top,
                               have_left, scratch->predictor, width, domain);
        }
        if (status != AV2H_OK)
            return status;
    } else if (mode->mode == AV2H_INTRA_DC) {
        static const uint8_t weights[5U][16U] = {{96U},
                                                 {86U, 107U},
                                                 {77U, 90U, 102U, 115U},
                                                 {71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U},
                                                 {68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U, 98U, 102U,
                                                  106U, 109U, 113U, 117U, 121U, 124U}};
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
        dc_value = count == 0U ? domain->midpoint
                   : width != height && above_count != 0U && left_count != 0U
                       ? dc_average_rectangular_u16(sum, count)
                       : (uint16_t)((sum + count / 2U) / count);
        for (index = 0U; index < (size_t)width * height; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->edge_ibp != 0U && !(width == 4U && height == 4U)) {
            if (above_count >= width) {
                uint16_t row_limit = (uint16_t)(height >> 2U);
                uint16_t column_start =
                    width < height && left_count >= height ? (uint16_t)(width >> 2U) : 0U;
                uint8_t weight_row = (uint8_t)((height == 4U    ? 2U
                                                : height == 8U  ? 3U
                                                : height == 16U ? 4U
                                                : height == 32U ? 5U
                                                                : 6U) -
                                               2U);
                for (row = 0U; row < row_limit; ++row) {
                    unsigned int weight = weights[weight_row][row];
                    for (column = column_start; column < width; ++column) {
                        size_t output = (size_t)row * width + column;
                        scratch->predictor[output] =
                            (uint16_t)((scratch->above[column] * (128U - weight) +
                                        scratch->predictor[output] * weight + 64U) >>
                                       7U);
                    }
                }
            }
            if (left_count >= height) {
                uint16_t row_start =
                    width >= height && above_count >= width ? (uint16_t)(height >> 2U) : 0U;
                uint16_t column_limit = (uint16_t)(width >> 2U);
                uint8_t weight_row = (uint8_t)((width == 4U    ? 2U
                                                : width == 8U  ? 3U
                                                : width == 16U ? 4U
                                                : width == 32U ? 5U
                                                               : 6U) -
                                               2U);
                for (row = row_start; row < height; ++row) {
                    for (column = 0U; column < column_limit; ++column) {
                        unsigned int weight = weights[weight_row][column];
                        size_t output = (size_t)row * width + column;
                        scratch->predictor[output] =
                            (uint16_t)((scratch->left[row] * (128U - weight) +
                                        scratch->predictor[output] * weight + 64U) >>
                                       7U);
                    }
                }
            }
        }
    } else {
        references.has_above_left =
            (uint8_t)(block_x != 0U && block_y != 0U &&
                      plane_sample_reconstructed_u16(plane, block_x - 1U, block_y - 1U));
        references.above_left =
            references.has_above_left
                ? av2h_reconstructed_sample_load_u16(plane->data, plane->stride_samples,
                                                     block_x - 1U, block_y - 1U)
                : 0U;
        references.above = above_count == 0U ? NULL : scratch->above;
        references.above_count = above_count;
        references.left = left_count == 0U ? NULL : scratch->left;
        references.left_count = left_count;
        status = av2h_chroma_predict_u16(&luma_mode, width, height, &references, scratch->predictor,
                                         width, domain);
        if (status != AV2H_OK)
            return status;
    }
    av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 2U);
    status = av2h_chroma_reconstruct_u16(diagnostics, mode, prefix, transform, coefficients,
                                         scratch->predictor, scratch, &reconstruction, domain);
    if (status != AV2H_OK)
        return status;
    av2h_predictor_commit_u16(plane->data + (size_t)block_y * plane->stride_samples + block_x,
                              plane->stride_samples, reconstruction.pixels, width, visible_width,
                              visible_height);
    plane_mark_reconstructed_u16(plane, block_x, block_y, visible_width, visible_height);
    av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 3U);
    return AV2H_OK;
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/reconstruct_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/reconstruct_kernel.inc"
#include "internal/sample_kernel_end.inc"
