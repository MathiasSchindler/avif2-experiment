#ifndef EXPERIMENTAL_ENCODER_INPUT_H
#define EXPERIMENTAL_ENCODER_INPUT_H

#include "encoder/platform.h"

#define ENC_I400_MAX_DIMENSION 16384U
#define ENC_I400_MAX_INPUT_BYTES (256U * 1024U * 1024U)

typedef struct {
    const uint8_t *plane;
    size_t width;
    size_t height;
    size_t stride;
    EncReadMapping storage;
} EncI400Input;

EncStatus enc_i400_input_open(const char *path, size_t width, size_t height,
                              size_t stride, EncI400Input *input);
EncStatus enc_i400_input_close(EncI400Input *input);

#endif
