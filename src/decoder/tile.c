#include "tile.h"

#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"
#include "tx_geometry.h"

#include <stddef.h>

Av2hStatus av2h_tile_luma_tx_node_get(const Av2hTileDecoder *decoder, uint16_t index,
                                      Av2hPartitionNode *node) {
    if (decoder == NULL || node == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (decoder->luma.selected_tx_count != 0U) {
        if (index >= decoder->luma.selected_tx_count) {
            return AV2H_INVALID_ARGUMENT;
        }
        *node = decoder->luma.tx_nodes[index];
        return AV2H_OK;
    }
    return av2h_luma_tx_node_get(&decoder->luma.coding_node, index, node);
}

static void tile_reset_left_partition_contexts(Av2hTileDecoder *decoder) {
    size_t structure;
    size_t index;
    for (structure = 0U; structure < AV2H_PARTITION_STRUCTURE_COUNT; ++structure) {
        for (index = 0U; index < decoder->entropy_state.partition_contexts.left_count; ++index) {
            decoder->entropy_state.partition_contexts.left[structure][index] = 0U;
        }
    }
}

const Av2hTileDiagnostics *av2h_tile_diagnostics_get(const Av2hDecodeDiagnostics *diagnostics) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return diagnostics != NULL ? &diagnostics->tile : NULL;
#else
    (void)diagnostics;
    return NULL;
#endif
}

void av2h_tile_diagnostics_set_luma_range(Av2hDecodeDiagnostics *diagnostics, size_t index,
                                          uint32_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL && index < 5U) {
        diagnostics->tile.luma_stage_ranges[index] = value;
    }
#else
    (void)diagnostics;
    (void)index;
    (void)value;
#endif
}

void av2h_tile_diagnostics_set_luma_stage(Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL)
        diagnostics->tile.luma_tile_stage = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}

void av2h_tile_diagnostics_set_chroma_stage(Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL)
        diagnostics->tile.chroma_tile_stage = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}

void av2h_tile_diagnostics_set_chroma_reason(Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL)
        diagnostics->tile.chroma_parse_reason = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}

static int tile_luma_lossless_unit_complete(const Av2hTileDecoder *decoder,
                                            Av2hPartitionNode leaf) {
    Av2hPartitionNode next;
    if (decoder->luma.tx_index >= decoder->luma.tx_count)
        return 1;
    if (av2h_luma_lossless_tx_node_get(&decoder->luma.coding_node, decoder->traversal.mi_columns,
                                       decoder->traversal.mi_rows, decoder->luma.tx_index,
                                       &next) != AV2H_OK) {
        return 0;
    }
    return ((leaf.row - decoder->luma.coding_node.row) / 16U !=
                (next.row - decoder->luma.coding_node.row) / 16U ||
            (leaf.column - decoder->luma.coding_node.column) / 16U !=
                (next.column - decoder->luma.coding_node.column) / 16U);
}

static int tile_chroma_lossless_unit_complete(const Av2hTileDecoder *decoder,
                                              Av2hPartitionNode leaf) {
    Av2hPartitionNode next;
    if (decoder->chroma.tx_index >= decoder->chroma.tx_count)
        return 1;
    if (av2h_chroma_lossless_tx_node_get(&decoder->chroma.coding_node,
                                         decoder->traversal.mi_columns, decoder->traversal.mi_rows,
                                         decoder->entropy_state.frame->chroma_format_idc,
                                         decoder->chroma.tx_index, &next) != AV2H_OK) {
        return 0;
    }
    return ((leaf.row - decoder->chroma.coding_node.row) / 16U !=
                (next.row - decoder->chroma.coding_node.row) / 16U ||
            (leaf.column - decoder->chroma.coding_node.column) / 16U !=
                (next.column - decoder->chroma.coding_node.column) / 16U);
}

static Av2hStatus tile_group_parse(Av2hSpan payload, size_t uncompressed_header_bits,
                                   uint16_t tile_count, uint8_t tile_id_bits,
                                   uint8_t tile_size_bytes, uint16_t expected_start_tile,
                                   uint8_t first_group, Av2hTileGroup *group) {
    Av2hBitReader reader;
    size_t header_size;
    size_t cursor;
    size_t tile_index;
    if (group == NULL || (payload.data == NULL && payload.size != 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(group, 0U, sizeof(*group));
    if (payload.size == 0U)
        return AV2H_TRUNCATED;
    if (uncompressed_header_bits == SIZE_MAX)
        return AV2H_OVERFLOW;
    if (tile_count == 0U || tile_count > AV2H_PROFILE_MAX_TILES ||
        expected_start_tile >= tile_count || tile_size_bytes > 4U) {
        return AV2H_LIMIT_EXCEEDED;
    }
    av2h_bit_reader_init(&reader, payload.data, payload.size, 0U);
    if (av2h_bit_reader_read(&reader, 1U) != first_group) {
        return first_group != 0U ? AV2H_UNSUPPORTED : AV2H_INVALID_DATA;
    }
    if (first_group != 0U || av2h_bit_reader_read(&reader, 1U) != 0U) {
        (void)av2h_bit_reader_skip(&reader, uncompressed_header_bits);
    }
    if (reader.status != AV2H_OK)
        return reader.status;
    group->start_tile = 0U;
    group->end_tile = (uint16_t)(tile_count - 1U);
    if (tile_count > 1U && av2h_bit_reader_read(&reader, 1U) != 0U) {
        group->start_tile = (uint16_t)av2h_bit_reader_read(&reader, tile_id_bits);
        group->end_tile = (uint16_t)av2h_bit_reader_read(&reader, tile_id_bits);
    }
    if (reader.status != AV2H_OK)
        return reader.status;
    if (group->start_tile != expected_start_tile || group->start_tile > group->end_tile ||
        group->end_tile >= tile_count) {
        return AV2H_INVALID_DATA;
    }
    if (av2h_bit_reader_align_zero(&reader) != AV2H_OK) {
        return reader.status;
    }
    header_size = reader.bit_position >> 3U;
    if (header_size >= payload.size)
        return AV2H_TRUNCATED;
    group->header_bits = reader.bit_position;
    group->header_size = header_size;
    group->tile_count = (uint16_t)(group->end_tile - group->start_tile + 1U);
    cursor = header_size;
    for (tile_index = 0U; tile_index < group->tile_count; ++tile_index) {
        size_t tile_size;
        if (tile_index + 1U == group->tile_count) {
            tile_size = payload.size - cursor;
        } else {
            unsigned int byte_index;
            uint32_t encoded_size = 0U;
            if (tile_size_bytes == 0U || payload.size - cursor < tile_size_bytes) {
                return AV2H_TRUNCATED;
            }
            for (byte_index = 0U; byte_index < tile_size_bytes; ++byte_index) {
                encoded_size |= (uint32_t)payload.data[cursor + byte_index] << (byte_index * 8U);
            }
            cursor += tile_size_bytes;
            tile_size = (size_t)encoded_size + 1U;
            if (tile_size > payload.size - cursor)
                return AV2H_TRUNCATED;
        }
        if (tile_size == 0U)
            return AV2H_TRUNCATED;
        group->tiles[tile_index].data = payload.data + cursor;
        group->tiles[tile_index].size = tile_size;
        cursor += tile_size;
    }
    group->entropy = group->tiles[0];
    return AV2H_OK;
}

Av2hStatus av2h_tile_group_single_parse(Av2hSpan payload, size_t uncompressed_header_bits,
                                        Av2hTileGroup *group) {
    return tile_group_parse(payload, uncompressed_header_bits, 1U, 0U, 0U, 0U, 1U, group);
}

Av2hStatus av2h_tile_group_parse(Av2hSpan payload, const Av2hFrameState *frame,
                                 uint16_t expected_start_tile, Av2hTileGroup *group) {
    uint32_t tile_count;
    if (frame == NULL)
        return AV2H_INVALID_ARGUMENT;
    tile_count = (uint32_t)frame->tile_column_count * frame->tile_row_count;
    if (tile_count == 0U || tile_count > AV2H_PROFILE_MAX_TILES) {
        return AV2H_LIMIT_EXCEEDED;
    }
    return tile_group_parse(payload, frame->uncompressed_header_bits, (uint16_t)tile_count,
                            (uint8_t)(frame->tile_column_log2 + frame->tile_row_log2),
                            frame->tile_size_bytes, expected_start_tile, 1U, group);
}

Av2hStatus av2h_tile_groups_ledger_parse(Av2hSpan input, const Av2hObuLedger *ledger,
                                         const Av2hFrameState *frame, Av2hTileGroup *group) {
    Av2hTileGroup current;
    uint32_t total_tiles;
    uint16_t expected_tile = 0U;
    uint8_t found_frame = 0U;
    size_t obu_index;
    if (ledger == NULL || frame == NULL || group == NULL ||
        (input.data == NULL && input.size != 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    total_tiles = (uint32_t)frame->tile_column_count * frame->tile_row_count;
    if (total_tiles == 0U || total_tiles > AV2H_PROFILE_MAX_TILES) {
        return AV2H_LIMIT_EXCEEDED;
    }
    av2h_memory_fill(group, 0U, sizeof(*group));
    for (obu_index = 0U; obu_index < ledger->count; ++obu_index) {
        const Av2hObu *obu = &ledger->entries[obu_index];
        Av2hFrameState group_frame;
        Av2hStatus status;
        size_t tile_index;
        if (obu->type != AV2H_OBU_CLOSED_LOOP_KEY && obu->type != AV2H_OBU_REGULAR_TILE_GROUP) {
            continue;
        }
        if (obu->payload_offset > input.size ||
            obu->payload_size > input.size - obu->payload_offset) {
            return AV2H_TRUNCATED;
        }
        if (found_frame != 0U && expected_tile >= total_tiles) {
            return AV2H_INVALID_DATA;
        }
        if (found_frame == 0U) {
            if (obu->type != AV2H_OBU_CLOSED_LOOP_KEY) {
                return AV2H_INVALID_DATA;
            }
            found_frame = 1U;
            group_frame = *frame;
        } else {
            group_frame = *frame;
        }
        if (expected_tile == 0U) {
            status = av2h_tile_group_parse(
                (Av2hSpan){input.data + obu->payload_offset, obu->payload_size}, &group_frame,
                expected_tile, &current);
        } else {
            status = tile_group_parse(
                (Av2hSpan){input.data + obu->payload_offset, obu->payload_size},
                group_frame.uncompressed_header_bits, (uint16_t)total_tiles,
                (uint8_t)(group_frame.tile_column_log2 + group_frame.tile_row_log2),
                group_frame.tile_size_bytes, expected_tile, 0U, &current);
        }
        if (status != AV2H_OK)
            return status;
        if ((size_t)group->tile_count + current.tile_count > AV2H_PROFILE_MAX_TILES) {
            return AV2H_LIMIT_EXCEEDED;
        }
        if (group->tile_count == 0U) {
            group->header_bits = current.header_bits;
            group->header_size = current.header_size;
            group->start_tile = current.start_tile;
        }
        for (tile_index = 0U; tile_index < current.tile_count; ++tile_index) {
            group->tiles[group->tile_count++] = current.tiles[tile_index];
        }
        group->end_tile = current.end_tile;
        expected_tile = (uint16_t)(current.end_tile + 1U);
    }
    if (found_frame == 0U || expected_tile != total_tiles || group->tile_count != total_tiles) {
        return AV2H_INVALID_DATA;
    }
    group->entropy = group->tiles[0];
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_fail(Av2hTileDecoder *decoder, Av2hStatus status) {
    if (decoder != NULL && status != AV2H_OK)
        decoder->control.status = status;
    return status;
}

static Av2hStatus tile_superblock_geometry(const Av2hSequenceState *sequence,
                                           uint16_t *superblock_mi,
                                           Av2hBlockSize *root_block_size) {
    if (sequence == NULL || superblock_mi == NULL || root_block_size == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->superblock_size == 0U || sequence->superblock_size == 64U) {
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
    if (decoder == NULL)
        return AV2H_INVALID_ARGUMENT;
    root.row = decoder->superblock.sb_row;
    root.column = decoder->superblock.sb_column;
    root.block_size = decoder->root.root_block_size;
    decoder->root.luma_root_partition = AV2H_PARTITION_INVALID;
    decoder->root.chroma_root_partition = AV2H_PARTITION_INVALID;
    decoder->root.chroma_root_forced_partition = AV2H_PARTITION_INVALID;
    decoder->root.square_child_luma_partition = AV2H_PARTITION_INVALID;
    decoder->root.luma_root_child_count = 0U;
    decoder->root.luma_root_children_share = 1U;
    decoder->root.square_child_luma_child_count = 0U;
    decoder->root.square_child_luma_children_share = 1U;
    decoder->root.square_root_child_index = 0U;
    decoder->traversal.luma_partition_node_count = 0U;
    status = av2h_partition_traversal_reset_root(&decoder->traversal.luma_traversal, root);
    if (status != AV2H_OK)
        return status;
    status = av2h_partition_traversal_reset_root(&decoder->traversal.chroma_traversal, root);
    if (status != AV2H_OK)
        return status;
    return AV2H_OK;
}

static Av2hStatus tile_restoration_entropy_init(Av2hRestorationEntropyState *state,
                                                const Av2hSequenceState *sequence,
                                                const Av2hFrameState *frame,
                                                const Av2hTileContextStorage *storage) {
    size_t flag_offset = 0U;
    size_t tap_offset = 0U;
    unsigned int plane;
    Av2hStatus status;
    if (state == NULL || sequence == NULL || frame == NULL || storage == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(state, 0U, sizeof(*state));
    if (sequence->enable_restoration == 0U) {
        return AV2H_OK;
    }
    state->superblock_mi = (uint16_t)(sequence->superblock_size / 4U);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t count = frame->restoration_unit_count[plane];
        size_t plane_taps;
        size_t index;
        if (frame->restoration_type[plane] == AV2H_RESTORE_NONE)
            continue;
        if ((frame->restoration_type[plane] != AV2H_RESTORE_PC_WIENER &&
             frame->restoration_type[plane] != AV2H_RESTORE_WIENER_NONSEP &&
             (plane != 0U || frame->restoration_type[plane] != AV2H_RESTORE_SWITCHABLE)) ||
            count == 0U || !av2h_size_multiply(count, AV2H_WIENER_NONSEP_MAX_TAPS, &plane_taps) ||
            storage->restoration_unit_flags == NULL || storage->restoration_unit_taps == NULL ||
            flag_offset > storage->restoration_unit_flag_capacity ||
            count > storage->restoration_unit_flag_capacity - flag_offset ||
            tap_offset > storage->restoration_unit_tap_capacity ||
            plane_taps > storage->restoration_unit_tap_capacity - tap_offset) {
            return AV2H_UNSUPPORTED;
        }
        state->unit_flags[plane] = storage->restoration_unit_flags + flag_offset;
        state->unit_taps[plane] = storage->restoration_unit_taps + tap_offset;
        state->unit_capacity[plane] = count;
        state->unit_columns[plane] = frame->restoration_unit_columns[plane];
        state->unit_rows[plane] = frame->restoration_unit_rows[plane];
        state->unit_size[plane] = frame->restoration_unit_size[plane];
        state->active_mask |= (uint8_t)(1U << plane);
        for (index = 0U; index < count; ++index) {
            state->unit_flags[plane][index] = 0U;
        }
        flag_offset += count;
        tap_offset += plane_taps;
    }
    status = av2h_binary_cdf_init(state->restore_cdf, (Av2hBinaryCdfSpec){9051U, 32U, 0U});
    if (status != AV2H_OK)
        return status;
    status =
        av2h_binary_cdf_init(state->pc_wiener_restore_cdf, (Av2hBinaryCdfSpec){12799U, 25U, 0U});
    if (status != AV2H_OK)
        return status;
    status = av2h_binary_cdf_init(state->switchable_restore_cdf[0U],
                                  (Av2hBinaryCdfSpec){25542U, 62U, 0U});
    if (status != AV2H_OK)
        return status;
    status = av2h_binary_cdf_init(state->switchable_restore_cdf[1U],
                                  (Av2hBinaryCdfSpec){25550U, 37U, 0U});
    if (status != AV2H_OK)
        return status;
    status = av2h_binary_cdf_init(state->length_cdf[0], (Av2hBinaryCdfSpec){4898U, 56U, 0U});
    if (status != AV2H_OK)
        return status;
    status = av2h_binary_cdf_init(state->length_cdf[1], (Av2hBinaryCdfSpec){6088U, 26U, 0U});
    if (status != AV2H_OK)
        return status;
    status = av2h_binary_cdf_init(state->uv_sym_cdf, (Av2hBinaryCdfSpec){29286U, 65U, 0U});
    if (status != AV2H_OK)
        return status;
    {
        static const uint16_t thresholds[3] = {16557U, 24352U, 29677U};
        status = av2h_cdf_init(state->four_part_cdf, 8U, thresholds, 4U, 6U, 0U);
    }
    if (status != AV2H_OK)
        return status;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        unsigned int slot;
        for (slot = 0U; slot < 4U; ++slot) {
            state->bank[plane][slot][0] = 8;
            state->bank[plane][slot][1] = 8;
            state->bank[plane][slot][2] = 2;
            state->bank[plane][slot][3] = 2;
        }
    }
    return AV2H_OK;
}

static Av2hStatus tile_tx_partition_cdfs_init(Av2hTxPartitionCdfs *cdfs) {
    static const Av2hBinaryCdfSpec do_partition[2U][2U][AV2H_TX_PARTITION_GROUPS] = {
        {{{26330U, 30U, 0U},
          {29620U, 0U, 0U},
          {20420U, 30U, 0U},
          {21694U, 31U, 0U},
          {13317U, 31U, 0U},
          {15391U, 31U, 0U},
          {15952U, 31U, 0U},
          {14736U, 31U, 0U},
          {13810U, 5U, 0U}},
         {{31646U, 55U, 0U},
          {32393U, 25U, 0U},
          {30802U, 26U, 0U},
          {30485U, 31U, 0U},
          {20759U, 32U, 0U},
          {22159U, 1U, 0U},
          {26832U, 31U, 0U},
          {27351U, 1U, 0U},
          {24696U, 1U, 0U}}},
        {{{29308U, 0U, 0U},
          {32550U, 93U, 0U},
          {27963U, 1U, 0U},
          {27618U, 31U, 0U},
          {22367U, 7U, 0U},
          {23478U, 35U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {27990U, 0U, 0U}},
         {{16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U},
          {16384U, 0U, 0U}}}};
    static const Av2hBinaryCdfSpec two_or_three_way[2U][2U][2U] = {
        {{{25131U, 30U, 0U}, {24514U, 30U, 0U}}, {{19534U, 6U, 0U}, {18637U, 0U, 0U}}},
        {{{30226U, 50U, 0U}, {30703U, 33U, 0U}}, {{16384U, 0U, 0U}, {16384U, 0U, 0U}}}};
    static const uint16_t partition_type[2U][2U][AV2H_TX_PARTITION_TYPE_GROUPS][6U] = {
        {
            {
                {32744U, 32748U, 32752U, 32756U, 32760U, 32764U},
                {3006U, 22888U, 27132U, 29972U, 29976U, 32724U},
                {3673U, 8849U, 27652U, 27656U, 29944U, 29948U},
                {5219U, 19419U, 28052U, 28836U, 29528U, 31296U},
                {3055U, 19649U, 22157U, 27038U, 27466U, 31646U},
                {6044U, 11255U, 26170U, 26493U, 28585U, 29584U},
                {5896U, 20361U, 25685U, 27552U, 28695U, 31097U},
                {2355U, 17601U, 21703U, 26050U, 27881U, 31397U},
                {4701U, 13502U, 24958U, 26413U, 28166U, 30129U},
                {3319U, 16449U, 21641U, 27154U, 29260U, 31512U},
                {2526U, 17088U, 19643U, 29378U, 29382U, 32724U},
                {2298U, 4406U, 23886U, 23890U, 30148U, 30152U},
                {1553U, 16160U, 18679U, 27983U, 29592U, 32139U},
                {2316U, 4714U, 22731U, 23797U, 29514U, 30077U},
            },
            {
                {10790U, 22602U, 32736U, 32740U, 32744U, 32748U},
                {14384U, 19116U, 26545U, 28687U, 28691U, 32724U},
                {13910U, 21690U, 26343U, 26347U, 28432U, 28436U},
                {15891U, 21712U, 28890U, 29571U, 30307U, 31363U},
                {15946U, 19447U, 20270U, 23423U, 23858U, 31148U},
                {16053U, 16957U, 20312U, 20924U, 24770U, 25959U},
                {18479U, 23053U, 26582U, 26966U, 27567U, 29836U},
                {9312U, 11882U, 14354U, 17792U, 19827U, 29477U},
                {8490U, 10668U, 13295U, 15353U, 19952U, 22575U},
                {6404U, 12066U, 16173U, 20041U, 24512U, 28421U},
                {10019U, 14455U, 17658U, 27012U, 27016U, 32724U},
                {9479U, 14904U, 19374U, 19378U, 28027U, 28031U},
                {3717U, 7198U, 8103U, 20546U, 23558U, 31447U},
                {4058U, 5429U, 8987U, 13978U, 25126U, 26655U},
            },
        },
        {
            {
                {32434U, 32490U, 32545U, 32601U, 32657U, 32712U},
                {1491U, 14241U, 29930U, 32517U, 32524U, 32724U},
                {1719U, 16525U, 31000U, 31004U, 32575U, 32579U},
                {1645U, 16749U, 29324U, 30425U, 32016U, 32485U},
                {2908U, 15802U, 24689U, 28470U, 32122U, 32542U},
                {3470U, 17931U, 25841U, 29589U, 31907U, 32465U},
                {5638U, 19594U, 28693U, 29977U, 30703U, 32154U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {2734U, 12129U, 30342U, 31805U, 31844U, 32724U},
                {3849U, 21783U, 31043U, 31056U, 32181U, 32193U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
            },
            {
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
                {4681U, 9362U, 14043U, 18725U, 23406U, 28087U},
            },
        },
    };
    static const uint8_t partition_parameter[2U][2U][AV2H_TX_PARTITION_TYPE_GROUPS] = {
        {
            {60U, 31U, 31U, 31U, 31U, 31U, 31U, 32U, 32U, 32U, 31U, 31U, 32U, 37U},
            {32U, 31U, 31U, 32U, 32U, 32U, 62U, 62U, 62U, 62U, 37U, 37U, 62U, 62U},
        },
        {
            {50U, 57U, 30U, 62U, 25U, 43U, 50U, 0U, 0U, 0U, 50U, 65U, 0U, 0U},
            {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
        },
    };
    size_t index;
    Av2hStatus status;
    if (cdfs == NULL)
        return AV2H_INVALID_ARGUMENT;
    for (index = 0U; index < 4U * AV2H_TX_PARTITION_GROUPS; ++index) {
        size_t fsc = index / (2U * AV2H_TX_PARTITION_GROUPS);
        size_t is_inter = (index / AV2H_TX_PARTITION_GROUPS) % 2U;
        size_t group = index % AV2H_TX_PARTITION_GROUPS;
        status = av2h_binary_cdf_init(cdfs->do_partition[fsc][is_inter][group],
                                      do_partition[fsc][is_inter][group]);
        if (status != AV2H_OK)
            return status;
    }
    for (index = 0U; index < 8U; ++index) {
        size_t fsc = index / 4U;
        size_t direction = (index / 2U) % 2U;
        size_t context = index % 2U;
        status = av2h_binary_cdf_init(cdfs->two_or_three_way[fsc][direction][context],
                                      two_or_three_way[fsc][direction][context]);
        if (status != AV2H_OK)
            return status;
    }
    for (index = 0U; index < 4U * AV2H_TX_PARTITION_TYPE_GROUPS; ++index) {
        size_t fsc = index / (2U * AV2H_TX_PARTITION_TYPE_GROUPS);
        size_t direction = (index / AV2H_TX_PARTITION_TYPE_GROUPS) % 2U;
        size_t group = index % AV2H_TX_PARTITION_TYPE_GROUPS;
        status = av2h_cdf_init(
            cdfs->partition_type[fsc][direction][group], AV2H_TX_PARTITION_TYPE_SYMBOLS + 4U,
            partition_type[fsc][direction][group], AV2H_TX_PARTITION_TYPE_SYMBOLS,
            partition_parameter[fsc][direction][group], 0U);
        if (status != AV2H_OK)
            return status;
    }
    return AV2H_OK;
}

void av2h_tile_capture_root_partition(Av2hTileDecoder *decoder, const Av2hLeafPath *path,
                                      Av2hPartitionType *root_partition) {
    uint16_t root_row = decoder->superblock.sb_row;
    uint16_t root_column = decoder->superblock.sb_column;
    Av2hBlockSize root_block_size = decoder->root.root_block_size;
    size_t index;
    if (*root_partition != AV2H_PARTITION_INVALID)
        return;
    if (decoder->control.phase == AV2H_TILE_DECODE_CHROMA &&
        decoder->entropy_state.sequence->enable_sdp &&
        decoder->root.root_block_size == AV2H_BLOCK_128X128 &&
        decoder->root.square_root_child_index < 4U) {
        root_row = (uint16_t)(root_row + (decoder->root.square_root_child_index >= 2U ? 16U : 0U));
        root_column = (uint16_t)(root_column +
                                 ((decoder->root.square_root_child_index & 1U) != 0U ? 16U : 0U));
        root_block_size = AV2H_BLOCK_64X64;
    }
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        if (decision->node.row == root_row && decision->node.column == root_column &&
            decision->node.block_size == root_block_size) {
            *root_partition = decision->partition;
            return;
        }
    }
}

uint8_t av2h_tile_partition_direction(Av2hPartitionType partition) {
    if (partition == AV2H_PARTITION_HORZ || partition == AV2H_PARTITION_HORZ_3 ||
        partition == AV2H_PARTITION_HORZ_4A || partition == AV2H_PARTITION_HORZ_4B) {
        return 1U;
    }
    if (partition == AV2H_PARTITION_VERT || partition == AV2H_PARTITION_VERT_3 ||
        partition == AV2H_PARTITION_VERT_4A || partition == AV2H_PARTITION_VERT_4B) {
        return 2U;
    }
    return 0U;
}

Av2hStatus av2h_tile_decoder_init(Av2hTileDecoder *decoder, Av2hSpan entropy,
                                  const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                  const Av2hTileContextStorage *storage, uint16_t tile_id) {
    size_t cell_count;
    size_t sb_columns;
    size_t sb_rows;
    uint32_t frame_tile_count;
    uint16_t tile_column;
    uint16_t tile_row;
    size_t index;
    Av2hStatus status;
    if (decoder == NULL || sequence == NULL || frame == NULL || storage == NULL ||
        entropy.data == NULL || entropy.size == 0U || frame->width == 0U || frame->height == 0U ||
        storage->above_count == 0U || storage->left_count == 0U ||
        storage->luma_partition_above == NULL || storage->luma_partition_left == NULL ||
        storage->chroma_partition_above == NULL || storage->chroma_partition_left == NULL ||
        storage->luma_mode_above == NULL || storage->luma_mode_left == NULL ||
        storage->coefficient_above == NULL || storage->coefficient_left == NULL ||
        storage->colocated_luma == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    decoder->traversal.mi_columns = av2h_frame_mi_dimension(frame->width);
    decoder->traversal.mi_rows = av2h_frame_mi_dimension(frame->height);
    if (decoder->traversal.mi_columns > storage->above_count ||
        decoder->traversal.mi_rows > storage->left_count ||
        !av2h_size_multiply(decoder->traversal.mi_columns, decoder->traversal.mi_rows,
                            &cell_count) ||
        cell_count > storage->colocated_luma_count || decoder->traversal.mi_columns > 65535U ||
        decoder->traversal.mi_rows > 65535U) {
        return AV2H_OUT_OF_MEMORY;
    }
    decoder->entropy_state.entropy = entropy;
    decoder->entropy_state.sequence = sequence;
    decoder->entropy_state.frame = frame;
    decoder->traversal.colocated_luma = storage->colocated_luma;
    decoder->traversal.colocated_luma_count = storage->colocated_luma_count;
    decoder->chroma.tx_index = 0U;
    decoder->chroma.tx_count = 0U;
    decoder->chroma.tx_unit_start = 0U;
    status = tile_superblock_geometry(sequence, &decoder->superblock.superblock_mi,
                                      &decoder->root.root_block_size);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    frame_tile_count = (uint32_t)frame->tile_column_count * frame->tile_row_count;
    if (tile_id == UINT16_MAX || frame_tile_count == 0U) {
        decoder->traversal.tile_start_column = 0U;
        decoder->traversal.tile_end_column = (uint16_t)decoder->traversal.mi_columns;
        decoder->traversal.tile_start_row = 0U;
        decoder->traversal.tile_end_row = (uint16_t)decoder->traversal.mi_rows;
    } else {
        if (tile_id >= frame_tile_count) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
        }
        tile_column = (uint16_t)(tile_id % frame->tile_column_count);
        tile_row = (uint16_t)(tile_id / frame->tile_column_count);
        decoder->traversal.tile_start_column = (uint16_t)(frame->tile_column_start_sb[tile_column] *
                                                          decoder->superblock.superblock_mi);
        decoder->traversal.tile_end_column =
            (uint16_t)(frame->tile_column_start_sb[tile_column + 1U] *
                       decoder->superblock.superblock_mi);
        decoder->traversal.tile_start_row =
            (uint16_t)(frame->tile_row_start_sb[tile_row] * decoder->superblock.superblock_mi);
        decoder->traversal.tile_end_row =
            (uint16_t)(frame->tile_row_start_sb[tile_row + 1U] * decoder->superblock.superblock_mi);
        if (decoder->traversal.tile_end_column > decoder->traversal.mi_columns) {
            decoder->traversal.tile_end_column = (uint16_t)decoder->traversal.mi_columns;
        }
        if (decoder->traversal.tile_end_row > decoder->traversal.mi_rows) {
            decoder->traversal.tile_end_row = (uint16_t)decoder->traversal.mi_rows;
        }
    }
    if (decoder->traversal.tile_start_column >= decoder->traversal.tile_end_column ||
        decoder->traversal.tile_start_row >= decoder->traversal.tile_end_row) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
    }
    if (storage->left_count < decoder->superblock.superblock_mi) {
        return av2h_tile_decoder_fail(decoder, AV2H_OUT_OF_MEMORY);
    }
    sb_columns = (decoder->traversal.tile_end_column - decoder->traversal.tile_start_column +
                  decoder->superblock.superblock_mi - 1U) /
                 decoder->superblock.superblock_mi;
    sb_rows = (decoder->traversal.tile_end_row - decoder->traversal.tile_start_row +
               decoder->superblock.superblock_mi - 1U) /
              decoder->superblock.superblock_mi;
    if (!av2h_size_multiply(sb_columns, sb_rows, &decoder->superblock.sb_count)) {
        return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
    }
    decoder->superblock.sb_row = decoder->traversal.tile_start_row;
    decoder->superblock.sb_column = decoder->traversal.tile_start_column;
    decoder->superblock.current_q_index = frame->base_q_index;
    decoder->superblock.current_block_q_index = frame->base_q_index;
    decoder->superblock.sb_index = 0U;
    decoder->control.phase = AV2H_TILE_DECODE_LUMA;
    decoder->control.event = AV2H_TILE_EVENT_NONE;
    decoder->control.status = AV2H_OK;
    decoder->luma.tx_index = 0U;
    decoder->luma.tx_count = 0U;
    decoder->chroma.shared_resume_luma = 0U;
    decoder->chroma.shared_complete = 0U;
    decoder->chroma.shared_mode_pending = 0U;
    decoder->chroma.shared_commit_luma = 0U;
    for (index = 0U; index < cell_count; ++index) {
        decoder->traversal.colocated_luma[index].context = (Av2hLumaModeContext){0};
        decoder->traversal.colocated_luma[index].mode = (uint8_t)AV2H_INTRA_DC;
        decoder->traversal.colocated_luma[index].angle_delta = 0;
        decoder->traversal.colocated_luma[index].chroma_cfl = 0U;
    }
    status = av2h_partition_cdfs_init(&decoder->entropy_state.partition_cdfs);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_leaf_cdfs_init(&decoder->entropy_state.leaf_cdfs);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status =
        av2h_gdf_entropy_state_init(&decoder->entropy_state.gdf_entropy, sequence, frame,
                                    storage->gdf_block_flags, storage->gdf_block_flag_capacity);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = tile_id != UINT16_MAX && tile_id != 0U
                 ? av2h_cdef_entropy_state_init_preserving_controls(
                       &decoder->entropy_state.cdef_entropy, sequence, frame,
                       storage->cdef_unit_strengths, storage->cdef_unit_strength_capacity)
                 : av2h_cdef_entropy_state_init(&decoder->entropy_state.cdef_entropy, sequence,
                                                frame, storage->cdef_unit_strengths,
                                                storage->cdef_unit_strength_capacity);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    decoder->entropy_state.cdef_entropy.tile_start_column = decoder->traversal.tile_start_column;
    decoder->entropy_state.cdef_entropy.tile_start_row = decoder->traversal.tile_start_row;
    status = tile_id != UINT16_MAX && tile_id != 0U
                 ? av2h_ccso_entropy_state_init_preserving_controls(
                       &decoder->entropy_state.ccso_entropy, sequence, frame,
                       storage->ccso_unit_controls, storage->ccso_unit_control_capacity)
                 : av2h_ccso_entropy_state_init(&decoder->entropy_state.ccso_entropy, sequence,
                                                frame, storage->ccso_unit_controls,
                                                storage->ccso_unit_control_capacity);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    decoder->entropy_state.ccso_entropy.tile_start_column = decoder->traversal.tile_start_column;
    decoder->entropy_state.ccso_entropy.tile_start_row = decoder->traversal.tile_start_row;
    status = tile_restoration_entropy_init(&decoder->entropy_state.restoration_entropy, sequence,
                                           frame, storage);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = tile_tx_partition_cdfs_init(&decoder->entropy_state.tx_partition_cdfs);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_coeff_cdfs_init(&decoder->entropy_state.coeff_cdfs, frame->base_q_index);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_transform_cdfs_init(&decoder->entropy_state.transform_cdfs);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status =
        av2h_coeff_level_cdfs_init_for_q(&decoder->entropy_state.level_cdfs, frame->base_q_index);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_partition_context_state_init(
        &decoder->entropy_state.partition_contexts, storage->luma_partition_above,
        storage->luma_partition_left, storage->chroma_partition_above,
        storage->chroma_partition_left, storage->above_count, decoder->superblock.superblock_mi);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_luma_mode_context_state_init(&decoder->entropy_state.luma_mode_contexts,
                                               storage->luma_mode_above, storage->above_count,
                                               storage->luma_mode_left, storage->left_count);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    decoder->entropy_state.luma_mode_contexts.above_count = decoder->traversal.mi_columns;
    decoder->entropy_state.luma_mode_contexts.superblock_mi =
        (uint16_t)(sequence->superblock_size / 4U);
    decoder->entropy_state.luma_mode_contexts.spatial =
        (const uint8_t *)decoder->traversal.colocated_luma;
    decoder->entropy_state.luma_mode_contexts.spatial_stride_bytes =
        sizeof(*decoder->traversal.colocated_luma);
    decoder->entropy_state.luma_mode_contexts.spatial_columns = decoder->traversal.mi_columns;
    decoder->entropy_state.luma_mode_contexts.spatial_rows = decoder->traversal.mi_rows;
    status = av2h_coeff_context_state_init(&decoder->entropy_state.coefficient_contexts,
                                           storage->coefficient_above, storage->above_count,
                                           storage->coefficient_left, storage->left_count);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    if (decoder->traversal.mi_rows >
        (size_t)decoder->entropy_state.luma_mode_contexts.superblock_mi * 2U) {
        decoder->entropy_state.coefficient_contexts.above_count = decoder->traversal.mi_columns;
        decoder->entropy_state.coefficient_contexts.left_count = decoder->traversal.mi_rows;
    }
    decoder->entropy_state.coefficient_contexts.mi_columns = decoder->traversal.mi_columns;
    decoder->entropy_state.coefficient_contexts.mi_rows = decoder->traversal.mi_rows;
    status = av2h_partition_traversal_init(
        &decoder->traversal.luma_traversal, (uint16_t)decoder->traversal.mi_rows,
        (uint16_t)decoder->traversal.mi_columns,
        (Av2hPartitionNode){0U, 0U, decoder->root.root_block_size}, 0U,
        sequence->enable_ext_partitions, sequence->enable_uneven_4way_partitions);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_partition_traversal_init(
        &decoder->traversal.chroma_traversal, (uint16_t)decoder->traversal.mi_rows,
        (uint16_t)decoder->traversal.mi_columns,
        (Av2hPartitionNode){0U, 0U, decoder->root.root_block_size}, 1U,
        sequence->enable_ext_partitions, sequence->enable_uneven_4way_partitions);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = tile_reset_traversal_roots(decoder);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = av2h_range_init(&decoder->entropy_state.range, &decoder->entropy_state.entropy, 1U);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    return AV2H_OK;
}

static Av2hStatus tile_advance_root_or_finish(Av2hTileDecoder *decoder) {
    Av2hTileFilterControls controls;
    Av2hStatus status;
    decoder->traversal.luma_partition_node_count = 0U;
    if (decoder->superblock.sb_index + 1U < decoder->superblock.sb_count) {
        ++decoder->superblock.sb_index;
        if ((size_t)decoder->superblock.sb_column + decoder->superblock.superblock_mi <
            decoder->traversal.tile_end_column) {
            decoder->superblock.sb_column =
                (uint16_t)(decoder->superblock.sb_column + decoder->superblock.superblock_mi);
        } else {
            decoder->superblock.sb_column = decoder->traversal.tile_start_column;
            decoder->superblock.sb_row =
                (uint16_t)(decoder->superblock.sb_row + decoder->superblock.superblock_mi);
            tile_reset_left_partition_contexts(decoder);
        }
        status = tile_reset_traversal_roots(decoder);
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
        decoder->control.phase = AV2H_TILE_DECODE_LUMA;
        decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    if ((uint32_t)decoder->entropy_state.frame->tile_column_count *
            decoder->entropy_state.frame->tile_row_count <=
        1U) {
        status = av2h_tile_filter_controls_handoff(decoder, &controls);
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
    }
    decoder->control.phase = AV2H_TILE_DECODE_DONE;
    decoder->control.event = AV2H_TILE_EVENT_DONE;
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_finish_root(Av2hTileDecoder *decoder) {
    Av2hPartitionTraversal *traversal;
    Av2hStatus status;
    if (decoder == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (decoder->control.status != AV2H_OK)
        return decoder->control.status;
    if (decoder->control.phase == AV2H_TILE_DECODE_LUMA) {
        traversal = &decoder->traversal.luma_traversal;
    } else if (decoder->control.phase == AV2H_TILE_DECODE_CHROMA) {
        traversal = &decoder->traversal.chroma_traversal;
    } else {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->control.phase == AV2H_TILE_DECODE_CHROMA && decoder->chroma.shared_resume_luma &&
        decoder->traversal.chroma_traversal.stack_count == 0U &&
        !decoder->traversal.chroma_traversal.has_pending_leaf) {
        if (decoder->chroma.shared_commit_luma) {
            status = av2h_partition_traversal_commit_leaf(
                &decoder->traversal.luma_traversal, &decoder->entropy_state.partition_contexts,
                decoder->shared_chroma.node);
            if (status != AV2H_OK) {
                return av2h_tile_decoder_fail(decoder, status);
            }
            decoder->chroma.shared_commit_luma = 0U;
        }
        decoder->chroma.shared_resume_luma = 0U;
        decoder->chroma.shared_complete = 1U;
        decoder->control.phase = AV2H_TILE_DECODE_LUMA;
        decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    if (traversal->stack_count != 0U || decoder->traversal.luma_traversal.has_pending_leaf ||
        decoder->traversal.chroma_traversal.has_pending_leaf) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->control.phase == AV2H_TILE_DECODE_LUMA) {
        if (av2h_frame_plane_count(decoder->entropy_state.frame) == 1U) {
            return tile_advance_root_or_finish(decoder);
        }
        if (decoder->chroma.shared_complete) {
            decoder->chroma.shared_complete = 0U;
            decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
            return av2h_tile_decoder_finish_root(decoder);
        }
        if (decoder->entropy_state.sequence->enable_sdp) {
            if (decoder->root.luma_root_partition == AV2H_PARTITION_NONE) {
                decoder->root.chroma_root_forced_partition = AV2H_PARTITION_NONE;
            } else if (decoder->root.luma_root_child_count == 2U &&
                       decoder->root.luma_root_children_share &&
                       av2h_tile_partition_direction(decoder->root.luma_root_partition) != 0U) {
                decoder->root.chroma_root_forced_partition = decoder->root.luma_root_partition;
            }
        }
        decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
        decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
        return AV2H_OK;
    }
    if (decoder->entropy_state.sequence->enable_sdp &&
        decoder->root.root_block_size == AV2H_BLOCK_128X128 &&
        decoder->root.square_root_child_index < 4U) {
        const Av2hPartitionNode *pending = NULL;
        size_t pending_index = decoder->traversal.luma_traversal.stack_count;
        while (pending_index != 0U) {
            const Av2hPartitionNode *candidate =
                &decoder->traversal.luma_traversal.stack[--pending_index];
            if (candidate->row < decoder->traversal.mi_rows &&
                candidate->column < decoder->traversal.mi_columns) {
                pending = candidate;
                break;
            }
        }
        if (pending != NULL) {
            decoder->root.square_root_child_index =
                (uint8_t)((pending->row >= decoder->superblock.sb_row + 16U ? 2U : 0U) +
                          (pending->column >= decoder->superblock.sb_column + 16U ? 1U : 0U));
            decoder->root.square_child_luma_partition = AV2H_PARTITION_INVALID;
            decoder->control.phase = AV2H_TILE_DECODE_LUMA;
            decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
            return AV2H_OK;
        }
        decoder->root.square_root_child_index = 4U;
    }
    return tile_advance_root_or_finish(decoder);
}

Av2hStatus av2h_tile_decoder_commit_leaf(Av2hTileDecoder *decoder, Av2hPartitionNode leaf) {
    Av2hStatus status;
    if (decoder == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (decoder->control.status != AV2H_OK)
        return decoder->control.status;
    if (decoder->control.phase == AV2H_TILE_DECODE_LUMA) {
        if (decoder->luma.tx_count != 0U) {
            Av2hPartitionNode expected;
            status = decoder->luma.coding_mode.q_index == 0U &&
                             decoder->luma.coding_mode.lossless_tx_size_large == 0U
                         ? av2h_luma_lossless_tx_node_get(
                               &decoder->luma.coding_node, decoder->traversal.mi_columns,
                               decoder->traversal.mi_rows, decoder->luma.tx_index, &expected)
                         : av2h_tile_luma_tx_node_get(decoder, decoder->luma.tx_index, &expected);
            if (status != AV2H_OK || expected.row != leaf.row || expected.column != leaf.column ||
                expected.block_size != leaf.block_size) {
                return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
            }
            ++decoder->luma.tx_index;
            if (decoder->luma.tx_index < decoder->luma.tx_count) {
                if (decoder->chroma.shared_resume_luma && decoder->chroma.tx_count != 0U &&
                    decoder->luma.coding_node.row == decoder->chroma.coding_node.row &&
                    decoder->luma.coding_node.column == decoder->chroma.coding_node.column &&
                    decoder->luma.coding_node.block_size ==
                        decoder->chroma.coding_node.block_size &&
                    ((decoder->luma.coding_mode.q_index == 0U &&
                      decoder->luma.coding_mode.lossless_tx_size_large == 0U)
                         ? tile_luma_lossless_unit_complete(decoder, leaf)
                         : decoder->entropy_state.frame->chroma_format_idc == 2U)) {
                    decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
                    decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                    return AV2H_OK;
                }
                if (decoder->entropy_state.sequence->enable_sdp &&
                    ((decoder->luma.coding_mode.q_index == 0U &&
                      decoder->luma.coding_mode.lossless_tx_size_large == 0U)
                         ? tile_luma_lossless_unit_complete(decoder, leaf)
                         : decoder->luma.tx_index == 1U) &&
                    ((decoder->luma.coding_node.block_size == AV2H_BLOCK_128X128 &&
                      decoder->root.luma_root_partition == AV2H_PARTITION_NONE) ||
                     ((decoder->luma.coding_node.block_size == AV2H_BLOCK_64X128 ||
                       decoder->luma.coding_node.block_size == AV2H_BLOCK_128X64) &&
                      decoder->chroma.shared_mode_pending))) {
                    if (decoder->luma.coding_node.block_size != AV2H_BLOCK_128X128) {
                        status = av2h_partition_traversal_reset_root(
                            &decoder->traversal.chroma_traversal, decoder->shared_chroma.node);
                        if (status != AV2H_OK) {
                            return av2h_tile_decoder_fail(decoder, status);
                        }
                    }
                    decoder->root.chroma_root_forced_partition = AV2H_PARTITION_NONE;
                    decoder->root.square_root_child_index = 4U;
                    decoder->chroma.shared_resume_luma = 1U;
                    decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
                    decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                }
                return AV2H_OK;
            }
            leaf = decoder->luma.coding_node;
            decoder->luma.tx_index = 0U;
            decoder->luma.tx_count = 0U;
            decoder->luma.selected_tx_count = 0U;
            if (decoder->chroma.shared_resume_luma && decoder->chroma.tx_count != 0U &&
                decoder->luma.coding_node.row == decoder->chroma.coding_node.row &&
                decoder->luma.coding_node.column == decoder->chroma.coding_node.column &&
                decoder->luma.coding_node.block_size == decoder->chroma.coding_node.block_size &&
                (decoder->entropy_state.frame->chroma_format_idc == 2U ||
                 (decoder->luma.coding_mode.q_index == 0U &&
                  decoder->entropy_state.frame->chroma_format_idc == 0U &&
                  decoder->luma.coding_mode.lossless_tx_size_large == 0U))) {
                decoder->chroma.shared_commit_luma = 1U;
                decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
                decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                return AV2H_OK;
            }
            if (decoder->entropy_state.sequence->enable_sdp &&
                decoder->chroma.shared_mode_pending && !decoder->chroma.shared_complete &&
                (leaf.block_size == AV2H_BLOCK_64X128 || leaf.block_size == AV2H_BLOCK_128X64)) {
                status = av2h_partition_traversal_reset_root(&decoder->traversal.chroma_traversal,
                                                             decoder->shared_chroma.node);
                if (status != AV2H_OK) {
                    return av2h_tile_decoder_fail(decoder, status);
                }
                decoder->root.chroma_root_forced_partition = AV2H_PARTITION_NONE;
                decoder->chroma.shared_commit_luma = 1U;
                decoder->chroma.shared_resume_luma = 1U;
                decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
                decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                return AV2H_OK;
            }
        }
        status = av2h_partition_traversal_commit_leaf(
            &decoder->traversal.luma_traversal, &decoder->entropy_state.partition_contexts, leaf);
    } else if (decoder->control.phase == AV2H_TILE_DECODE_CHROMA) {
        if (decoder->chroma.tx_count != 0U) {
            Av2hPartitionNode expected;
            status = decoder->superblock.current_block_q_index == 0U
                         ? av2h_chroma_lossless_tx_node_get(
                               &decoder->chroma.coding_node, decoder->traversal.mi_columns,
                               decoder->traversal.mi_rows,
                               decoder->entropy_state.frame->chroma_format_idc,
                               decoder->chroma.tx_index, &expected)
                         : av2h_chroma_tx_node_get(&decoder->chroma.coding_node,
                                                   decoder->entropy_state.frame->chroma_format_idc,
                                                   decoder->chroma.tx_index, &expected);
            if (status != AV2H_OK || expected.row != leaf.row || expected.column != leaf.column ||
                expected.block_size != leaf.block_size) {
                return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
            }
            ++decoder->chroma.tx_index;
            if (decoder->superblock.current_block_q_index == 0U && decoder->chroma.tx_plane != 0U &&
                tile_chroma_lossless_unit_complete(decoder, leaf)) {
                if (decoder->chroma.tx_plane == 1U) {
                    decoder->chroma.tx_plane = 2U;
                    decoder->chroma.tx_index = decoder->chroma.tx_unit_start;
                    return AV2H_OK;
                }
                decoder->chroma.tx_plane = 1U;
                decoder->chroma.tx_unit_start = decoder->chroma.tx_index;
                if (decoder->chroma.tx_index < decoder->chroma.tx_count) {
                    if (decoder->chroma.shared_resume_luma && decoder->luma.tx_count != 0U &&
                        decoder->luma.tx_index < decoder->luma.tx_count &&
                        decoder->luma.coding_node.row == decoder->chroma.coding_node.row &&
                        decoder->luma.coding_node.column == decoder->chroma.coding_node.column &&
                        decoder->luma.coding_node.block_size ==
                            decoder->chroma.coding_node.block_size) {
                        decoder->control.phase = AV2H_TILE_DECODE_LUMA;
                        decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                    }
                    return AV2H_OK;
                }
                decoder->chroma.tx_plane = 0U;
            }
            if (decoder->chroma.tx_index < decoder->chroma.tx_count) {
                if (decoder->chroma.tx_plane == 0U &&
                    decoder->superblock.current_block_q_index != 0U &&
                    decoder->chroma.shared_resume_luma && decoder->luma.tx_count != 0U &&
                    decoder->luma.tx_index < decoder->luma.tx_count &&
                    decoder->luma.coding_node.row == decoder->chroma.coding_node.row &&
                    decoder->luma.coding_node.column == decoder->chroma.coding_node.column &&
                    decoder->luma.coding_node.block_size ==
                        decoder->chroma.coding_node.block_size &&
                    decoder->entropy_state.frame->chroma_format_idc == 2U) {
                    decoder->control.phase = AV2H_TILE_DECODE_LUMA;
                    decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
                }
                return AV2H_OK;
            }
            if (decoder->superblock.current_block_q_index != 0U && decoder->chroma.tx_plane == 1U) {
                decoder->chroma.tx_plane = 2U;
                decoder->chroma.tx_index = 0U;
                return AV2H_OK;
            }
            leaf = decoder->chroma.coding_node;
            decoder->chroma.tx_index = 0U;
            decoder->chroma.tx_count = 0U;
            decoder->chroma.tx_unit_start = 0U;
            decoder->chroma.tx_plane = 0U;
        }
        status = av2h_partition_traversal_commit_leaf(
            &decoder->traversal.chroma_traversal, &decoder->entropy_state.partition_contexts, leaf);
    } else {
        status = AV2H_INVALID_ARGUMENT;
    }
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    return AV2H_OK;
}
