#ifndef EXPERIMENTAL_ENCODER_M3_H
#define EXPERIMENTAL_ENCODER_M3_H

#include "encoder/base.h"

#define ENC_M3_MIN_DIMENSION 1U
#define ENC_M3_MAX_DIMENSION 64U
#define ENC_M3_FROZEN_PRODUCTION_MAX_DIMENSION 896U
#define ENC_M3_PRODUCTION_MAX_DIMENSION 1440U
#define ENC_M3_CHROMA_MAX_DIMENSION ENC_M3_PRODUCTION_MAX_DIMENSION
#define ENC_M3_MAX_MI_COLUMNS \
    (2U * ((ENC_M3_PRODUCTION_MAX_DIMENSION + 7U) / 8U))
#define ENC_M3_LEGACY_MAX_DIMENSION 16U
#define ENC_M3_MAX_PIXELS 4096U
#define ENC_M3_MAX_BLOCKS 64U
#define ENC_M3_MAX_STREAM_SIZE 4352U
#define ENC_M3_LEGACY_MAX_STREAM_SIZE 256U
#define ENC_M3_WORKSPACE_ALIGNMENT 2U
#define ENC_M3_LEGACY_WORKSPACE_SIZE 784U
#define ENC_M3_MAX_WORKSPACE_SIZE 21376U
#define ENC_M3_DEFAULT_QINDEX 100U
#define ENC_M3_MIN_QINDEX 1U
#define ENC_M3_MAX_QINDEX 255U

typedef struct {
    size_t stream_size;
    uint64_t squared_error;
    unsigned int quant_level;
    unsigned int quant_sign;
    uint8_t reconstructed_sample;
    size_t block_count;
    size_t pixel_count;
    size_t workspace_size;
    unsigned int qindex;
    uint8_t intra_mode;
} EncM3Result;

typedef struct {
    size_t size;
    size_t alignment;
    size_t pixel_count;
    size_t padded_width;
    size_t padded_height;
    size_t padded_pixel_count;
    size_t block_count;
    size_t stream_capacity;
} EncM3WorkspaceLayout;

typedef enum {
    ENC_M3_PIXEL_I420 = 0,
    ENC_M3_PIXEL_I400 = 1,
    ENC_M3_PIXEL_I444 = 2
} EncM3PixelFormat;

typedef enum {
    ENC_M3_INTRA_SEARCH_OFF = 0,
    ENC_M3_INTRA_SEARCH_DIRECTIONAL = 1,
    ENC_M3_INTRA_SEARCH_DIRECTIONAL_EXACT = 2,
    ENC_M3_INTRA_SEARCH_INTRA = 3,
    ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL = 4,
    ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT = 5,
    ENC_M3_INTRA_SEARCH_PLANE_QDELTA = 6
} EncM3IntraSearch;

typedef struct {
    const uint8_t *data;
    size_t stride;
    size_t capacity;
} EncM3Plane8;

typedef struct {
    EncM3PixelFormat format;
    size_t width;
    size_t height;
    EncM3Plane8 planes[3U];
} EncM3Image8;

typedef struct {
    uint8_t *data;
    size_t stride;
    size_t capacity;
} EncM3MutablePlane8;

typedef struct {
    EncM3PixelFormat format;
    size_t width;
    size_t height;
    EncM3MutablePlane8 planes[3U];
} EncM3MutableImage8;

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
    size_t reconstruction_size;
} EncM3ChromaWorkspaceLayout;

typedef struct {
    size_t stream_size;
    size_t workspace_size;
    size_t plane_size[3U];
    size_t transform_count[3U];
    uint64_t squared_error[3U];
    int16_t dc_level[3U];
    uint8_t reconstructed_sample[3U];
    unsigned int qindex;
    EncM3PixelFormat format;
    int8_t u_qdelta;
    int8_t v_qdelta;
    uint8_t luma_mode;
    uint8_t chroma_mode;
} EncM3ChromaResult;

uint8_t enc_m3_dc_prediction(
    const uint8_t *above, const uint8_t *left, size_t block_size);
void enc_m3_intra_prediction(
    uint8_t mode, unsigned int plane,
    const uint8_t *above, size_t above_count,
    const uint8_t *left, size_t left_count, uint8_t corner,
    size_t width, size_t height, uint8_t *output);

EncStatus enc_m3_query_workspace(
    size_t width, size_t height, EncM3WorkspaceLayout *layout);
EncStatus enc_m3_query_chroma_workspace(
    EncM3PixelFormat format, size_t width, size_t height,
    EncM3ChromaWorkspaceLayout *layout);
EncStatus enc_m3_dc_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncM3ChromaResult *result);
EncStatus enc_m3_plane_qdelta_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex,
    int u_qdelta, int v_qdelta,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncM3ChromaResult *result);
EncStatus enc_m3_dc_yuv8_q_workspace_levels(
    const EncM3Image8 *input, unsigned int qindex,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    int16_t *u_levels, int16_t *v_levels, size_t level_capacity,
    EncM3ChromaResult *result);
EncStatus enc_m3_intra_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex, EncM3IntraSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncM3ChromaResult *result);
EncStatus enc_m3_dc_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);
EncStatus enc_m3_intra_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, EncM3IntraSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);
EncStatus enc_m3_dc_i400_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);
EncStatus enc_m3_dc_i400(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);
EncStatus enc_m3_dc_i400_q(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result);

#endif
