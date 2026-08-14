#include "tile.h"
#include "internal/coeff_modules.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"
#include "tx_geometry.h"

static void tile_capture_luma_root_child(
    Av2hTileDecoder *decoder, const Av2hFirstLeafPath *path) {
    Av2hBlockSize child_size;
    uint16_t first_row = decoder->sb_row;
    uint16_t first_column = decoder->sb_column;
    uint16_t second_row = decoder->sb_row;
    uint16_t second_column = decoder->sb_column;
    Av2hPartitionType root_partition = decoder->luma_root_partition;
    uint8_t *child_count = &decoder->luma_root_child_count;
    uint8_t *children_share = &decoder->luma_root_children_share;
    uint8_t square_child = 0U;
    size_t index;
    if (decoder->root_block_size == AV2H_BLOCK_128X128 &&
        av2h_tile_partition_direction(
            decoder->luma_root_partition) != 0U) {
        for (index = 0U; index < path->decision_count; ++index) {
            const Av2hPartitionDecision *decision = &path->decisions[index];
            if (decision->node.block_size == AV2H_BLOCK_64X64 &&
                decision->node.row >= decoder->sb_row &&
                decision->node.row < decoder->sb_row + 32U &&
                decision->node.column >= decoder->sb_column &&
                decision->node.column < decoder->sb_column + 32U) {
                decoder->square_root_child_index = (uint8_t)(
                    (decision->node.row >= decoder->sb_row + 16U ? 2U : 0U) +
                    (decision->node.column >= decoder->sb_column + 16U ?
                         1U : 0U));
                break;
            }
        }
    }
    if (decoder->root_block_size == AV2H_BLOCK_128X128 &&
        decoder->square_root_child_index < 4U) {
        uint16_t root_row = (uint16_t)(decoder->sb_row +
            (decoder->square_root_child_index >= 2U ? 16U : 0U));
        uint16_t root_column = (uint16_t)(decoder->sb_column +
            ((decoder->square_root_child_index & 1U) != 0U ? 16U : 0U));
        for (index = 0U; index < path->decision_count; ++index) {
            const Av2hPartitionDecision *decision = &path->decisions[index];
            if (decision->node.row == root_row &&
                decision->node.column == root_column &&
                decision->node.block_size == AV2H_BLOCK_64X64) {
                decoder->square_child_luma_partition = decision->partition;
                break;
            }
        }
        if (decoder->square_child_luma_partition !=
                AV2H_PARTITION_INVALID) {
            root_partition = decoder->square_child_luma_partition;
            first_row = root_row;
            first_column = root_column;
            second_row = root_row;
            second_column = root_column;
            child_count = &decoder->square_child_luma_child_count;
            children_share = &decoder->square_child_luma_children_share;
            square_child = 1U;
        }
    }
    if (root_partition == AV2H_PARTITION_HORZ) {
        child_size = square_child ? AV2H_BLOCK_64X32 :
            decoder->root_block_size == AV2H_BLOCK_64X64 ?
            AV2H_BLOCK_64X32 :
            decoder->root_block_size == AV2H_BLOCK_128X128 ?
                AV2H_BLOCK_128X64 : AV2H_BLOCK_INVALID;
        second_row = (uint16_t)(second_row +
            (child_size == AV2H_BLOCK_128X64 ? 16U : 8U));
    } else if (root_partition == AV2H_PARTITION_VERT) {
        child_size = square_child ? AV2H_BLOCK_32X64 :
            decoder->root_block_size == AV2H_BLOCK_64X64 ?
            AV2H_BLOCK_32X64 :
            decoder->root_block_size == AV2H_BLOCK_128X128 ?
                AV2H_BLOCK_64X128 : AV2H_BLOCK_INVALID;
        second_column = (uint16_t)(second_column +
            (child_size == AV2H_BLOCK_64X128 ? 16U : 8U));
    } else {
        return;
    }
    if (child_size == AV2H_BLOCK_INVALID) return;
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        int is_first = decision->node.row == first_row &&
            decision->node.column == first_column;
        int is_second = decision->node.row == second_row &&
            decision->node.column == second_column;
        if (decision->node.block_size != child_size ||
            (!is_first && !is_second)) {
            continue;
        }
        ++*child_count;
        if (decision->partition == AV2H_PARTITION_NONE ||
            av2h_tile_partition_direction(decision->partition) ==
                av2h_tile_partition_direction(root_partition)) {
            *children_share = 0U;
        }
    }
}

static Av2hStatus tile_store_luma_partition_path(
    Av2hTileDecoder *decoder, const Av2hFirstLeafPath *path) {
    size_t index;
    if (decoder == 0 || path == 0) return AV2H_INVALID_ARGUMENT;
    for (index = 0U; index < path->decision_count; ++index) {
        size_t stored_index;
        for (stored_index = decoder->luma_partition_node_count;
             stored_index != 0U; --stored_index) {
            Av2hTileLumaPartitionNode *stored =
                &decoder->luma_partition_nodes[stored_index - 1U];
            if (stored->node.row == path->decisions[index].node.row &&
                stored->node.column == path->decisions[index].node.column &&
                stored->node.block_size ==
                    path->decisions[index].node.block_size) {
                if (stored->partition !=
                    (uint8_t)path->decisions[index].partition) {
                    return AV2H_INVALID_DATA;
                }
                break;
            }
        }
        if (stored_index != 0U) continue;
        if (decoder->luma_partition_node_count ==
            AV2H_MAX_LUMA_PARTITION_NODES) {
            return AV2H_OVERFLOW;
        }
        Av2hTileLumaPartitionNode *stored =
            &decoder->luma_partition_nodes[
                decoder->luma_partition_node_count++];
        stored->node = path->decisions[index].node;
        stored->partition = (uint8_t)path->decisions[index].partition;
    }
    return AV2H_OK;
}

static uint16_t restoration_recenter(uint16_t range, uint16_t reference,
                                     uint16_t value) {
    uint16_t mirrored_reference;
    if ((uint32_t)reference * 2U <= range) {
        if (value > (uint16_t)(reference * 2U)) return value;
        return (value & 1U) == 0U ?
            (uint16_t)(reference + value / 2U) :
            (uint16_t)(reference - (value + 1U) / 2U);
    }
    mirrored_reference = (uint16_t)(range - 1U - reference);
    return (uint16_t)(range - 1U - restoration_recenter(
        range, mirrored_reference, value));
}

static Av2hStatus restoration_read_four_part(
    Av2hRangeDecoder *decoder, uint16_t *cdf, uint8_t bits,
    uint16_t reference, uint16_t *value) {
    static const uint8_t bit_reductions[4U] = { 3U, 3U, 2U, 1U };
    uint32_t part;
    uint8_t literal_bits;
    uint16_t offset;
    uint16_t symbol;
    if (decoder == 0 || cdf == 0 || value == 0 || bits < 3U || bits > 15U ||
        reference >= ((uint16_t)1U << bits)) {
        return AV2H_INVALID_ARGUMENT;
    }
    part = av2h_range_read_adaptive(decoder, cdf, 4U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    literal_bits = (uint8_t)(bits - bit_reductions[part]);
    offset = part == 0U ? 0U :
             part == 1U ? (uint16_t)(1U << (bits - 3U)) :
             part == 2U ? (uint16_t)(1U << (bits - 2U)) :
                          (uint16_t)(1U << (bits - 1U));
    symbol = offset;
    if (literal_bits != 0U) {
        symbol = (uint16_t)(symbol + av2h_range_read_bypass(
            decoder, literal_bits));
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    *value = restoration_recenter(
        (uint16_t)(1U << bits), reference, symbol);
    return AV2H_OK;
}

static const int16_t *restoration_bank_reference(
    const Av2hRestorationEntropyState *state, unsigned int plane,
    uint8_t reference) {
    uint8_t slot;
    if (state->bank_size[plane] == 0U) return state->bank[plane][0U];
    slot = (uint8_t)((state->bank_pointer[plane] + 4U - reference) % 4U);
    return state->bank[plane][slot];
}

static void restoration_bank_add(Av2hRestorationEntropyState *state,
                                 unsigned int plane,
                                 const int16_t *taps) {
    uint8_t slot;
    if (state->bank_size[plane] < 4U) {
        slot = state->bank_size[plane]++;
    } else {
        slot = (uint8_t)((state->bank_pointer[plane] + 1U) % 4U);
    }
    state->bank_pointer[plane] = slot;
    av2h_memory_copy(state->bank[plane][slot], taps,
                     AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
}

static Av2hStatus tile_read_wiener_unit_filter(
    Av2hTileDecoder *decoder, unsigned int plane, int16_t *taps) {
    static const uint8_t coefficient_bits[AV2H_WIENER_NONSEP_MAX_TAPS] = {
        6U, 6U, 5U, 5U, 5U, 5U, 5U, 5U, 5U,
        5U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U
    };
    static const int16_t coefficient_minimums[
        AV2H_WIENER_NONSEP_MAX_TAPS] = {
        -24, -24, -14, -14, -16, -16, -16, -16, -16,
        -16, -8, -8, -8, -8, -8, -8, -8, -8
    };
    Av2hRestorationEntropyState *state = &decoder->restoration_entropy;
    const int16_t *reference_taps;
    uint32_t exact_match;
    uint8_t reference = 0U;
    uint8_t subset = 0U;
    uint8_t subset_taps;
    uint8_t symmetric = 1U;
    size_t index;
    Av2hStatus status;
    if (plane == 0U || taps == 0) return AV2H_INVALID_ARGUMENT;
    exact_match = av2h_range_read_bypass(&decoder->range, 1U);
    if (av2h_range_status(&decoder->range) != AV2H_OK) {
        return av2h_range_status(&decoder->range);
    }
    while (reference + 1U < state->bank_size[plane]) {
        uint32_t selected = av2h_range_read_bypass(&decoder->range, 1U);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_range_status(&decoder->range);
        }
        if (selected != 0U) break;
        ++reference;
    }
    reference_taps = restoration_bank_reference(state, plane, reference);
    if (exact_match != 0U) {
        av2h_memory_copy(taps, reference_taps,
                         AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
        if (state->bank_size[plane] == 0U) {
            restoration_bank_add(state, plane, taps);
        }
        return AV2H_OK;
    }
    av2h_memory_fill(taps, 0U,
                     AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
    while (subset < 2U) {
        uint32_t longer = av2h_range_read_adaptive(
            &decoder->range, state->length_cdf[1U], 2U);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_range_status(&decoder->range);
        }
        if (longer == 0U) break;
        ++subset;
    }
    subset_taps = subset == 0U ? 6U :
                  subset == 1U ? 10U : AV2H_WIENER_NONSEP_MAX_TAPS;
    if (subset != 0U) {
        symmetric = (uint8_t)av2h_range_read_adaptive(
            &decoder->range, state->uv_sym_cdf, 2U);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_range_status(&decoder->range);
        }
    }
    for (index = 0U; index < subset_taps; ++index) {
        uint16_t decoded;
        uint16_t centered_reference = (uint16_t)(
            reference_taps[index] - coefficient_minimums[index]);
        status = restoration_read_four_part(
            &decoder->range, state->four_part_cdf,
            coefficient_bits[index], centered_reference, &decoded);
        if (status != AV2H_OK) return status;
        taps[index] = (int16_t)(decoded + coefficient_minimums[index]);
        if (symmetric != 0U && index >= 6U) {
            if (index + 1U >= subset_taps) return AV2H_INVALID_DATA;
            taps[index + 1U] = taps[index];
            ++index;
        }
    }
    restoration_bank_add(state, plane, taps);
    return AV2H_OK;
}

static Av2hStatus tile_read_restoration_units(Av2hTileDecoder *decoder) {
    Av2hRestorationEntropyState *state = &decoder->restoration_entropy;
    unsigned int plane;
    if (state->active_mask == 0U) return AV2H_OK;
    if (state->superblock_mi == 0U) {
        return AV2H_INVALID_DATA;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        unsigned int shift;
        size_t pixel_row;
        size_t pixel_column;
        size_t superblock_pixels;
        size_t unit_row_start;
        size_t unit_row_end;
        size_t unit_column_start;
        size_t unit_column_end;
        size_t unit_row;
        if ((state->active_mask & (1U << plane)) == 0U) continue;
        if (state->unit_size[plane] == 0U ||
            state->unit_columns[plane] == 0U ||
            state->unit_rows[plane] == 0U) {
            return AV2H_INVALID_DATA;
        }
        shift = av2h_chroma_shift(decoder->frame->chroma_format_idc, plane);
        pixel_row = ((size_t)decoder->sb_row * 4U) >> shift;
        pixel_column = ((size_t)decoder->sb_column * 4U) >> shift;
        superblock_pixels = ((size_t)state->superblock_mi * 4U) >> shift;
        unit_row_start = (pixel_row + state->unit_size[plane] - 1U) /
            state->unit_size[plane];
        unit_row_end = (pixel_row + superblock_pixels +
            state->unit_size[plane] - 1U) / state->unit_size[plane];
        unit_column_start = (pixel_column + state->unit_size[plane] - 1U) /
            state->unit_size[plane];
        unit_column_end = (pixel_column + superblock_pixels +
            state->unit_size[plane] - 1U) / state->unit_size[plane];
        if (unit_row_end > state->unit_rows[plane]) {
            unit_row_end = state->unit_rows[plane];
        }
        if (unit_column_end > state->unit_columns[plane]) {
            unit_column_end = state->unit_columns[plane];
        }
        for (unit_row = unit_row_start; unit_row < unit_row_end; ++unit_row) {
            size_t unit_column;
            for (unit_column = unit_column_start;
                 unit_column < unit_column_end; ++unit_column) {
            size_t unit_index = unit_row * state->unit_columns[plane] +
                unit_column;
            uint32_t value;
            int16_t *taps;
            Av2hStatus status;
            if (unit_index >= state->unit_capacity[plane] ||
                unit_index != state->decoded_count[plane]) {
                return AV2H_INVALID_DATA;
            }
            value = av2h_range_read_adaptive(
                &decoder->range,
                decoder->frame->restoration_type[plane] ==
                        AV2H_RESTORE_PC_WIENER ?
                    state->pc_wiener_restore_cdf : state->restore_cdf,
                2U);
            if (av2h_range_status(&decoder->range) != AV2H_OK) {
                return av2h_range_status(&decoder->range);
            }
            if (value > 1U) return AV2H_INVALID_DATA;
            state->unit_flags[plane][unit_index] = (uint8_t)value;
            taps = state->unit_taps[plane] +
                unit_index * AV2H_WIENER_NONSEP_MAX_TAPS;
            if (value != 0U) {
                if (decoder->frame->restoration_type[plane] ==
                    AV2H_RESTORE_PC_WIENER) {
                    av2h_memory_fill(
                        taps, 0U,
                        AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
                } else if (
                    decoder->frame->restoration_frame_filters_on[plane]) {
                    av2h_memory_copy(
                        taps, decoder->frame->restoration_frame_filter[plane],
                        AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
                } else {
                    status = tile_read_wiener_unit_filter(
                        decoder, plane, taps);
                    if (status != AV2H_OK) return status;
                }
            } else {
                av2h_memory_fill(
                    taps, 0U,
                    AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
            }
            ++state->decoded_count[plane];
            }
        }
    }
    return AV2H_OK;
}

static int luma_geometry_supported(const Av2hPartitionNode *leaf) {
    const Av2hTxDescriptor *descriptor;
    return leaf != 0 && av2h_luma_tx_descriptor_get(
        leaf->block_size, &descriptor) == AV2H_OK;
}

static Av2hBlockSize tile_tx_block_size(Av2hFirstTxSize tx_size) {
    static const Av2hBlockSize block_sizes[AV2H_TX_SIZE_COUNT] = {
        AV2H_BLOCK_4X4, AV2H_BLOCK_8X8, AV2H_BLOCK_16X16,
        AV2H_BLOCK_32X32, AV2H_BLOCK_64X64, AV2H_BLOCK_4X8,
        AV2H_BLOCK_8X4, AV2H_BLOCK_8X16, AV2H_BLOCK_16X8,
        AV2H_BLOCK_16X32, AV2H_BLOCK_32X16, AV2H_BLOCK_32X64,
        AV2H_BLOCK_64X32, AV2H_BLOCK_4X16, AV2H_BLOCK_16X4,
        AV2H_BLOCK_8X32, AV2H_BLOCK_32X8, AV2H_BLOCK_16X64,
        AV2H_BLOCK_64X16, AV2H_BLOCK_4X32, AV2H_BLOCK_32X4,
        AV2H_BLOCK_8X64, AV2H_BLOCK_64X8,
        AV2H_BLOCK_4X64, AV2H_BLOCK_64X4
    };
    return (unsigned int)tx_size < AV2H_TX_SIZE_COUNT ?
        block_sizes[(unsigned int)tx_size] : AV2H_BLOCK_INVALID;
}

static Av2hStatus tile_tx_size_from_dimensions(
    uint8_t width, uint8_t height, Av2hFirstTxSize *tx_size) {
    unsigned int index;
    if (tx_size == 0) return AV2H_INVALID_ARGUMENT;
    for (index = 0U; index < AV2H_TX_SIZE_COUNT; ++index) {
        const Av2hTxDescriptor *descriptor;
        if (av2h_tx_descriptor_get(
                (Av2hFirstTxSize)index, &descriptor) == AV2H_OK &&
            descriptor->output_width == width &&
            descriptor->output_height == height) {
            *tx_size = (Av2hFirstTxSize)index;
            return AV2H_OK;
        }
    }
    return AV2H_UNSUPPORTED;
}

static Av2hStatus tile_read_luma_tx_partition(
    Av2hTileDecoder *decoder, const Av2hPartitionNode *coding_node,
    const Av2hFirstLeafMode *mode) {
    static const uint8_t split_group[AV2H_BLOCK_SIZE_COUNT] = {
        0U, 0U, 0U, 1U, 2U, 2U, 3U, 4U, 4U, 5U,
        6U, 6U, 7U, 7U, 7U, 7U, 7U, 7U, 7U, 8U,
        8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U
    };
    static const uint8_t both_group[AV2H_BLOCK_SIZE_COUNT] = {
        255U, 255U, 255U, 0U, 1U, 2U, 3U, 4U, 5U, 6U,
        7U, 8U, 9U, 9U, 9U, 9U, 9U, 9U, 9U, 255U,
        255U, 10U, 11U, 12U, 13U, 255U, 255U, 10U, 11U
    };
    static const uint8_t partition_count[8U] = {
        1U, 4U, 2U, 2U, 4U, 4U, 5U, 5U
    };
    static const uint8_t row_shift[8U][5U] = {
        { 0U }, { 1U, 1U, 1U, 1U }, { 1U, 1U }, { 0U, 0U },
        { 2U, 2U, 2U, 2U }, { 0U, 0U, 0U, 0U },
        { 2U, 2U, 1U, 2U, 2U }, { 1U, 1U, 0U, 1U, 1U }
    };
    static const uint8_t column_shift[8U][5U] = {
        { 0U }, { 1U, 1U, 1U, 1U }, { 0U, 0U }, { 1U, 1U },
        { 0U, 0U, 0U, 0U }, { 2U, 2U, 2U, 2U },
        { 1U, 1U, 0U, 1U, 1U }, { 2U, 2U, 1U, 2U, 2U }
    };
    static const uint8_t row_offset[8U][5U] = {
        { 0U }, { 0U, 0U, 1U, 1U }, { 0U, 1U }, { 0U, 0U },
        { 0U, 1U, 2U, 3U }, { 0U, 0U, 0U, 0U },
        { 0U, 0U, 1U, 3U, 3U }, { 0U, 1U, 0U, 0U, 1U }
    };
    static const uint8_t column_offset[8U][5U] = {
        { 0U }, { 0U, 1U, 0U, 1U }, { 0U, 0U }, { 0U, 1U },
        { 0U, 0U, 0U, 0U }, { 0U, 1U, 2U, 3U },
        { 0U, 1U, 0U, 0U, 1U }, { 0U, 0U, 1U, 3U, 3U }
    };
    const Av2hTxDescriptor *maximum;
    Av2hFirstTxSize candidate;
    uint8_t allow_horizontal;
    uint8_t allow_vertical;
    uint8_t partition = 0U;
    uint8_t count;
    uint8_t selected_count = 0U;
    uint8_t width_step;
    uint8_t height_step;
    size_t index;
    uint32_t symbol;
    Av2hStatus status;
    if (decoder == 0 || coding_node == 0 || mode == 0 ||
        coding_node->block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    decoder->luma_selected_tx_count = 0U;
    decoder->luma_tx_partition = 0U;
    if (decoder->frame->tx_mode != 2U || mode->skip_transform != 0U ||
        av2h_quantizer_is_lossless(
            decoder->sequence, decoder->frame,
            decoder->current_block_q_index)) {
        return AV2H_OK;
    }
    if (coding_node->block_size >= AV2H_BLOCK_64X128 &&
        coding_node->block_size <= AV2H_BLOCK_256X256) {
        return AV2H_OK;
    }
    status = av2h_luma_tx_descriptor_get(coding_node->block_size, &maximum);
    if (status != AV2H_OK) return status;
    allow_horizontal = (uint8_t)(maximum->output_height >= 8U &&
        tile_tx_size_from_dimensions(maximum->output_width,
            (uint8_t)(maximum->output_height / 2U), &candidate) == AV2H_OK);
    allow_vertical = (uint8_t)(maximum->output_width >= 8U &&
        tile_tx_size_from_dimensions((uint8_t)(maximum->output_width / 2U),
            maximum->output_height, &candidate) == AV2H_OK);
    if (!allow_horizontal && !allow_vertical) return AV2H_OK;
    symbol = av2h_range_read_adaptive(
        &decoder->range,
        decoder->tx_partition_cdfs.do_partition[
            mode->fsc_mode != 0U][
            mode->use_intrabc != 0U][
            split_group[coding_node->block_size]], 2U);
    if (av2h_range_status(&decoder->range) != AV2H_OK) {
        return av2h_range_status(&decoder->range);
    }
    if (symbol == 0U) return AV2H_OK;
    if (mode->fsc_mode != 0U) return AV2H_UNSUPPORTED;
    if (allow_horizontal && allow_vertical) {
        uint8_t group = both_group[coding_node->block_size];
        if (group >= AV2H_TX_PARTITION_TYPE_GROUPS ||
            decoder->sequence->reduced_tx_partition_set) {
            return AV2H_UNSUPPORTED;
        }
        symbol = av2h_range_read_adaptive(
            &decoder->range,
            decoder->tx_partition_cdfs.partition_type[group],
            AV2H_TX_PARTITION_TYPE_SYMBOLS);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_range_status(&decoder->range);
        }
        partition = (uint8_t)(symbol + 1U);
    } else {
        uint8_t fixed_two_way = (uint8_t)(
            coding_node->block_size == AV2H_BLOCK_4X8 ||
            coding_node->block_size == AV2H_BLOCK_8X4);
        uint8_t context = coding_node->block_size == AV2H_BLOCK_4X16 ||
                                  coding_node->block_size == AV2H_BLOCK_4X32 ?
                              0U : 1U;
        symbol = 0U;
        if (!fixed_two_way &&
            !decoder->sequence->reduced_tx_partition_set) {
            symbol = av2h_range_read_adaptive(
                &decoder->range,
                decoder->tx_partition_cdfs.two_or_three_way[
                    mode->use_intrabc != 0U][context],
                2U);
        }
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_range_status(&decoder->range);
        }
        partition = allow_horizontal ?
            (uint8_t)(symbol == 0U ? 2U : 4U) :
            (uint8_t)(symbol == 0U ? 3U : 5U);
    }
    count = partition_count[partition];
    width_step = (uint8_t)(maximum->output_width / 8U);
    height_step = (uint8_t)(maximum->output_height / 8U);
    if (partition == 4U || partition == 5U) {
        width_step = (uint8_t)(width_step / 2U);
        height_step = (uint8_t)(height_step / 2U);
    }
    if (partition == 6U) height_step = (uint8_t)(height_step / 2U);
    if (partition == 7U) width_step = (uint8_t)(width_step / 2U);
    for (index = 0U; index < count; ++index) {
        Av2hFirstTxSize tx_size;
        Av2hBlockSize block_size;
        uint8_t width = (uint8_t)(
            maximum->output_width >> column_shift[partition][index]);
        uint8_t height = (uint8_t)(
            maximum->output_height >> row_shift[partition][index]);
        status = tile_tx_size_from_dimensions(width, height, &tx_size);
        if (status != AV2H_OK) return status;
        block_size = tile_tx_block_size(tx_size);
        if (block_size == AV2H_BLOCK_INVALID) return AV2H_UNSUPPORTED;
        Av2hPartitionNode node = *coding_node;
        node.row = (uint16_t)(coding_node->row +
            row_offset[partition][index] * height_step);
        node.column = (uint16_t)(
            coding_node->column +
            column_offset[partition][index] * width_step);
        node.block_size = block_size;
        if (node.row >= decoder->mi_rows ||
            node.column >= decoder->mi_columns) {
            continue;
        }
        decoder->luma_tx_nodes[selected_count++] = node;
    }
    decoder->luma_tx_partition = partition;
    decoder->luma_selected_tx_count = selected_count;
    return AV2H_OK;
}

static Av2hStatus tile_handoff_square_child(Av2hTileDecoder *decoder,
                                             uint8_t *has_leaf) {
    Av2hPartitionNode chroma_root;
    const Av2hPartitionNode *pending = 0;
    size_t pending_index;
    uint8_t next;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (decoder->sb_row == 0U && decoder->sb_column == 480U &&
        decoder->square_root_child_index == 1U) {
        av2h_partition_diagnostic_leaf_stage(
            (Av2hPartitionNode){ 0U, 496U, AV2H_BLOCK_64X64 }, 90U,
            (uint8_t)decoder->luma_root_partition,
            (uint16_t)decoder->luma_traversal.stack_count,
            av2h_range_tell(&decoder->range));
    }
#endif
    if (!decoder->sequence->enable_sdp ||
        decoder->root_block_size != AV2H_BLOCK_128X128 ||
        decoder->square_child_luma_partition == AV2H_PARTITION_INVALID ||
        decoder->square_root_child_index >= 4U) {
        return AV2H_OK;
    }
    pending_index = decoder->luma_traversal.stack_count;
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
        next = (uint8_t)((pending->row >= decoder->sb_row + 16U ? 2U : 0U) +
                         (pending->column >= decoder->sb_column + 16U ?
                              1U : 0U));
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        if (decoder->sb_row == 0U && decoder->sb_column == 480U &&
            decoder->square_root_child_index == 1U) {
            av2h_partition_diagnostic_leaf_stage(
                (Av2hPartitionNode){ 0U, 496U, AV2H_BLOCK_64X64 }, 91U,
                (uint8_t)pending->row, pending->column,
                av2h_range_tell(&decoder->range));
        }
#endif
        if (next == decoder->square_root_child_index) {
            return AV2H_OK;
        }
    }
    chroma_root.row = (uint16_t)(decoder->sb_row +
        (decoder->square_root_child_index >= 2U ? 16U : 0U));
    chroma_root.column = (uint16_t)(decoder->sb_column +
        ((decoder->square_root_child_index & 1U) != 0U ? 16U : 0U));
    chroma_root.block_size = AV2H_BLOCK_64X64;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (chroma_root.row == 0U && chroma_root.column == 496U) {
        av2h_partition_diagnostic_leaf_stage(
            chroma_root, 100U,
            (uint8_t)decoder->square_child_luma_partition,
            decoder->square_child_luma_child_count,
            av2h_range_tell(&decoder->range));
    }
#endif
    if (av2h_partition_traversal_reset_root(
            &decoder->chroma_traversal, chroma_root) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    decoder->chroma_root_partition = AV2H_PARTITION_INVALID;
    decoder->chroma_root_forced_partition = AV2H_PARTITION_INVALID;
    if (decoder->square_child_luma_partition == AV2H_PARTITION_NONE) {
        decoder->chroma_root_forced_partition = AV2H_PARTITION_NONE;
    } else if (decoder->square_child_luma_child_count == 2U &&
               decoder->square_child_luma_children_share &&
               av2h_tile_partition_direction(
                   decoder->square_child_luma_partition) != 0U) {
        decoder->chroma_root_forced_partition =
            decoder->square_child_luma_partition;
    }
    decoder->square_child_luma_child_count = 0U;
    decoder->square_child_luma_children_share = 1U;
    decoder->phase = AV2H_TILE_DECODE_CHROMA;
    decoder->event = AV2H_TILE_EVENT_ROOT_HANDOFF;
    *has_leaf = 0U;
    return AV2H_OK;
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
        leaf->mode.skip_transform) {
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
            cell->context.segment_id = mode->segment_id;
            cell->context.q_index = mode->q_index;
            cell->context.use_intrabc = mode->use_intrabc;
            cell->context.intrabc_mode = mode->intrabc_mode;
            cell->context.intrabc_drl_index = mode->intrabc_drl_index;
            cell->context.mv_precision = mode->mv_precision;
            cell->context.displacement_x = mode->displacement_x;
            cell->context.displacement_y = mode->displacement_y;
            cell->context.owner_row = node->row;
            cell->context.owner_column = node->column;
            cell->context.skip_transform = mode->skip_transform;
            cell->context.joint_mode = mode->joint_mode;
            cell->context.fsc_mode = mode->fsc_mode;
            cell->context.mrl_index = mode->mrl_index;
            cell->context.use_intra_dip = mode->use_intra_dip;
            cell->context.multi_line_mrl = mode->multi_line_mrl;
            cell->mode = (uint8_t)mode->mode;
            cell->angle_delta = mode->angle_delta;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_next_luma(
    Av2hTileDecoder *decoder,
    Av2hTileLumaLeaf *leaf,
    Av2hFirstCoeffBlock *block,
    uint8_t *has_leaf) {
    Av2hFirstLeafPath path;
    uint8_t dc_sign_context;
    uint16_t tx_count;
    Av2hStatus status;
    if (decoder == 0 || leaf == 0 || block == 0 || has_leaf == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (decoder->status != AV2H_OK) return decoder->status;
    if (decoder->phase != AV2H_TILE_DECODE_LUMA) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->luma_tx_count != 0U) {
        if (decoder->luma_tx_index >= decoder->luma_tx_count) {
            return av2h_tile_decoder_fail(
                decoder, AV2H_INVALID_ARGUMENT);
        }
        av2h_memory_copy(
            &leaf->mode, &decoder->luma_coding_mode,
            sizeof(leaf->mode));
        leaf->mode.tx_extended_edges_unavailable = (uint8_t)(
            (decoder->luma_tx_partition == 6U ||
             decoder->luma_tx_partition == 7U) &&
            decoder->luma_tx_index >= 1U);
        status = decoder->frame->base_q_index == 0U &&
            decoder->luma_coding_mode.lossless_tx_size_large == 0U ?
            av2h_luma_lossless_tx_node_get(
                    &decoder->luma_coding_node, decoder->mi_columns,
                    decoder->mi_rows, decoder->luma_tx_index, &leaf->node) :
            av2h_tile_luma_tx_node_get(
                decoder, decoder->luma_tx_index, &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
        *has_leaf = 1U;
        goto read_transform;
    }
    av2h_tile_diagnostics_set_luma_stage(0U);
    status = tile_handoff_square_child(decoder, has_leaf);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (decoder->phase == AV2H_TILE_DECODE_CHROMA) return AV2H_OK;
    if (decoder->luma_partition_node_count == 0U) {
        status = tile_read_restoration_units(decoder);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    }
    status = av2h_partition_traversal_next(
        &decoder->luma_traversal, &decoder->range,
        &decoder->partition_cdfs, &decoder->partition_contexts, &path,
        has_leaf);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_partition_path(decoder, &path);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (!*has_leaf) {
        return av2h_tile_decoder_finish_root(decoder);
    }
    av2h_tile_capture_root_partition(
        decoder, &path, &decoder->luma_root_partition);
    tile_capture_luma_root_child(decoder, &path);
    leaf->node = path.leaf;
    av2h_tile_diagnostics_set_luma_range(0U, decoder->range.range);
    if (!luma_geometry_supported(&leaf->node)) {
        return av2h_tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
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
    av2h_tile_diagnostics_set_luma_stage(1U);
    status = av2h_luma_mode_read_with_state_and_delta_q(
        &decoder->range, &decoder->leaf_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &decoder->luma_mode_contexts,
        &decoder->gdf_entropy, &decoder->cdef_entropy,
        &decoder->ccso_entropy, &decoder->current_q_index, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    decoder->current_block_q_index = leaf->mode.q_index;
    status = av2h_luma_palette_map_read(
        &decoder->range, &decoder->leaf_cdfs, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (decoder->frame->base_q_index == 0U &&
        leaf->mode.skip_transform == 0U &&
        (leaf->mode.fsc_mode != 0U || leaf->mode.use_intrabc != 0U) &&
        leaf->node.block_size != AV2H_BLOCK_4X4) {
        static const uint8_t size_group[AV2H_BLOCK_SIZE_COUNT] = {
            0U, 0U, 0U, 1U, 1U, 1U, 2U, 2U, 2U, 3U,
            3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U, 0U,
            0U, 1U, 1U, 2U, 2U, 1U, 1U, 2U, 2U
        };
        uint8_t group = size_group[leaf->node.block_size];
        leaf->mode.lossless_tx_size_large = (uint8_t)
            av2h_range_read_adaptive(
                &decoder->range,
                decoder->leaf_cdfs.lossless_tx_size[group]
                    [leaf->mode.use_intrabc != 0U], 2U);
        if (av2h_range_status(&decoder->range) != AV2H_OK) {
            return av2h_tile_decoder_fail(
                decoder, av2h_range_status(&decoder->range));
        }
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 60U, leaf->mode.gdf_flags_read,
        leaf->mode.cdef_strengths_read, leaf->mode.after_gdf_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 61U, leaf->mode.ccso_controls_read,
        leaf->mode.mode_index, leaf->mode.after_ccso_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 62U, leaf->mode.mode_set,
        leaf->mode.mode_index, leaf->mode.after_mode_index_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 63U, leaf->mode.fsc_mode,
        leaf->mode.mrl_index, leaf->mode.after_mrl_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 1U, (uint8_t)leaf->mode.mode, 0U,
        av2h_range_tell(&decoder->range));
#endif
    av2h_tile_diagnostics_set_luma_range(1U, decoder->range.range);
    status = av2h_luma_mode_context_state_update(
        &decoder->luma_mode_contexts, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return av2h_tile_decoder_fail(decoder, AV2H_OVERFLOW);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_mode(decoder, &leaf->node, &leaf->mode);
    if (status == AV2H_UNSUPPORTED) return av2h_tile_decoder_fail(decoder, AV2H_OUT_OF_MEMORY);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        if (decoder->sequence->enable_sdp &&
                ((leaf->node.block_size == AV2H_BLOCK_128X128 &&
                    decoder->luma_root_partition == AV2H_PARTITION_NONE) ||
                 ((leaf->node.block_size == AV2H_BLOCK_64X128 ||
                     leaf->node.block_size == AV2H_BLOCK_128X64) &&
                    av2h_tile_partition_direction(
                            decoder->luma_root_partition) != 0U))) {
        status = av2h_tile_decoder_read_shared_chroma_mode(
            decoder, &leaf->node);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, 70U, decoder->shared_chroma_mode.mode_index,
            decoder->shared_chroma_mode.cfl_kind,
            av2h_range_tell(&decoder->range));
#endif
    }
    status = tile_read_luma_tx_partition(decoder, &leaf->node, &leaf->mode);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    status = decoder->luma_selected_tx_count != 0U ?
        (tx_count = decoder->luma_selected_tx_count, AV2H_OK) :
        decoder->frame->base_q_index == 0U &&
            leaf->mode.lossless_tx_size_large == 0U ?
        av2h_luma_lossless_tx_node_count(
            &leaf->node, decoder->mi_columns, decoder->mi_rows, &tx_count) :
        av2h_luma_tx_node_count(leaf->node.block_size, &tx_count);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    if (tx_count > 1U) {
        decoder->luma_coding_node = leaf->node;
        av2h_memory_copy(
            &decoder->luma_coding_mode, &leaf->mode,
            sizeof(decoder->luma_coding_mode));
        decoder->luma_tx_index = 0U;
        decoder->luma_tx_count = tx_count;
        status = decoder->frame->base_q_index == 0U &&
            decoder->luma_coding_mode.lossless_tx_size_large == 0U ?
            av2h_luma_lossless_tx_node_get(
                &decoder->luma_coding_node, decoder->mi_columns,
                decoder->mi_rows, 0U, &leaf->node) :
            av2h_tile_luma_tx_node_get(decoder, 0U, &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
        leaf->mode.tx_extended_edges_unavailable = (uint8_t)(
            (decoder->luma_tx_partition == 6U ||
             decoder->luma_tx_partition == 7U) &&
            decoder->luma_tx_index >= 1U);
    }
read_transform:
    if (leaf->mode.skip_transform != 0U) {
        status = tile_initialize_skipped_intrabc(
            decoder, leaf, &leaf->prefix, &leaf->transform, block);
        if (status == AV2H_UNSUPPORTED) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
        }
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        leaf->prefix.q_index = decoder->current_block_q_index;
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(2U);
    if (decoder->luma_tx_count != 0U) {
        uint8_t txb_skip_context;
        status = av2h_coeff_contexts_from_state_for_coding_block(
            &decoder->coefficient_contexts, 0U, &leaf->node,
            decoder->luma_coding_node.block_size, leaf->mode.fsc_mode,
            &txb_skip_context, &dc_sign_context);
        if (status == AV2H_OK) {
            status = av2h_coeff_prefix_read(
                &decoder->range, &decoder->coeff_cdfs, decoder->frame,
                &leaf->node, &leaf->mode, txb_skip_context, &leaf->prefix);
        }
    } else {
        status = av2h_coeff_prefix_read_with_state(
            &decoder->range, &decoder->coeff_cdfs, decoder->frame,
            &leaf->node, &leaf->mode, &decoder->coefficient_contexts,
            &leaf->prefix, &dc_sign_context);
    }
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
    leaf->prefix.q_index = decoder->current_block_q_index;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 21U, leaf->prefix.eob_point, leaf->prefix.eob_extra,
        leaf->prefix.after_eob_point_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 22U, leaf->prefix.eob_offset_bits,
        leaf->prefix.eob_extra, leaf->prefix.after_eob_extra_bit);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 2U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        av2h_range_tell(&decoder->range));
#endif
    av2h_tile_diagnostics_set_luma_range(2U, decoder->range.range);
    if (leaf->prefix.all_zero) {
        status = tile_initialize_all_zero_luma(
            decoder, leaf, &leaf->transform, block);
        if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
        decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(3U);
    if (decoder->current_block_q_index == 0U) {
        size_t tell = av2h_range_tell(&decoder->range);
        size_t before_primary = tell;
        uint8_t lossless_inter_tx_type = 0U;
        if (leaf->mode.use_intrabc != 0U &&
            leaf->prefix.tx_size == AV2H_TX_4X4) {
            lossless_inter_tx_type = (uint8_t)av2h_range_read_adaptive(
                &decoder->range,
                decoder->leaf_cdfs.lossless_inter_tx_type, 2U);
            if (av2h_range_status(&decoder->range) != AV2H_OK) {
                return av2h_tile_decoder_fail(
                    decoder, av2h_range_status(&decoder->range));
            }
            tell = av2h_range_tell(&decoder->range);
        }
        av2h_memory_fill(&leaf->transform, 0U, sizeof(leaf->transform));
        leaf->transform.primary_type = leaf->mode.fsc_mode != 0U ||
            (leaf->mode.use_intrabc != 0U &&
             (lossless_inter_tx_type != 0U ||
              leaf->prefix.tx_size != AV2H_TX_4X4)) ?
                AV2H_TX_IDTX : AV2H_TX_DCT_DCT;
        leaf->transform.before_primary_bits = before_primary;
        leaf->transform.after_primary_bits = tell;
        leaf->transform.after_secondary_type_bits = tell;
        leaf->transform.after_secondary_bits = tell;
        status = AV2H_OK;
    } else {
        status = av2h_first_transform_type_read(
            &decoder->range, &decoder->transform_cdfs, decoder->sequence,
            decoder->frame, &leaf->mode, &leaf->prefix, &leaf->transform);
    }
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 3U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        av2h_range_tell(&decoder->range));
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        av2h_partition_diagnostic_leaf_stage(
            leaf->node, 89U, leaf->transform.primary_type,
            leaf->transform.secondary_symbol, leaf->prefix.tx_size);
    }
#endif
    av2h_tile_diagnostics_set_luma_range(3U, decoder->range.range);
    av2h_tile_diagnostics_set_luma_stage(4U);
    status = av2h_coeff_block_read_with_state(
        &decoder->range, &decoder->level_cdfs, decoder->sequence,
        decoder->frame, &leaf->node, &leaf->mode, &leaf->prefix,
        &leaf->transform, &decoder->coefficient_contexts, block);
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        size_t coefficient_index;
        for (coefficient_index = 0U; coefficient_index < 8U;
             ++coefficient_index) {
            av2h_partition_diagnostic_leaf_stage(
                leaf->node, (uint8_t)(80U + coefficient_index),
                (uint8_t)block->quantized[coefficient_index],
                (uint16_t)block->dequantized[coefficient_index],
                coefficient_index);
        }
    }
    if (leaf->node.row == 184U && leaf->node.column == 280U &&
        leaf->node.block_size == AV2H_BLOCK_32X32) {
        uint16_t scan_index = leaf->prefix.eob;
        while (scan_index != 0U) {
            uint16_t position;
            --scan_index;
            position = av2h_coeff_scan_position(
                scan_index, block->width, block->height, 0U, 0U);
            av2h_partition_diagnostic_leaf_stage(
                leaf->node, 40U, block->base_levels[position], position,
                scan_index);
        }
    }
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 31U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        block->after_levels_bits);
    av2h_partition_diagnostic_leaf_stage(
        leaf->node, 4U, (uint8_t)leaf->mode.mode, leaf->prefix.eob,
        av2h_range_tell(&decoder->range));
#endif
    av2h_tile_diagnostics_set_luma_range(4U, decoder->range.range);
    decoder->event = AV2H_TILE_EVENT_LUMA_LEAF;
    return AV2H_OK;
}

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
    if (status != AV2H_OK) return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        size_t residual_index;
        for (residual_index = 0U; residual_index < 8U; ++residual_index) {
            av2h_partition_diagnostic_leaf_stage(
                leaf->node, (uint8_t)(90U + residual_index), 0U,
                (uint16_t)scratch->residual[residual_index], residual_index);
        }
    }
#endif
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}