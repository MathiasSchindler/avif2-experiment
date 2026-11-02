#include "decoder/internal/frame_workspace.h"
#include "output_layout.h"

Av2hStatus av2h_planar_output_size(
    const Av2hFrameState *frame,
    size_t *size_bytes) {
    return av2h_planar_output_size_bytes(frame, 1U, size_bytes);
}

Av2hStatus av2h_planar_output_size_bytes(
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    size_t *size_bytes) {
    Av2hPlaneLayout planes[3];
    if (frame == 0 || size_bytes == 0 || frame->width == 0U ||
        frame->height == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    return av2h_output_layout_calculate(
        frame->width, frame->height, frame->chroma_format_idc,
        bytes_per_sample, planes, size_bytes, 0);
}

Av2hStatus av2h_planar_output_bind_u16(
    const Av2hFrameState *frame,
    void *output,
    size_t output_size_bytes,
    Av2hPlanarOutputU16 *planes) {
    Av2hPlaneLayout layout[AV2H_FRAME_PLANE_COUNT];
    Av2hPlanarOutputU16 bound;
    size_t required_size;
    unsigned int plane;
    Av2hStatus status;
    if (frame == 0 || output == 0 || planes == 0 ||
        ((uintptr_t)output & (_Alignof(uint16_t) - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_output_layout_calculate(
        frame->width, frame->height, frame->chroma_format_idc, 2U,
        layout, &required_size, 0);
    if (status != AV2H_OK) return status;
    if (output_size_bytes < required_size) return AV2H_OUT_OF_MEMORY;
    av2h_memory_fill(&bound, 0U, sizeof(bound));
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        bound.plane[plane] = (Av2hFilterPlaneU16){
            (uint16_t *)((uint8_t *)output + layout[plane].offset),
            layout[plane].row_bytes / sizeof(uint16_t),
            (uint16_t)layout[plane].width,
            (uint16_t)layout[plane].height
        };
    }
    *planes = bound;
    return AV2H_OK;
}

#define AV2H_PARALLEL_COPY_MIN_BYTES (64U * 1024U)
#define AV2H_PARALLEL_COPY_TARGET_CHUNK_BYTES (16U * 1024U)

typedef struct {
    const uint8_t *source;
    uint8_t *destination;
    size_t source_stride;
    size_t row_bytes;
} Av2hCopyRowsU8;

static int copy_rows_u8(size_t begin, size_t end,
                        unsigned int worker_index, void *argument) {
    Av2hCopyRowsU8 *copy = (Av2hCopyRowsU8 *)argument;
    size_t row;
    (void)worker_index;
    for (row = begin; row < end; ++row) {
        av2h_memory_copy(
            copy->destination + row * copy->row_bytes,
            copy->source + row * copy->source_stride,
            copy->row_bytes);
    }
    return 0;
}

static int copy_rows_dispatch(const Av2hExecution *execution,
                              size_t height,
                              size_t row_bytes,
                              Av2hParallelBody body,
                              void *argument) {
    size_t total_bytes = height * row_bytes;
    size_t min_chunk;
    if (execution == 0 || execution->parallel_for == 0 ||
        execution->width <= 1U || height <= 1U ||
        total_bytes < AV2H_PARALLEL_COPY_MIN_BYTES) {
        return body(0U, height, 0U, argument);
    }
    min_chunk = AV2H_PARALLEL_COPY_TARGET_CHUNK_BYTES / row_bytes;
    if (min_chunk == 0U) min_chunk = 1U;
    return execution->parallel_for(
        execution->context, height, min_chunk, body, argument);
}

Av2hStatus av2h_copy_visible_planar_with_execution_private(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes,
    const Av2hExecution *execution) {
    size_t offset = 0U;
    unsigned int plane;
    Av2hStatus status = av2h_frame_output_validate_u8(
        frame, coded, coded_data_size_bytes, output, output_size_bytes);
    if (status != AV2H_OK) return status;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlane *source = &coded->plane[plane];
        size_t width = av2h_frame_plane_dimension(
            frame, plane, frame->width);
        size_t height = av2h_frame_plane_dimension(
            frame, plane, frame->height);
        Av2hCopyRowsU8 copy = {
            source->data, output + offset, source->stride_samples, width
        };
        if (copy_rows_dispatch(
                execution, height, width, copy_rows_u8, &copy) != 0) {
            return AV2H_INVALID_DATA;
        }
        offset += width * height;
    }
    return AV2H_OK;
}

Av2hStatus av2h_copy_visible_planar_private(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes) {
    return av2h_copy_visible_planar_with_execution_private(
        frame, coded, coded_data_size_bytes, output, output_size_bytes, 0);
}

typedef struct {
    const uint16_t *source;
    uint8_t *destination;
    size_t source_stride;
    size_t width;
} Av2hCopyRowsU16;

static int copy_rows_u16(size_t begin, size_t end,
                         unsigned int worker_index, void *argument) {
    Av2hCopyRowsU16 *copy = (Av2hCopyRowsU16 *)argument;
    size_t row;
    (void)worker_index;
    for (row = begin; row < end; ++row) {
        size_t column;
        uint8_t *destination =
            copy->destination + row * copy->width * sizeof(uint16_t);
        const uint16_t *source =
            copy->source + row * copy->source_stride;
        for (column = 0U; column < copy->width; ++column) {
            uint16_t sample = source[column];
            destination[column * 2U] = (uint8_t)sample;
            destination[column * 2U + 1U] = (uint8_t)(sample >> 8U);
        }
    }
    return 0;
}

Av2hStatus av2h_copy_visible_planar_u16_with_execution_private(
    const Av2hFrameState *frame,
    const Av2hReconstructionPlaneU16 coded[AV2H_FRAME_PLANE_COUNT],
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes,
    const Av2hExecution *execution) {
    size_t offset = 0U;
    unsigned int plane;
    Av2hStatus status = av2h_frame_output_validate_u16(
        frame, coded, coded_data_size_bytes, output, output_size_bytes);
    if (status != AV2H_OK) return status;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hReconstructionPlaneU16 *source = &coded[plane];
        size_t width = av2h_frame_plane_dimension(
            frame, plane, frame->width);
        size_t height = av2h_frame_plane_dimension(
            frame, plane, frame->height);
        size_t row_bytes = width * sizeof(uint16_t);
        Av2hCopyRowsU16 copy = {
            source->data, output + offset, source->stride_samples, width
        };
        if (copy_rows_dispatch(
                execution, height, row_bytes, copy_rows_u16, &copy) != 0) {
            return AV2H_INVALID_DATA;
        }
        offset += row_bytes * height;
    }
    return AV2H_OK;
}

Av2hStatus av2h_copy_visible_planar_u16_private(
    const Av2hFrameState *frame,
    const Av2hReconstructionPlaneU16 coded[AV2H_FRAME_PLANE_COUNT],
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes) {
    return av2h_copy_visible_planar_u16_with_execution_private(
        frame, coded, coded_data_size_bytes, output, output_size_bytes, 0);
}
