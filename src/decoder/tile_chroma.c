#include "tile.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"
#include "tx_geometry.h"

static uint8_t tile_cfl_allowed_for_sdp(
    const Av2hTileDecoder *decoder,
    Av2hBlockSize block_size) {
    Av2hPartitionType luma_root_partition = decoder->luma_root_partition;
    uint8_t luma_direction;
    uint8_t chroma_direction;
    if (decoder->frame->base_q_index == 0U &&
        decoder->frame->chroma_format_idc == 2U &&
        block_size != AV2H_BLOCK_4X4) {
        return 0U;
    }
    if (!decoder->sequence->enable_sdp) return 1U;
    if (block_size == AV2H_BLOCK_128X128 ||
        block_size == AV2H_BLOCK_64X128 ||
        block_size == AV2H_BLOCK_128X64) {
        return 1U;
    }
    if (decoder->root_block_size == AV2H_BLOCK_128X128 &&
        decoder->square_root_child_index < 4U &&
        decoder->square_child_luma_partition != AV2H_PARTITION_INVALID) {
        luma_root_partition = decoder->square_child_luma_partition;
    }
    if (decoder->chroma_root_partition == AV2H_PARTITION_NONE) return 1U;
    if (luma_root_partition == decoder->chroma_root_partition) {
        return 1U;
    }
    luma_direction = av2h_tile_partition_direction(
        luma_root_partition);
    chroma_direction = av2h_tile_partition_direction(
        decoder->chroma_root_partition);
    return (uint8_t)(luma_direction != 0U &&
                     luma_direction == chroma_direction);
}

static int tile_luma_partition_lookup(
    const Av2hTileDecoder *decoder,
    Av2hPartitionNode node,
    Av2hPartitionType *partition) {
    size_t index;
    for (index = decoder->luma_partition_node_count; index != 0U; --index) {
        const Av2hTileLumaPartitionNode *stored =
            &decoder->luma_partition_nodes[index - 1U];
        if (stored->node.row == node.row &&
            stored->node.column == node.column &&
            stored->node.block_size == node.block_size) {
            *partition = (Av2hPartitionType)stored->partition;
            return 1;
        }
    }
    return 0;
}

static int tile_sdp_child_shares(Av2hPartitionType parent,
                                 Av2hPartitionType child) {
    if (child == AV2H_PARTITION_NONE) return 0;
    if (parent == AV2H_PARTITION_HORZ ||
        parent == AV2H_PARTITION_HORZ_3) {
        return child != AV2H_PARTITION_HORZ &&
               child != AV2H_PARTITION_HORZ_3 &&
               child != AV2H_PARTITION_HORZ_4A &&
               child != AV2H_PARTITION_HORZ_4B;
    }
    if (parent == AV2H_PARTITION_VERT ||
        parent == AV2H_PARTITION_VERT_3) {
        return child != AV2H_PARTITION_VERT &&
               child != AV2H_PARTITION_VERT_3 &&
               child != AV2H_PARTITION_VERT_4A &&
               child != AV2H_PARTITION_VERT_4B;
    }
    return 0;
}

static int tile_derived_chroma_partition(
    const Av2hTileDecoder *decoder,
    Av2hPartitionNode node,
    Av2hPartitionType *partition) {
    Av2hPartitionNode children[4U];
    Av2hPartitionType luma_partition;
    size_t child_count;
    size_t index;
    if (!decoder->sequence->enable_sdp ||
        node.block_size < AV2H_BLOCK_64X64 ||
        node.block_size > AV2H_BLOCK_256X256 ||
        !tile_luma_partition_lookup(decoder, node, &luma_partition)) {
        return 0;
    }
    if (node.block_size > AV2H_BLOCK_64X64 ||
        luma_partition == AV2H_PARTITION_NONE) {
        *partition = luma_partition;
        return 1;
    }
    if (av2h_partition_child_nodes(
            node, luma_partition, children, &child_count) != AV2H_OK) {
        return 0;
    }
    for (index = 0U; index < child_count; ++index) {
        Av2hPartitionType child_partition;
        if (tile_luma_partition_lookup(
                decoder, children[index], &child_partition) &&
            !tile_sdp_child_shares(luma_partition, child_partition)) {
            return 0;
        }
    }
    *partition = luma_partition;
    return 1;
}

static Av2hStatus tile_store_chroma_mode(
    Av2hTileDecoder *decoder,
    const Av2hPartitionNode *node,
    const Av2hFirstChromaMode *mode) {
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
            decoder->colocated_luma[index].chroma_cfl = mode->is_cfl;
            decoder->colocated_luma[index].chroma_smooth = (uint8_t)(
                !mode->is_cfl &&
                (mode->mode == AV2H_INTRA_SMOOTH ||
                 mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                 mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL));
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_read_shared_chroma_mode(
    Av2hTileDecoder *decoder,
    const Av2hPartitionNode *node) {
    Av2hTileLumaModeCell *colocated;
    uint8_t cfl_context = 0U;
    size_t index;
    Av2hStatus status;
    if (decoder == 0 || node == 0 ||
        (node->block_size != AV2H_BLOCK_128X128 &&
         node->block_size != AV2H_BLOCK_64X128 &&
         node->block_size != AV2H_BLOCK_128X64)) {
        return AV2H_INVALID_ARGUMENT;
    }
    index = (size_t)node->row * decoder->mi_columns + node->column;
    if (index >= decoder->colocated_luma_count) return AV2H_OVERFLOW;
    colocated = &decoder->colocated_luma[index];
    if (!colocated->context.present) return AV2H_INVALID_DATA;
    if (node->row != 0U) {
        cfl_context = decoder->colocated_luma[
            ((size_t)node->row - 1U) * decoder->mi_columns +
            node->column].chroma_cfl;
    }
    if (node->column != 0U) {
        cfl_context = (uint8_t)(cfl_context + decoder->colocated_luma[
            (size_t)node->row * decoder->mi_columns +
            node->column - 1U].chroma_cfl);
    }
    status = av2h_chroma_mode_read_with_delta_for_sdp(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        (Av2hFirstLeafIntraMode)colocated->mode, colocated->angle_delta,
        node->block_size, cfl_context,
        tile_cfl_allowed_for_sdp(decoder, node->block_size),
        (uint8_t)(colocated->context.q_index == 0U),
        &decoder->shared_chroma_mode);
    if (status != AV2H_OK) return status;
    decoder->shared_chroma_mode.above_smooth = node->row == 0U ? 0U :
        decoder->colocated_luma[
            ((size_t)node->row - 1U) * decoder->mi_columns +
            node->column].chroma_smooth;
    decoder->shared_chroma_mode.left_smooth = node->column == 0U ? 0U :
        decoder->colocated_luma[
            (size_t)node->row * decoder->mi_columns +
            node->column - 1U].chroma_smooth;
    status = tile_store_chroma_mode(
        decoder, node, &decoder->shared_chroma_mode);
    if (status != AV2H_OK) return status;
    decoder->shared_chroma_mode_pending = 1U;
    decoder->shared_chroma_node = *node;
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_next_chroma(
    Av2hTileDecoder *decoder,
    Av2hTileChromaLeaf *leaf,
    Av2hFirstCoeffBlock *u_block,
    Av2hFirstCoeffBlock *v_block,
    uint8_t *has_leaf) {
    Av2hFirstLeafPath path;
    Av2hTileLumaModeCell *colocated;
    uint8_t cfl_context;
    uint8_t continuing_transform;
    Av2hFirstCoeffBlock *blocks[2U];
    size_t index;
    Av2hStatus status;
    if (decoder == 0 || leaf == 0 || u_block == 0 || v_block == 0 ||
        has_leaf == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase != AV2H_TILE_DECODE_CHROMA) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    av2h_tile_diagnostics_set_chroma_stage(0U);
    av2h_tile_diagnostics_set_chroma_reason(1U);
    continuing_transform = decoder->chroma_tx_count != 0U;
    if (continuing_transform) {
        path.decision_count = 0U;
        path.leaf = decoder->chroma_coding_node;
        *has_leaf = 1U;
        status = AV2H_OK;
    } else if (decoder->chroma_root_forced_partition ==
                   AV2H_PARTITION_INVALID &&
               decoder->chroma_traversal.stack_count != 0U) {
        Av2hPartitionType derived_partition;
        path.decision_count = 0U;
        *has_leaf = 0U;
        status = AV2H_OK;
        while (decoder->chroma_traversal.stack_count != 0U &&
               tile_derived_chroma_partition(
                   decoder,
                   decoder->chroma_traversal.stack[
                       decoder->chroma_traversal.stack_count - 1U],
                   &derived_partition)) {
            if (decoder->chroma_root_partition == AV2H_PARTITION_INVALID) {
                decoder->chroma_root_partition = derived_partition;
            }
            status = av2h_partition_traversal_force(
                &decoder->chroma_traversal, derived_partition, has_leaf);
            if (status != AV2H_OK || *has_leaf) break;
        }
        if (status == AV2H_OK && !*has_leaf) {
            status = av2h_partition_traversal_next(
                &decoder->chroma_traversal, &decoder->range,
                &decoder->partition_cdfs, &decoder->partition_contexts,
                &path, has_leaf);
        }
    } else if (decoder->chroma_root_forced_partition != AV2H_PARTITION_INVALID) {
        Av2hPartitionNode root = {
            decoder->sb_row, decoder->sb_column, decoder->root_block_size
        };
        if (decoder->shared_chroma_commit_luma ||
            (decoder->shared_chroma_resume_luma &&
             decoder->shared_chroma_mode_pending)) {
            root = decoder->shared_chroma_node;
        } else if (decoder->sequence->enable_sdp &&
            decoder->root_block_size == AV2H_BLOCK_128X128 &&
            decoder->square_root_child_index < 4U) {
            root.row = (uint16_t)(root.row +
                (decoder->square_root_child_index >= 2U ? 16U : 0U));
            root.column = (uint16_t)(root.column +
                ((decoder->square_root_child_index & 1U) != 0U ? 16U : 0U));
            root.block_size = AV2H_BLOCK_64X64;
        }
        if (decoder->chroma_traversal.stack_count != 1U ||
            decoder->chroma_traversal.has_pending_leaf ||
            decoder->chroma_traversal.stack[0].row != root.row ||
            decoder->chroma_traversal.stack[0].column != root.column ||
            decoder->chroma_traversal.stack[0].block_size !=
                root.block_size) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
        }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_forced_decision(
            root, decoder->chroma_root_forced_partition,
            decoder->chroma_traversal.structure);
#endif
        decoder->chroma_root_partition =
            decoder->chroma_root_forced_partition;
        if (decoder->chroma_root_forced_partition == AV2H_PARTITION_NONE) {
            decoder->chroma_traversal.stack_count = 0U;
            decoder->chroma_traversal.pending_leaf = root;
            decoder->chroma_traversal.has_pending_leaf = 1U;
            path.decision_count = 0U;
            path.leaf = root;
            *has_leaf = 1U;
            status = AV2H_OK;
        } else {
            Av2hPartitionNode first = root;
            Av2hPartitionNode second = root;
            if (decoder->chroma_root_forced_partition ==
                    AV2H_PARTITION_SPLIT) {
                Av2hPartitionNode third = root;
                Av2hPartitionNode fourth = root;
                if (root.block_size != AV2H_BLOCK_128X128) {
                    return av2h_tile_decoder_fail(
                        decoder, AV2H_UNSUPPORTED);
                }
                first.block_size = AV2H_BLOCK_64X64;
                second = first;
                third = first;
                fourth = first;
                second.column = (uint16_t)(second.column + 16U);
                third.row = (uint16_t)(third.row + 16U);
                fourth.row = third.row;
                fourth.column = second.column;
                decoder->chroma_traversal.stack[0] = fourth;
                decoder->chroma_traversal.stack[1] = third;
                decoder->chroma_traversal.stack[2] = second;
                decoder->chroma_traversal.stack[3] = first;
                decoder->chroma_traversal.stack_count = 4U;
            } else if (decoder->chroma_root_forced_partition ==
                    AV2H_PARTITION_HORZ) {
                first.block_size = root.block_size ==
                    AV2H_BLOCK_64X64 ? AV2H_BLOCK_64X32 :
                    AV2H_BLOCK_128X64;
                second = first;
                second.row = (uint16_t)(second.row +
                    (root.block_size == AV2H_BLOCK_64X64 ? 8U : 16U));
            } else if (decoder->chroma_root_forced_partition ==
                       AV2H_PARTITION_VERT) {
                first.block_size = root.block_size ==
                    AV2H_BLOCK_64X64 ? AV2H_BLOCK_32X64 :
                    AV2H_BLOCK_64X128;
                second = first;
                second.column = (uint16_t)(second.column +
                    (root.block_size == AV2H_BLOCK_64X64 ? 8U : 16U));
            } else {
                return av2h_tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
            }
            if (decoder->chroma_root_forced_partition !=
                AV2H_PARTITION_SPLIT) {
                decoder->chroma_traversal.stack[0] = second;
                decoder->chroma_traversal.stack[1] = first;
                decoder->chroma_traversal.stack_count = 2U;
            }
            status = av2h_partition_traversal_next(
                &decoder->chroma_traversal, &decoder->range,
                &decoder->partition_cdfs, &decoder->partition_contexts,
                &path, has_leaf);
        }
        decoder->chroma_root_forced_partition = AV2H_PARTITION_INVALID;
    } else {
        status = av2h_partition_traversal_next(
            &decoder->chroma_traversal, &decoder->range,
            &decoder->partition_cdfs, &decoder->partition_contexts, &path,
            has_leaf);
    }
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (!*has_leaf) {
        return av2h_tile_decoder_finish_root(decoder);
    }
    av2h_tile_diagnostics_set_chroma_reason(2U);
    av2h_tile_capture_root_partition(
        decoder, &path, &decoder->chroma_root_partition);
    leaf->node = path.leaf;
    index = (size_t)leaf->node.row * decoder->mi_columns + leaf->node.column;
    if (index >= decoder->colocated_luma_count) {
        av2h_tile_diagnostics_set_chroma_reason(3U);
        return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
    }
    colocated = &decoder->colocated_luma[index];
    if (!colocated->context.present) {
        av2h_tile_diagnostics_set_chroma_reason(4U);
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
    }
    if (!continuing_transform) {
        decoder->current_block_q_index = colocated->context.q_index;
    }
    cfl_context = 0U;
    if (leaf->node.row != 0U) {
        cfl_context = decoder->colocated_luma[
            ((size_t)leaf->node.row - 1U) * decoder->mi_columns +
            leaf->node.column].chroma_cfl;
    }
    if (leaf->node.column != 0U) {
        cfl_context = (uint8_t)(cfl_context +
            decoder->colocated_luma[
                (size_t)leaf->node.row * decoder->mi_columns +
                leaf->node.column - 1U].chroma_cfl);
    }
    av2h_tile_diagnostics_set_chroma_reason(5U);
    if (decoder->chroma_tx_count != 0U) {
        av2h_memory_copy(
            &leaf->mode, &decoder->chroma_coding_mode, sizeof(leaf->mode));
    } else if (decoder->shared_chroma_mode_pending &&
        leaf->node.row == decoder->shared_chroma_node.row &&
        leaf->node.column == decoder->shared_chroma_node.column &&
        leaf->node.block_size == decoder->shared_chroma_node.block_size) {
        av2h_memory_copy(
            &leaf->mode, &decoder->shared_chroma_mode, sizeof(leaf->mode));
        decoder->shared_chroma_mode_pending = 0U;
    } else if (decoder->frame->base_q_index == 0U) {
        av2h_memory_fill(&leaf->mode, 0U, sizeof(leaf->mode));
        leaf->mode.use_dpcm_uv = (uint8_t)av2h_range_read_adaptive(
            &decoder->range, decoder->leaf_cdfs.dpcm_uv, 2U);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_tile_decoder_fail(
                decoder, av2h_range_status(&decoder->range));
        }
        if (leaf->mode.use_dpcm_uv != 0U) {
            leaf->mode.dpcm_mode_uv = (uint8_t)av2h_range_read_adaptive(
                &decoder->range,
                decoder->leaf_cdfs.dpcm_uv_vert_horz, 2U);
            if (av2h_range_status(&decoder->range) != AV2H_OK) {
                return av2h_tile_decoder_fail(
                    decoder, av2h_range_status(&decoder->range));
            }
            leaf->mode.mode = leaf->mode.dpcm_mode_uv != 0U ?
                AV2H_INTRA_HORIZONTAL : AV2H_INTRA_VERTICAL;
            leaf->mode.uv_mode = leaf->mode.dpcm_mode_uv != 0U ? 2U : 1U;
            leaf->mode.mode_index = leaf->mode.uv_mode;
            leaf->mode.angle_delta = leaf->mode.mode == colocated->mode ?
                colocated->angle_delta : 0;
            leaf->mode.chroma_format_idc =
                decoder->sequence->info.chroma_format_idc;
            leaf->mode.superblock_size = decoder->sequence->superblock_size;
            leaf->mode.edge_ibp = decoder->sequence->enable_ibp;
            status = AV2H_OK;
        } else {
            status = av2h_chroma_mode_read_with_delta_for_sdp(
                &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
                (Av2hFirstLeafIntraMode)colocated->mode,
                colocated->angle_delta, leaf->node.block_size, cfl_context,
                tile_cfl_allowed_for_sdp(
                    decoder, leaf->node.block_size), 1U, &leaf->mode);
        }
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    } else {
        status = av2h_chroma_mode_read_with_delta_for_sdp(
            &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
            (Av2hFirstLeafIntraMode)colocated->mode, colocated->angle_delta,
            leaf->node.block_size, cfl_context,
            tile_cfl_allowed_for_sdp(
                decoder, leaf->node.block_size), 0U, &leaf->mode);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, 71U, leaf->mode.mode_index, leaf->mode.cfl_kind,
            av2h_range_tell(&decoder->range));
#endif
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 50U, leaf->mode.mode_index, leaf->mode.cfl_kind,
        av2h_range_tell(&decoder->range));
#endif
    if (!continuing_transform) {
        leaf->mode.above_smooth = leaf->node.row == 0U ? 0U :
            decoder->colocated_luma[
                ((size_t)leaf->node.row - 1U) * decoder->mi_columns +
                leaf->node.column].chroma_smooth;
        leaf->mode.left_smooth = leaf->node.column == 0U ? 0U :
            decoder->colocated_luma[
                (size_t)leaf->node.row * decoder->mi_columns +
                leaf->node.column - 1U].chroma_smooth;
    }
    av2h_tile_diagnostics_set_chroma_reason(0U);
    if (!continuing_transform && !decoder->shared_chroma_resume_luma) {
        status = tile_store_chroma_mode(decoder, &leaf->node, &leaf->mode);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    }
    if (decoder->chroma_tx_count == 0U) {
        uint16_t tx_count;
        status = decoder->frame->base_q_index == 0U ?
            av2h_chroma_lossless_tx_node_count(
                &leaf->node, decoder->mi_columns, decoder->mi_rows,
                decoder->frame->chroma_format_idc, &tx_count) :
            av2h_chroma_tx_node_count(
                leaf->node.block_size, decoder->frame->chroma_format_idc,
                &tx_count);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
        if (tx_count > 1U) {
            decoder->chroma_tx_count = tx_count;
            decoder->chroma_coding_node = leaf->node;
            av2h_memory_copy(
                &decoder->chroma_coding_mode, &leaf->mode,
                sizeof(leaf->mode));
            decoder->chroma_tx_index = 0U;
            decoder->chroma_tx_unit_start = 0U;
        }
    }
    if (decoder->chroma_tx_count != 0U) {
        status = decoder->frame->base_q_index == 0U ?
            av2h_chroma_lossless_tx_node_get(
                &decoder->chroma_coding_node,
                decoder->mi_columns, decoder->mi_rows,
                decoder->frame->chroma_format_idc,
                decoder->chroma_tx_index, &leaf->node) :
            av2h_chroma_tx_node_get(
                &decoder->chroma_coding_node,
                decoder->frame->chroma_format_idc,
                decoder->chroma_tx_index, &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
    }
    blocks[0] = u_block;
    blocks[1] = v_block;
    leaf->decoded_plane = 0U;
    if (decoder->frame->base_q_index == 0U &&
        decoder->chroma_tx_count > 1U) {
        if (decoder->chroma_tx_plane == 0U) {
            decoder->chroma_tx_plane = 1U;
        }
        leaf->decoded_plane = decoder->chroma_tx_plane;
        index = (size_t)decoder->chroma_tx_plane - 1U;
    } else {
        index = 0U;
    }
    for (; index < (leaf->decoded_plane == 0U ? 2U :
                    (size_t)leaf->decoded_plane); ++index) {
        Av2hBlockSize coding_block_size = decoder->chroma_tx_count != 0U ?
            decoder->chroma_coding_node.block_size : leaf->node.block_size;
        uint8_t plane = (uint8_t)(index + 1U);
        uint16_t u_eob = index != 0U ? leaf->prefix[0].eob : 0U;
        size_t block_width_mi;
        size_t block_height_mi;
        uint8_t cctx_allowed;
        status = av2h_block_dimensions_mi(
            coding_block_size, &block_width_mi, &block_height_mi);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        cctx_allowed = (uint8_t)(
                        decoder->sequence->enable_cctx && decoder->current_block_q_index != 0U &&
                        !(decoder->frame->chroma_format_idc == 2U &&
                            block_width_mi >= 8U && block_height_mi >= 8U));
        av2h_tile_diagnostics_set_chroma_stage(
            (uint8_t)(1U + index * 3U));
        if (leaf->decoded_plane == 2U) {
            u_eob = decoder->chroma_u_eob;
        }
        status = av2h_chroma_coeff_prefix_read_with_state(
            &decoder->range, &decoder->coeff_cdfs, decoder->frame,
            &leaf->node, coding_block_size,
            plane, u_eob,
            &decoder->coefficient_contexts,
            &leaf->prefix[index]);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        leaf->prefix[index].q_index = decoder->current_block_q_index;
        if (leaf->decoded_plane == 1U) {
            decoder->chroma_u_eob = leaf->prefix[index].eob;
            if (decoder->chroma_tx_index >=
                AV2H_MAX_LOSSLESS_CHROMA_TX_NODES) {
                return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
            }
            decoder->chroma_u_eobs[decoder->chroma_tx_index] =
                leaf->prefix[index].eob;
        }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, (uint8_t)(51U + index * 3U),
            leaf->prefix[index].all_zero, leaf->prefix[index].eob,
            av2h_range_tell(&decoder->range));
#endif
        av2h_tile_diagnostics_set_chroma_stage(
            (uint8_t)(2U + index * 3U));
        if (decoder->current_block_q_index == 0U) {
            size_t tell = av2h_range_tell(&decoder->range);
            av2h_memory_fill(
                &leaf->transform[index], 0U,
                sizeof(leaf->transform[index]));
            leaf->transform[index].primary_type =
                colocated->context.fsc_mode != 0U ?
                    AV2H_TX_IDTX : AV2H_TX_DCT_DCT;
            leaf->transform[index].before_primary_bits = tell;
            leaf->transform[index].after_primary_bits = tell;
            leaf->transform[index].after_secondary_type_bits = tell;
            leaf->transform[index].after_secondary_bits = tell;
            status = AV2H_OK;
        } else {
            status = av2h_chroma_transform_type_read(
                &decoder->range, &decoder->transform_cdfs, decoder->frame,
                &leaf->mode, plane, cctx_allowed, &leaf->prefix[index],
                &leaf->transform[index]);
        }
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    #if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, (uint8_t)(52U + index * 3U),
            leaf->transform[index].primary_type,
            leaf->transform[index].cctx_type,
            av2h_range_tell(&decoder->range));
    #endif
        av2h_tile_diagnostics_set_chroma_stage(
            (uint8_t)(3U + index * 3U));
        status = av2h_chroma_coeff_block_read_with_state(
            &decoder->range, &decoder->level_cdfs, decoder->sequence,
            decoder->frame, &leaf->node, plane, &leaf->prefix[index],
            &leaf->transform[index],
            &decoder->coefficient_contexts, blocks[index]);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    #if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, (uint8_t)(53U + index * 3U),
            blocks[index]->cul_level, blocks[index]->nonzero_count,
            av2h_range_tell(&decoder->range));
    #endif
    }
    if (leaf->transform[0].cctx_type != 0U && leaf->prefix[1].all_zero) {
        leaf->transform[1].primary_type = leaf->transform[0].primary_type;
    }
    decoder->event = AV2H_TILE_EVENT_CHROMA_LEAF;
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_reconstruct_next_chroma(
    Av2hTileDecoder *decoder,
    Av2hReconstructionPlane *u_plane,
    Av2hReconstructionPlane *v_plane,
    Av2hReconstructionScratch *scratch,
    Av2hTileChromaLeaf *leaf,
    uint8_t *has_leaf) {
    Av2hFirstCoeffBlock u_block;
    Av2hFirstCoeffBlock v_block;
    Av2hStatus status;
    if (decoder == 0 || u_plane == 0 || v_plane == 0 || scratch == 0 ||
        leaf == 0 || has_leaf == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tile_decoder_next_chroma(
        decoder, leaf, &u_block, &v_block, has_leaf);
    if (status != AV2H_OK || !*has_leaf) return status;
    status = av2h_chroma_inverse_cross_transform(
        &u_block, &v_block, leaf->transform[0].cctx_type,
        decoder->sequence->info.bit_depth_idc == 0U ? 10U : 8U);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_chroma_reconstruct_to_plane_format(
        &leaf->node, &leaf->mode, &leaf->prefix[0], &leaf->transform[0],
        &u_block, u_plane, 0, 1U, 0,
        decoder->sequence->info.chroma_format_idc, scratch);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_chroma_reconstruct_to_plane_format(
        &leaf->node, &leaf->mode, &leaf->prefix[1], &leaf->transform[1],
        &v_block, v_plane, 0, 2U, 0,
        decoder->sequence->info.chroma_format_idc, scratch);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}