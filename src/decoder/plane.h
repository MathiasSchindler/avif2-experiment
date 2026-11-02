#ifndef AV2H_DECODER_PLANE_H
#define AV2H_DECODER_PLANE_H

#include <stddef.h>
#include <stdint.h>

#define AV2H_FRAME_PLANE_COUNT 3U

typedef struct {
    uint8_t *data;
    size_t stride_samples;
    uint16_t width;
    uint16_t height;
    uint16_t reference_width;
    uint16_t reference_height;
    uint8_t *reconstructed;
    size_t reconstructed_stride_cells;
    size_t reconstructed_count_cells;
} Av2hReconstructionPlane;

typedef struct {
    uint16_t *data;
    size_t stride_samples;
    uint16_t width;
    uint16_t height;
    uint16_t reference_width;
    uint16_t reference_height;
    uint8_t *reconstructed;
    size_t reconstructed_stride_cells;
    size_t reconstructed_count_cells;
} Av2hReconstructionPlaneU16;

uint8_t av2h_plane_sample_reconstructed(
    const Av2hReconstructionPlane *plane,
    size_t x,
    size_t y);

#endif
