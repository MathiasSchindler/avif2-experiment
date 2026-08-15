#ifndef EXPERIMENTAL_ENCODER_INPUT_H
#define EXPERIMENTAL_ENCODER_INPUT_H

#include "encoder/platform.h"

#define ENC_INPUT_MAX_DIMENSION 16384U
#define ENC_INPUT_MAX_BYTES (256U * 1024U * 1024U)
#define ENC_I400_MAX_DIMENSION ENC_INPUT_MAX_DIMENSION
#define ENC_I400_MAX_INPUT_BYTES ENC_INPUT_MAX_BYTES

typedef enum {
    ENC_PIXEL_FORMAT_I400_8 = 0,
    ENC_PIXEL_FORMAT_I420_8,
    ENC_PIXEL_FORMAT_I444_8
} EncPixelFormat;

typedef struct {
    const uint8_t *data;
    size_t size;
} EncConstByteSpan;

typedef struct {
    size_t width;
    size_t height;
    size_t stride;
    EncConstByteSpan bytes;
} EncPlane;

typedef struct {
    EncPixelFormat format;
    size_t plane_count;
    EncPlane planes[3];
} EncPlanarImage;

typedef struct {
    EncPlanarImage image;
    EncReadMapping storage;
} EncPlanarInput;

typedef struct {
    const uint8_t *plane;
    size_t width;
    size_t height;
    size_t stride;
    EncReadMapping storage;
} EncI400Input;

EncStatus enc_planar_image_layout(EncPixelFormat format, size_t width,
                                  size_t height, const size_t strides[3],
                                  size_t storage_size, const uint8_t *storage,
                                  EncPlanarImage *image);
EncStatus enc_planar_image_validate(const EncPlanarImage *image);
EncStatus enc_planar_input_open(const char *path, EncPixelFormat format,
                                size_t width, size_t height,
                                const size_t strides[3],
                                EncPlanarInput *input);
EncStatus enc_planar_input_close(EncPlanarInput *input);

/*
 * Recognizes only the prepared-corpus names:
 * source-WxH-gray8.i400, source-WxH-yuv420p8.i420, and
 * source-WxH-yuv444p8.i444. Generic raw names deliberately do not imply size.
 */
EncStatus enc_planar_dimensions_from_prepared_name(
    const char *path, EncPixelFormat *format, size_t *width, size_t *height);

EncStatus enc_i400_input_open(const char *path, size_t width, size_t height,
                              size_t stride, EncI400Input *input);
EncStatus enc_i400_input_close(EncI400Input *input);

#endif
