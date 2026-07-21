#ifndef AV2H_BASE_H
#define AV2H_BASE_H

#include "av2heif.h"

int av2h_size_add(size_t left, size_t right, size_t *result);
int av2h_size_multiply(size_t left, size_t right, size_t *result);
int av2h_size_align(size_t value, size_t alignment, size_t *result);

void av2h_memory_copy(void *destination, const void *source, size_t count);
void av2h_memory_fill(void *destination, unsigned char value, size_t count);
int av2h_memory_compare(const void *left, const void *right, size_t count);

uint16_t av2h_load_u16be(const unsigned char *bytes);
uint32_t av2h_load_u32be(const unsigned char *bytes);
uint64_t av2h_load_u64be(const unsigned char *bytes);

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t position;
    size_t base_offset;
    Av2hStatus status;
} Av2hByteReader;

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

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t bit_position;
    size_t base_offset;
    Av2hStatus status;
} Av2hBitReader;

void av2h_bit_reader_init(Av2hBitReader *reader,
                          const void *data,
                          size_t size,
                          size_t base_offset);
size_t av2h_bit_reader_offset(const Av2hBitReader *reader);
size_t av2h_bit_reader_remaining(const Av2hBitReader *reader);
uint32_t av2h_bit_reader_read(Av2hBitReader *reader, unsigned int bit_count);
Av2hStatus av2h_bit_reader_skip(Av2hBitReader *reader, size_t bit_count);
Av2hStatus av2h_bit_reader_align(Av2hBitReader *reader);

typedef struct {
    unsigned char *data;
    size_t size;
    size_t position;
    Av2hStatus status;
    int sizing_only;
} Av2hByteWriter;

void av2h_byte_writer_init(Av2hByteWriter *writer, void *data, size_t size);
void av2h_byte_writer_init_sizing(Av2hByteWriter *writer);
Av2hStatus av2h_byte_writer_write(Av2hByteWriter *writer,
                                  const void *data,
                                  size_t size);
Av2hStatus av2h_byte_writer_u8(Av2hByteWriter *writer, uint8_t value);
Av2hStatus av2h_byte_writer_u16be(Av2hByteWriter *writer, uint16_t value);
Av2hStatus av2h_byte_writer_u32be(Av2hByteWriter *writer, uint32_t value);
size_t av2h_byte_writer_required(const Av2hByteWriter *writer);

typedef struct {
    unsigned char *data;
    size_t size;
    size_t bit_position;
    Av2hStatus status;
    int sizing_only;
} Av2hBitWriter;

void av2h_bit_writer_init(Av2hBitWriter *writer, void *data, size_t size);
void av2h_bit_writer_init_sizing(Av2hBitWriter *writer);
Av2hStatus av2h_bit_writer_write(Av2hBitWriter *writer,
                                 uint32_t value,
                                 unsigned int bit_count);
Av2hStatus av2h_bit_writer_align_zero(Av2hBitWriter *writer);
size_t av2h_bit_writer_required(const Av2hBitWriter *writer);

typedef struct {
    unsigned char *data;
    size_t size;
    size_t used;
    Av2hStatus status;
    int sizing_only;
} Av2hArena;

void av2h_arena_init(Av2hArena *arena, void *data, size_t size);
void av2h_arena_init_sizing(Av2hArena *arena);
void *av2h_arena_allocate(Av2hArena *arena, size_t size, size_t alignment);
size_t av2h_arena_required(const Av2hArena *arena);

#endif