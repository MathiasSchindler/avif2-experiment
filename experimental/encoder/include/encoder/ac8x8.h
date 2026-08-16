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

typedef enum {
    ENC_AC8X8_COEFFICIENT_SEARCH_OFF = 0,
    ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED = 1
} EncAc8x8CoefficientSearch;

typedef struct {
    size_t size;
    size_t alignment;
    size_t stream_capacity;
    size_t pixel_count;
    size_t block_count;
} EncAc8x8WorkspaceLayout;

typedef struct {
    size_t stream_size;
    size_t baseline_stream_size;
    size_t candidate_stream_size;
    uint64_t squared_error;
    uint64_t baseline_squared_error;
    uint64_t candidate_squared_error;
    size_t pixel_count;
    size_t workspace_size;
    size_t searched_blocks;
    size_t ac_blocks;
    unsigned int qindex;
    int admitted;
} EncAc8x8Result;

typedef struct {
    size_t size;
    size_t alignment;
    size_t stream_capacity;
    size_t plane_width[3U];
    size_t plane_height[3U];
    size_t plane_size[3U];
    size_t coded_plane_width[3U];
    size_t coded_plane_height[3U];
    size_t coded_plane_size[3U];
    size_t transform_count[3U];
    size_t coded_reconstruction_offset;
    size_t baseline_reconstruction_offset;
    size_t luma_coefficients_offset;
    size_t chroma_dc_levels_offset;
    size_t chroma_coefficients_offset;
    size_t entropy_state_offset;
    size_t entropy_state_size;
    size_t stream_staging_offset;
    size_t stream_staging_count;
} EncAc8x8YuvWorkspaceLayout;

typedef struct {
    size_t stream_size;
    size_t baseline_stream_size;
    size_t candidate_stream_size;
    size_t workspace_size;
    size_t plane_size[3U];
    size_t transform_count[3U];
    uint64_t squared_error[3U];
    uint64_t baseline_squared_error[3U];
    uint64_t candidate_squared_error[3U];
    size_t searched_blocks;
    size_t ac_blocks;
    size_t candidate_trials;
    unsigned int qindex;
    EncM3PixelFormat format;
    int8_t u_qdelta;
    int8_t v_qdelta;
    uint8_t luma_mode;
    uint8_t chroma_mode;
    int admitted;
} EncAc8x8YuvResult;

EncStatus enc_ac8x8_query_workspace(
    size_t width, size_t height, EncAc8x8WorkspaceLayout *layout);
EncStatus enc_ac8x8_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, EncAc8x8Search search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncAc8x8Result *result);
EncStatus enc_ac8x8_query_yuv8_workspace(
    EncM3PixelFormat format, size_t width, size_t height,
    EncAc8x8YuvWorkspaceLayout *layout);
EncStatus enc_ac8x8_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex,
    EncAc8x8CoefficientSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncAc8x8YuvResult *result);

#endif
