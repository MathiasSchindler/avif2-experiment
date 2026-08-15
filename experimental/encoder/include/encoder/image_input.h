#ifndef EXPERIMENTAL_ENCODER_IMAGE_INPUT_H
#define EXPERIMENTAL_ENCODER_IMAGE_INPUT_H

#include "encoder/base.h"

#define ENC_IMAGE_MAX_DIMENSION 16384U
#define ENC_IMAGE_MAX_BYTES (256U * 1024U * 1024U)

typedef enum {
    ENC_IMAGE_I400 = 1,
    ENC_IMAGE_I444 = 3
} EncImageType;

typedef struct {
    EncImageType type;
    size_t width;
    size_t height;
    size_t output_size;
    size_t scratch_size;
} EncImageLayout;

typedef struct {
    EncImageType type;
    size_t width;
    size_t height;
    uint8_t *plane[3];
    size_t stride[3];
} EncImage;

EncStatus enc_image_query(const uint8_t *data, size_t size,
                          EncImageLayout *layout);
EncStatus enc_image_decode(const uint8_t *data, size_t size,
                           void *output, size_t output_size,
                           void *scratch, size_t scratch_size,
                           EncImage *image);

#endif
