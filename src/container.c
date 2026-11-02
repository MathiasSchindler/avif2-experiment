#include "base.h"
#include "carriage.h"
#include "container.h"
#include "pixel_format.h"

enum {
    PROPERTY_UNKNOWN = 0,
    PROPERTY_AV2C,
    PROPERTY_ISPE,
    PROPERTY_PIXI,
    PROPERTY_COLR
};

typedef struct {
    size_t offset;
    size_t payload_offset;
    size_t end;
    size_t payload_size;
    uint32_t type;
} ContainerBox;

typedef struct {
    uint32_t type;
    uint8_t kind;
} ContainerProperty;

typedef struct {
    uint16_t primary_item_id;
    uint16_t location_item_id;
    uint16_t information_item_id;
    uint16_t association_item_id;
    uint32_t extent_offset;
    uint32_t extent_length;
    uint32_t width;
    uint32_t height;
    uint8_t channel_count;
    uint8_t bits_per_channel;
    Av2hAv2C configuration;
    Av2hColorInfo color;
    ContainerProperty properties[16];
    size_t property_count;
    uint8_t property_counts[5];
    uint8_t association_counts[5];
    uint8_t association_essential[5];
} ContainerState;

Av2hStatus av2h_container_fail(Av2hError *error,
                            Av2hStatus status,
                            size_t offset) {
    if (error != 0) {
        error->status = status;
        error->offset = offset;
        error->context = AV2H_CONTEXT_HEIF;
        error->obu_index = SIZE_MAX;
        error->obu_type = 0U;
    }
    return status;
}

Av2hStatus av2h_container_sequence_pixel_metadata(
    const Av2hSequenceInfo *sequence,
    ContainerPixelMetadata *metadata) {
    if (sequence == 0 || metadata == 0 ||
        sequence->chroma_format_idc > 2U) {
        return AV2H_INVALID_ARGUMENT;
    }
    metadata->channel_count =
        sequence->chroma_format_idc == 1U ? 1U : 3U;
    if (sequence->bit_depth_idc == 0U) {
        metadata->bits_per_channel = 10U;
    } else if (sequence->bit_depth_idc == 1U) {
        metadata->bits_per_channel = 8U;
    } else if (sequence->bit_depth_idc == 2U) {
        metadata->bits_per_channel = 12U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

static Av2hStatus validate_pixel_metadata(
    const Av2hSequenceInfo *sequence,
    uint8_t channel_count,
    uint8_t bits_per_channel,
    Av2hPixelFormat *decoded_format) {
    ContainerPixelMetadata expected;
    Av2hStatus status = av2h_container_sequence_pixel_metadata(sequence, &expected);
    if (status != AV2H_OK) return status;
    if (channel_count != expected.channel_count ||
        bits_per_channel != expected.bits_per_channel) {
        return AV2H_INVALID_DATA;
    }
    if (expected.bits_per_channel == 8U) {
        if (!av2h_pixel_format_init_8bit(sequence->chroma_format_idc,
                                         decoded_format)) {
            return AV2H_UNSUPPORTED;
        }
    } else if (expected.bits_per_channel == 10U &&
               ((sequence->seq_profile_idc == 0U &&
                 sequence->chroma_format_idc == 0U) ||
                (sequence->seq_profile_idc == 4U &&
                 sequence->chroma_format_idc == 2U))) {
        int initialized = sequence->chroma_format_idc == 2U ?
            av2h_pixel_format_init_10bit_444(decoded_format) :
            av2h_pixel_format_init_10bit_420(decoded_format);
        if (!initialized) {
            return AV2H_UNSUPPORTED;
        }
    } else {
        return AV2H_UNSUPPORTED;
    }
    if (decoded_format->bit_depth != expected.bits_per_channel) {
        return AV2H_INVALID_DATA;
    }
    return AV2H_OK;
}

int av2h_container_color_description_equal(const Av2hColorInfo *left,
                                   const Av2hColorInfo *right) {
    return left->color_description_idc == right->color_description_idc &&
           left->color_primaries == right->color_primaries &&
           left->transfer_characteristics ==
               right->transfer_characteristics &&
           left->matrix_coefficients == right->matrix_coefficients &&
           left->full_range_flag == right->full_range_flag;
}

static Av2hStatus read_box(Av2hSpan input,
                           size_t offset,
                           size_t limit,
                           ContainerBox *box,
                           Av2hError *error) {
    Av2hBoxHeader header;
    Av2hStatus status;
    if (box == 0 || limit > input.size || offset > limit) {
        return av2h_container_fail(error, AV2H_INVALID_ARGUMENT, offset);
    }
    status = av2h_box_header_parse((Av2hSpan){ input.data, limit }, offset,
                                   &header, error);
    if (status != AV2H_OK) {
        if (error != 0) error->context = AV2H_CONTEXT_HEIF;
        return status;
    }
    if (header.header_size != 8U) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED, offset);
    }
    if (av2h_load_u32be(input.data + offset) == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, offset);
    }
    box->offset = offset;
    box->payload_offset = offset + header.header_size;
    box->end = offset + (size_t)header.size;
    box->payload_size = (size_t)header.payload_size;
    box->type = header.type;
    return AV2H_OK;
}

static Av2hStatus reader_result(Av2hByteReader *reader,
                                Av2hError *error) {
    if (reader->status != AV2H_OK) {
        return av2h_container_fail(error, reader->status,
                         av2h_byte_reader_offset(reader));
    }
    if (av2h_byte_reader_remaining(reader) != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(reader));
    }
    return AV2H_OK;
}

static Av2hStatus reader_status(Av2hByteReader *reader,
                                Av2hError *error) {
    if (reader->status == AV2H_OK) return AV2H_OK;
    return av2h_container_fail(error, reader->status,
                     av2h_byte_reader_offset(reader));
}

static Av2hStatus full_box_reader(Av2hSpan input,
                                  const ContainerBox *box,
                                  uint8_t expected_version,
                                  Av2hByteReader *reader,
                                  Av2hError *error) {
    uint32_t full;
    if (box->payload_size < 4U) {
        return av2h_container_fail(error, AV2H_TRUNCATED, box->payload_offset);
    }
    av2h_byte_reader_init(reader, input.data + box->payload_offset,
                          box->payload_size, box->payload_offset);
    full = av2h_byte_reader_u32be(reader);
    if ((uint8_t)(full >> 24) != expected_version) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED, box->payload_offset);
    }
    if ((full & 0x00ffffffU) != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, box->payload_offset + 1U);
    }
    return AV2H_OK;
}

static Av2hStatus parse_ftyp(Av2hSpan input,
                             const ContainerBox *box,
                             Av2hError *error) {
    if (box->payload_size != 12U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, box->payload_offset);
    }
    if (av2h_load_u32be(input.data + box->payload_offset) != BRAND_MIF1 ||
        av2h_load_u32be(input.data + box->payload_offset + 4U) != 0U ||
        av2h_load_u32be(input.data + box->payload_offset + 8U) != BRAND_MIF1) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED, box->payload_offset);
    }
    return AV2H_OK;
}

static Av2hStatus parse_hdlr(Av2hSpan input,
                             const ContainerBox *box,
                             Av2hError *error) {
    Av2hByteReader reader;
    const unsigned char *name;
    size_t name_size;
    uint32_t predefined;
    uint32_t handler;
    uint32_t reserved[3];
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    predefined = av2h_byte_reader_u32be(&reader);
    handler = av2h_byte_reader_u32be(&reader);
    reserved[0] = av2h_byte_reader_u32be(&reader);
    reserved[1] = av2h_byte_reader_u32be(&reader);
    reserved[2] = av2h_byte_reader_u32be(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (predefined != 0U || handler != HANDLER_PICT ||
        reserved[0] != 0U || reserved[1] != 0U || reserved[2] != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader));
    }
    name_size = av2h_byte_reader_remaining(&reader);
    name = av2h_byte_reader_take(&reader, name_size);
    if (name_size == 0U || name == 0 || name[name_size - 1U] != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader));
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_pitm(Av2hSpan input,
                             const ContainerBox *box,
                             ContainerState *state,
                             Av2hError *error) {
    Av2hByteReader reader;
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    state->primary_item_id = av2h_byte_reader_u16be(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (state->primary_item_id == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader) - 2U);
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_iloc(Av2hSpan input,
                             const ContainerBox *box,
                             ContainerState *state,
                             Av2hError *error) {
    Av2hByteReader reader;
    uint8_t field_sizes;
    uint8_t base_sizes;
    uint16_t item_count;
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    field_sizes = av2h_byte_reader_u8(&reader);
    base_sizes = av2h_byte_reader_u8(&reader);
    item_count = av2h_byte_reader_u16be(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (field_sizes != 0x44U || base_sizes != 0U) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED,
                         box->payload_offset + 4U);
    }
    if (item_count != 1U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         box->payload_offset + 6U);
    }
    state->location_item_id = av2h_byte_reader_u16be(&reader);
    {
        uint16_t data_reference_index = av2h_byte_reader_u16be(&reader);
        uint16_t extent_count = av2h_byte_reader_u16be(&reader);
        status = reader_status(&reader, error);
        if (status != AV2H_OK) return status;
        if (state->location_item_id == 0U || data_reference_index != 0U ||
            extent_count != 1U) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader));
        }
    }
    state->extent_offset = av2h_byte_reader_u32be(&reader);
    state->extent_length = av2h_byte_reader_u32be(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (state->extent_length == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader) - 4U);
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_infe(Av2hSpan input,
                             const ContainerBox *box,
                             ContainerState *state,
                             Av2hError *error) {
    Av2hByteReader reader;
    const unsigned char *name;
    size_t name_size;
    Av2hStatus status = full_box_reader(input, box, 2U, &reader, error);
    if (status != AV2H_OK) return status;
    state->information_item_id = av2h_byte_reader_u16be(&reader);
    {
        uint16_t protection_index = av2h_byte_reader_u16be(&reader);
        uint32_t item_type = av2h_byte_reader_u32be(&reader);
        status = reader_status(&reader, error);
        if (status != AV2H_OK) return status;
        if (state->information_item_id == 0U || protection_index != 0U ||
            item_type != ITEM_AV02) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader));
        }
    }
    name_size = av2h_byte_reader_remaining(&reader);
    name = av2h_byte_reader_take(&reader, name_size);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (name_size == 0U || name == 0 || name[name_size - 1U] != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader));
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_iinf(Av2hSpan input,
                             const ContainerBox *box,
                             ContainerState *state,
                             Av2hError *error) {
    Av2hByteReader reader;
    ContainerBox infe;
    uint16_t entry_count;
    size_t child_offset;
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    entry_count = av2h_byte_reader_u16be(&reader);
    child_offset = av2h_byte_reader_offset(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (entry_count != 1U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, child_offset - 2U);
    }
    status = read_box(input, child_offset, box->end, &infe, error);
    if (status != AV2H_OK) return status;
    if (infe.type != BOX_INFE || infe.end != box->end) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, child_offset);
    }
    return parse_infe(input, &infe, state, error);
}

static Av2hStatus parse_av2c_property(Av2hSpan input,
                                      const ContainerBox *box,
                                      ContainerState *state,
                                      Av2hError *error) {
    Av2hObu entries[2];
    Av2hObuLedger ledger = { entries, 2U, 0U };
    Av2hSpan payload = {
        input.data + box->payload_offset,
        box->payload_size
    };
    Av2hStatus status = av2h_av2c_parse(payload, &state->configuration,
                                        &ledger, error);
    if (status != AV2H_OK && error != 0) error->offset += box->payload_offset;
    return status;
}

static Av2hStatus parse_ispe_property(Av2hSpan input,
                                      const ContainerBox *box,
                                      ContainerState *state,
                                      Av2hError *error) {
    Av2hByteReader reader;
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    state->width = av2h_byte_reader_u32be(&reader);
    state->height = av2h_byte_reader_u32be(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (state->width == 0U || state->height == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         box->payload_offset + 4U);
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_pixi_property(Av2hSpan input,
                                      const ContainerBox *box,
                                      ContainerState *state,
                                      Av2hError *error) {
    Av2hByteReader reader;
    size_t index;
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    state->channel_count = av2h_byte_reader_u8(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (state->channel_count == 0U || state->channel_count > 4U) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED,
                         box->payload_offset + 4U);
    }
    state->bits_per_channel = av2h_byte_reader_u8(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    for (index = 1U; index < state->channel_count; ++index) {
        uint8_t bits = av2h_byte_reader_u8(&reader);
        status = reader_status(&reader, error);
        if (status != AV2H_OK) return status;
        if (bits != state->bits_per_channel) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader) - 1U);
        }
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_colr_property(Av2hSpan input,
                                      const ContainerBox *box,
                                      ContainerState *state,
                                      Av2hError *error) {
    Av2hByteReader reader;
    uint16_t primaries;
    uint16_t transfer;
    uint16_t matrix;
    uint8_t range;
    av2h_byte_reader_init(&reader, input.data + box->payload_offset,
                          box->payload_size, box->payload_offset);
    {
        uint32_t color_type = av2h_byte_reader_u32be(&reader);
        Av2hStatus status = reader_status(&reader, error);
        if (status != AV2H_OK) return status;
        if (color_type != COLOR_NCLX) {
            return av2h_container_fail(error, AV2H_UNSUPPORTED, box->payload_offset);
        }
    }
    primaries = av2h_byte_reader_u16be(&reader);
    transfer = av2h_byte_reader_u16be(&reader);
    matrix = av2h_byte_reader_u16be(&reader);
    range = av2h_byte_reader_u8(&reader);
    {
        Av2hStatus status = reader_status(&reader, error);
        if (status != AV2H_OK) return status;
    }
    if (primaries > UINT8_MAX || transfer > UINT8_MAX || matrix > UINT8_MAX) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED, box->payload_offset);
    }
    if ((range & 0x7fU) != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader) - 1U);
    }
    state->color.color_description_present = 1U;
    state->color.color_description_idc = 0U;
    state->color.color_primaries = (uint8_t)primaries;
    state->color.transfer_characteristics = (uint8_t)transfer;
    state->color.matrix_coefficients = (uint8_t)matrix;
    state->color.full_range_flag = (uint8_t)(range >> 7);
    state->color.chroma_sample_position_x =
        AV2H_CHROMA_SAMPLE_POSITION_UNSPECIFIED;
    state->color.chroma_sample_position_y =
        AV2H_CHROMA_SAMPLE_POSITION_UNSPECIFIED;
    return reader_result(&reader, error);
}

static Av2hStatus parse_ipco(Av2hSpan input,
                              const ContainerBox *box,
                              ContainerState *state,
                              Av2hError *error) {
    size_t offset = box->payload_offset;
    while (offset < box->end) {
        ContainerBox property;
        uint8_t kind = PROPERTY_UNKNOWN;
        Av2hStatus status;
        if (state->property_count == 16U) {
            return av2h_container_fail(error, AV2H_LIMIT_EXCEEDED, offset);
        }
        status = read_box(input, offset, box->end, &property, error);
        if (status != AV2H_OK) return status;
        if (property.type == BOX_AV2C) kind = PROPERTY_AV2C;
        else if (property.type == BOX_ISPE) kind = PROPERTY_ISPE;
        else if (property.type == BOX_PIXI) kind = PROPERTY_PIXI;
        else if (property.type == BOX_COLR) kind = PROPERTY_COLR;
        state->properties[state->property_count].type = property.type;
        state->properties[state->property_count].kind = kind;
        ++state->property_count;
        if (kind != PROPERTY_UNKNOWN) {
            ++state->property_counts[kind];
            if (state->property_counts[kind] != 1U) {
                return av2h_container_fail(error, AV2H_INVALID_DATA, property.offset);
            }
        }
        if (kind == PROPERTY_AV2C) {
            status = parse_av2c_property(input, &property, state, error);
        } else if (kind == PROPERTY_ISPE) {
            status = parse_ispe_property(input, &property, state, error);
        } else if (kind == PROPERTY_PIXI) {
            status = parse_pixi_property(input, &property, state, error);
        } else if (kind == PROPERTY_COLR) {
            status = parse_colr_property(input, &property, state, error);
        } else {
            status = AV2H_OK;
        }
        if (status != AV2H_OK) return status;
        offset = property.end;
    }
    if (state->property_count == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, box->payload_offset);
    }
    return AV2H_OK;
}

static Av2hStatus parse_ipma(Av2hSpan input,
                              const ContainerBox *box,
                              ContainerState *state,
                              Av2hError *error) {
    Av2hByteReader reader;
    uint32_t entry_count;
    uint8_t association_count;
    uint32_t seen_indices = 0U;
    uint8_t previous_property_index = 0U;
    size_t index;
    Av2hStatus status = full_box_reader(input, box, 0U, &reader, error);
    if (status != AV2H_OK) return status;
    entry_count = av2h_byte_reader_u32be(&reader);
    state->association_item_id = av2h_byte_reader_u16be(&reader);
    association_count = av2h_byte_reader_u8(&reader);
    status = reader_status(&reader, error);
    if (status != AV2H_OK) return status;
    if (entry_count != 1U || state->association_item_id == 0U ||
        association_count == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA,
                         av2h_byte_reader_offset(&reader));
    }
    for (index = 0U; index < association_count; ++index) {
        uint8_t association = av2h_byte_reader_u8(&reader);
        uint8_t property_index = (uint8_t)(association & 0x7fU);
        uint8_t essential = (uint8_t)(association >> 7);
        uint8_t kind;
        uint32_t mask;
        status = reader_status(&reader, error);
        if (status != AV2H_OK) return status;
        if (property_index == 0U || property_index > state->property_count) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader) - 1U);
        }
        if (property_index <= previous_property_index) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader) - 1U);
        }
        previous_property_index = property_index;
        mask = (uint32_t)1U << (property_index - 1U);
        if ((seen_indices & mask) != 0U) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader) - 1U);
        }
        seen_indices |= mask;
        kind = state->properties[property_index - 1U].kind;
        if (kind == PROPERTY_UNKNOWN) {
            if (essential != 0U) {
                return av2h_container_fail(error, AV2H_UNSUPPORTED,
                                 av2h_byte_reader_offset(&reader) - 1U);
            }
            continue;
        }
        ++state->association_counts[kind];
        state->association_essential[kind] = essential;
        if (state->association_counts[kind] != 1U) {
            return av2h_container_fail(error, AV2H_INVALID_DATA,
                             av2h_byte_reader_offset(&reader) - 1U);
        }
    }
    return reader_result(&reader, error);
}

static Av2hStatus parse_iprp(Av2hSpan input,
                              const ContainerBox *box,
                              ContainerState *state,
                              Av2hError *error) {
    ContainerBox ipco;
    ContainerBox ipma;
    Av2hStatus status = read_box(input, box->payload_offset, box->end,
                                 &ipco, error);
    if (status != AV2H_OK) return status;
    if (ipco.type != BOX_IPCO) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, ipco.offset);
    }
    status = parse_ipco(input, &ipco, state, error);
    if (status != AV2H_OK) return status;
    status = read_box(input, ipco.end, box->end, &ipma, error);
    if (status != AV2H_OK) return status;
    if (ipma.type != BOX_IPMA || ipma.end != box->end) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, ipma.offset);
    }
    return parse_ipma(input, &ipma, state, error);
}

static Av2hStatus parse_meta(Av2hSpan input,
                              const ContainerBox *box,
                              ContainerState *state,
                              Av2hError *error) {
    size_t offset;
    int seen_hdlr = 0;
    int seen_pitm = 0;
    int seen_iloc = 0;
    int seen_iinf = 0;
    int seen_iprp = 0;
    if (box->payload_size < 4U) {
        return av2h_container_fail(error, AV2H_TRUNCATED, box->payload_offset);
    }
    if (av2h_load_u32be(input.data + box->payload_offset) != 0U) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED, box->payload_offset);
    }
    offset = box->payload_offset + 4U;
    while (offset < box->end) {
        ContainerBox child;
        Av2hStatus status = read_box(input, offset, box->end, &child, error);
        if (status != AV2H_OK) return status;
        if (!seen_hdlr && child.type != BOX_HDLR) {
            return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
        }
        if (child.type == BOX_HDLR) {
            if (seen_hdlr) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_hdlr = 1;
            status = parse_hdlr(input, &child, error);
        } else if (child.type == BOX_PITM) {
            if (seen_pitm) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_pitm = 1;
            status = parse_pitm(input, &child, state, error);
        } else if (child.type == BOX_ILOC) {
            if (seen_iloc) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_iloc = 1;
            status = parse_iloc(input, &child, state, error);
        } else if (child.type == BOX_IINF) {
            if (seen_iinf) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_iinf = 1;
            status = parse_iinf(input, &child, state, error);
        } else if (child.type == BOX_IPRP) {
            if (seen_iprp) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_iprp = 1;
            status = parse_iprp(input, &child, state, error);
        } else {
            return av2h_container_fail(error, AV2H_UNSUPPORTED, child.offset);
        }
        if (status != AV2H_OK) return status;
        offset = child.end;
    }
    if (!seen_hdlr || !seen_pitm || !seen_iloc || !seen_iinf || !seen_iprp) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, box->offset);
    }
    return AV2H_OK;
}

static Av2hStatus probe_ftyp(Av2hSpan input,
                             const ContainerBox *box,
                             Av2hContainerProbe *probe,
                             Av2hError *error) {
    size_t offset;
    if (box->payload_size < 8U || (box->payload_size % 4U) != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, box->payload_offset);
    }
    for (offset = box->payload_offset; offset < box->end; offset += 4U) {
        uint32_t brand;
        if (offset == box->payload_offset + 4U) continue;
        brand = av2h_load_u32be(input.data + offset);
        if (brand == BRAND_MIF1) probe->has_mif1_brand = 1U;
        if (brand == BRAND_AVIF) probe->has_avif_brand = 1U;
    }
    return AV2H_OK;
}

static Av2hStatus probe_iprp(Av2hSpan input,
                             const ContainerBox *box,
                             ContainerState *state,
                             Av2hContainerProbe *probe,
                             Av2hError *error) {
    ContainerBox ipco;
    ContainerBox ipma;
    size_t offset;
    Av2hStatus status = read_box(input, box->payload_offset, box->end,
                                 &ipco, error);
    if (status != AV2H_OK) return status;
    if (ipco.type != BOX_IPCO) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, ipco.offset);
    }
    offset = ipco.payload_offset;
    while (offset < ipco.end) {
        ContainerBox property;
        uint8_t kind = PROPERTY_UNKNOWN;
        if (state->property_count == 16U) {
            return av2h_container_fail(error, AV2H_LIMIT_EXCEEDED, offset);
        }
        status = read_box(input, offset, ipco.end, &property, error);
        if (status != AV2H_OK) return status;
        if (property.type == BOX_AV2C) {
            if (probe->av2c_payload.data != 0) {
                return av2h_container_fail(error, AV2H_INVALID_DATA, property.offset);
            }
            kind = PROPERTY_AV2C;
            probe->av2c_payload.data = input.data + property.payload_offset;
            probe->av2c_payload.size = property.payload_size;
        }
        state->properties[state->property_count].type = property.type;
        state->properties[state->property_count].kind = kind;
        ++state->property_count;
        offset = property.end;
    }
    status = read_box(input, ipco.end, box->end, &ipma, error);
    if (status != AV2H_OK) return status;
    if (ipma.type != BOX_IPMA || ipma.end != box->end) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, ipma.offset);
    }
    status = parse_ipma(input, &ipma, state, error);
    if (status != AV2H_OK) return status;
    probe->av2c_associated = state->association_counts[PROPERTY_AV2C];
    return AV2H_OK;
}

static Av2hStatus probe_meta(Av2hSpan input,
                             const ContainerBox *box,
                             ContainerState *state,
                             Av2hContainerProbe *probe,
                             Av2hError *error) {
    size_t offset;
    int seen_hdlr = 0;
    int seen_pitm = 0;
    int seen_iloc = 0;
    int seen_iinf = 0;
    int seen_iprp = 0;
    if (box->payload_size < 4U) {
        return av2h_container_fail(error, AV2H_TRUNCATED, box->payload_offset);
    }
    if (av2h_load_u32be(input.data + box->payload_offset) != 0U) {
        return av2h_container_fail(error, AV2H_UNSUPPORTED, box->payload_offset);
    }
    offset = box->payload_offset + 4U;
    while (offset < box->end) {
        ContainerBox child;
        Av2hStatus status = read_box(input, offset, box->end, &child, error);
        if (status != AV2H_OK) return status;
        if (!seen_hdlr && child.type != BOX_HDLR) {
            return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
        }
        if (child.type == BOX_HDLR) {
            if (seen_hdlr) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_hdlr = 1;
            status = parse_hdlr(input, &child, error);
        } else if (child.type == BOX_PITM) {
            if (seen_pitm) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_pitm = 1;
            status = parse_pitm(input, &child, state, error);
        } else if (child.type == BOX_ILOC) {
            if (seen_iloc) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_iloc = 1;
            status = parse_iloc(input, &child, state, error);
        } else if (child.type == BOX_IINF) {
            if (seen_iinf) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_iinf = 1;
            status = parse_iinf(input, &child, state, error);
        } else if (child.type == BOX_IPRP) {
            if (seen_iprp) return av2h_container_fail(error, AV2H_INVALID_DATA, child.offset);
            seen_iprp = 1;
            status = probe_iprp(input, &child, state, probe, error);
        } else {
            status = AV2H_OK;
        }
        if (status != AV2H_OK) return status;
        offset = child.end;
    }
    if (!seen_hdlr || !seen_pitm || !seen_iloc || !seen_iinf || !seen_iprp) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, box->offset);
    }
    probe->has_primary_av02 =
        (uint8_t)(state->primary_item_id == state->information_item_id &&
                  state->primary_item_id == state->location_item_id &&
                  state->primary_item_id == state->association_item_id);
    return AV2H_OK;
}

Av2hStatus av2h_container_probe(Av2hSpan input,
                                Av2hContainerProbe *probe,
                                Av2hError *error) {
    ContainerState state = { 0 };
    ContainerBox mdat = { 0U, 0U, 0U, 0U, 0U };
    size_t offset = 0U;
    size_t extent_end;
    int seen_ftyp = 0;
    int seen_meta = 0;
    int seen_mdat = 0;
    if (probe == 0 || (input.data == 0 && input.size != 0U)) {
        return av2h_container_fail(error, AV2H_INVALID_ARGUMENT, 0U);
    }
    av2h_memory_fill(probe, 0U, sizeof(*probe));
    while (offset < input.size) {
        ContainerBox box;
        Av2hStatus status = read_box(input, offset, input.size, &box, error);
        if (status != AV2H_OK) return status;
        if (box.type == BOX_FTYP) {
            if (seen_ftyp || box.offset != 0U) {
                return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
            }
            seen_ftyp = 1;
            status = probe_ftyp(input, &box, probe, error);
            if (status != AV2H_OK) return status;
        } else if (box.type == BOX_META) {
            if (seen_meta) return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
            seen_meta = 1;
            status = probe_meta(input, &box, &state, probe, error);
            if (status != AV2H_OK) return status;
        } else if (box.type == BOX_MDAT) {
            if (seen_mdat) return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
            seen_mdat = 1;
            mdat = box;
        }
        offset = box.end;
    }
    if (!seen_ftyp || !seen_meta || !seen_mdat || !probe->has_primary_av02 ||
        probe->av2c_associated != 1U || probe->av2c_payload.data == 0) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, 0U);
    }
    if (!av2h_size_add(state.extent_offset, state.extent_length, &extent_end) ||
        state.extent_offset < mdat.payload_offset || extent_end > mdat.end ||
        state.extent_length == 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, state.extent_offset);
    }
    probe->item_payload.data = input.data + state.extent_offset;
    probe->item_payload.size = state.extent_length;
    return AV2H_OK;
}

Av2hStatus av2h_heif_parse(Av2hSpan input,
                           Av2hHeifImage *image,
                           Av2hError *error) {
    ContainerBox ftyp;
    ContainerBox meta = { 0U, 0U, 0U, 0U, 0U };
    ContainerBox mdat = { 0U, 0U, 0U, 0U, 0U };
    ContainerState state = { 0 };
    Av2hSequenceInfo sequence;
    Av2hColorInfo content_color;
    uint8_t content_color_present;
    size_t offset = 0U;
    size_t extent_end;
    int seen_ftyp = 0;
    int seen_meta = 0;
    int seen_mdat = 0;
    Av2hStatus status;
    if (image == 0 || (input.data == 0 && input.size != 0U)) {
        return av2h_container_fail(error, AV2H_INVALID_ARGUMENT, 0U);
    }
    while (offset < input.size) {
        ContainerBox box;
        status = read_box(input, offset, input.size, &box, error);
        if (status != AV2H_OK) return status;
        if (!seen_ftyp && box.type != BOX_FTYP) {
            return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
        }
        if (box.type == BOX_FTYP) {
            if (seen_ftyp || box.offset != 0U) {
                return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
            }
            seen_ftyp = 1;
            ftyp = box;
        } else if (box.type == BOX_META) {
            if (seen_meta) return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
            seen_meta = 1;
            meta = box;
        } else if (box.type == BOX_MDAT) {
            if (seen_mdat) return av2h_container_fail(error, AV2H_INVALID_DATA, box.offset);
            seen_mdat = 1;
            mdat = box;
        } else {
            return av2h_container_fail(error, AV2H_UNSUPPORTED, box.offset);
        }
        offset = box.end;
    }
    if (!seen_ftyp || !seen_meta || !seen_mdat) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, 0U);
    }
    status = parse_ftyp(input, &ftyp, error);
    if (status != AV2H_OK) return status;
    status = parse_meta(input, &meta, &state, error);
    if (status != AV2H_OK) return status;
    if (state.primary_item_id != 1U ||
        state.location_item_id != state.primary_item_id ||
        state.information_item_id != state.primary_item_id ||
        state.association_item_id != state.primary_item_id) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, meta.offset);
    }
    if (state.property_counts[PROPERTY_AV2C] != 1U ||
        state.property_counts[PROPERTY_ISPE] != 1U ||
        state.property_counts[PROPERTY_PIXI] != 1U ||
        state.property_counts[PROPERTY_COLR] != 1U ||
        state.association_counts[PROPERTY_AV2C] != 1U ||
        state.association_counts[PROPERTY_ISPE] != 1U ||
        state.association_counts[PROPERTY_PIXI] != 1U ||
        state.association_counts[PROPERTY_COLR] != 1U ||
        state.association_essential[PROPERTY_AV2C] != 1U ||
        state.association_essential[PROPERTY_ISPE] != 0U ||
        state.association_essential[PROPERTY_PIXI] != 0U ||
        state.association_essential[PROPERTY_COLR] != 0U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, meta.offset);
    }
    if (!av2h_size_add(state.extent_offset, state.extent_length, &extent_end) ||
        extent_end > input.size ||
        state.extent_offset != mdat.payload_offset ||
        state.extent_length != mdat.payload_size) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, state.extent_offset);
    }
    image->source.config_obus = state.configuration.config_obus;
    image->source.item_payload.data = input.data + state.extent_offset;
    image->source.item_payload.size = state.extent_length;
    image->source.color = state.color;
    status = av2h_carriage_validate_metadata(
        image->source.config_obus, image->source.item_payload,
        &sequence, &content_color, &content_color_present, error);
    if (status != AV2H_OK) return status;
    if (content_color_present != 0U) {
        if (!av2h_container_color_description_equal(&image->source.color, &content_color)) {
            return av2h_container_fail(error, AV2H_INVALID_DATA, meta.offset);
        }
        image->source.color.chroma_sample_position_x =
            content_color.chroma_sample_position_x;
        image->source.color.chroma_sample_position_y =
            content_color.chroma_sample_position_y;
    }
    if (state.width != (uint32_t)sequence.max_frame_width_minus_1 + 1U ||
        state.height != (uint32_t)sequence.max_frame_height_minus_1 + 1U) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, meta.offset);
    }
    image->sequence = sequence;
    image->width = state.width;
    image->height = state.height;
    image->channel_count = state.channel_count;
    image->bits_per_channel = state.bits_per_channel;
    status = validate_pixel_metadata(&sequence, state.channel_count,
                                     state.bits_per_channel,
                                     &image->pixel_format);
    if (status != AV2H_OK) return av2h_container_fail(error, status, meta.offset);
    image->property_count = state.property_count;
    return AV2H_OK;
}
