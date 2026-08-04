#include "tile.h"
#include "internal/tile_modules.h"

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

void av2h_tile_debug_trace_leaf(Av2hDebugLeafTraceEntry *trace,
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

Av2hStatus av2h_tile_decoder_fail(Av2hTileDecoder *decoder,
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

void av2h_tile_capture_root_partition(
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

uint8_t av2h_tile_partition_direction(Av2hPartitionType partition) {
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
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    sb_columns =
        (decoder->mi_columns + decoder->superblock_mi - 1U) /
        decoder->superblock_mi;
    sb_rows =
        (decoder->mi_rows + decoder->superblock_mi - 1U) /
        decoder->superblock_mi;
    if (!av2h_size_multiply(sb_columns, sb_rows, &decoder->sb_count)) {
        return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
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
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_leaf_cdfs_init(&decoder->leaf_cdfs);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_gdf_entropy_state_init(
        &decoder->gdf_entropy, sequence, frame, storage->gdf_block_flags,
        storage->gdf_block_flag_capacity);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_cdef_entropy_state_init(
        &decoder->cdef_entropy, sequence, frame,
        storage->cdef_unit_strengths,
        storage->cdef_unit_strength_capacity);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_ccso_entropy_state_init(
        &decoder->ccso_entropy, sequence, frame,
        storage->ccso_unit_controls,
        storage->ccso_unit_control_capacity);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_first_coeff_cdfs_init_for_q(
        &decoder->coeff_cdfs, frame->base_q_index);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_first_transform_cdfs_init(&decoder->transform_cdfs);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_first_coeff_level_cdfs_init_for_q(
        &decoder->level_cdfs, frame->base_q_index);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_partition_context_state_init(
        &decoder->partition_contexts, storage->luma_partition_above,
        storage->luma_partition_left, storage->chroma_partition_above,
        storage->chroma_partition_left, storage->above_count,
        storage->left_count);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_luma_mode_context_state_init(
        &decoder->luma_mode_contexts, storage->luma_mode_above,
        storage->above_count, storage->luma_mode_left, storage->left_count);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
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
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
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
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_partition_traversal_init(
        &decoder->chroma_traversal, (uint16_t)decoder->mi_rows,
        (uint16_t)decoder->mi_columns,
        (Av2hPartitionNode){ 0U, 0U, decoder->root_block_size }, 1U,
        sequence->enable_ext_partitions,
        sequence->enable_uneven_4way_partitions);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = tile_reset_traversal_roots(decoder);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = av2h_range_init(&decoder->range, &decoder->entropy, 1U);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    return AV2H_OK;
}

int32_t av2h_debug_chimp_second_sb[40U];

Av2hStatus av2h_tile_decoder_finish_root(Av2hTileDecoder *decoder) {
    Av2hPartitionTraversal *traversal;
    Av2hTileFilterControls controls;
    Av2hStatus status;
    if (decoder == 0) return AV2H_INVALID_ARGUMENT;
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase == AV2H_TILE_DECODE_LUMA) {
        traversal = &decoder->luma_traversal;
    } else if (decoder->phase == AV2H_TILE_DECODE_CHROMA) {
        traversal = &decoder->chroma_traversal;
    } else {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (traversal->stack_count != 0U ||
        decoder->luma_traversal.has_pending_leaf ||
        decoder->chroma_traversal.has_pending_leaf) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
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
                       av2h_tile_partition_direction(
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
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        decoder->phase = AV2H_TILE_DECODE_LUMA;
        decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    status = av2h_tile_filter_controls_handoff(decoder, &controls);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
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
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    return AV2H_OK;
}