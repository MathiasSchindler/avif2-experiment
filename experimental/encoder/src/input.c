#include "encoder/input.h"

static void enc_planar_image_reset(EncPlanarImage *image) {
    size_t plane;
    image->format = ENC_PIXEL_FORMAT_I400_8;
    image->plane_count = 0U;
    for (plane = 0U; plane < 3U; ++plane) {
        image->planes[plane].width = 0U;
        image->planes[plane].height = 0U;
        image->planes[plane].stride = 0U;
        image->planes[plane].bytes.data = 0;
        image->planes[plane].bytes.size = 0U;
    }
}

static void enc_planar_input_reset(EncPlanarInput *input) {
    enc_planar_image_reset(&input->image);
    input->storage.data = 0;
    input->storage.size = 0U;
}

static int enc_format_plane_geometry(EncPixelFormat format, size_t width,
                                     size_t height, size_t plane,
                                     size_t *plane_width,
                                     size_t *plane_height) {
    if (format == ENC_PIXEL_FORMAT_I400_8) {
        if (plane != 0U) return 0;
        *plane_width = width;
        *plane_height = height;
        return 1;
    }
    if (format == ENC_PIXEL_FORMAT_I444_8) {
        if (plane >= 3U) return 0;
        *plane_width = width;
        *plane_height = height;
        return 1;
    }
    if (format == ENC_PIXEL_FORMAT_I420_8 && plane < 3U) {
        if (plane == 0U) {
            *plane_width = width;
            *plane_height = height;
        } else {
            *plane_width = width / 2U + width % 2U;
            *plane_height = height / 2U + height % 2U;
        }
        return 1;
    }
    return 0;
}

static size_t enc_format_plane_count(EncPixelFormat format) {
    if (format == ENC_PIXEL_FORMAT_I400_8) return 1U;
    if (format == ENC_PIXEL_FORMAT_I420_8 ||
        format == ENC_PIXEL_FORMAT_I444_8) {
        return 3U;
    }
    return 0U;
}

static EncStatus enc_planar_geometry(EncPixelFormat format, size_t width,
                                     size_t height,
                                     const size_t strides[3],
                                     EncPlanarImage *image,
                                     size_t *total_size) {
    size_t plane_count;
    size_t plane;
    size_t offset = 0U;
    if (image == 0 || total_size == 0) return ENC_INVALID_ARGUMENT;
    enc_planar_image_reset(image);
    plane_count = enc_format_plane_count(format);
    if (plane_count == 0U || strides == 0 || width == 0U || height == 0U ||
        width > ENC_INPUT_MAX_DIMENSION ||
        height > ENC_INPUT_MAX_DIMENSION) {
        return ENC_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < plane_count; ++plane) {
        size_t plane_width;
        size_t plane_height;
        size_t plane_size;
        size_t next_offset;
        if (!enc_format_plane_geometry(format, width, height, plane,
                                       &plane_width, &plane_height) ||
            strides[plane] < plane_width) {
            return ENC_INVALID_ARGUMENT;
        }
        if (!enc_size_multiply(strides[plane], plane_height, &plane_size) ||
            !enc_size_add(offset, plane_size, &next_offset)) {
            return ENC_OVERFLOW;
        }
        if (plane_size == 0U || next_offset > ENC_INPUT_MAX_BYTES) {
            return ENC_INVALID_ARGUMENT;
        }
        image->planes[plane].width = plane_width;
        image->planes[plane].height = plane_height;
        image->planes[plane].stride = strides[plane];
        image->planes[plane].bytes.size = plane_size;
        offset = next_offset;
    }
    image->format = format;
    image->plane_count = plane_count;
    *total_size = offset;
    return ENC_OK;
}

EncStatus enc_planar_image_layout(EncPixelFormat format, size_t width,
                                  size_t height, const size_t strides[3],
                                  size_t storage_size, const uint8_t *storage,
                                  EncPlanarImage *image) {
    EncStatus status;
    size_t total_size;
    size_t plane;
    size_t offset = 0U;
    if (image == 0) return ENC_INVALID_ARGUMENT;
    status = enc_planar_geometry(format, width, height, strides, image,
                                 &total_size);
    if (status != ENC_OK) return status;
    if (storage_size != total_size ||
        (storage == 0 && storage_size != 0U)) {
        enc_planar_image_reset(image);
        return ENC_SIZE_MISMATCH;
    }
    for (plane = 0U; plane < image->plane_count; ++plane) {
        image->planes[plane].bytes.data =
            storage == 0 ? 0 : storage + offset;
        offset += image->planes[plane].bytes.size;
    }
    return ENC_OK;
}

static int enc_spans_overlap(EncConstByteSpan left, EncConstByteSpan right,
                             int *overflow) {
    uintptr_t left_start = (uintptr_t)left.data;
    uintptr_t right_start = (uintptr_t)right.data;
    uintptr_t left_end;
    uintptr_t right_end;
    if (left.size > UINTPTR_MAX - left_start ||
        right.size > UINTPTR_MAX - right_start) {
        *overflow = 1;
        return 0;
    }
    left_end = left_start + left.size;
    right_end = right_start + right.size;
    return left_start < right_end && right_start < left_end;
}

EncStatus enc_planar_image_validate(const EncPlanarImage *image) {
    size_t plane_count;
    size_t plane;
    size_t total_size = 0U;
    if (image == 0) return ENC_INVALID_ARGUMENT;
    plane_count = enc_format_plane_count(image->format);
    if (plane_count == 0U || image->plane_count != plane_count) {
        return ENC_INVALID_ARGUMENT;
    }
    if (image->planes[0].width == 0U || image->planes[0].height == 0U ||
        image->planes[0].width > ENC_INPUT_MAX_DIMENSION ||
        image->planes[0].height > ENC_INPUT_MAX_DIMENSION) {
        return ENC_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < plane_count; ++plane) {
        const EncPlane *descriptor = &image->planes[plane];
        size_t expected_width;
        size_t expected_height;
        size_t expected_size;
        size_t other;
        if (!enc_format_plane_geometry(
                image->format, image->planes[0].width,
                image->planes[0].height, plane, &expected_width,
                &expected_height) ||
            descriptor->bytes.data == 0 ||
            descriptor->width != expected_width ||
            descriptor->height != expected_height ||
            descriptor->stride < descriptor->width) {
            return ENC_INVALID_ARGUMENT;
        }
        if (!enc_size_multiply(descriptor->stride, descriptor->height,
                               &expected_size)) {
            return ENC_OVERFLOW;
        }
        if (descriptor->bytes.size != expected_size ||
            expected_size > ENC_INPUT_MAX_BYTES) {
            return ENC_INVALID_ARGUMENT;
        }
        if (!enc_size_add(total_size, expected_size, &total_size)) {
            return ENC_OVERFLOW;
        }
        if (total_size > ENC_INPUT_MAX_BYTES) return ENC_INVALID_ARGUMENT;
        for (other = 0U; other < plane; ++other) {
            int overflow = 0;
            if (enc_spans_overlap(descriptor->bytes,
                                  image->planes[other].bytes, &overflow)) {
                return ENC_INVALID_ARGUMENT;
            }
            if (overflow) return ENC_OVERFLOW;
        }
    }
    return ENC_OK;
}

EncStatus enc_planar_input_open(const char *path, EncPixelFormat format,
                                size_t width, size_t height,
                                const size_t strides[3],
                                EncPlanarInput *input) {
    EncPlanarImage layout;
    EncStatus status;
    size_t expected_size;
    if (input == 0) return ENC_INVALID_ARGUMENT;
    enc_planar_input_reset(input);
    if (path == 0) return ENC_INVALID_ARGUMENT;
    status = enc_planar_geometry(format, width, height, strides, &layout,
                                 &expected_size);
    if (status != ENC_OK) return status;
    status = enc_file_map_readonly_exact(path, expected_size, &input->storage);
    if (status != ENC_OK) return status;
    status = enc_planar_image_layout(format, width, height, strides,
                                     input->storage.size, input->storage.data,
                                     &input->image);
    if (status != ENC_OK) {
        (void)enc_file_unmap(&input->storage);
        enc_planar_input_reset(input);
        return status;
    }
    return ENC_OK;
}

EncStatus enc_planar_input_close(EncPlanarInput *input) {
    EncStatus status;
    if (input == 0 || input->storage.data == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_file_unmap(&input->storage);
    enc_planar_input_reset(input);
    return status;
}

static int enc_match_text(const char **cursor, const char *text) {
    const char *position = *cursor;
    while (*text != '\0') {
        if (*position != *text) return 0;
        ++position;
        ++text;
    }
    *cursor = position;
    return 1;
}

static int enc_parse_canonical_size(const char **cursor, size_t *value) {
    const char *position = *cursor;
    size_t parsed = 0U;
    if (*position < '1' || *position > '9') return 0;
    do {
        size_t digit = (size_t)(*position - '0');
        if (parsed > (SIZE_MAX - digit) / 10U) return 0;
        parsed = parsed * 10U + digit;
        ++position;
    } while (*position >= '0' && *position <= '9');
    *cursor = position;
    *value = parsed;
    return 1;
}

EncStatus enc_planar_dimensions_from_prepared_name(
    const char *path, EncPixelFormat *format, size_t *width, size_t *height) {
    const char *name;
    const char *cursor;
    const char *scan;
    EncPixelFormat parsed_format;
    if (path == 0 || format == 0 || width == 0 || height == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    name = path;
    for (scan = path; *scan != '\0'; ++scan) {
        if (*scan == '/') name = scan + 1;
    }
    cursor = name;
    if (!enc_match_text(&cursor, "source-") ||
        !enc_parse_canonical_size(&cursor, width) || *cursor++ != 'x' ||
        !enc_parse_canonical_size(&cursor, height)) {
        return ENC_INVALID_ARGUMENT;
    }
    if (enc_match_text(&cursor, "-gray8.i400")) {
        parsed_format = ENC_PIXEL_FORMAT_I400_8;
    } else if (enc_match_text(&cursor, "-yuv420p8.i420")) {
        parsed_format = ENC_PIXEL_FORMAT_I420_8;
    } else if (enc_match_text(&cursor, "-yuv444p8.i444")) {
        parsed_format = ENC_PIXEL_FORMAT_I444_8;
    } else {
        return ENC_INVALID_ARGUMENT;
    }
    if (*cursor != '\0' || *width > ENC_INPUT_MAX_DIMENSION ||
        *height > ENC_INPUT_MAX_DIMENSION) {
        return ENC_INVALID_ARGUMENT;
    }
    *format = parsed_format;
    return ENC_OK;
}

static void enc_i400_input_reset(EncI400Input *input) {
    input->plane = 0;
    input->width = 0U;
    input->height = 0U;
    input->stride = 0U;
    input->storage.data = 0;
    input->storage.size = 0U;
}

EncStatus enc_i400_input_open(const char *path, size_t width, size_t height,
                              size_t stride, EncI400Input *input) {
    EncPlanarInput planar;
    size_t strides[3] = {stride, 0U, 0U};
    EncStatus status;
    if (input == 0) return ENC_INVALID_ARGUMENT;
    enc_i400_input_reset(input);
    status = enc_planar_input_open(path, ENC_PIXEL_FORMAT_I400_8, width,
                                   height, strides, &planar);
    if (status != ENC_OK) return status;
    input->plane = planar.image.planes[0].bytes.data;
    input->width = planar.image.planes[0].width;
    input->height = planar.image.planes[0].height;
    input->stride = planar.image.planes[0].stride;
    input->storage = planar.storage;
    return ENC_OK;
}

EncStatus enc_i400_input_close(EncI400Input *input) {
    EncStatus status;
    if (input == 0 || input->storage.data == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_file_unmap(&input->storage);
    enc_i400_input_reset(input);
    return status;
}
