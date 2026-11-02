#ifndef AV2H_DECODER_INTERNAL_INTRABC_PREDICTION_H
#define AV2H_DECODER_INTERNAL_INTRABC_PREDICTION_H

#include "decoder/leaf_mode.h"
#include "decoder/plane.h"

struct Av2hDecodeDiagnostics;

#define AV2H_SUBPEL_BITS 4U
#define AV2H_INTRABC_MAX_BLOCK 128U

int32_t av2h_intrabc_plane_position(
    uint16_t block_position, int32_t displacement, uint8_t subsampling);

int32_t av2h_intrabc_integer_position(int32_t position);

Av2hStatus av2h_intrabc_convolve(
    const uint8_t *source8,
    const uint16_t *source16,
    size_t source_stride,
    int32_t source_x,
    int32_t source_y,
    uint32_t subpel_x,
    uint32_t subpel_y,
    uint16_t width,
    uint16_t height,
    uint8_t bit_depth,
    uint8_t *destination8,
    uint16_t *destination16);

int32_t av2h_intrabc_full_pel(int16_t displacement);

Av2hStatus av2h_intrabc_morph_u16(
    const uint16_t *plane_data,
    size_t stride,
    uint32_t plane_width,
    uint32_t plane_height,
    uint16_t coding_x,
    uint16_t coding_y,
    uint16_t coding_width,
    uint16_t coding_height,
    int32_t reference_x,
    int32_t reference_y,
    uint16_t width,
    uint16_t height,
    uint8_t bit_depth,
    uint16_t *predictor);

Av2hStatus av2h_intrabc_predict(
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hFirstLeafMode *mode,
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor);

#endif
