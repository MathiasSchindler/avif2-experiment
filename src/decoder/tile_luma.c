#include "tile.h"
#include "internal/coeff_modules.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"
#include "tx_geometry.h"

static void tile_capture_luma_root_child(
    Av2hTileDecoder *decoder, const Av2hFirstLeafPath *path) {
    Av2hBlockSize child_size;
    uint16_t first_row = decoder->sb_row;
    uint16_t first_column = decoder->sb_column;
    uint16_t second_row = decoder->sb_row;
    uint16_t second_column = decoder->sb_column;
    Av2hPartitionType root_partition = decoder->luma_root_partition;
    uint8_t *child_count = &decoder->luma_root_child_count;
    uint8_t *children_share = &decoder->luma_root_children_share;
    uint8_t square_child = 0U;
    size_t index;
    if (decoder->root_block_size == AV2H_BLOCK_128X128 &&
        av2h_tile_partition_direction(
            decoder->luma_root_partition) != 0U) {
        for (index = 0U; index < path->decision_count; ++index) {
            const Av2hPartitionDecision *decision = &path->decisions[index];
            if (decision->node.block_size == AV2H_BLOCK_64X64 &&
                decision->node.row >= decoder->sb_row &&
                decision->node.row < decoder->sb_row + 32U &&
                decision->node.column >= decoder->sb_column &&
                decision->node.column < decoder->sb_column + 32U) {
                decoder->square_root_child_index = (uint8_t)(
                    (decision->node.row >= decoder->sb_row + 16U ? 2U : 0U) +
                    (decision->node.column >= decoder->sb_column + 16U ?
                         1U : 0U));
                break;
            }
        }
    }
    if (decoder->root_block_size == AV2H_BLOCK_128X128 &&
        decoder->square_root_child_index < 4U) {
        uint16_t root_row = (uint16_t)(decoder->sb_row +
            (decoder->square_root_child_index >= 2U ? 16U : 0U));
        uint16_t root_column = (uint16_t)(decoder->sb_column +
            ((decoder->square_root_child_index & 1U) != 0U ? 16U : 0U));
        for (index = 0U; index < path->decision_count; ++index) {
            const Av2hPartitionDecision *decision = &path->decisions[index];
            if (decision->node.row == root_row &&
                decision->node.column == root_column &&
                decision->node.block_size == AV2H_BLOCK_64X64) {
                decoder->square_child_luma_partition = decision->partition;
                break;
            }
        }
        if (decoder->square_child_luma_partition !=
                AV2H_PARTITION_INVALID) {
            root_partition = decoder->square_child_luma_partition;
            first_row = root_row;
            first_column = root_column;
            second_row = root_row;
            second_column = root_column;
            child_count = &decoder->square_child_luma_child_count;
            children_share = &decoder->square_child_luma_children_share;
            square_child = 1U;
        }
    }
    if (root_partition == AV2H_PARTITION_HORZ) {
        child_size = square_child ? AV2H_BLOCK_64X32 :
            decoder->root_block_size == AV2H_BLOCK_64X64 ?
            AV2H_BLOCK_64X32 :
            decoder->root_block_size == AV2H_BLOCK_128X128 ?
                AV2H_BLOCK_128X64 : AV2H_BLOCK_INVALID;
        second_row = (uint16_t)(second_row +
            (child_size == AV2H_BLOCK_128X64 ? 16U : 8U));
    } else if (root_partition == AV2H_PARTITION_VERT) {
        child_size = square_child ? AV2H_BLOCK_32X64 :
            decoder->root_block_size == AV2H_BLOCK_64X64 ?
            AV2H_BLOCK_32X64 :
            decoder->root_block_size == AV2H_BLOCK_128X128 ?
                AV2H_BLOCK_64X128 : AV2H_BLOCK_INVALID;
        second_column = (uint16_t)(second_column +
            (child_size == AV2H_BLOCK_64X128 ? 16U : 8U));
    } else {
        return;
    }
    if (child_size == AV2H_BLOCK_INVALID) return;
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        int is_first = decision->node.row == first_row &&
            decision->node.column == first_column;
        int is_second = decision->node.row == second_row &&
            decision->node.column == second_column;
        if (decision->node.block_size != child_size ||
            (!is_first && !is_second)) {
            continue;
        }
        ++*child_count;
        if (decision->partition == AV2H_PARTITION_NONE ||
            av2h_tile_partition_direction(decision->partition) ==
                av2h_tile_partition_direction(root_partition)) {
            *children_share = 0U;
        }
    }
}

static Av2hStatus tile_store_luma_partition_path(
    Av2hTileDecoder *decoder, const Av2hFirstLeafPath *path) {
    size_t index;
    if (decoder == 0 || path == 0) return AV2H_INVALID_ARGUMENT;
    if (path->decision_count > AV2H_MAX_LUMA_PARTITION_NODES -
            decoder->luma_partition_node_count) {
        return AV2H_OVERFLOW;
    }
    for (index = 0U; index < path->decision_count; ++index) {
        Av2hTileLumaPartitionNode *stored =
            &decoder->luma_partition_nodes[
                decoder->luma_partition_node_count++];
        stored->node = path->decisions[index].node;
        stored->partition = (uint8_t)path->decisions[index].partition;
    }
    return AV2H_OK;
}

static int luma_geometry_supported(const Av2hPartitionNode *leaf) {
    const Av2hTxDescriptor *descriptor;
    return leaf != 0 && av2h_luma_tx_descriptor_get(
        leaf->block_size, &descriptor) == AV2H_OK;
}

static Av2hStatus tile_handoff_square_child(Av2hTileDecoder *decoder,
                                             uint8_t *has_leaf) {
    Av2hPartitionNode chroma_root;
    const Av2hPartitionNode *pending = 0;
    size_t pending_index;
    uint8_t next;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (decoder->sb_row == 0U && decoder->sb_column == 480U &&
        decoder->square_root_child_index == 1U) {
        av2h_partition_diagnostic_leaf_stage(
            (Av2hPartitionNode){ 0U, 496U, AV2H_BLOCK_64X64 }, 90U,
            (uint8_t)decoder->luma_root_partition,
            (uint16_t)decoder->luma_traversal.stack_count,
            av2h_range_tell(&decoder->range));
    }
#endif
    if (!decoder->sequence->enable_sdp ||
        decoder->root_block_size != AV2H_BLOCK_128X128 ||
        decoder->square_child_luma_partition == AV2H_PARTITION_INVALID ||
        decoder->square_root_child_index >= 4U) {
        return AV2H_OK;
    }
    pending_index = decoder->luma_traversal.stack_count;
    while (pending_index != 0U) {
        const Av2hPartitionNode *candidate =
            &decoder->luma_traversal.stack[--pending_index];
        if (candidate->row < decoder->mi_rows &&
            candidate->column < decoder->mi_columns) {
            pending = candidate;
            break;
        }
    }
    if (pending != 0) {
        next = (uint8_t)((pending->row >= decoder->sb_row + 16U ? 2U : 0U) +
                         (pending->column >= decoder->sb_column + 16U ?
                              1U : 0U));
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        if (decoder->sb_row == 0U && decoder->sb_column == 480U &&
            decoder->square_root_child_index == 1U) {
            av2h_partition_diagnostic_leaf_stage(
                (Av2hPartitionNode){ 0U, 496U, AV2H_BLOCK_64X64 }, 91U,
                (uint8_t)pending->row, pending->column,
                av2h_range_tell(&decoder->range));
        }
#endif
        if (next == decoder->square_root_child_index) {
            return AV2H_OK;
        }
    }
    chroma_root.row = (uint16_t)(decoder->sb_row +
        (decoder->square_root_child_index >= 2U ? 16U : 0U));
    chroma_root.column = (uint16_t)(decoder->sb_column +
        ((decoder->square_root_child_index & 1U) != 0U ? 16U : 0U));
    chroma_root.block_size = AV2H_BLOCK_64X64;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (chroma_root.row == 0U && chroma_root.column == 496U) {
        av2h_partition_diagnostic_leaf_stage(
            chroma_root, 100U,
            (uint8_t)decoder->square_child_luma_partition,
            decoder->square_child_luma_child_count,
            av2h_range_tell(&decoder->range));
    }
#endif
    if (av2h_partition_traversal_reset_root(
            &decoder->chroma_traversal, chroma_root) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    decoder->chroma_root_partition = AV2H_PARTITION_INVALID;
    decoder->chroma_root_forced_partition = AV2H_PARTITION_INVALID;
    if (decoder->square_child_luma_partition == AV2H_PARTITION_NONE) {
        decoder->chroma_root_forced_partition = AV2H_PARTITION_NONE;
    } else if (decoder->square_child_luma_child_count == 2U &&
               decoder->square_child_luma_children_share &&
               av2h_tile_partition_direction(
                   decoder->square_child_luma_partition) != 0U) {
        decoder->chroma_root_forced_partition =
            decoder->square_child_luma_partition;
    }
    decoder->square_child_luma_child_count = 0U;
    decoder->square_child_luma_children_share = 1U;
    decoder->phase = AV2H_TILE_DECODE_CHROMA;
    decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
    *has_leaf = 0U;
    return AV2H_OK;
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
            cell->context.owner_row = node->row;
            cell->context.owner_column = node->column;
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
    uint16_t tx_count;
    Av2hStatus status;
    if (decoder == 0 || leaf == 0 || block == 0 || has_leaf == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase != AV2H_TILE_DECODE_LUMA) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->luma_tx_count != 0U) {
        if (decoder->luma_tx_index >= decoder->luma_tx_count) {
            return av2h_tile_decoder_fail(
                decoder, AV2H_INVALID_ARGUMENT);
        }
        av2h_memory_copy(
            &leaf->mode, &decoder->luma_coding_mode,
            sizeof(leaf->mode));
        status = decoder->frame->base_q_index == 0U &&
            decoder->luma_coding_mode.lossless_tx_size_large == 0U ?
            av2h_luma_lossless_tx_node_get(
                    &decoder->luma_coding_node, decoder->mi_columns,
                    decoder->mi_rows, decoder->luma_tx_index, &leaf->node) :
            av2h_luma_tx_node_get(
                &decoder->luma_coding_node, decoder->luma_tx_index,
                &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
        *has_leaf = 1U;
        goto read_transform;
    }
    av2h_tile_diagnostics_set_luma_stage(0U);
    status = tile_handoff_square_child(decoder, has_leaf);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (decoder->phase == AV2H_TILE_DECODE_CHROMA) return AV2H_OK;
    status = av2h_partition_traversal_next(
        &decoder->luma_traversal, &decoder->range,
        &decoder->partition_cdfs, &decoder->partition_contexts, &path,
        has_leaf);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_partition_path(decoder, &path);
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
    status = av2h_luma_mode_read_with_state_and_delta_q(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &decoder->luma_mode_contexts,
        &decoder->gdf_entropy, &decoder->cdef_entropy,
        &decoder->ccso_entropy, &decoder->current_q_index, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_luma_palette_map_read(
        &decoder->range, &decoder->leaf_cdfs, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (decoder->frame->base_q_index == 0U && leaf->mode.fsc_mode != 0U &&
        leaf->node.block_size != AV2H_BLOCK_4X4) {
        static const uint8_t size_group[AV2H_BLOCK_SIZE_COUNT] = {
            0U, 0U, 0U, 1U, 1U, 1U, 2U, 2U, 2U, 3U,
            3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U, 0U,
            0U, 1U, 1U, 2U, 2U, 1U, 1U, 2U, 2U
        };
        uint8_t group = size_group[leaf->node.block_size];
        leaf->mode.lossless_tx_size_large = (uint8_t)
            av2h_range_read_adaptive(
                &decoder->range,
                decoder->leaf_cdfs.lossless_tx_size[group], 2U);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_tile_decoder_fail(
                decoder, av2h_range_status(&decoder->range));
        }
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 60U, leaf->mode.gdf_flags_read,
        leaf->mode.cdef_strengths_read, leaf->mode.after_gdf_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 61U, leaf->mode.ccso_controls_read,
        leaf->mode.mode_index, leaf->mode.after_ccso_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 62U, leaf->mode.mode_set,
        leaf->mode.mode_index, leaf->mode.after_mode_index_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 63U, leaf->mode.fsc_mode,
        leaf->mode.mrl_index, leaf->mode.after_mrl_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 1U, (uint8_t)leaf->mode.mode, 0U,
        av2h_range_tell(&decoder->range));
#endif
    av2h_tile_diagnostics_set_luma_range(1U, decoder->range.range);
    status = av2h_luma_mode_context_state_update(
        &decoder->luma_mode_contexts, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_mode(decoder, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return av2h_tile_decoder_fail(decoder, AV2H_OUT_OF_MEMORY);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        if (decoder->sequence->enable_sdp &&
                ((leaf->node.block_size == AV2H_BLOCK_128X128 &&
                    decoder->luma_root_partition == AV2H_PARTITION_NONE) ||
                 ((leaf->node.block_size == AV2H_BLOCK_64X128 ||
                     leaf->node.block_size == AV2H_BLOCK_128X64) &&
                    av2h_tile_partition_direction(
                            decoder->luma_root_partition) != 0U))) {
        status = av2h_tile_decoder_read_shared_chroma_mode(
            decoder, &leaf->node);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, 70U, decoder->shared_chroma_mode.mode_index,
            decoder->shared_chroma_mode.cfl_kind,
            av2h_range_tell(&decoder->range));
#endif
    }
    status = decoder->frame->base_q_index == 0U &&
            leaf->mode.lossless_tx_size_large == 0U ?
        av2h_luma_lossless_tx_node_count(
            &leaf->node, decoder->mi_columns, decoder->mi_rows, &tx_count) :
        av2h_luma_tx_node_count(leaf->node.block_size, &tx_count);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (tx_count > 1U) {
        decoder->luma_coding_node = leaf->node;
        av2h_memory_copy(
            &decoder->luma_coding_mode, &leaf->mode,
            sizeof(decoder->luma_coding_mode));
        decoder->luma_tx_index = 0U;
        decoder->luma_tx_count = tx_count;
        status = decoder->frame->base_q_index == 0U &&
            decoder->luma_coding_mode.lossless_tx_size_large == 0U ?
            av2h_luma_lossless_tx_node_get(
                &decoder->luma_coding_node, decoder->mi_columns,
                decoder->mi_rows, 0U, &leaf->node) :
            av2h_luma_tx_node_get(
                &decoder->luma_coding_node, 0U, &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
    }
read_transform:
    if (leaf->mode.skip_transform != 0U) {
        status = tile_initialize_skipped_intrabc(
            decoder, leaf, &leaf->prefix, &leaf->transform, block);
        if (status == AV2H_UNSUPPORTED) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
        }
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        leaf->prefix.q_index = decoder->current_q_index;
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(2U);
    if (decoder->luma_tx_count != 0U) {
        uint8_t txb_skip_context;
        status = av2h_coeff_contexts_from_state_for_coding_block(
            &decoder->coefficient_contexts, 0U, &leaf->node,
            decoder->luma_coding_node.block_size, leaf->mode.fsc_mode,
            &txb_skip_context, &dc_sign_context);
        if (status == AV2H_OK) {
            status = av2h_coeff_prefix_read(
                &decoder->range, &decoder->coeff_cdfs, decoder->frame,
                &leaf->node, &leaf->mode, txb_skip_context, &leaf->prefix);
        }
    } else {
        status = av2h_coeff_prefix_read_with_state(
            &decoder->range, &decoder->coeff_cdfs, decoder->frame,
            &leaf->node, &leaf->mode, &decoder->coefficient_contexts,
            &leaf->prefix, &dc_sign_context);
    }
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    leaf->prefix.q_index = decoder->current_q_index;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 21U, leaf->prefix.eob_point, leaf->prefix.eob_extra,
        leaf->prefix.after_eob_point_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 22U, leaf->prefix.eob_offset_bits,
        leaf->prefix.eob_extra, leaf->prefix.after_eob_extra_bit);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 2U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        av2h_range_tell(&decoder->range));
#endif
    av2h_tile_diagnostics_set_luma_range(2U, decoder->range.range);
    if (leaf->prefix.all_zero) {
        status = tile_initialize_all_zero_luma(
            decoder, leaf, &leaf->transform, block);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(3U);
    if (decoder->current_q_index == 0U && leaf->mode.fsc_mode == 0U) {
        size_t tell = av2h_range_tell(&decoder->range);
        av2h_memory_fill(&leaf->transform, 0U, sizeof(leaf->transform));
        leaf->transform.primary_type = AV2H_TX_DCT_DCT;
        leaf->transform.before_primary_bits = tell;
        leaf->transform.after_primary_bits = tell;
        leaf->transform.after_secondary_type_bits = tell;
        leaf->transform.after_secondary_bits = tell;
        status = AV2H_OK;
    } else {
        status = av2h_first_transform_type_read(
            &decoder->range, &decoder->transform_cdfs, decoder->sequence,
            decoder->frame, &leaf->mode, &leaf->prefix, &leaf->transform);
    }
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 3U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        av2h_range_tell(&decoder->range));
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, 89U, leaf->transform.primary_type,
            leaf->transform.secondary_symbol, leaf->prefix.tx_size);
    }
#endif
    av2h_tile_diagnostics_set_luma_range(3U, decoder->range.range);
    av2h_tile_diagnostics_set_luma_stage(4U);
    status = av2h_coeff_block_read_with_state(
        &decoder->range, &decoder->level_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &leaf->mode, &leaf->prefix,
        &leaf->transform, &decoder->coefficient_contexts, block);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        size_t coefficient_index;
        for (coefficient_index = 0U; coefficient_index < 8U;
             ++coefficient_index) {
            av2h_partition_diagnostic_leaf_stage(
                leaf->node, (uint8_t)(80U + coefficient_index),
                (uint8_t)block->quantized[coefficient_index],
                (uint16_t)block->dequantized[coefficient_index],
                coefficient_index);
        }
    }
    if (leaf->node.row == 184U && leaf->node.column == 280U &&
        leaf->node.block_size == AV2H_BLOCK_32X32) {
        uint16_t scan_index = leaf->prefix.eob;
        while (scan_index != 0U) {
            uint16_t position;
            --scan_index;
            position = av2h_coeff_scan_position(
                scan_index, block->width, block->height, 0U, 0U);
            av2h_partition_diagnostic_leaf_stage(
                leaf->node, 40U, block->base_levels[position], position,
                scan_index);
        }
    }
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 31U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        block->after_levels_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 4U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        av2h_range_tell(&decoder->range));
#endif
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
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        size_t residual_index;
        for (residual_index = 0U; residual_index < 8U; ++residual_index) {
            av2h_partition_diagnostic_leaf_stage(
                leaf->node, (uint8_t)(90U + residual_index), 0U,
                (uint16_t)scratch->residual[residual_index], residual_index);
        }
    }
#endif
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}