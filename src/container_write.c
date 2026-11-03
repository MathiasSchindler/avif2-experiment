#include "container.h"

#include "base.h"
#include "carriage.h"

#include <stddef.h>

static void write_box_header(Av2hByteWriter *writer, uint32_t size, uint32_t type) {
    (void)av2h_byte_writer_u32be(writer, size);
    (void)av2h_byte_writer_u32be(writer, type);
}

static Av2hStatus add_size(size_t *value, size_t amount) {
    size_t result;
    if (!av2h_size_add(*value, amount, &result))
        return AV2H_OVERFLOW;
    *value = result;
    return AV2H_OK;
}

Av2hStatus av2h_heif_write(const Av2hHeifSource *source, void *output, size_t output_size,
                           size_t *written, Av2hError *error) {
    Av2hSequenceInfo sequence;
    Av2hColorInfo content_color;
    uint8_t content_color_present;
    Av2hAv2C configuration;
    Av2hByteWriter writer;
    Av2hStatus status;
    size_t av2c_size;
    size_t meta_size;
    size_t payload_offset;
    size_t required = 20U;
    size_t nested_written;
    uint32_t width;
    uint32_t height;
    ContainerPixelMetadata pixel_metadata;
    if (source == NULL || written == NULL || (output == NULL && output_size != 0U) ||
        (source != NULL && source->config_obus.data == NULL && source->config_obus.size != 0U) ||
        (source != NULL && source->item_payload.data == NULL && source->item_payload.size != 0U) ||
        source->color.full_range_flag > 1U) {
        return av2h_container_fail(error, AV2H_INVALID_ARGUMENT, 0U);
    }
    if (source->config_obus.size > (size_t)UINT32_MAX - 238U ||
        source->item_payload.size > (size_t)UINT32_MAX - 8U) {
        return av2h_container_fail(error, AV2H_OVERFLOW, 0U);
    }
    status = av2h_carriage_validate_metadata(source->config_obus, source->item_payload, &sequence,
                                             &content_color, &content_color_present, error);
    if (status != AV2H_OK)
        return status;
    if (content_color_present != 0U &&
        (!av2h_container_color_description_equal(&source->color, &content_color) ||
         source->color.chroma_sample_position_x != content_color.chroma_sample_position_x ||
         source->color.chroma_sample_position_y != content_color.chroma_sample_position_y)) {
        return av2h_container_fail(error, AV2H_INVALID_DATA, 0U);
    }
    status = av2h_container_sequence_pixel_metadata(&sequence, &pixel_metadata);
    if (status != AV2H_OK)
        return av2h_container_fail(error, status, 0U);
    meta_size = 226U + pixel_metadata.channel_count;
    configuration.configuration_version = 1U;
    configuration.sequence = sequence;
    configuration.config_obus = source->config_obus;
    status = av2h_av2c_write(&configuration, NULL, 0U, &av2c_size);
    if (status != AV2H_OK)
        return status;
    if (add_size(&meta_size, av2c_size) != AV2H_OK || add_size(&required, meta_size) != AV2H_OK ||
        add_size(&required, 8U) != AV2H_OK ||
        add_size(&required, source->item_payload.size) != AV2H_OK) {
        return av2h_container_fail(error, AV2H_OVERFLOW, 0U);
    }
    payload_offset = 20U + meta_size + 8U;
    if (meta_size > UINT32_MAX || av2c_size > UINT32_MAX - 8U ||
        source->item_payload.size > UINT32_MAX - 8U || payload_offset > UINT32_MAX ||
        required > UINT32_MAX) {
        return av2h_container_fail(error, AV2H_OVERFLOW, 0U);
    }
    width = (uint32_t)sequence.max_frame_width_minus_1 + 1U;
    height = (uint32_t)sequence.max_frame_height_minus_1 + 1U;
    *written = required;
    if (output == NULL && output_size == 0U)
        return AV2H_OK;
    if (required > output_size)
        return av2h_container_fail(error, AV2H_OUT_OF_MEMORY, 0U);

    av2h_byte_writer_init(&writer, output, output_size);
    write_box_header(&writer, 20U, BOX_FTYP);
    (void)av2h_byte_writer_u32be(&writer, BRAND_MIF1);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, BRAND_MIF1);

    write_box_header(&writer, (uint32_t)meta_size, BOX_META);
    (void)av2h_byte_writer_u32be(&writer, 0U);

    write_box_header(&writer, 33U, BOX_HDLR);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, HANDLER_PICT);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u8(&writer, 0U);

    write_box_header(&writer, 14U, BOX_PITM);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u16be(&writer, 1U);

    write_box_header(&writer, 30U, BOX_ILOC);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u8(&writer, 0x44U);
    (void)av2h_byte_writer_u8(&writer, 0U);
    (void)av2h_byte_writer_u16be(&writer, 1U);
    (void)av2h_byte_writer_u16be(&writer, 1U);
    (void)av2h_byte_writer_u16be(&writer, 0U);
    (void)av2h_byte_writer_u16be(&writer, 1U);
    (void)av2h_byte_writer_u32be(&writer, (uint32_t)payload_offset);
    (void)av2h_byte_writer_u32be(&writer, (uint32_t)source->item_payload.size);

    write_box_header(&writer, 38U, BOX_IINF);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u16be(&writer, 1U);
    write_box_header(&writer, 24U, BOX_INFE);
    (void)av2h_byte_writer_u32be(&writer, 2U << 24);
    (void)av2h_byte_writer_u16be(&writer, 1U);
    (void)av2h_byte_writer_u16be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, ITEM_AV02);
    (void)av2h_byte_writer_write(&writer, "AV2", 4U);

    write_box_header(&writer, (uint32_t)(99U + pixel_metadata.channel_count + av2c_size), BOX_IPRP);
    write_box_header(&writer, (uint32_t)(68U + pixel_metadata.channel_count + av2c_size), BOX_IPCO);
    write_box_header(&writer, (uint32_t)(8U + av2c_size), BOX_AV2C);
    status = av2h_av2c_write(&configuration, writer.data + writer.position,
                             writer.size - writer.position, &nested_written);
    if (status != AV2H_OK || nested_written != av2c_size)
        return status;
    writer.position += nested_written;

    write_box_header(&writer, 20U, BOX_ISPE);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, width);
    (void)av2h_byte_writer_u32be(&writer, height);

    write_box_header(&writer, (uint32_t)(13U + pixel_metadata.channel_count), BOX_PIXI);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u8(&writer, pixel_metadata.channel_count);
    {
        uint8_t channel;
        for (channel = 0U; channel < pixel_metadata.channel_count; ++channel) {
            (void)av2h_byte_writer_u8(&writer, pixel_metadata.bits_per_channel);
        }
    }

    write_box_header(&writer, 19U, BOX_COLR);
    (void)av2h_byte_writer_u32be(&writer, COLOR_NCLX);
    (void)av2h_byte_writer_u16be(&writer, source->color.color_primaries);
    (void)av2h_byte_writer_u16be(&writer, source->color.transfer_characteristics);
    (void)av2h_byte_writer_u16be(&writer, source->color.matrix_coefficients);
    (void)av2h_byte_writer_u8(&writer, (uint8_t)(source->color.full_range_flag << 7));

    write_box_header(&writer, 23U, BOX_IPMA);
    (void)av2h_byte_writer_u32be(&writer, 0U);
    (void)av2h_byte_writer_u32be(&writer, 1U);
    (void)av2h_byte_writer_u16be(&writer, 1U);
    (void)av2h_byte_writer_u8(&writer, 4U);
    (void)av2h_byte_writer_u8(&writer, 0x81U);
    (void)av2h_byte_writer_u8(&writer, 0x02U);
    (void)av2h_byte_writer_u8(&writer, 0x03U);
    (void)av2h_byte_writer_u8(&writer, 0x04U);

    write_box_header(&writer, (uint32_t)(8U + source->item_payload.size), BOX_MDAT);
    (void)av2h_byte_writer_write(&writer, source->item_payload.data, source->item_payload.size);
    if (writer.status != AV2H_OK || writer.position != required) {
        return av2h_container_fail(
            error, writer.status == AV2H_OK ? AV2H_INVALID_DATA : writer.status, writer.position);
    }
    {
        Av2hHeifImage parsed;
        status =
            av2h_heif_parse((Av2hSpan){(const unsigned char *)output, required}, &parsed, error);
        if (status != AV2H_OK &&
            !(status == AV2H_UNSUPPORTED && pixel_metadata.bits_per_channel != 8U)) {
            return status;
        }
        if (parsed.width != width || parsed.height != height ||
            parsed.channel_count != pixel_metadata.channel_count ||
            parsed.bits_per_channel != pixel_metadata.bits_per_channel ||
            parsed.source.config_obus.size != source->config_obus.size ||
            parsed.source.item_payload.size != source->item_payload.size ||
            parsed.source.color.color_primaries != source->color.color_primaries ||
            parsed.source.color.transfer_characteristics !=
                source->color.transfer_characteristics ||
            parsed.source.color.matrix_coefficients != source->color.matrix_coefficients ||
            parsed.source.color.full_range_flag != source->color.full_range_flag ||
            av2h_memory_compare(parsed.source.config_obus.data, source->config_obus.data,
                                source->config_obus.size) != 0 ||
            av2h_memory_compare(parsed.source.item_payload.data, source->item_payload.data,
                                source->item_payload.size) != 0) {
            return av2h_container_fail(error, AV2H_INVALID_DATA, 0U);
        }
    }
    return AV2H_OK;
}
