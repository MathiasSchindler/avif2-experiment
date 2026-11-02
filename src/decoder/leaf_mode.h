#ifndef AV2H_DECODER_LEAF_MODE_H
#define AV2H_DECODER_LEAF_MODE_H

#include "base.h"
#include "plane.h"
#include "quantizer.h"

#define AV2H_PALETTE_MAX_SIZE 8U
#define AV2H_MRL_INDEX_SYMBOLS 4U
#define AV2H_INTRA_DIP_MODES 6U

#define AV2H_PALETTE_MAP_CAPACITY 1536U

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
    uint8_t segment_id;
    Av2hQIndex q_index;
    uint8_t use_dpcm_y;
    uint8_t dpcm_mode_y;
    uint8_t lossless_tx_size_large;
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
    uint16_t palette_origin_row;
    uint16_t palette_origin_column;
    uint16_t block_origin_row;
    uint16_t block_origin_column;
    uint16_t block_width;
    uint16_t block_height;
    uint16_t palette_colors[AV2H_PALETTE_MAX_SIZE];
    uint8_t palette_map[AV2H_PALETTE_MAP_CAPACITY];
    uint8_t use_intra_dip;
    uint8_t intra_dip_mode;
    uint8_t enable_ibp;
    uint8_t tx_extended_edges_unavailable;
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

typedef enum {
    AV2H_CFL_NONE = 0,
    AV2H_CFL_EXPLICIT_ALPHA = 1,
    AV2H_CFL_DERIVED_ALPHA = 2,
    AV2H_CFL_MULTI_PARAM = 3
} Av2hChromaCflKind;

typedef struct {
    uint8_t is_cfl;
    uint8_t use_dpcm_uv;
    uint8_t dpcm_mode_uv;
    uint8_t uses_cfl;
    uint8_t ibp_configured;
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

uint8_t av2h_palette_map_get(
    const Av2hFirstLeafMode *mode, size_t index);

Av2hStatus av2h_palette_map_set(
    Av2hFirstLeafMode *mode, size_t index, uint8_t color);

#endif
