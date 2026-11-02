#include "base.h"
#include "carriage.h"
#include "decoder/internal/obu.h"

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
    return av2h_obu_fail_at(error, AV2H_INVALID_DATA, &extra_entry,
                    base_offset + extra_entry.offset, context);
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
    Av2hObu local_entries[2];
    Av2hObuLedger local_ledger = { local_entries, 2U, 0U };
    Av2hSequenceInfo parsed;
    Av2hStatus status;
    if ((input.data == 0 && input.size != 0U) || configuration == 0 || ledger == 0 ||
        (ledger->entries == 0 && ledger->capacity != 0U)) {
        return av2h_obu_fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_AV2C);
    }
    ledger->count = 0U;
    if (input.size < 10U) {
        return av2h_obu_fail(error, AV2H_TRUNCATED, input.size, AV2H_CONTEXT_AV2C);
    }
    configuration->configuration_version = input.data[0];
    if (configuration->configuration_version != 1U) {
        return av2h_obu_fail(error, AV2H_UNSUPPORTED, 0U, AV2H_CONTEXT_AV2C);
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
        return av2h_obu_fail(error, AV2H_INVALID_DATA, 4U, AV2H_CONTEXT_AV2C);
    }
    configuration->sequence.max_frame_width_minus_1 = av2h_load_u16be(input.data + 5U);
    configuration->sequence.max_frame_height_minus_1 = av2h_load_u16be(input.data + 7U);
    if (configuration->sequence.film_grain_params_present != 0U) {
        return av2h_obu_fail(error, AV2H_UNSUPPORTED, 3U, AV2H_CONTEXT_AV2C);
    }
    configuration->config_obus.data = input.data + 9U;
    configuration->config_obus.size = input.size - 9U;
    status = av2h_obu_parse(configuration->config_obus, &local_ledger, error);
    if (status == AV2H_LIMIT_EXCEEDED) {
        return reject_extra_obu(configuration->config_obus, &local_entries[1],
                                9U, AV2H_CONTEXT_AV2C, error);
    }
    if (status != AV2H_OK) {
        if (error != 0) error->offset += 9U;
        return status;
    }
    if ((local_ledger.count != 1U && local_ledger.count != 2U) ||
        local_entries[0].type != AV2H_OBU_SEQUENCE_HEADER ||
        (local_ledger.count == 2U &&
         local_entries[1].type != AV2H_OBU_CONTENT_INTERPRETATION)) {
        const Av2hObu *offending = local_ledger.count == 2U ?
            &local_entries[1] :
            (local_ledger.count == 0U ? 0 : &local_entries[0]);
        return av2h_obu_fail_at(error, AV2H_INVALID_DATA,
                offending,
                9U + (offending == 0 ? 0U : offending->offset),
                AV2H_CONTEXT_AV2C);
    }
    if (ledger->capacity < local_ledger.count) {
        return av2h_obu_fail(error, AV2H_LIMIT_EXCEEDED, 9U, AV2H_CONTEXT_AV2C);
    }
    ledger->entries[0] = local_entries[0];
    if (local_ledger.count == 2U) ledger->entries[1] = local_entries[1];
    ledger->count = local_ledger.count;
    status = av2h_sequence_header_parse(configuration->config_obus,
                                        &local_entries[0], &parsed, error);
    if (status != AV2H_OK) {
        if (error != 0) error->offset += 9U;
        return status;
    }
    if (local_ledger.count == 2U) {
        Av2hColorInfo color;
        status = av2h_content_interpretation_parse(
            configuration->config_obus, &local_entries[1], &color, error);
        if (status != AV2H_OK) {
            if (error != 0) error->offset += 9U;
            return status;
        }
    }
    if (!sequence_equal(&configuration->sequence, &parsed)) {
        return av2h_obu_fail(error, AV2H_INVALID_DATA, 1U, AV2H_CONTEXT_AV2C);
    }
    return AV2H_OK;
}

Av2hStatus av2h_av2c_write(const Av2hAv2C *configuration,
                           void *output,
                           size_t output_size,
                           size_t *written) {
    Av2hByteWriter bytes;
    Av2hBitWriter bits;
    Av2hObu entries[2];
    Av2hObuLedger ledger = { entries, 2U, 0U };
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
        (ledger.count != 1U && ledger.count != 2U) ||
        entries[0].type != AV2H_OBU_SEQUENCE_HEADER ||
        (ledger.count == 2U &&
         entries[1].type != AV2H_OBU_CONTENT_INTERPRETATION) ||
        av2h_sequence_header_parse(configuration->config_obus, &entries[0],
                                   &parsed, &error) != AV2H_OK ||
        !sequence_equal(&configuration->sequence, &parsed)) {
        return AV2H_INVALID_DATA;
    }
    if (ledger.count == 2U) {
        Av2hColorInfo color;
        if (av2h_content_interpretation_parse(
                configuration->config_obus, &entries[1], &color,
                &error) != AV2H_OK) {
            return AV2H_INVALID_DATA;
        }
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

Av2hStatus av2h_carriage_validate_metadata(Av2hSpan config_obus,
                                           Av2hSpan item_payload,
                                           Av2hSequenceInfo *sequence,
                                           Av2hColorInfo *color,
                                           uint8_t *color_present,
                                           Av2hError *error) {
    Av2hObu config_entries[2];
    Av2hObu payload_entry;
    Av2hObuLedger config_ledger = { config_entries, 2U, 0U };
    Av2hObuLedger payload_ledger = { &payload_entry, 1U, 0U };
    Av2hStatus status;
    if (sequence == 0 || (color == 0) != (color_present == 0)) {
        return av2h_obu_fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_OBU_ORDER);
    }
    if (color_present != 0) *color_present = 0U;
    status = av2h_obu_parse(config_obus, &config_ledger, error);
    if (status == AV2H_LIMIT_EXCEEDED) {
        return reject_extra_obu(config_obus, &config_entries[1], 0U,
                                AV2H_CONTEXT_OBU_ORDER, error);
    }
    if (status != AV2H_OK) return status;
    if ((config_ledger.count != 1U && config_ledger.count != 2U) ||
        config_entries[0].type != AV2H_OBU_SEQUENCE_HEADER ||
        (config_ledger.count == 2U &&
         config_entries[1].type != AV2H_OBU_CONTENT_INTERPRETATION)) {
        const Av2hObu *offending = config_ledger.count == 2U ?
            &config_entries[1] :
            (config_ledger.count == 0U ? 0 : &config_entries[0]);
        return av2h_obu_fail_at(error, AV2H_INVALID_DATA,
                        offending,
                        offending == 0 ? 0U : offending->offset,
                        AV2H_CONTEXT_OBU_ORDER);
    }
    status = av2h_sequence_header_parse(config_obus, &config_entries[0],
                                        sequence, error);
    if (status != AV2H_OK) return status;
    if (config_ledger.count == 2U) {
        Av2hColorInfo ignored;
        status = av2h_content_interpretation_parse(
            config_obus, &config_entries[1], color != 0 ? color : &ignored,
            error);
        if (status != AV2H_OK) return status;
        if (color_present != 0) *color_present = 1U;
    }
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
        return av2h_obu_fail_at(error, AV2H_INVALID_DATA,
            payload_ledger.count == 0U ? 0 : &payload_entry,
            payload_ledger.count == 0U ? 0U : payload_entry.offset,
                AV2H_CONTEXT_OBU_ORDER);
    }
    return AV2H_OK;
}

Av2hStatus av2h_carriage_validate(Av2hSpan config_obus,
                                  Av2hSpan item_payload,
                                  Av2hSequenceInfo *sequence,
                                  Av2hError *error) {
    return av2h_carriage_validate_metadata(
        config_obus, item_payload, sequence, 0, 0, error);
}
