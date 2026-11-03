#ifndef AV2H_DECODER_BLOCK_GEOMETRY_H
#define AV2H_DECODER_BLOCK_GEOMETRY_H

#include "base.h"

/*
 * Predictor and reconstructed sample buffers must hold the full output
 * geometry of a luma transform block, which reaches 64x64 (4096 samples)
 * for the large Emerald Q208 blocks.  Coefficient buffers stay bounded at
 * AV2H_COEFF_CAPACITY (1024) because AV2 clamps every coefficient
 * dimension to at most 32 (the adjusted transform size).
 */
#define AV2H_RECON_SAMPLE_CAPACITY 4096U

typedef enum {
    AV2H_BLOCK_4X4,
    AV2H_BLOCK_4X8,
    AV2H_BLOCK_8X4,
    AV2H_BLOCK_8X8,
    AV2H_BLOCK_8X16,
    AV2H_BLOCK_16X8,
    AV2H_BLOCK_16X16,
    AV2H_BLOCK_16X32,
    AV2H_BLOCK_32X16,
    AV2H_BLOCK_32X32,
    AV2H_BLOCK_32X64,
    AV2H_BLOCK_64X32,
    AV2H_BLOCK_64X64,
    AV2H_BLOCK_64X128,
    AV2H_BLOCK_128X64,
    AV2H_BLOCK_128X128,
    AV2H_BLOCK_128X256,
    AV2H_BLOCK_256X128,
    AV2H_BLOCK_256X256,
    AV2H_BLOCK_4X16,
    AV2H_BLOCK_16X4,
    AV2H_BLOCK_8X32,
    AV2H_BLOCK_32X8,
    AV2H_BLOCK_16X64,
    AV2H_BLOCK_64X16,
    AV2H_BLOCK_4X32,
    AV2H_BLOCK_32X4,
    AV2H_BLOCK_8X64,
    AV2H_BLOCK_64X8,
    AV2H_BLOCK_4X64,
    AV2H_BLOCK_64X4,
    AV2H_BLOCK_SIZE_COUNT,
    AV2H_BLOCK_INVALID = 255
} Av2hBlockSize;

typedef struct {
    uint16_t row;
    uint16_t column;
    Av2hBlockSize block_size;
} Av2hPartitionNode;

Av2hStatus av2h_block_dimensions_mi(Av2hBlockSize block_size, size_t *width, size_t *height);

#endif
