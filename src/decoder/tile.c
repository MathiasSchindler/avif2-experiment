#include "tile.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"
#include "tx_geometry.h"

static Av2hTileDiagnostics tile_diagnostics;

static void tile_reset_left_partition_contexts(Av2hTileDecoder *decoder) {
    size_t structure;
    size_t index;
    for (structure = 0U;
         structure < AV2H_PARTITION_STRUCTURE_COUNT; ++structure) {
        for (index = 0U;
             index < decoder->partition_contexts.left_count; ++index) {
            decoder->partition_contexts.left[structure][index] = 0U;
        }
    }
}

const Av2hTileDiagnostics *av2h_tile_diagnostics_get(void) {
    return &tile_diagnostics;
}

void av2h_tile_diagnostics_set_luma_range(size_t index, uint32_t value) {
    if (index < 5U) tile_diagnostics.luma_stage_ranges[index] = value;
}

void av2h_tile_diagnostics_set_luma_stage(uint8_t value) {
    tile_diagnostics.luma_tile_stage = value;
}

void av2h_tile_diagnostics_set_chroma_stage(uint8_t value) {
    tile_diagnostics.chroma_tile_stage = value;
}

void av2h_tile_diagnostics_set_chroma_reason(uint8_t value) {
    tile_diagnostics.chroma_parse_reason = value;
}

static uint8_t tile_luma_lossless_unit_complete(
    const Av2hTileDecoder *decoder,
    Av2hPartitionNode leaf) {
    Av2hPartitionNode next;
    if (decoder->luma_tx_index >= decoder->luma_tx_count) return 1U;
    if (av2h_luma_lossless_tx_node_get(
            &decoder->luma_coding_node, decoder->mi_columns,
            decoder->mi_rows, decoder->luma_tx_index, &next) != AV2H_OK) {
        return 0U;
    }
    return (uint8_t)(
        (leaf.row - decoder->luma_coding_node.row) / 16U !=
            (next.row - decoder->luma_coding_node.row) / 16U ||
        (leaf.column - decoder->luma_coding_node.column) / 16U !=
            (next.column - decoder->luma_coding_node.column) / 16U);
}

static uint8_t tile_chroma_lossless_unit_complete(
    const Av2hTileDecoder *decoder,
    Av2hPartitionNode leaf) {
    Av2hPartitionNode next;
    if (decoder->chroma_tx_index >= decoder->chroma_tx_count) return 1U;
    if (av2h_chroma_lossless_tx_node_get(
            &decoder->chroma_coding_node, decoder->mi_columns,
            decoder->mi_rows, decoder->frame->chroma_format_idc,
            decoder->chroma_tx_index, &next) != AV2H_OK) {
        return 0U;
    }
    return (uint8_t)(
        (leaf.row - decoder->chroma_coding_node.row) / 16U !=
            (next.row - decoder->chroma_coding_node.row) / 16U ||
        (leaf.column - decoder->chroma_coding_node.column) / 16U !=
            (next.column - decoder->chroma_coding_node.column) / 16U);
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
    decoder->square_child_luma_partition = AV2H_PARTITION_INVALID;
    decoder->luma_root_child_count = 0U;
    decoder->luma_root_children_share = 1U;
    decoder->square_child_luma_child_count = 0U;
    decoder->square_child_luma_children_share = 1U;
    decoder->square_root_child_index = 0U;
    decoder->luma_partition_node_count = 0U;
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
    uint16_t root_row = decoder->sb_row;
    uint16_t root_column = decoder->sb_column;
    Av2hBlockSize root_block_size = decoder->root_block_size;
    size_t index;
    if (*root_partition != AV2H_PARTITION_INVALID) return;
    if (decoder->phase == AV2H_TILE_DECODE_CHROMA &&
        decoder->sequence->enable_sdp &&
        decoder->root_block_size == AV2H_BLOCK_128X128 &&
        decoder->square_root_child_index < 4U) {
        root_row = (uint16_t)(root_row +
            (decoder->square_root_child_index >= 2U ? 16U : 0U));
        root_column = (uint16_t)(root_column +
            ((decoder->square_root_child_index & 1U) != 0U ? 16U : 0U));
        root_block_size = AV2H_BLOCK_64X64;
    }
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        if (decision->node.row == root_row &&
            decision->node.column == root_column &&
            decision->node.block_size == root_block_size) {
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
    decoder->chroma_tx_index = 0U;
    decoder->chroma_tx_count = 0U;
    decoder->chroma_tx_unit_start = 0U;
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
    if (!sequence->enable_sdp && !av2h_sequence_is_monochrome(sequence)) {
        decoder->sb_count = 1U;
    }
    decoder->sb_row = 0U;
    decoder->sb_column = 0U;
    decoder->current_q_index = frame->base_q_index;
    decoder->sb_index = 0U;
    decoder->phase = AV2H_TILE_DECODE_LUMA;
    decoder->event = AV2H_TILE_EVENT_NONE;
    decoder->status = AV2H_OK;
    decoder->luma_tx_index = 0U;
    decoder->luma_tx_count = 0U;
    decoder->shared_chroma_resume_luma = 0U;
    decoder->shared_chroma_complete = 0U;
    decoder->shared_chroma_mode_pending = 0U;
    decoder->shared_chroma_commit_luma = 0U;
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
        decoder->superblock_mi);
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
    decoder->luma_mode_contexts.spatial_stride_bytes =
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

static Av2hStatus tile_advance_root_or_finish(Av2hTileDecoder *decoder) {
    Av2hTileFilterControls controls;
    Av2hStatus status;
    if (decoder->sb_index + 1U < decoder->sb_count) {
        ++decoder->sb_index;
        if ((size_t)decoder->sb_column + decoder->superblock_mi <
            decoder->mi_columns) {
            decoder->sb_column =
                (uint16_t)(decoder->sb_column + decoder->superblock_mi);
        } else {
            decoder->sb_column = 0U;
            decoder->sb_row =
                (uint16_t)(decoder->sb_row + decoder->superblock_mi);
            tile_reset_left_partition_contexts(decoder);
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
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->phase == AV2H_TILE_DECODE_CHROMA &&
        decoder->shared_chroma_resume_luma &&
        decoder->chroma_traversal.stack_count == 0U &&
        !decoder->chroma_traversal.has_pending_leaf) {
        if (decoder->shared_chroma_commit_luma) {
            status = av2h_partition_traversal_commit_leaf(
                &decoder->luma_traversal, &decoder->partition_contexts,
                decoder->shared_chroma_node);
            if (status != AV2H_OK) {
                return av2h_tile_decoder_fail(decoder, status);
            }
            decoder->shared_chroma_commit_luma = 0U;
        }
        decoder->shared_chroma_resume_luma = 0U;
        decoder->shared_chroma_complete = 1U;
        decoder->phase = AV2H_TILE_DECODE_LUMA;
        decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    if (traversal->stack_count != 0U ||
        decoder->luma_traversal.has_pending_leaf ||
        decoder->chroma_traversal.has_pending_leaf) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->phase == AV2H_TILE_DECODE_LUMA) {
        if (av2h_frame_plane_count(decoder->frame) == 1U) {
            return tile_advance_root_or_finish(decoder);
        }
        if (decoder->shared_chroma_complete) {
            decoder->shared_chroma_complete = 0U;
            decoder->phase = AV2H_TILE_DECODE_CHROMA;
            return av2h_tile_decoder_finish_root(decoder);
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
    if (decoder->sequence->enable_sdp &&
        decoder->root_block_size == AV2H_BLOCK_128X128 &&
        decoder->square_root_child_index < 4U) {
        const Av2hPartitionNode *pending = 0;
        size_t pending_index = decoder->luma_traversal.stack_count;
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
            decoder->square_root_child_index = (uint8_t)(
                (pending->row >= decoder->sb_row + 16U ? 2U : 0U) +
                (pending->column >= decoder->sb_column + 16U ? 1U : 0U));
            decoder->square_child_luma_partition = AV2H_PARTITION_INVALID;
            decoder->phase = AV2H_TILE_DECODE_LUMA;
            decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
            return AV2H_OK;
        }
        decoder->square_root_child_index = 4U;
    }
    return tile_advance_root_or_finish(decoder);
}

Av2hStatus av2h_tile_decoder_commit_leaf(
    Av2hTileDecoder *decoder,
    Av2hPartitionNode leaf) {
    Av2hStatus status;
    if (decoder == 0) return AV2H_INVALID_ARGUMENT;
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase == AV2H_TILE_DECODE_LUMA) {
        if (decoder->luma_tx_count != 0U) {
            Av2hPartitionNode expected;
                status = decoder->frame->base_q_index == 0U &&
                    decoder->luma_coding_mode.lossless_tx_size_large == 0U ?
                av2h_luma_lossless_tx_node_get(
                    &decoder->luma_coding_node, decoder->mi_columns,
                    decoder->mi_rows, decoder->luma_tx_index, &expected) :
                av2h_luma_tx_node_get(
                    &decoder->luma_coding_node, decoder->luma_tx_index,
                    &expected);
            if (status != AV2H_OK || expected.row != leaf.row ||
                expected.column != leaf.column ||
                expected.block_size != leaf.block_size) {
                return av2h_tile_decoder_fail(
                    decoder, AV2H_INVALID_ARGUMENT);
            }
            ++decoder->luma_tx_index;
            if (decoder->luma_tx_index < decoder->luma_tx_count) {
                if (decoder->shared_chroma_resume_luma &&
                    decoder->chroma_tx_count != 0U &&
                    decoder->luma_coding_node.row ==
                        decoder->chroma_coding_node.row &&
                    decoder->luma_coding_node.column ==
                        decoder->chroma_coding_node.column &&
                    decoder->luma_coding_node.block_size ==
                        decoder->chroma_coding_node.block_size &&
                                        ((decoder->frame->base_q_index == 0U &&
                                            decoder->luma_coding_mode.lossless_tx_size_large == 0U) ?
                                                tile_luma_lossless_unit_complete(decoder, leaf) :
                                                decoder->frame->chroma_format_idc == 2U)) {
                    decoder->phase = AV2H_TILE_DECODE_CHROMA;
                    decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                    return AV2H_OK;
                }
                if (decoder->sequence->enable_sdp &&
                                        ((decoder->frame->base_q_index == 0U &&
                                            decoder->luma_coding_mode.lossless_tx_size_large == 0U) ?
                                                tile_luma_lossless_unit_complete(decoder, leaf) :
                                                decoder->luma_tx_index == 1U) &&
                    ((decoder->luma_coding_node.block_size ==
                          AV2H_BLOCK_128X128 &&
                      decoder->luma_root_partition == AV2H_PARTITION_NONE) ||
                     ((decoder->luma_coding_node.block_size ==
                           AV2H_BLOCK_64X128 ||
                       decoder->luma_coding_node.block_size ==
                           AV2H_BLOCK_128X64) &&
                      decoder->shared_chroma_mode_pending))) {
                    if (decoder->luma_coding_node.block_size !=
                            AV2H_BLOCK_128X128) {
                        status = av2h_partition_traversal_reset_root(
                            &decoder->chroma_traversal,
                            decoder->shared_chroma_node);
                        if (status != AV2H_OK) {
                            return av2h_tile_decoder_fail(decoder, status);
                        }
                    }
                    decoder->chroma_root_forced_partition =
                        AV2H_PARTITION_NONE;
                    decoder->square_root_child_index = 4U;
                    decoder->shared_chroma_resume_luma = 1U;
                    decoder->phase = AV2H_TILE_DECODE_CHROMA;
                    decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                }
                return AV2H_OK;
            }
            leaf = decoder->luma_coding_node;
            decoder->luma_tx_index = 0U;
            decoder->luma_tx_count = 0U;
            if (decoder->shared_chroma_resume_luma &&
                decoder->chroma_tx_count != 0U &&
                decoder->luma_coding_node.row ==
                    decoder->chroma_coding_node.row &&
                decoder->luma_coding_node.column ==
                    decoder->chroma_coding_node.column &&
                decoder->luma_coding_node.block_size ==
                    decoder->chroma_coding_node.block_size &&
                (decoder->frame->chroma_format_idc == 2U ||
                 (decoder->frame->base_q_index == 0U &&
                  decoder->frame->chroma_format_idc == 0U &&
                  decoder->luma_coding_mode.lossless_tx_size_large == 0U))) {
                decoder->shared_chroma_commit_luma = 1U;
                decoder->phase = AV2H_TILE_DECODE_CHROMA;
                decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                return AV2H_OK;
            }
            if (decoder->sequence->enable_sdp &&
                decoder->shared_chroma_mode_pending &&
                !decoder->shared_chroma_complete &&
                (leaf.block_size == AV2H_BLOCK_64X128 ||
                 leaf.block_size == AV2H_BLOCK_128X64)) {
                status = av2h_partition_traversal_reset_root(
                    &decoder->chroma_traversal,
                    decoder->shared_chroma_node);
                if (status != AV2H_OK) {
                    return av2h_tile_decoder_fail(decoder, status);
                }
                decoder->chroma_root_forced_partition =
                    AV2H_PARTITION_NONE;
                decoder->shared_chroma_commit_luma = 1U;
                decoder->shared_chroma_resume_luma = 1U;
                decoder->phase = AV2H_TILE_DECODE_CHROMA;
                decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                return AV2H_OK;
            }
        }
        status = av2h_partition_traversal_commit_leaf(
            &decoder->luma_traversal, &decoder->partition_contexts, leaf);
    } else if (decoder->phase == AV2H_TILE_DECODE_CHROMA) {
        if (decoder->chroma_tx_count != 0U) {
            Av2hPartitionNode expected;
            status = decoder->frame->base_q_index == 0U ?
                av2h_chroma_lossless_tx_node_get(
                    &decoder->chroma_coding_node,
                    decoder->mi_columns, decoder->mi_rows,
                    decoder->frame->chroma_format_idc,
                    decoder->chroma_tx_index, &expected) :
                av2h_chroma_tx_node_get(
                    &decoder->chroma_coding_node,
                    decoder->frame->chroma_format_idc,
                    decoder->chroma_tx_index, &expected);
            if (status != AV2H_OK || expected.row != leaf.row ||
                expected.column != leaf.column ||
                expected.block_size != leaf.block_size) {
                return av2h_tile_decoder_fail(
                    decoder, AV2H_INVALID_ARGUMENT);
            }
            ++decoder->chroma_tx_index;
            if (decoder->frame->base_q_index == 0U &&
                decoder->chroma_tx_plane != 0U &&
                tile_chroma_lossless_unit_complete(decoder, leaf)) {
                if (decoder->chroma_tx_plane == 1U) {
                    decoder->chroma_tx_plane = 2U;
                    decoder->chroma_tx_index =
                        decoder->chroma_tx_unit_start;
                    return AV2H_OK;
                }
                decoder->chroma_tx_plane = 1U;
                decoder->chroma_tx_unit_start = decoder->chroma_tx_index;
                if (decoder->chroma_tx_index < decoder->chroma_tx_count) {
                    if (decoder->shared_chroma_resume_luma &&
                        decoder->luma_tx_count != 0U &&
                        decoder->luma_tx_index < decoder->luma_tx_count &&
                        decoder->luma_coding_node.row ==
                            decoder->chroma_coding_node.row &&
                        decoder->luma_coding_node.column ==
                            decoder->chroma_coding_node.column &&
                        decoder->luma_coding_node.block_size ==
                            decoder->chroma_coding_node.block_size) {
                        decoder->phase = AV2H_TILE_DECODE_LUMA;
                        decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                    }
                    return AV2H_OK;
                }
                decoder->chroma_tx_plane = 0U;
            }
            if (decoder->chroma_tx_index < decoder->chroma_tx_count) {
                if (decoder->chroma_tx_plane == 0U &&
                    decoder->frame->base_q_index != 0U &&
                    decoder->shared_chroma_resume_luma &&
                    decoder->luma_tx_count != 0U &&
                    decoder->luma_tx_index < decoder->luma_tx_count &&
                    decoder->luma_coding_node.row ==
                        decoder->chroma_coding_node.row &&
                    decoder->luma_coding_node.column ==
                        decoder->chroma_coding_node.column &&
                    decoder->luma_coding_node.block_size ==
                        decoder->chroma_coding_node.block_size &&
                    decoder->frame->chroma_format_idc == 2U) {
                    decoder->phase = AV2H_TILE_DECODE_LUMA;
                    decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                }
                return AV2H_OK;
            }
            if (decoder->frame->base_q_index != 0U &&
                decoder->chroma_tx_plane == 1U) {
                decoder->chroma_tx_plane = 2U;
                decoder->chroma_tx_index = 0U;
                return AV2H_OK;
            }
            leaf = decoder->chroma_coding_node;
            decoder->chroma_tx_index = 0U;
            decoder->chroma_tx_count = 0U;
            decoder->chroma_tx_unit_start = 0U;
            decoder->chroma_tx_plane = 0U;
        }
        status = av2h_partition_traversal_commit_leaf(
            &decoder->chroma_traversal, &decoder->partition_contexts, leaf);
    } else {
        status = AV2H_INVALID_ARGUMENT;
    }
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    return AV2H_OK;
}