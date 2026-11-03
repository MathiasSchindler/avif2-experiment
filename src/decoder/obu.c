#include "internal/obu.h"

#include "base.h"

#include <stddef.h>

static size_t leb128_size(size_t value) {
    size_t size = 1U;
    while (value >= 128U) {
        value >>= 7;
        ++size;
    }
    return size;
}

static int obu_global_xlayer_allowed(uint8_t type) {
    return type == AV2H_OBU_TEMPORAL_DELIMITER || type == AV2H_OBU_METADATA_SHORT ||
           type == AV2H_OBU_METADATA_GROUP || type == AV2H_OBU_BUFFER_REMOVAL_TIMING ||
           type == AV2H_OBU_LAYER_CONFIGURATION_RECORD || type == AV2H_OBU_ATLAS_SEGMENT ||
           type == AV2H_OBU_OPERATING_POINT_SET ||
           type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION || type == AV2H_OBU_PADDING;
}

static int obu_layer_ids_valid(const Av2hObu *obu) {
    if (obu->xlayer_id == 31U && (obu->temporal_id != 0U || obu->mlayer_id != 0U)) {
        return 0;
    }
    if (obu->type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION && obu->xlayer_id != 31U) {
        return 0;
    }
    if (obu->type == AV2H_OBU_TEMPORAL_DELIMITER && obu->xlayer_id != 31U) {
        return 0;
    }
    if ((obu->type == AV2H_OBU_SEQUENCE_HEADER || obu->type == AV2H_OBU_TEMPORAL_DELIMITER ||
         obu->type == AV2H_OBU_LAYER_CONFIGURATION_RECORD ||
         obu->type == AV2H_OBU_OPERATING_POINT_SET || obu->type == AV2H_OBU_ATLAS_SEGMENT) &&
        (obu->temporal_id != 0U || obu->mlayer_id != 0U)) {
        return 0;
    }
    if (obu->xlayer_id == 31U && !obu_global_xlayer_allowed(obu->type)) {
        return 0;
    }
    if ((obu->type == AV2H_OBU_CLOSED_LOOP_KEY || obu->type == AV2H_OBU_OPEN_LOOP_KEY) &&
        obu->temporal_id != 0U) {
        return 0;
    }
    return 1;
}

Av2hStatus av2h_leb128_read(Av2hSpan input, size_t *value, size_t *encoded_size, Av2hError *error) {
    uint64_t result = 0U;
    size_t index;
    if ((input.data == NULL && input.size != 0U) || value == NULL || encoded_size == NULL) {
        return av2h_obu_fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_LEB128);
    }
    for (index = 0U; index < 8U; ++index) {
        unsigned char byte;
        if (index >= input.size) {
            return av2h_obu_fail(error, AV2H_TRUNCATED, index, AV2H_CONTEXT_LEB128);
        }
        byte = input.data[index];
        result |= (uint64_t)(byte & 0x7fU) << (index * 7U);
        if ((byte & 0x80U) == 0U) {
            if (result > (uint64_t)SIZE_MAX || leb128_size((size_t)result) != index + 1U) {
                return av2h_obu_fail(error, AV2H_INVALID_DATA, 0U, AV2H_CONTEXT_LEB128);
            }
            *value = (size_t)result;
            *encoded_size = index + 1U;
            return AV2H_OK;
        }
    }
    return av2h_obu_fail(error, AV2H_INVALID_DATA, 0U, AV2H_CONTEXT_LEB128);
}

Av2hStatus av2h_leb128_write(size_t value, void *output, size_t output_size, size_t *written) {
    unsigned char *bytes = (unsigned char *)output;
    size_t required = leb128_size(value);
    size_t index;
    if (written == NULL || (output == NULL && output_size != 0U))
        return AV2H_INVALID_ARGUMENT;
    if (value >= ((uint64_t)1U << 56))
        return AV2H_OVERFLOW;
    *written = required;
    if (output == NULL && output_size == 0U)
        return AV2H_OK;
    if (required > output_size)
        return AV2H_OUT_OF_MEMORY;
    for (index = 0U; index < required; ++index) {
        unsigned char byte = (unsigned char)(value & 0x7fU);
        value >>= 7;
        if (value != 0U)
            byte |= 0x80U;
        bytes[index] = byte;
    }
    return AV2H_OK;
}

Av2hStatus av2h_obu_parse(Av2hSpan input, Av2hObuLedger *ledger, Av2hError *error) {
    size_t offset = 0U;
    if ((input.data == NULL && input.size != 0U) || ledger == NULL ||
        (ledger->entries == NULL && ledger->capacity != 0U)) {
        return av2h_obu_fail(error, AV2H_INVALID_ARGUMENT, 0U, AV2H_CONTEXT_OBU_HEADER);
    }
    ledger->count = 0U;
    while (offset < input.size) {
        Av2hSpan remaining = {input.data + offset, input.size - offset};
        size_t obu_size;
        size_t length_size;
        size_t end;
        unsigned char first;
        Av2hObu *obu;
        Av2hStatus status = av2h_leb128_read(remaining, &obu_size, &length_size, error);
        if (status != AV2H_OK) {
            if (error != NULL) {
                error->offset += offset;
                error->obu_index = ledger->count;
            }
            return status;
        }
        if (obu_size == 0U) {
            return av2h_obu_fail(error, AV2H_INVALID_DATA, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        if (!av2h_size_add(offset, length_size, &end) || !av2h_size_add(end, obu_size, &end)) {
            return av2h_obu_fail(error, AV2H_OVERFLOW, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        if (end > input.size) {
            return av2h_obu_fail(error, AV2H_TRUNCATED, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        first = input.data[offset + length_size];
        if (((first >> 2) & 31U) == 0U || ((first >> 2) & 31U) > 25U) {
            return av2h_obu_fail(error, AV2H_INVALID_DATA, offset + length_size,
                                 AV2H_CONTEXT_OBU_HEADER);
        }
        if (ledger->count >= ledger->capacity) {
            return av2h_obu_fail(error, AV2H_LIMIT_EXCEEDED, offset, AV2H_CONTEXT_OBU_HEADER);
        }
        obu = &ledger->entries[ledger->count];
        obu->index = ledger->count;
        obu->offset = offset;
        obu->length_field_size = length_size;
        obu->obu_size = obu_size;
        obu->extension_flag = (uint8_t)(first >> 7);
        obu->header_size = obu->extension_flag != 0U ? 2U : 1U;
        if (obu_size < obu->header_size) {
            return av2h_obu_fail(error, AV2H_INVALID_DATA, offset + length_size,
                                 AV2H_CONTEXT_OBU_HEADER);
        }
        obu->type = (uint8_t)((first >> 2) & 31U);
        obu->temporal_id = (uint8_t)(first & 3U);
        obu->mlayer_id = 0U;
        obu->xlayer_id = obu->type == AV2H_OBU_TEMPORAL_DELIMITER ||
                                 obu->type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION
                             ? 31U
                             : 0U;
        if (obu->extension_flag != 0U) {
            unsigned char second = input.data[offset + length_size + 1U];
            obu->mlayer_id = (uint8_t)(second >> 5);
            obu->xlayer_id = (uint8_t)(second & 31U);
        }
        if (!obu_layer_ids_valid(obu)) {
            return av2h_obu_fail_at(error, AV2H_INVALID_DATA, obu, offset + length_size,
                                    AV2H_CONTEXT_OBU_HEADER);
        }
        obu->payload_offset = offset + length_size + obu->header_size;
        obu->payload_size = obu_size - obu->header_size;
        ++ledger->count;
        offset = end;
    }
    if (ledger->count == 0U) {
        return av2h_obu_fail(error, AV2H_INVALID_DATA, 0U, AV2H_CONTEXT_OBU_HEADER);
    }
    return AV2H_OK;
}
