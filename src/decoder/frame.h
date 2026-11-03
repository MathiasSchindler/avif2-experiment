#ifndef AV2H_DECODER_FRAME_H
#define AV2H_DECODER_FRAME_H

#include "decoder/plane.h"
#include "decoder/quantizer.h"
#include "decoder/sequence.h"

#include <stddef.h>

#define AV2H_CDEF_MAX_STRENGTHS 8U
#define AV2H_CCSO_BAND_COUNT 64U
#define AV2H_CCSO_LUT_SIZE (AV2H_CCSO_BAND_COUNT * 16U)
#define AV2H_WIENER_NONSEP_MAX_TAPS 18U
#define AV2H_WIENER_NONSEP_MAX_CLASSES 16U
#define AV2H_PICTURE_QM_COUNT 4U
#define AV2H_CUSTOM_QM_COUNT 15U
#define AV2H_CUSTOM_QM_FUNDAMENTAL_SIZE 128U
#define AV2H_MAX_TILE_COUNT (AV2H_MAX_TILE_COLUMNS * AV2H_MAX_TILE_ROWS)

typedef enum {
    AV2H_RESTORE_NONE = 0,
    AV2H_RESTORE_PC_WIENER = 1,
    AV2H_RESTORE_WIENER_NONSEP = 2,
    AV2H_RESTORE_SWITCHABLE = 3
} Av2hRestorationType;

typedef struct {
    uint8_t enabled;
    uint8_t band_only;
    uint8_t scale_index;
    uint8_t quant_index;
    uint8_t extended_filter_support;
    uint8_t edge_classifier;
    uint8_t max_band_log2;
    uint16_t offset_count;
    int8_t *filter_offset;
} Av2hCcsoPlane;

typedef struct {
    uint8_t custom_qm[AV2H_CUSTOM_QM_COUNT][AV2H_FRAME_PLANE_COUNT]
                     [AV2H_CUSTOM_QM_FUNDAMENTAL_SIZE];
    int16_t restoration_frame_filter[AV2H_FRAME_PLANE_COUNT][AV2H_WIENER_NONSEP_MAX_CLASSES]
                                    [AV2H_WIENER_NONSEP_MAX_TAPS];
    int8_t ccso_filter_offset[AV2H_FRAME_PLANE_COUNT][AV2H_CCSO_LUT_SIZE];
} Av2hFrameSyntaxStorage;

typedef struct {
    size_t restoration_bit_positions[AV2H_FRAME_PLANE_COUNT];
    size_t ccso_bit_positions[AV2H_FRAME_PLANE_COUNT];
} Av2hFrameSyntaxReferences;

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
    uint8_t tile_column_log2;
    uint8_t tile_row_log2;
    uint8_t tile_column_count;
    uint8_t tile_row_count;
    uint8_t tile_size_bytes;
    uint16_t context_update_tile_id;
    uint16_t tile_column_start_sb[AV2H_MAX_TILE_COLUMNS + 1U];
    uint16_t tile_row_start_sb[AV2H_MAX_TILE_ROWS + 1U];
    uint8_t segmentation_enabled;
    uint8_t seg_feature_mask[AV2H_SEGMENT_COUNT];
    int16_t seg_alt_q[AV2H_SEGMENT_COUNT];
    uint8_t seg_last_active_id;
    uint8_t using_qmatrix;
    uint8_t picture_qm_count;
    uint8_t qm_index_bits;
    uint8_t qm_y[AV2H_PICTURE_QM_COUNT];
    uint8_t qm_u[AV2H_PICTURE_QM_COUNT];
    uint8_t qm_v[AV2H_PICTURE_QM_COUNT];
    uint8_t segment_qm_index[AV2H_SEGMENT_COUNT];
    uint16_t custom_qm_definition_bitmap;
    uint16_t custom_qm_bitmap;
    uint8_t custom_qm_plane_count[AV2H_CUSTOM_QM_COUNT];
    uint8_t (*custom_qm)[AV2H_FRAME_PLANE_COUNT][AV2H_CUSTOM_QM_FUNDAMENTAL_SIZE];
    uint8_t delta_q_present;
    uint8_t delta_q_res;
    uint8_t allow_parity_hiding;
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
    uint8_t restoration_type[AV2H_FRAME_PLANE_COUNT];
    uint8_t restoration_frame_filters_on[AV2H_FRAME_PLANE_COUNT];
    uint8_t restoration_num_filter_classes[AV2H_FRAME_PLANE_COUNT];
    int16_t (
        *restoration_frame_filter)[AV2H_WIENER_NONSEP_MAX_CLASSES][AV2H_WIENER_NONSEP_MAX_TAPS];
    uint16_t restoration_unit_size[AV2H_FRAME_PLANE_COUNT];
    uint16_t restoration_unit_columns[AV2H_FRAME_PLANE_COUNT];
    uint16_t restoration_unit_rows[AV2H_FRAME_PLANE_COUNT];
    uint32_t restoration_unit_count[AV2H_FRAME_PLANE_COUNT];
    uint16_t ccso_unit_size;
    Av2hCcsoPlane ccso[AV2H_FRAME_PLANE_COUNT];
    uint8_t tx_mode;
    uint8_t reduced_tx_set;
} Av2hFrameState;

static inline void av2h_frame_syntax_storage_bind(Av2hFrameState *frame,
                                                  Av2hFrameSyntaxStorage *storage) {
    unsigned int plane;
    frame->custom_qm = storage != NULL ? storage->custom_qm : NULL;
    frame->restoration_frame_filter = storage != NULL ? storage->restoration_frame_filter : NULL;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane)
        frame->ccso[plane].filter_offset =
            storage != NULL ? storage->ccso_filter_offset[plane] : NULL;
}

static inline int av2h_frame_syntax_storage_valid(const Av2hFrameState *frame) {
    unsigned int plane;
    if (frame->using_qmatrix != 0U && frame->custom_qm_bitmap != 0U && frame->custom_qm == NULL)
        return 0;
    for (plane = 0U; plane < (frame->plane_count == 1U ? 1U : AV2H_FRAME_PLANE_COUNT); ++plane) {
        if ((frame->ccso[plane].enabled != 0U && frame->ccso[plane].filter_offset == NULL) ||
            (frame->restoration_frame_filters_on[plane] != 0U &&
             frame->restoration_frame_filter == NULL))
            return 0;
    }
    return 1;
}

static inline unsigned int av2h_frame_plane_count(const Av2hFrameState *frame) {
    return frame != NULL && frame->plane_count == 1U ? 1U : AV2H_FRAME_PLANE_COUNT;
}

static inline size_t av2h_frame_plane_dimension(const Av2hFrameState *frame, unsigned int plane,
                                                size_t luma_dimension) {
    return av2h_plane_dimension(luma_dimension, frame != NULL ? frame->chroma_format_idc : 0U,
                                plane);
}

static inline size_t av2h_frame_mi_dimension(uint16_t pixels) {
    return (((size_t)pixels + 7U) / 8U) * 2U;
}

int av2h_frame_tile_layout_read(Av2hBitReader *reader, const Av2hSequenceState *sequence,
                                Av2hFrameState *frame);

static inline int av2h_frame_restoration_needs_tx_skip(const Av2hFrameState *frame) {
    if (frame == NULL)
        return 0;
    if (frame->restoration_type[0] == AV2H_RESTORE_PC_WIENER ||
        frame->restoration_type[0] == AV2H_RESTORE_SWITCHABLE)
        return 1;
    return frame->restoration_type[0] == AV2H_RESTORE_WIENER_NONSEP &&
           frame->restoration_num_filter_classes[0] > 1U;
}

Av2hStatus av2h_frame_header_closed_key_parse(Av2hSpan payload, const Av2hSequenceState *sequence,
                                              Av2hFrameState *frame,
                                              Av2hFrameSyntaxStorage *storage,
                                              Av2hFrameSyntaxReferences *references,
                                              Av2hError *error);
Av2hStatus av2h_frame_syntax_materialize(Av2hSpan payload, const Av2hSequenceState *sequence,
                                         Av2hFrameState *frame,
                                         const Av2hFrameSyntaxReferences *references,
                                         Av2hFrameSyntaxStorage *storage, Av2hError *error);

#endif
