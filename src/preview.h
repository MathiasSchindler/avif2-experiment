#ifndef AV2H_PREVIEW_H
#define AV2H_PREVIEW_H

#include "pixel_format.h"

typedef enum {
    AV2H_COLOR_RANGE_LIMITED = 0,
    AV2H_COLOR_RANGE_FULL = 1
} Av2hColorRange;

typedef struct {
    Av2hPixelFormat format;
    uint8_t color_range;
    uint8_t matrix_coefficients;
    uint32_t width;
    uint32_t height;
    const void *plane[3U];
    size_t stride_samples[3U];
} Av2hPlanarPreview;

const char *av2h_raw_output_format_name(const Av2hPixelFormat *format);
const char *av2h_raw_output_extension(const Av2hPixelFormat *format);
size_t av2h_raw_output_extension_count(void);
const char *av2h_raw_output_extension_at(size_t index);

Av2hStatus av2h_planar_preview_init_tight_8bit(
    const uint8_t *planar,
    uint32_t width,
    uint32_t height,
    uint8_t chroma_format_idc,
    uint8_t color_range,
    uint8_t matrix_coefficients,
    Av2hPlanarPreview *preview);

Av2hStatus av2h_planar_to_bgr8_row(
    const Av2hPlanarPreview *preview,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes);

Av2hStatus av2h_planar_to_rgb8_row(
    const Av2hPlanarPreview *preview,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes);

#endif