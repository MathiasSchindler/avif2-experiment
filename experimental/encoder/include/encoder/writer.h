#ifndef EXPERIMENTAL_ENCODER_WRITER_H
#define EXPERIMENTAL_ENCODER_WRITER_H

#include "encoder/base.h"

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t position;
    EncStatus status;
} EncByteWriter;

void enc_byte_writer_init(EncByteWriter *writer, void *data, size_t capacity);
EncStatus enc_byte_writer_write(EncByteWriter *writer, const void *data,
                                size_t size);
EncStatus enc_byte_writer_u8(EncByteWriter *writer, uint8_t value);

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t bit_position;
    EncStatus status;
} EncBitWriter;

void enc_bit_writer_init(EncBitWriter *writer, void *data, size_t capacity);
EncStatus enc_bit_writer_write(EncBitWriter *writer, uint32_t value,
                               unsigned int bit_count);
EncStatus enc_bit_writer_align_zero(EncBitWriter *writer);
size_t enc_bit_writer_bytes_used(const EncBitWriter *writer);

#endif
