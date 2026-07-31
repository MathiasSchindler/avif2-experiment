#include "decoder/frame_output.h"

#include "decoder/tx_geometry.h"

extern uint8_t av2h_debug_mhccp_reason;
extern uint8_t av2h_debug_filter_reason;

typedef struct {
    size_t luma_mode_above;
    size_t luma_mode_left;
    size_t coefficient_above;
    size_t coefficient_left;
    size_t colocated_luma;
} Av2hFrameContextSizes;

static int ranges_overlap(const void *left,
                          size_t left_size,
                          const void *right,
                          size_t right_size) {
    uintptr_t maximum = (uintptr_t)-1;
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    uintptr_t left_end;
    uintptr_t right_end;
    if (left_size == 0U || right_size == 0U) return 0;
    if (left == 0 || right == 0 ||
        left_size > maximum - left_start ||
        right_size > maximum - right_start) {
        return 1;
    }
    left_end = left_start + left_size;
    right_end = right_start + right_size;
    return left_start < right_end && right_start < left_end;
}

static int output_overlaps(uint8_t *output,
                           size_t output_size,
                           const void *memory,
                           size_t memory_size) {
    return ranges_overlap(output, output_size, memory, memory_size);
}

static int sized_plane_bytes(size_t stride,
                             size_t height,
                             size_t *bytes) {
    return av2h_size_multiply(stride, height, bytes);
}

Av2hStatus av2h_i420_output_size(
    const Av2hFrameState *frame,
    size_t *size) {
    size_t luma;
    size_t chroma;
    size_t total;
    size_t chroma_width;
    size_t chroma_height;
    if (frame == 0 || size == 0 || frame->width == 0U ||
        frame->height == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    chroma_width = ((size_t)frame->width + 1U) >> 1U;
    chroma_height = ((size_t)frame->height + 1U) >> 1U;
    if (!av2h_size_multiply(frame->width, frame->height, &luma) ||
        !av2h_size_multiply(chroma_width, chroma_height, &chroma) ||
        !av2h_size_add(chroma, chroma, &total) ||
        !av2h_size_add(luma, total, &total)) {
        return AV2H_OVERFLOW;
    }
    *size = total;
    return AV2H_OK;
}

Av2hStatus av2h_deblock_map_commit_transform(
    Av2hDeblockMap *map,
    unsigned int plane,
    const Av2hPartitionNode *leaf,
    const Av2hFirstCoeffPrefix *prefix,
    uint8_t q_index,
    uint8_t skip,
    uint8_t all_zero,
    uint8_t lossless) {
    const Av2hTxDescriptor *descriptor;
    size_t block_column;
    size_t block_row;
    size_t transform_columns;
    size_t transform_rows;
    size_t row;
    Av2hStatus status;
    if (map == 0 || leaf == 0 || prefix == 0 ||
        plane >= AV2H_FRAME_PLANE_COUNT || skip > 1U ||
        all_zero > 1U || lossless > 1U || map->plane[plane] == 0 ||
        map->columns[plane] == 0U || map->rows[plane] == 0U ||
        map->stride[plane] < map->columns[plane]) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    if ((descriptor->output_width & 3U) != 0U ||
        (descriptor->output_height & 3U) != 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (plane == 0U) {
        block_column = leaf->column;
        block_row = leaf->row;
    } else {
        if ((leaf->column & 1U) != 0U || (leaf->row & 1U) != 0U) {
            return AV2H_UNSUPPORTED;
        }
        block_column = leaf->column >> 1U;
        block_row = leaf->row >> 1U;
    }
    transform_columns = descriptor->output_width >> 2U;
    transform_rows = descriptor->output_height >> 2U;
    if (block_column >= map->columns[plane] ||
        block_row >= map->rows[plane]) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (transform_columns > map->columns[plane] - block_column) {
        transform_columns = map->columns[plane] - block_column;
    }
    if (transform_rows > map->rows[plane] - block_row) {
        transform_rows = map->rows[plane] - block_row;
    }
    for (row = 0U; row < transform_rows; ++row) {
        size_t column;
        for (column = 0U; column < transform_columns; ++column) {
            Av2hDeblockCell *cell = (Av2hDeblockCell *)&map->plane[plane][
                (block_row + row) * map->stride[plane] +
                block_column + column];
            if (cell->transform_width != 0U ||
                cell->transform_height != 0U) {
                return AV2H_INVALID_DATA;
            }
            *cell = (Av2hDeblockCell){
                (uint16_t)block_column,
                (uint16_t)block_row,
                (uint16_t)block_column,
                (uint16_t)block_row,
                descriptor->output_width,
                descriptor->output_height,
                q_index,
                (uint8_t)(skip != 0U || all_zero != 0U),
                lossless
            };
        }
    }
    return AV2H_OK;
}

static int filter_workspace_valid(
    const Av2hFilterWorkspaceLayout *layout,
    const Av2hFilterWorkspace *workspace) {
    unsigned int plane;
    if (layout == 0 || workspace == 0 ||
        workspace->ccso_controls.columns != layout->ccso_columns ||
        workspace->ccso_controls.rows != layout->ccso_rows) {
        return 0;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hFilterPlane *deblocked =
            &workspace->deblocked.plane[plane];
        const Av2hFilterPlane *cdef = &workspace->cdef.plane[plane];
        if (deblocked->data == 0 || cdef->data == 0 ||
            deblocked->width != layout->plane_width[plane] ||
            deblocked->height != layout->plane_height[plane] ||
            cdef->width != layout->plane_width[plane] ||
            cdef->height != layout->plane_height[plane] ||
            deblocked->stride < deblocked->width ||
            cdef->stride < cdef->width ||
            workspace->deblock_map.plane[plane] == 0 ||
            workspace->deblock_map.columns[plane] !=
                layout->deblock_columns[plane] ||
            workspace->deblock_map.rows[plane] !=
                layout->deblock_rows[plane] ||
            workspace->deblock_map.stride[plane] !=
                layout->deblock_columns[plane] ||
            workspace->ccso_controls.plane[plane] == 0 ||
            workspace->ccso_controls.stride[plane] !=
                layout->ccso_columns) {
            return 0;
        }
    }
    return 1;
}

static int decode_planes_valid(
    const Av2hFrameState *frame,
    const Av2hFilterWorkspaceLayout *layout,
    const Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hReconstructionPlane *coded = &workspace->coded[plane];
        size_t required_data;
        size_t required_ready;
        size_t reference_width = plane == 0U ?
            frame->width : ((size_t)frame->width + 1U) >> 1U;
        size_t reference_height = plane == 0U ?
            frame->height : ((size_t)frame->height + 1U) >> 1U;
        if (coded->data == 0 || coded->reconstructed == 0 ||
            coded->width != layout->plane_width[plane] ||
            coded->height != layout->plane_height[plane] ||
            coded->stride < coded->width ||
            coded->reference_width != reference_width ||
            coded->reference_height != reference_height ||
            coded->reconstructed_stride <
                ((size_t)coded->width + 3U) / 4U ||
            !sized_plane_bytes(coded->stride, coded->height,
                               &required_data) ||
            required_data > workspace->coded_data_size[plane] ||
            !av2h_size_multiply(
                coded->reconstructed_stride,
                ((size_t)coded->height + 3U) / 4U,
                &required_ready) ||
            required_ready > coded->reconstructed_count) {
            return 0;
        }
    }
    return 1;
}

static int tile_context_valid(
    const Av2hFrameState *frame,
    const Av2hTileContextStorage *context,
    Av2hFrameContextSizes *sizes) {
    size_t mi_columns;
    size_t mi_rows;
    size_t cell_count;
    if (frame == 0 || context == 0 || sizes == 0 ||
        context->luma_partition_above == 0 ||
        context->luma_partition_left == 0 ||
        context->chroma_partition_above == 0 ||
        context->chroma_partition_left == 0 ||
        context->luma_mode_above == 0 || context->luma_mode_left == 0 ||
        context->coefficient_above == 0 ||
        context->coefficient_left == 0 ||
        context->colocated_luma == 0) {
        return 0;
    }
    mi_columns = ((size_t)frame->width + 3U) / 4U;
    mi_rows = ((size_t)frame->height + 3U) / 4U;
    if (context->above_count < mi_columns ||
        context->left_count < mi_rows ||
        !av2h_size_multiply(mi_columns, mi_rows, &cell_count) ||
        context->colocated_luma_count < cell_count ||
        !av2h_size_multiply(
            context->above_count, sizeof(*context->luma_mode_above),
            &sizes->luma_mode_above) ||
        !av2h_size_multiply(
            context->left_count, sizeof(*context->luma_mode_left),
            &sizes->luma_mode_left) ||
        !av2h_size_multiply(
            context->above_count, AV2H_FRAME_PLANE_COUNT,
            &sizes->coefficient_above) ||
        !av2h_size_multiply(
            context->left_count, AV2H_FRAME_PLANE_COUNT,
            &sizes->coefficient_left) ||
        !av2h_size_multiply(
            context->colocated_luma_count,
            sizeof(*context->colocated_luma),
            &sizes->colocated_luma) ||
        (context->gdf_block_flag_capacity != 0U &&
         context->gdf_block_flags == 0) ||
        (context->cdef_unit_strength_capacity != 0U &&
         context->cdef_unit_strengths == 0)) {
        return 0;
    }
    return 1;
}

static int output_is_disjoint(
    uint8_t *output,
    size_t output_size,
    const Av2hFilterWorkspaceLayout *layout,
    const Av2hFrameDecodeWorkspace *workspace,
    const Av2hFrameContextSizes *context_sizes,
    const Av2hFrameDecodeResult *result) {
    const Av2hTileContextStorage *context = &workspace->tile_context;
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hReconstructionPlane *coded = &workspace->coded[plane];
        const Av2hFilterPlane *deblocked =
            &workspace->filter_workspace->deblocked.plane[plane];
        const Av2hFilterPlane *cdef =
            &workspace->filter_workspace->cdef.plane[plane];
        size_t coded_size;
        size_t deblocked_size;
        size_t cdef_size;
        size_t map_size;
        size_t control_size;
        if (!sized_plane_bytes(coded->stride, coded->height, &coded_size) ||
            !sized_plane_bytes(
                deblocked->stride, deblocked->height, &deblocked_size) ||
            !sized_plane_bytes(cdef->stride, cdef->height, &cdef_size) ||
            !av2h_size_multiply(
                workspace->filter_workspace->deblock_map.stride[plane],
                workspace->filter_workspace->deblock_map.rows[plane],
                &map_size) ||
            !av2h_size_multiply(
                map_size, sizeof(Av2hDeblockCell), &map_size) ||
            !av2h_size_multiply(
                workspace->filter_workspace->ccso_controls.stride[plane],
                layout->ccso_rows, &control_size) ||
            output_overlaps(output, output_size, coded->data, coded_size) ||
            output_overlaps(
                output, output_size, coded->reconstructed,
                coded->reconstructed_count) ||
            output_overlaps(
                output, output_size, deblocked->data, deblocked_size) ||
            output_overlaps(output, output_size, cdef->data, cdef_size) ||
            output_overlaps(
                output, output_size,
                workspace->filter_workspace->deblock_map.plane[plane],
                map_size) ||
            output_overlaps(
                output, output_size,
                workspace->filter_workspace->ccso_controls.plane[plane],
                control_size)) {
            return 0;
        }
    }
    if (output_overlaps(
            output, output_size, context->luma_partition_above,
            context->above_count) ||
        output_overlaps(
            output, output_size, context->luma_partition_left,
            context->left_count) ||
        output_overlaps(
            output, output_size, context->chroma_partition_above,
            context->above_count) ||
        output_overlaps(
            output, output_size, context->chroma_partition_left,
            context->left_count) ||
        output_overlaps(
            output, output_size, context->luma_mode_above,
            context_sizes->luma_mode_above) ||
        output_overlaps(
            output, output_size, context->luma_mode_left,
            context_sizes->luma_mode_left) ||
        output_overlaps(
            output, output_size, context->coefficient_above,
            context_sizes->coefficient_above) ||
        output_overlaps(
            output, output_size, context->coefficient_left,
            context_sizes->coefficient_left) ||
        output_overlaps(
            output, output_size, context->colocated_luma,
            context_sizes->colocated_luma) ||
        output_overlaps(
            output, output_size, context->gdf_block_flags,
            context->gdf_block_flag_capacity) ||
        output_overlaps(
            output, output_size, context->cdef_unit_strengths,
            context->cdef_unit_strength_capacity) ||
        output_overlaps(
            output, output_size, workspace->reconstruction_scratch,
            sizeof(*workspace->reconstruction_scratch)) ||
        (result != 0 &&
         output_overlaps(output, output_size, result, sizeof(*result)))) {
        return 0;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (output_overlaps(
                output, output_size, context->ccso_unit_controls[plane],
                context->ccso_unit_control_capacity)) {
            return 0;
        }
    }
    return 1;
}

static void clear_decode_storage(
    const Av2hFilterWorkspaceLayout *layout,
    Av2hFrameDecodeWorkspace *workspace,
    const Av2hFrameContextSizes *context_sizes) {
    Av2hTileContextStorage *context = &workspace->tile_context;
    unsigned int plane;
    av2h_memory_fill(
        context->luma_partition_above, 0U, context->above_count);
    av2h_memory_fill(
        context->luma_partition_left, 0U, context->left_count);
    av2h_memory_fill(
        context->chroma_partition_above, 0U, context->above_count);
    av2h_memory_fill(
        context->chroma_partition_left, 0U, context->left_count);
    av2h_memory_fill(
        context->luma_mode_above, 0U,
        context_sizes->luma_mode_above);
    av2h_memory_fill(
        context->luma_mode_left, 0U,
        context_sizes->luma_mode_left);
    av2h_memory_fill(
        context->coefficient_above, 0U, context_sizes->coefficient_above);
    av2h_memory_fill(
        context->coefficient_left, 0U, context_sizes->coefficient_left);
    av2h_memory_fill(
        context->colocated_luma, 0U, context_sizes->colocated_luma);
    if (context->gdf_block_flags != 0) {
        av2h_memory_fill(
            context->gdf_block_flags, 0U,
            context->gdf_block_flag_capacity);
    }
    if (context->cdef_unit_strengths != 0) {
        av2h_memory_fill(
            context->cdef_unit_strengths, 0U,
            context->cdef_unit_strength_capacity);
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        Av2hReconstructionPlane *coded = &workspace->coded[plane];
        size_t data_size = coded->stride * coded->height;
        size_t ready_size = coded->reconstructed_stride *
            (((size_t)coded->height + 3U) / 4U);
        size_t map_size =
            workspace->filter_workspace->deblock_map.stride[plane] *
            workspace->filter_workspace->deblock_map.rows[plane] *
            sizeof(Av2hDeblockCell);
        size_t control_size =
            workspace->filter_workspace->ccso_controls.stride[plane] *
            layout->ccso_rows;
        av2h_memory_fill(coded->data, 0U, data_size);
        av2h_memory_fill(coded->reconstructed, 0U, ready_size);
        av2h_memory_fill(
            (void *)workspace->filter_workspace->deblock_map.plane[plane],
            0U, map_size);
        av2h_memory_fill(
            (void *)workspace->filter_workspace->ccso_controls.plane[plane],
            0U, control_size);
        if (context->ccso_unit_controls[plane] != 0) {
            av2h_memory_fill(
                context->ccso_unit_controls[plane], 0U,
                context->ccso_unit_control_capacity);
        }
    }
}

static int maps_and_readiness_complete(
    const Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hDeblockMap *map =
            &workspace->filter_workspace->deblock_map;
        const Av2hReconstructionPlane *coded = &workspace->coded[plane];
        size_t row;
        for (row = 0U; row < map->rows[plane]; ++row) {
            size_t column;
            for (column = 0U; column < map->columns[plane]; ++column) {
                const Av2hDeblockCell *cell =
                    &map->plane[plane][row * map->stride[plane] + column];
                if (cell->transform_width == 0U ||
                    cell->transform_height == 0U ||
                    coded->reconstructed[
                        row * coded->reconstructed_stride + column] == 0U) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

Av2hStatus av2h_copy_visible_i420_private(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size) {
    Av2hFilterWorkspaceLayout layout;
    size_t required_output;
    size_t offset = 0U;
    unsigned int plane;
    Av2hStatus status;
    if (frame == 0 || coded == 0 || coded_data_size == 0 || output == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_i420_output_size(frame, &required_output);
    if (status != AV2H_OK) return status;
    if (output_size < required_output) return AV2H_OUT_OF_MEMORY;
    status = av2h_filter_workspace_query(frame, &layout);
    if (status != AV2H_OK) return status;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hFilterPlane *source = &coded->plane[plane];
        size_t source_size;
        if (source->data == 0 ||
            source->width != layout.plane_width[plane] ||
            source->height != layout.plane_height[plane] ||
            source->stride < source->width ||
            !sized_plane_bytes(
                source->stride, source->height, &source_size) ||
            source_size > coded_data_size[plane] ||
            output_overlaps(
                output, required_output, source->data, source_size)) {
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        const Av2hFilterPlane *source = &coded->plane[plane];
        size_t width = plane == 0U ?
            frame->width : ((size_t)frame->width + 1U) >> 1U;
        size_t height = plane == 0U ?
            frame->height : ((size_t)frame->height + 1U) >> 1U;
        size_t row;
        for (row = 0U; row < height; ++row) {
            av2h_memory_copy(
                output + offset,
                source->data + row * source->stride,
                width);
            offset += width;
        }
    }
    return AV2H_OK;
}

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
    Av2hFilterPlanes final_planes;
    Av2hFrameDecodeResult decoded = { 0 };
    Av2hFrameContextSizes context_sizes;
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
    status = av2h_filter_workspace_query(frame, &layout);
    if (status != AV2H_OK) return status;
    if (!filter_workspace_valid(&layout, workspace->filter_workspace) ||
        !decode_planes_valid(frame, &layout, workspace) ||
        !tile_context_valid(
            frame, &workspace->tile_context, &context_sizes) ||
        !output_is_disjoint(
            output, required_output, &layout, workspace,
            &context_sizes, result)) {
        return AV2H_INVALID_ARGUMENT;
    }
    clear_decode_storage(&layout, workspace, &context_sizes);
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
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_OVERFLOW : status;
                decoded.mode = (uint8_t)leaf.mode.mode;
                decoded.angle_delta = leaf.mode.angle_delta;
                decoded.mrl_index = leaf.mode.mrl_index;
                decoded.multi_line_mrl = leaf.mode.multi_line_mrl;
                decoded.enable_ibp = leaf.mode.enable_ibp;
                decoded.use_intrabc = leaf.mode.use_intrabc;
                decoded.fsc_mode = leaf.mode.fsc_mode;
                decoded.skip_transform = leaf.mode.skip_transform;
                decoded.palette_size = leaf.mode.palette_size;
                decoded.tx_size = (uint8_t)leaf.prefix.tx_size;
                decoded.inverse_transform_stage =
                    av2h_debug_inverse_transform_stage;
                decoded.primary_type = (uint8_t)leaf.transform.primary_type;
                decoded.primary_symbol = leaf.transform.primary_symbol;
                decoded.primary_set_symbol =
                    leaf.transform.primary_long_side_symbol;
                decoded.secondary_symbol = leaf.transform.secondary_symbol;
                decoded.secondary_set = leaf.transform.secondary_set;
                if (av2h_debug_luma_tile_stage == 1U) {
                    decoded.syntax_stage = av2h_debug_luma_mode_stage;
                }
                if (av2h_debug_luma_tile_stage == 3U) {
                    decoded.syntax_stage = av2h_debug_transform_stage;
                }
                decoded.eob = leaf.prefix.eob;
                decoded.prefix_stage = av2h_debug_luma_prefix_stage;
                decoded.txb_skip_context = leaf.prefix.txb_skip_context;
                decoded.all_zero = leaf.prefix.all_zero;
                decoded.eob_point = leaf.prefix.eob_point;
                decoded.after_all_zero_tell =
                    leaf.prefix.after_all_zero_bits;
                decoded.after_eob_point_tell =
                    leaf.prefix.after_eob_point_bits;
                decoded.eob_range = av2h_debug_eob_range;
                decoded.skip_range = av2h_debug_skip_range;
                decoded.before_prefix_range =
                    av2h_debug_luma_stage_ranges[1];
                decoded.eob_tx_size = av2h_debug_eob_tx;
                decoded.eob_class = av2h_debug_eob_class;
                decoded.coefficient_position = block.diagnostic_position;
                decoded.coefficient_context = block.diagnostic_context;
                decoded.coefficient_kind = block.diagnostic_kind;
                AV2H_DECODE_FAIL(
                    returned, status, AV2H_FRAME_DECODE_STAGE_LUMA_PARSE,
                    0U, &leaf.node, decoded.syntax_stage != 0U ?
                        decoded.syntax_stage : av2h_debug_luma_tile_stage);
            }
            if (!has_leaf) continue;
            status = av2h_luma_reconstruct_to_plane(
                &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                &block, &workspace->coded[0],
                workspace->reconstruction_scratch);
            if (status != AV2H_OK) {
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_OVERFLOW : status;
                decoded.mode = (uint8_t)leaf.mode.mode;
                decoded.angle_delta = leaf.mode.angle_delta;
                decoded.mrl_index = leaf.mode.mrl_index;
                decoded.multi_line_mrl = leaf.mode.multi_line_mrl;
                decoded.enable_ibp = leaf.mode.enable_ibp;
                decoded.use_intrabc = leaf.mode.use_intrabc;
                decoded.fsc_mode = leaf.mode.fsc_mode;
                decoded.skip_transform = leaf.mode.skip_transform;
                decoded.tx_size = (uint8_t)leaf.prefix.tx_size;
                decoded.inverse_transform_stage =
                    av2h_debug_inverse_transform_stage;
                decoded.primary_type = (uint8_t)leaf.transform.primary_type;
                decoded.primary_symbol = leaf.transform.primary_symbol;
                decoded.secondary_symbol = leaf.transform.secondary_symbol;
                decoded.secondary_set = leaf.transform.secondary_set;
                decoded.eob = leaf.prefix.eob;
                AV2H_DECODE_FAIL(
                    returned, status,
                    AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT,
                    0U, &leaf.node, av2h_debug_luma_reconstruct_stage);
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
                unsigned int diagnostic_index =
                    av2h_debug_chroma_tile_stage >= 4U ? 1U : 0U;
                Av2hStatus returned = status == AV2H_UNSUPPORTED ?
                    AV2H_INVALID_ARGUMENT : status;
                decoded.mode = (uint8_t)leaf.mode.mode;
                decoded.angle_delta = leaf.mode.angle_delta;
                decoded.syntax_stage = av2h_debug_chroma_mode_stage;
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
                    av2h_debug_chroma_parse_reason;
                AV2H_DECODE_FAIL(
                    returned, status, AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE,
                    av2h_debug_chroma_tile_stage >= 4U ? 2U : 1U,
                    &leaf.node, av2h_debug_chroma_tile_stage != 0U ?
                        av2h_debug_chroma_tile_stage :
                        av2h_debug_chroma_mode_stage);
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
                    decoded.mhccp_reason = av2h_debug_mhccp_reason;
                    decoded.eob = leaf.prefix[index].eob;
                    AV2H_DECODE_FAIL(
                        returned, status,
                        AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
                        plane, &leaf.node,
                        av2h_debug_chroma_reconstruct_stage);
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
    if (!maps_and_readiness_complete(workspace)) {
        AV2H_DECODE_FAIL(
            AV2H_INVALID_DATA, AV2H_INVALID_DATA,
            AV2H_FRAME_DECODE_STAGE_COMPLETENESS, 0U, 0, 0U);
    }
    decoded.entropy_tell = av2h_range_tell(&decoder.range);
    workspace->filter_workspace->cdef_controls.strengths =
        workspace->tile_context.cdef_unit_strengths;
    workspace->filter_workspace->cdef_controls.columns =
        ((size_t)frame->width + 63U) / 64U;
    workspace->filter_workspace->cdef_controls.rows =
        ((size_t)frame->height + 63U) / 64U;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (workspace->tile_context.ccso_unit_controls[plane] != 0) {
            av2h_memory_copy(
                (void *)workspace->filter_workspace->ccso_controls.plane[plane],
                workspace->tile_context.ccso_unit_controls[plane],
                layout.ccso_control_count);
        }
        final_planes.plane[plane] = (Av2hFilterPlane){
            workspace->coded[plane].data,
            workspace->coded[plane].stride,
            workspace->coded[plane].width,
            workspace->coded[plane].height
        };
    }
    status = av2h_filter_frame(
        &final_planes, sequence, frame, workspace->filter_workspace);
    if (status != AV2H_OK) {
        Av2hStatus returned = status == AV2H_UNSUPPORTED ?
            AV2H_OUT_OF_MEMORY : status;
        decoded.filter_reason = av2h_debug_filter_reason;
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
