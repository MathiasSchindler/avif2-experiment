#ifndef EXPERIMENTAL_ENCODER_BASE_H
#define EXPERIMENTAL_ENCODER_BASE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ENC_OK = 0,
    ENC_INVALID_ARGUMENT,
    ENC_OVERFLOW,
    ENC_OUT_OF_SPACE,
    ENC_IO_ERROR,
    ENC_SIZE_MISMATCH,
    ENC_UNSUPPORTED
} EncStatus;

int enc_size_add(size_t left, size_t right, size_t *result);
int enc_size_multiply(size_t left, size_t right, size_t *result);
EncStatus enc_memory_copy(void *destination, size_t destination_size,
                          const void *source, size_t count);
void enc_memory_fill(void *destination, uint8_t value, size_t count);

#endif
