#include "sequence.h"

#include "internal/obu.h"
#include "shared/bit_math.h"

#include <stddef.h>

static Av2hStatus fail_obu_syntax(Av2hError *error,
                                  Av2hStatus status,
                                  const Av2hObu *obu,
                                  size_t offset,
                                  uint8_t syntax,
                                  uint32_t parsed_value) {
    status = av2h_obu_fail_at(error, status, obu, offset,
                      AV2H_CONTEXT_SEQUENCE_HEADER);
    if (error != NULL) {
        error->syntax_stage = syntax;
        error->parsed_value = parsed_value;
    }
    return status;
}

static uint32_t sequence_tile_width_scale(const Av2hSequenceState *state) {
    unsigned int level = state->info.seq_level_idx;
    if (level > 21U) return 0U;
    if (state->info.seq_tier != 0U) {
        if (level >= 18U) return 16U;
        if (level >= 14U) return 8U;
        return 4U;
    }
    return level >= 18U ? 8U : 4U;
}

static uint32_t sequence_tile_area_scale(const Av2hSequenceState *state) {
    unsigned int level = state->info.seq_level_idx;
    if (level > 21U) return 0U;
    if (state->info.seq_tier != 0U) {
        if (level >= 18U) return 32U;
        if (level >= 14U) return 16U;
        return 4U;
    }
    if (level >= 18U) return 16U;
    if (level >= 14U) return 8U;
    return 4U;
}

static int read_sequence_tools(Av2hBitReader *reader,
                               uint8_t monochrome,
                               uint8_t single_picture,
                               Av2hSequenceState *state) {
    uint32_t value;
    if (av2h_bit_reader_read(reader, 1U) != 0U) {
        state->superblock_size = 256U;
    } else if (av2h_bit_reader_read(reader, 1U) != 0U) {
        state->superblock_size = 128U;
    } else {
        state->superblock_size = 64U;
    }
    /* Intra frames downsize a 256x256 sequence superblock to 128x128; only
       intra frames are decoded, so the frame size is stored directly. */
    if (state->superblock_size == 256U) {
        state->superblock_size = 128U;
    }    state->enable_sdp = monochrome
        ? 0U : (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_ext_partitions =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->enable_ext_partitions != 0U) {
        state->enable_uneven_4way_partitions =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    state->max_pb_aspect_ratio_log2_minus_1 = 2U;
    if (av2h_bit_reader_read(reader, 1U) != 0U) {
        state->max_pb_aspect_ratio_log2_minus_1 =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }

    state->enable_ext_seg = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->seq_seg_info_present_flag =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->seq_seg_info_present_flag != 0U) {
        size_t segment;
        size_t segment_count = av2h_sequence_segment_count(state);
        state->seg_allow_info_change =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
        for (segment = 0U; segment < segment_count; ++segment) {
            unsigned int feature;
            for (feature = 0U; feature < 3U; ++feature) {
                uint8_t enabled =
                    (uint8_t)av2h_bit_reader_read(reader, 1U);
                if (enabled == 0U) continue;
                state->seg_feature_mask[segment] |= (uint8_t)(1U << feature);
                if (feature != 0U) return 0;
                state->seg_alt_q[segment] = (int16_t)
                    av2h_bit_reader_inverse_signed(reader, 9U);
                if (state->seg_alt_q[segment] < -351 ||
                    state->seg_alt_q[segment] > 351) {
                    return 0;
                }
                state->seg_last_active_id = (uint8_t)segment;
            }
        }
    }

    state->enable_intra_dip = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_intra_edge_filter =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_mrls = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_cfl_intra = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->cfl_downsample_filter_index = monochrome
        ? 0U : (uint8_t)av2h_bit_reader_read(reader, 2U);
    state->enable_mhccp = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_ibp = (uint8_t)av2h_bit_reader_read(reader, 1U);

    state->enable_refmvbank = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (av2h_bit_reader_read(reader, 1U) == 0U) {
        state->enable_drl_reorder =
            av2h_bit_reader_read(reader, 1U) != 0U ? 1U : 2U;
    }
    state->default_max_bvp_drl_bits = (uint8_t)
        (av2h_bit_reader_quasi_uniform(reader, 3U) + 1U);
    state->allow_frame_max_bvp_drl_bits =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (single_picture) {
        state->enable_bawp = (uint8_t)av2h_bit_reader_read(reader, 1U);
        state->force_screen_content_tools = 2U;
        state->force_integer_mv = 2U;
    } else {
        return 0;
    }

    state->enable_fsc = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_idtx_intra = state->enable_fsc != 0U
        ? 1U : (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_ist = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_inter_ist = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_chroma_dctonly = monochrome
        ? 0U : (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->reduced_tx_partition_set =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_cctx = monochrome
        ? 0U : (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_tcq = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->enable_tcq == 0U) {
        state->enable_parity_hiding =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    state->separate_uv_delta_q = monochrome
        ? 0U : (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->equal_ac_dc_q = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->equal_ac_dc_q == 0U) {
        value = av2h_bit_reader_read(reader, 5U);
        state->base_y_dc_delta_q = (int8_t)((int32_t)value - 23);
        state->y_dc_delta_q_enabled =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    if (!monochrome) {
        if (state->equal_ac_dc_q == 0U) {
            value = av2h_bit_reader_read(reader, 5U);
            state->base_uv_dc_delta_q = (int8_t)((int32_t)value - 23);
            state->uv_dc_delta_q_enabled =
                (uint8_t)av2h_bit_reader_read(reader, 1U);
        }
        value = av2h_bit_reader_read(reader, 5U);
        state->base_uv_ac_delta_q = (int8_t)((int32_t)value - 23);
        state->uv_ac_delta_q_enabled =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
        if (state->equal_ac_dc_q != 0U) {
            state->base_uv_dc_delta_q = state->base_uv_ac_delta_q;
        }
    }

    state->disable_loopfilters_across_tiles =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_cdef = (uint8_t)av2h_bit_reader_read(reader, 1U);
    state->enable_gdf = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->enable_gdf != 0U && state->superblock_size == 64U) {
        state->gdf_unit_matches_superblock_size =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    state->enable_restoration =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->enable_restoration != 0U) {
        unsigned int restoration_type;
        for (restoration_type = 1U; restoration_type < 3U;
             ++restoration_type) {
            state->lr_tools_disable_mask[0] |= (uint8_t)(
                av2h_bit_reader_read(reader, 1U) << restoration_type);
        }
        if (av2h_bit_reader_read(reader, 1U) != 0U) {
            state->lr_tools_disable_mask[1] = 1U << 1U;
            state->lr_tools_disable_mask[1] |= (uint8_t)(
                av2h_bit_reader_read(reader, 1U) << 2U);
        } else {
            state->lr_tools_disable_mask[1] =
                (uint8_t)(state->lr_tools_disable_mask[0] | (1U << 1U));
        }
    }
    state->enable_ccso = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->enable_ccso != 0U) {
        state->ccso_unit_matches_superblock_size =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    state->deblock_parameter_bits_minus_2 =
        (uint8_t)av2h_bit_reader_read(reader, 2U);
    state->seq_tile_info_present_flag =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (state->seq_tile_info_present_flag != 0U) {
        uint32_t superblock_mi = state->superblock_size / 4U;
        uint32_t mi_columns =
            ((uint32_t)state->info.max_frame_width_minus_1 + 8U) / 4U;
        uint32_t mi_rows =
            ((uint32_t)state->info.max_frame_height_minus_1 + 8U) / 4U;
        uint32_t sb_columns =
            (mi_columns + superblock_mi - 1U) / superblock_mi;
        uint32_t sb_rows =
            (mi_rows + superblock_mi - 1U) / superblock_mi;
        uint32_t width_scale = sequence_tile_width_scale(state);
        uint32_t area_scale = sequence_tile_area_scale(state);
        uint32_t max_width_sb = width_scale == 0U ? sb_columns :
            (width_scale * 4096U) / state->superblock_size / 4U;
        uint32_t max_area_sb = area_scale == 0U ? sb_columns * sb_rows :
            (area_scale * (4096U * 2304U)) /
                state->superblock_size / state->superblock_size / 4U;
        unsigned int min_log2_columns;
        unsigned int min_log2_tiles;
        unsigned int min_log2_rows;
        unsigned int max_log2_columns;
        unsigned int max_log2_rows;
        uint32_t remaining;
        uint32_t start;
        uint32_t widest = 1U;
        size_t index;
        if (superblock_mi == 0U || max_width_sb == 0U || max_area_sb == 0U) {
            return 0;
        }
        state->tile_sb_columns = (uint16_t)sb_columns;
        state->tile_sb_rows = (uint16_t)sb_rows;
        min_log2_columns = av2h_ceil_log2_ratio_u32(max_width_sb, sb_columns);
        min_log2_tiles = av2h_ceil_log2_ratio_u32(max_area_sb, sb_columns * sb_rows);
        if (min_log2_tiles < min_log2_columns) min_log2_tiles = min_log2_columns;
        max_log2_columns = av2h_ceil_log2_u32(
            sb_columns < AV2H_MAX_TILE_COLUMNS ? sb_columns : AV2H_MAX_TILE_COLUMNS);
        max_log2_rows = av2h_ceil_log2_u32(
            sb_rows < AV2H_MAX_TILE_ROWS ? sb_rows : AV2H_MAX_TILE_ROWS);
        state->allow_tile_info_change =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
        state->uniform_tile_spacing =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
        if (state->uniform_tile_spacing != 0U) {
            state->tile_column_log2 = (uint8_t)min_log2_columns;
            while (state->tile_column_log2 < max_log2_columns &&
                   av2h_bit_reader_read(reader, 1U) != 0U) {
                ++state->tile_column_log2;
            }
            min_log2_rows = min_log2_tiles > state->tile_column_log2 ?
                min_log2_tiles - state->tile_column_log2 : 0U;
            state->tile_row_log2 = (uint8_t)min_log2_rows;
            while (state->tile_row_log2 < max_log2_rows &&
                   av2h_bit_reader_read(reader, 1U) != 0U) {
                ++state->tile_row_log2;
            }
            state->tile_column_count = (uint8_t)(1U << state->tile_column_log2);
            state->tile_row_count = (uint8_t)(1U << state->tile_row_log2);
        } else {
            remaining = sb_columns;
            start = 0U;
            index = 0U;
            while (remaining != 0U && index < AV2H_MAX_TILE_COLUMNS) {
                uint32_t maximum = remaining < max_width_sb ? remaining : max_width_sb;
                uint32_t size = av2h_bit_reader_quasi_uniform(reader, maximum) + 1U;
                state->tile_column_start_sb[index++] = (uint16_t)start;
                start += size;
                remaining -= size;
                if (size > widest) widest = size;
            }
            if (remaining != 0U) return 0;
            state->tile_column_start_sb[index] = (uint16_t)start;
            state->tile_column_count = (uint8_t)index;
            state->tile_column_log2 = (uint8_t)av2h_ceil_log2_u32((uint32_t)index);
            max_area_sb = sb_columns * sb_rows;
            if (min_log2_tiles != 0U) max_area_sb >>= min_log2_tiles + 1U;
            max_width_sb = max_area_sb / widest;
            if (max_width_sb == 0U) max_width_sb = 1U;
            remaining = sb_rows;
            start = 0U;
            index = 0U;
            while (remaining != 0U && index < AV2H_MAX_TILE_ROWS) {
                uint32_t maximum = remaining < max_width_sb ? remaining : max_width_sb;
                uint32_t size = av2h_bit_reader_quasi_uniform(reader, maximum) + 1U;
                state->tile_row_start_sb[index++] = (uint16_t)start;
                start += size;
                remaining -= size;
            }
            if (remaining != 0U) return 0;
            state->tile_row_start_sb[index] = (uint16_t)start;
            state->tile_row_count = (uint8_t)index;
            state->tile_row_log2 = (uint8_t)av2h_ceil_log2_u32((uint32_t)index);
        }
        return reader->status == AV2H_OK;
    }
    return reader->status == AV2H_OK;
}

Av2hStatus av2h_sequence_state_parse(Av2hSpan input,
                                     const Av2hObu *obu,
                                     Av2hSequenceState *state,
                                     Av2hError *error) {
    Av2hBitReader reader;
    Av2hSequenceInfo *sequence;
    uint32_t sequence_id;
    uint32_t chroma;
    uint32_t bit_depth;
    uint32_t width_bits;
    uint32_t height_bits;
    if (obu == NULL || state == NULL || input.data == NULL ||
        obu->type != AV2H_OBU_SEQUENCE_HEADER || obu->extension_flag != 0U ||
        obu->temporal_id != 0U || obu->payload_offset > input.size ||
        obu->payload_size > input.size - obu->payload_offset) {
        return av2h_obu_fail_at(error, AV2H_INVALID_ARGUMENT, obu, 0U,
                AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    av2h_memory_fill(state, 0U, sizeof(*state));
    sequence = &state->info;
    av2h_bit_reader_init(&reader, input.data + obu->payload_offset,
                         obu->payload_size, obu->payload_offset);
    sequence_id = av2h_bit_reader_uvlc(&reader);
    state->sequence_header_id = (uint8_t)sequence_id;
    sequence->seq_profile_idc = (uint8_t)av2h_bit_reader_read(&reader, 5U);
    if (reader.status != AV2H_OK) {
        return av2h_obu_fail_at(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (sequence_id >= 16U ||
        (sequence->seq_profile_idc != 0U &&
         sequence->seq_profile_idc != 4U) ||
        av2h_bit_reader_read(&reader, 1U) != 1U) {
        if (reader.status != AV2H_OK) {
            return av2h_obu_fail_at(error, reader.status, obu,
                            av2h_bit_reader_offset(&reader),
                            AV2H_CONTEXT_SEQUENCE_HEADER);
        }
        return fail_obu_syntax(
            error, AV2H_UNSUPPORTED, obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_ID_PROFILE,
            (sequence_id << 8U) | sequence->seq_profile_idc);
    }
    sequence->seq_level_idx = (uint8_t)av2h_bit_reader_read(&reader, 5U);
    if (sequence->seq_level_idx > 21U && sequence->seq_level_idx != 31U) {
        return av2h_obu_fail_at(error, AV2H_INVALID_DATA, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    sequence->seq_tier = 0U;
    chroma = av2h_bit_reader_uvlc(&reader);
    bit_depth = av2h_bit_reader_uvlc(&reader);
    if (reader.status != AV2H_OK) {
        return av2h_obu_fail_at(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (chroma > 2U || bit_depth > 2U ||
        (sequence->seq_profile_idc == 4U) != (chroma == 2U)) {
        return fail_obu_syntax(
            error, AV2H_UNSUPPORTED, obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_FORMAT,
            (chroma << 8U) | bit_depth);
    }
    sequence->chroma_format_idc = (uint8_t)chroma;
    sequence->bit_depth_idc = (uint8_t)bit_depth;
    sequence->still_picture = 1U;
    sequence->monotonic_output_order_flag = 1U;
    sequence->initial_display_delay_present = 0U;
    sequence->initial_display_delay_minus_1 = 0U;
    width_bits = av2h_bit_reader_read(&reader, 4U) + 1U;
    height_bits = av2h_bit_reader_read(&reader, 4U) + 1U;
    state->num_bits_width = (uint8_t)width_bits;
    state->num_bits_height = (uint8_t)height_bits;
    sequence->max_frame_width_minus_1 =
        (uint16_t)av2h_bit_reader_read(&reader, width_bits);
    sequence->max_frame_height_minus_1 =
        (uint16_t)av2h_bit_reader_read(&reader, height_bits);
    if (reader.status != AV2H_OK) {
        return av2h_obu_fail_at(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (av2h_bit_reader_read(&reader, 1U) != 0U) {
        return fail_obu_syntax(
            error, AV2H_UNSUPPORTED, obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_DIMENSIONS, 1U);
    }
    if (reader.status != AV2H_OK) {
        return av2h_obu_fail_at(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (!read_sequence_tools(&reader, (uint8_t)(chroma == 1U), 1U, state)) {
        return fail_obu_syntax(
            error, reader.status == AV2H_OK ? AV2H_UNSUPPORTED : reader.status,
            obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_TOOLS, 0U);
    }
    sequence->film_grain_params_present =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (sequence->film_grain_params_present != 0U) {
        return fail_obu_syntax(
            error, AV2H_UNSUPPORTED, obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_FILM_GRAIN,
            sequence->film_grain_params_present);
    }
    if (av2h_bit_reader_read(&reader, 1U) != 0U) {
        return fail_obu_syntax(
            error, AV2H_UNSUPPORTED, obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_DISPLAY_MODEL, 1U);
    }
    if (!av2h_obu_check_trailing_bits(&reader)) {
        return fail_obu_syntax(
            error, AV2H_INVALID_DATA, obu, av2h_bit_reader_offset(&reader),
            AV2H_DIAGNOSTIC_SYNTAX_SEQUENCE_TRAILING_BITS, 0U);
    }
    return AV2H_OK;
}

Av2hStatus av2h_sequence_header_parse(Av2hSpan input,
                                      const Av2hObu *obu,
                                      Av2hSequenceInfo *sequence,
                                      Av2hError *error) {
    Av2hSequenceState state;
    Av2hStatus status;
    if (sequence == NULL) {
        return av2h_obu_fail_at(error, AV2H_INVALID_ARGUMENT, obu, 0U,
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    status = av2h_sequence_state_parse(input, obu, &state, error);
    if (status == AV2H_OK) *sequence = state.info;
    return status;
}
