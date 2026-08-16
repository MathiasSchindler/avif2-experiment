#ifndef EXPERIMENTAL_ENCODER_AC8X8_H
#define EXPERIMENTAL_ENCODER_AC8X8_H

#include "encoder/m3.h"

#define ENC_AC8X8_MAX_SEARCHED_BLOCKS 256U
#define ENC_AC8X8_MAX_POSITIONS 16U
#define ENC_AC8X8_MAX_NONZERO 8U
#define ENC_AC8X8_LEVEL_LIMIT 63U

typedef enum {
    ENC_AC8X8_SEARCH_OFF = 0,
    ENC_AC8X8_SEARCH_BOUNDED = 1,
    ENC_AC8X8_SEARCH_DIRECTIONAL = 2,
    ENC_AC8X8_SEARCH_DIRECTIONAL_EXACT = 3,
    ENC_AC8X8_SEARCH_INTRA = 4
} EncAc8x8Search;

typedef struct {
    size_t size;
    size_t alignment;
    size_t stream_capacity;
    size_t pixel_count;
    size_t block_count;
} EncAc8x8WorkspaceLayout;

typedef struct {
    size_t stream_size;
    uint64_t squared_error;
    size_t pixel_count;
    size_t workspace_size;
    size_t searched_blocks;
    size_t ac_blocks;
    unsigned int qindex;
    int admitted;
} EncAc8x8Result;

EncStatus enc_ac8x8_query_workspace(
    size_t width, size_t height, EncAc8x8WorkspaceLayout *layout);
EncStatus enc_ac8x8_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, EncAc8x8Search search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncAc8x8Result *result);

#endif
