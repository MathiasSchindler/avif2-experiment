#include "decoder/internal/frame_workspace.h"

#include "output_layout.h"

#include <stddef.h>

#define AV2H_PARALLEL_COPY_MIN_BYTES (64U * 1024U)
#define AV2H_PARALLEL_COPY_TARGET_CHUNK_BYTES (16U * 1024U)

typedef struct {
    const uint8_t *source;
    uint8_t *destination;
    size_t source_stride;
    size_t row_bytes;
} Av2hCopyRowsU8;

typedef struct {
    const uint16_t *source;
    uint8_t *destination;
    size_t source_stride;
    size_t width;
} Av2hCopyRowsU16;

Av2hStatus av2h_planar_output_size(const Av2hFrameState *frame, size_t bytes_per_sample,
                                   size_t *size_bytes) {
    Av2hPlaneLayout planes[3];
    if (frame == NULL || size_bytes == NULL || frame->width == 0U || frame->height == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    return av2h_output_layout_calculate(frame->width, frame->height, frame->chroma_format_idc,
                                        bytes_per_sample, planes, size_bytes, NULL);
}

Av2hStatus av2h_planar_output_bind_u16(const Av2hFrameState *frame, void *output,
                                       size_t output_size_bytes, Av2hPlanarOutputU16 *planes) {
    Av2hPlaneLayout layout[AV2H_FRAME_PLANE_COUNT];
    Av2hPlanarOutputU16 bound;
    size_t required_size;
    unsigned int plane;
    Av2hStatus status;
    if (frame == NULL || output == NULL || planes == NULL ||
        ((uintptr_t)output & (_Alignof(uint16_t) - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_output_layout_calculate(frame->width, frame->height, frame->chroma_format_idc, 2U,
                                          layout, &required_size, NULL);
    if (status != AV2H_OK)
        return status;
    if (output_size_bytes < required_size)
        return AV2H_OUT_OF_MEMORY;
    av2h_memory_fill(&bound, 0U, sizeof(bound));
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        bound.plane[plane] =
            (Av2hFilterPlaneU16){(uint16_t *)((uint8_t *)output + layout[plane].offset),
                                 layout[plane].row_bytes / sizeof(uint16_t),
                                 (uint16_t)layout[plane].width, (uint16_t)layout[plane].height};
    }
    *planes = bound;
    return AV2H_OK;
}

static int copy_rows_dispatch(const Av2hExecution *execution, size_t height, size_t row_bytes,
                              Av2hParallelBody body, void *argument) {
    size_t total_bytes = height * row_bytes;
    size_t min_chunk;
    if (execution == NULL || execution->parallel_for == NULL || execution->width <= 1U ||
        height <= 1U || total_bytes < AV2H_PARALLEL_COPY_MIN_BYTES) {
        return body(0U, height, 0U, argument);
    }
    min_chunk = AV2H_PARALLEL_COPY_TARGET_CHUNK_BYTES / row_bytes;
    if (min_chunk == 0U)
        min_chunk = 1U;
    return execution->parallel_for(execution->context, height, min_chunk, body, argument);
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/frame_copy_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/frame_copy_kernel.inc"
#include "internal/sample_kernel_end.inc"
