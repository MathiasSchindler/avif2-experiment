#include "tile.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"
#include "tx_geometry.h"

static void tile_capture_luma_root_child(
    Av2hTileDecoder *decoder, const Av2hFirstLeafPath *path) {
    Av2hBlockSize child_size;
    uint16_t second_row = decoder->sb_row;
    uint16_t second_column = decoder->sb_column;
    size_t index;
    if (decoder->luma_root_partition == AV2H_PARTITION_HORZ) {
        child_size = decoder->root_block_size == AV2H_BLOCK_64X64 ?
            AV2H_BLOCK_64X32 :
            decoder->root_block_size == AV2H_BLOCK_128X128 ?
                AV2H_BLOCK_128X64 : AV2H_BLOCK_INVALID;
        second_row = (uint16_t)(decoder->sb_row +
            decoder->superblock_mi / 2U);
    } else if (decoder->luma_root_partition == AV2H_PARTITION_VERT) {
        child_size = decoder->root_block_size == AV2H_BLOCK_64X64 ?
            AV2H_BLOCK_32X64 :
            decoder->root_block_size == AV2H_BLOCK_128X128 ?
                AV2H_BLOCK_64X128 : AV2H_BLOCK_INVALID;
        second_column = (uint16_t)(decoder->sb_column +
            decoder->superblock_mi / 2U);
    } else {
        return;
    }
    if (child_size == AV2H_BLOCK_INVALID) return;
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        int is_first = decision->node.row == decoder->sb_row &&
            decision->node.column == decoder->sb_column;
        int is_second = decision->node.row == second_row &&
            decision->node.column == second_column;
        if (decision->node.block_size != child_size ||
            (!is_first && !is_second)) {
            continue;
        }
        ++decoder->luma_root_child_count;
        if (decision->partition == AV2H_PARTITION_NONE ||
            av2h_tile_partition_direction(decision->partition) ==
                av2h_tile_partition_direction(
                    decoder->luma_root_partition)) {
            decoder->luma_root_children_share = 0U;
        }
    }
}

static int luma_geometry_supported(const Av2hPartitionNode *leaf) {
    const Av2hTxDescriptor *descriptor;
    return leaf != 0 && av2h_luma_tx_descriptor_get(
        leaf->block_size, &descriptor) == AV2H_OK;
}

static Av2hStatus tile_initialize_skipped_intrabc(
    Av2hTileDecoder *decoder,
    const Av2hTileLumaLeaf *leaf,
    Av2hFirstCoeffPrefix *prefix,
    Av2hFirstTransformType *transform,
    Av2hFirstCoeffBlock *block) {
    size_t tell;
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (decoder == 0 || leaf == 0 || prefix == 0 || transform == 0 ||
        block == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (leaf->mode.use_intrabc != 1U ||
        leaf->mode.skip_transform != 1U) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_luma_tx_descriptor_get(
        leaf->node.block_size, &descriptor);
    if (status != AV2H_OK) return status;
    prefix->tx_size = descriptor->tx_size;
    prefix->tx_size_context = descriptor->tx_size_context;
    tell = av2h_range_tell(&decoder->range);
    {
        Av2hFirstTxSize tx_size = prefix->tx_size;
        uint8_t tx_size_context = prefix->tx_size_context;
        av2h_memory_fill(prefix, 0U, sizeof(*prefix));
        prefix->tx_size = tx_size;
        prefix->tx_size_context = tx_size_context;
    }
    prefix->all_zero = 1U;
    prefix->before_all_zero_bits = tell;
    prefix->after_all_zero_bits = tell;
    prefix->after_eob_point_bits = tell;
    prefix->after_eob_extra_bit = tell;
    prefix->after_eob_bits = tell;

    av2h_memory_fill(transform, 0U, sizeof(*transform));
    transform->primary_type = AV2H_TX_DCT_DCT;
    transform->before_primary_bits = tell;
    transform->after_primary_bits = tell;
    transform->after_secondary_bits = tell;

    av2h_memory_fill(block, 0U, sizeof(*block));
    block->width = descriptor->coefficient_width;
    block->height = descriptor->coefficient_height;
    block->coefficient_count = block->width * block->height;
    block->before_levels_bits = tell;
    block->after_levels_bits = tell;
    block->after_levels_range = decoder->range.range;
    block->after_levels_value = decoder->range.value;
    block->after_coefficients_bits = tell;
    status = av2h_coeff_context_state_update(
        &decoder->coefficient_contexts, 0U, &leaf->node, 0U);
    return status;
}

static Av2hStatus tile_initialize_all_zero_luma(
    Av2hTileDecoder *decoder,
    const Av2hTileLumaLeaf *leaf,
    Av2hFirstTransformType *transform,
    Av2hFirstCoeffBlock *block) {
    uint16_t width;
    uint16_t height;
    size_t tell;
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (decoder == 0 || leaf == 0 || transform == 0 || block == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(leaf->prefix.tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    width = descriptor->coefficient_width;
    height = descriptor->coefficient_height;
    if (!leaf->prefix.all_zero || leaf->prefix.eob != 0U ||
        leaf->mode.use_intrabc || leaf->mode.skip_transform) {
        return AV2H_UNSUPPORTED;
    }
    tell = av2h_range_tell(&decoder->range);
    av2h_memory_fill(transform, 0U, sizeof(*transform));
    transform->primary_type = AV2H_TX_DCT_DCT;
    transform->before_primary_bits = tell;
    transform->after_primary_bits = tell;
    transform->after_secondary_bits = tell;

    av2h_memory_fill(block, 0U, sizeof(*block));
    block->width = width;
    block->height = height;
    block->coefficient_count = (uint16_t)(width * height);
    block->before_levels_bits = tell;
    block->after_levels_bits = tell;
    block->after_levels_range = decoder->range.range;
    block->after_levels_value = decoder->range.value;
    block->after_coefficients_bits = tell;
    status = av2h_coeff_context_state_update(
        &decoder->coefficient_contexts, 0U, &leaf->node, 0U);
    return status;
}

static Av2hStatus tile_store_luma_mode(Av2hTileDecoder *decoder,
                                       const Av2hPartitionNode *node,
                                       const Av2hFirstLeafMode *mode) {
    size_t width;
    size_t height;
    size_t row;
    size_t column;
    Av2hStatus status = av2h_block_dimensions_mi(
        node->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    if (node->column >= decoder->mi_columns ||
        node->row >= decoder->mi_rows) {
        return AV2H_OVERFLOW;
    }
    if (width > decoder->mi_columns - node->column) {
        width = decoder->mi_columns - node->column;
    }
    if (height > decoder->mi_rows - node->row) {
        height = decoder->mi_rows - node->row;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t index = ((size_t)node->row + row) * decoder->mi_columns +
                           (size_t)node->column + column;
            Av2hTileLumaModeCell *cell = &decoder->colocated_luma[index];
            cell->context.present = 1U;
            cell->context.use_intrabc = mode->use_intrabc;
            cell->context.intrabc_mode = mode->intrabc_mode;
            cell->context.intrabc_drl_index = mode->intrabc_drl_index;
            cell->context.mv_precision = mode->mv_precision;
            cell->context.displacement_x = mode->displacement_x;
            cell->context.displacement_y = mode->displacement_y;
            cell->context.skip_transform = mode->skip_transform;
            cell->context.joint_mode = mode->joint_mode;
            cell->context.fsc_mode = mode->fsc_mode;
            cell->context.mrl_index = mode->mrl_index;
            cell->context.multi_line_mrl = mode->multi_line_mrl;
            cell->mode = (uint8_t)mode->mode;
            cell->angle_delta = mode->angle_delta;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_next_luma(
    Av2hTileDecoder *decoder,
    Av2hTileLumaLeaf *leaf,
    Av2hFirstCoeffBlock *block,
    uint8_t *has_leaf) {
    Av2hFirstLeafPath path;
    uint8_t dc_sign_context;
    Av2hStatus status;
    if (decoder == 0 || leaf == 0 || block == 0 || has_leaf == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase != AV2H_TILE_DECODE_LUMA) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    av2h_tile_diagnostics_set_luma_stage(0U);
    status = av2h_partition_traversal_next(
        &decoder->luma_traversal, &decoder->range,
        &decoder->partition_cdfs, &decoder->partition_contexts, &path,
        has_leaf);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (!*has_leaf) {
        return av2h_tile_decoder_finish_root(decoder);
    }
    av2h_tile_capture_root_partition(
        decoder, &path, &decoder->luma_root_partition);
    tile_capture_luma_root_child(decoder, &path);
    leaf->node = path.leaf;
    av2h_tile_diagnostics_set_luma_range(0U, decoder->range.range);
    if (!luma_geometry_supported(&leaf->node)) {
        return av2h_tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
    }
    if (decoder->luma_mode_contexts.superblock_mi != 0U &&
        leaf->node.row % decoder->luma_mode_contexts.superblock_mi == 0U &&
        leaf->node.column == 0U) {
        decoder->luma_mode_contexts.intrabc_bank_count = 0U;
        decoder->luma_mode_contexts.intrabc_bank_start = 0U;
        decoder->luma_mode_contexts.intrabc_bank_remaining_hits = 0U;
        decoder->luma_mode_contexts.intrabc_bank_unit_hits = 0U;
        decoder->luma_mode_contexts.intrabc_bank_superblock_hits = 0U;
    }
    av2h_tile_diagnostics_set_luma_stage(1U);
    status = av2h_luma_mode_read_with_state_and_gdf(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &decoder->luma_mode_contexts,
        &decoder->gdf_entropy, &decoder->cdef_entropy,
        &decoder->ccso_entropy, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_luma_palette_map_read(
        &decoder->range, &decoder->leaf_cdfs, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    av2h_tile_diagnostics_set_luma_range(1U, decoder->range.range);
    status = av2h_luma_mode_context_state_update(
        &decoder->luma_mode_contexts, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_mode(decoder, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return av2h_tile_decoder_fail(decoder, AV2H_OUT_OF_MEMORY);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (leaf->mode.skip_transform != 0U) {
        status = tile_initialize_skipped_intrabc(
            decoder, leaf, &leaf->prefix, &leaf->transform, block);
        if (status == AV2H_UNSUPPORTED) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
        }
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(2U);
    status = av2h_coeff_prefix_read_with_state(
        &decoder->range, &decoder->coeff_cdfs, decoder->frame, &leaf->node,
        &leaf->mode, &decoder->coefficient_contexts, &leaf->prefix,
        &dc_sign_context);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    av2h_tile_diagnostics_set_luma_range(2U, decoder->range.range);
    if (leaf->prefix.all_zero) {
        status = tile_initialize_all_zero_luma(
            decoder, leaf, &leaf->transform, block);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(3U);
    status = av2h_first_transform_type_read(
        &decoder->range, &decoder->transform_cdfs, decoder->sequence,
        decoder->frame, &leaf->mode, &leaf->prefix, &leaf->transform);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    av2h_tile_diagnostics_set_luma_range(3U, decoder->range.range);
    av2h_tile_diagnostics_set_luma_stage(4U);
    status = av2h_coeff_block_read_with_state(
        &decoder->range, &decoder->level_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &leaf->mode, &leaf->prefix,
        &leaf->transform, &decoder->coefficient_contexts, block);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    av2h_tile_diagnostics_set_luma_range(4U, decoder->range.range);
    decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_reconstruct_next_luma(
    Av2hTileDecoder *decoder,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch,
    Av2hTileLumaLeaf *leaf,
    uint8_t *has_leaf) {
    Av2hFirstCoeffBlock block;
    Av2hStatus status;
    if (decoder == 0 || plane == 0 || scratch == 0 || leaf == 0 ||
        has_leaf == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tile_decoder_next_luma(
        decoder, leaf, &block, has_leaf);
    if (status != AV2H_OK || !*has_leaf) return status;
    status = av2h_luma_reconstruct_to_plane(
        &leaf->node, &leaf->mode, &leaf->prefix, &leaf->transform,
        &block, plane, scratch);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}