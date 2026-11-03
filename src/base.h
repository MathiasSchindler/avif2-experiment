#ifndef AV2H_BASE_H
#define AV2H_BASE_H

#include "av2heif.h"

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t position;
    size_t base_offset;
    Av2hStatus status;
} Av2hByteReader;

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t bit_position;
    size_t base_offset;
    Av2hStatus status;
} Av2hBitReader;

typedef struct {
    unsigned char *data;
    size_t size;
    size_t position;
    Av2hStatus status;
    uint8_t sizing_only;
} Av2hByteWriter;

typedef struct {
    unsigned char *data;
    size_t size;
    size_t bit_position;
    Av2hStatus status;
    uint8_t sizing_only;
} Av2hBitWriter;

typedef struct {
    unsigned char *data;
    size_t size;
    size_t used;
    Av2hStatus status;
    uint8_t sizing_only;
} Av2hArena;

int av2h_size_add(size_t left, size_t right, size_t *result);
int av2h_size_multiply(size_t left, size_t right, size_t *result);
int av2h_size_align(size_t value, size_t alignment, size_t *result);

void av2h_memory_copy(void *destination, const void *source, size_t count);
void av2h_memory_fill(void *destination, unsigned char value, size_t count);
int av2h_memory_compare(const void *left, const void *right, size_t count);

uint16_t av2h_load_u16be(const unsigned char *bytes);
uint32_t av2h_load_u32be(const unsigned char *bytes);
uint64_t av2h_load_u64be(const unsigned char *bytes);

static inline uint16_t av2h_load_u16le(const unsigned char *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static inline uint32_t av2h_load_u32le(const unsigned char *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
        ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static inline void av2h_store_u16le(unsigned char *bytes, uint16_t value) {
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8U);
}

static inline void av2h_store_u32le(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8U);
    bytes[2] = (unsigned char)(value >> 16U);
    bytes[3] = (unsigned char)(value >> 24U);
}

static inline void av2h_store_u16be(unsigned char *bytes, uint16_t value) {
    bytes[0] = (unsigned char)(value >> 8U);
    bytes[1] = (unsigned char)value;
}

static inline void av2h_store_u32be(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)(value >> 24U);
    bytes[1] = (unsigned char)(value >> 16U);
    bytes[2] = (unsigned char)(value >> 8U);
    bytes[3] = (unsigned char)value;
}

void av2h_byte_reader_init(Av2hByteReader *reader,
                           const void *data,
                           size_t size,
                           size_t base_offset);
size_t av2h_byte_reader_offset(const Av2hByteReader *reader);
size_t av2h_byte_reader_remaining(const Av2hByteReader *reader);
const unsigned char *av2h_byte_reader_take(Av2hByteReader *reader, size_t count);
uint8_t av2h_byte_reader_u8(Av2hByteReader *reader);
uint16_t av2h_byte_reader_u16be(Av2hByteReader *reader);
uint32_t av2h_byte_reader_u32be(Av2hByteReader *reader);
Av2hStatus av2h_byte_reader_skip(Av2hByteReader *reader, size_t count);

void av2h_bit_reader_init(Av2hBitReader *reader,
                          const void *data,
                          size_t size,
                          size_t base_offset);
size_t av2h_bit_reader_offset(const Av2hBitReader *reader);
size_t av2h_bit_reader_remaining(const Av2hBitReader *reader);
uint32_t av2h_bit_reader_read(Av2hBitReader *reader, unsigned int bit_count);
Av2hStatus av2h_bit_reader_skip(Av2hBitReader *reader, size_t bit_count);
Av2hStatus av2h_bit_reader_align(Av2hBitReader *reader);
Av2hStatus av2h_bit_reader_align_zero(Av2hBitReader *reader);
uint32_t av2h_bit_reader_uvlc(Av2hBitReader *reader);
int32_t av2h_bit_reader_svlc(Av2hBitReader *reader);
int32_t av2h_bit_reader_inverse_signed(Av2hBitReader *reader,
                                       unsigned int magnitude_bits);
uint32_t av2h_bit_reader_quasi_uniform(Av2hBitReader *reader,
                                       uint32_t symbol_count);

void av2h_byte_writer_init(Av2hByteWriter *writer, void *data, size_t size);
void av2h_byte_writer_init_sizing(Av2hByteWriter *writer);
Av2hStatus av2h_byte_writer_write(Av2hByteWriter *writer,
                                  const void *data,
                                  size_t size);
Av2hStatus av2h_byte_writer_u8(Av2hByteWriter *writer, uint8_t value);
Av2hStatus av2h_byte_writer_u16be(Av2hByteWriter *writer, uint16_t value);
Av2hStatus av2h_byte_writer_u32be(Av2hByteWriter *writer, uint32_t value);
size_t av2h_byte_writer_required(const Av2hByteWriter *writer);

void av2h_bit_writer_init(Av2hBitWriter *writer, void *data, size_t size);
void av2h_bit_writer_init_sizing(Av2hBitWriter *writer);
Av2hStatus av2h_bit_writer_write(Av2hBitWriter *writer,
                                 uint32_t value,
                                 unsigned int bit_count);
Av2hStatus av2h_bit_writer_align_zero(Av2hBitWriter *writer);
size_t av2h_bit_writer_required(const Av2hBitWriter *writer);

void av2h_arena_init(Av2hArena *arena, void *data, size_t size);
void av2h_arena_init_sizing(Av2hArena *arena);
void *av2h_arena_allocate(Av2hArena *arena, size_t size, size_t alignment);
size_t av2h_arena_required(const Av2hArena *arena);

#endif