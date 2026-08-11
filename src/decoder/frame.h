#ifndef AV2H_DECODER_FRAME_H
#define AV2H_DECODER_FRAME_H

#include "decoder/sequence.h"

#define AV2H_CDEF_MAX_STRENGTHS 8U
#define AV2H_FRAME_PLANE_COUNT 3U
#define AV2H_CCSO_BAND_COUNT 64U
#define AV2H_CCSO_LUT_SIZE (AV2H_CCSO_BAND_COUNT * 16U)

typedef uint16_t Av2hQIndex;

typedef struct {
    uint8_t enabled;
    uint8_t band_only;
    uint8_t scale_index;
    uint8_t quant_index;
    uint8_t extended_filter_support;
    uint8_t edge_classifier;
    uint8_t max_band_log2;
    uint16_t offset_count;
    int8_t filter_offset[AV2H_CCSO_LUT_SIZE];
} Av2hCcsoPlane;

typedef struct {
    size_t uncompressed_header_bits;
    uint16_t width;
    uint16_t height;
    uint8_t plane_count;
    uint8_t chroma_format_idc;
    Av2hQIndex base_q_index;
    int16_t y_dc_delta_q;
    int16_t u_dc_delta_q;
    int16_t u_ac_delta_q;
    int16_t v_dc_delta_q;
    int16_t v_ac_delta_q;
    int16_t deblock_delta[4];
    uint8_t first_tile_group;
    uint8_t allow_screen_content_tools;
    uint8_t force_integer_mv;
    uint8_t allow_intrabc;
    uint8_t allow_global_intrabc;
    uint8_t allow_local_intrabc;
    uint8_t max_bvp_drl_bits;
    uint8_t disable_cdf_update;
    uint8_t uniform_tile_spacing;
    uint8_t segmentation_enabled;
    uint8_t using_qmatrix;
    uint8_t delta_q_present;
    uint8_t delta_q_res;
    uint8_t deblock_enabled[4];
    uint8_t gdf_mode;
    uint8_t gdf_qp_index;
    uint8_t gdf_scale_index;
    uint16_t gdf_block_size;
    uint16_t gdf_block_columns;
    uint16_t gdf_block_rows;
    uint32_t gdf_block_count;
    uint8_t cdef_damping;
    uint8_t cdef_strength_count;
    uint8_t cdef_on_skip_transform;
    uint8_t cdef_y_strength[AV2H_CDEF_MAX_STRENGTHS];
    uint8_t cdef_uv_strength[AV2H_CDEF_MAX_STRENGTHS];
    uint16_t ccso_unit_size;
    Av2hCcsoPlane ccso[AV2H_FRAME_PLANE_COUNT];
    uint8_t tx_mode;
    uint8_t reduced_tx_set;
} Av2hFrameState;

static inline unsigned int av2h_frame_plane_count(
    const Av2hFrameState *frame) {
    return frame != 0 && frame->plane_count == 1U ?
        1U : AV2H_FRAME_PLANE_COUNT;
}

static inline size_t av2h_frame_plane_dimension(
    const Av2hFrameState *frame,
    unsigned int plane,
    size_t luma_dimension) {
    return av2h_plane_dimension(
        luma_dimension, frame != 0 ? frame->chroma_format_idc : 0U, plane);
}

static inline size_t av2h_frame_mi_dimension(uint16_t pixels) {
    return (((size_t)pixels + 7U) / 8U) * 2U;
}

Av2hStatus av2h_frame_header_closed_key_parse(
    Av2hSpan payload,
    const Av2hSequenceState *sequence,
    Av2hFrameState *frame);

#endif