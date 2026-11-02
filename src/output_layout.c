#include "output_layout.h"
#include "error.h"

static Av2hStatus output_layout_fail(Av2hError *error, Av2hStatus status) {
    return av2h_error_set_core(error, status, 0U, AV2H_CONTEXT_QUERY);
}

static Av2hStatus plane_layout(uint32_t width,
                               uint32_t height,
                               size_t bytes_per_sample,
                               size_t offset,
                               Av2hPlaneLayout *plane,
                               Av2hError *error) {
    size_t row_bytes;
    size_t size;
    if (!av2h_size_multiply((size_t)width, bytes_per_sample, &row_bytes) ||
        !av2h_size_multiply(row_bytes, (size_t)height, &size)) {
        return output_layout_fail(error, AV2H_OVERFLOW);
    }
    plane->width = width;
    plane->height = height;
    plane->row_bytes = row_bytes;
    plane->offset = offset;
    plane->size = size;
    return AV2H_OK;
}

Av2hStatus av2h_output_layout_calculate(uint32_t width,
                                        uint32_t height,
                                        uint8_t chroma_format_idc,
                                        size_t bytes_per_sample,
                                        Av2hPlaneLayout planes[3],
                                        size_t *output_size,
                                        Av2hError *error) {
    Av2hPlaneLayout calculated[3];
    Av2hStatus status;
    size_t calculated_size;
    uint32_t chroma_width;
    uint32_t chroma_height;
    if (width == 0U || height == 0U || chroma_format_idc > 2U ||
        bytes_per_sample == 0U || planes == 0 || output_size == 0) {
        return output_layout_fail(error, AV2H_INVALID_ARGUMENT);
    }
    av2h_memory_fill(calculated, 0U, sizeof(calculated));
    status = plane_layout(width, height, bytes_per_sample, 0U,
                          &calculated[0], error);
    if (status != AV2H_OK) return status;
    calculated_size = calculated[0].size;
    if (chroma_format_idc != 1U) {
        chroma_width = chroma_format_idc == 0U ?
            (width >> 1U) + (width & 1U) : width;
        chroma_height = chroma_format_idc == 0U ?
            (height >> 1U) + (height & 1U) : height;
        status = plane_layout(chroma_width, chroma_height, bytes_per_sample,
                              calculated_size, &calculated[1], error);
        if (status != AV2H_OK) return status;
        if (!av2h_size_add(calculated_size, calculated[1].size,
                           &calculated_size)) {
            return output_layout_fail(error, AV2H_OVERFLOW);
        }
        status = plane_layout(chroma_width, chroma_height, bytes_per_sample,
                              calculated_size, &calculated[2], error);
        if (status != AV2H_OK) return status;
        if (!av2h_size_add(calculated_size, calculated[2].size,
                           &calculated_size)) {
            return output_layout_fail(error, AV2H_OVERFLOW);
        }
    }
    av2h_memory_copy(planes, calculated, sizeof(calculated));
    *output_size = calculated_size;
    return AV2H_OK;
}