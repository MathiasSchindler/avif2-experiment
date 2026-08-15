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
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (data == 0 && size != 0) {
        return ENC_INVALID_ARGUMENT;
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
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (bit_count > 32) {
        writer->status = ENC_INVALID_ARGUMENT;
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

static EncStatus bit_writer_fail(EncBitWriter *writer, EncStatus status) {
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status == ENC_OK) {
        writer->status = status;
    }
    return writer->status;
}

static unsigned int bit_width_u32(uint32_t value) {
    unsigned int width = 0U;
    while (value != 0U) {
        ++width;
        value >>= 1U;
    }
    return width;
}

EncStatus enc_bit_writer_uvlc(EncBitWriter *writer, uint32_t value) {
    uint32_t code;
    unsigned int width;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (value == UINT32_MAX) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    code = value + 1U;
    width = bit_width_u32(code);
    if (enc_bit_writer_write(writer, 0U, width - 1U) != ENC_OK) {
        return writer->status;
    }
    return enc_bit_writer_write(writer, code, width);
}

EncStatus enc_bit_writer_svlc(EncBitWriter *writer, int32_t value) {
    uint32_t code;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (value == INT32_MIN) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    if (value > 0) {
        code = (uint32_t)value * 2U - 1U;
    } else {
        code = (uint32_t)(-(int64_t)value) * 2U;
    }
    return enc_bit_writer_uvlc(writer, code);
}

EncStatus enc_bit_writer_signed_literal(EncBitWriter *writer, int32_t value,
                                        unsigned int magnitude_bits) {
    int64_t minimum;
    int64_t maximum;
    unsigned int total_bits;
    uint32_t encoded;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (magnitude_bits > 31U) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    total_bits = magnitude_bits + 1U;
    minimum = -(INT64_C(1) << magnitude_bits);
    maximum = (INT64_C(1) << magnitude_bits) - 1;
    if ((int64_t)value < minimum || (int64_t)value > maximum) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    encoded = (uint32_t)value;
    if (total_bits < 32U) {
        encoded &= (UINT32_C(1) << total_bits) - 1U;
    }
    return enc_bit_writer_write(writer, encoded, total_bits);
}

EncStatus enc_bit_writer_ns(EncBitWriter *writer, uint32_t value,
                            uint32_t symbol_count) {
    unsigned int width;
    uint32_t cutoff;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (symbol_count == 0U || value >= symbol_count) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    if (symbol_count == 1U) {
        return writer->status;
    }
    width = bit_width_u32(symbol_count - 1U);
    cutoff = (uint32_t)((UINT64_C(1) << width) - symbol_count);
    if (value < cutoff) {
        return enc_bit_writer_write(writer, value, width - 1U);
    }
    value += cutoff;
    if (enc_bit_writer_write(writer, value >> 1U, width - 1U) != ENC_OK) {
        return writer->status;
    }
    return enc_bit_writer_write(writer, value & 1U, 1U);
}

EncStatus enc_bit_writer_subexp(EncBitWriter *writer, uint32_t value,
                                uint32_t symbol_count,
                                unsigned int parameter) {
    unsigned int iteration = 0U;
    uint64_t consumed = 0U;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (symbol_count == 0U || value >= symbol_count || parameter > 31U) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    for (;;) {
        unsigned int bits =
            iteration != 0U ? parameter + iteration - 1U : parameter;
        uint64_t group_size;
        if (bits > 31U) {
            return bit_writer_fail(writer, ENC_OVERFLOW);
        }
        group_size = UINT64_C(1) << bits;
        if ((uint64_t)symbol_count <= consumed + 3U * group_size) {
            return enc_bit_writer_ns(
                writer, value - (uint32_t)consumed,
                symbol_count - (uint32_t)consumed);
        }
        if ((uint64_t)value < consumed + group_size) {
            if (enc_bit_writer_write(writer, 0U, 1U) != ENC_OK) {
                return writer->status;
            }
            return enc_bit_writer_write(
                writer, value - (uint32_t)consumed, bits);
        }
        if (enc_bit_writer_write(writer, 1U, 1U) != ENC_OK) {
            return writer->status;
        }
        consumed += group_size;
        ++iteration;
    }
}

static uint32_t recenter_nonnegative(uint32_t reference, uint32_t value) {
    if (value > 2U * reference) {
        return value;
    }
    if (value >= reference) {
        return 2U * (value - reference);
    }
    return 2U * (reference - value) - 1U;
}

EncStatus enc_bit_writer_unsigned_subexp_with_ref(
    EncBitWriter *writer, uint32_t value, uint32_t symbol_count,
    uint32_t reference, unsigned int parameter) {
    uint32_t recentered;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (symbol_count == 0U || value >= symbol_count ||
        reference >= symbol_count) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    if (reference <= symbol_count / 2U) {
        recentered = recenter_nonnegative(reference, value);
    } else {
        recentered = recenter_nonnegative(
            symbol_count - 1U - reference, symbol_count - 1U - value);
    }
    return enc_bit_writer_subexp(
        writer, recentered, symbol_count, parameter);
}

EncStatus enc_bit_writer_signed_subexp_with_ref(
    EncBitWriter *writer, int32_t value, int32_t low, int32_t high,
    int32_t reference, unsigned int parameter) {
    int64_t symbol_count;
    int64_t shifted_value;
    int64_t shifted_reference;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    symbol_count = (int64_t)high - low;
    shifted_value = (int64_t)value - low;
    shifted_reference = (int64_t)reference - low;
    if (symbol_count <= 0 || symbol_count > UINT32_MAX ||
        shifted_value < 0 || shifted_value >= symbol_count ||
        shifted_reference < 0 || shifted_reference >= symbol_count) {
        return bit_writer_fail(writer, ENC_INVALID_ARGUMENT);
    }
    return enc_bit_writer_unsigned_subexp_with_ref(
        writer, (uint32_t)shifted_value, (uint32_t)symbol_count,
        (uint32_t)shifted_reference, parameter);
}

EncStatus enc_bit_writer_align_zero(EncBitWriter *writer) {
    unsigned int padding;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    padding = (unsigned int)((8 - (writer->bit_position % 8)) % 8);
    return enc_bit_writer_write(writer, 0, padding);
}

EncStatus enc_bit_writer_trailing_bits(EncBitWriter *writer) {
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (enc_bit_writer_write(writer, 1U, 1U) != ENC_OK) {
        return writer->status;
    }
    return enc_bit_writer_align_zero(writer);
}

size_t enc_bit_writer_bytes_used(const EncBitWriter *writer) {
    if (writer == 0 || writer->bit_position > SIZE_MAX - 7) {
        return 0;
    }
    return (writer->bit_position + 7) / 8;
}
