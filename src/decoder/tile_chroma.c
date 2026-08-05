#include "tile.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"

static uint8_t tile_cfl_allowed_for_sdp(const Av2hTileDecoder *decoder) {
    Av2hPartitionType luma_root_partition = decoder->luma_root_partition;
    uint8_t luma_direction;
    uint8_t chroma_direction;
    if (!decoder->sequence->enable_sdp) return 1U;
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

Av2hStatus av2h_tile_decoder_next_chroma(
    Av2hTileDecoder *decoder,
    Av2hTileChromaLeaf *leaf,
    Av2hFirstCoeffBlock *u_block,
    Av2hFirstCoeffBlock *v_block,
    uint8_t *has_leaf) {
    Av2hFirstLeafPath path;
    Av2hTileLumaModeCell *colocated;
    uint8_t cfl_context;
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
    if (decoder->chroma_root_forced_partition != AV2H_PARTITION_INVALID) {
        Av2hPartitionNode root = {
            decoder->sb_row, decoder->sb_column, decoder->root_block_size
        };
        if (decoder->sequence->enable_sdp &&
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
    status = av2h_chroma_mode_read_with_delta_for_sdp(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        (Av2hFirstLeafIntraMode)colocated->mode, colocated->angle_delta,
        leaf->node.block_size, cfl_context,
        tile_cfl_allowed_for_sdp(decoder), &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    leaf->mode.above_smooth = leaf->node.row == 0U ? 0U :
        decoder->colocated_luma[
            ((size_t)leaf->node.row - 1U) * decoder->mi_columns +
            leaf->node.column].chroma_smooth;
    leaf->mode.left_smooth = leaf->node.column == 0U ? 0U :
        decoder->colocated_luma[
            (size_t)leaf->node.row * decoder->mi_columns +
            leaf->node.column - 1U].chroma_smooth;
    av2h_tile_diagnostics_set_chroma_reason(0U);
    status = tile_store_chroma_mode(decoder, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    blocks[0] = u_block;
    blocks[1] = v_block;
    for (index = 0U; index < 2U; ++index) {
        uint8_t plane = (uint8_t)(index + 1U);
        av2h_tile_diagnostics_set_chroma_stage(
            (uint8_t)(1U + index * 3U));
        status = av2h_chroma_coeff_prefix_read_with_state(
            &decoder->range, &decoder->coeff_cdfs, decoder->frame,
            &leaf->node, plane, &decoder->coefficient_contexts,
            &leaf->prefix[index]);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        av2h_tile_diagnostics_set_chroma_stage(
            (uint8_t)(2U + index * 3U));
        status = av2h_chroma_transform_type_read(
            &decoder->range, &decoder->transform_cdfs, decoder->frame,
            &leaf->mode, plane, &leaf->prefix[index],
            &leaf->transform[index]);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        av2h_tile_diagnostics_set_chroma_stage(
            (uint8_t)(3U + index * 3U));
        status = av2h_chroma_coeff_block_read_with_state(
            &decoder->range, &decoder->level_cdfs, decoder->sequence,
            decoder->frame, &leaf->node, plane, &leaf->prefix[index],
            &decoder->coefficient_contexts, blocks[index]);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
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
        &u_block, &v_block, leaf->transform[0].cctx_type);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_chroma_reconstruct_to_plane_ex(
        &leaf->node, &leaf->mode, &leaf->prefix[0], &leaf->transform[0],
        &u_block, u_plane, 0, 1U, 0, scratch);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_chroma_reconstruct_to_plane_ex(
        &leaf->node, &leaf->mode, &leaf->prefix[1], &leaf->transform[1],
        &v_block, v_plane, 0, 2U, 0, scratch);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}