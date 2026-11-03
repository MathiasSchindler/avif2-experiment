#include "base.h"

#include "shared/bit_math.h"

#include <stddef.h>

const char *av2h_status_string(Av2hStatus status) {
    switch (status) {
    case AV2H_OK:
        return "ok";
    case AV2H_INVALID_ARGUMENT:
        return "invalid argument";
    case AV2H_TRUNCATED:
        return "truncated input";
    case AV2H_INVALID_DATA:
        return "invalid data";
    case AV2H_OVERFLOW:
        return "integer overflow";
    case AV2H_LIMIT_EXCEEDED:
        return "limit exceeded";
    case AV2H_OUT_OF_MEMORY:
        return "out of memory";
    case AV2H_UNSUPPORTED:
        return "unsupported feature";
    }
    return "unknown error";
}

const char *av2h_diagnostic_stage_string(uint8_t stage) {
    static const char *const names[] = {"none",   "sequence-header", "frame-header",   "tile",
                                        "leaf",   "coefficient",     "reconstruction", "filter",
                                        "output", "complete"};
    return stage < sizeof(names) / sizeof(names[0]) ? names[stage] : "unknown";
}

const char *av2h_diagnostic_reason_string(uint8_t reason) {
    static const char *const names[] = {
        "none",   "argument",       "truncated", "malformed",  "unsupported-tuple", "entropy",
        "syntax", "reconstruction", "filter",    "incomplete", "resource"};
    return reason < sizeof(names) / sizeof(names[0]) ? names[reason] : "unknown";
}

const char *av2h_diagnostic_syntax_string(uint8_t syntax) {
    static const char *const names[] = {"none",
                                        "sequence-id-profile",
                                        "sequence-format",
                                        "sequence-dimensions",
                                        "sequence-tools",
                                        "sequence-film-grain",
                                        "sequence-display-model",
                                        "sequence-trailing-bits",
                                        "frame-profile",
                                        "frame-first-tile-group",
                                        "frame-header-ids",
                                        "frame-tile-layout",
                                        "frame-segmentation",
                                        "frame-quantization",
                                        "frame-filters",
                                        "tile-entropy",
                                        "leaf-mode",
                                        "coefficient",
                                        "reconstruction",
                                        "filter"};
    return syntax < sizeof(names) / sizeof(names[0]) ? names[syntax] : "unknown";
}

int av2h_size_add(size_t left, size_t right, size_t *result) {
    if (result == NULL || right > SIZE_MAX - left)
        return 0;
    *result = left + right;
    return 1;
}

int av2h_size_multiply(size_t left, size_t right, size_t *result) {
    if (result == NULL || (left != 0U && right > SIZE_MAX / left))
        return 0;
    *result = left * right;
    return 1;
}

int av2h_size_align(size_t value, size_t alignment, size_t *result) {
    size_t mask;
    if (result == NULL || alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
        return 0;
    }
    mask = alignment - 1U;
    if (value > SIZE_MAX - mask)
        return 0;
    *result = (value + mask) & ~mask;
    return 1;
}

void av2h_memory_copy(void *destination, const void *source, size_t count) {
    unsigned char *output = (unsigned char *)destination;
    const unsigned char *input = (const unsigned char *)source;
    size_t index;
    for (index = 0U; index < count; ++index)
        output[index] = input[index];
}

void av2h_memory_fill(void *destination, unsigned char value, size_t count) {
    unsigned char *output = (unsigned char *)destination;
    size_t index;
    for (index = 0U; index < count; ++index)
        output[index] = value;
}

int av2h_memory_compare(const void *left, const void *right, size_t count) {
    const unsigned char *left_bytes = (const unsigned char *)left;
    const unsigned char *right_bytes = (const unsigned char *)right;
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (left_bytes[index] < right_bytes[index])
            return -1;
        if (left_bytes[index] > right_bytes[index])
            return 1;
    }
    return 0;
}

uint16_t av2h_load_u16be(const unsigned char *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

uint32_t av2h_load_u32be(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

uint64_t av2h_load_u64be(const unsigned char *bytes) {
    return ((uint64_t)av2h_load_u32be(bytes) << 32) | (uint64_t)av2h_load_u32be(bytes + 4U);
}

static void byte_reader_fail(Av2hByteReader *reader, Av2hStatus status) {
    if (reader->status == AV2H_OK)
        reader->status = status;
}

void av2h_byte_reader_init(Av2hByteReader *reader, const void *data, size_t size,
                           size_t base_offset) {
    size_t end_offset;
    if (reader == NULL)
        return;
    reader->data = (const unsigned char *)data;
    reader->size = size;
    reader->position = 0U;
    reader->base_offset = base_offset;
    reader->status = AV2H_OK;
    if ((data == NULL && size != 0U) || !av2h_size_add(base_offset, size, &end_offset)) {
        reader->status = AV2H_INVALID_ARGUMENT;
    }
}

size_t av2h_byte_reader_offset(const Av2hByteReader *reader) {
    return reader->base_offset + reader->position;
}

size_t av2h_byte_reader_remaining(const Av2hByteReader *reader) {
    if (reader->position > reader->size)
        return 0U;
    return reader->size - reader->position;
}

const unsigned char *av2h_byte_reader_take(Av2hByteReader *reader, size_t count) {
    const unsigned char *result;
    if (reader == NULL || reader->status != AV2H_OK)
        return NULL;
    if (count > av2h_byte_reader_remaining(reader)) {
        byte_reader_fail(reader, AV2H_TRUNCATED);
        return NULL;
    }
    result = reader->data == NULL ? NULL : reader->data + reader->position;
    reader->position += count;
    return result;
}

uint8_t av2h_byte_reader_u8(Av2hByteReader *reader) {
    const unsigned char *bytes = av2h_byte_reader_take(reader, 1U);
    return bytes == NULL ? 0U : bytes[0];
}

uint16_t av2h_byte_reader_u16be(Av2hByteReader *reader) {
    const unsigned char *bytes = av2h_byte_reader_take(reader, 2U);
    return bytes == NULL ? 0U : av2h_load_u16be(bytes);
}

uint32_t av2h_byte_reader_u32be(Av2hByteReader *reader) {
    const unsigned char *bytes = av2h_byte_reader_take(reader, 4U);
    return bytes == NULL ? 0U : av2h_load_u32be(bytes);
}

Av2hStatus av2h_byte_reader_skip(Av2hByteReader *reader, size_t count) {
    (void)av2h_byte_reader_take(reader, count);
    return reader == NULL ? AV2H_INVALID_ARGUMENT : reader->status;
}

static void bit_reader_fail(Av2hBitReader *reader, Av2hStatus status) {
    if (reader->status == AV2H_OK)
        reader->status = status;
}

void av2h_bit_reader_init(Av2hBitReader *reader, const void *data, size_t size,
                          size_t base_offset) {
    size_t end_offset;
    if (reader == NULL)
        return;
    reader->data = (const unsigned char *)data;
    reader->size = size;
    reader->bit_position = 0U;
    reader->base_offset = base_offset;
    reader->status = AV2H_OK;
    if ((data == NULL && size != 0U) || !av2h_size_add(base_offset, size, &end_offset)) {
        reader->status = AV2H_INVALID_ARGUMENT;
    }
}

size_t av2h_bit_reader_offset(const Av2hBitReader *reader) {
    return reader->base_offset + (reader->bit_position >> 3);
}

size_t av2h_bit_reader_remaining(const Av2hBitReader *reader) {
    size_t total_bits;
    if (reader == NULL || !av2h_size_multiply(reader->size, 8U, &total_bits) ||
        reader->bit_position > total_bits) {
        return 0U;
    }
    return total_bits - reader->bit_position;
}

uint32_t av2h_bit_reader_read(Av2hBitReader *reader, unsigned int bit_count) {
    uint32_t value;
    if (reader == NULL || reader->status != AV2H_OK)
        return 0U;
    if (bit_count > 32U) {
        bit_reader_fail(reader, AV2H_INVALID_ARGUMENT);
        return 0U;
    }
    if ((size_t)bit_count > av2h_bit_reader_remaining(reader)) {
        bit_reader_fail(reader, AV2H_TRUNCATED);
        return 0U;
    }
    if (bit_count == 0U)
        return 0U;
    value = av2h_read_bits_msb(reader->data + (reader->bit_position >> 3U),
                               (unsigned int)(reader->bit_position & 7U), bit_count);
    reader->bit_position += bit_count;
    return value;
}

Av2hStatus av2h_bit_reader_skip(Av2hBitReader *reader, size_t bit_count) {
    size_t remaining = bit_count;
    if (reader == NULL)
        return AV2H_INVALID_ARGUMENT;
    while (remaining != 0U && reader->status == AV2H_OK) {
        unsigned int chunk = remaining > 32U ? 32U : (unsigned int)remaining;
        (void)av2h_bit_reader_read(reader, chunk);
        remaining -= chunk;
    }
    return reader->status;
}

Av2hStatus av2h_bit_reader_align(Av2hBitReader *reader) {
    size_t remainder;
    if (reader == NULL)
        return AV2H_INVALID_ARGUMENT;
    remainder = reader->bit_position & 7U;
    if (remainder != 0U) {
        (void)av2h_bit_reader_skip(reader, 8U - remainder);
    }
    return reader->status;
}

Av2hStatus av2h_bit_reader_align_zero(Av2hBitReader *reader) {
    size_t remainder;
    unsigned int padding;
    if (reader == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (reader->status != AV2H_OK)
        return reader->status;
    remainder = reader->bit_position & 7U;
    if (remainder == 0U)
        return AV2H_OK;
    padding = 8U - (unsigned int)remainder;
    if (av2h_bit_reader_read(reader, padding) != 0U && reader->status == AV2H_OK) {
        bit_reader_fail(reader, AV2H_INVALID_DATA);
    }
    return reader->status;
}

uint32_t av2h_bit_reader_uvlc(Av2hBitReader *reader) {
    unsigned int leading_zeros = 0U;
    uint32_t suffix;
    if (reader == NULL || reader->status != AV2H_OK)
        return 0U;
    while (leading_zeros < 32U) {
        uint32_t bit = av2h_bit_reader_read(reader, 1U);
        if (reader->status != AV2H_OK)
            return 0U;
        if (bit != 0U)
            break;
        ++leading_zeros;
    }
    if (leading_zeros == 32U) {
        bit_reader_fail(reader, AV2H_INVALID_DATA);
        return 0U;
    }
    suffix = av2h_bit_reader_read(reader, leading_zeros);
    if (reader->status != AV2H_OK)
        return 0U;
    return (((uint32_t)1U << leading_zeros) - 1U) + suffix;
}

int32_t av2h_bit_reader_svlc(Av2hBitReader *reader) {
    uint32_t value = av2h_bit_reader_uvlc(reader);
    uint32_t magnitude;
    if (reader == NULL || reader->status != AV2H_OK)
        return 0;
    magnitude = (value >> 1U) + (value & 1U);
    return (value & 1U) != 0U ? (int32_t)magnitude : -(int32_t)magnitude;
}

int32_t av2h_bit_reader_inverse_signed(Av2hBitReader *reader, unsigned int magnitude_bits) {
    uint32_t value;
    uint32_t sign_bit;
    unsigned int total_bits;
    if (reader == NULL || magnitude_bits >= 31U) {
        if (reader != NULL)
            bit_reader_fail(reader, AV2H_INVALID_ARGUMENT);
        return 0;
    }
    total_bits = magnitude_bits + 1U;
    value = av2h_bit_reader_read(reader, total_bits);
    if (reader->status != AV2H_OK)
        return 0;
    sign_bit = (uint32_t)1U << magnitude_bits;
    if ((value & sign_bit) == 0U)
        return (int32_t)value;
    return (int32_t)(value | (~(uint32_t)0U << total_bits));
}

uint32_t av2h_bit_reader_quasi_uniform(Av2hBitReader *reader, uint32_t symbol_count) {
    unsigned int bits = 0U;
    uint32_t threshold;
    uint32_t value;
    uint32_t probe;
    if (reader == NULL || symbol_count == 0U || symbol_count > 65535U) {
        if (reader != NULL)
            bit_reader_fail(reader, AV2H_INVALID_ARGUMENT);
        return 0U;
    }
    if (symbol_count == 1U)
        return 0U;
    probe = symbol_count - 1U;
    while (probe != 0U) {
        ++bits;
        probe >>= 1U;
    }
    threshold = ((uint32_t)1U << bits) - symbol_count;
    value = av2h_bit_reader_read(reader, bits - 1U);
    if (reader->status != AV2H_OK || value < threshold)
        return value;
    return (value << 1U) - threshold + av2h_bit_reader_read(reader, 1U);
}

static void byte_writer_fail(Av2hByteWriter *writer, Av2hStatus status) {
    if (writer->status == AV2H_OK)
        writer->status = status;
}

void av2h_byte_writer_init(Av2hByteWriter *writer, void *data, size_t size) {
    if (writer == NULL)
        return;
    writer->data = (unsigned char *)data;
    writer->size = size;
    writer->position = 0U;
    writer->status = data == NULL && size != 0U ? AV2H_INVALID_ARGUMENT : AV2H_OK;
    writer->sizing_only = 0U;
}

void av2h_byte_writer_init_sizing(Av2hByteWriter *writer) {
    if (writer == NULL)
        return;
    writer->data = NULL;
    writer->size = 0U;
    writer->position = 0U;
    writer->status = AV2H_OK;
    writer->sizing_only = 1U;
}

Av2hStatus av2h_byte_writer_write(Av2hByteWriter *writer, const void *data, size_t size) {
    size_t end;
    if (writer == NULL || (data == NULL && size != 0U))
        return AV2H_INVALID_ARGUMENT;
    if (writer->status != AV2H_OK)
        return writer->status;
    if (!av2h_size_add(writer->position, size, &end)) {
        byte_writer_fail(writer, AV2H_OVERFLOW);
        return writer->status;
    }
    if (!writer->sizing_only && end > writer->size) {
        byte_writer_fail(writer, AV2H_OUT_OF_MEMORY);
        return writer->status;
    }
    if (!writer->sizing_only && size != 0U) {
        av2h_memory_copy(writer->data + writer->position, data, size);
    }
    writer->position = end;
    return writer->status;
}

Av2hStatus av2h_byte_writer_u8(Av2hByteWriter *writer, uint8_t value) {
    return av2h_byte_writer_write(writer, &value, 1U);
}

Av2hStatus av2h_byte_writer_u16be(Av2hByteWriter *writer, uint16_t value) {
    unsigned char bytes[2];
    av2h_store_u16be(bytes, value);
    return av2h_byte_writer_write(writer, bytes, sizeof(bytes));
}

Av2hStatus av2h_byte_writer_u32be(Av2hByteWriter *writer, uint32_t value) {
    unsigned char bytes[4];
    av2h_store_u32be(bytes, value);
    return av2h_byte_writer_write(writer, bytes, sizeof(bytes));
}

size_t av2h_byte_writer_required(const Av2hByteWriter *writer) {
    return writer == NULL ? 0U : writer->position;
}

static void bit_writer_fail(Av2hBitWriter *writer, Av2hStatus status) {
    if (writer->status == AV2H_OK)
        writer->status = status;
}

void av2h_bit_writer_init(Av2hBitWriter *writer, void *data, size_t size) {
    if (writer == NULL)
        return;
    writer->data = (unsigned char *)data;
    writer->size = size;
    writer->bit_position = 0U;
    writer->status = data == NULL && size != 0U ? AV2H_INVALID_ARGUMENT : AV2H_OK;
    writer->sizing_only = 0U;
}

void av2h_bit_writer_init_sizing(Av2hBitWriter *writer) {
    if (writer == NULL)
        return;
    writer->data = NULL;
    writer->size = 0U;
    writer->bit_position = 0U;
    writer->status = AV2H_OK;
    writer->sizing_only = 1U;
}

Av2hStatus av2h_bit_writer_write(Av2hBitWriter *writer, uint32_t value, unsigned int bit_count) {
    unsigned int index;
    size_t end_bits;
    size_t capacity_bits;
    if (writer == NULL || bit_count > 32U)
        return AV2H_INVALID_ARGUMENT;
    if (writer->status != AV2H_OK)
        return writer->status;
    if (!av2h_size_add(writer->bit_position, bit_count, &end_bits) ||
        !av2h_size_multiply(writer->size, 8U, &capacity_bits)) {
        bit_writer_fail(writer, AV2H_OVERFLOW);
        return writer->status;
    }
    if (!writer->sizing_only && end_bits > capacity_bits) {
        bit_writer_fail(writer, AV2H_OUT_OF_MEMORY);
        return writer->status;
    }
    for (index = 0U; index < bit_count; ++index) {
        size_t byte_index = writer->bit_position >> 3;
        unsigned int output_shift = 7U - (unsigned int)(writer->bit_position & 7U);
        unsigned int input_shift = bit_count - index - 1U;
        if (!writer->sizing_only) {
            if ((writer->bit_position & 7U) == 0U)
                writer->data[byte_index] = 0U;
            writer->data[byte_index] |=
                (unsigned char)(((value >> input_shift) & 1U) << output_shift);
        }
        ++writer->bit_position;
    }
    return writer->status;
}

Av2hStatus av2h_bit_writer_align_zero(Av2hBitWriter *writer) {
    size_t remainder;
    if (writer == NULL)
        return AV2H_INVALID_ARGUMENT;
    remainder = writer->bit_position & 7U;
    if (remainder != 0U) {
        return av2h_bit_writer_write(writer, 0U, (unsigned int)(8U - remainder));
    }
    return writer->status;
}

size_t av2h_bit_writer_required(const Av2hBitWriter *writer) {
    if (writer == NULL)
        return 0U;
    return (writer->bit_position >> 3) + ((writer->bit_position & 7U) != 0U ? 1U : 0U);
}

void av2h_arena_init(Av2hArena *arena, void *data, size_t size) {
    if (arena == NULL)
        return;
    arena->data = (unsigned char *)data;
    arena->size = size;
    arena->used = 0U;
    arena->status = data == NULL && size != 0U ? AV2H_INVALID_ARGUMENT : AV2H_OK;
    arena->sizing_only = 0U;
}

void av2h_arena_init_sizing(Av2hArena *arena) {
    if (arena == NULL)
        return;
    arena->data = NULL;
    arena->size = 0U;
    arena->used = 0U;
    arena->status = AV2H_OK;
    arena->sizing_only = 1U;
}

void *av2h_arena_allocate(Av2hArena *arena, size_t size, size_t alignment) {
    size_t mask;
    size_t base_modulo;
    size_t used_modulo;
    size_t padding;
    size_t aligned;
    size_t end;
    if (arena == NULL || arena->status != AV2H_OK)
        return NULL;
    if (alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
        arena->status = AV2H_INVALID_ARGUMENT;
        return NULL;
    }
    mask = alignment - 1U;
    if (arena->sizing_only) {
        padding = mask;
    } else {
        base_modulo = (size_t)((uintptr_t)arena->data & mask);
        used_modulo = arena->used & mask;
        padding = (alignment - ((base_modulo + used_modulo) & mask)) & mask;
    }
    if (!av2h_size_add(arena->used, padding, &aligned) || !av2h_size_add(aligned, size, &end)) {
        arena->status = AV2H_OVERFLOW;
        return NULL;
    }
    arena->used = end;
    if (arena->sizing_only)
        return NULL;
    if (end > arena->size) {
        arena->status = AV2H_OUT_OF_MEMORY;
        return NULL;
    }
    if (arena->data == NULL)
        return NULL;
    return arena->data + aligned;
}

size_t av2h_arena_required(const Av2hArena *arena) {
    return arena == NULL ? 0U : arena->used;
}