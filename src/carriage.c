#include "base.h"
#include "decoder/sequence.h"

static Av2hStatus fail(Av2hError *error,
                       Av2hStatus status,
                       size_t offset,
                       uint32_t context) {
    if (error != 0) {
        error->status = status;
        error->offset = offset;
        error->context = context;
        error->obu_index = SIZE_MAX;
        error->obu_type = 0U;
    }
    return status;
}

static Av2hStatus fail_obu(Av2hError *error,
                           Av2hStatus status,
                           const Av2hObu *obu,
                           size_t offset,
                           uint32_t context) {
    (void)fail(error, status, offset, context);
    if (error != 0 && obu != 0) {
        error->obu_index = obu->index;
        error->obu_type = obu->type;
    }
    return status;
}

static Av2hStatus reject_extra_obu(Av2hSpan input,
                                    const Av2hObu *first,
                                    size_t base_offset,
                                    uint32_t context,
                                    Av2hError *error) {
    size_t extra_offset = first->offset + first->length_field_size +
                          first->obu_size;
    Av2hSpan extra = {
        input.data + extra_offset,
        input.size - extra_offset
    };
    Av2hObu extra_entry;
    Av2hObuLedger extra_ledger = { &extra_entry, 1U, 0U };
    Av2hError extra_error;
    Av2hStatus status = av2h_obu_parse(extra, &extra_ledger, &extra_error);
    if (status != AV2H_OK && status != AV2H_LIMIT_EXCEEDED) {
        if (error != 0) {
            *error = extra_error;
            error->offset += base_offset + extra_offset;
            error->obu_index = 1U;
        }
        return status;
    }
    extra_entry.index = 1U;
    extra_entry.offset += extra_offset;
    return fail_obu(error, AV2H_INVALID_DATA, &extra_entry,
                    base_offset + extra_entry.offset, context);
}

static size_t leb128_size(size_t value) {
    size_t size = 1U;
    while (value >= 128U) {
        value >>= 7;
        ++size;
    }
    return size;
}

static int obu_global_xlayer_allowed(uint8_t type) {
    return type == AV2H_OBU_TEMPORAL_DELIMITER ||
           type == AV2H_OBU_METADATA_SHORT ||
           type == AV2H_OBU_METADATA_GROUP ||
           type == AV2H_OBU_BUFFER_REMOVAL_TIMING ||
           type == AV2H_OBU_LAYER_CONFIGURATION_RECORD ||
           type == AV2H_OBU_ATLAS_SEGMENT ||
           type == AV2H_OBU_OPERATING_POINT_SET ||
           type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION ||
           type == AV2H_OBU_PADDING;
}

static int obu_layer_ids_valid(const Av2hObu *obu) {
    if (obu->xlayer_id == 31U &&
        (obu->temporal_id != 0U || obu->mlayer_id != 0U)) {
        return 0;
    }
    if (obu->type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION &&
        obu->xlayer_id != 31U) {
        return 0;
    }
    if (obu->type == AV2H_OBU_TEMPORAL_DELIMITER && obu->xlayer_id != 31U) {
        return 0;
    }
    if ((obu->type == AV2H_OBU_SEQUENCE_HEADER ||
         obu->type == AV2H_OBU_TEMPORAL_DELIMITER ||
         obu->type == AV2H_OBU_LAYER_CONFIGURATION_RECORD ||
         obu->type == AV2H_OBU_OPERATING_POINT_SET ||
         obu->type == AV2H_OBU_ATLAS_SEGMENT) &&
        (obu->temporal_id != 0U || obu->mlayer_id != 0U)) {
        return 0;
    }
    if (obu->xlayer_id == 31U && !obu_global_xlayer_allowed(obu->type)) {
        return 0;
    }
    if ((obu->type == AV2H_OBU_CLOSED_LOOP_KEY ||
         obu->type == AV2H_OBU_OPEN_LOOP_KEY) && obu->temporal_id != 0U) {
        return 0;
    }
    return 1;
}

Av2hStatus av2h_leb128_read(Av2hSpan input,
                            size_t *value,
                            size_t *encoded_size,
                            Av2hError *error) {
    uint64_t result = 0U;
    size_t index;
    if ((input.data == 0 && input.size != 0U) || value == 0 || encoded_size == 0) {
        return fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_LEB128);
    }
    for (index = 0U; index < 8U; ++index) {
        unsigned char byte;
        if (index >= input.size) {
            return fail(error, AV2H_TRUNCATED, index, AV2H_CONTEXT_LEB128);
        }
        byte = input.data[index];
        result |= (uint64_t)(byte & 0x7fU) << (index * 7U);
        if ((byte & 0x80U) == 0U) {
            if (result > (uint64_t)SIZE_MAX || leb128_size((size_t)result) != index + 1U) {
                return fail(error, AV2H_INVALID_DATA, 0U, AV2H_CONTEXT_LEB128);
            }
            *value = (size_t)result;
            *encoded_size = index + 1U;
            return AV2H_OK;
        }
    }
    return fail(error, AV2H_INVALID_DATA, 0U, AV2H_CONTEXT_LEB128);
}

Av2hStatus av2h_leb128_write(size_t value,
                             void *output,
                             size_t output_size,
                             size_t *written) {
    unsigned char *bytes = (unsigned char *)output;
    size_t required = leb128_size(value);
    size_t index;
    if (written == 0 || (output == 0 && output_size != 0U)) return AV2H_INVALID_ARGUMENT;
    if (value >= ((uint64_t)1U << 56)) return AV2H_OVERFLOW;
    *written = required;
    if (output == 0 && output_size == 0U) return AV2H_OK;
    if (required > output_size) return AV2H_OUT_OF_MEMORY;
    for (index = 0U; index < required; ++index) {
        unsigned char byte = (unsigned char)(value & 0x7fU);
        value >>= 7;
        if (value != 0U) byte |= 0x80U;
        bytes[index] = byte;
    }
    return AV2H_OK;
}

Av2hStatus av2h_obu_parse(Av2hSpan input,
                          Av2hObuLedger *ledger,
                          Av2hError *error) {
    size_t offset = 0U;
    if ((input.data == 0 && input.size != 0U) || ledger == 0 ||
        (ledger->entries == 0 && ledger->capacity != 0U)) {
        return fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_OBU_HEADER);
    }
    ledger->count = 0U;
    while (offset < input.size) {
        Av2hSpan remaining = { input.data + offset, input.size - offset };
        size_t obu_size;
        size_t length_size;
        size_t end;
        unsigned char first;
        Av2hObu *obu;
        Av2hStatus status = av2h_leb128_read(remaining, &obu_size, &length_size, error);
        if (status != AV2H_OK) {
            if (error != 0) {
                error->offset += offset;
                error->obu_index = ledger->count;
            }
            return status;
        }
        if (obu_size == 0U) {
            return fail(error, AV2H_INVALID_DATA, offset,
                        AV2H_CONTEXT_OBU_HEADER);
        }
        if (!av2h_size_add(offset, length_size, &end) ||
            !av2h_size_add(end, obu_size, &end)) {
            return fail(error, AV2H_OVERFLOW, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        if (end > input.size) {
            return fail(error, AV2H_TRUNCATED, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        first = input.data[offset + length_size];
        if (((first >> 2) & 31U) == 0U || ((first >> 2) & 31U) > 25U) {
            return fail(error, AV2H_INVALID_DATA, offset + length_size,
                        AV2H_CONTEXT_OBU_HEADER);
        }
        if (ledger->count >= ledger->capacity) {
            return fail(error, AV2H_LIMIT_EXCEEDED, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        obu = &ledger->entries[ledger->count];
        obu->index = ledger->count;
        obu->offset = offset;
        obu->length_field_size = length_size;
        obu->obu_size = obu_size;
        obu->extension_flag = (uint8_t)(first >> 7);
        obu->header_size = obu->extension_flag != 0U ? 2U : 1U;
        if (obu_size < obu->header_size) {
            return fail(error, AV2H_INVALID_DATA, offset + length_size,
                        AV2H_CONTEXT_OBU_HEADER);
        }
        obu->type = (uint8_t)((first >> 2) & 31U);
        obu->temporal_id = (uint8_t)(first & 3U);
        obu->mlayer_id = 0U;
        obu->xlayer_id =
            obu->type == AV2H_OBU_TEMPORAL_DELIMITER ||
                    obu->type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION
                ? 31U : 0U;
        if (obu->extension_flag != 0U) {
            unsigned char second = input.data[offset + length_size + 1U];
            obu->mlayer_id = (uint8_t)(second >> 5);
            obu->xlayer_id = (uint8_t)(second & 31U);
        }
        if (!obu_layer_ids_valid(obu)) {
            return fail_obu(error, AV2H_INVALID_DATA, obu,
                            offset + length_size, AV2H_CONTEXT_OBU_HEADER);
        }
        obu->payload_offset = offset + length_size + obu->header_size;
        obu->payload_size = obu_size - obu->header_size;
        ++ledger->count;
        offset = end;
    }
    if (ledger->count == 0U) {
        return fail(error, AV2H_INVALID_DATA, 0U, AV2H_CONTEXT_OBU_HEADER);
    }
    return AV2H_OK;
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
    state->enable_sdp = monochrome
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
    if (state->seq_seg_info_present_flag != 0U) return 0;

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
        (void)av2h_bit_reader_read(reader, 2U);
        if (av2h_bit_reader_read(reader, 1U) != 0U) {
            (void)av2h_bit_reader_read(reader, 1U);
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
    if (state->seq_tile_info_present_flag != 0U) return 0;
    return reader->status == AV2H_OK;
}

static int check_trailing_bits(Av2hBitReader *reader) {
    size_t remaining = av2h_bit_reader_remaining(reader);
    size_t index;
    if (remaining == 0U || remaining > 8U) return 0;
    if (av2h_bit_reader_read(reader, 1U) != 1U) return 0;
    for (index = 1U; index < remaining; ++index) {
        if (av2h_bit_reader_read(reader, 1U) != 0U) return 0;
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
    if (obu == 0 || state == 0 || input.data == 0 ||
        obu->type != AV2H_OBU_SEQUENCE_HEADER || obu->extension_flag != 0U ||
        obu->temporal_id != 0U || obu->payload_offset > input.size ||
        obu->payload_size > input.size - obu->payload_offset) {
        return fail_obu(error, AV2H_INVALID_ARGUMENT, obu, 0U,
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
        return fail_obu(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (sequence_id >= 16U || sequence->seq_profile_idc != 0U ||
        av2h_bit_reader_read(&reader, 1U) != 1U) {
        if (reader.status != AV2H_OK) {
            return fail_obu(error, reader.status, obu,
                            av2h_bit_reader_offset(&reader),
                            AV2H_CONTEXT_SEQUENCE_HEADER);
        }
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    sequence->seq_level_idx = (uint8_t)av2h_bit_reader_read(&reader, 5U);
    if (sequence->seq_level_idx > 21U && sequence->seq_level_idx != 31U) {
        return fail_obu(error, AV2H_INVALID_DATA, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    sequence->seq_tier = 0U;
    chroma = av2h_bit_reader_uvlc(&reader);
    bit_depth = av2h_bit_reader_uvlc(&reader);
    if (reader.status != AV2H_OK) {
        return fail_obu(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (chroma != 0U || bit_depth != 1U) {
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
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
        return fail_obu(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (av2h_bit_reader_read(&reader, 1U) != 0U) {
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (reader.status != AV2H_OK) {
        return fail_obu(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (!read_sequence_tools(&reader, 0U, 1U, state)) {
        return fail_obu(error,
                        reader.status == AV2H_OK ? AV2H_UNSUPPORTED
                                                 : reader.status,
                        obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    sequence->film_grain_params_present =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (sequence->film_grain_params_present != 0U) {
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (av2h_bit_reader_read(&reader, 1U) != 0U) {
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    if (!check_trailing_bits(&reader)) {
        return fail_obu(error, AV2H_INVALID_DATA, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    return AV2H_OK;
}

Av2hStatus av2h_sequence_header_parse(Av2hSpan input,
                                      const Av2hObu *obu,
                                      Av2hSequenceInfo *sequence,
                                      Av2hError *error) {
    Av2hSequenceState state;
    Av2hStatus status;
    if (sequence == 0) {
        return fail_obu(error, AV2H_INVALID_ARGUMENT, obu, 0U,
                        AV2H_CONTEXT_SEQUENCE_HEADER);
    }
    status = av2h_sequence_state_parse(input, obu, &state, error);
    if (status == AV2H_OK) *sequence = state.info;
    return status;
}

void av2h_color_info_default(Av2hColorInfo *color) {
    if (color == 0) return;
    color->color_description_present = 0U;
    color->color_description_idc = 0U;
    color->color_primaries = 2U;
    color->transfer_characteristics = 2U;
    color->matrix_coefficients = 2U;
    color->full_range_flag = 0U;
}

static uint32_t read_rice_golomb(Av2hBitReader *reader, unsigned int parameter) {
    uint32_t quotient = 0U;
    while (quotient < 32U && av2h_bit_reader_read(reader, 1U) != 0U) ++quotient;
    if (reader->status != AV2H_OK) return UINT32_MAX;
    if (quotient == 32U) {
        reader->status = AV2H_INVALID_DATA;
        return UINT32_MAX;
    }
    return (quotient << parameter) + av2h_bit_reader_read(reader, parameter);
}

Av2hStatus av2h_content_interpretation_parse(Av2hSpan input,
                                             const Av2hObu *obu,
                                             Av2hColorInfo *color,
                                             Av2hError *error) {
    Av2hBitReader reader;
    uint32_t scan_type;
    uint32_t chroma_position_present;
    uint32_t aspect_present;
    uint32_t timing_present;
    uint32_t identifier;
    if (obu == 0 || color == 0 || input.data == 0 ||
        obu->type != AV2H_OBU_CONTENT_INTERPRETATION ||
        obu->payload_offset > input.size || obu->payload_size > input.size - obu->payload_offset) {
        return fail_obu(error, AV2H_INVALID_ARGUMENT, obu, 0U,
                AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    av2h_color_info_default(color);
    av2h_bit_reader_init(&reader, input.data + obu->payload_offset,
                         obu->payload_size, obu->payload_offset);
    scan_type = av2h_bit_reader_read(&reader, 2U);
    color->color_description_present =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    chroma_position_present = av2h_bit_reader_read(&reader, 1U);
    aspect_present = av2h_bit_reader_read(&reader, 1U);
    timing_present = av2h_bit_reader_read(&reader, 1U);
    if (av2h_bit_reader_read(&reader, 2U) != 0U) {
        return fail_obu(error, AV2H_INVALID_DATA, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (color->color_description_present != 0U) {
        identifier = read_rice_golomb(&reader, 2U);
        if (identifier > 127U) {
            return fail_obu(error,
                            reader.status == AV2H_OK ? AV2H_INVALID_DATA
                                                     : reader.status,
                            obu,
                            av2h_bit_reader_offset(&reader),
                            AV2H_CONTEXT_CONTENT_INTERPRETATION);
        }
        color->color_description_idc = (uint8_t)identifier;
        if (identifier == 0U) {
            color->color_primaries = (uint8_t)av2h_bit_reader_read(&reader, 8U);
            color->transfer_characteristics =
                (uint8_t)av2h_bit_reader_read(&reader, 8U);
            color->matrix_coefficients =
                (uint8_t)av2h_bit_reader_read(&reader, 8U);
        } else if (identifier == 1U) {
            color->color_primaries = 1U; color->transfer_characteristics = 1U;
            color->matrix_coefficients = 1U;
        } else if (identifier == 2U) {
            color->color_primaries = 9U; color->transfer_characteristics = 16U;
            color->matrix_coefficients = 9U;
        } else if (identifier == 3U) {
            color->color_primaries = 9U; color->transfer_characteristics = 18U;
            color->matrix_coefficients = 9U;
        } else if (identifier == 4U) {
            color->color_primaries = 1U; color->transfer_characteristics = 13U;
            color->matrix_coefficients = 0U;
        } else if (identifier == 5U) {
            color->color_primaries = 1U; color->transfer_characteristics = 13U;
            color->matrix_coefficients = 5U;
        } else {
            color->color_primaries = 2U;
            color->transfer_characteristics = 2U;
            color->matrix_coefficients = 2U;
        }
        color->full_range_flag = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    }
    if (chroma_position_present != 0U) {
        (void)av2h_bit_reader_uvlc(&reader);
        if (scan_type != 1U) (void)av2h_bit_reader_uvlc(&reader);
    }
    if (aspect_present != 0U || timing_present != 0U) {
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (av2h_bit_reader_read(&reader, 1U) != 0U) {
        return fail_obu(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (reader.status != AV2H_OK) {
        return fail_obu(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (!check_trailing_bits(&reader)) {
        return fail_obu(error,
                        reader.status == AV2H_OK ? AV2H_INVALID_DATA
                                                 : reader.status,
                        obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    return AV2H_OK;
}

static int sequence_equal(const Av2hSequenceInfo *left,
                          const Av2hSequenceInfo *right) {
    return left->seq_profile_idc == right->seq_profile_idc &&
           left->seq_level_idx == right->seq_level_idx &&
           left->seq_tier == right->seq_tier &&
           left->chroma_format_idc == right->chroma_format_idc &&
           left->bit_depth_idc == right->bit_depth_idc &&
           left->monotonic_output_order_flag == right->monotonic_output_order_flag &&
           left->still_picture == right->still_picture &&
           left->film_grain_params_present == right->film_grain_params_present &&
           left->initial_display_delay_present == right->initial_display_delay_present &&
           left->initial_display_delay_minus_1 == right->initial_display_delay_minus_1 &&
           left->max_frame_width_minus_1 == right->max_frame_width_minus_1 &&
           left->max_frame_height_minus_1 == right->max_frame_height_minus_1;
}

Av2hStatus av2h_av2c_parse(Av2hSpan input,
                           Av2hAv2C *configuration,
                           Av2hObuLedger *ledger,
                           Av2hError *error) {
    Av2hBitReader reader;
    Av2hObu local_entry;
    Av2hObuLedger local_ledger = { &local_entry, 1U, 0U };
    Av2hSequenceInfo parsed;
    Av2hStatus status;
    if ((input.data == 0 && input.size != 0U) || configuration == 0 || ledger == 0 ||
        (ledger->entries == 0 && ledger->capacity != 0U)) {
        return fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_AV2C);
    }
    ledger->count = 0U;
    if (input.size < 10U) {
        return fail(error, AV2H_TRUNCATED, input.size, AV2H_CONTEXT_AV2C);
    }
    configuration->configuration_version = input.data[0];
    if (configuration->configuration_version != 1U) {
        return fail(error, AV2H_UNSUPPORTED, 0U, AV2H_CONTEXT_AV2C);
    }
    av2h_bit_reader_init(&reader, input.data + 1U, 4U, 1U);
    configuration->sequence.seq_profile_idc =
        (uint8_t)av2h_bit_reader_read(&reader, 5U);
    configuration->sequence.seq_level_idx =
        (uint8_t)av2h_bit_reader_read(&reader, 5U);
    configuration->sequence.seq_tier =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    configuration->sequence.chroma_format_idc =
        (uint8_t)av2h_bit_reader_read(&reader, 3U);
    configuration->sequence.bit_depth_idc =
        (uint8_t)av2h_bit_reader_read(&reader, 3U);
    configuration->sequence.monotonic_output_order_flag =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    configuration->sequence.still_picture =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    configuration->sequence.film_grain_params_present =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    configuration->sequence.initial_display_delay_present =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    configuration->sequence.initial_display_delay_minus_1 =
        (uint8_t)av2h_bit_reader_read(&reader, 4U);
    if (av2h_bit_reader_read(&reader, 7U) != 0U) {
        return fail(error, AV2H_INVALID_DATA, 4U, AV2H_CONTEXT_AV2C);
    }
    configuration->sequence.max_frame_width_minus_1 = av2h_load_u16be(input.data + 5U);
    configuration->sequence.max_frame_height_minus_1 = av2h_load_u16be(input.data + 7U);
    if (configuration->sequence.film_grain_params_present != 0U) {
        return fail(error, AV2H_UNSUPPORTED, 3U, AV2H_CONTEXT_AV2C);
    }
    configuration->config_obus.data = input.data + 9U;
    configuration->config_obus.size = input.size - 9U;
    status = av2h_obu_parse(configuration->config_obus, &local_ledger, error);
    if (status == AV2H_LIMIT_EXCEEDED) {
        return reject_extra_obu(configuration->config_obus, &local_entry,
                                9U, AV2H_CONTEXT_AV2C, error);
    }
    if (status != AV2H_OK) {
        if (error != 0) error->offset += 9U;
        return status;
    }
    if (local_ledger.count != 1U ||
        local_entry.type != AV2H_OBU_SEQUENCE_HEADER) {
        return fail_obu(error, AV2H_INVALID_DATA,
                local_ledger.count == 0U ? 0 : &local_entry,
                        9U, AV2H_CONTEXT_AV2C);
    }
    if (ledger->capacity < 1U) {
        return fail(error, AV2H_LIMIT_EXCEEDED, 9U, AV2H_CONTEXT_AV2C);
    }
    ledger->entries[0] = local_entry;
    ledger->count = 1U;
    status = av2h_sequence_header_parse(configuration->config_obus,
                                        &local_entry, &parsed, error);
    if (status != AV2H_OK) {
        if (error != 0) error->offset += 9U;
        return status;
    }
    if (!sequence_equal(&configuration->sequence, &parsed)) {
        return fail(error, AV2H_INVALID_DATA, 1U, AV2H_CONTEXT_AV2C);
    }
    return AV2H_OK;
}

Av2hStatus av2h_av2c_write(const Av2hAv2C *configuration,
                           void *output,
                           size_t output_size,
                           size_t *written) {
    Av2hByteWriter bytes;
    Av2hBitWriter bits;
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    Av2hSequenceInfo parsed;
    Av2hError error;
    unsigned char packed[4];
    size_t required;
    if (configuration == 0 || written == 0 ||
        (configuration->config_obus.data == 0 && configuration->config_obus.size != 0U) ||
        !av2h_size_add(9U, configuration->config_obus.size, &required)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (configuration->sequence.film_grain_params_present != 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (configuration->configuration_version != 1U ||
        configuration->sequence.seq_profile_idc > 31U ||
        configuration->sequence.seq_level_idx > 31U ||
        configuration->sequence.seq_tier > 1U ||
        configuration->sequence.chroma_format_idc > 7U ||
        configuration->sequence.bit_depth_idc > 7U ||
        configuration->sequence.monotonic_output_order_flag > 1U ||
        configuration->sequence.still_picture > 1U ||
        configuration->sequence.initial_display_delay_present > 1U ||
        configuration->sequence.initial_display_delay_minus_1 > 15U) {
        return AV2H_INVALID_DATA;
    }
    if (av2h_obu_parse(configuration->config_obus, &ledger, &error) != AV2H_OK ||
        ledger.count != 1U || entry.type != AV2H_OBU_SEQUENCE_HEADER ||
        av2h_sequence_header_parse(configuration->config_obus, &entry,
                                   &parsed, &error) != AV2H_OK ||
        !sequence_equal(&configuration->sequence, &parsed)) {
        return AV2H_INVALID_DATA;
    }
    *written = required;
    if (output == 0 && output_size == 0U) return AV2H_OK;
    av2h_byte_writer_init(&bytes, output, output_size);
    if (required > output_size) return AV2H_OUT_OF_MEMORY;
    av2h_bit_writer_init(&bits, packed, sizeof(packed));
    (void)av2h_bit_writer_write(&bits, configuration->sequence.seq_profile_idc, 5U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.seq_level_idx, 5U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.seq_tier, 1U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.chroma_format_idc, 3U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.bit_depth_idc, 3U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.monotonic_output_order_flag, 1U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.still_picture, 1U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.film_grain_params_present, 1U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.initial_display_delay_present, 1U);
    (void)av2h_bit_writer_write(&bits, configuration->sequence.initial_display_delay_minus_1, 4U);
    (void)av2h_bit_writer_write(&bits, 0U, 7U);
    if (bits.status != AV2H_OK) return bits.status;
    (void)av2h_byte_writer_u8(&bytes, 1U);
    (void)av2h_byte_writer_write(&bytes, packed, sizeof(packed));
    (void)av2h_byte_writer_u16be(&bytes, configuration->sequence.max_frame_width_minus_1);
    (void)av2h_byte_writer_u16be(&bytes, configuration->sequence.max_frame_height_minus_1);
    (void)av2h_byte_writer_write(&bytes, configuration->config_obus.data,
                                 configuration->config_obus.size);
    return bytes.status;
}

Av2hStatus av2h_carriage_validate(Av2hSpan config_obus,
                                  Av2hSpan item_payload,
                                  Av2hSequenceInfo *sequence,
                                  Av2hError *error) {
    Av2hObu config_entry;
    Av2hObu payload_entry;
    Av2hObuLedger config_ledger = { &config_entry, 1U, 0U };
    Av2hObuLedger payload_ledger = { &payload_entry, 1U, 0U };
    Av2hStatus status;
    if (sequence == 0) {
        return fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_OBU_ORDER);
    }
    status = av2h_obu_parse(config_obus, &config_ledger, error);
    if (status == AV2H_LIMIT_EXCEEDED) {
        return reject_extra_obu(config_obus, &config_entry, 0U,
                                AV2H_CONTEXT_OBU_ORDER, error);
    }
    if (status != AV2H_OK) return status;
    if (config_ledger.count != 1U ||
        config_entry.type != AV2H_OBU_SEQUENCE_HEADER) {
        return fail_obu(error, AV2H_INVALID_DATA,
                        config_ledger.count == 0U ? 0 : &config_entry,
                        config_ledger.count == 0U ? 0U : config_entry.offset,
                        AV2H_CONTEXT_OBU_ORDER);
    }
    status = av2h_sequence_header_parse(config_obus, &config_entry, sequence, error);
    if (status != AV2H_OK) return status;
    status = av2h_obu_parse(item_payload, &payload_ledger, error);
    if (status == AV2H_LIMIT_EXCEEDED) {
        return reject_extra_obu(item_payload, &payload_entry, 0U,
                                AV2H_CONTEXT_OBU_ORDER, error);
    }
    if (status != AV2H_OK) return status;
    if (payload_ledger.count != 1U ||
        payload_entry.type != AV2H_OBU_CLOSED_LOOP_KEY ||
        payload_entry.extension_flag != 0U ||
        payload_entry.temporal_id != 0U ||
        payload_entry.payload_size == 0U) {
        return fail_obu(error, AV2H_INVALID_DATA,
            payload_ledger.count == 0U ? 0 : &payload_entry,
            payload_ledger.count == 0U ? 0U : payload_entry.offset,
                AV2H_CONTEXT_OBU_ORDER);
    }
    return AV2H_OK;
}