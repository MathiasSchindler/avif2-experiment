#ifndef AV2H_DECODER_SEQUENCE_H
#define AV2H_DECODER_SEQUENCE_H

#include "base.h"

typedef struct {
    Av2hSequenceInfo info;
    uint8_t sequence_header_id;
    uint16_t superblock_size;
    uint8_t num_bits_width;
    uint8_t num_bits_height;
    uint8_t force_screen_content_tools;
    uint8_t force_integer_mv;
    uint8_t enable_sdp;
    uint8_t enable_ext_partitions;
    uint8_t enable_uneven_4way_partitions;
    uint8_t max_pb_aspect_ratio_log2_minus_1;
    uint8_t enable_ext_seg;
    uint8_t seq_seg_info_present_flag;
    uint8_t enable_intra_dip;
    uint8_t enable_intra_edge_filter;
    uint8_t enable_mrls;
    uint8_t enable_cfl_intra;
    uint8_t cfl_downsample_filter_index;
    uint8_t enable_mhccp;
    uint8_t enable_ibp;
    uint8_t enable_refmvbank;
    uint8_t enable_drl_reorder;
    uint8_t default_max_bvp_drl_bits;
    uint8_t allow_frame_max_bvp_drl_bits;
    uint8_t enable_bawp;
    uint8_t enable_fsc;
    uint8_t enable_idtx_intra;
    uint8_t enable_ist;
    uint8_t enable_inter_ist;
    uint8_t enable_chroma_dctonly;
    uint8_t reduced_tx_partition_set;
    uint8_t enable_cctx;
    uint8_t enable_tcq;
    uint8_t enable_parity_hiding;
    uint8_t separate_uv_delta_q;
    uint8_t equal_ac_dc_q;
    uint8_t y_dc_delta_q_enabled;
    uint8_t uv_dc_delta_q_enabled;
    uint8_t uv_ac_delta_q_enabled;
    int8_t base_y_dc_delta_q;
    int8_t base_uv_dc_delta_q;
    int8_t base_uv_ac_delta_q;
    uint8_t disable_loopfilters_across_tiles;
    uint8_t enable_cdef;
    uint8_t enable_gdf;
    uint8_t gdf_unit_matches_superblock_size;
    uint8_t enable_restoration;
    uint8_t enable_ccso;
    uint8_t ccso_unit_matches_superblock_size;
    uint8_t deblock_parameter_bits_minus_2;
    uint8_t seq_tile_info_present_flag;
} Av2hSequenceState;

Av2hStatus av2h_sequence_state_parse(Av2hSpan input,
                                      const Av2hObu *obu,
                                      Av2hSequenceState *state,
                                      Av2hError *error);

#endif
