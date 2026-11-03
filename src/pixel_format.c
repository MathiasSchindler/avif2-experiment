#include "pixel_format.h"

#include <stddef.h>

int av2h_pixel_format_init_8bit(uint8_t chroma_format_idc, Av2hPixelFormat *format) {
    if (format == NULL || chroma_format_idc > 2U)
        return 0;
    format->chroma_format_idc = chroma_format_idc;
    format->bit_depth = 8U;
    format->bytes_per_sample = 1U;
    format->sample_storage = (uint8_t)AV2H_SAMPLE_STORAGE_U8;
    format->byte_order = (uint8_t)AV2H_BYTE_ORDER_NOT_APPLICABLE;
    format->plane_order = (uint8_t)AV2H_PLANE_ORDER_YUV;
    return 1;
}

int av2h_pixel_format_init_10bit_420(Av2hPixelFormat *format) {
    if (format == NULL)
        return 0;
    format->chroma_format_idc = 0U;
    format->bit_depth = 10U;
    format->bytes_per_sample = 2U;
    format->sample_storage = (uint8_t)AV2H_SAMPLE_STORAGE_U16;
    format->byte_order = (uint8_t)AV2H_BYTE_ORDER_LITTLE_ENDIAN;
    format->plane_order = (uint8_t)AV2H_PLANE_ORDER_YUV;
    return 1;
}

int av2h_pixel_format_init_10bit_444(Av2hPixelFormat *format) {
    if (!av2h_pixel_format_init_10bit_420(format))
        return 0;
    format->chroma_format_idc = 2U;
    return 1;
}

int av2h_pixel_format_is_valid(const Av2hPixelFormat *format) {
    int is_8bit;
    int is_10bit;
    if (format == NULL || format->plane_order != (uint8_t)AV2H_PLANE_ORDER_YUV) {
        return 0;
    }
    is_8bit = format->chroma_format_idc <= 2U && format->bit_depth == 8U &&
              format->bytes_per_sample == 1U &&
              format->sample_storage == (uint8_t)AV2H_SAMPLE_STORAGE_U8 &&
              format->byte_order == (uint8_t)AV2H_BYTE_ORDER_NOT_APPLICABLE;
    is_10bit = (format->chroma_format_idc == 0U || format->chroma_format_idc == 2U) &&
               format->bit_depth == 10U && format->bytes_per_sample == 2U &&
               format->sample_storage == (uint8_t)AV2H_SAMPLE_STORAGE_U16 &&
               format->byte_order == (uint8_t)AV2H_BYTE_ORDER_LITTLE_ENDIAN;
    return is_8bit || is_10bit;
}

int av2h_pixel_format_equal(const Av2hPixelFormat *left, const Av2hPixelFormat *right) {
    return left != NULL && right != NULL && left->chroma_format_idc == right->chroma_format_idc &&
           left->bit_depth == right->bit_depth &&
           left->bytes_per_sample == right->bytes_per_sample &&
           left->sample_storage == right->sample_storage && left->byte_order == right->byte_order &&
           left->plane_order == right->plane_order;
}

unsigned int av2h_pixel_format_plane_count(const Av2hPixelFormat *format) {
    if (!av2h_pixel_format_is_valid(format))
        return 0U;
    return format->chroma_format_idc == 1U ? 1U : 3U;
}

size_t av2h_pixel_format_plane_dimension(const Av2hPixelFormat *format, size_t luma_dimension,
                                         unsigned int plane) {
    unsigned int shift;
    if (!av2h_pixel_format_is_valid(format) || plane >= av2h_pixel_format_plane_count(format)) {
        return 0U;
    }
    shift = plane != 0U && format->chroma_format_idc != 2U ? 1U : 0U;
    return (luma_dimension + ((size_t)1U << shift) - 1U) >> shift;
}