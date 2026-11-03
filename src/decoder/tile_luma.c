#include "tile.h"

#include "internal/coeff_modules.h"
#include "internal/decode_diagnostics.h"
#include "internal/tile_modules.h"

#include "internal/wiener_nonsep.h"
#include "tx_geometry.h"

#include <stddef.h>

static void tile_capture_luma_root_child(Av2hTileDecoder *decoder, const Av2hLeafPath *path) {
    Av2hBlockSize child_size;
    uint16_t first_row = decoder->superblock.sb_row;
    uint16_t first_column = decoder->superblock.sb_column;
    uint16_t second_row = decoder->superblock.sb_row;
    uint16_t second_column = decoder->superblock.sb_column;
    Av2hPartitionType root_partition = decoder->root.luma_root_partition;
    uint8_t *child_count = &decoder->root.luma_root_child_count;
    uint8_t *children_share = &decoder->root.luma_root_children_share;
    uint8_t square_child = 0U;
    size_t index;
    if (decoder->root.root_block_size == AV2H_BLOCK_128X128 &&
        av2h_tile_partition_direction(decoder->root.luma_root_partition) != 0U) {
        for (index = 0U; index < path->decision_count; ++index) {
            const Av2hPartitionDecision *decision = &path->decisions[index];
            if (decision->node.block_size == AV2H_BLOCK_64X64 &&
                decision->node.row >= decoder->superblock.sb_row &&
                decision->node.row < decoder->superblock.sb_row + 32U &&
                decision->node.column >= decoder->superblock.sb_column &&
                decision->node.column < decoder->superblock.sb_column + 32U) {
                decoder->root.square_root_child_index =
                    (uint8_t)((decision->node.row >= decoder->superblock.sb_row + 16U ? 2U : 0U) +
                              (decision->node.column >= decoder->superblock.sb_column + 16U ? 1U
                                                                                            : 0U));
                break;
            }
        }
    }
    if (decoder->root.root_block_size == AV2H_BLOCK_128X128 &&
        decoder->root.square_root_child_index < 4U) {
        uint16_t root_row = (uint16_t)(decoder->superblock.sb_row +
                                       (decoder->root.square_root_child_index >= 2U ? 16U : 0U));
        uint16_t root_column =
            (uint16_t)(decoder->superblock.sb_column +
                       ((decoder->root.square_root_child_index & 1U) != 0U ? 16U : 0U));
        for (index = 0U; index < path->decision_count; ++index) {
            const Av2hPartitionDecision *decision = &path->decisions[index];
            if (decision->node.row == root_row && decision->node.column == root_column &&
                decision->node.block_size == AV2H_BLOCK_64X64) {
                decoder->root.square_child_luma_partition = decision->partition;
                break;
            }
        }
        if (decoder->root.square_child_luma_partition != AV2H_PARTITION_INVALID) {
            root_partition = decoder->root.square_child_luma_partition;
            first_row = root_row;
            first_column = root_column;
            second_row = root_row;
            second_column = root_column;
            child_count = &decoder->root.square_child_luma_child_count;
            children_share = &decoder->root.square_child_luma_children_share;
            square_child = 1U;
        }
    }
    if (root_partition == AV2H_PARTITION_HORZ) {
        child_size = square_child                                          ? AV2H_BLOCK_64X32
                     : decoder->root.root_block_size == AV2H_BLOCK_64X64   ? AV2H_BLOCK_64X32
                     : decoder->root.root_block_size == AV2H_BLOCK_128X128 ? AV2H_BLOCK_128X64
                                                                           : AV2H_BLOCK_INVALID;
        second_row = (uint16_t)(second_row + (child_size == AV2H_BLOCK_128X64 ? 16U : 8U));
    } else if (root_partition == AV2H_PARTITION_VERT) {
        child_size = square_child                                          ? AV2H_BLOCK_32X64
                     : decoder->root.root_block_size == AV2H_BLOCK_64X64   ? AV2H_BLOCK_32X64
                     : decoder->root.root_block_size == AV2H_BLOCK_128X128 ? AV2H_BLOCK_64X128
                                                                           : AV2H_BLOCK_INVALID;
        second_column = (uint16_t)(second_column + (child_size == AV2H_BLOCK_64X128 ? 16U : 8U));
    } else {
        return;
    }
    if (child_size == AV2H_BLOCK_INVALID)
        return;
    for (index = 0U; index < path->decision_count; ++index) {
        const Av2hPartitionDecision *decision = &path->decisions[index];
        int is_first = decision->node.row == first_row && decision->node.column == first_column;
        int is_second = decision->node.row == second_row && decision->node.column == second_column;
        if (decision->node.block_size != child_size || (!is_first && !is_second)) {
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

static Av2hStatus tile_store_luma_partition_path(Av2hTileDecoder *decoder,
                                                 const Av2hLeafPath *path) {
    size_t index;
    if (decoder == NULL || path == NULL)
        return AV2H_INVALID_ARGUMENT;
    for (index = 0U; index < path->decision_count; ++index) {
        size_t stored_index;
        for (stored_index = decoder->traversal.luma_partition_node_count; stored_index != 0U;
             --stored_index) {
            Av2hTileLumaPartitionNode *stored =
                &decoder->traversal.luma_partition_nodes[stored_index - 1U];
            if (stored->node.row == path->decisions[index].node.row &&
                stored->node.column == path->decisions[index].node.column &&
                stored->node.block_size == path->decisions[index].node.block_size) {
                if (stored->partition != (uint8_t)path->decisions[index].partition) {
                    return AV2H_INVALID_DATA;
                }
                break;
            }
        }
        if (stored_index != 0U)
            continue;
        if (decoder->traversal.luma_partition_node_count == AV2H_MAX_LUMA_PARTITION_NODES) {
            return AV2H_OVERFLOW;
        }
        Av2hTileLumaPartitionNode *stored =
            &decoder->traversal
                 .luma_partition_nodes[decoder->traversal.luma_partition_node_count++];
        stored->node = path->decisions[index].node;
        stored->partition = (uint8_t)path->decisions[index].partition;
    }
    return AV2H_OK;
}

static uint16_t restoration_recenter(uint16_t range, uint16_t reference, uint16_t value) {
    uint16_t mirrored_reference;
    if ((uint32_t)reference * 2U <= range) {
        if (value > (uint16_t)(reference * 2U))
            return value;
        return (value & 1U) == 0U ? (uint16_t)(reference + value / 2U)
                                  : (uint16_t)(reference - (value + 1U) / 2U);
    }
    mirrored_reference = (uint16_t)(range - 1U - reference);
    return (uint16_t)(range - 1U - restoration_recenter(range, mirrored_reference, value));
}

static Av2hStatus restoration_read_four_part(Av2hRangeDecoder *decoder, uint16_t *cdf, uint8_t bits,
                                             uint16_t reference, uint16_t *value) {
    static const uint8_t bit_reductions[4U] = {3U, 3U, 2U, 1U};
    uint32_t part;
    uint8_t literal_bits;
    uint16_t offset;
    uint16_t symbol;
    if (decoder == NULL || cdf == NULL || value == NULL || bits < 3U || bits > 15U ||
        reference >= ((uint16_t)1U << bits)) {
        return AV2H_INVALID_ARGUMENT;
    }
    part = av2h_range_read_adaptive(decoder, cdf, 4U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    literal_bits = (uint8_t)(bits - bit_reductions[part]);
    offset = part == 0U   ? 0U
             : part == 1U ? (uint16_t)(1U << (bits - 3U))
             : part == 2U ? (uint16_t)(1U << (bits - 2U))
                          : (uint16_t)(1U << (bits - 1U));
    symbol = offset;
    if (literal_bits != 0U) {
        symbol = (uint16_t)(symbol + av2h_range_read_bypass(decoder, literal_bits));
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    *value = restoration_recenter((uint16_t)(1U << bits), reference, symbol);
    return AV2H_OK;
}

static const int16_t *restoration_bank_reference(const Av2hRestorationEntropyState *state,
                                                 unsigned int plane, uint8_t reference) {
    uint8_t slot;
    if (state->bank_size[plane] == 0U)
        return state->bank[plane][0U];
    slot = (uint8_t)((state->bank_pointer[plane] + 4U - reference) % 4U);
    return state->bank[plane][slot];
}

static void restoration_bank_add(Av2hRestorationEntropyState *state, unsigned int plane,
                                 const int16_t *taps) {
    uint8_t slot;
    if (state->bank_size[plane] < 4U) {
        slot = state->bank_size[plane]++;
    } else {
        slot = (uint8_t)((state->bank_pointer[plane] + 1U) % 4U);
    }
    state->bank_pointer[plane] = slot;
    av2h_memory_copy(state->bank[plane][slot], taps, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
}

static Av2hStatus tile_read_wiener_unit_filter(Av2hTileDecoder *decoder, unsigned int plane,
                                               int16_t *taps) {
    const Av2hWienerNonsepParameters *parameters = av2h_wienerns_parameters(plane);
    Av2hRestorationEntropyState *state = &decoder->entropy_state.restoration_entropy;
    const int16_t *reference_taps;
    uint32_t exact_match;
    uint8_t reference = 0U;
    unsigned int subset = 0U;
    unsigned int symmetric = 1U;
    size_t index;
    Av2hStatus status;
    if (plane >= AV2H_FRAME_PLANE_COUNT || taps == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    exact_match = av2h_range_read_bypass(&decoder->entropy_state.range, 1U);
    if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
        return av2h_range_status(&decoder->entropy_state.range);
    }
    while (reference + 1U < state->bank_size[plane]) {
        uint32_t selected = av2h_range_read_bypass(&decoder->entropy_state.range, 1U);
        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
            return av2h_range_status(&decoder->entropy_state.range);
        }
        if (selected != 0U)
            break;
        ++reference;
    }
    reference_taps = restoration_bank_reference(state, plane, reference);
    if (exact_match != 0U) {
        av2h_memory_copy(taps, reference_taps, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
        if (state->bank_size[plane] == 0U) {
            restoration_bank_add(state, plane, taps);
        }
        return AV2H_OK;
    }
    av2h_memory_fill(taps, 0U, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
    while (subset + 1U < parameters->subset_count) {
        uint32_t longer = av2h_range_read_adaptive(&decoder->entropy_state.range,
                                                   state->length_cdf[plane != 0U], 2U);
        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
            return av2h_range_status(&decoder->entropy_state.range);
        }
        if (longer == 0U)
            break;
        ++subset;
    }
    if (parameters->symmetric_first < parameters->coefficient_count &&
        (parameters->subset_mask[subset] >> parameters->symmetric_first) != 0U) {
        symmetric = av2h_range_read_adaptive(&decoder->entropy_state.range, state->uv_sym_cdf, 2U);
        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
            return av2h_range_status(&decoder->entropy_state.range);
        }
    }
    for (index = 0U; index < parameters->coefficient_count; ++index) {
        uint16_t decoded = 0U;
        uint16_t centered_reference;
        if ((parameters->subset_mask[subset] & (1U << index)) == 0U) {
            continue;
        }
        centered_reference = (uint16_t)(reference_taps[index] - parameters->minimum[index]);
        status = restoration_read_four_part(&decoder->entropy_state.range, state->four_part_cdf,
                                            parameters->bits[index], centered_reference, &decoded);
        if (status != AV2H_OK)
            return status;
        taps[index] = (int16_t)(decoded + parameters->minimum[index]);
        if (symmetric != 0U && index >= parameters->symmetric_first &&
            index + 1U < parameters->coefficient_count) {
            taps[index + 1U] = taps[index];
            ++index;
        }
    }
    restoration_bank_add(state, plane, taps);
    return AV2H_OK;
}

static Av2hStatus tile_read_restoration_units(Av2hTileDecoder *decoder) {
    Av2hRestorationEntropyState *state = &decoder->entropy_state.restoration_entropy;
    unsigned int plane;
    if (state->active_mask == 0U)
        return AV2H_OK;
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
        if ((state->active_mask & (1U << plane)) == 0U)
            continue;
        if (state->unit_size[plane] == 0U || state->unit_columns[plane] == 0U ||
            state->unit_rows[plane] == 0U) {
            return AV2H_INVALID_DATA;
        }
        shift = av2h_chroma_shift(decoder->entropy_state.frame->chroma_format_idc, plane);
        pixel_row = ((size_t)decoder->superblock.sb_row * 4U) >> shift;
        pixel_column = ((size_t)decoder->superblock.sb_column * 4U) >> shift;
        superblock_pixels = ((size_t)state->superblock_mi * 4U) >> shift;
        unit_row_start = (pixel_row + state->unit_size[plane] - 1U) / state->unit_size[plane];
        unit_row_end = (pixel_row + superblock_pixels + state->unit_size[plane] - 1U) /
                       state->unit_size[plane];
        unit_column_start = (pixel_column + state->unit_size[plane] - 1U) / state->unit_size[plane];
        unit_column_end = (pixel_column + superblock_pixels + state->unit_size[plane] - 1U) /
                          state->unit_size[plane];
        if (unit_row_end > state->unit_rows[plane]) {
            unit_row_end = state->unit_rows[plane];
        }
        if (unit_column_end > state->unit_columns[plane]) {
            unit_column_end = state->unit_columns[plane];
        }
        for (unit_row = unit_row_start; unit_row < unit_row_end; ++unit_row) {
            size_t unit_column;
            for (unit_column = unit_column_start; unit_column < unit_column_end; ++unit_column) {
                size_t unit_index = unit_row * state->unit_columns[plane] + unit_column;
                uint32_t value;
                uint8_t unit_type = AV2H_RESTORE_NONE;
                int16_t *taps;
                Av2hStatus status;
                if (unit_index >= state->unit_capacity[plane] ||
                    unit_index != state->decoded_count[plane]) {
                    return AV2H_INVALID_DATA;
                }
                if (decoder->entropy_state.frame->restoration_type[plane] ==
                    AV2H_RESTORE_SWITCHABLE) {
                    unsigned int candidate;
                    if (plane != 0U)
                        return AV2H_INVALID_DATA;
                    unit_type = AV2H_RESTORE_WIENER_NONSEP;
                    for (candidate = AV2H_RESTORE_NONE; candidate < AV2H_RESTORE_WIENER_NONSEP;
                         ++candidate) {
                        if ((decoder->entropy_state.sequence->lr_tools_disable_mask[0] &
                             (1U << candidate)) != 0U) {
                            continue;
                        }
                        value =
                            av2h_range_read_adaptive(&decoder->entropy_state.range,
                                                     state->switchable_restore_cdf[candidate], 2U);
                        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
                            return av2h_range_status(&decoder->entropy_state.range);
                        }
                        if (value != 0U) {
                            unit_type = (uint8_t)candidate;
                            break;
                        }
                    }
                } else {
                    value = av2h_range_read_adaptive(
                        &decoder->entropy_state.range,
                        decoder->entropy_state.frame->restoration_type[plane] ==
                                AV2H_RESTORE_PC_WIENER
                            ? state->pc_wiener_restore_cdf
                            : state->restore_cdf,
                        2U);
                    if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
                        return av2h_range_status(&decoder->entropy_state.range);
                    }
                    if (value != 0U) {
                        unit_type = decoder->entropy_state.frame->restoration_type[plane];
                    }
                }
                state->unit_flags[plane][unit_index] = unit_type;
                taps = state->unit_taps[plane] + unit_index * AV2H_WIENER_NONSEP_MAX_TAPS;
                if (unit_type != AV2H_RESTORE_NONE) {
                    if (unit_type == AV2H_RESTORE_PC_WIENER) {
                        av2h_memory_fill(taps, 0U, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
                    } else if (unit_type == AV2H_RESTORE_WIENER_NONSEP &&
                               decoder->entropy_state.frame->restoration_frame_filters_on[plane]) {
                        av2h_memory_copy(
                            taps, decoder->entropy_state.frame->restoration_frame_filter[plane][0],
                            AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
                    } else if (unit_type == AV2H_RESTORE_WIENER_NONSEP) {
                        status = tile_read_wiener_unit_filter(decoder, plane, taps);
                        if (status != AV2H_OK)
                            return status;
                    } else {
                        return AV2H_INVALID_DATA;
                    }
                } else {
                    av2h_memory_fill(taps, 0U, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
                }
                ++state->decoded_count[plane];
            }
        }
    }
    return AV2H_OK;
}

static int luma_geometry_supported(const Av2hPartitionNode *leaf) {
    const Av2hTxDescriptor *descriptor;
    return leaf != NULL && av2h_luma_tx_descriptor_get(leaf->block_size, &descriptor) == AV2H_OK;
}

static Av2hBlockSize tile_tx_block_size(Av2hTxSize tx_size) {
    static const Av2hBlockSize block_sizes[AV2H_TX_SIZE_COUNT] = {
        AV2H_BLOCK_4X4,   AV2H_BLOCK_8X8,   AV2H_BLOCK_16X16, AV2H_BLOCK_32X32, AV2H_BLOCK_64X64,
        AV2H_BLOCK_4X8,   AV2H_BLOCK_8X4,   AV2H_BLOCK_8X16,  AV2H_BLOCK_16X8,  AV2H_BLOCK_16X32,
        AV2H_BLOCK_32X16, AV2H_BLOCK_32X64, AV2H_BLOCK_64X32, AV2H_BLOCK_4X16,  AV2H_BLOCK_16X4,
        AV2H_BLOCK_8X32,  AV2H_BLOCK_32X8,  AV2H_BLOCK_16X64, AV2H_BLOCK_64X16, AV2H_BLOCK_4X32,
        AV2H_BLOCK_32X4,  AV2H_BLOCK_8X64,  AV2H_BLOCK_64X8,  AV2H_BLOCK_4X64,  AV2H_BLOCK_64X4};
    return (unsigned int)tx_size < AV2H_TX_SIZE_COUNT ? block_sizes[(unsigned int)tx_size]
                                                      : AV2H_BLOCK_INVALID;
}

static Av2hStatus tile_tx_size_from_dimensions(uint8_t width, uint8_t height, Av2hTxSize *tx_size) {
    unsigned int index;
    if (tx_size == NULL)
        return AV2H_INVALID_ARGUMENT;
    for (index = 0U; index < AV2H_TX_SIZE_COUNT; ++index) {
        const Av2hTxDescriptor *descriptor;
        if (av2h_tx_descriptor_get((Av2hTxSize)index, &descriptor) == AV2H_OK &&
            descriptor->output_width == width && descriptor->output_height == height) {
            *tx_size = (Av2hTxSize)index;
            return AV2H_OK;
        }
    }
    return AV2H_UNSUPPORTED;
}

static Av2hStatus tile_read_luma_tx_partition(Av2hTileDecoder *decoder,
                                              const Av2hPartitionNode *coding_node,
                                              const Av2hLeafMode *mode) {
    static const uint8_t split_group[AV2H_BLOCK_SIZE_COUNT] = {
        0U, 0U, 0U, 1U, 2U, 2U, 3U, 4U, 4U, 5U, 6U, 6U, 7U, 7U, 7U,
        7U, 7U, 7U, 7U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U};
    static const uint8_t both_group[AV2H_BLOCK_SIZE_COUNT] = {
        255U, 255U, 255U, 0U, 1U,   2U,   3U,  4U,  5U,  6U,  7U,   8U,   9U,  9U, 9U,
        9U,   9U,   9U,   9U, 255U, 255U, 10U, 11U, 12U, 13U, 255U, 255U, 10U, 11U};
    static const uint8_t partition_count[8U] = {1U, 4U, 2U, 2U, 4U, 4U, 5U, 5U};
    static const uint8_t row_shift[8U][5U] = {{0U},
                                              {1U, 1U, 1U, 1U},
                                              {1U, 1U},
                                              {0U, 0U},
                                              {2U, 2U, 2U, 2U},
                                              {0U, 0U, 0U, 0U},
                                              {2U, 2U, 1U, 2U, 2U},
                                              {1U, 1U, 0U, 1U, 1U}};
    static const uint8_t column_shift[8U][5U] = {{0U},
                                                 {1U, 1U, 1U, 1U},
                                                 {0U, 0U},
                                                 {1U, 1U},
                                                 {0U, 0U, 0U, 0U},
                                                 {2U, 2U, 2U, 2U},
                                                 {1U, 1U, 0U, 1U, 1U},
                                                 {2U, 2U, 1U, 2U, 2U}};
    static const uint8_t row_offset[8U][5U] = {{0U},
                                               {0U, 0U, 1U, 1U},
                                               {0U, 1U},
                                               {0U, 0U},
                                               {0U, 1U, 2U, 3U},
                                               {0U, 0U, 0U, 0U},
                                               {0U, 0U, 1U, 3U, 3U},
                                               {0U, 1U, 0U, 0U, 1U}};
    static const uint8_t column_offset[8U][5U] = {{0U},
                                                  {0U, 1U, 0U, 1U},
                                                  {0U, 0U},
                                                  {0U, 1U},
                                                  {0U, 0U, 0U, 0U},
                                                  {0U, 1U, 2U, 3U},
                                                  {0U, 1U, 0U, 0U, 1U},
                                                  {0U, 0U, 1U, 3U, 3U}};
    const Av2hTxDescriptor *maximum;
    Av2hTxSize candidate;
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
    if (decoder == NULL || coding_node == NULL || mode == NULL ||
        coding_node->block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    decoder->luma.selected_tx_count = 0U;
    decoder->luma.tx_partition = 0U;
    if (decoder->entropy_state.frame->tx_mode != 2U || mode->skip_transform != 0U ||
        av2h_quantizer_is_lossless(decoder->entropy_state.sequence, decoder->entropy_state.frame,
                                   decoder->superblock.current_block_q_index)) {
        return AV2H_OK;
    }
    if (coding_node->block_size >= AV2H_BLOCK_64X128 &&
        coding_node->block_size <= AV2H_BLOCK_256X256) {
        return AV2H_OK;
    }
    status = av2h_luma_tx_descriptor_get(coding_node->block_size, &maximum);
    if (status != AV2H_OK)
        return status;
    allow_horizontal = (uint8_t)(maximum->output_height >= 8U &&
                                 tile_tx_size_from_dimensions(
                                     maximum->output_width, (uint8_t)(maximum->output_height / 2U),
                                     &candidate) == AV2H_OK);
    allow_vertical =
        (uint8_t)(maximum->output_width >= 8U &&
                  tile_tx_size_from_dimensions((uint8_t)(maximum->output_width / 2U),
                                               maximum->output_height, &candidate) == AV2H_OK);
    if (!allow_horizontal && !allow_vertical)
        return AV2H_OK;
    symbol =
        av2h_range_read_adaptive(&decoder->entropy_state.range,
                                 decoder->entropy_state.tx_partition_cdfs
                                     .do_partition[mode->fsc_mode != 0U][mode->use_intrabc != 0U]
                                                  [split_group[coding_node->block_size]],
                                 2U);
    if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
        return av2h_range_status(&decoder->entropy_state.range);
    }
    if (symbol == 0U)
        return AV2H_OK;
    if (allow_horizontal && allow_vertical) {
        uint8_t group = both_group[coding_node->block_size];
        if (group >= AV2H_TX_PARTITION_TYPE_GROUPS ||
            decoder->entropy_state.sequence->reduced_tx_partition_set) {
            return AV2H_UNSUPPORTED;
        }
        symbol = av2h_range_read_adaptive(
            &decoder->entropy_state.range,
            decoder->entropy_state.tx_partition_cdfs
                .partition_type[mode->fsc_mode != 0U][mode->use_intrabc != 0U][group],
            AV2H_TX_PARTITION_TYPE_SYMBOLS);
        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
            return av2h_range_status(&decoder->entropy_state.range);
        }
        partition = (uint8_t)(symbol + 1U);
    } else {
        uint8_t fixed_two_way = (uint8_t)(coding_node->block_size == AV2H_BLOCK_4X8 ||
                                          coding_node->block_size == AV2H_BLOCK_8X4);
        uint8_t context =
            coding_node->block_size == AV2H_BLOCK_4X16 || coding_node->block_size == AV2H_BLOCK_4X32
                ? 0U
                : 1U;
        symbol = 0U;
        if (!fixed_two_way && !decoder->entropy_state.sequence->reduced_tx_partition_set) {
            symbol = av2h_range_read_adaptive(
                &decoder->entropy_state.range,
                decoder->entropy_state.tx_partition_cdfs
                    .two_or_three_way[mode->fsc_mode != 0U][mode->use_intrabc != 0U][context],
                2U);
        }
        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
            return av2h_range_status(&decoder->entropy_state.range);
        }
        partition = allow_horizontal ? (uint8_t)(symbol == 0U ? 2U : 4U)
                                     : (uint8_t)(symbol == 0U ? 3U : 5U);
    }
    count = partition_count[partition];
    width_step = (uint8_t)(maximum->output_width / 8U);
    height_step = (uint8_t)(maximum->output_height / 8U);
    if (partition == 4U || partition == 5U) {
        width_step = (uint8_t)(width_step / 2U);
        height_step = (uint8_t)(height_step / 2U);
    }
    if (partition == 6U)
        height_step = (uint8_t)(height_step / 2U);
    if (partition == 7U)
        width_step = (uint8_t)(width_step / 2U);
    for (index = 0U; index < count; ++index) {
        Av2hTxSize tx_size;
        Av2hBlockSize block_size;
        uint8_t width = (uint8_t)(maximum->output_width >> column_shift[partition][index]);
        uint8_t height = (uint8_t)(maximum->output_height >> row_shift[partition][index]);
        status = tile_tx_size_from_dimensions(width, height, &tx_size);
        if (status != AV2H_OK)
            return status;
        block_size = tile_tx_block_size(tx_size);
        if (block_size == AV2H_BLOCK_INVALID)
            return AV2H_UNSUPPORTED;
        Av2hPartitionNode node = *coding_node;
        node.row = (uint16_t)(coding_node->row + row_offset[partition][index] * height_step);
        node.column =
            (uint16_t)(coding_node->column + column_offset[partition][index] * width_step);
        node.block_size = block_size;
        if (node.row >= decoder->traversal.mi_rows ||
            node.column >= decoder->traversal.mi_columns) {
            continue;
        }
        decoder->luma.tx_nodes[selected_count++] = node;
    }
    decoder->luma.tx_partition = partition;
    decoder->luma.selected_tx_count = selected_count;
    return AV2H_OK;
}

static Av2hStatus tile_handoff_square_child(Av2hTileDecoder *decoder, uint8_t *has_leaf) {
    Av2hPartitionNode chroma_root;
    const Av2hPartitionNode *pending = NULL;
    size_t pending_index;
    uint8_t next;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (decoder->superblock.sb_row == 0U && decoder->superblock.sb_column == 480U &&
        decoder->root.square_root_child_index == 1U) {
        av2h_partition_diagnostic_leaf_stage(
            decoder->entropy_state.range.diagnostics,
            (Av2hPartitionNode){0U, 496U, AV2H_BLOCK_64X64}, 90U,
            (uint8_t)decoder->root.luma_root_partition,
            (uint16_t)decoder->traversal.luma_traversal.stack_count,
            av2h_range_tell(&decoder->entropy_state.range));
    }
#endif
    if (!decoder->entropy_state.sequence->enable_sdp ||
        decoder->root.root_block_size != AV2H_BLOCK_128X128 ||
        decoder->root.square_child_luma_partition == AV2H_PARTITION_INVALID ||
        decoder->root.square_root_child_index >= 4U) {
        return AV2H_OK;
    }
    pending_index = decoder->traversal.luma_traversal.stack_count;
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
        next = (uint8_t)((pending->row >= decoder->superblock.sb_row + 16U ? 2U : 0U) +
                         (pending->column >= decoder->superblock.sb_column + 16U ? 1U : 0U));
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        if (decoder->superblock.sb_row == 0U && decoder->superblock.sb_column == 480U &&
            decoder->root.square_root_child_index == 1U) {
            av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics,
                                                 (Av2hPartitionNode){0U, 496U, AV2H_BLOCK_64X64},
                                                 91U, (uint8_t)pending->row, pending->column,
                                                 av2h_range_tell(&decoder->entropy_state.range));
        }
#endif
        if (next == decoder->root.square_root_child_index) {
            return AV2H_OK;
        }
    }
    chroma_root.row = (uint16_t)(decoder->superblock.sb_row +
                                 (decoder->root.square_root_child_index >= 2U ? 16U : 0U));
    chroma_root.column =
        (uint16_t)(decoder->superblock.sb_column +
                   ((decoder->root.square_root_child_index & 1U) != 0U ? 16U : 0U));
    chroma_root.block_size = AV2H_BLOCK_64X64;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (chroma_root.row == 0U && chroma_root.column == 496U) {
        av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, chroma_root,
                                             100U,
                                             (uint8_t)decoder->root.square_child_luma_partition,
                                             decoder->root.square_child_luma_child_count,
                                             av2h_range_tell(&decoder->entropy_state.range));
    }
#endif
    if (av2h_partition_traversal_reset_root(&decoder->traversal.chroma_traversal, chroma_root) !=
        AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    decoder->root.chroma_root_partition = AV2H_PARTITION_INVALID;
    decoder->root.chroma_root_forced_partition = AV2H_PARTITION_INVALID;
    if (decoder->root.square_child_luma_partition == AV2H_PARTITION_NONE) {
        decoder->root.chroma_root_forced_partition = AV2H_PARTITION_NONE;
    } else if (decoder->root.square_child_luma_child_count == 2U &&
               decoder->root.square_child_luma_children_share &&
               av2h_tile_partition_direction(decoder->root.square_child_luma_partition) != 0U) {
        decoder->root.chroma_root_forced_partition = decoder->root.square_child_luma_partition;
    }
    decoder->root.square_child_luma_child_count = 0U;
    decoder->root.square_child_luma_children_share = 1U;
    decoder->control.phase = AV2H_TILE_DECODE_CHROMA;
    decoder->control.event = AV2H_TILE_EVENT_ROOT_HANDOFF;
    *has_leaf = 0U;
    return AV2H_OK;
}

static Av2hStatus tile_initialize_skipped_intrabc(Av2hTileDecoder *decoder,
                                                  const Av2hTileLumaLeaf *leaf,
                                                  Av2hCoeffPrefix *prefix,
                                                  Av2hTransformType *transform,
                                                  Av2hCoeffBlock *block) {
    size_t tell;
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (decoder == NULL || leaf == NULL || prefix == NULL || transform == NULL || block == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (leaf->mode.use_intrabc != 1U || leaf->mode.skip_transform != 1U) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_luma_tx_descriptor_get(leaf->node.block_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    prefix->tx_size = descriptor->tx_size;
    prefix->tx_size_context = descriptor->tx_size_context;
    tell = av2h_range_tell(&decoder->entropy_state.range);
    {
        Av2hTxSize tx_size = prefix->tx_size;
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
    block->after_levels_range = decoder->entropy_state.range.range;
    block->after_levels_value = decoder->entropy_state.range.value;
    block->after_coefficients_bits = tell;
    status = av2h_coeff_context_state_update(&decoder->entropy_state.coefficient_contexts, 0U,
                                             &leaf->node, 0U);
    return status;
}

static Av2hStatus tile_initialize_all_zero_luma(Av2hTileDecoder *decoder,
                                                const Av2hTileLumaLeaf *leaf,
                                                Av2hTransformType *transform,
                                                Av2hCoeffBlock *block) {
    uint16_t width;
    uint16_t height;
    size_t tell;
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (decoder == NULL || leaf == NULL || transform == NULL || block == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(leaf->prefix.tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    width = descriptor->coefficient_width;
    height = descriptor->coefficient_height;
    if (!leaf->prefix.all_zero || leaf->prefix.eob != 0U || leaf->mode.skip_transform) {
        return AV2H_UNSUPPORTED;
    }
    tell = av2h_range_tell(&decoder->entropy_state.range);
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
    block->after_levels_range = decoder->entropy_state.range.range;
    block->after_levels_value = decoder->entropy_state.range.value;
    block->after_coefficients_bits = tell;
    status = av2h_coeff_context_state_update(&decoder->entropy_state.coefficient_contexts, 0U,
                                             &leaf->node, 0U);
    return status;
}

static Av2hStatus tile_store_luma_mode(Av2hTileDecoder *decoder, const Av2hPartitionNode *node,
                                       const Av2hLeafMode *mode) {
    size_t width;
    size_t height;
    size_t row;
    size_t column;
    Av2hStatus status = av2h_block_dimensions_mi(node->block_size, &width, &height);
    if (status != AV2H_OK)
        return status;
    if (node->column >= decoder->traversal.mi_columns || node->row >= decoder->traversal.mi_rows) {
        return AV2H_OVERFLOW;
    }
    if (width > decoder->traversal.mi_columns - node->column) {
        width = decoder->traversal.mi_columns - node->column;
    }
    if (height > decoder->traversal.mi_rows - node->row) {
        height = decoder->traversal.mi_rows - node->row;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t index = ((size_t)node->row + row) * decoder->traversal.mi_columns +
                           (size_t)node->column + column;
            Av2hTileLumaModeCell *cell = &decoder->traversal.colocated_luma[index];
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

Av2hStatus av2h_tile_decoder_next_luma(Av2hTileDecoder *decoder, Av2hTileLumaLeaf *leaf,
                                       Av2hCoeffBlock *block, uint8_t *has_leaf) {
    Av2hLeafPath path;
    uint8_t dc_sign_context;
    uint8_t shared_chroma_tree;
    uint16_t tx_count;
    Av2hStatus status;
    if (decoder == NULL || leaf == NULL || block == NULL || has_leaf == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (decoder->control.status != AV2H_OK)
        return decoder->control.status;
    if (decoder->control.phase != AV2H_TILE_DECODE_LUMA) {
        return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
    }
    if (decoder->luma.tx_count != 0U) {
        if (decoder->luma.tx_index >= decoder->luma.tx_count) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_ARGUMENT);
        }
        av2h_memory_copy(&leaf->mode, &decoder->luma.coding_mode, sizeof(leaf->mode));
        leaf->mode.tx_extended_edges_unavailable =
            (uint8_t)((decoder->luma.tx_partition == 6U || decoder->luma.tx_partition == 7U) &&
                      decoder->luma.tx_index >= 1U);
        status = decoder->luma.coding_mode.q_index == 0U &&
                         decoder->luma.coding_mode.lossless_tx_size_large == 0U
                     ? av2h_luma_lossless_tx_node_get(
                           &decoder->luma.coding_node, decoder->traversal.mi_columns,
                           decoder->traversal.mi_rows, decoder->luma.tx_index, &leaf->node)
                     : av2h_tile_luma_tx_node_get(decoder, decoder->luma.tx_index, &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
        *has_leaf = 1U;
        goto read_transform;
    }
    av2h_tile_diagnostics_set_luma_stage(decoder->entropy_state.range.diagnostics, 0U);
    status = tile_handoff_square_child(decoder, has_leaf);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    if (decoder->control.phase == AV2H_TILE_DECODE_CHROMA)
        return AV2H_OK;
    if (decoder->traversal.luma_partition_node_count == 0U) {
        status = tile_read_restoration_units(decoder);
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
    }
    status = av2h_partition_traversal_next(
        &decoder->traversal.luma_traversal, &decoder->entropy_state.range,
        &decoder->entropy_state.partition_cdfs, &decoder->entropy_state.partition_contexts, &path,
        has_leaf);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_partition_path(decoder, &path);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    if (!*has_leaf) {
        return av2h_tile_decoder_finish_root(decoder);
    }
    av2h_tile_capture_root_partition(decoder, &path, &decoder->root.luma_root_partition);
    tile_capture_luma_root_child(decoder, &path);
    leaf->node = path.leaf;
    av2h_tile_diagnostics_set_luma_range(decoder->entropy_state.range.diagnostics, 0U,
                                         decoder->entropy_state.range.range);
    if (!luma_geometry_supported(&leaf->node)) {
        return av2h_tile_decoder_fail(decoder, AV2H_UNSUPPORTED);
    }
    if (decoder->entropy_state.luma_mode_contexts.superblock_mi != 0U &&
        leaf->node.row % decoder->entropy_state.luma_mode_contexts.superblock_mi == 0U &&
        leaf->node.column == 0U) {
        decoder->entropy_state.luma_mode_contexts.intrabc_bank_count = 0U;
        decoder->entropy_state.luma_mode_contexts.intrabc_bank_start = 0U;
        decoder->entropy_state.luma_mode_contexts.intrabc_bank_remaining_hits = 0U;
        decoder->entropy_state.luma_mode_contexts.intrabc_bank_unit_hits = 0U;
        decoder->entropy_state.luma_mode_contexts.intrabc_bank_superblock_hits = 0U;
    }
    av2h_tile_diagnostics_set_luma_stage(decoder->entropy_state.range.diagnostics, 1U);
    shared_chroma_tree =
        (uint8_t)(decoder->entropy_state.sequence->enable_sdp &&
                  ((leaf->node.block_size == AV2H_BLOCK_128X128 &&
                    decoder->root.luma_root_partition == AV2H_PARTITION_NONE) ||
                   ((leaf->node.block_size == AV2H_BLOCK_64X128 ||
                     leaf->node.block_size == AV2H_BLOCK_128X64) &&
                    av2h_tile_partition_direction(decoder->root.luma_root_partition) != 0U)));
    status = av2h_luma_mode_read_with_state(
        &decoder->entropy_state.range, &decoder->entropy_state.leaf_cdfs,
        decoder->entropy_state.sequence, decoder->entropy_state.frame, &leaf->node,
        &decoder->entropy_state.luma_mode_contexts, &decoder->entropy_state.gdf_entropy,
        &decoder->entropy_state.cdef_entropy, &decoder->entropy_state.ccso_entropy,
        &decoder->superblock.current_q_index, shared_chroma_tree, &leaf->mode);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    decoder->superblock.current_block_q_index = leaf->mode.q_index;
    if (shared_chroma_tree) {
        status = tile_store_luma_mode(decoder, &leaf->node, &leaf->mode);
        if (status == AV2H_UNSUPPORTED) {
            return av2h_tile_decoder_fail(decoder, AV2H_OUT_OF_MEMORY);
        }
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
        status = av2h_tile_decoder_read_shared_chroma_mode(decoder, &leaf->node);
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
        {
            Av2hLumaModeNeighbors neighbors;
            status = av2h_luma_mode_neighbors_from_state(&decoder->entropy_state.luma_mode_contexts,
                                                         &leaf->node, &neighbors);
            if (status != AV2H_OK) {
                return av2h_tile_decoder_fail(decoder, status);
            }
            status = av2h_luma_mode_read_palette_and_dip(
                &decoder->entropy_state.range, &decoder->entropy_state.leaf_cdfs,
                decoder->entropy_state.sequence, decoder->entropy_state.frame, &leaf->node,
                &neighbors, &leaf->mode);
        }
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
    }
    status = av2h_luma_palette_map_read(
        &decoder->entropy_state.range, &decoder->entropy_state.leaf_cdfs, &leaf->node, &leaf->mode);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    if (leaf->mode.q_index == 0U && leaf->mode.skip_transform == 0U &&
        (leaf->mode.fsc_mode != 0U || leaf->mode.use_intrabc != 0U) &&
        leaf->node.block_size != AV2H_BLOCK_4X4) {
        static const uint8_t size_group[AV2H_BLOCK_SIZE_COUNT] = {
            0U, 0U, 0U, 1U, 1U, 1U, 2U, 2U, 2U, 3U, 3U, 3U, 3U, 3U, 3U,
            3U, 3U, 3U, 3U, 0U, 0U, 1U, 1U, 2U, 2U, 1U, 1U, 2U, 2U};
        uint8_t group = size_group[leaf->node.block_size];
        leaf->mode.lossless_tx_size_large = (uint8_t)av2h_range_read_adaptive(
            &decoder->entropy_state.range,
            decoder->entropy_state.leaf_cdfs.lossless_tx_size[group][leaf->mode.use_intrabc != 0U],
            2U);
        if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder,
                                          av2h_range_status(&decoder->entropy_state.range));
        }
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 60U,
                                         leaf->mode.gdf_flags_read, leaf->mode.cdef_strengths_read,
                                         leaf->mode.after_gdf_bits);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 61U,
                                         leaf->mode.ccso_controls_read, leaf->mode.mode_index,
                                         leaf->mode.after_ccso_bits);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 62U,
                                         leaf->mode.mode_set, leaf->mode.mode_index,
                                         leaf->mode.after_mode_index_bits);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 63U,
                                         leaf->mode.fsc_mode, leaf->mode.mrl_index,
                                         leaf->mode.after_mrl_bits);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 1U,
                                         (uint8_t)leaf->mode.mode, 0U,
                                         av2h_range_tell(&decoder->entropy_state.range));
#endif
    av2h_tile_diagnostics_set_luma_range(decoder->entropy_state.range.diagnostics, 1U,
                                         decoder->entropy_state.range.range);
    status = av2h_luma_mode_context_state_update(&decoder->entropy_state.luma_mode_contexts,
                                                 &leaf->node, &leaf->mode);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = tile_store_luma_mode(decoder, &leaf->node, &leaf->mode);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (shared_chroma_tree) {
        av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node,
                                             70U, decoder->shared_chroma.mode.mode_index,
                                             decoder->shared_chroma.mode.cfl_kind,
                                             av2h_range_tell(&decoder->entropy_state.range));
    }
#endif
    status = tile_read_luma_tx_partition(decoder, &leaf->node, &leaf->mode);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    status = decoder->luma.selected_tx_count != 0U
                 ? (tx_count = decoder->luma.selected_tx_count, AV2H_OK)
             : leaf->mode.q_index == 0U && leaf->mode.lossless_tx_size_large == 0U
                 ? av2h_luma_lossless_tx_node_count(&leaf->node, decoder->traversal.mi_columns,
                                                    decoder->traversal.mi_rows, &tx_count)
                 : av2h_luma_tx_node_count(leaf->node.block_size, &tx_count);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    if (tx_count > 1U) {
        decoder->luma.coding_node = leaf->node;
        av2h_memory_copy(&decoder->luma.coding_mode, &leaf->mode,
                         sizeof(decoder->luma.coding_mode));
        decoder->luma.tx_index = 0U;
        decoder->luma.tx_count = tx_count;
        status = decoder->luma.coding_mode.q_index == 0U &&
                         decoder->luma.coding_mode.lossless_tx_size_large == 0U
                     ? av2h_luma_lossless_tx_node_get(&decoder->luma.coding_node,
                                                      decoder->traversal.mi_columns,
                                                      decoder->traversal.mi_rows, 0U, &leaf->node)
                     : av2h_tile_luma_tx_node_get(decoder, 0U, &leaf->node);
        if (status != AV2H_OK) {
            return av2h_tile_decoder_fail(decoder, status);
        }
        leaf->mode.tx_extended_edges_unavailable =
            (uint8_t)((decoder->luma.tx_partition == 6U || decoder->luma.tx_partition == 7U) &&
                      decoder->luma.tx_index >= 1U);
    }
read_transform:
    if (leaf->mode.skip_transform != 0U) {
        status =
            tile_initialize_skipped_intrabc(decoder, leaf, &leaf->prefix, &leaf->transform, block);
        if (status == AV2H_UNSUPPORTED) {
            return av2h_tile_decoder_fail(decoder, AV2H_INVALID_DATA);
        }
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
        leaf->prefix.q_index = decoder->superblock.current_block_q_index;
        decoder->control.event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(decoder->entropy_state.range.diagnostics, 2U);
    if (decoder->luma.tx_count != 0U) {
        uint8_t txb_skip_context;
        status = av2h_coeff_contexts_from_state_for_coding_block(
            &decoder->entropy_state.coefficient_contexts, 0U, &leaf->node,
            decoder->luma.coding_node.block_size, leaf->mode.fsc_mode, &txb_skip_context,
            &dc_sign_context);
        if (status == AV2H_OK) {
            status = av2h_coeff_prefix_read(&decoder->entropy_state.range,
                                            &decoder->entropy_state.coeff_cdfs,
                                            decoder->entropy_state.frame, &leaf->node, &leaf->mode,
                                            txb_skip_context, &leaf->prefix);
        }
    } else {
        status = av2h_coeff_prefix_read_with_state(
            &decoder->entropy_state.range, &decoder->entropy_state.coeff_cdfs,
            decoder->entropy_state.frame, &leaf->node, &leaf->mode,
            &decoder->entropy_state.coefficient_contexts, &leaf->prefix, &dc_sign_context);
    }
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
    leaf->prefix.q_index = decoder->superblock.current_block_q_index;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 21U,
                                         leaf->prefix.eob_point, leaf->prefix.eob_extra,
                                         leaf->prefix.after_eob_point_bits);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 22U,
                                         leaf->prefix.eob_offset_bits, leaf->prefix.eob_extra,
                                         leaf->prefix.after_eob_extra_bit);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 2U,
                                         (uint8_t)leaf->mode.mode, leaf->prefix.eob,
                                         av2h_range_tell(&decoder->entropy_state.range));
#endif
    av2h_tile_diagnostics_set_luma_range(decoder->entropy_state.range.diagnostics, 2U,
                                         decoder->entropy_state.range.range);
    if (leaf->prefix.all_zero) {
        status = tile_initialize_all_zero_luma(decoder, leaf, &leaf->transform, block);
        if (status != AV2H_OK)
            return av2h_tile_decoder_fail(decoder, status);
        decoder->control.event = AV2H_TILE_EVENT_LUMA_LEAF;
        return AV2H_OK;
    }
    av2h_tile_diagnostics_set_luma_stage(decoder->entropy_state.range.diagnostics, 3U);
    if (decoder->superblock.current_block_q_index == 0U) {
        size_t tell = av2h_range_tell(&decoder->entropy_state.range);
        size_t before_primary = tell;
        uint8_t lossless_inter_tx_type = 0U;
        if (leaf->mode.use_intrabc != 0U && leaf->prefix.tx_size == AV2H_TX_4X4) {
            lossless_inter_tx_type = (uint8_t)av2h_range_read_adaptive(
                &decoder->entropy_state.range,
                decoder->entropy_state.leaf_cdfs.lossless_inter_tx_type, 2U);
            if (av2h_range_status(&decoder->entropy_state.range) != AV2H_OK) {
                return av2h_tile_decoder_fail(decoder,
                                              av2h_range_status(&decoder->entropy_state.range));
            }
            tell = av2h_range_tell(&decoder->entropy_state.range);
        }
        av2h_memory_fill(&leaf->transform, 0U, sizeof(leaf->transform));
        leaf->transform.primary_type =
            leaf->mode.fsc_mode != 0U ||
                    (leaf->mode.use_intrabc != 0U &&
                     (lossless_inter_tx_type != 0U || leaf->prefix.tx_size != AV2H_TX_4X4))
                ? AV2H_TX_IDTX
                : AV2H_TX_DCT_DCT;
        leaf->transform.before_primary_bits = before_primary;
        leaf->transform.after_primary_bits = tell;
        leaf->transform.after_secondary_type_bits = tell;
        leaf->transform.after_secondary_bits = tell;
        status = AV2H_OK;
    } else {
        status = av2h_transform_type_read(
            &decoder->entropy_state.range, &decoder->entropy_state.transform_cdfs,
            decoder->entropy_state.sequence, decoder->entropy_state.frame, &leaf->mode,
            &leaf->prefix, &leaf->transform);
    }
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 3U,
                                         (uint8_t)leaf->mode.mode, leaf->prefix.eob,
                                         av2h_range_tell(&decoder->entropy_state.range));
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        av2h_partition_diagnostic_leaf_stage(
            decoder->entropy_state.range.diagnostics, leaf->node, 89U, leaf->transform.primary_type,
            leaf->transform.secondary_symbol, leaf->prefix.tx_size);
    }
#endif
    av2h_tile_diagnostics_set_luma_range(decoder->entropy_state.range.diagnostics, 3U,
                                         decoder->entropy_state.range.range);
    av2h_tile_diagnostics_set_luma_stage(decoder->entropy_state.range.diagnostics, 4U);
    status = av2h_coeff_block_read_with_state(
        &decoder->entropy_state.range, &decoder->entropy_state.level_cdfs,
        decoder->entropy_state.sequence, decoder->entropy_state.frame, &leaf->node, &leaf->mode,
        &leaf->prefix, &leaf->transform, &decoder->entropy_state.coefficient_contexts, block);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        size_t coefficient_index;
        for (coefficient_index = 0U; coefficient_index < 8U; ++coefficient_index) {
            av2h_partition_diagnostic_leaf_stage(
                decoder->entropy_state.range.diagnostics, leaf->node,
                (uint8_t)(80U + coefficient_index), (uint8_t)block->quantized[coefficient_index],
                (uint16_t)block->dequantized[coefficient_index], coefficient_index);
        }
    }
    if (leaf->node.row == 184U && leaf->node.column == 280U &&
        leaf->node.block_size == AV2H_BLOCK_32X32) {
        uint16_t scan_index = leaf->prefix.eob;
        while (scan_index != 0U) {
            uint16_t position;
            --scan_index;
            position = av2h_coeff_scan_position(scan_index, block->width, block->height, 0U, 0U);
            av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics,
                                                 leaf->node, 40U, block->base_levels[position],
                                                 position, scan_index);
        }
    }
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 31U,
                                         (uint8_t)leaf->mode.mode, leaf->prefix.eob,
                                         block->after_levels_bits);
    av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics, leaf->node, 4U,
                                         (uint8_t)leaf->mode.mode, leaf->prefix.eob,
                                         av2h_range_tell(&decoder->entropy_state.range));
#endif
    av2h_tile_diagnostics_set_luma_range(decoder->entropy_state.range.diagnostics, 4U,
                                         decoder->entropy_state.range.range);
    decoder->control.event = AV2H_TILE_EVENT_LUMA_LEAF;
    return AV2H_OK;
}

Av2hStatus av2h_tile_decoder_reconstruct_next_luma(Av2hTileDecoder *decoder,
                                                   Av2hReconstructionPlane *plane,
                                                   Av2hReconstructionScratch *scratch,
                                                   Av2hTileLumaLeaf *leaf, Av2hCoeffBlock *block,
                                                   uint8_t *has_leaf) {
    Av2hStatus status;
    if (decoder == NULL || plane == NULL || scratch == NULL || leaf == NULL || block == NULL ||
        has_leaf == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tile_decoder_next_luma(decoder, leaf, block, has_leaf);
    if (status != AV2H_OK || !*has_leaf)
        return status;
    status = av2h_luma_reconstruct_to_plane(NULL, &leaf->node, &leaf->mode, &leaf->prefix,
                                            &leaf->transform, block, plane, scratch);
    if (status != AV2H_OK)
        return av2h_tile_decoder_fail(decoder, status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (leaf->node.row == 0U && leaf->node.column == 0U) {
        size_t residual_index;
        for (residual_index = 0U; residual_index < 8U; ++residual_index) {
            av2h_partition_diagnostic_leaf_stage(decoder->entropy_state.range.diagnostics,
                                                 leaf->node, (uint8_t)(90U + residual_index), 0U,
                                                 (uint16_t)scratch->residual[residual_index],
                                                 residual_index);
        }
    }
#endif
    return av2h_tile_decoder_commit_leaf(decoder, leaf->node);
}
