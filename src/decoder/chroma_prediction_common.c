#include "internal/chroma_prediction.h"

#include "internal/sample_ops.h"

#include <stddef.h>

int av2h_chroma_plane_region_reconstructed(
    const Av2hReconstructionPlane *plane,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < height; row += 4U) {
        for (column = 0U; column < width; column += 4U) {
            if (!av2h_plane_sample_reconstructed(
                    plane, (size_t)x + column, (size_t)y + row)) {
                return 0;
            }
        }
    }
    return 1;
}

uint8_t av2h_chroma_plane_coded_sample(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    return av2h_reference_load_u8(
        plane->data, plane->stride_samples,
        plane->width, plane->height, x, y);
}

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

int av2h_chroma_plane_region_reconstructed_u16(
    const Av2hReconstructionPlaneU16 *plane,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < height; row += 4U) {
        for (column = 0U; column < width; column += 4U) {
            if (!av2h_chroma_plane_sample_reconstructed_u16(
                    plane, (size_t)x + column, (size_t)y + row)) {
                return 0;
            }
        }
    }
    return 1;
}

uint16_t av2h_chroma_plane_coded_sample_u16(
    const Av2hReconstructionPlaneU16 *plane, size_t x, size_t y) {
    return av2h_reference_load_u16(
        plane->data, plane->stride_samples,
        plane->width, plane->height, x, y);
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