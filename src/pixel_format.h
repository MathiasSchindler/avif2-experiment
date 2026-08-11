#ifndef AV2H_PIXEL_FORMAT_H
#define AV2H_PIXEL_FORMAT_H

#include "base.h"

int av2h_pixel_format_init_8bit(uint8_t chroma_format_idc,
                                Av2hPixelFormat *format);
int av2h_pixel_format_is_valid(const Av2hPixelFormat *format);
int av2h_pixel_format_equal(const Av2hPixelFormat *left,
                            const Av2hPixelFormat *right);
unsigned int av2h_pixel_format_plane_count(const Av2hPixelFormat *format);
size_t av2h_pixel_format_plane_dimension(const Av2hPixelFormat *format,
                                         size_t luma_dimension,
                                         unsigned int plane);

#endif