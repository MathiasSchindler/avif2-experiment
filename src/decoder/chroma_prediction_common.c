#include "internal/chroma_prediction.h"

#include "internal/sample_ops.h"

#include <stddef.h>

int av2h_chroma_plane_sample_reconstructed_u16(
    const Av2hReconstructionPlaneU16 *plane, size_t x, size_t y) {
    size_t cell_x;
    size_t cell_y;
    size_t index;
    if (plane == NULL || x >= plane->width || y >= plane->height ||
        plane->reconstructed == NULL) {
        return 0;
    }
    cell_x = x / 4U;
    cell_y = y / 4U;
    index = cell_y * plane->reconstructed_stride_cells + cell_x;
    return index < plane->reconstructed_count_cells &&
        plane->reconstructed[index] != 0U;
}

int av2h_chroma_geometry_supported(uint16_t width, uint16_t height) {
    return (width == 4U && (height == 4U || height == 8U ||
             height == 16U || height == 32U || height == 64U)) ||
           (width == 8U &&
                (height == 4U || height == 8U || height == 16U ||
                 height == 32U || height == 64U)) ||
           (width == 16U &&
                (height == 4U || height == 8U || height == 16U ||
                 height == 32U || height == 64U)) ||
           (width == 32U &&
                (height == 4U || height == 8U || height == 16U ||
                 height == 32U || height == 64U)) ||
           (width == 64U &&
                (height == 4U || height == 8U || height == 16U ||
                 height == 32U || height == 64U));
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/chroma_prediction_common_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/chroma_prediction_common_kernel.inc"
#include "internal/sample_kernel_end.inc"
