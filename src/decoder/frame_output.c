#include "decoder/frame_output.h"

#include "decoder/internal/decode_diagnostics.h"
#include "decoder/internal/filter_diagnostics.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/internal/tile_modules.h"
#include "decoder/tx_geometry.h"

#include <stddef.h>

static void luma_failure_context(
    Av2hFrameDecodeResult *decoded,
    const Av2hTileLumaLeaf *leaf,
    const Av2hReconstructionDiagnostics *diagnostics) {
    decoded->mode = (uint8_t)leaf->mode.mode;
    decoded->angle_delta = leaf->mode.angle_delta;
    decoded->mrl_index = leaf->mode.mrl_index;
    decoded->multi_line_mrl = leaf->mode.multi_line_mrl;
    decoded->enable_ibp = leaf->mode.enable_ibp;
    decoded->use_intrabc = leaf->mode.use_intrabc;
    decoded->displacement_x = leaf->mode.displacement_x;
    decoded->displacement_y = leaf->mode.displacement_y;
    decoded->morph_pred = leaf->mode.morph_pred;
    decoded->fsc_mode = leaf->mode.fsc_mode;
    decoded->skip_transform = leaf->mode.skip_transform;
    decoded->palette_size = leaf->mode.palette_size;
    decoded->tx_size = (uint8_t)leaf->prefix.tx_size;
    decoded->inverse_transform_stage =
        diagnostics != NULL ?
            diagnostics->inverse_transform_stage : 0U;
    decoded->intrabc_source_x = diagnostics != NULL ?
        diagnostics->intrabc_source_x : 0U;
    decoded->intrabc_source_y = diagnostics != NULL ?
        diagnostics->intrabc_source_y : 0U;
    decoded->intrabc_map_index = diagnostics != NULL ?
        diagnostics->intrabc_map_index : 0U;
    decoded->primary_type = (uint8_t)leaf->transform.primary_type;
    decoded->primary_symbol = leaf->transform.primary_symbol;
    decoded->secondary_symbol = leaf->transform.secondary_symbol;
    decoded->secondary_set = leaf->transform.secondary_set;
    decoded->eob = leaf->prefix.eob;
}

static void chroma_failure_context(
    Av2hFrameDecodeResult *decoded,
    const Av2hTileChromaLeaf *leaf,
    unsigned int index) {
    decoded->mode = (uint8_t)leaf->mode.mode;
    decoded->angle_delta = leaf->mode.angle_delta;
    decoded->tx_size = (uint8_t)leaf->prefix[index].tx_size;
    decoded->primary_type = (uint8_t)leaf->transform[index].primary_type;
    decoded->primary_symbol = leaf->transform[index].primary_symbol;
    decoded->secondary_symbol = leaf->transform[index].secondary_symbol;
    decoded->secondary_set = leaf->transform[index].secondary_set;
    decoded->eob = leaf->prefix[index].eob;
}

static uint8_t *tile_readiness_map(
    Av2hFrameDecodeWorkspace *workspace,
    unsigned int plane,
    size_t *stride,
    size_t *count) {
    if (workspace->bytes_per_sample == 1U) {
        *stride = workspace->coded[plane].reconstructed_stride_cells;
        *count = workspace->coded[plane].reconstructed_count_cells;
        return workspace->coded[plane].reconstructed;
    }
    *stride = workspace->coded_u16[plane].reconstructed_stride_cells;
    *count = workspace->coded_u16[plane].reconstructed_count_cells;
    return workspace->coded_u16[plane].reconstructed;
}

static void tile_readiness_clear(
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    const Av2hTileDecoder *decoder) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t x0 = decoder->traversal.tile_start_column >> shift;
        size_t y0 = decoder->traversal.tile_start_row >> shift;
        size_t x1 = ((size_t)decoder->traversal.tile_end_column +
            (1U << shift) - 1U) >> shift;
        size_t y1 = ((size_t)decoder->traversal.tile_end_row +
            (1U << shift) - 1U) >> shift;
        size_t stride;
        size_t count;
        uint8_t *map = tile_readiness_map(
            workspace, plane, &stride, &count);
        size_t row;
        av2h_memory_fill(map, 2U, count);
        for (row = y0; row < y1; ++row) {
            size_t width = x1 - x0;
            if (row * stride + x0 < count &&
                width <= count - (row * stride + x0)) {
                av2h_memory_fill(map + row * stride + x0, 0U, width);
            }
        }
    }
}

static int tile_readiness_complete(
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    const Av2hTileDecoder *decoder) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t x0 = decoder->traversal.tile_start_column >> shift;
        size_t y0 = decoder->traversal.tile_start_row >> shift;
        size_t x1 = ((size_t)decoder->traversal.tile_end_column +
            (1U << shift) - 1U) >> shift;
        size_t y1 = ((size_t)decoder->traversal.tile_end_row +
            (1U << shift) - 1U) >> shift;
        size_t stride;
        size_t count;
        uint8_t *map = tile_readiness_map(
            workspace, plane, &stride, &count);
        size_t row;
        for (row = y0; row < y1; ++row) {
            size_t column;
            for (column = x0; column < x1; ++column) {
                size_t index = row * stride + column;
                if (index >= count || map[index] == 0U) return 0;
            }
        }
    }
    return 1;
}

static void tile_readiness_restore_previous(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint16_t tile_id) {
    uint16_t previous;
    uint16_t superblock_mi = (uint16_t)(sequence->superblock_size / 4U);
    for (previous = 0U; previous < tile_id; ++previous) {
        uint16_t tile_column =
            (uint16_t)(previous % frame->tile_column_count);
        uint16_t tile_row =
            (uint16_t)(previous / frame->tile_column_count);
        unsigned int plane;
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            unsigned int shift = av2h_chroma_shift(
                frame->chroma_format_idc, plane);
            size_t x0 = ((size_t)frame->tile_column_start_sb[tile_column] *
                superblock_mi) >> shift;
            size_t y0 = ((size_t)frame->tile_row_start_sb[tile_row] *
                superblock_mi) >> shift;
            size_t x1 = ((size_t)frame->tile_column_start_sb[tile_column + 1U] *
                superblock_mi) >> shift;
            size_t y1 = ((size_t)frame->tile_row_start_sb[tile_row + 1U] *
                superblock_mi) >> shift;
            size_t stride;
            size_t count;
            uint8_t *map = tile_readiness_map(
                workspace, plane, &stride, &count);
            size_t row;
            for (row = y0; row < y1; ++row) {
                size_t width = x1 - x0;
                if (row * stride + x0 < count &&
                    width <= count - (row * stride + x0)) {
                    av2h_memory_fill(map + row * stride + x0, 1U, width);
                }
            }
        }
    }
}

Av2hStatus av2h_decode_frame_group_planar_private(
    const Av2hTileGroup *group,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size_bytes,
    Av2hFrameDecodeResult *result,
    const Av2hExecution *execution) {
    Av2hFilterWorkspaceLayout layout;
    Av2hTileDecoder decoder;
    Av2hTileFilterControls filter_controls;
    Av2hFilterPlanes final_planes;
    Av2hFilterPlanesU16 final_planes_u16;
    Av2hFrameDecodeResult decoded = { 0 };
    Av2hTileChromaLeaf pending_chroma_leaves[4U];
    Av2hCoeffBlock pending_chroma_blocks[4U][2U];
    uint8_t pending_chroma_count = 0U;
    size_t required_output_bytes;
    size_t tile_index;
    Av2hStatus status;
    unsigned int plane;
#define AV2H_DECODE_FAIL(returned_status, original_cause, failure_stage,       \
                         failure_plane, failure_leaf, failure_syntax_stage)   \
    do {                                                                       \
    const Av2hPartitionNode *diagnostic_leaf = (failure_leaf);             \
        decoded.entropy_tell = av2h_range_tell(&decoder.entropy_state.range);               \
        decoded.cause = (original_cause);                                      \
        decoded.stage = (failure_stage);                                       \
        decoded.plane = (uint8_t)(failure_plane);                              \
        decoded.syntax_stage = (uint8_t)(failure_syntax_stage);                \
        if (diagnostic_leaf != NULL) {                                         \
            decoded.row = diagnostic_leaf->row;                                \
            decoded.column = diagnostic_leaf->column;                          \
            decoded.block_size = (uint8_t)diagnostic_leaf->block_size;         \
        }                                                                      \
        if (result != NULL) *result = decoded;                                 \
        return (returned_status);                                              \
    } while (0)
    if (group == NULL || group->tile_count == 0U ||
        group->tile_count > AV2H_PROFILE_MAX_TILES || sequence == NULL ||
        frame == NULL || workspace == NULL || output == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (workspace->diagnostics != NULL) {
        Av2hPartitionDecision *partition_trace =
            workspace->diagnostics->partition_trace;
        uint8_t *partition_trace_structure =
            workspace->diagnostics->partition_trace_structure;
        size_t partition_trace_capacity =
            workspace->diagnostics->partition_trace_capacity;
        Av2hPartitionLeafDiagnostic *partition_leaf_trace =
            workspace->diagnostics->partition_leaf_trace;
        size_t partition_leaf_trace_capacity =
            workspace->diagnostics->partition_leaf_trace_capacity;
        av2h_memory_fill(
            workspace->diagnostics, 0U, sizeof(*workspace->diagnostics));
        workspace->diagnostics->partition_trace = partition_trace;
        workspace->diagnostics->partition_trace_structure =
            partition_trace_structure;
        workspace->diagnostics->partition_trace_capacity =
            partition_trace_capacity;
        workspace->diagnostics->partition_leaf_trace =
            partition_leaf_trace;
        workspace->diagnostics->partition_leaf_trace_capacity =
            partition_leaf_trace_capacity;
        decoded.diagnostics = workspace->diagnostics;
    }
#endif
    if (workspace->bytes_per_sample != 1U &&
        workspace->bytes_per_sample != 2U) return AV2H_UNSUPPORTED;
    if ((workspace->bytes_per_sample == 1U &&
         workspace->reconstruction_scratch == NULL) ||
        (workspace->bytes_per_sample == 2U &&
         workspace->reconstruction_scratch_u16 == NULL) ||
        workspace->filter_workspace == NULL) return AV2H_INVALID_ARGUMENT;
    status = av2h_planar_output_size(
        frame, workspace->bytes_per_sample, &required_output_bytes);
    if (status != AV2H_OK) return status;
    if (output_size_bytes < required_output_bytes) return AV2H_OUT_OF_MEMORY;
    status = av2h_frame_workspace_prepare(
        sequence, frame, workspace, output, required_output_bytes, result,
        &layout);
    if (status != AV2H_OK) return status;
    for (tile_index = 0U; tile_index < group->tile_count; ++tile_index) {
        uint16_t tile_id = (uint16_t)(group->start_tile + tile_index);
        pending_chroma_count = 0U;
        status = av2h_tile_decoder_init(
            &decoder, group->tiles[tile_index], sequence, frame,
            &workspace->tile_context, tile_id);
        if (status != AV2H_OK) return status;
        tile_readiness_clear(frame, workspace, &decoder);
        decoder.diagnostics = workspace->diagnostics;
        decoder.entropy_state.range.diagnostics = workspace->diagnostics;
        decoder.traversal.luma_traversal.diagnostics = workspace->diagnostics;
        decoder.traversal.chroma_traversal.diagnostics = workspace->diagnostics;
        while (decoder.control.phase != AV2H_TILE_DECODE_DONE) {
        uint8_t has_leaf = 0U;
        if (decoder.control.phase == AV2H_TILE_DECODE_LUMA &&
            decoder.chroma.shared_complete && pending_chroma_count != 0U &&
            decoder.luma.tx_count == 0U) {
            uint8_t pending_index;
            for (pending_index = 0U; pending_index < pending_chroma_count;
                 ++pending_index) {
                Av2hTileChromaLeaf *pending_leaf =
                    &pending_chroma_leaves[pending_index];
                if (workspace->bytes_per_sample == 2U) {
                    for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                        unsigned int index = plane - 1U;
                        status =
                            av2h_chroma_reconstruct_to_plane_u16(
                            workspace->diagnostics,
                            &pending_leaf->node, &pending_leaf->mode,
                            &pending_leaf->prefix[index],
                            &pending_leaf->transform[index],
                            &pending_chroma_blocks[pending_index][index],
                            &workspace->coded_u16[plane],
                            &workspace->coded_u16[0],
                            (uint8_t)plane,
                            frame->chroma_format_idc,
                            workspace->reconstruction_scratch_u16);
                        if (status != AV2H_OK) {
                            AV2H_DECODE_FAIL(
                                status, status,
                                AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                                plane, &pending_leaf->node, 0U);
                        }
                        status = av2h_deblock_map_commit_transform(
                            &workspace->filter_workspace->deblock_map,
                            frame->chroma_format_idc, plane,
                            &pending_leaf->node,
                            &pending_leaf->prefix[index], 0U,
                            pending_leaf->prefix[index].all_zero,
                            (uint8_t)av2h_quantizer_is_lossless(
                                sequence, frame, frame->base_q_index));
                        if (status != AV2H_OK) {
                            AV2H_DECODE_FAIL(
                                status, status,
                                AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                                plane, &pending_leaf->node, 0U);
                        }
                    }
                    ++decoded.chroma_leaf_count;
                    continue;
                }
                for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                    unsigned int index = plane - 1U;
                    status =
                        av2h_chroma_reconstruct_to_plane(
                        workspace->diagnostics,
                        &pending_leaf->node, &pending_leaf->mode,
                        &pending_leaf->prefix[index],
                        &pending_leaf->transform[index],
                        &pending_chroma_blocks[pending_index][index],
                        &workspace->coded[plane], &workspace->coded[0],
                        (uint8_t)plane, NULL, frame->chroma_format_idc,
                        workspace->reconstruction_scratch);
                    if (status != AV2H_OK) {
                        AV2H_DECODE_FAIL(
                            status, status,
                            AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                            plane, &pending_leaf->node, 0U);
                    }
                    status = av2h_deblock_map_commit_transform(
                        &workspace->filter_workspace->deblock_map,
                        frame->chroma_format_idc, plane, &pending_leaf->node,
                        &pending_leaf->prefix[index],
                        0U,
                        pending_leaf->prefix[index].all_zero,
                        (uint8_t)av2h_quantizer_is_lossless(
                            sequence, frame, frame->base_q_index));
                    if (status != AV2H_OK) {
                        AV2H_DECODE_FAIL(
                            status, status,
                            AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                            plane, &pending_leaf->node, 0U);
                    }
                }
                ++decoded.chroma_leaf_count;
            }
            pending_chroma_count = 0U;
        }
        if (decoder.control.phase == AV2H_TILE_DECODE_LUMA) {
            Av2hTileLumaLeaf leaf;
            Av2hCoeffBlock block;
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(&block, 0U, sizeof(block));
            status = av2h_tile_decoder_next_luma(
                &decoder, &leaf, &block, &has_leaf);
            if (status != AV2H_OK) {
                const Av2hTileDiagnostics *tile_diagnostics =
                    av2h_tile_diagnostics_get(workspace->diagnostics);
                const Av2hLeafDiagnostics *leaf_diagnostics =
                    av2h_leaf_diagnostics_get(workspace->diagnostics);
                const Av2hCoefficientDiagnostics *coefficient_diagnostics =
                    av2h_coefficient_diagnostics_get(workspace->diagnostics);
                const Av2hTransformDiagnostics *transform_diagnostics =
                    av2h_transform_diagnostics_get(workspace->diagnostics);
                const Av2hReconstructionDiagnostics *
                    reconstruction_diagnostics =
                        av2h_reconstruction_diagnostics_get(
                            workspace->diagnostics);
                uint8_t luma_tile_stage = tile_diagnostics != NULL ?
                    tile_diagnostics->luma_tile_stage : 0U;
                luma_failure_context(
                    &decoded, &leaf, reconstruction_diagnostics);
                decoded.primary_set_symbol =
                    leaf.transform.primary_long_side_symbol;
                if (luma_tile_stage == 1U) {
                    decoded.syntax_stage = leaf_diagnostics != NULL ?
                        leaf_diagnostics->luma_mode_stage : 0U;
                }
                if (luma_tile_stage == 3U) {
                    decoded.syntax_stage = transform_diagnostics != NULL ?
                        transform_diagnostics->transform_stage : 0U;
                }
                decoded.prefix_stage = coefficient_diagnostics != NULL ?
                    coefficient_diagnostics->luma_prefix_stage : 0U;
                decoded.txb_skip_context = leaf.prefix.txb_skip_context;
                decoded.all_zero = leaf.prefix.all_zero;
                decoded.eob_point = leaf.prefix.eob_point;
                decoded.after_all_zero_tell =
                    leaf.prefix.after_all_zero_bits;
                decoded.after_eob_point_tell =
                    leaf.prefix.after_eob_point_bits;
                decoded.eob_range = coefficient_diagnostics != NULL ?
                    coefficient_diagnostics->eob_range : 0U;
                decoded.skip_range = coefficient_diagnostics != NULL ?
                    coefficient_diagnostics->skip_range : 0U;
                decoded.before_prefix_range = tile_diagnostics != NULL ?
                    tile_diagnostics->luma_stage_ranges[1] : 0U;
                decoded.eob_tx_size = coefficient_diagnostics != NULL ?
                    coefficient_diagnostics->eob_tx : 0U;
                decoded.eob_class = coefficient_diagnostics != NULL ?
                    coefficient_diagnostics->eob_class : 0U;
                decoded.coefficient_position = block.diagnostic_position;
                decoded.coefficient_context = block.diagnostic_context;
                decoded.coefficient_kind = block.diagnostic_kind;
                {
                    const Av2hPartitionNode *failure_node =
                        luma_tile_stage == 0U ?
                            &decoder.traversal.luma_traversal.diagnostic_node :
                            &leaf.node;
                    AV2H_DECODE_FAIL(
                        status, status,
                        AV2H_FRAME_DECODE_STAGE_LUMA_PARSE,
                        0U, failure_node, decoded.syntax_stage != 0U ?
                            decoded.syntax_stage : luma_tile_stage);
                }
            }
            if (!has_leaf) continue;
            if (workspace->bytes_per_sample == 1U) {
                status = av2h_luma_reconstruct_to_plane(
                    workspace->diagnostics,
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &workspace->coded[0],
                    workspace->reconstruction_scratch);
            } else {
                status =
                    av2h_luma_reconstruct_to_plane_u16(
                    workspace->diagnostics,
                    &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                    &block, &workspace->coded_u16[0],
                    workspace->reconstruction_scratch_u16);
            }
            if (status != AV2H_OK) {
                const Av2hReconstructionDiagnostics *diagnostics =
                    av2h_reconstruction_diagnostics_get(
                        workspace->diagnostics);
                luma_failure_context(&decoded, &leaf, diagnostics);
                AV2H_DECODE_FAIL(
                    status, status,
                    AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT,
                    0U, &leaf.node,
                    diagnostics != NULL ? diagnostics->luma_stage : 0U);
            }
            status = av2h_deblock_map_commit_transform(
                &workspace->filter_workspace->deblock_map,
                frame->chroma_format_idc, 0U,
                &leaf.node, &leaf.prefix,
                leaf.mode.skip_transform, leaf.prefix.all_zero,
                (uint8_t)av2h_quantizer_is_lossless(
                    sequence, frame, frame->base_q_index));
            if (status != AV2H_OK) {
                AV2H_DECODE_FAIL(
                    status, status, AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK,
                    0U, &leaf.node, 0U);
            }
            if (av2h_frame_restoration_needs_tx_skip(frame)) {
                status = av2h_restoration_tx_skip_commit(
                    workspace->filter_workspace, &leaf.node, &leaf.prefix);
                if (status != AV2H_OK) {
                    AV2H_DECODE_FAIL(
                        status, status, AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK,
                        0U, &leaf.node, 0U);
                }
            }
            status = av2h_tile_decoder_commit_leaf(&decoder, leaf.node);
            if (status != AV2H_OK) {
                AV2H_DECODE_FAIL(
                    status, status, AV2H_FRAME_DECODE_STAGE_LUMA_COMMIT,
                    0U, &leaf.node, 0U);
            }
            ++decoded.luma_leaf_count;
        } else if (decoder.control.phase == AV2H_TILE_DECODE_CHROMA) {
            Av2hTileChromaLeaf leaf;
            Av2hCoeffBlock blocks[2U];
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(blocks, 0U, sizeof(blocks));
            status = av2h_tile_decoder_next_chroma(
                &decoder, &leaf, &blocks[0], &blocks[1], &has_leaf);
            if (status != AV2H_OK) {
                const Av2hTileDiagnostics *tile_diagnostics =
                    av2h_tile_diagnostics_get(workspace->diagnostics);
                const Av2hLeafDiagnostics *leaf_diagnostics =
                    av2h_leaf_diagnostics_get(workspace->diagnostics);
                uint8_t chroma_tile_stage = tile_diagnostics != NULL ?
                    tile_diagnostics->chroma_tile_stage : 0U;
                unsigned int diagnostic_index =
                    chroma_tile_stage >= 4U ? 1U : 0U;
                chroma_failure_context(&decoded, &leaf, diagnostic_index);
                decoded.syntax_stage = leaf_diagnostics != NULL ?
                    leaf_diagnostics->chroma_mode_stage : 0U;
                decoded.chroma_mode_index = leaf.mode.mode_index;
                decoded.txb_skip_context =
                    leaf.prefix[diagnostic_index].txb_skip_context;
                decoded.all_zero =
                    leaf.prefix[diagnostic_index].all_zero;
                decoded.eob_point =
                    leaf.prefix[diagnostic_index].eob_point;
                decoded.after_all_zero_tell =
                    leaf.prefix[diagnostic_index].after_all_zero_bits;
                decoded.after_eob_point_tell =
                    leaf.prefix[diagnostic_index].after_eob_point_bits;
                decoded.coefficient_position =
                    blocks[diagnostic_index].diagnostic_position;
                decoded.coefficient_context =
                    blocks[diagnostic_index].diagnostic_context;
                decoded.coefficient_kind =
                    blocks[diagnostic_index].diagnostic_kind;
                decoded.chroma_parse_reason =
                    tile_diagnostics != NULL ?
                        tile_diagnostics->chroma_parse_reason : 0U;
                AV2H_DECODE_FAIL(
                    status, status, AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE,
                    chroma_tile_stage >= 4U ? 2U : 1U,
                    &leaf.node, chroma_tile_stage != 0U ?
                        chroma_tile_stage : decoded.syntax_stage);
            }
            if (!has_leaf) continue;
            if (leaf.decoded_plane != 0U) {
                unsigned int index = leaf.decoded_plane - 1U;
                plane = leaf.decoded_plane;
                if (workspace->bytes_per_sample == 1U) {
                    status =
                        av2h_chroma_reconstruct_to_plane(
                        workspace->diagnostics,
                        &leaf.node, &leaf.mode, &leaf.prefix[index],
                        &leaf.transform[index], &blocks[index],
                        &workspace->coded[plane], &workspace->coded[0],
                        (uint8_t)plane, NULL, frame->chroma_format_idc,
                        workspace->reconstruction_scratch);
                } else {
                    status =
                        av2h_chroma_reconstruct_to_plane_u16(
                        workspace->diagnostics,
                        &leaf.node, &leaf.mode, &leaf.prefix[index],
                        &leaf.transform[index], &blocks[index],
                        &workspace->coded_u16[plane],
                        &workspace->coded_u16[0], (uint8_t)plane,
                        frame->chroma_format_idc,
                        workspace->reconstruction_scratch_u16);
                }
                if (status != AV2H_OK) {
                    AV2H_DECODE_FAIL(
                        status, status,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                        plane, &leaf.node, 0U);
                }
                status = av2h_deblock_map_commit_transform(
                    &workspace->filter_workspace->deblock_map,
                    frame->chroma_format_idc, plane,
                    &leaf.node, &leaf.prefix[index], 0U,
                    leaf.prefix[index].all_zero, 1U);
                if (status != AV2H_OK) {
                    AV2H_DECODE_FAIL(
                        status, status,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                        plane, &leaf.node, 0U);
                }
                status = av2h_tile_decoder_commit_leaf(&decoder, leaf.node);
                if (status != AV2H_OK) {
                    AV2H_DECODE_FAIL(
                        status, status,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT,
                        plane, &leaf.node, 0U);
                }
                if (plane == 2U) ++decoded.chroma_leaf_count;
                continue;
            }
            decoded.cctx_type = leaf.transform[0].cctx_type;
            decoded.u_coefficient_count = blocks[0].coefficient_count;
            decoded.v_coefficient_count = blocks[1].coefficient_count;
            status = av2h_chroma_inverse_cross_transform(
                &blocks[0], &blocks[1], leaf.transform[0].cctx_type,
                workspace->bytes_per_sample == 2U ? 10U : 8U);
            if (status != AV2H_OK) {
                AV2H_DECODE_FAIL(
                    status, status,
                    AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM,
                    0U, &leaf.node, 0U);
            }
                        if (decoder.chroma.shared_resume_luma &&
                                !(decoder.luma.coding_mode.q_index == 0U &&
                                    decoder.luma.coding_mode.lossless_tx_size_large == 0U)) {
                if (pending_chroma_count >= 4U) {
                    AV2H_DECODE_FAIL(
                        AV2H_OVERFLOW, AV2H_OVERFLOW,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT,
                        0U, &leaf.node, 0U);
                }
                av2h_memory_copy(
                    &pending_chroma_leaves[pending_chroma_count], &leaf,
                    sizeof(pending_chroma_leaves[pending_chroma_count]));
                av2h_memory_copy(
                    &pending_chroma_blocks[pending_chroma_count], blocks,
                    sizeof(pending_chroma_blocks[pending_chroma_count]));
                ++pending_chroma_count;
                status = av2h_tile_decoder_commit_leaf(&decoder, leaf.node);
                if (status != AV2H_OK) {
                    AV2H_DECODE_FAIL(
                        status, status, AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT,
                        0U, &leaf.node, 0U);
                }
                continue;
            }
            if (workspace->bytes_per_sample == 1U) {
                for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                    unsigned int index = plane - 1U;
                    status =
                        av2h_chroma_reconstruct_to_plane(
                        workspace->diagnostics,
                        &leaf.node, &leaf.mode, &leaf.prefix[index],
                        &leaf.transform[index], &blocks[index],
                        &workspace->coded[plane], &workspace->coded[0],
                        (uint8_t)plane, NULL, frame->chroma_format_idc,
                        workspace->reconstruction_scratch);
                    if (status != AV2H_OK) {
                        const Av2hPredictionDiagnostics *prediction_diagnostics =
                            av2h_prediction_diagnostics_get(
                                workspace->diagnostics);
                        const Av2hReconstructionDiagnostics *
                            reconstruction_diagnostics =
                                av2h_reconstruction_diagnostics_get(
                                    workspace->diagnostics);
                        chroma_failure_context(&decoded, &leaf, index);
                        decoded.is_cfl = leaf.mode.is_cfl;
                        decoded.cfl_kind = (uint8_t)leaf.mode.cfl_kind;
                        decoded.mhccp_direction = leaf.mode.mhccp_direction;
                        decoded.cfl_filter_index =
                            leaf.mode.cfl_downsample_filter_index;
                        decoded.mhccp_reason = prediction_diagnostics != NULL ?
                            prediction_diagnostics->mhccp_reason : 0U;
                        AV2H_DECODE_FAIL(
                            status, status,
                            AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                            plane, &leaf.node,
                            reconstruction_diagnostics != NULL ?
                                reconstruction_diagnostics->chroma_stage : 0U);
                    }
                }
                for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                    unsigned int index = plane - 1U;
                    status = av2h_deblock_map_commit_transform(
                        &workspace->filter_workspace->deblock_map,
                        frame->chroma_format_idc, plane,
                        &leaf.node, &leaf.prefix[index],
                        0U,
                        leaf.prefix[index].all_zero,
                        (uint8_t)av2h_quantizer_is_lossless(
                            sequence, frame, frame->base_q_index));
                    if (status != AV2H_OK) {
                        AV2H_DECODE_FAIL(
                            status, status,
                            AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                            plane, &leaf.node, 0U);
                    }
                }
            } else {
                for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                    unsigned int index = plane - 1U;
                    status =
                        av2h_chroma_reconstruct_to_plane_u16(
                        workspace->diagnostics,
                        &leaf.node, &leaf.mode, &leaf.prefix[index],
                        &leaf.transform[index], &blocks[index],
                        &workspace->coded_u16[plane],
                        &workspace->coded_u16[0],
                        (uint8_t)plane,
                        frame->chroma_format_idc,
                        workspace->reconstruction_scratch_u16);
                    if (status != AV2H_OK) {
                        const Av2hReconstructionDiagnostics *diagnostics =
                            av2h_reconstruction_diagnostics_get(
                                workspace->diagnostics);
                        chroma_failure_context(&decoded, &leaf, index);
                        decoded.chroma_mode_index = leaf.mode.mode_index;
                        decoded.is_cfl = leaf.mode.is_cfl;
                        decoded.cfl_kind = (uint8_t)leaf.mode.cfl_kind;
                        decoded.mhccp_direction =
                            leaf.mode.mhccp_direction;
                        AV2H_DECODE_FAIL(
                            status, status,
                            AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                            plane, &leaf.node,
                            diagnostics != NULL ?
                                diagnostics->chroma_stage : 0U);
                    }
                    status = av2h_deblock_map_commit_transform(
                        &workspace->filter_workspace->deblock_map,
                        frame->chroma_format_idc, plane,
                        &leaf.node, &leaf.prefix[index], 0U,
                        leaf.prefix[index].all_zero,
                        (uint8_t)av2h_quantizer_is_lossless(
                            sequence, frame, frame->base_q_index));
                    if (status != AV2H_OK) {
                        AV2H_DECODE_FAIL(
                            status, status,
                            AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                            plane, &leaf.node, 0U);
                    }
                }
            }
            status = av2h_tile_decoder_commit_leaf(&decoder, leaf.node);
            if (status != AV2H_OK) {
                AV2H_DECODE_FAIL(
                    status, status, AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT,
                    0U, &leaf.node, 0U);
            }
            ++decoded.chroma_leaf_count;
        } else {
            AV2H_DECODE_FAIL(
                AV2H_INVALID_DATA, AV2H_INVALID_DATA,
                AV2H_FRAME_DECODE_STAGE_COMPLETENESS,
                0U, 0, 0U);
        }
        }
        if (!tile_readiness_complete(frame, workspace, &decoder)) {
            AV2H_DECODE_FAIL(
                AV2H_INVALID_DATA, AV2H_INVALID_DATA,
                AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, 0, 0U);
        }
        tile_readiness_restore_previous(
            sequence, frame, workspace, tile_id);
    }
    if (!av2h_frame_workspace_complete(frame, workspace)) {
        AV2H_DECODE_FAIL(
            AV2H_INVALID_DATA, AV2H_INVALID_DATA,
            AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, 0, 0U);
    }
    if (decoder.entropy_state.restoration_entropy.active_mask != 0U) {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            if ((decoder.entropy_state.restoration_entropy.active_mask &
                    (1U << plane)) != 0U &&
                decoder.entropy_state.restoration_entropy.decoded_count[plane] !=
                    frame->restoration_unit_count[plane]) {
                AV2H_DECODE_FAIL(
                    AV2H_INVALID_DATA, AV2H_INVALID_DATA,
                    AV2H_FRAME_DECODE_STAGE_COMPLETENESS,
                    plane, 0, 0U);
            }
        }
    }
    status = av2h_tile_filter_controls_handoff(&decoder, &filter_controls);
    if (status != AV2H_OK) {
        AV2H_DECODE_FAIL(
            status, status, AV2H_FRAME_DECODE_STAGE_COMPLETENESS,
            0U, 0, 0U);
    }
    decoded.entropy_tell = av2h_range_tell(&decoder.entropy_state.range);
    workspace->filter_workspace->cdef_controls = filter_controls.cdef;
    workspace->filter_workspace->gdf_controls = filter_controls.gdf;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        workspace->filter_workspace->restoration_controls.unit_flags[plane] =
            decoder.entropy_state.restoration_entropy.unit_flags[plane];
        workspace->filter_workspace->restoration_controls.unit_taps[plane] =
            decoder.entropy_state.restoration_entropy.unit_taps[plane];
        workspace->filter_workspace->restoration_controls
            .unit_columns[plane] =
                decoder.entropy_state.restoration_entropy.unit_columns[plane];
        workspace->filter_workspace->restoration_controls.unit_rows[plane] =
            decoder.entropy_state.restoration_entropy.unit_rows[plane];
        workspace->filter_workspace->restoration_controls.unit_size[plane] =
            decoder.entropy_state.restoration_entropy.unit_size[plane];
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (filter_controls.ccso.plane[plane] != NULL) {
            av2h_memory_copy(
                (void *)workspace->filter_workspace->ccso_controls.plane[plane],
                filter_controls.ccso.plane[plane],
                layout.ccso_control_count);
        }
        workspace->filter_workspace->ccso_controls.stride_units[plane] =
            filter_controls.ccso.stride_units[plane];
    }
    workspace->filter_workspace->ccso_controls.columns =
        filter_controls.ccso.columns;
    workspace->filter_workspace->ccso_controls.rows =
        filter_controls.ccso.rows;
    if (workspace->bytes_per_sample == 2U) {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            final_planes_u16.plane[plane] = (Av2hFilterPlaneU16){
                workspace->coded_u16[plane].data,
                workspace->coded_u16[plane].stride_samples,
                workspace->coded_u16[plane].width,
                workspace->coded_u16[plane].height
            };
        }
        status = av2h_filter_stage_u16(
            &final_planes_u16, sequence, frame,
            workspace->filter_workspace);
        if (status != AV2H_OK) {
            AV2H_DECODE_FAIL(
                status, status, AV2H_FRAME_DECODE_STAGE_FILTER,
                0U, 0, 0U);
        }
        status = av2h_copy_visible_planar_u16_private(
            frame, workspace->coded_u16, workspace->coded_data_size_bytes,
            output, output_size_bytes, execution);
        if (status != AV2H_OK) {
            AV2H_DECODE_FAIL(
                status, status, AV2H_FRAME_DECODE_STAGE_OUTPUT,
                0U, 0, 0U);
        }
        if (result != NULL) *result = decoded;
        return AV2H_OK;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        final_planes.plane[plane] = (Av2hFilterPlane){
            workspace->coded[plane].data,
            workspace->coded[plane].stride_samples,
            workspace->coded[plane].width,
            workspace->coded[plane].height
        };
    }
    status = av2h_filter_frame(
        &final_planes, sequence, frame, workspace->filter_workspace);
    if (status != AV2H_OK) {
        const Av2hFilterDiagnostics *filter_diagnostics =
            av2h_filter_diagnostics_get(workspace->diagnostics);
        decoded.filter_reason = filter_diagnostics != NULL ?
            filter_diagnostics->reason : 0U;
        AV2H_DECODE_FAIL(
            status, status, AV2H_FRAME_DECODE_STAGE_FILTER,
            0U, 0, 0U);
    }
    status = av2h_copy_visible_planar_private(
        frame, &final_planes, workspace->coded_data_size_bytes,
        output, output_size_bytes, execution);
    if (status != AV2H_OK) {
        AV2H_DECODE_FAIL(
            status, status, AV2H_FRAME_DECODE_STAGE_OUTPUT,
            0U, 0, 0U);
    }
    if (result != NULL) *result = decoded;
#undef AV2H_DECODE_FAIL
    return AV2H_OK;
}

Av2hStatus av2h_decode_frame_planar_private(
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size_bytes,
    Av2hFrameDecodeResult *result,
    const Av2hExecution *execution) {
    Av2hTileGroup group;
    av2h_memory_fill(&group, 0U, sizeof(group));
    group.tile_count = 1U;
    group.tiles[0] = entropy;
    group.entropy = entropy;
    return av2h_decode_frame_group_planar_private(
        &group, sequence, frame, workspace, output, output_size_bytes,
        result, execution);
}
