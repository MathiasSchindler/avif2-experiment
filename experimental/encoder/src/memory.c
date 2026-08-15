#include "encoder/base.h"

#include <stdint.h>

int enc_size_add(size_t left, size_t right, size_t *result) {
    if (result == 0 || right > SIZE_MAX - left) {
        return 0;
    }
    *result = left + right;
    return 1;
}

int enc_size_multiply(size_t left, size_t right, size_t *result) {
    if (result == 0 || (left != 0U && right > SIZE_MAX / left)) {
        return 0;
    }
    *result = left * right;
    return 1;
}

EncStatus enc_memory_copy(void *destination, size_t destination_size,
                          const void *source, size_t count) {
    uint8_t *output = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    size_t index;
    if ((destination == 0 && count != 0) || (source == 0 && count != 0)) {
        return ENC_INVALID_ARGUMENT;
    }
    if (count > destination_size) {
        return ENC_OUT_OF_SPACE;
    }
    for (index = 0; index < count; ++index) {
        output[index] = input[index];
    }
    return ENC_OK;
}

void enc_memory_fill(void *destination, uint8_t value, size_t count) {
    uint8_t *bytes = (uint8_t *)destination;
    size_t index;
    for (index = 0; index < count; ++index) {
        bytes[index] = value;
    }
}
