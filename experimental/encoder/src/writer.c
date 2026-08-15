#include "encoder/writer.h"

void enc_byte_writer_init(EncByteWriter *writer, void *data, size_t capacity) {
    if (writer == 0) {
        return;
    }
    writer->data = (uint8_t *)data;
    writer->capacity = capacity;
    writer->position = 0;
    writer->status = (data == 0 && capacity != 0) ? ENC_INVALID_ARGUMENT : ENC_OK;
}

EncStatus enc_byte_writer_write(EncByteWriter *writer, const void *data,
                                size_t size) {
    size_t end;
    if (writer == 0 || (data == 0 && size != 0)) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (!enc_size_add(writer->position, size, &end) || end > writer->capacity) {
        writer->status = ENC_OUT_OF_SPACE;
        return writer->status;
    }
    writer->status = enc_memory_copy(writer->data + writer->position,
                                     writer->capacity - writer->position,
                                     data, size);
    if (writer->status == ENC_OK) {
        writer->position = end;
    }
    return writer->status;
}

EncStatus enc_byte_writer_u8(EncByteWriter *writer, uint8_t value) {
    return enc_byte_writer_write(writer, &value, 1);
}

void enc_bit_writer_init(EncBitWriter *writer, void *data, size_t capacity) {
    if (writer == 0) {
        return;
    }
    writer->data = (uint8_t *)data;
    writer->capacity = capacity;
    writer->bit_position = 0;
    writer->status = (data == 0 && capacity != 0) ? ENC_INVALID_ARGUMENT : ENC_OK;
    if (writer->status == ENC_OK) {
        enc_memory_fill(data, 0, capacity);
    }
}

EncStatus enc_bit_writer_write(EncBitWriter *writer, uint32_t value,
                               unsigned int bit_count) {
    size_t total_bits;
    unsigned int index;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (bit_count > 32) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (writer->capacity > SIZE_MAX / 8 ||
        !enc_size_add(writer->bit_position, bit_count, &total_bits) ||
        total_bits > writer->capacity * 8) {
        writer->status = ENC_OUT_OF_SPACE;
        return writer->status;
    }
    for (index = 0; index < bit_count; ++index) {
        size_t output_bit = writer->bit_position + index;
        unsigned int input_shift = bit_count - index - 1;
        uint8_t bit = (uint8_t)((value >> input_shift) & 1U);
        writer->data[output_bit / 8] |=
            (uint8_t)(bit << (7U - (unsigned int)(output_bit % 8)));
    }
    writer->bit_position = total_bits;
    return ENC_OK;
}

EncStatus enc_bit_writer_align_zero(EncBitWriter *writer) {
    unsigned int padding;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    padding = (unsigned int)((8 - (writer->bit_position % 8)) % 8);
    return enc_bit_writer_write(writer, 0, padding);
}

size_t enc_bit_writer_bytes_used(const EncBitWriter *writer) {
    if (writer == 0 || writer->bit_position > SIZE_MAX - 7) {
        return 0;
    }
    return (writer->bit_position + 7) / 8;
}
