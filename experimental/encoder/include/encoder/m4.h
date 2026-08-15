#ifndef EXPERIMENTAL_ENCODER_M4_H
#define EXPERIMENTAL_ENCODER_M4_H

#include "encoder/base.h"

#define ENC_M4_PIXELS 64U
#define ENC_M4_MAX_STREAM_SIZE 64U

typedef struct {
    size_t stream_size;
    uint64_t squared_error;
    int16_t dc_level;
    int16_t ac_level;
    size_t pixel_count;
} EncM4Result;

EncStatus enc_m4_ac_i400(
    const uint8_t *input, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM4Result *result);

#endif
