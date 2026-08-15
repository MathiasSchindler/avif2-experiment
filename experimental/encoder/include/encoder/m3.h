#ifndef EXPERIMENTAL_ENCODER_M3_H
#define EXPERIMENTAL_ENCODER_M3_H

#include "encoder/base.h"

#define ENC_M3_MIN_DIMENSION 1U
#define ENC_M3_MAX_DIMENSION 16U
#define ENC_M3_MAX_PIXELS 256U
#define ENC_M3_MAX_BLOCKS 4U
#define ENC_M3_MAX_STREAM_SIZE 256U
#define ENC_M3_DEFAULT_QINDEX 100U
#define ENC_M3_MIN_QINDEX 1U
#define ENC_M3_MAX_QINDEX 255U

typedef struct {
    size_t stream_size;
    uint64_t squared_error;
    unsigned int quant_level;
    unsigned int quant_sign;
    uint8_t reconstructed_sample;
    size_t block_count;
    size_t pixel_count;
    unsigned int qindex;
} EncM3Result;

uint8_t enc_m3_dc_prediction(
    const uint8_t *above, const uint8_t *left, size_t block_size);

EncStatus enc_m3_dc_i400(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);
EncStatus enc_m3_dc_i400_q(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);

#endif
