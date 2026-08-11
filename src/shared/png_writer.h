#ifndef AV2H_PNG_WRITER_H
#define AV2H_PNG_WRITER_H

#include "base.h"

typedef Av2hStatus (*Av2hPngWriteCallback)(
    void *context,
    const uint8_t *data,
    size_t size);

typedef Av2hStatus (*Av2hPngRowCallback)(
    void *context,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes);

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    Av2hPngRowCallback row_callback;
    void *row_context;
} Av2hPngRgbImage;

Av2hStatus av2h_png_write_rgb(
    const Av2hPngRgbImage *image,
    uint8_t *row_buffer,
    size_t row_buffer_size,
    Av2hPngWriteCallback write_callback,
    void *write_context);

#endif