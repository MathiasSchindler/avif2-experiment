#include "base.h"

#include "internal/obu.h"

#include <stddef.h>

void av2h_color_info_default(Av2hColorInfo *color) {
    if (color == NULL) return;
    color->color_description_present = 0U;
    color->color_description_idc = 0U;
    color->color_primaries = 2U;
    color->transfer_characteristics = 2U;
    color->matrix_coefficients = 2U;
    color->full_range_flag = 0U;
    color->chroma_sample_position_x =
        AV2H_CHROMA_SAMPLE_POSITION_UNSPECIFIED;
    color->chroma_sample_position_y =
        AV2H_CHROMA_SAMPLE_POSITION_UNSPECIFIED;
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
    if (obu == NULL || color == NULL || input.data == NULL ||
        obu->type != AV2H_OBU_CONTENT_INTERPRETATION ||
        obu->payload_offset > input.size || obu->payload_size > input.size - obu->payload_offset) {
        return av2h_obu_fail_at(error, AV2H_INVALID_ARGUMENT, obu, 0U,
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
        return av2h_obu_fail_at(error, AV2H_INVALID_DATA, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (color->color_description_present != 0U) {
        identifier = read_rice_golomb(&reader, 2U);
        if (identifier > 127U) {
            return av2h_obu_fail_at(error,
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
        uint32_t position = av2h_bit_reader_uvlc(&reader);
        if (position > AV2H_CHROMA_SAMPLE_POSITION_UNSPECIFIED) {
            return av2h_obu_fail_at(error, AV2H_INVALID_DATA, obu,
                            av2h_bit_reader_offset(&reader),
                            AV2H_CONTEXT_CONTENT_INTERPRETATION);
        }
        color->chroma_sample_position_x = (uint8_t)position;
        color->chroma_sample_position_y = (uint8_t)position;
        if (scan_type != 1U) {
            position = av2h_bit_reader_uvlc(&reader);
            if (position > AV2H_CHROMA_SAMPLE_POSITION_UNSPECIFIED) {
                return av2h_obu_fail_at(error, AV2H_INVALID_DATA, obu,
                                av2h_bit_reader_offset(&reader),
                                AV2H_CONTEXT_CONTENT_INTERPRETATION);
            }
            color->chroma_sample_position_y = (uint8_t)position;
        }
    }
    if (aspect_present != 0U || timing_present != 0U) {
        return av2h_obu_fail_at(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (av2h_bit_reader_read(&reader, 1U) != 0U) {
        return av2h_obu_fail_at(error, AV2H_UNSUPPORTED, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (reader.status != AV2H_OK) {
        return av2h_obu_fail_at(error, reader.status, obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    if (!av2h_obu_check_trailing_bits(&reader)) {
        return av2h_obu_fail_at(error,
                        reader.status == AV2H_OK ? AV2H_INVALID_DATA
                                                 : reader.status,
                        obu,
                        av2h_bit_reader_offset(&reader),
                        AV2H_CONTEXT_CONTENT_INTERPRETATION);
    }
    return AV2H_OK;
}
