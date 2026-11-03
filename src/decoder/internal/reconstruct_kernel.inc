static int AV2H_KERNEL_NAME(plane_sample_inside_active_tile, plane_sample_inside_active_tile_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * plane, size_t x,
    size_t y) {
    return plane_sample_inside_active_tile_cells(plane->reconstructed,
                                                 plane->reconstructed_stride_cells,
                                                 plane->reconstructed_count_cells, x, y);
}
static void AV2H_KERNEL_NAME(plane_mark_reconstructed, plane_mark_reconstructed_u16)(
    AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * plane, uint16_t x,
    uint16_t y, uint16_t width, uint16_t height) {
    plane_mark_reconstructed_cells(plane->reconstructed, plane->reconstructed_stride_cells, x, y,
                                   width, height);
}
static Av2hStatus AV2H_KERNEL_NAME(palette_fill_predictor, palette_fill_predictor_u16)(
    const Av2hLeafMode *mode, uint16_t block_x, uint16_t block_y, uint16_t output_width,
    uint16_t output_height, AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor) {
    size_t offset_x;
    size_t offset_y;
    size_t row;
    size_t column;
    Av2hStatus status = palette_map_origin(mode, block_x, block_y, output_width, output_height,
                                           &offset_x, &offset_y);
    if (status != AV2H_OK)
        return status;
    for (row = 0U; row < output_height; ++row) {
        for (column = 0U; column < output_width; ++column) {
            uint8_t color_index = av2h_palette_map_get(
                mode, (offset_y + row) * mode->palette_width + offset_x + column);
            if (color_index >= mode->palette_size)
                return AV2H_INVALID_DATA;
#if AV2H_KERNEL_WIDE
            predictor[row * (size_t)output_width + column] = mode->palette_colors[color_index];
#else
            predictor[row * (size_t)output_width + column] =
                (uint8_t)mode->palette_colors[color_index];
#endif
        }
    }
    return AV2H_OK;
}
Av2hStatus AV2H_KERNEL_NAME(av2h_luma_reconstruct_to_plane, av2h_luma_reconstruct_to_plane_u16)(
    Av2hDecodeDiagnostics *diagnostics, const Av2hPartitionNode *leaf, const Av2hLeafMode *mode,
    const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients,
    AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * plane,
    AV2H_KERNEL_TYPE(Av2hReconstructionScratch, Av2hReconstructionScratchU16) * scratch
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    AV2H_KERNEL_TYPE(Av2hBlockReconstruction, Av2hBlockReconstructionU16) reconstruction;
#if !AV2H_KERNEL_WIDE
    Av2hLumaReferences references;
#endif
    const Av2hTxDescriptor *descriptor;
    uint16_t block_x;
    uint16_t block_y;
#if !AV2H_KERNEL_WIDE
    uint16_t output_width;
    uint16_t output_height;
#endif
    uint16_t visible_width;
    uint16_t visible_height;
#if AV2H_KERNEL_WIDE
    size_t index;
    uint64_t sum = 0U;
    size_t count = 0U;
    size_t above_count = 0U;
    size_t above_target;
    size_t left_count = 0U;
    size_t left_target;
#else
    size_t above_count;
    size_t left_count;
#endif
    size_t secondary_above_count = 0U;
    size_t secondary_left_count = 0U;
    size_t above_reference_y;
#if AV2H_KERNEL_WIDE
    Av2hLumaReferencesU16 references = {.left = NULL};
    uint16_t dc_value;
#endif
    uint8_t is_top_superblock_boundary;
#if AV2H_KERNEL_WIDE
    uint8_t top_vertical_multi_line_mrl;
    uint8_t zone_one_mrl;
    uint8_t directional_mrl;
    uint8_t directional_multi_line_mrl;
    int prediction_angle;
#else
    size_t index;
#endif
    Av2hStatus status;
#if AV2H_KERNEL_WIDE
    if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
        coefficients == NULL || plane == NULL || scratch == NULL || plane->data == NULL ||
        plane->reconstructed == NULL || plane->stride_samples < plane->width ||
        plane->reference_width == 0U || plane->reference_height == 0U ||
        plane->reference_width > plane->width || plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells <
            plane->reconstructed_stride_cells * (((size_t)plane->height + 3U) / 4U)) {
#else
    Av2hLeafMode effective_mode;
    if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
        coefficients == NULL || plane == NULL || scratch == NULL || plane->data == NULL ||
        plane->stride_samples < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == NULL ||
        plane->reference_width > plane->width || plane->reference_height > plane->height ||
        plane->reconstructed_stride_cells < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count_cells <
            plane->reconstructed_stride_cells * (((size_t)plane->height + 3U) / 4U)) {
#endif
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 0U);
#if !AV2H_KERNEL_WIDE
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
#endif
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
#if AV2H_KERNEL_WIDE
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    if (block_x >= plane->width || block_y >= plane->height) {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 20U);
        return AV2H_UNSUPPORTED;
    }
    prediction_angle =
        av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
                                 (mode->mrl_index == 1U   ? 1
                                  : mode->mrl_index == 2U ? -1
                                                          : 0),
                             descriptor->output_width, descriptor->output_height);
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
    visible_width = descriptor->output_width < plane->width - block_x
                        ? descriptor->output_width
                        : (uint16_t)(plane->width - block_x);
    visible_height = descriptor->output_height < plane->height - block_y
                         ? descriptor->output_height
                         : (uint16_t)(plane->height - block_y);
    above_target = descriptor->output_width;
    if ((mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) &&
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
#else
    output_width = descriptor->output_width;
    output_height = descriptor->output_height;
#endif
    is_top_superblock_boundary = (uint8_t)(mode->mrl_index != 0U && mode->superblock_size != 0U &&
                                           block_y % mode->superblock_size == 0U);
    above_reference_y = (size_t)block_y - 1U - (is_top_superblock_boundary ? 0U : mode->mrl_index);
#if AV2H_KERNEL_WIDE
    if (block_y != 0U) {
        for (index = 0U; index < above_target; ++index) {
            size_t sample_x = block_x + index;
            if (sample_x >= plane->width) {
                if (av2h_directional_base_angle(mode->mode) != 0)
                    break;
                sample_x = plane->width - 1U;
#else
    if (block_x >= plane->width || block_y >= plane->height || coefficients->width > 32U ||
        coefficients->height > 32U) {
        return AV2H_INVALID_ARGUMENT;
#endif
            }
#if AV2H_KERNEL_WIDE
            if (!plane_sample_reconstructed_u16(plane, sample_x, above_reference_y)) {
#else
    visible_width =
        output_width < plane->width - block_x ? output_width : (uint16_t)(plane->width - block_x);
    visible_height = output_height < plane->height - block_y ? output_height
                                                             : (uint16_t)(plane->height - block_y);
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
#endif
                break;
            }
#if AV2H_KERNEL_WIDE
            scratch->above[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, sample_x, above_reference_y);
            sum += scratch->above[index];
            ++count;
            ++above_count;
#else
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
#endif
        }
    }
#if AV2H_KERNEL_WIDE
    if (top_vertical_multi_line_mrl != 0U) {
        for (index = 0U; index < descriptor->output_width; ++index) {
            if (!plane_sample_reconstructed_u16(plane, block_x + index, block_y - 1U)) {
#else
    } else if (left_count != 0U && mode->mrl_index != 0U &&
               mode->tx_extended_edges_unavailable == 0U &&
               av2h_directional_base_angle(mode->mode) != 0 &&
               (av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) +
                                         mode->angle_delta * 3 +
                                         (mode->mrl_index == 1U   ? 1
                                          : mode->mrl_index == 2U ? -1
                                                                  : 0),
                                     output_width, output_height) > 180 ||
                (mode->enable_ibp != 0U &&
                 av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) +
                                          mode->angle_delta * 3 +
                                          (mode->mrl_index == 1U   ? 1
                                           : mode->mrl_index == 2U ? -1
                                                                   : 0),
                                      output_width, output_height) < 90))) {
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
#endif
                break;
            }
#if AV2H_KERNEL_WIDE
            scratch->secondary_above[index] = av2h_reconstructed_sample_load_u16(
                plane->data, plane->stride_samples, block_x + index, block_y - 1U);
            ++secondary_above_count;
#endif
        }
#if !AV2H_KERNEL_WIDE
        for (index = 0U; index < left_count; ++index) {
            size_t source_y = (size_t)block_y + index;
            if (source_y >= plane->height) {
                source_y = plane->height - 1U;
#endif
            }
#if AV2H_KERNEL_WIDE
            left_target = descriptor->output_height;
            if ((mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
                block_y + left_target < plane->height) {
                ++left_target;
#else
scratch->left[index] = av2h_reconstructed_sample_load_u8(plane->data, plane->stride_samples,
                                                         block_x - 1U - mode->mrl_index, source_y);
#endif
            }
#if AV2H_KERNEL_WIDE
            if (av2h_directional_base_angle(mode->mode) != 0) {
                if (prediction_angle > 180 || (mode->enable_ibp != 0U && prediction_angle < 90)) {
                    left_target += descriptor->output_width + 2U * mode->mrl_index;
                    if (left_target > 2U * descriptor->output_height) {
                        left_target = 2U * descriptor->output_height;
#else
if ((mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
     mode->mode == AV2H_INTRA_PAETH) &&
    left_count != 0U && left_count < output_height) {
    uint8_t edge = scratch->left[left_count - 1U];
    for (index = left_count; index < output_height; ++index) {
        scratch->left[index] = edge;
#endif
                    }
#if !AV2H_KERNEL_WIDE
                    left_count = output_height;
#endif
                }
#if AV2H_KERNEL_WIDE
            }
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
                    if (!plane_sample_reconstructed_u16(plane, block_x - 1U - mode->mrl_index,
                                                        sample_y)) {
                        break;
                    }
                    scratch->left[index] = av2h_reconstructed_sample_load_u16(
                        plane->data, plane->stride_samples, block_x - 1U - mode->mrl_index,
                        sample_y);
                    sum += scratch->left[index];
                    ++count;
                    ++left_count;
                }
            }
            if (directional_multi_line_mrl != 0U) {
#else
if (mode->mrl_index != 0U && mode->multi_line_mrl != 0U) {
#endif
                if (block_y != 0U) {
                    secondary_above_count = (size_t)plane->width - block_x;
#if AV2H_KERNEL_WIDE
                    if (secondary_above_count >
                        sizeof(scratch->secondary_above) / sizeof(scratch->secondary_above[0])) {
                        secondary_above_count =
                            sizeof(scratch->secondary_above) / sizeof(scratch->secondary_above[0]);
#else
        if (mode->tx_extended_edges_unavailable != 0U && secondary_above_count > output_width) {
            secondary_above_count = output_width;
        }
        if (secondary_above_count > 2U * output_width) {
            secondary_above_count = 2U * output_width;
        }
        if (secondary_above_count > sizeof(scratch->secondary_above)) {
            secondary_above_count = sizeof(scratch->secondary_above);
#endif
                    }
                    for (index = 0U; index < secondary_above_count; ++index) {
#if AV2H_KERNEL_WIDE
                        if (!plane_sample_reconstructed_u16(plane, block_x + index, block_y - 1U)) {
#else
            if (!av2h_plane_sample_reconstructed(plane, block_x + index, (size_t)block_y - 1U)) {
#endif
                            secondary_above_count = index;
                            break;
                        }
#if AV2H_KERNEL_WIDE
                        scratch->secondary_above[index] = av2h_reconstructed_sample_load_u16(
                            plane->data, plane->stride_samples, block_x + index, block_y - 1U);
#else
            scratch->secondary_above[index] = av2h_reconstructed_sample_load_u8(
                plane->data, plane->stride_samples, block_x + index, (size_t)block_y - 1U);
#endif
                    }
                }
                if (block_x != 0U) {
#if AV2H_KERNEL_WIDE
                    secondary_left_count = descriptor->output_height;
                    if (av2h_luma_wide_angle(
                            av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
                                (mode->mrl_index == 1U   ? 1
                                 : mode->mrl_index == 2U ? -1
#else
        secondary_left_count = output_height;
        if (mode->tx_extended_edges_unavailable == 0U &&
            av2h_directional_base_angle(mode->mode) != 0 &&
            av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) + mode->angle_delta * 3 +
                                     (mode->mrl_index == 1U   ? 1
                                      : mode->mrl_index == 2U ? -1
#endif
                                                         : 0),
#if AV2H_KERNEL_WIDE
                            descriptor->output_width, descriptor->output_height) > 180) {
                        secondary_left_count += descriptor->output_width;
                        if (secondary_left_count > 2U * descriptor->output_height) {
                            secondary_left_count = 2U * descriptor->output_height;
#else
                                 output_width, output_height) > 180) {
            secondary_left_count += output_width;
            if (secondary_left_count > 2U * output_height) {
                secondary_left_count = 2U * output_height;
#endif
                        }
                    }
#if AV2H_KERNEL_WIDE
                    if (secondary_left_count >
                        sizeof(scratch->secondary_left) / sizeof(scratch->secondary_left[0])) {
                        secondary_left_count =
                            sizeof(scratch->secondary_left) / sizeof(scratch->secondary_left[0]);
#else
        if (secondary_left_count > sizeof(scratch->secondary_left)) {
            secondary_left_count = sizeof(scratch->secondary_left);
#endif
                    }
                    for (index = 0U; index < secondary_left_count; ++index) {
#if AV2H_KERNEL_WIDE
                        size_t sample_y = block_y + index;
                        if (sample_y >= plane->height)
                            break;
                        if (!plane_sample_reconstructed_u16(plane, block_x - 1U, sample_y)) {
#else
            size_t source_y = (size_t)block_y + index;
            if (source_y >= plane->height) {
                source_y = plane->height - 1U;
            }
            if (!av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U, source_y)) {
#endif
                            secondary_left_count = index;
                            break;
                        }
#if AV2H_KERNEL_WIDE
                        scratch->secondary_left[index] = av2h_reconstructed_sample_load_u16(
                            plane->data, plane->stride_samples, block_x - 1U, sample_y);
#else
            scratch->secondary_left[index] = av2h_reconstructed_sample_load_u8(
                plane->data, plane->stride_samples, block_x - 1U, source_y);
#endif
                    }
                }
            }
#if AV2H_KERNEL_WIDE
            dc_value = count == 0U ? domain->midpoint
                       : descriptor->output_width != descriptor->output_height &&
                               above_count != 0U && left_count != 0U
                           ? dc_average_rectangular_u16(sum, count)
                           : (uint16_t)((sum + count / 2U) / count);
#endif
            references.left = left_count == 0U ? NULL : scratch->left;
            references.left_count = left_count;
            references.above = above_count == 0U ? NULL : scratch->above;
            references.above_count = above_count;
            references.secondary_left = secondary_left_count == 0U ? NULL : scratch->secondary_left;
            references.secondary_left_count = secondary_left_count;
            references.secondary_above =
                secondary_above_count == 0U ? NULL : scratch->secondary_above;
            references.secondary_above_count = secondary_above_count;
#if AV2H_KERNEL_WIDE
            references.has_secondary_above_left =
                (uint8_t)(secondary_above_count != 0U && secondary_left_count != 0U);
            references.secondary_above_left =
                references.has_secondary_above_left
                    ? av2h_reconstructed_sample_load_u16(plane->data, plane->stride_samples,
                                                         block_x - 1U, block_y - 1U)
                    : 0U;
            references.has_above_left =
                (uint8_t)(block_x > mode->mrl_index && block_y > mode->mrl_index &&
                          plane_sample_inside_active_tile_u16(
                              plane, (size_t)block_x - 1U - mode->mrl_index,
                              (size_t)block_y - 1U - mode->mrl_index));
            references.above_left =
                references.has_above_left
                    ? av2h_reconstructed_sample_load_u16(plane->data, plane->stride_samples,
                                                         block_x - 1U - mode->mrl_index,
                                                         block_y - 1U - mode->mrl_index)
                    : 0U;
#else
references.has_secondary_above_left = secondary_above_count != 0U && secondary_left_count != 0U;
references.secondary_above_left =
    references.has_secondary_above_left
        ? plane->data[((size_t)block_y - 1U) * plane->stride_samples + block_x - 1U]
        : 0U;
references.has_above_left =
    block_x > mode->mrl_index &&block_y >
    mode->mrl_index &&plane_sample_inside_active_tile(plane, (size_t)block_x - 1U - mode->mrl_index,
                                                      (size_t)block_y - 1U - mode->mrl_index);
references.above_left =
    references.has_above_left
        ? plane->data[((size_t)block_y - 1U - mode->mrl_index) * plane->stride_samples + block_x -
                      1U - mode->mrl_index]
        : 0U;
references.negative_count = 0U;
#endif
            if (mode->mrl_index != 0U && references.has_above_left) {
                size_t negative_index;
                references.negative_count = (uint8_t)(mode->mrl_index + 1U);
                for (negative_index = 1U; negative_index <= references.negative_count;
                     ++negative_index) {
#if AV2H_KERNEL_WIDE
                    references.above_negative[negative_index] = av2h_reconstructed_sample_load_u16(
                        plane->data, plane->stride_samples, block_x - negative_index,
                        above_reference_y);
                    references.left_negative[negative_index] = av2h_reconstructed_sample_load_u16(
                        plane->data, plane->stride_samples, block_x - 1U - mode->mrl_index,
                        block_y - (is_top_superblock_boundary ? 1U : negative_index));
#else
        references.above_negative[negative_index] =
            plane->data[above_reference_y * plane->stride_samples + block_x - negative_index];
        references.left_negative[negative_index] =
            plane->data[((size_t)block_y - (is_top_superblock_boundary ? 1U : negative_index)) *
                            plane->stride_samples +
                        block_x - 1U - mode->mrl_index];
#endif
                }
            }
#if AV2H_KERNEL_WIDE
            for (index = 0U; index < (size_t)descriptor->output_width * descriptor->output_height;
                 ++index) {
                scratch->predictor[index] = dc_value;
            }
            if (mode->mode == AV2H_INTRA_DC && mode->enable_ibp != 0U && mode->palette_size == 0U &&
                !(descriptor->output_width == 4U && descriptor->output_height == 4U)) {
                static const uint8_t weights[5U][16U] = {
                    {96U},
                    {86U, 107U},
                    {77U, 90U, 102U, 115U},
                    {71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U},
                    {68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U, 98U, 102U, 106U, 109U, 113U, 117U,
                     121U, 124U}};
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
                            size_t predictor_index =
                                (size_t)row * descriptor->output_width + column;
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
                            size_t predictor_index =
                                (size_t)row * descriptor->output_width + column;
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
                status =
                    palette_fill_predictor_u16(mode, block_x, block_y, descriptor->output_width,
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
                                ? (uint16_t)((scratch
                                                  ->above[column < above_count ? column
                                                                               : above_count - 1U] +
                                              scratch
                                                  ->secondary_above[column < secondary_above_count
                                                                        ? column
                                                                        : secondary_above_count -
                                                                              1U] +
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
                status = av2h_luma_predict_u16(
                    &prediction_mode, descriptor->output_width, descriptor->output_height,
                    &references, scratch->predictor, descriptor->output_width, domain);
#else
if (mode->palette_size != 0U) {
    status = palette_fill_predictor(mode, block_x, block_y, output_width, output_height,
                                    scratch->predictor);
#endif
                if (status != AV2H_OK)
                    return status;
#if !AV2H_KERNEL_WIDE
                status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                               scratch->predictor, output_width, scratch,
                                               &reconstruction);
            } else if (mode->use_intrabc) {
                status = av2h_intrabc_predict(diagnostics, mode, plane, block_x, block_y,
                                              output_width, output_height, scratch->predictor);
                if (status != AV2H_OK)
                    return status;
                status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                               scratch->predictor, output_width, scratch,
                                               &reconstruction);
            } else if (mode->mode == AV2H_INTRA_DC && above_count == 0U && left_count == 0U &&
                       mode->use_intra_dip == 0U) {
                for (index = 0U; index < (size_t)output_width * output_height; ++index) {
                    scratch->predictor[index] = domain->midpoint;
#endif
                }
#if AV2H_KERNEL_WIDE
                status =
                    av2h_luma_reconstruct_u16(diagnostics, mode, prefix, transform, coefficients,
                                              scratch->predictor, scratch, &reconstruction, domain);
#else
status = av2h_luma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                               scratch->predictor, output_width, scratch, &reconstruction);
}
else {
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
#endif
                if (status != AV2H_OK)
                    return status;
                av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 3U);
#if AV2H_KERNEL_WIDE
                av2h_predictor_commit_u16(plane->data + (size_t)block_y * plane->stride_samples +
                                              block_x,
                                          plane->stride_samples, reconstruction.pixels,
                                          descriptor->output_width, visible_width, visible_height);
#else
if (reconstruction.width != output_width || reconstruction.height != output_height) {
    return AV2H_UNSUPPORTED;
}
av2h_predictor_commit_u8(plane->data + (size_t)block_y * plane->stride_samples + block_x,
                         plane->stride_samples, reconstruction.pixels, output_width, visible_width,
                         visible_height);
#endif
                AV2H_KERNEL_NAME(plane_mark_reconstructed, plane_mark_reconstructed_u16)(
                    plane, block_x, block_y, visible_width, visible_height);
                return AV2H_OK;
            }
            Av2hStatus AV2H_KERNEL_NAME(av2h_chroma_reconstruct_to_plane,
                                        av2h_chroma_reconstruct_to_plane_u16)(
                Av2hDecodeDiagnostics * diagnostics, const Av2hPartitionNode *leaf,
                const Av2hChromaLeafMode *mode, const Av2hCoeffPrefix *prefix,
                const Av2hTransformType *transform, const Av2hCoeffBlock *coefficients,
                AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * plane,
                const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *
                    luma_plane,
                uint8_t chroma_plane,
#if !AV2H_KERNEL_WIDE
                Av2hReconstructionPlane *predictor_plane,
#endif
                uint8_t chroma_format_idc,
                AV2H_KERNEL_TYPE(Av2hReconstructionScratch, Av2hReconstructionScratchU16) * scratch
#if AV2H_KERNEL_WIDE
                ,
                const Av2hSampleDomain *domain
#endif
            ) {
#if AV2H_KERNEL_WIDE
                if (!av2h_sample_domain_valid(domain))
                    return AV2H_INVALID_ARGUMENT;
                Av2hLeafMode luma_mode;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
                AV2H_KERNEL_TYPE(Av2hBlockReconstruction, Av2hBlockReconstructionU16)
                reconstruction;
#if !AV2H_KERNEL_WIDE
                Av2hLumaReferences references;
#endif
                const Av2hTxDescriptor *descriptor;
                uint16_t block_x;
                uint16_t block_y;
                uint16_t width;
                uint16_t height;
                uint16_t visible_width;
                uint16_t visible_height;
#if AV2H_KERNEL_WIDE
                size_t above_count = 0U;
                size_t above_target;
                size_t left_count = 0U;
                size_t left_target;
#else
    uint8_t have_top;
    uint8_t have_left;
#endif
                size_t index;
#if AV2H_KERNEL_WIDE
                int prediction_angle;
                uint8_t apply_edge_ibp;
#endif
                Av2hStatus status;
#if AV2H_KERNEL_WIDE
                Av2hLumaReferencesU16 references = {.left = NULL};
#endif
                av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 0U);
#if AV2H_KERNEL_WIDE
                if (leaf == NULL || mode == NULL || prefix == NULL || transform == NULL ||
                    coefficients == NULL || plane == NULL || scratch == NULL ||
                    plane->data == NULL || plane->reconstructed == NULL ||
                    (chroma_format_idc != 0U && chroma_format_idc != 2U) || chroma_plane < 1U ||
                    chroma_plane > 2U || plane->stride_samples < plane->width ||
                    plane->reference_width == 0U || plane->reference_height == 0U ||
                    plane->reference_width > plane->width ||
                    plane->reference_height > plane->height) {
#else
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
#endif
                    return AV2H_INVALID_ARGUMENT;
                }
#if !AV2H_KERNEL_WIDE
                av2h_reconstruction_diagnostics_set_chroma_stage(diagnostics, 1U);
#endif
                status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
                if (status != AV2H_OK)
                    return status;
#if AV2H_KERNEL_WIDE
                width = descriptor->output_width;
                height = descriptor->output_height;
#else
    width = descriptor->output_width > 32U ? descriptor->output_width : coefficients->width;
    height = descriptor->output_height > 32U ? descriptor->output_height : coefficients->height;
#endif
                block_x = (uint16_t)((uint32_t)leaf->column * (chroma_format_idc == 2U ? 4U : 2U));
                block_y = (uint16_t)((uint32_t)leaf->row * (chroma_format_idc == 2U ? 4U : 2U));
#if AV2H_KERNEL_WIDE
                if ((chroma_format_idc == 2U && mode->is_cfl != 0U &&
                     mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA &&
                     mode->cfl_kind != AV2H_CFL_DERIVED_ALPHA &&
                     mode->cfl_kind != AV2H_CFL_MULTI_PARAM) ||
                    (mode->is_cfl != 0U && mode->cfl_kind != AV2H_CFL_MULTI_PARAM &&
                     mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA &&
                     mode->cfl_kind != AV2H_CFL_DERIVED_ALPHA) ||
                    (mode->mode != AV2H_INTRA_DC && mode->mode != AV2H_INTRA_VERTICAL &&
                     mode->mode != AV2H_INTRA_HORIZONTAL && mode->mode != AV2H_INTRA_SMOOTH &&
                     mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
                     mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL && mode->mode != AV2H_INTRA_PAETH &&
                     mode->mode != AV2H_INTRA_D45 && mode->mode != AV2H_INTRA_D67 &&
                     mode->mode != AV2H_INTRA_D113 && mode->mode != AV2H_INTRA_D135 &&
                     mode->mode != AV2H_INTRA_D157 && mode->mode != AV2H_INTRA_D203) ||
                    !av2h_chroma_geometry_supported(width, height) || block_x >= plane->width ||
                    block_y >= plane->height) {
                    return AV2H_UNSUPPORTED;
#else
    if (block_x >= plane->width || block_y >= plane->height ||
        !av2h_chroma_geometry_supported(width, height)) {
        return AV2H_INVALID_ARGUMENT;
#endif
                }
                visible_width =
                    width < plane->width - block_x ? width : (uint16_t)(plane->width - block_x);
                visible_height =
                    height < plane->height - block_y ? height : (uint16_t)(plane->height - block_y);
#if AV2H_KERNEL_WIDE
                prediction_angle =
                    av2h_directional_base_angle(mode->mode) == 0
                        ? 0
                        : av2h_luma_wide_angle(av2h_directional_base_angle(mode->mode) +
                                                   mode->angle_delta * 3,
                                               width, height);
                apply_edge_ibp = (uint8_t)(mode->edge_ibp != 0U && (width != 4U || height != 4U));
                above_target = width;
                if (width <= 32U &&
                    (mode->mode == AV2H_INTRA_SMOOTH ||
                     mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) &&
                    block_x + above_target < plane->width) {
                    ++above_target;
#else
    if (predictor_plane != NULL &&
        (predictor_plane->data == NULL ||
         predictor_plane->stride_samples < predictor_plane->width ||
         block_x >= predictor_plane->width || block_y >= predictor_plane->height ||
         visible_width > predictor_plane->width - block_x ||
         visible_height > predictor_plane->height - block_y)) {
        return AV2H_INVALID_ARGUMENT;
#endif
                }
#if AV2H_KERNEL_WIDE
                if (width <= 32U &&
                    ((prediction_angle > 0 && prediction_angle < 90) ||
                     (apply_edge_ibp != 0U && prediction_angle > 180 && prediction_angle < 270))) {
                    above_target += height;
                    if (above_target > sizeof(scratch->above) / sizeof(scratch->above[0])) {
                        above_target = sizeof(scratch->above) / sizeof(scratch->above[0]);
#else
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
#endif
                    }
#if !AV2H_KERNEL_WIDE
                    if (left_count != 0U && height <= 32U && angle > 180 && angle < 270) {
                        left_count += height;
#endif
                    }
#if AV2H_KERNEL_WIDE
                    if (block_y != 0U) {
                        for (index = 0U; index < above_target; ++index) {
                            size_t sample_x = block_x + index;
                            if (sample_x >= plane->width)
                                sample_x = plane->width - 1U;
                            if (!plane_sample_reconstructed_u16(plane, sample_x, block_y - 1U)) {
#else
    if (above_count > sizeof(scratch->above)) {
        above_count = sizeof(scratch->above);
    }
    if (left_count > sizeof(scratch->left)) {
        left_count = sizeof(scratch->left);
    }
    for (index = 0U; index < above_count; ++index) {
        if (!av2h_plane_sample_reconstructed(plane, block_x + index, (size_t)block_y - 1U)) {
            above_count = index;
#endif
                                break;
                            }
#if AV2H_KERNEL_WIDE
                            scratch->above[above_count++] = av2h_reconstructed_sample_load_u16(
                                plane->data, plane->stride_samples, sample_x, block_y - 1U);
#else
    }
    for (index = 0U; index < above_count; ++index) {
        scratch->above[index] = av2h_reconstructed_sample_load_u8(
            plane->data, plane->stride_samples, block_x + index, (size_t)block_y - 1U);
    }
    if (above_count != 0U && above_count < width) {
        uint8_t edge = scratch->above[above_count - 1U];
        while (above_count < width) {
            scratch->above[above_count++] = edge;
#endif
                        }
                    }
#if AV2H_KERNEL_WIDE
                    left_target = height;
                    if (height <= 32U &&
                        (mode->mode == AV2H_INTRA_SMOOTH ||
                         mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
                        block_y + left_target < plane->height) {
                        ++left_target;
#else
    for (index = 0U; index < left_count; ++index) {
        size_t sy = (size_t)block_y + index;
        if (sy >= plane->height) {
            sy = plane->height - 1U;
#endif
                    }
#if AV2H_KERNEL_WIDE
                    if (height <= 32U &&
                        ((prediction_angle > 180 && prediction_angle < 270) ||
                         (apply_edge_ibp != 0U && prediction_angle > 0 && prediction_angle < 90))) {
                        left_target += width;
                        if (left_target > sizeof(scratch->left) / sizeof(scratch->left[0])) {
                            left_target = sizeof(scratch->left) / sizeof(scratch->left[0]);
                        }
                    }
                    if (block_x != 0U) {
                        for (index = 0U; index < left_target; ++index) {
                            size_t sample_y = block_y + index;
                            if (sample_y >= plane->height)
                                sample_y = plane->height - 1U;
                            if (!plane_sample_reconstructed_u16(plane, block_x - 1U, sample_y)) {
#else
        if (!av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U, sy)) {
            left_count = index;
#endif
                                break;
                            }
#if AV2H_KERNEL_WIDE
                            scratch->left[left_count++] = av2h_reconstructed_sample_load_u16(
                                plane->data, plane->stride_samples, block_x - 1U, sample_y);
#endif
                        }
#if !AV2H_KERNEL_WIDE
                        for (index = 0U; index < left_count; ++index) {
                            size_t sy = (size_t)block_y + index;
                            if (sy >= plane->height) {
                                sy = plane->height - 1U;
#endif
                            }
#if AV2H_KERNEL_WIDE
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
#else
scratch->left[index] =
    av2h_reconstructed_sample_load_u8(plane->data, plane->stride_samples, block_x - 1U, sy);
#endif
                                    }
#if !AV2H_KERNEL_WIDE
                                    references.left = left_count == 0U ? NULL : scratch->left;
                                    references.left_count = left_count;
                                    references.above = above_count == 0U ? NULL : scratch->above;
                                    references.above_count = above_count;
                                    have_top = above_count >= width ? 1U : 0U;
                                    have_left = left_count >= height ? 1U : 0U;
#endif
                                }
#if AV2H_KERNEL_WIDE
                                if (have_left != 0U) {
                                    size_t step = height > 32U ? 2U : 1U;
                                    for (index = 0U; index < height; index += step) {
                                        sum += scratch->left[index];
                                        ++count;
#else
references.above_left = 0U;
references.has_above_left = 0U;
if (block_x != 0U && block_y != 0U &&
    av2h_plane_sample_reconstructed(plane, (size_t)block_x - 1U, (size_t)block_y - 1U)) {
    references.above_left =
        plane->data[((size_t)block_y - 1U) * plane->stride_samples + block_x - 1U];
    references.has_above_left = 1U;
#endif
                                    }
#if !AV2H_KERNEL_WIDE
                                    if (!mode->is_cfl && mode->mode != AV2H_INTRA_DC &&
                                        (references.above_count == 0U ||
                                         references.left_count == 0U)) {
                                        size_t above_count = (size_t)width + height;
                                        size_t left_count = (size_t)height + width;
                                        uint8_t had_above = references.above_count != 0U;
                                        uint8_t had_left = references.left_count != 0U;
                                        uint8_t above_value =
                                            had_left ? references.left[0] : (domain->midpoint - 1U);
                                        uint8_t left_value = had_above ? references.above[0]
                                                                       : (domain->midpoint + 1U);
                                        if (above_count > sizeof(scratch->above)) {
                                            above_count = sizeof(scratch->above);
#endif
                                        }
#if AV2H_KERNEL_WIDE
                                        dc_value =
                                            count == 0U ? domain->midpoint
                                            : width != height && have_top != 0U && have_left != 0U
                                                ? dc_average_rectangular_u16(sum, count)
                                                : (uint16_t)((sum + count / 2U) / count);
#else
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
#endif
                                        for (index = 0U; index < (size_t)width * height; ++index) {
                                            scratch->predictor[index] = dc_value;
                                        }
                                        if (mode->cfl_kind == AV2H_CFL_MULTI_PARAM) {
#if AV2H_KERNEL_WIDE
                                            status = av2h_mhccp_predict_observed_u16(
                                                mode, luma_plane, plane, block_x, block_y, width,
                                                height, scratch->predictor, width, domain);
                                        } else if (mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA) {
                                            status =
                                                chroma_format_idc == 2U
                                                    ? av2h_cfl_derived_predict_u16_444(
                                                          luma_plane, plane, block_x, block_y,
                                                          width, height, have_top, have_left,
                                                          scratch->predictor, width, domain)
                                                    : av2h_cfl_derived_predict_u16(
                                                          luma_plane, plane,
                                                          mode->cfl_downsample_filter_index,
                                                          mode->superblock_size, block_x, block_y,
                                                          width, height, have_top, have_left,
                                                          scratch->predictor, width, domain);
#else
        status = av2h_mhccp_predict_observed(diagnostics, mode, luma_plane, plane, block_x, block_y,
                                             width, height, scratch->predictor, width);
#endif
                                        } else {
                                            int32_t alpha_q3 = 0;
#if !AV2H_KERNEL_WIDE
                                            uint8_t derive_alpha =
                                                (uint8_t)(mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA);
                                            if (!derive_alpha) {
                                                if (mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA)
                                                    return AV2H_UNSUPPORTED;
#endif
                                                status = cfl_explicit_alpha_q3(mode, chroma_plane,
                                                                               &alpha_q3);
                                                if (status != AV2H_OK)
                                                    return status;
#if AV2H_KERNEL_WIDE
                                                status =
                                                    chroma_format_idc == 2U
                                                        ? av2h_cfl_explicit_predict_u16_444(
                                                              luma_plane, alpha_q3, block_x,
                                                              block_y, width, height, have_top,
                                                              have_left, scratch->predictor, width,
                                                              domain)
                                                        : av2h_cfl_explicit_predict_u16(
                                                              luma_plane,
                                                              mode->cfl_downsample_filter_index,
                                                              alpha_q3, mode->superblock_size,
                                                              block_x, block_y, width, height,
                                                              have_top, have_left,
                                                              scratch->predictor, width, domain);
#else
    }
    status = av2h_cfl_derived_predict(luma_plane, plane, mode->cfl_downsample_filter_index,
                                      derive_alpha, alpha_q3, mode->superblock_size,
                                      mode->chroma_format_idc, block_x, block_y, width, height,
                                      have_top, have_left, scratch->predictor, width);
#endif
                                            }
                                            if (status != AV2H_OK)
                                                return status;
#if AV2H_KERNEL_WIDE
                                        }
                                        else if (mode->mode == AV2H_INTRA_DC) {
                                            static const uint8_t weights[5U][16U] = {
                                                {96U},
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
                                                       : width != height && above_count != 0U &&
                                                               left_count != 0U
                                                           ? dc_average_rectangular_u16(sum, count)
                                                           : (uint16_t)((sum + count / 2U) / count);
                                            for (index = 0U; index < (size_t)width * height;
                                                 ++index) {
                                                scratch->predictor[index] = dc_value;
                                            }
                                            if (mode->edge_ibp != 0U &&
                                                !(width == 4U && height == 4U)) {
                                                if (above_count >= width) {
                                                    uint16_t row_limit = (uint16_t)(height >> 2U);
                                                    uint16_t column_start =
                                                        width < height && left_count >= height
                                                            ? (uint16_t)(width >> 2U)
                                                            : 0U;
                                                    uint8_t weight_row =
                                                        (uint8_t)((height == 4U    ? 2U
                                                                   : height == 8U  ? 3U
                                                                   : height == 16U ? 4U
                                                                   : height == 32U ? 5U
                                                                                   : 6U) -
                                                                  2U);
                                                    for (row = 0U; row < row_limit; ++row) {
                                                        unsigned int weight =
                                                            weights[weight_row][row];
                                                        for (column = column_start; column < width;
                                                             ++column) {
                                                            size_t output =
                                                                (size_t)row * width + column;
                                                            scratch->predictor[output] =
                                                                (uint16_t)((scratch->above[column] *
                                                                                (128U - weight) +
                                                                            scratch->predictor
                                                                                    [output] *
                                                                                weight +
                                                                            64U) >>
                                                                           7U);
                                                        }
                                                    }
                                                }
                                                if (left_count >= height) {
                                                    uint16_t row_start =
                                                        width >= height && above_count >= width
                                                            ? (uint16_t)(height >> 2U)
                                                            : 0U;
                                                    uint16_t column_limit = (uint16_t)(width >> 2U);
                                                    uint8_t weight_row =
                                                        (uint8_t)((width == 4U    ? 2U
                                                                   : width == 8U  ? 3U
                                                                   : width == 16U ? 4U
                                                                   : width == 32U ? 5U
                                                                                  : 6U) -
                                                                  2U);
                                                    for (row = row_start; row < height; ++row) {
                                                        for (column = 0U; column < column_limit;
                                                             ++column) {
                                                            unsigned int weight =
                                                                weights[weight_row][column];
                                                            size_t output =
                                                                (size_t)row * width + column;
                                                            scratch->predictor[output] =
                                                                (uint16_t)((scratch->left[row] *
                                                                                (128U - weight) +
                                                                            scratch->predictor
                                                                                    [output] *
                                                                                weight +
                                                                            64U) >>
                                                                           7U);
                                                        }
                                                    }
                                                }
                                            }
#endif
                                        }
                                        else {
#if AV2H_KERNEL_WIDE
                                            references.has_above_left =
                                                (uint8_t)(block_x != 0U && block_y != 0U &&
                                                          plane_sample_reconstructed_u16(
                                                              plane, block_x - 1U, block_y - 1U));
                                            references.above_left =
                                                references.has_above_left
                                                    ? av2h_reconstructed_sample_load_u16(
                                                          plane->data, plane->stride_samples,
                                                          block_x - 1U, block_y - 1U)
                                                    : 0U;
                                            references.above =
                                                above_count == 0U ? NULL : scratch->above;
                                            references.above_count = above_count;
                                            references.left =
                                                left_count == 0U ? NULL : scratch->left;
                                            references.left_count = left_count;
                                            status = av2h_chroma_predict_u16(
                                                &luma_mode, width, height, &references,
                                                scratch->predictor, width, domain);
#else
    status = av2h_chroma_predict(mode, width, height, &references, scratch->predictor, width);
#endif
                                            if (status != AV2H_OK)
                                                return status;
                                        }
                                        av2h_reconstruction_diagnostics_set_chroma_stage(
                                            diagnostics, 2U);
#if AV2H_KERNEL_WIDE
                                        status = av2h_chroma_reconstruct_u16(
                                            diagnostics, mode, prefix, transform, coefficients,
                                            scratch->predictor, scratch, &reconstruction, domain);
                                        if (status != AV2H_OK)
#else
if (predictor_plane != NULL) {
    av2h_predictor_commit_u8(
        predictor_plane->data + (size_t)block_y * predictor_plane->stride_samples + block_x,
        predictor_plane->stride_samples, scratch->predictor, width, visible_width, visible_height);
}
status = av2h_chroma_reconstruct(diagnostics, mode, prefix, transform, coefficients,
                                 scratch->predictor, width, height, scratch, &reconstruction);
if (status != AV2H_OK) {
    if (prefix->all_zero && coefficients->nonzero_count == 0U) {
#endif
                                            return status;
#if AV2H_KERNEL_WIDE
                                        av2h_predictor_commit_u16(
                                            plane->data + (size_t)block_y * plane->stride_samples +
                                                block_x,
                                            plane->stride_samples, reconstruction.pixels, width,
                                            visible_width, visible_height);
#else
    }
    const Av2hReconstructionDiagnostics *snapshot =
        av2h_reconstruction_diagnostics_get(diagnostics);
    av2h_reconstruction_diagnostics_set_chroma_stage(
        diagnostics, (uint8_t)(10U + (snapshot != NULL ? snapshot->inverse_transform_stage : 0U)));
    return status;
}
av2h_predictor_commit_u8(plane->data + (size_t)block_y * plane->stride_samples + block_x,
                         plane->stride_samples, reconstruction.pixels, width, visible_width,
                         visible_height);
#endif
                                        AV2H_KERNEL_NAME(plane_mark_reconstructed,
                                                         plane_mark_reconstructed_u16)(
                                            plane, block_x, block_y, visible_width, visible_height);
#if !AV2H_KERNEL_WIDE
                                        if (prefix->all_zero && coefficients->nonzero_count == 0U) {
#endif
                                            av2h_reconstruction_diagnostics_set_chroma_stage(
                                                diagnostics, 3U);
#if !AV2H_KERNEL_WIDE
                                        }
#endif
                                        return AV2H_OK;
                                    }