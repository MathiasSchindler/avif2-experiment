#include "preview.h"

static int format_is_u8(const Av2hPixelFormat *format) {
    return format != 0 && format->bit_depth == 8U &&
        format->bytes_per_sample == 1U &&
        format->sample_storage == (uint8_t)AV2H_SAMPLE_STORAGE_U8 &&
        format->byte_order == (uint8_t)AV2H_BYTE_ORDER_NOT_APPLICABLE &&
        format->plane_order == (uint8_t)AV2H_PLANE_ORDER_YUV;
}

static int format_is_10bit_le(const Av2hPixelFormat *format) {
    return format != 0 && format->bit_depth == 10U &&
        format->bytes_per_sample == 2U &&
        format->sample_storage == (uint8_t)AV2H_SAMPLE_STORAGE_U16 &&
        format->byte_order == (uint8_t)AV2H_BYTE_ORDER_LITTLE_ENDIAN &&
        format->plane_order == (uint8_t)AV2H_PLANE_ORDER_YUV;
}

const char *av2h_raw_output_format_name(const Av2hPixelFormat *format) {
    if (format_is_u8(format)) {
        if (format->chroma_format_idc == 1U) return "gray8";
        if (format->chroma_format_idc == 0U) return "yuv420p";
        if (format->chroma_format_idc == 2U) return "yuv444p";
    }
    if (format_is_10bit_le(format)) {
        if (format->chroma_format_idc == 0U) return "yuv420p10le";
        if (format->chroma_format_idc == 2U) return "yuv444p10le";
    }
    return 0;
}

const char *av2h_raw_output_extension(const Av2hPixelFormat *format) {
    if (!format_is_u8(format)) return 0;
    if (format->chroma_format_idc == 1U) return ".i400";
    if (format->chroma_format_idc == 0U) return ".i420";
    if (format->chroma_format_idc == 2U) return ".i444";
    return 0;
}

size_t av2h_raw_output_extension_count(void) {
    return 3U;
}

const char *av2h_raw_output_extension_at(size_t index) {
    if (index == 0U) return ".i400";
    if (index == 1U) return ".i420";
    if (index == 2U) return ".i444";
    return 0;
}

Av2hStatus av2h_planar_preview_init_tight_8bit(
    const uint8_t *planar,
    uint32_t width,
    uint32_t height,
    uint8_t chroma_format_idc,
    uint8_t color_range,
    uint8_t matrix_coefficients,
    Av2hPlanarPreview *preview) {
    Av2hPixelFormat format;
    size_t luma_size;
    size_t chroma_width;
    size_t chroma_height;
    size_t chroma_size;
    if (planar == 0 || preview == 0 || width == 0U || height == 0U ||
        color_range != (uint8_t)AV2H_COLOR_RANGE_LIMITED ||
        matrix_coefficients != 6U ||
        !av2h_pixel_format_init_8bit(chroma_format_idc, &format) ||
        !av2h_size_multiply(width, height, &luma_size)) {
        return AV2H_INVALID_ARGUMENT;
    }
    chroma_width = av2h_pixel_format_plane_dimension(&format, width, 1U);
    chroma_height = av2h_pixel_format_plane_dimension(&format, height, 1U);
    if (chroma_format_idc != 1U &&
        !av2h_size_multiply(chroma_width, chroma_height, &chroma_size)) {
        return AV2H_OVERFLOW;
    }
    preview->format = format;
    preview->color_range = color_range;
    preview->matrix_coefficients = matrix_coefficients;
    preview->width = width;
    preview->height = height;
    preview->plane[0U] = planar;
    preview->stride_samples[0U] = width;
    if (chroma_format_idc == 1U) {
        preview->plane[1U] = 0;
        preview->plane[2U] = 0;
        preview->stride_samples[1U] = 0U;
        preview->stride_samples[2U] = 0U;
    } else {
        preview->plane[1U] = planar + luma_size;
        preview->plane[2U] = planar + luma_size + chroma_size;
        preview->stride_samples[1U] = chroma_width;
        preview->stride_samples[2U] = chroma_width;
    }
    return AV2H_OK;
}

static uint8_t scale_sample(int value) {
    if (value <= 0) return 0U;
    value /= 256;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static Av2hStatus planar_to_color8_row(
    const Av2hPlanarPreview *preview,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes,
    int blue_first) {
    const uint8_t *luma;
    const uint8_t *chroma_blue = 0;
    const uint8_t *chroma_red = 0;
    size_t required_size;
    size_t chroma_row;
    uint32_t column;
    if (preview == 0 || destination == 0 ||
        !format_is_u8(&preview->format) || preview->width == 0U ||
        preview->height == 0U || row >= preview->height ||
        preview->plane[0U] == 0 ||
        preview->stride_samples[0U] < preview->width ||
        preview->color_range != (uint8_t)AV2H_COLOR_RANGE_LIMITED ||
        preview->matrix_coefficients != 6U ||
        !av2h_size_multiply(preview->width, 3U, &required_size) ||
        destination_size_bytes < required_size) {
        return AV2H_INVALID_ARGUMENT;
    }
    luma = (const uint8_t *)preview->plane[0U] +
        (size_t)row * preview->stride_samples[0U];
    chroma_row = preview->format.chroma_format_idc == 2U ? row : row / 2U;
    if (preview->format.chroma_format_idc != 1U) {
        size_t chroma_width = preview->format.chroma_format_idc == 2U ?
            preview->width : ((size_t)preview->width + 1U) / 2U;
        if (preview->plane[1U] == 0 || preview->plane[2U] == 0 ||
            preview->stride_samples[1U] < chroma_width ||
            preview->stride_samples[2U] < chroma_width) {
            return AV2H_INVALID_ARGUMENT;
        }
        chroma_blue = (const uint8_t *)preview->plane[1U] +
            chroma_row * preview->stride_samples[1U];
        chroma_red = (const uint8_t *)preview->plane[2U] +
            chroma_row * preview->stride_samples[2U];
    }
    for (column = 0U; column < preview->width; ++column) {
        size_t chroma_column = preview->format.chroma_format_idc == 2U ?
            column : column / 2U;
        int y = luma[column];
        int cb = preview->format.chroma_format_idc == 1U ?
            128 : chroma_blue[chroma_column];
        int cr = preview->format.chroma_format_idc == 1U ?
            128 : chroma_red[chroma_column];
        int c = y - 16;
        int d = cb - 128;
        int e = cr - 128;
        uint8_t blue;
        uint8_t red;
        if (c < 0) c = 0;
        blue = scale_sample(298 * c + 516 * d + 128);
        red = scale_sample(298 * c + 409 * e + 128);
        destination[(size_t)column * 3U] = blue_first ? blue : red;
        destination[(size_t)column * 3U + 1U] =
            scale_sample(298 * c - 100 * d - 208 * e + 128);
        destination[(size_t)column * 3U + 2U] = blue_first ? red : blue;
    }
    return AV2H_OK;
}

Av2hStatus av2h_planar_to_bgr8_row(
    const Av2hPlanarPreview *preview,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes) {
    return planar_to_color8_row(
        preview, row, destination, destination_size_bytes, 1);
}

Av2hStatus av2h_planar_to_rgb8_row(
    const Av2hPlanarPreview *preview,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes) {
    return planar_to_color8_row(
        preview, row, destination, destination_size_bytes, 0);
}