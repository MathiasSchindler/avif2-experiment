#include "decoder/frame_output.h"

#include "decoder/internal/decode_diagnostics.h"
#include "decoder/internal/filter_diagnostics.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/internal/tile_modules.h"
#include "decoder/tx_geometry.h"

#include <stddef.h>

typedef struct {
    const Av2hSequenceState *sequence;
    const Av2hFrameState *frame;
    Av2hFrameDecodeWorkspace *workspace;
    const Av2hSampleDomain *domain;
    Av2hTileDecoder *decoder;
    Av2hFrameDecodeResult *decoded;
    Av2hFrameDecodeScratch *scratch;
    uint8_t pending_chroma_count;
} Av2hFrameDecodeContext;

static void luma_failure_context(Av2hFrameDecodeResult *decoded, const Av2hTileLumaLeaf *leaf,
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
        diagnostics != NULL ? diagnostics->inverse_transform_stage : 0U;
    decoded->intrabc_source_x = diagnostics != NULL ? diagnostics->intrabc_source_x : 0U;
    decoded->intrabc_source_y = diagnostics != NULL ? diagnostics->intrabc_source_y : 0U;
    decoded->intrabc_map_index = diagnostics != NULL ? diagnostics->intrabc_map_index : 0U;
    decoded->primary_type = (uint8_t)leaf->transform.primary_type;
    decoded->primary_symbol = leaf->transform.primary_symbol;
    decoded->secondary_symbol = leaf->transform.secondary_symbol;
    decoded->secondary_set = leaf->transform.secondary_set;
    decoded->eob = leaf->prefix.eob;
}

static void chroma_failure_context(Av2hFrameDecodeResult *decoded, const Av2hTileChromaLeaf *leaf,
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

static uint8_t *tile_readiness_map(Av2hFrameDecodeWorkspace *workspace, unsigned int plane,
                                   size_t *stride, size_t *count) {
    if (workspace->bytes_per_sample == 1U) {
        *stride = workspace->coded[plane].reconstructed_stride_cells;
        *count = workspace->coded[plane].reconstructed_count_cells;
        return workspace->coded[plane].reconstructed;
    }
    *stride = workspace->coded_u16[plane].reconstructed_stride_cells;
    *count = workspace->coded_u16[plane].reconstructed_count_cells;
    return workspace->coded_u16[plane].reconstructed;
}

static void tile_readiness_clear(const Av2hFrameState *frame, Av2hFrameDecodeWorkspace *workspace,
                                 const Av2hTileDecoder *decoder) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
        size_t x0 = decoder->traversal.tile_start_column >> shift;
        size_t y0 = decoder->traversal.tile_start_row >> shift;
        size_t x1 = ((size_t)decoder->traversal.tile_end_column + (1U << shift) - 1U) >> shift;
        size_t y1 = ((size_t)decoder->traversal.tile_end_row + (1U << shift) - 1U) >> shift;
        size_t stride;
        size_t count;
        uint8_t *map = tile_readiness_map(workspace, plane, &stride, &count);
        size_t row;
        av2h_memory_fill(map, 2U, count);
        for (row = y0; row < y1; ++row) {
            size_t width = x1 - x0;
            if (row * stride + x0 < count && width <= count - (row * stride + x0)) {
                av2h_memory_fill(map + row * stride + x0, 0U, width);
            }
        }
    }
}

static int tile_readiness_complete(const Av2hFrameState *frame, Av2hFrameDecodeWorkspace *workspace,
                                   const Av2hTileDecoder *decoder) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
        size_t x0 = decoder->traversal.tile_start_column >> shift;
        size_t y0 = decoder->traversal.tile_start_row >> shift;
        size_t x1 = ((size_t)decoder->traversal.tile_end_column + (1U << shift) - 1U) >> shift;
        size_t y1 = ((size_t)decoder->traversal.tile_end_row + (1U << shift) - 1U) >> shift;
        size_t stride;
        size_t count;
        uint8_t *map = tile_readiness_map(workspace, plane, &stride, &count);
        size_t row;
        for (row = y0; row < y1; ++row) {
            size_t column;
            for (column = x0; column < x1; ++column) {
                size_t index = row * stride + column;
                if (index >= count || map[index] == 0U)
                    return 0;
            }
        }
    }
    return 1;
}

static void tile_readiness_restore_previous(const Av2hSequenceState *sequence,
                                            const Av2hFrameState *frame,
                                            Av2hFrameDecodeWorkspace *workspace, uint16_t tile_id) {
    uint16_t previous;
    uint16_t superblock_mi = (uint16_t)(sequence->superblock_size / 4U);
    for (previous = 0U; previous < tile_id; ++previous) {
        uint16_t tile_column = (uint16_t)(previous % frame->tile_column_count);
        uint16_t tile_row = (uint16_t)(previous / frame->tile_column_count);
        unsigned int plane;
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
            size_t x0 = ((size_t)frame->tile_column_start_sb[tile_column] * superblock_mi) >> shift;
            size_t y0 = ((size_t)frame->tile_row_start_sb[tile_row] * superblock_mi) >> shift;
            size_t x1 =
                ((size_t)frame->tile_column_start_sb[tile_column + 1U] * superblock_mi) >> shift;
            size_t y1 = ((size_t)frame->tile_row_start_sb[tile_row + 1U] * superblock_mi) >> shift;
            size_t stride;
            size_t count;
            uint8_t *map = tile_readiness_map(workspace, plane, &stride, &count);
            size_t row;
            for (row = y0; row < y1; ++row) {
                size_t width = x1 - x0;
                if (row * stride + x0 < count && width <= count - (row * stride + x0)) {
                    av2h_memory_fill(map + row * stride + x0, 1U, width);
                }
            }
        }
    }
}

static Av2hStatus frame_decode_failure(Av2hFrameDecodeContext *context, Av2hStatus status,
                                       Av2hFrameDecodeStage stage, unsigned int plane,
                                       const Av2hPartitionNode *leaf, uint8_t syntax_stage) {
    Av2hFrameDecodeResult *decoded = context->decoded;
    decoded->entropy_tell = av2h_range_tell(&context->decoder->entropy_state.range);
    decoded->cause = status;
    decoded->stage = stage;
    decoded->plane = (uint8_t)plane;
    decoded->syntax_stage = syntax_stage;
    if (leaf != NULL) {
        decoded->row = leaf->row;
        decoded->column = leaf->column;
        decoded->block_size = (uint8_t)leaf->block_size;
    }
    return status;
}

static void reset_frame_diagnostics(Av2hFrameDecodeContext *context) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (context->workspace->diagnostics != NULL) {
        Av2hPartitionDecision *partition_trace = context->workspace->diagnostics->partition_trace;
        uint8_t *partition_trace_structure =
            context->workspace->diagnostics->partition_trace_structure;
        size_t partition_trace_capacity = context->workspace->diagnostics->partition_trace_capacity;
        Av2hPartitionLeafDiagnostic *partition_leaf_trace =
            context->workspace->diagnostics->partition_leaf_trace;
        size_t partition_leaf_trace_capacity =
            context->workspace->diagnostics->partition_leaf_trace_capacity;
        av2h_memory_fill(context->workspace->diagnostics, 0U,
                         sizeof(*context->workspace->diagnostics));
        context->workspace->diagnostics->partition_trace = partition_trace;
        context->workspace->diagnostics->partition_trace_structure = partition_trace_structure;
        context->workspace->diagnostics->partition_trace_capacity = partition_trace_capacity;
        context->workspace->diagnostics->partition_leaf_trace = partition_leaf_trace;
        context->workspace->diagnostics->partition_leaf_trace_capacity =
            partition_leaf_trace_capacity;
        context->decoded->diagnostics = context->workspace->diagnostics;
    }

#else
    (void)context;
#endif
}

static Av2hStatus populate_luma_parse_failure(Av2hFrameDecodeContext *context, Av2hStatus status,
                                              const Av2hTileLumaLeaf *leaf,
                                              const Av2hCoeffBlock *block) {

    const Av2hTileDiagnostics *tile_diagnostics =
        av2h_tile_diagnostics_get(context->workspace->diagnostics);
    const Av2hLeafDiagnostics *leaf_diagnostics =
        av2h_leaf_diagnostics_get(context->workspace->diagnostics);
    const Av2hCoefficientDiagnostics *coefficient_diagnostics =
        av2h_coefficient_diagnostics_get(context->workspace->diagnostics);
    const Av2hTransformDiagnostics *transform_diagnostics =
        av2h_transform_diagnostics_get(context->workspace->diagnostics);
    const Av2hReconstructionDiagnostics *reconstruction_diagnostics =
        av2h_reconstruction_diagnostics_get(context->workspace->diagnostics);
    uint8_t luma_tile_stage = tile_diagnostics != NULL ? tile_diagnostics->luma_tile_stage : 0U;
    luma_failure_context(context->decoded, leaf, reconstruction_diagnostics);
    context->decoded->primary_set_symbol = leaf->transform.primary_long_side_symbol;
    if (luma_tile_stage == 1U) {
        context->decoded->syntax_stage =
            leaf_diagnostics != NULL ? leaf_diagnostics->luma_mode_stage : 0U;
    }
    if (luma_tile_stage == 3U) {
        context->decoded->syntax_stage =
            transform_diagnostics != NULL ? transform_diagnostics->transform_stage : 0U;
    }
    context->decoded->prefix_stage =
        coefficient_diagnostics != NULL ? coefficient_diagnostics->luma_prefix_stage : 0U;
    context->decoded->txb_skip_context = leaf->prefix.txb_skip_context;
    context->decoded->all_zero = leaf->prefix.all_zero;
    context->decoded->eob_point = leaf->prefix.eob_point;
    context->decoded->after_all_zero_tell = leaf->prefix.after_all_zero_bits;
    context->decoded->after_eob_point_tell = leaf->prefix.after_eob_point_bits;
    context->decoded->eob_range =
        coefficient_diagnostics != NULL ? coefficient_diagnostics->eob_range : 0U;
    context->decoded->skip_range =
        coefficient_diagnostics != NULL ? coefficient_diagnostics->skip_range : 0U;
    context->decoded->before_prefix_range =
        tile_diagnostics != NULL ? tile_diagnostics->luma_stage_ranges[1] : 0U;
    context->decoded->eob_tx_size =
        coefficient_diagnostics != NULL ? coefficient_diagnostics->eob_tx : 0U;
    context->decoded->eob_class =
        coefficient_diagnostics != NULL ? coefficient_diagnostics->eob_class : 0U;
    context->decoded->coefficient_position = block->diagnostic_position;
    context->decoded->coefficient_context = block->diagnostic_context;
    context->decoded->coefficient_kind = block->diagnostic_kind;
    {
        const Av2hPartitionNode *failure_node =
            luma_tile_stage == 0U ? &context->decoder->traversal.luma_traversal.diagnostic_node
                                  : &leaf->node;
        return frame_decode_failure(
            context, status, AV2H_FRAME_DECODE_STAGE_LUMA_PARSE, 0U, failure_node,
            context->decoded->syntax_stage != 0U ? context->decoded->syntax_stage
                                                 : luma_tile_stage);
    }
}

static Av2hStatus decode_luma_leaf(Av2hFrameDecodeContext *context) {
    Av2hStatus status;
    uint8_t has_leaf = 0U;

    Av2hTileLumaLeaf leaf;
    Av2hCoeffBlock *block = &context->scratch->active_blocks[0];
    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(block, 0U, sizeof(*block));
    status = av2h_tile_decoder_next_luma(context->decoder, &leaf, block, &has_leaf);
    if (status != AV2H_OK)
        return populate_luma_parse_failure(context, status, &leaf, block);
    if (!has_leaf)
        return AV2H_OK;
    if (context->workspace->bytes_per_sample == 1U) {
        status = av2h_luma_reconstruct_to_plane(
            context->workspace->diagnostics, &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
            block, &context->workspace->coded[0], context->workspace->reconstruction_scratch);
    } else {
        status = av2h_luma_reconstruct_to_plane_u16(
            context->workspace->diagnostics, &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
            block, &context->workspace->coded_u16[0],
            context->workspace->reconstruction_scratch_u16, context->domain);
    }
    if (status != AV2H_OK) {
        const Av2hReconstructionDiagnostics *diagnostics =
            av2h_reconstruction_diagnostics_get(context->workspace->diagnostics);
        luma_failure_context(context->decoded, &leaf, diagnostics);
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT, 0U,
                                    &leaf.node, diagnostics != NULL ? diagnostics->luma_stage : 0U);
    }
    status = av2h_deblock_map_commit_transform(
        &context->workspace->filter_workspace->deblock_map, context->frame->chroma_format_idc, 0U,
        &leaf.node, &leaf.prefix, leaf.mode.skip_transform, leaf.prefix.all_zero,
        (uint8_t)av2h_quantizer_is_lossless(context->sequence, context->frame,
                                            context->frame->base_q_index));
    if (status != AV2H_OK) {
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK, 0U,
                                    &leaf.node, 0U);
    }
    if (av2h_frame_restoration_needs_tx_skip(context->frame)) {
        status = av2h_restoration_tx_skip_commit(context->workspace->filter_workspace, &leaf.node,
                                                 &leaf.prefix);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK, 0U,
                                        &leaf.node, 0U);
        }
    }
    status = av2h_tile_decoder_commit_leaf(context->decoder, leaf.node);
    if (status != AV2H_OK) {
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_LUMA_COMMIT, 0U,
                                    &leaf.node, 0U);
    }
    ++context->decoded->luma_leaf_count;

    return AV2H_OK;
}

static Av2hStatus populate_chroma_parse_failure(Av2hFrameDecodeContext *context, Av2hStatus status,
                                                const Av2hTileChromaLeaf *leaf,
                                                const Av2hCoeffBlock blocks[2U]) {

    const Av2hTileDiagnostics *tile_diagnostics =
        av2h_tile_diagnostics_get(context->workspace->diagnostics);
    const Av2hLeafDiagnostics *leaf_diagnostics =
        av2h_leaf_diagnostics_get(context->workspace->diagnostics);
    uint8_t chroma_tile_stage = tile_diagnostics != NULL ? tile_diagnostics->chroma_tile_stage : 0U;
    unsigned int diagnostic_index = chroma_tile_stage >= 4U ? 1U : 0U;
    chroma_failure_context(context->decoded, leaf, diagnostic_index);
    context->decoded->syntax_stage =
        leaf_diagnostics != NULL ? leaf_diagnostics->chroma_mode_stage : 0U;
    context->decoded->chroma_mode_index = leaf->mode.mode_index;
    context->decoded->txb_skip_context = leaf->prefix[diagnostic_index].txb_skip_context;
    context->decoded->all_zero = leaf->prefix[diagnostic_index].all_zero;
    context->decoded->eob_point = leaf->prefix[diagnostic_index].eob_point;
    context->decoded->after_all_zero_tell = leaf->prefix[diagnostic_index].after_all_zero_bits;
    context->decoded->after_eob_point_tell = leaf->prefix[diagnostic_index].after_eob_point_bits;
    context->decoded->coefficient_position = blocks[diagnostic_index].diagnostic_position;
    context->decoded->coefficient_context = blocks[diagnostic_index].diagnostic_context;
    context->decoded->coefficient_kind = blocks[diagnostic_index].diagnostic_kind;
    context->decoded->chroma_parse_reason =
        tile_diagnostics != NULL ? tile_diagnostics->chroma_parse_reason : 0U;
    return frame_decode_failure(
        context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE, chroma_tile_stage >= 4U ? 2U : 1U,
        &leaf->node, chroma_tile_stage != 0U ? chroma_tile_stage : context->decoded->syntax_stage);
}

static Av2hStatus populate_chroma_reconstruction_failure(Av2hFrameDecodeContext *context,
                                                         Av2hStatus status,
                                                         const Av2hTileChromaLeaf *leaf,
                                                         unsigned int index) {
    const Av2hReconstructionDiagnostics *diagnostics =
        av2h_reconstruction_diagnostics_get(context->workspace->diagnostics);
    chroma_failure_context(context->decoded, leaf, index);
    context->decoded->is_cfl = leaf->mode.is_cfl;
    context->decoded->cfl_kind = (uint8_t)leaf->mode.cfl_kind;
    context->decoded->mhccp_direction = leaf->mode.mhccp_direction;
    if (context->workspace->bytes_per_sample == 1U) {
        const Av2hPredictionDiagnostics *prediction =
            av2h_prediction_diagnostics_get(context->workspace->diagnostics);
        context->decoded->cfl_filter_index = leaf->mode.cfl_downsample_filter_index;
        context->decoded->mhccp_reason = prediction != NULL ? prediction->mhccp_reason : 0U;
    } else {
        context->decoded->chroma_mode_index = leaf->mode.mode_index;
    }
    return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                                index + 1U, &leaf->node,
                                diagnostics != NULL ? diagnostics->chroma_stage : 0U);
}

static Av2hStatus reconstruct_chroma_plane(Av2hFrameDecodeContext *context,
                                           const Av2hTileChromaLeaf *leaf,
                                           const Av2hCoeffBlock blocks[2U], unsigned int plane) {
    Av2hFrameDecodeWorkspace *workspace = context->workspace;
    unsigned int index = plane - 1U;
    if (workspace->bytes_per_sample == 1U) {
        return av2h_chroma_reconstruct_to_plane(
            workspace->diagnostics, &leaf->node, &leaf->mode, &leaf->prefix[index],
            &leaf->transform[index], &blocks[index], &workspace->coded[plane], &workspace->coded[0],
            (uint8_t)plane, NULL, context->frame->chroma_format_idc,
            workspace->reconstruction_scratch);
    }
    return av2h_chroma_reconstruct_to_plane_u16(
        workspace->diagnostics, &leaf->node, &leaf->mode, &leaf->prefix[index],
        &leaf->transform[index], &blocks[index], &workspace->coded_u16[plane],
        &workspace->coded_u16[0], (uint8_t)plane, context->frame->chroma_format_idc,
        workspace->reconstruction_scratch_u16, context->domain);
}

static Av2hStatus commit_chroma_deblock(Av2hFrameDecodeContext *context,
                                        const Av2hTileChromaLeaf *leaf, unsigned int plane,
                                        uint8_t lossless) {
    unsigned int index = plane - 1U;
    return av2h_deblock_map_commit_transform(
        &context->workspace->filter_workspace->deblock_map, context->frame->chroma_format_idc,
        plane, &leaf->node, &leaf->prefix[index], 0U, leaf->prefix[index].all_zero, lossless);
}

static Av2hStatus flush_pending_chroma_leaves(Av2hFrameDecodeContext *context) {
    uint8_t pending_index;
    for (pending_index = 0U; pending_index < context->pending_chroma_count; ++pending_index) {
        const Av2hTileChromaLeaf *leaf = &context->scratch->pending_chroma_leaves[pending_index];
        unsigned int plane;
        for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            Av2hStatus status = reconstruct_chroma_plane(
                context, leaf, context->scratch->pending_chroma_blocks[pending_index], plane);
            if (status != AV2H_OK) {
                return frame_decode_failure(context, status,
                                            AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT, plane,
                                            &leaf->node, 0U);
            }
            status = commit_chroma_deblock(
                context, leaf, plane,
                (uint8_t)av2h_quantizer_is_lossless(context->sequence, context->frame,
                                                    context->frame->base_q_index));
            if (status != AV2H_OK) {
                return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                                            plane, &leaf->node, 0U);
            }
        }
        ++context->decoded->chroma_leaf_count;
    }
    context->pending_chroma_count = 0U;
    return AV2H_OK;
}

static Av2hStatus decode_chroma_leaf(Av2hFrameDecodeContext *context) {
    Av2hTileChromaLeaf leaf;
    Av2hCoeffBlock *blocks = context->scratch->active_blocks;
    uint8_t has_leaf = 0U;
    unsigned int plane;
    Av2hStatus status;
    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(blocks, 0U, sizeof(context->scratch->active_blocks));
    status =
        av2h_tile_decoder_next_chroma(context->decoder, &leaf, &blocks[0], &blocks[1], &has_leaf);
    if (status != AV2H_OK)
        return populate_chroma_parse_failure(context, status, &leaf, blocks);
    if (!has_leaf)
        return AV2H_OK;
    if (leaf.decoded_plane != 0U) {
        plane = leaf.decoded_plane;
        status = reconstruct_chroma_plane(context, &leaf, blocks, plane);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                                        plane, &leaf.node, 0U);
        }
        status = commit_chroma_deblock(context, &leaf, plane, 1U);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                                        plane, &leaf.node, 0U);
        }
        status = av2h_tile_decoder_commit_leaf(context->decoder, leaf.node);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT,
                                        plane, &leaf.node, 0U);
        }
        if (plane == 2U)
            ++context->decoded->chroma_leaf_count;
        return AV2H_OK;
    }
    context->decoded->cctx_type = leaf.transform[0].cctx_type;
    context->decoded->u_coefficient_count = blocks[0].coefficient_count;
    context->decoded->v_coefficient_count = blocks[1].coefficient_count;
    status = av2h_chroma_inverse_cross_transform(
        &blocks[0], &blocks[1], leaf.transform[0].cctx_type, context->domain->bit_depth);
    if (status != AV2H_OK) {
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM,
                                    0U, &leaf.node, 0U);
    }
    if (context->decoder->chroma.shared_resume_luma &&
        !(context->decoder->luma.coding_mode.q_index == 0U &&
          context->decoder->luma.coding_mode.lossless_tx_size_large == 0U)) {
        if (context->pending_chroma_count >= AV2H_PENDING_CHROMA_CAPACITY) {
            return frame_decode_failure(context, AV2H_OVERFLOW,
                                        AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT, 0U, &leaf.node, 0U);
        }
        av2h_memory_copy(&context->scratch->pending_chroma_leaves[context->pending_chroma_count],
                         &leaf, sizeof(leaf));
        av2h_memory_copy(context->scratch->pending_chroma_blocks[context->pending_chroma_count],
                         blocks, sizeof(context->scratch->active_blocks));
        ++context->pending_chroma_count;
        status = av2h_tile_decoder_commit_leaf(context->decoder, leaf.node);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT, 0U,
                                        &leaf.node, 0U);
        }
        return AV2H_OK;
    }
    for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        status = reconstruct_chroma_plane(context, &leaf, blocks, plane);
        if (status != AV2H_OK)
            return populate_chroma_reconstruction_failure(context, status, &leaf, plane - 1U);
        /* Wide paths commit per plane; byte paths reconstruct both planes first. */
        if (context->workspace->bytes_per_sample == 2U) {
            status = commit_chroma_deblock(
                context, &leaf, plane,
                (uint8_t)av2h_quantizer_is_lossless(context->sequence, context->frame,
                                                    context->frame->base_q_index));
            if (status != AV2H_OK) {
                return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                                            plane, &leaf.node, 0U);
            }
        }
    }
    if (context->workspace->bytes_per_sample == 1U) {
        for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            status = commit_chroma_deblock(
                context, &leaf, plane,
                (uint8_t)av2h_quantizer_is_lossless(context->sequence, context->frame,
                                                    context->frame->base_q_index));
            if (status != AV2H_OK) {
                return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                                            plane, &leaf.node, 0U);
            }
        }
    }
    status = av2h_tile_decoder_commit_leaf(context->decoder, leaf.node);
    if (status != AV2H_OK) {
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT, 0U,
                                    &leaf.node, 0U);
    }
    ++context->decoded->chroma_leaf_count;
    return AV2H_OK;
}

static Av2hStatus decode_tile_leaves(Av2hFrameDecodeContext *context) {
    while (context->decoder->control.phase != AV2H_TILE_DECODE_DONE) {
        Av2hStatus status;
        if (context->decoder->control.phase == AV2H_TILE_DECODE_LUMA &&
            context->decoder->chroma.shared_complete && context->pending_chroma_count != 0U &&
            context->decoder->luma.tx_count == 0U) {
            status = flush_pending_chroma_leaves(context);
            if (status != AV2H_OK)
                return status;
        }
        if (context->decoder->control.phase == AV2H_TILE_DECODE_LUMA) {
            status = decode_luma_leaf(context);
        } else if (context->decoder->control.phase == AV2H_TILE_DECODE_CHROMA) {
            status = decode_chroma_leaf(context);
        } else {
            return frame_decode_failure(context, AV2H_INVALID_DATA,
                                        AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, NULL, 0U);
        }
        if (status != AV2H_OK)
            return status;
    }
    return AV2H_OK;
}

static Av2hStatus finish_frame_tiles(Av2hFrameDecodeContext *context,
                                     const Av2hFilterWorkspaceLayout *layout) {
    Av2hTileFilterControls filter_controls;
    Av2hStatus status;
    unsigned int plane;
    if (!av2h_frame_workspace_complete(context->frame, context->workspace)) {
        return frame_decode_failure(context, AV2H_INVALID_DATA,
                                    AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, NULL, 0U);
    }
    if (context->decoder->entropy_state.restoration_entropy.active_mask != 0U) {
        for (plane = 0U; plane < av2h_frame_plane_count(context->frame); ++plane) {
            if ((context->decoder->entropy_state.restoration_entropy.active_mask & (1U << plane)) !=
                    0U &&
                context->decoder->entropy_state.restoration_entropy.decoded_count[plane] !=
                    context->frame->restoration_unit_count[plane]) {
                return frame_decode_failure(context, AV2H_INVALID_DATA,
                                            AV2H_FRAME_DECODE_STAGE_COMPLETENESS, plane, NULL, 0U);
            }
        }
    }
    status = av2h_tile_filter_controls_handoff(context->decoder, &filter_controls);
    if (status != AV2H_OK) {
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, NULL,
                                    0U);
    }
    context->decoded->entropy_tell = av2h_range_tell(&context->decoder->entropy_state.range);
    context->workspace->filter_workspace->cdef_controls = filter_controls.cdef;
    context->workspace->filter_workspace->gdf_controls = filter_controls.gdf;
    for (plane = 0U; plane < av2h_frame_plane_count(context->frame); ++plane) {
        context->workspace->filter_workspace->restoration_controls.unit_flags[plane] =
            context->decoder->entropy_state.restoration_entropy.unit_flags[plane];
        context->workspace->filter_workspace->restoration_controls.unit_taps[plane] =
            context->decoder->entropy_state.restoration_entropy.unit_taps[plane];
        context->workspace->filter_workspace->restoration_controls.unit_columns[plane] =
            context->decoder->entropy_state.restoration_entropy.unit_columns[plane];
        context->workspace->filter_workspace->restoration_controls.unit_rows[plane] =
            context->decoder->entropy_state.restoration_entropy.unit_rows[plane];
        context->workspace->filter_workspace->restoration_controls.unit_size[plane] =
            context->decoder->entropy_state.restoration_entropy.unit_size[plane];
    }
    for (plane = 0U; plane < av2h_frame_plane_count(context->frame); ++plane) {
        if (filter_controls.ccso.plane[plane] != NULL) {
            av2h_memory_copy(
                (void *)context->workspace->filter_workspace->ccso_controls.plane[plane],
                filter_controls.ccso.plane[plane], layout->ccso_control_count);
        }
        context->workspace->filter_workspace->ccso_controls.stride_units[plane] =
            filter_controls.ccso.stride_units[plane];
    }
    context->workspace->filter_workspace->ccso_controls.columns = filter_controls.ccso.columns;
    context->workspace->filter_workspace->ccso_controls.rows = filter_controls.ccso.rows;

    return AV2H_OK;
}

static Av2hStatus filter_and_output_frame(Av2hFrameDecodeContext *context, uint8_t *output,
                                          size_t output_size_bytes,
                                          const Av2hExecution *execution) {
    Av2hFilterPlanes final_planes;
    Av2hFilterPlanesU16 final_planes_u16;
    Av2hStatus status;
    unsigned int plane;
    if (context->workspace->bytes_per_sample == 2U) {
        for (plane = 0U; plane < av2h_frame_plane_count(context->frame); ++plane) {
            final_planes_u16.plane[plane] =
                (Av2hFilterPlaneU16){context->workspace->coded_u16[plane].data,
                                     context->workspace->coded_u16[plane].stride_samples,
                                     context->workspace->coded_u16[plane].width,
                                     context->workspace->coded_u16[plane].height};
        }
        status = av2h_filter_stage_u16(&final_planes_u16, context->sequence, context->frame,
                                       context->workspace->filter_workspace, context->domain);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_FILTER, 0U, NULL,
                                        0U);
        }
        status = av2h_copy_visible_planar_u16_private(context->frame, context->workspace->coded_u16,
                                                      context->workspace->coded_data_size_bytes,
                                                      output, output_size_bytes, execution);
        if (status != AV2H_OK) {
            return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_OUTPUT, 0U, NULL,
                                        0U);
        }
        return AV2H_OK;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(context->frame); ++plane) {
        final_planes.plane[plane] = (Av2hFilterPlane){
            context->workspace->coded[plane].data, context->workspace->coded[plane].stride_samples,
            context->workspace->coded[plane].width, context->workspace->coded[plane].height};
    }
    status = av2h_filter_frame(&final_planes, context->sequence, context->frame,
                               context->workspace->filter_workspace);
    if (status != AV2H_OK) {
        const Av2hFilterDiagnostics *filter_diagnostics =
            av2h_filter_diagnostics_get(context->workspace->diagnostics);
        context->decoded->filter_reason =
            filter_diagnostics != NULL ? filter_diagnostics->reason : 0U;
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_FILTER, 0U, NULL, 0U);
    }
    status = av2h_copy_visible_planar_private(context->frame, &final_planes,
                                              context->workspace->coded_data_size_bytes, output,
                                              output_size_bytes, execution);
    if (status != AV2H_OK) {
        return frame_decode_failure(context, status, AV2H_FRAME_DECODE_STAGE_OUTPUT, 0U, NULL, 0U);
    }
    return AV2H_OK;
}

static Av2hStatus decode_frame_tiles_planar(const Av2hSpan *tiles, size_t tile_count,
                                            uint16_t start_tile, const Av2hSequenceState *sequence,
                                            const Av2hFrameState *frame,
                                            Av2hFrameDecodeWorkspace *workspace, uint8_t *output,
                                            size_t output_size_bytes, Av2hFrameDecodeResult *result,
                                            const Av2hExecution *execution) {
    Av2hFrameDecodeContext context;
    Av2hFrameDecodeResult decoded = {0};
    Av2hFilterWorkspaceLayout layout;
    size_t required_output_bytes;
    size_t tile_index;
    Av2hStatus status;
    if (tiles == NULL || tile_count == 0U || tile_count > AV2H_PROFILE_MAX_TILES ||
        sequence == NULL || frame == NULL || workspace == NULL || output == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    context.sequence = sequence;
    context.frame = frame;
    context.workspace = workspace;
    context.domain = av2h_sample_domain_from_idc(sequence->info.bit_depth_idc);
    context.decoded = &decoded;
    reset_frame_diagnostics(&context);
    if (workspace->bytes_per_sample != 1U && workspace->bytes_per_sample != 2U)
        return AV2H_UNSUPPORTED;
    if ((workspace->bytes_per_sample == 1U && workspace->reconstruction_scratch == NULL) ||
        (workspace->bytes_per_sample == 2U && workspace->reconstruction_scratch_u16 == NULL) ||
        workspace->filter_workspace == NULL)
        return AV2H_INVALID_ARGUMENT;
    status = av2h_planar_output_size(frame, workspace->bytes_per_sample, &required_output_bytes);
    if (status != AV2H_OK)
        return status;
    if (output_size_bytes < required_output_bytes)
        return AV2H_OUT_OF_MEMORY;
    status = av2h_frame_workspace_prepare(sequence, frame, workspace, output, required_output_bytes,
                                          result, &layout);
    if (status != AV2H_OK)
        return status;
    context.scratch = workspace->decode_scratch;
    context.decoder = &context.scratch->tile_decoder;
    for (tile_index = 0U; tile_index < tile_count; ++tile_index) {
        uint16_t tile_id = (uint16_t)(start_tile + tile_index);
        context.pending_chroma_count = 0U;
        status = av2h_tile_decoder_init(context.decoder, tiles[tile_index], sequence, frame,
                                        &workspace->tile_context, tile_id);
        if (status != AV2H_OK)
            return status;
        tile_readiness_clear(frame, workspace, context.decoder);
        context.decoder->diagnostics = workspace->diagnostics;
        context.decoder->entropy_state.range.diagnostics = workspace->diagnostics;
        context.decoder->traversal.luma_traversal.diagnostics = workspace->diagnostics;
        context.decoder->traversal.chroma_traversal.diagnostics = workspace->diagnostics;
        status = decode_tile_leaves(&context);
        if (status != AV2H_OK)
            goto finished;
        if (!tile_readiness_complete(frame, workspace, context.decoder)) {
            status = frame_decode_failure(&context, AV2H_INVALID_DATA,
                                          AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, NULL, 0U);
            goto finished;
        }
        tile_readiness_restore_previous(sequence, frame, workspace, tile_id);
    }
    status = finish_frame_tiles(&context, &layout);
    if (status != AV2H_OK)
        goto finished;
    status = filter_and_output_frame(&context, output, output_size_bytes, execution);
finished:
    if (result != NULL)
        *result = decoded;
    return status;
}

Av2hStatus av2h_decode_frame_group_planar_private(
    const Av2hTileGroup *group, const Av2hSequenceState *sequence, const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace, uint8_t *output, size_t output_size_bytes,
    Av2hFrameDecodeResult *result, const Av2hExecution *execution) {
    if (group == NULL)
        return AV2H_INVALID_ARGUMENT;
    return decode_frame_tiles_planar(group->tiles, group->tile_count, group->start_tile, sequence,
                                     frame, workspace, output, output_size_bytes, result,
                                     execution);
}

Av2hStatus av2h_decode_frame_planar_private(Av2hSpan entropy, const Av2hSequenceState *sequence,
                                            const Av2hFrameState *frame,
                                            Av2hFrameDecodeWorkspace *workspace, uint8_t *output,
                                            size_t output_size_bytes, Av2hFrameDecodeResult *result,
                                            const Av2hExecution *execution) {
    return decode_frame_tiles_planar(&entropy, 1U, 0U, sequence, frame, workspace, output,
                                     output_size_bytes, result, execution);
}
