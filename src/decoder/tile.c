#include "tile.h"
#include "tx_geometry.h"

uint32_t av2h_debug_luma_stage_ranges[5U];
uint8_t av2h_debug_luma_tile_stage;
uint8_t av2h_debug_chroma_colocated_mode;
int8_t av2h_debug_chroma_colocated_delta;
uint8_t av2h_debug_chroma_tile_stage;
uint8_t av2h_debug_chroma_parse_reason;
Av2hDebugLeafTraceEntry av2h_debug_luma_leaf_trace[
    AV2H_DEBUG_LEAF_TRACE_CAPACITY];
Av2hDebugLeafTraceEntry av2h_debug_chroma_leaf_trace[
    AV2H_DEBUG_LEAF_TRACE_CAPACITY];
size_t av2h_debug_luma_leaf_trace_count;
size_t av2h_debug_chroma_leaf_trace_count;
int32_t av2h_debug_intrabc_owners[32U];

static void tile_debug_trace_leaf(Av2hDebugLeafTraceEntry *trace,
                                  size_t *count,
                                  Av2hPartitionNode node,
                                  uint8_t mode,
                                  int8_t angle_delta,
                                  uint8_t mode_set,
                                  uint8_t mode_index,
                                  size_t before_mode_bits,
                                  size_t after_mode_index_bits,
                                  const Av2hRangeDecoder *decoder) {
    Av2hDebugLeafTraceEntry *entry =
        &trace[*count % AV2H_DEBUG_LEAF_TRACE_CAPACITY];
    entry->node = node;
    entry->after_bits = av2h_range_tell(decoder);
    entry->mode = mode;
    entry->angle_delta = angle_delta;
    entry->mode_set = mode_set;
    entry->mode_index = mode_index;
    entry->before_mode_bits = before_mode_bits;
    entry->after_mode_index_bits = after_mode_index_bits;
    ++*count;
}

Av2hStatus av2h_tile_group_single_parse(Av2hSpan payload,
                                         size_t uncompressed_header_bits,
                                         Av2hTileGroup *group) {
    Av2hBitReader reader;
    size_t header_bits;
    size_t header_size;
    if (group == 0 || (payload.data == 0 && payload.size != 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    group->header_bits = 0U;
    group->header_size = 0U;
    group->entropy.data = 0;
    group->entropy.size = 0U;
    if (payload.size == 0U) return AV2H_TRUNCATED;
    if (!av2h_size_add(uncompressed_header_bits, 1U, &header_bits)) {
        return AV2H_OVERFLOW;
    }
    av2h_bit_reader_init(&reader, payload.data, payload.size, 0U);
    if (av2h_bit_reader_read(&reader, 1U) == 0U) return AV2H_UNSUPPORTED;
    (void)av2h_bit_reader_skip(&reader, uncompressed_header_bits);
    if (reader.status != AV2H_OK) return reader.status;
    if (av2h_bit_reader_align_zero(&reader) != AV2H_OK) {
        return reader.status;
    }
    header_size = reader.bit_position >> 3U;
    if (header_size >= payload.size) return AV2H_TRUNCATED;
    group->header_bits = header_bits;
    group->header_size = header_size;
    group->entropy.data = payload.data + header_size;
    group->entropy.size = payload.size - header_size;
    return AV2H_OK;
}

static Av2hStatus tile_decoder_fail(Av2hTileDecoder *decoder,
                                    Av2hStatus status) {
    if (decoder != 0 && status != AV2H_OK) decoder->status = status;
    return status;
}

static Av2hStatus tile_superblock_geometry(
    const Av2hSequenceState *sequence,
    uint16_t *superblock_mi,
    Av2hBlockSize *root_block_size) {
    if (sequence == 0 || superblock_mi == 0 || root_block_size == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->superblock_size == 0U ||
        sequence->superblock_size == 64U) {
        *superblock_mi = 16U;
        *root_block_size = AV2H_BLOCK_64X64;
        return AV2H_OK;
    }
    if (sequence->superblock_size == 128U) {
        *superblock_mi = 32U;
        *root_block_size = AV2H_BLOCK_128X128;
        return AV2H_OK;
    }
    return AV2H_UNSUPPORTED;
}

static Av2hStatus tile_reset_traversal_roots(Av2hTileDecoder *decoder) {
    Av2hPartitionNode root;
    Av2hStatus status;
    if (decoder == 0) return AV2H_INVALID_ARGUMENT;
    root.row = decoder->sb_row;
    root.column = decoder->sb_column;
    root.block_size = decoder->root_block_size;
    decoder->luma_root_partition = AV2H_PARTITION_INVALID;
    decoder->chroma_root_partition = AV2H_PARTITION_INVALID;
    decoder->chroma_root_forced_partition = AV2H_PARTITION_INVALID;
    decoder->luma_root_child_count = 0U;
    decoder->luma_root_children_share = 1U;
    status = av2h_partition_traversal_reset_root(
        &decoder->luma_traversal, root);
    if (status != AV2H_OK) return status;
    status = av2h_partition_traversal_reset_root(
        &decoder->chroma_traversal, root);
    if (status != AV2H_OK) return status;
    return AV2H_OK;
}

static void tile_capture_root_partition(
    Av2hTileDecoder *decoder,
    const Av2hFirstLeafPath *path,
    Av2hPartitionType *root_partition) {
    size_t index;
    if (*root_partition != AV2H_PARTITION_INVALID) return;
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        if (decision->node.row == decoder->sb_row &&
            decision->node.column == decoder->sb_column &&
            decision->node.block_size == decoder->root_block_size) {
            *root_partition = decision->partition;
            return;
        }
    }
}

static uint8_t tile_partition_direction(Av2hPartitionType partition) {
    if (partition == AV2H_PARTITION_HORZ ||
        partition == AV2H_PARTITION_HORZ_3 ||
        partition == AV2H_PARTITION_HORZ_4A ||
        partition == AV2H_PARTITION_HORZ_4B) {
        return 1U;
    }
    if (partition == AV2H_PARTITION_VERT ||
        partition == AV2H_PARTITION_VERT_3 ||
        partition == AV2H_PARTITION_VERT_4A ||
        partition == AV2H_PARTITION_VERT_4B) {
        return 2U;
    }
    return 0U;
}

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
            tile_partition_direction(decision->partition) ==
                tile_partition_direction(decoder->luma_root_partition)) {
            decoder->luma_root_children_share = 0U;
        }
    }
}

static uint8_t tile_cfl_allowed_for_sdp(const Av2hTileDecoder *decoder) {
    uint8_t luma_direction;
    uint8_t chroma_direction;
    if (!decoder->sequence->enable_sdp) return 1U;
    if (decoder->chroma_root_partition == AV2H_PARTITION_NONE) return 1U;
    if (decoder->luma_root_partition == decoder->chroma_root_partition) {
        return 1U;
    }
    luma_direction = tile_partition_direction(decoder->luma_root_partition);
    chroma_direction = tile_partition_direction(
        decoder->chroma_root_partition);
    return (uint8_t)(luma_direction != 0U &&
                     luma_direction == chroma_direction);
}

static int ccso_entropy_complete(const Av2hCcsoEntropyState *state) {
    size_t plane;
    size_t index;
    if (state == 0) return 0;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (state->unit_controls[plane] == 0) return 0;
        for (index = 0U; index < state->unit_count; ++index) {
            if (state->unit_controls[plane][index] ==
                AV2H_CCSO_UNTRANSMITTED) {
                return 0;
            }
        }
    }
    return 1;
}

static int q_luma_geometry_supported(const Av2hFrameState *frame,
                                     const Av2hPartitionNode *leaf) {
    if (frame->base_q_index == 128U) return 1;
    if (frame->base_q_index == 208U &&
        frame->width == 192U && frame->height == 96U) {
        switch (leaf->block_size) {
            case AV2H_BLOCK_16X16:
            case AV2H_BLOCK_16X64:
            case AV2H_BLOCK_32X8:
            case AV2H_BLOCK_32X16:
            case AV2H_BLOCK_32X32:
            case AV2H_BLOCK_32X64:
            case AV2H_BLOCK_64X16:
            case AV2H_BLOCK_64X32:
                return 1;
            default:
                return 0;
        }
    }
    switch (leaf->block_size) {
        case AV2H_BLOCK_4X4:
        case AV2H_BLOCK_4X8:
        case AV2H_BLOCK_8X4:
        case AV2H_BLOCK_4X16:
        case AV2H_BLOCK_8X8:
        case AV2H_BLOCK_16X8:
        case AV2H_BLOCK_16X4:
        case AV2H_BLOCK_16X16:
        case AV2H_BLOCK_32X4:
        case AV2H_BLOCK_32X8:
        case AV2H_BLOCK_32X16:
        case AV2H_BLOCK_8X16:
        case AV2H_BLOCK_8X32:
        case AV2H_BLOCK_16X32:
        case AV2H_BLOCK_32X32:
        case AV2H_BLOCK_32X64:
        case AV2H_BLOCK_16X64:
        case AV2H_BLOCK_64X32:
        case AV2H_BLOCK_64X64:
        case AV2H_BLOCK_64X16:
        case AV2H_BLOCK_64X8:
        case AV2H_BLOCK_8X64:
        case AV2H_BLOCK_4X32:
            return 1;
        default:
            return 0;
    }
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
            {
                static const uint16_t debug_positions[8U][2U] = {
                    { 19U, 16U }, { 17U, 17U }, { 18U, 16U }, { 20U, 16U },
                    { 17U, 18U }, { 17U, 16U }, { 19U, 14U }, { 18U, 14U }
                };
                size_t debug_index;
                for (debug_index = 0U; debug_index < 8U; ++debug_index) {
                    if ((size_t)node->row + row ==
                            debug_positions[debug_index][0] &&
                        (size_t)node->column + column ==
                            debug_positions[debug_index][1]) {
                        av2h_debug_intrabc_owners[debug_index * 4U] = node->row;
                        av2h_debug_intrabc_owners[debug_index * 4U + 1U] =
                            node->column;
                        av2h_debug_intrabc_owners[debug_index * 4U + 2U] =
                            node->block_size;
                        av2h_debug_intrabc_owners[debug_index * 4U + 3U] =
                            mode->use_intrabc;
                    }
                }
            }
        }
    }
    return AV2H_OK;
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

Av2hStatus av2h_tile_decoder_init(
    Av2hTileDecoder *decoder,
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hTileContextStorage *storage) {
    size_t cell_count;
    size_t sb_columns;
    size_t sb_rows;
    size_t index;
    Av2hStatus status;
    if (decoder == 0 || sequence == 0 || frame == 0 || storage == 0 ||
        entropy.data == 0 || entropy.size == 0U || frame->width == 0U ||
        frame->height == 0U || storage->above_count == 0U ||
        storage->left_count == 0U || storage->luma_partition_above == 0 ||
        storage->luma_partition_left == 0 ||
        storage->chroma_partition_above == 0 ||
        storage->chroma_partition_left == 0 ||
        storage->luma_mode_above == 0 || storage->luma_mode_left == 0 ||
        storage->coefficient_above == 0 || storage->coefficient_left == 0 ||
        storage->colocated_luma == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_debug_partition_trace_count = 0U;
    av2h_debug_luma_leaf_trace_count = 0U;
    av2h_debug_chroma_leaf_trace_count = 0U;
    decoder->mi_columns = av2h_frame_mi_dimension(frame->width);
    decoder->mi_rows = av2h_frame_mi_dimension(frame->height);
    if (decoder->mi_columns > storage->above_count ||
        decoder->mi_rows > storage->left_count ||
        !av2h_size_multiply(decoder->mi_columns, decoder->mi_rows,
                            &cell_count) ||
        cell_count > storage->colocated_luma_count ||
        decoder->mi_columns > 65535U || decoder->mi_rows > 65535U) {
        return AV2H_OUT_OF_MEMORY;
    }
    decoder->entropy = entropy;
    decoder->sequence = sequence;
    decoder->frame = frame;
    decoder->colocated_luma = storage->colocated_luma;
    decoder->colocated_luma_count = storage->colocated_luma_count;
    status = tile_superblock_geometry(
        sequence, &decoder->superblock_mi, &decoder->root_block_size);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    sb_columns =
        (decoder->mi_columns + decoder->superblock_mi - 1U) /
        decoder->superblock_mi;
    sb_rows =
        (decoder->mi_rows + decoder->superblock_mi - 1U) /
        decoder->superblock_mi;
    if (!av2h_size_multiply(sb_columns, sb_rows, &decoder->sb_count)) {
        return tile_decoder_fail(decoder, AV2H_OVERFLOW);
    }
    if (!sequence->enable_sdp) decoder->sb_count = 1U;
    decoder->sb_row = 0U;
    decoder->sb_column = 0U;
    decoder->sb_index = 0U;
    decoder->phase = AV2H_TILE_DECODE_LUMA;
    decoder->event = AV2H_TILE_EVENT_NONE;
    decoder->status = AV2H_OK;
    for (index = 0U; index < cell_count; ++index) {
        decoder->colocated_luma[index].context =
            (Av2hLumaModeContext){ 0 };
        decoder->colocated_luma[index].mode = (uint8_t)AV2H_INTRA_DC;
        decoder->colocated_luma[index].angle_delta = 0;
        decoder->colocated_luma[index].chroma_cfl = 0U;
    }
    status = av2h_partition_cdfs_init(&decoder->partition_cdfs);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_leaf_cdfs_init(&decoder->leaf_cdfs);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_gdf_entropy_state_init(
        &decoder->gdf_entropy, sequence, frame, storage->gdf_block_flags,
        storage->gdf_block_flag_capacity);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_cdef_entropy_state_init(
        &decoder->cdef_entropy, sequence, frame,
        storage->cdef_unit_strengths,
        storage->cdef_unit_strength_capacity);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_ccso_entropy_state_init(
        &decoder->ccso_entropy, sequence, frame,
        storage->ccso_unit_controls,
        storage->ccso_unit_control_capacity);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_first_coeff_cdfs_init_for_q(
        &decoder->coeff_cdfs, frame->base_q_index);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_first_transform_cdfs_init(&decoder->transform_cdfs);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_first_coeff_level_cdfs_init_for_q(
        &decoder->level_cdfs, frame->base_q_index);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_partition_context_state_init(
        &decoder->partition_contexts, storage->luma_partition_above,
        storage->luma_partition_left, storage->chroma_partition_above,
        storage->chroma_partition_left, storage->above_count,
        storage->left_count);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_luma_mode_context_state_init(
        &decoder->luma_mode_contexts, storage->luma_mode_above,
        storage->above_count, storage->luma_mode_left, storage->left_count);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    decoder->luma_mode_contexts.above_count = decoder->mi_columns;
    decoder->luma_mode_contexts.superblock_mi =
        (uint16_t)(sequence->superblock_size / 4U);
    decoder->luma_mode_contexts.spatial =
        (const uint8_t *)decoder->colocated_luma;
    decoder->luma_mode_contexts.spatial_stride =
        sizeof(*decoder->colocated_luma);
    decoder->luma_mode_contexts.spatial_columns = decoder->mi_columns;
    decoder->luma_mode_contexts.spatial_rows = decoder->mi_rows;
    status = av2h_coeff_context_state_init(
        &decoder->coefficient_contexts, storage->coefficient_above,
        storage->above_count, storage->coefficient_left,
        storage->left_count);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (decoder->mi_rows >
        (size_t)decoder->luma_mode_contexts.superblock_mi * 2U) {
        decoder->coefficient_contexts.above_count = decoder->mi_columns;
        decoder->coefficient_contexts.left_count = decoder->mi_rows;
    }
    decoder->coefficient_contexts.mi_columns = decoder->mi_columns;
    decoder->coefficient_contexts.mi_rows = decoder->mi_rows;
    status = av2h_partition_traversal_init(
        &decoder->luma_traversal, (uint16_t)decoder->mi_rows,
        (uint16_t)decoder->mi_columns,
        (Av2hPartitionNode){ 0U, 0U, decoder->root_block_size }, 0U,
        sequence->enable_ext_partitions,
        sequence->enable_uneven_4way_partitions);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_partition_traversal_init(
        &decoder->chroma_traversal, (uint16_t)decoder->mi_rows,
        (uint16_t)decoder->mi_columns,
        (Av2hPartitionNode){ 0U, 0U, decoder->root_block_size }, 1U,
        sequence->enable_ext_partitions,
        sequence->enable_uneven_4way_partitions);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = tile_reset_traversal_roots(decoder);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_range_init(&decoder->range, &decoder->entropy, 1U);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
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
        return tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    av2h_debug_luma_tile_stage = 0U;
    status = av2h_partition_traversal_next(
        &decoder->luma_traversal, &decoder->range,
        &decoder->partition_cdfs, &decoder->partition_contexts, &path,
        has_leaf);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (!*has_leaf) {
        return av2h_tile_decoder_finish_root(decoder);
    }
    tile_capture_root_partition(
        decoder, &path, &decoder->luma_root_partition);
    tile_capture_luma_root_child(decoder, &path);
    leaf->node = path.leaf;
    if (av2h_debug_chimp_second_sb[0] == 0 && leaf->node.column >= 16U) {
        const Av2hPartitionDecision *last_decision =
            &path.decisions[path.decision_count - 1U];
        av2h_debug_chimp_second_sb[0] = 1;
        av2h_debug_chimp_second_sb[1] = leaf->node.row;
        av2h_debug_chimp_second_sb[2] = leaf->node.column;
        av2h_debug_chimp_second_sb[3] = leaf->node.block_size;
        av2h_debug_chimp_second_sb[4] = av2h_range_tell(&decoder->range);
        av2h_debug_chimp_second_sb[5] = decoder->range.range;
        av2h_debug_chimp_second_sb[29] = path.decision_count;
        av2h_debug_chimp_second_sb[30] = last_decision->partition;
        av2h_debug_chimp_second_sb[31] = last_decision->forced;
        av2h_debug_chimp_second_sb[32] = last_decision->after_bits;
    }
    av2h_debug_luma_stage_ranges[0] = decoder->range.range;
    if (decoder->frame->gdf_mode < 2U &&
        !q_luma_geometry_supported(decoder->frame, &leaf->node)) {
        return tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
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
    av2h_debug_luma_tile_stage = 1U;
    status = av2h_luma_mode_read_with_state_and_gdf(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &decoder->luma_mode_contexts,
        &decoder->gdf_entropy, &decoder->cdef_entropy,
        &decoder->ccso_entropy, &leaf->mode);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (leaf->node.row == 16U && leaf->node.column == 0U) {
        av2h_debug_chimp_second_sb[29] = leaf->mode.before_gdf_bits;
        av2h_debug_chimp_second_sb[30] = leaf->mode.before_mode_bits;
        av2h_debug_chimp_second_sb[31] = leaf->mode.after_mode_index_bits;
        av2h_debug_chimp_second_sb[32] = leaf->mode.after_fsc_bits;
        av2h_debug_chimp_second_sb[33] = leaf->mode.after_mrl_bits;
        av2h_debug_chimp_second_sb[34] = av2h_range_tell(&decoder->range);
        av2h_debug_chimp_second_sb[35] = leaf->mode.fsc_neighbor_context;
    }
    if (av2h_debug_chimp_second_sb[1] == leaf->node.row &&
        av2h_debug_chimp_second_sb[2] == leaf->node.column) {
        av2h_debug_chimp_second_sb[6] = av2h_range_tell(&decoder->range);
        av2h_debug_chimp_second_sb[7] = decoder->range.range;
        av2h_debug_chimp_second_sb[8] = leaf->mode.mode;
        av2h_debug_chimp_second_sb[17] = leaf->mode.before_mode_bits;
        av2h_debug_chimp_second_sb[18] = av2h_debug_y_mode_range;
        for (size_t debug_index = 0U; debug_index < 9U; ++debug_index) {
            av2h_debug_chimp_second_sb[20U + debug_index] =
                av2h_debug_y_mode_index_cdf[debug_index];
        }
    }
    status = av2h_luma_palette_map_read(
        &decoder->range, &decoder->leaf_cdfs, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (av2h_debug_chimp_second_sb[1] == leaf->node.row &&
        av2h_debug_chimp_second_sb[2] == leaf->node.column) {
        av2h_debug_chimp_second_sb[9] = av2h_range_tell(&decoder->range);
        av2h_debug_chimp_second_sb[10] = decoder->range.range;
    }
    av2h_debug_luma_stage_ranges[1] = decoder->range.range;
    status = av2h_luma_mode_context_state_update(
        &decoder->luma_mode_contexts, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return tile_decoder_fail(decoder, AV2H_OVERFLOW);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (av2h_debug_chimp_second_sb[1] == leaf->node.row &&
        av2h_debug_chimp_second_sb[2] == leaf->node.column) {
        av2h_debug_chimp_second_sb[11] = av2h_range_tell(&decoder->range);
        av2h_debug_chimp_second_sb[12] = decoder->range.range;
        av2h_debug_chimp_second_sb[13] = leaf->prefix.eob;
    }
    status = tile_store_luma_mode(decoder, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return tile_decoder_fail(decoder, AV2H_OUT_OF_MEMORY);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (av2h_debug_chimp_second_sb[1] == leaf->node.row &&
        av2h_debug_chimp_second_sb[2] == leaf->node.column) {
        av2h_debug_chimp_second_sb[14] = av2h_range_tell(&decoder->range);
        av2h_debug_chimp_second_sb[15] = decoder->range.range;
        av2h_debug_chimp_second_sb[16] = leaf->transform.primary_type;
    }
    if (leaf->mode.skip_transform != 0U) {
        status = tile_initialize_skipped_intrabc(
            decoder, leaf, &leaf->prefix, &leaf->transform, block);
        if (status == AV2H_UNSUPPORTED) {
            return tile_decoder_fail(decoder, AV2H_INVALID_DATA);
        }
        if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
        tile_debug_trace_leaf(
            av2h_debug_luma_leaf_trace,
            &av2h_debug_luma_leaf_trace_count, leaf->node,
            (uint8_t)leaf->mode.mode, leaf->mode.angle_delta,
            leaf->mode.mode_set, leaf->mode.mode_index,
            leaf->mode.before_mode_bits, leaf->mode.after_mode_index_bits,
            &decoder->range);
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_debug_luma_tile_stage = 2U;
    status = av2h_coeff_prefix_read_with_state(
        &decoder->range, &decoder->coeff_cdfs, decoder->frame, &leaf->node,
        &leaf->mode, &decoder->coefficient_contexts, &leaf->prefix,
        &dc_sign_context);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (leaf->node.row == 16U && leaf->node.column == 0U) {
        av2h_debug_chimp_second_sb[36] = leaf->prefix.before_all_zero_bits;
        av2h_debug_chimp_second_sb[37] = leaf->prefix.after_all_zero_bits;
        av2h_debug_chimp_second_sb[38] = leaf->prefix.after_eob_point_bits;
        av2h_debug_chimp_second_sb[39] = leaf->prefix.after_eob_bits;
    }
    av2h_debug_luma_stage_ranges[2] = decoder->range.range;
    if (leaf->prefix.all_zero) {
        status = tile_initialize_all_zero_luma(
            decoder, leaf, &leaf->transform, block);
        if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
        tile_debug_trace_leaf(
            av2h_debug_luma_leaf_trace,
            &av2h_debug_luma_leaf_trace_count, leaf->node,
            (uint8_t)leaf->mode.mode, leaf->mode.angle_delta,
            leaf->mode.mode_set, leaf->mode.mode_index,
            leaf->mode.before_mode_bits, leaf->mode.after_mode_index_bits,
            &decoder->range);
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_debug_luma_tile_stage = 3U;
    status = av2h_first_transform_type_read(
        &decoder->range, &decoder->transform_cdfs, decoder->sequence,
        decoder->frame, &leaf->mode, &leaf->prefix, &leaf->transform);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    av2h_debug_luma_stage_ranges[3] = decoder->range.range;
    av2h_debug_luma_tile_stage = 4U;
    status = av2h_coeff_block_read_with_state(
        &decoder->range, &decoder->level_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &leaf->mode, &leaf->prefix,
        &leaf->transform, &decoder->coefficient_contexts, block);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    av2h_debug_luma_stage_ranges[4] = decoder->range.range;
    tile_debug_trace_leaf(
        av2h_debug_luma_leaf_trace, &av2h_debug_luma_leaf_trace_count,
        leaf->node, (uint8_t)leaf->mode.mode, leaf->mode.angle_delta,
        leaf->mode.mode_set, leaf->mode.mode_index,
        leaf->mode.before_mode_bits, leaf->mode.after_mode_index_bits,
        &decoder->range);
    decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
    return AV2H_OK;
}

int32_t av2h_debug_chimp_second_sb[40U];

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
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
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
        return tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    av2h_debug_chroma_tile_stage = 0U;
    av2h_debug_chroma_parse_reason = 1U;
    if (decoder->chroma_root_forced_partition != AV2H_PARTITION_INVALID) {
        Av2hPartitionNode root = {
            decoder->sb_row, decoder->sb_column, decoder->root_block_size
        };
        if (decoder->chroma_traversal.stack_count != 1U ||
            decoder->chroma_traversal.has_pending_leaf ||
            decoder->chroma_traversal.stack[0].row != root.row ||
            decoder->chroma_traversal.stack[0].column != root.column ||
            decoder->chroma_traversal.stack[0].block_size !=
                root.block_size) {
            return tile_decoder_fail(decoder, AV2H_INVALID_DATA);
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
                    AV2H_PARTITION_HORZ) {
                first.block_size = decoder->root_block_size ==
                    AV2H_BLOCK_64X64 ? AV2H_BLOCK_64X32 :
                    AV2H_BLOCK_128X64;
                second = first;
                second.row = (uint16_t)(second.row +
                    decoder->superblock_mi / 2U);
            } else if (decoder->chroma_root_forced_partition ==
                       AV2H_PARTITION_VERT) {
                first.block_size = decoder->root_block_size ==
                    AV2H_BLOCK_64X64 ? AV2H_BLOCK_32X64 :
                    AV2H_BLOCK_64X128;
                second = first;
                second.column = (uint16_t)(second.column +
                    decoder->superblock_mi / 2U);
            } else {
                return tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
            }
            decoder->chroma_traversal.stack[0] = second;
            decoder->chroma_traversal.stack[1] = first;
            decoder->chroma_traversal.stack_count = 2U;
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
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    if (!*has_leaf) {
        return av2h_tile_decoder_finish_root(decoder);
    }
    av2h_debug_chroma_parse_reason = 2U;
    tile_capture_root_partition(
        decoder, &path, &decoder->chroma_root_partition);
    leaf->node = path.leaf;
    index = (size_t)leaf->node.row * decoder->mi_columns + leaf->node.column;
    if (index >= decoder->colocated_luma_count) {
        av2h_debug_chroma_parse_reason = 3U;
        return tile_decoder_fail(decoder, AV2H_OVERFLOW);
    }
    colocated = &decoder->colocated_luma[index];
    if (!colocated->context.present) {
        av2h_debug_chroma_parse_reason = 4U;
        return tile_decoder_fail(decoder, AV2H_INVALID_DATA);
    }
    av2h_debug_chroma_colocated_mode = colocated->mode;
    av2h_debug_chroma_colocated_delta = colocated->angle_delta;
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
    av2h_debug_chroma_parse_reason = 5U;
    status = av2h_chroma_mode_read_with_delta_for_sdp(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        (Av2hFirstLeafIntraMode)colocated->mode, colocated->angle_delta,
        leaf->node.block_size, cfl_context,
        tile_cfl_allowed_for_sdp(decoder), &leaf->mode);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    leaf->mode.above_smooth = leaf->node.row == 0U ? 0U :
        decoder->colocated_luma[
            ((size_t)leaf->node.row - 1U) * decoder->mi_columns +
            leaf->node.column].chroma_smooth;
    leaf->mode.left_smooth = leaf->node.column == 0U ? 0U :
        decoder->colocated_luma[
            (size_t)leaf->node.row * decoder->mi_columns +
            leaf->node.column - 1U].chroma_smooth;
    av2h_debug_chroma_parse_reason = 0U;
    status = tile_store_chroma_mode(decoder, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    blocks[0] = u_block;
    blocks[1] = v_block;
    for (index = 0U; index < 2U; ++index) {
        uint8_t plane = (uint8_t)(index + 1U);
        av2h_debug_chroma_tile_stage = (uint8_t)(1U + index * 3U);
        status = av2h_chroma_coeff_prefix_read_with_state(
            &decoder->range, &decoder->coeff_cdfs, decoder->frame,
            &leaf->node, plane, &decoder->coefficient_contexts,
            &leaf->prefix[index]);
        if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
        av2h_debug_chroma_tile_stage = (uint8_t)(2U + index * 3U);
        status = av2h_chroma_transform_type_read(
            &decoder->range, &decoder->transform_cdfs, decoder->frame,
            &leaf->mode, plane, &leaf->prefix[index],
            &leaf->transform[index]);
        if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
        av2h_debug_chroma_tile_stage = (uint8_t)(3U + index * 3U);
        status = av2h_chroma_coeff_block_read_with_state(
            &decoder->range, &decoder->level_cdfs, decoder->sequence,
            decoder->frame, &leaf->node, plane, &leaf->prefix[index],
            &decoder->coefficient_contexts, blocks[index]);
        if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    }
    tile_debug_trace_leaf(
        av2h_debug_chroma_leaf_trace, &av2h_debug_chroma_leaf_trace_count,
        leaf->node, (uint8_t)leaf->mode.mode, leaf->mode.angle_delta,
        0U, leaf->mode.mode_index,
        leaf->mode.before_cfl_bits, leaf->mode.after_mode_bits,
        &decoder->range);
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
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_chroma_reconstruct_to_plane_ex(
        &leaf->node, &leaf->mode, &leaf->prefix[0], &leaf->transform[0],
        &u_block, u_plane, 0, 1U, 0, scratch);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    status = av2h_chroma_reconstruct_to_plane_ex(
        &leaf->node, &leaf->mode, &leaf->prefix[1], &leaf->transform[1],
        &v_block, v_plane, 0, 2U, 0, scratch);
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}

Av2hStatus av2h_tile_decoder_finish_root(Av2hTileDecoder *decoder) {
    Av2hPartitionTraversal *traversal;
    Av2hStatus status;
    if (decoder == 0) return AV2H_INVALID_ARGUMENT;
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase == AV2H_TILE_DECODE_LUMA) {
        traversal = &decoder->luma_traversal;
    } else if (decoder->phase == AV2H_TILE_DECODE_CHROMA) {
        traversal = &decoder->chroma_traversal;
    } else {
        return tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (traversal->stack_count != 0U ||
        decoder->luma_traversal.has_pending_leaf ||
        decoder->chroma_traversal.has_pending_leaf) {
        return tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->phase == AV2H_TILE_DECODE_LUMA) {
        if (decoder->sb_index == 0U) {
            av2h_debug_chimp_second_sb[32] = av2h_range_tell(&decoder->range);
            av2h_debug_chimp_second_sb[33] = decoder->range.range;
        }
        if (decoder->sequence->enable_sdp) {
            if (decoder->luma_root_partition == AV2H_PARTITION_NONE) {
                decoder->chroma_root_forced_partition = AV2H_PARTITION_NONE;
            } else if (decoder->luma_root_child_count == 2U &&
                       decoder->luma_root_children_share &&
                       tile_partition_direction(
                           decoder->luma_root_partition) != 0U) {
                decoder->chroma_root_forced_partition =
                    decoder->luma_root_partition;
            }
        }
        decoder->phase = AV2H_TILE_DECODE_CHROMA;
        decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    if (decoder->sb_index + 1U < decoder->sb_count) {
        if (decoder->sb_index == 0U) {
            av2h_debug_chimp_second_sb[34] = av2h_range_tell(&decoder->range);
            av2h_debug_chimp_second_sb[35] = decoder->range.range;
        }
        ++decoder->sb_index;
        if ((size_t)decoder->sb_column + decoder->superblock_mi <
            decoder->mi_columns) {
            decoder->sb_column =
                (uint16_t)(decoder->sb_column + decoder->superblock_mi);
        } else {
            decoder->sb_column = 0U;
            decoder->sb_row =
                (uint16_t)(decoder->sb_row + decoder->superblock_mi);
        }
        status = tile_reset_traversal_roots(decoder);
        if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
        decoder->phase = AV2H_TILE_DECODE_LUMA;
        decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    if (decoder->frame->gdf_mode >= 2U &&
        decoder->gdf_entropy.decoded_count !=
            decoder->frame->gdf_block_count) {
        return tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
    }
    if (!ccso_entropy_complete(&decoder->ccso_entropy)) {
        return tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
    }
    decoder->phase = AV2H_TILE_DECODE_DONE;
    decoder->event = AV2H_TILE_EVENT_DONE;
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_commit_leaf(
    Av2hTileDecoder *decoder,
    Av2hPartitionNode leaf) {
    Av2hStatus status;
    if (decoder == 0) return AV2H_INVALID_ARGUMENT;
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase == AV2H_TILE_DECODE_LUMA) {
        status = av2h_partition_traversal_commit_leaf(
            &decoder->luma_traversal, &decoder->partition_contexts, leaf);
    } else if (decoder->phase == AV2H_TILE_DECODE_CHROMA) {
        status = av2h_partition_traversal_commit_leaf(
            &decoder->chroma_traversal, &decoder->partition_contexts, leaf);
    } else {
        status = AV2H_INVALID_ARGUMENT;
    }
    if (status != AV2H_OK) return tile_decoder_fail(decoder, status);
    return AV2H_OK;
}