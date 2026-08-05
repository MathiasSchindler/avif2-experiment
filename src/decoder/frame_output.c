#include "decoder/frame_output.h"

#include "decoder/internal/decode_diagnostics.h"
#include "decoder/internal/filter_diagnostics.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/internal/tile_modules.h"

Av2hStatus av2h_decode_frame_i420_private(
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size,
    Av2hFrameDecodeResult *result) {
    Av2hFilterWorkspaceLayout layout;
    Av2hTileDecoder decoder;
    Av2hTileFilterControls filter_controls;
    Av2hFilterPlanes final_planes;
    Av2hFrameDecodeResult decoded = { 0 };
    size_t required_output;
    Av2hStatus status;
    unsigned int plane;
#define AV2H_DECODE_FAIL(returned_status, original_cause, failure_stage,       \
                         failure_plane, failure_leaf, failure_syntax_stage)   \
    do {                                                                       \
    const Av2hPartitionNode *diagnostic_leaf = (failure_leaf);             \
        decoded.entropy_tell = av2h_range_tell(&decoder.range);               \
        decoded.cause = (original_cause);                                      \
        decoded.stage = (failure_stage);                                       \
        decoded.plane = (uint8_t)(failure_plane);                              \
        decoded.syntax_stage = (uint8_t)(failure_syntax_stage);                \
        if (diagnostic_leaf != 0) {                                            \
            decoded.row = diagnostic_leaf->row;                                \
            decoded.column = diagnostic_leaf->column;                          \
            decoded.block_size = (uint8_t)diagnostic_leaf->block_size;         \
        }                                                                      \
        if (result != 0) *result = decoded;                                    \
        return (returned_status);                                              \
    } while (0)
    if (sequence == 0 || frame == 0 || workspace == 0 || output == 0 ||
        workspace->reconstruction_scratch == 0 ||
        workspace->filter_workspace == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->enable_restoration != 0U) return AV2H_UNSUPPORTED;
    status = av2h_i420_output_size(frame, &required_output);
    if (status != AV2H_OK) return status;
    if (output_size < required_output) return AV2H_OUT_OF_MEMORY;
    status = av2h_frame_workspace_prepare(
        sequence, frame, workspace, output, required_output, result,
        &layout);
    if (status != AV2H_OK) return status;
    status = av2h_tile_decoder_init(
        &decoder, entropy, sequence, frame, &workspace->tile_context);
    if (status != AV2H_OK) return status;
    while (decoder.phase != AV2H_TILE_DECODE_DONE) {
        uint8_t has_leaf = 0U;
        if (decoder.phase == AV2H_TILE_DECODE_LUMA) {
            Av2hTileLumaLeaf leaf;
            Av2hFirstCoeffBlock block;
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(&block, 0U, sizeof(block));
            status = av2h_tile_decoder_next_luma(
                &decoder, &leaf, &block, &has_leaf);
            if (status != AV2H_OK) {
                const Av2hTileDiagnostics *tile_diagnostics =
                    av2h_tile_diagnostics_get();
                const Av2hLeafDiagnostics *leaf_diagnostics =
                    av2h_leaf_diagnostics_get();
                const Av2hCoefficientDiagnostics *coefficient_diagnostics =
                    av2h_coefficient_diagnostics_get();
                const Av2hTransformDiagnostics *transform_diagnostics =
                    av2h_transform_diagnostics_get();
                const Av2hReconstructionDiagnostics *
                    reconstruction_diagnostics =
                        av2h_reconstruction_diagnostics_get();
                uint8_t luma_tile_stage = tile_diagnostics != 0 ?
                    tile_diagnostics->luma_tile_stage : 0U;
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_OVERFLOW : status;
                decoded.mode = (uint8_t)leaf.mode.mode;
                decoded.angle_delta = leaf.mode.angle_delta;
                decoded.mrl_index = leaf.mode.mrl_index;
                decoded.multi_line_mrl = leaf.mode.multi_line_mrl;
                decoded.enable_ibp = leaf.mode.enable_ibp;
                decoded.use_intrabc = leaf.mode.use_intrabc;
                decoded.displacement_x = leaf.mode.displacement_x;
                decoded.displacement_y = leaf.mode.displacement_y;
                decoded.morph_pred = leaf.mode.morph_pred;
                decoded.fsc_mode = leaf.mode.fsc_mode;
                decoded.skip_transform = leaf.mode.skip_transform;
                decoded.palette_size = leaf.mode.palette_size;
                decoded.tx_size = (uint8_t)leaf.prefix.tx_size;
                decoded.inverse_transform_stage =
                    reconstruction_diagnostics != 0 ?
                        reconstruction_diagnostics->inverse_transform_stage :
                        0U;
                decoded.primary_type = (uint8_t)leaf.transform.primary_type;
                decoded.primary_symbol = leaf.transform.primary_symbol;
                decoded.primary_set_symbol =
                    leaf.transform.primary_long_side_symbol;
                decoded.secondary_symbol = leaf.transform.secondary_symbol;
                decoded.secondary_set = leaf.transform.secondary_set;
                if (luma_tile_stage == 1U) {
                    decoded.syntax_stage = leaf_diagnostics != 0 ?
                        leaf_diagnostics->luma_mode_stage : 0U;
                }
                if (luma_tile_stage == 3U) {
                    decoded.syntax_stage = transform_diagnostics != 0 ?
                        transform_diagnostics->transform_stage : 0U;
                }
                decoded.eob = leaf.prefix.eob;
                decoded.prefix_stage = coefficient_diagnostics != 0 ?
                    coefficient_diagnostics->luma_prefix_stage : 0U;
                decoded.txb_skip_context = leaf.prefix.txb_skip_context;
                decoded.all_zero = leaf.prefix.all_zero;
                decoded.eob_point = leaf.prefix.eob_point;
                decoded.after_all_zero_tell =
                    leaf.prefix.after_all_zero_bits;
                decoded.after_eob_point_tell =
                    leaf.prefix.after_eob_point_bits;
                decoded.eob_range = coefficient_diagnostics != 0 ?
                    coefficient_diagnostics->eob_range : 0U;
                decoded.skip_range = coefficient_diagnostics != 0 ?
                    coefficient_diagnostics->skip_range : 0U;
                decoded.before_prefix_range = tile_diagnostics != 0 ?
                    tile_diagnostics->luma_stage_ranges[1] : 0U;
                decoded.eob_tx_size = coefficient_diagnostics != 0 ?
                    coefficient_diagnostics->eob_tx : 0U;
                decoded.eob_class = coefficient_diagnostics != 0 ?
                    coefficient_diagnostics->eob_class : 0U;
                decoded.coefficient_position = block.diagnostic_position;
                decoded.coefficient_context = block.diagnostic_context;
                decoded.coefficient_kind = block.diagnostic_kind;
                {
                    const Av2hPartitionNode *failure_node =
                        luma_tile_stage == 0U ?
                            &decoder.luma_traversal.diagnostic_node :
                            &leaf.node;
                    AV2H_DECODE_FAIL(
                        returned, status,
                        AV2H_FRAME_DECODE_STAGE_LUMA_PARSE,
                        0U, failure_node, decoded.syntax_stage != 0U ?
                            decoded.syntax_stage : luma_tile_stage);
                }
            }
            if (!has_leaf) continue;
            status = av2h_luma_reconstruct_to_plane(
                &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                &block, &workspace->coded[0],
                workspace->reconstruction_scratch);
            if (status != AV2H_OK) {
                const Av2hReconstructionDiagnostics *diagnostics =
                    av2h_reconstruction_diagnostics_get();
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_OVERFLOW : status;
                decoded.mode = (uint8_t)leaf.mode.mode;
                decoded.angle_delta = leaf.mode.angle_delta;
                decoded.mrl_index = leaf.mode.mrl_index;
                decoded.multi_line_mrl = leaf.mode.multi_line_mrl;
                decoded.enable_ibp = leaf.mode.enable_ibp;
                decoded.use_intrabc = leaf.mode.use_intrabc;
                decoded.displacement_x = leaf.mode.displacement_x;
                decoded.displacement_y = leaf.mode.displacement_y;
                decoded.morph_pred = leaf.mode.morph_pred;
                decoded.fsc_mode = leaf.mode.fsc_mode;
                decoded.skip_transform = leaf.mode.skip_transform;
                decoded.tx_size = (uint8_t)leaf.prefix.tx_size;
                decoded.inverse_transform_stage =
                    diagnostics != 0 ?
                        diagnostics->inverse_transform_stage : 0U;
                decoded.primary_type = (uint8_t)leaf.transform.primary_type;
                decoded.primary_symbol = leaf.transform.primary_symbol;
                decoded.secondary_symbol = leaf.transform.secondary_symbol;
                decoded.secondary_set = leaf.transform.secondary_set;
                decoded.eob = leaf.prefix.eob;
                AV2H_DECODE_FAIL(
                    returned, status,
                    AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT,
                    0U, &leaf.node,
                    diagnostics != 0 ? diagnostics->luma_stage : 0U);
            }
            status = av2h_deblock_map_commit_transform(
                &workspace->filter_workspace->deblock_map, 0U,
                &leaf.node, &leaf.prefix, (uint8_t)frame->base_q_index,
                leaf.mode.skip_transform, leaf.prefix.all_zero,
                (uint8_t)(frame->base_q_index == 0U));
            if (status != AV2H_OK) {
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_OVERFLOW : status;
                AV2H_DECODE_FAIL(
                    returned, status, AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK,
                    0U, &leaf.node, 0U);
            }
            status = av2h_tile_decoder_commit_leaf(&decoder, leaf.node);
            if (status != AV2H_OK) {
                AV2H_DECODE_FAIL(
                    status, status, AV2H_FRAME_DECODE_STAGE_LUMA_COMMIT,
                    0U, &leaf.node, 0U);
            }
            ++decoded.luma_leaf_count;
        } else if (decoder.phase == AV2H_TILE_DECODE_CHROMA) {
            Av2hTileChromaLeaf leaf;
            Av2hFirstCoeffBlock blocks[2U];
            av2h_memory_fill(&leaf, 0U, sizeof(leaf));
            av2h_memory_fill(blocks, 0U, sizeof(blocks));
            status = av2h_tile_decoder_next_chroma(
                &decoder, &leaf, &blocks[0], &blocks[1], &has_leaf);
            if (status != AV2H_OK) {
                const Av2hTileDiagnostics *tile_diagnostics =
                    av2h_tile_diagnostics_get();
                const Av2hLeafDiagnostics *leaf_diagnostics =
                    av2h_leaf_diagnostics_get();
                uint8_t chroma_tile_stage = tile_diagnostics != 0 ?
                    tile_diagnostics->chroma_tile_stage : 0U;
                unsigned int diagnostic_index =
                    chroma_tile_stage >= 4U ? 1U : 0U;
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_INVALID_ARGUMENT : status;
                decoded.mode = (uint8_t)leaf.mode.mode;
                decoded.angle_delta = leaf.mode.angle_delta;
                decoded.syntax_stage = leaf_diagnostics != 0 ?
                    leaf_diagnostics->chroma_mode_stage : 0U;
                decoded.chroma_mode_index = leaf.mode.mode_index;
                decoded.tx_size =
                    (uint8_t)leaf.prefix[diagnostic_index].tx_size;
                decoded.primary_type = (uint8_t)
                    leaf.transform[diagnostic_index].primary_type;
                decoded.primary_symbol =
                    leaf.transform[diagnostic_index].primary_symbol;
                decoded.secondary_symbol =
                    leaf.transform[diagnostic_index].secondary_symbol;
                decoded.secondary_set =
                    leaf.transform[diagnostic_index].secondary_set;
                decoded.eob = leaf.prefix[diagnostic_index].eob;
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
                    tile_diagnostics != 0 ?
                        tile_diagnostics->chroma_parse_reason : 0U;
                AV2H_DECODE_FAIL(
                    returned, status, AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE,
                    chroma_tile_stage >= 4U ? 2U : 1U,
                    &leaf.node, chroma_tile_stage != 0U ?
                        chroma_tile_stage : decoded.syntax_stage);
            }
            if (!has_leaf) continue;
            decoded.cctx_type = leaf.transform[0].cctx_type;
            decoded.u_coefficient_count = blocks[0].coefficient_count;
            decoded.v_coefficient_count = blocks[1].coefficient_count;
            status = av2h_chroma_inverse_cross_transform(
                &blocks[0], &blocks[1], leaf.transform[0].cctx_type);
            if (status != AV2H_OK) {
                AV2H_DECODE_FAIL(
                    status, status,
                    AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM,
                    0U, &leaf.node, 0U);
            }
            for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                unsigned int index = plane - 1U;
                status = av2h_chroma_reconstruct_to_plane_ex(
                    &leaf.node, &leaf.mode, &leaf.prefix[index],
                    &leaf.transform[index], &blocks[index],
                    &workspace->coded[plane], &workspace->coded[0],
                    (uint8_t)plane, 0,
                    workspace->reconstruction_scratch);
                if (status != AV2H_OK) {
                    const Av2hPredictionDiagnostics *prediction_diagnostics =
                        av2h_prediction_diagnostics_get();
                    const Av2hReconstructionDiagnostics *
                        reconstruction_diagnostics =
                            av2h_reconstruction_diagnostics_get();
                    Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                        AV2H_INVALID_DATA : status;
                    decoded.mode = (uint8_t)leaf.mode.mode;
                    decoded.angle_delta = leaf.mode.angle_delta;
                    decoded.tx_size = (uint8_t)leaf.prefix[index].tx_size;
                    decoded.primary_type =
                        (uint8_t)leaf.transform[index].primary_type;
                    decoded.primary_symbol =
                        leaf.transform[index].primary_symbol;
                    decoded.secondary_symbol =
                        leaf.transform[index].secondary_symbol;
                    decoded.secondary_set =
                        leaf.transform[index].secondary_set;
                    decoded.is_cfl = leaf.mode.is_cfl;
                    decoded.cfl_kind = (uint8_t)leaf.mode.cfl_kind;
                    decoded.mhccp_direction = leaf.mode.mhccp_direction;
                    decoded.cfl_filter_index =
                        leaf.mode.cfl_downsample_filter_index;
                    decoded.mhccp_reason = prediction_diagnostics != 0 ?
                        prediction_diagnostics->mhccp_reason : 0U;
                    decoded.eob = leaf.prefix[index].eob;
                    AV2H_DECODE_FAIL(
                        returned, status,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                        plane, &leaf.node,
                        reconstruction_diagnostics != 0 ?
                            reconstruction_diagnostics->chroma_stage : 0U);
                }
            }
            for (plane = 1U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
                unsigned int index = plane - 1U;
                status = av2h_deblock_map_commit_transform(
                    &workspace->filter_workspace->deblock_map, plane,
                    &leaf.node, &leaf.prefix[index],
                    (uint8_t)frame->base_q_index, 0U,
                    leaf.prefix[index].all_zero,
                    (uint8_t)(frame->base_q_index == 0U));
                if (status != AV2H_OK) {
                    Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                        AV2H_OUT_OF_MEMORY : status;
                    AV2H_DECODE_FAIL(
                        returned, status,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
                        plane, &leaf.node, 0U);
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
    if (!av2h_frame_workspace_complete(workspace)) {
        AV2H_DECODE_FAIL(
            AV2H_INVALID_DATA, AV2H_INVALID_DATA,
            AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, 0, 0U);
    }
    status = av2h_tile_filter_controls_handoff(&decoder, &filter_controls);
    if (status != AV2H_OK) {
        AV2H_DECODE_FAIL(
            status, status, AV2H_FRAME_DECODE_STAGE_COMPLETENESS,
            0U, 0, 0U);
    }
    decoded.entropy_tell = av2h_range_tell(&decoder.range);
    workspace->filter_workspace->cdef_controls = filter_controls.cdef;
    workspace->filter_workspace->gdf_controls = filter_controls.gdf;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (filter_controls.ccso.plane[plane] != 0) {
            av2h_memory_copy(
                (void *)workspace->filter_workspace->ccso_controls.plane[plane],
                filter_controls.ccso.plane[plane],
                layout.ccso_control_count);
        }
        workspace->filter_workspace->ccso_controls.stride[plane] =
            filter_controls.ccso.stride[plane];
        final_planes.plane[plane] = (Av2hFilterPlane){
            workspace->coded[plane].data,
            workspace->coded[plane].stride,
            workspace->coded[plane].width,
            workspace->coded[plane].height
        };
    }
    workspace->filter_workspace->ccso_controls.columns =
        filter_controls.ccso.columns;
    workspace->filter_workspace->ccso_controls.rows =
        filter_controls.ccso.rows;
    status = av2h_filter_frame(
        &final_planes, sequence, frame, workspace->filter_workspace);
    if (status != AV2H_OK) {
        Av2hStatus returned = status == AV2H_UNSUPPORTED ?
            AV2H_OUT_OF_MEMORY : status;
        const Av2hFilterDiagnostics *filter_diagnostics =
            av2h_filter_diagnostics_get();
        decoded.filter_reason = filter_diagnostics != 0 ?
            filter_diagnostics->reason : 0U;
        AV2H_DECODE_FAIL(
            returned, status, AV2H_FRAME_DECODE_STAGE_FILTER,
            0U, 0, 0U);
    }
    status = av2h_copy_visible_i420_private(
        frame, &final_planes, workspace->coded_data_size,
        output, output_size);
    if (status != AV2H_OK) {
        AV2H_DECODE_FAIL(
            status, status, AV2H_FRAME_DECODE_STAGE_OUTPUT,
            0U, 0, 0U);
    }
    if (result != 0) *result = decoded;
#undef AV2H_DECODE_FAIL
    return AV2H_OK;
}
