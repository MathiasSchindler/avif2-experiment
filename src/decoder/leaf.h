#ifndef AV2H_DECODER_LEAF_H
#define AV2H_DECODER_LEAF_H

#include "frame.h"
#include "partition.h"

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
const int16_t *av2h_intrabc_diagnostic_candidates(size_t *count);
const int16_t *av2h_intrabc_diagnostic_bank(size_t *count);
#endif

#define AV2H_INTRABC_CONTEXT_COUNT 3U
#define AV2H_CCSO_CONTEXT_COUNT 4U
#define AV2H_Y_MODE_CONTEXT_COUNT 3U
#define AV2H_Y_MODE_SET_SYMBOLS 4U
#define AV2H_Y_MODE_INDEX_SYMBOLS 8U
#define AV2H_Y_MODE_OFFSET_SYMBOLS 6U
#define AV2H_FSC_MODE_CONTEXT_COUNT 4U
#define AV2H_FSC_BLOCK_CONTEXT_COUNT 6U
#define AV2H_MRL_CONTEXT_COUNT 3U
#define AV2H_MRL_INDEX_SYMBOLS 4U
#define AV2H_CFL_CONTEXT_COUNT 3U
#define AV2H_MHCCP_CONTEXT_GROUP_COUNT 4U
#define AV2H_MHCCP_DIRECTION_SYMBOLS 3U
#define AV2H_UV_MODE_CONTEXT_COUNT 2U
#define AV2H_UV_MODE_SYMBOLS 8U
#define AV2H_INTRABC_SHELL_CLASS_SYMBOLS 7U
#define AV2H_INTRABC_QTR_SHELL_CLASS_SYMBOLS 8U
#define AV2H_INTRABC_SHELL_CLASS_SET1_SYMBOLS 7U
#define AV2H_INTRABC_QTR_SHELL_CLASS_SET1_SYMBOLS 8U
#define AV2H_INTRABC_LOW_CLASS_COUNT 2U
#define AV2H_INTRABC_OTHER_CLASS_OFFSET_COUNT 16U
#define AV2H_INTRABC_GREATER_CONTEXT_COUNT 2U
#define AV2H_INTRABC_INDEX_CONTEXT_COUNT 4U
#define AV2H_SKIP_TRANSFORM_CONTEXT_COUNT 6U
#define AV2H_CDEF_INDEX0_CONTEXT_COUNT 4U
#define AV2H_CDEF_REMAINDER_CDF_COUNT 6U
#define AV2H_CDEF_MAX_REMAINDER_SYMBOLS 7U
#define AV2H_CDEF_UNTRANSMITTED 255U
#define AV2H_CCSO_UNTRANSMITTED 255U
#define AV2H_CCSO_MAX_UNIT_SIZE 256U
#define AV2H_PALETTE_MAX_SIZE 8U
#define AV2H_PALETTE_SIZE_COUNT 7U
#define AV2H_PALETTE_MAP_CAPACITY 1536U
#define AV2H_PALETTE_COLOR_CONTEXT_COUNT 5U
#define AV2H_PALETTE_ROW_CONTEXT_COUNT 4U
#define AV2H_MORPH_PRED_CONTEXT_COUNT 3U
#define AV2H_DELTA_Q_SYMBOLS 8U

typedef struct {
    uint16_t intrabc[AV2H_INTRABC_CONTEXT_COUNT][AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_mode[AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_precision[AV2H_BINARY_CDF_SIZE];
    uint16_t morph_pred[AV2H_MORPH_PRED_CONTEXT_COUNT]
                       [AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_shell_set[AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_shell_class[AV2H_INTRABC_SHELL_CLASS_SYMBOLS + 4U];
    uint16_t intrabc_qtr_shell_class
        [AV2H_INTRABC_QTR_SHELL_CLASS_SYMBOLS + 4U];
    uint16_t intrabc_shell_class_set1
        [AV2H_INTRABC_SHELL_CLASS_SET1_SYMBOLS + 4U];
    uint16_t intrabc_qtr_shell_class_set1
        [AV2H_INTRABC_QTR_SHELL_CLASS_SET1_SYMBOLS + 4U];
    uint16_t intrabc_shell_offset[AV2H_INTRABC_LOW_CLASS_COUNT]
                                  [AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_shell_offset_class2[AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_shell_offset_other
        [AV2H_INTRABC_OTHER_CLASS_OFFSET_COUNT][AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_col_greater[AV2H_INTRABC_GREATER_CONTEXT_COUNT]
                                [AV2H_BINARY_CDF_SIZE];
    uint16_t intrabc_col_index[AV2H_INTRABC_INDEX_CONTEXT_COUNT]
                              [AV2H_BINARY_CDF_SIZE];
    uint16_t skip_transform[AV2H_SKIP_TRANSFORM_CONTEXT_COUNT]
                           [AV2H_BINARY_CDF_SIZE];
    uint16_t delta_q[AV2H_DELTA_Q_SYMBOLS + 4U];
    uint16_t ccso[AV2H_FRAME_PLANE_COUNT][AV2H_CCSO_CONTEXT_COUNT]
                 [AV2H_BINARY_CDF_SIZE];
    uint16_t y_mode_set[AV2H_Y_MODE_SET_SYMBOLS + 4U];
    uint16_t y_mode_index[AV2H_Y_MODE_CONTEXT_COUNT]
                         [AV2H_Y_MODE_INDEX_SYMBOLS + 4U];
    uint16_t y_mode_offset[AV2H_Y_MODE_CONTEXT_COUNT]
                          [AV2H_Y_MODE_OFFSET_SYMBOLS + 4U];
    uint16_t palette_y_mode[AV2H_BINARY_CDF_SIZE];
    uint16_t palette_y_size[7U + 4U];
    uint16_t palette_y_identity[AV2H_PALETTE_ROW_CONTEXT_COUNT][3U + 4U];
    uint16_t palette_y_color_index[AV2H_PALETTE_SIZE_COUNT]
                                  [AV2H_PALETTE_COLOR_CONTEXT_COUNT]
                                  [AV2H_PALETTE_MAX_SIZE + 4U];
    uint16_t fsc_mode[AV2H_FSC_MODE_CONTEXT_COUNT]
                     [AV2H_FSC_BLOCK_CONTEXT_COUNT]
                     [AV2H_BINARY_CDF_SIZE];
    uint16_t mrl_index[AV2H_MRL_CONTEXT_COUNT]
                      [AV2H_MRL_INDEX_SYMBOLS + 4U];
    uint16_t mrl_secondary[AV2H_MRL_CONTEXT_COUNT]
                          [AV2H_BINARY_CDF_SIZE];
    uint16_t is_cfl[AV2H_CFL_CONTEXT_COUNT][AV2H_BINARY_CDF_SIZE];
    uint16_t cfl_mhccp_switch[AV2H_BINARY_CDF_SIZE];
    uint16_t mhccp_direction[AV2H_MHCCP_CONTEXT_GROUP_COUNT]
                            [AV2H_MHCCP_DIRECTION_SYMBOLS + 4U];
    uint16_t cfl_index[AV2H_BINARY_CDF_SIZE];
    uint16_t cfl_sign[8U + 4U];
    uint16_t cfl_alpha[6U][8U + 4U];
    uint16_t uv_mode[AV2H_UV_MODE_CONTEXT_COUNT][AV2H_UV_MODE_SYMBOLS + 4U];
} Av2hLeafCdfs;

typedef enum {
    AV2H_INTRA_DC = 0,
    AV2H_INTRA_VERTICAL = 1,
    AV2H_INTRA_HORIZONTAL = 2,
    AV2H_INTRA_D45 = 3,
    AV2H_INTRA_D135 = 4,
    AV2H_INTRA_D113 = 5,
    AV2H_INTRA_D157 = 6,
    AV2H_INTRA_D203 = 7,
    AV2H_INTRA_D67 = 8,
    AV2H_INTRA_SMOOTH = 9,
    AV2H_INTRA_SMOOTH_VERTICAL = 10,
    AV2H_INTRA_SMOOTH_HORIZONTAL = 11,
    AV2H_INTRA_PAETH = 12
} Av2hFirstLeafIntraMode;

typedef struct {
    uint8_t use_intrabc;
    uint8_t intrabc_mode;
    uint8_t skip_transform;
    uint8_t intrabc_drl_index;
    uint8_t mv_precision;
    uint8_t morph_pred;
    int16_t displacement_x;
    int16_t displacement_y;
    uint8_t ccso_present_mask;
    uint8_t ccso_block_control[AV2H_FRAME_PLANE_COUNT];
    uint8_t mode_set;
    uint8_t mode_index;
    uint8_t fsc_mode;
    uint8_t fsc_neighbor_context;
    uint8_t fsc_block_context;
    uint8_t joint_mode;
    int8_t angle_delta;
    uint8_t mrl_index;
    uint8_t multi_line_mrl;
    uint8_t palette_mode;
    uint8_t palette_size;
    uint8_t palette_width;
    uint8_t palette_height;
    uint8_t palette_colors[AV2H_PALETTE_MAX_SIZE];
    uint8_t palette_map[AV2H_PALETTE_MAP_CAPACITY];
    uint8_t enable_ibp;
    uint8_t edge_ibp;
    uint8_t above_smooth;
    uint8_t left_smooth;
    uint16_t superblock_size;
    Av2hFirstLeafIntraMode mode;
    size_t before_mode_bits;
    size_t after_mode_set_bits;
    size_t after_mode_index_bits;
    size_t after_fsc_bits;
    size_t after_mrl_bits;
    size_t before_palette_bits;
    size_t after_palette_bits;
    size_t after_palette_colors_bits;
    size_t after_palette_map_bits;
    size_t before_gdf_bits;
    size_t after_gdf_bits;
    uint16_t gdf_flags_read;
    size_t before_cdef_bits;
    size_t after_cdef_bits;
    uint8_t cdef_strengths_read;
    uint8_t cdef_strength_index;
    size_t before_ccso_bits;
    size_t after_ccso_bits;
    uint8_t ccso_controls_read;
} Av2hFirstLeafMode;

uint8_t av2h_palette_map_get(
    const Av2hFirstLeafMode *mode, size_t index);
Av2hStatus av2h_palette_map_set(
    Av2hFirstLeafMode *mode, size_t index, uint8_t color);

typedef struct {
    uint8_t present[2];
    uint8_t use_intrabc[2];
    uint8_t skip_transform[2];
    uint8_t joint_mode[2];
    uint8_t fsc_mode[2];
    uint8_t mrl_index[2];
    uint8_t multi_line_mrl[2];
    uint8_t morph_pred[2];
    uint8_t line_valid;
    uint8_t line_present[2];
    uint8_t line_skip_transform[2];
    uint8_t spatial_valid;
    uint8_t spatial_present[2];
    uint8_t spatial_use_intrabc[2];
    uint8_t spatial_skip_transform[2];
    uint8_t spatial_joint_mode[2];
    uint8_t spatial_fsc_mode[2];
    uint8_t spatial_mrl_index[2];
    uint8_t spatial_multi_line_mrl[2];
    uint8_t spatial_morph_pred[2];
    uint8_t edge_smooth[2];
    uint8_t palette_size[2];
    uint8_t palette_colors[2][AV2H_PALETTE_MAX_SIZE];
} Av2hLumaModeNeighbors;

typedef struct {
    uint8_t present;
    uint8_t use_intrabc;
    uint8_t intrabc_mode;
    uint8_t intrabc_drl_index;
    uint8_t mv_precision;
    uint8_t morph_pred;
    int16_t displacement_x;
    int16_t displacement_y;
    uint16_t owner_row;
    uint16_t owner_column;
    uint8_t skip_transform;
    uint8_t joint_mode;
    uint8_t fsc_mode;
    uint8_t mrl_index;
    uint8_t multi_line_mrl;
    uint8_t palette_size;
    uint8_t palette_colors[AV2H_PALETTE_MAX_SIZE];
} Av2hLumaModeContext;

typedef struct {
    Av2hLumaModeContext *above;
    Av2hLumaModeContext *left;
    const uint8_t *spatial;
    size_t above_count;
    size_t left_count;
    size_t spatial_stride_bytes;
    size_t spatial_columns;
    size_t spatial_rows;
    int16_t intrabc_bank_x[4U];
    int16_t intrabc_bank_y[4U];
    uint16_t intrabc_bank_remaining_hits;
    uint16_t superblock_mi;
    uint8_t intrabc_bank_count;
    uint8_t intrabc_bank_start;
    uint8_t intrabc_bank_unit_hits;
    uint8_t intrabc_bank_superblock_hits;
    uint8_t has_intrabc;
} Av2hLumaModeContextState;

typedef struct {
    uint16_t cdf[AV2H_BINARY_CDF_SIZE];
    uint8_t *block_flags;
    size_t block_flag_capacity;
    size_t decoded_count;
    uint16_t block_size;
    uint16_t block_columns;
    uint16_t block_rows;
    uint16_t superblock_mi;
    uint8_t mode;
} Av2hGdfEntropyState;

typedef struct {
    uint16_t index0_cdf[AV2H_CDEF_INDEX0_CONTEXT_COUNT]
                       [AV2H_BINARY_CDF_SIZE];
    uint16_t remainder_cdf[AV2H_CDEF_REMAINDER_CDF_COUNT]
                           [AV2H_CDEF_MAX_REMAINDER_SYMBOLS + 4U];
    uint8_t *unit_strengths;
    size_t unit_strength_capacity;
    size_t unit_count;
    uint16_t unit_columns;
    uint16_t unit_rows;
    uint16_t superblock_mi;
    uint8_t strength_count;
} Av2hCdefEntropyState;

typedef struct {
    uint8_t *unit_controls[AV2H_FRAME_PLANE_COUNT];
    size_t unit_control_capacity;
    size_t unit_count;
    uint16_t unit_columns;
    uint16_t unit_rows;
    uint16_t unit_size;
    uint16_t unit_size_mi;
    uint8_t enabled_mask;
} Av2hCcsoEntropyState;

typedef enum {
    AV2H_CFL_NONE = 0,
    AV2H_CFL_EXPLICIT_ALPHA = 1,
    AV2H_CFL_DERIVED_ALPHA = 2,
    AV2H_CFL_MULTI_PARAM = 3
} Av2hChromaCflKind;

typedef struct {
    uint8_t is_cfl;
    uint8_t uses_cfl;
    uint8_t edge_ibp;
    uint8_t above_smooth;
    uint8_t left_smooth;
    uint8_t cfl_index;
    uint8_t cfl_mhccp_switch;
    uint8_t mhccp_direction;
    uint8_t cfl_downsample_filter_index;
    uint8_t chroma_format_idc;
    uint16_t superblock_size;
    uint8_t cfl_derived_alpha;
    uint8_t explicit_alpha_coded;
    uint8_t cfl_alpha_signs;
    uint8_t cfl_alpha_idx;
    Av2hChromaCflKind cfl_kind;
    uint8_t uv_mode;
    uint8_t mode_index;
    Av2hFirstLeafIntraMode mode;
    int8_t angle_delta;
    size_t before_cfl_bits;
    size_t after_cfl_bits;
    size_t before_cfl_index_bits;
    size_t after_cfl_index_bits;
    size_t before_cfl_switch_bits;
    size_t after_cfl_switch_bits;
    size_t before_mhccp_direction_bits;
    size_t after_mhccp_direction_bits;
    size_t after_mode_bits;
} Av2hFirstChromaMode;

Av2hStatus av2h_leaf_cdfs_init(Av2hLeafCdfs *cdfs);
Av2hStatus av2h_luma_joint_mode_from_index(
    uint8_t mode_index,
    Av2hBlockSize block_size,
    const Av2hLumaModeNeighbors *neighbors,
    uint8_t *joint_mode);
Av2hStatus av2h_first_leaf_mode_read(Av2hRangeDecoder *decoder,
                                     Av2hLeafCdfs *cdfs,
                                     const Av2hSequenceState *sequence,
                                     const Av2hFrameState *frame,
                                     const Av2hPartitionNode *leaf,
                                     Av2hFirstLeafMode *mode);
Av2hStatus av2h_luma_mode_read(Av2hRangeDecoder *decoder,
                                 Av2hLeafCdfs *cdfs,
                                 const Av2hSequenceState *sequence,
                                 const Av2hFrameState *frame,
                                 const Av2hPartitionNode *leaf,
                                 const Av2hLumaModeNeighbors *neighbors,
                                 Av2hFirstLeafMode *mode);
Av2hStatus av2h_luma_mode_context_state_init(
    Av2hLumaModeContextState *state,
    Av2hLumaModeContext *above,
    size_t above_count,
    Av2hLumaModeContext *left,
    size_t left_count);
Av2hStatus av2h_gdf_entropy_state_init(
    Av2hGdfEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *block_flags,
    size_t block_flag_capacity);
Av2hStatus av2h_cdef_entropy_state_init(
    Av2hCdefEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *unit_strengths,
    size_t unit_strength_capacity);
Av2hStatus av2h_ccso_entropy_state_init(
    Av2hCcsoEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *const unit_controls[AV2H_FRAME_PLANE_COUNT],
    size_t unit_control_capacity);
Av2hStatus av2h_luma_mode_neighbors_from_state(
    const Av2hLumaModeContextState *state,
    const Av2hPartitionNode *leaf,
    Av2hLumaModeNeighbors *neighbors);
Av2hStatus av2h_luma_mode_context_state_update(
    Av2hLumaModeContextState *state,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode);
Av2hStatus av2h_luma_mode_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hFirstLeafMode *mode);
Av2hStatus av2h_luma_mode_read_with_state_and_gdf(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hGdfEntropyState *gdf_state,
    Av2hCdefEntropyState *cdef_state,
    Av2hCcsoEntropyState *ccso_state,
    Av2hFirstLeafMode *mode);
Av2hStatus av2h_luma_mode_read_with_state_and_delta_q(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hGdfEntropyState *gdf_state,
    Av2hCdefEntropyState *cdef_state,
    Av2hCcsoEntropyState *ccso_state,
    Av2hQIndex *current_q_index,
    Av2hFirstLeafMode *mode);
Av2hStatus av2h_luma_palette_map_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hPartitionNode *leaf,
    Av2hFirstLeafMode *mode);
Av2hStatus av2h_chroma_mode_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    uint8_t cfl_context,
    Av2hFirstChromaMode *mode);
Av2hStatus av2h_chroma_mode_read_with_delta(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    int8_t colocated_luma_delta,
    Av2hBlockSize block_size,
    uint8_t cfl_context,
    Av2hFirstChromaMode *mode);
Av2hStatus av2h_chroma_mode_read_with_delta_for_sdp(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    int8_t colocated_luma_delta,
    Av2hBlockSize block_size,
    uint8_t cfl_context,
    uint8_t cfl_allowed,
    Av2hFirstChromaMode *mode);

#endif