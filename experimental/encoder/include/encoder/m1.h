#ifndef EXPERIMENTAL_ENCODER_M1_H
#define EXPERIMENTAL_ENCODER_M1_H

#include "encoder/base.h"

#define ENC_M1_STREAM_SIZE 21U

EncStatus enc_m1_constant_128_8x8(void *data, size_t capacity, size_t *size);

#endif
