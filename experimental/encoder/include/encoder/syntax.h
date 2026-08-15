#ifndef EXPERIMENTAL_ENCODER_SYNTAX_H
#define EXPERIMENTAL_ENCODER_SYNTAX_H

#include "encoder/base.h"
#include "encoder/range.h"

#define ENC_M4_MIN_LEVEL_MAGNITUDE 1U
#define ENC_M4_MAX_LEVEL_MAGNITUDE 4U

#define ENC_DO_SPLIT_ROWS 128U
#define ENC_RECT_TYPE_ROWS 128U
#define ENC_DO_EXT_PARTITION_ROWS 128U
#define ENC_DELTA_Q_ROWS 1U
#define ENC_Y_MODE_SET_ROWS 1U
#define ENC_Y_MODE_INDEX_ROWS 3U
#define ENC_FSC_MODE_ROWS 24U
#define ENC_TX_DO_PARTITION_ROWS 36U
#define ENC_TX_PARTITION_TYPE_ROWS 56U
#define ENC_PALETTE_Y_MODE_ROWS 1U
#define ENC_TXB_SKIP_ROWS 400U
#define ENC_EOB_PT_64_ROWS 12U
#define ENC_COEFF_BASE_EOB_ROWS 80U
#define ENC_COEFF_BASE_LF_EOB_ROWS 80U
#define ENC_COEFF_BASE_LF_ROWS 1320U
#define ENC_COEFF_BR_LF_ROWS 56U
#define ENC_DC_SIGN_ROWS 48U
#define ENC_INTRA_TX_TYPE_SET1_ROWS 3U
#define ENC_INTRA_TX_TYPE_SET2_ROWS 3U
#define ENC_INTRA_TX_TYPE_LONG_ROWS 4U

typedef struct {
    uint16_t do_split[ENC_DO_SPLIT_ROWS][6U];
    uint16_t rect_type[ENC_RECT_TYPE_ROWS][6U];
    uint16_t do_ext_partition[ENC_DO_EXT_PARTITION_ROWS][6U];
    uint16_t delta_q[ENC_DELTA_Q_ROWS][12U];
    uint16_t y_mode_set[ENC_Y_MODE_SET_ROWS][8U];
    uint16_t y_mode_index[ENC_Y_MODE_INDEX_ROWS][12U];
    uint16_t fsc_mode[ENC_FSC_MODE_ROWS][6U];
    uint16_t tx_do_partition[ENC_TX_DO_PARTITION_ROWS][6U];
    uint16_t tx_partition_type[ENC_TX_PARTITION_TYPE_ROWS][11U];
    uint16_t palette_y_mode[ENC_PALETTE_Y_MODE_ROWS][6U];
    uint16_t txb_skip[ENC_TXB_SKIP_ROWS][6U];
    uint16_t eob_pt_64[ENC_EOB_PT_64_ROWS][11U];
    uint16_t coeff_base_eob[ENC_COEFF_BASE_EOB_ROWS][7U];
    uint16_t coeff_base_lf_eob[ENC_COEFF_BASE_LF_EOB_ROWS][9U];
    uint16_t coeff_base_lf[ENC_COEFF_BASE_LF_ROWS][10U];
    uint16_t coeff_br_lf[ENC_COEFF_BR_LF_ROWS][8U];
    uint16_t dc_sign[ENC_DC_SIGN_ROWS][6U];
    uint16_t intra_tx_type_set1[ENC_INTRA_TX_TYPE_SET1_ROWS][11U];
    uint16_t intra_tx_type_set2[ENC_INTRA_TX_TYPE_SET2_ROWS][6U];
    uint16_t intra_tx_type_long[ENC_INTRA_TX_TYPE_LONG_ROWS][8U];
} EncSyntaxCdfs;

typedef struct {
    uint32_t base_symbol;
    uint32_t br_symbol;
    uint32_t coded_level;
    uint32_t remainder;
} EncCoeffValueCoding;

EncStatus enc_range_writer_signed_literal(
    EncRangeWriter *writer, int32_t value, unsigned int magnitude_bits);
EncStatus enc_range_writer_ns(
    EncRangeWriter *writer, uint32_t value, uint32_t symbol_count);
EncStatus enc_range_writer_coeff_remainder(
    EncRangeWriter *writer, uint32_t value, uint32_t predicted_level);
EncStatus enc_range_writer_coeff_value(
    EncRangeWriter *writer, uint32_t magnitude, int is_eob,
    uint32_t base_levels, uint32_t base_range, uint32_t predicted_level,
    uint16_t *base_cdf, uint16_t *br_cdf, int update_cdf,
    EncCoeffValueCoding *coding);

EncStatus enc_syntax_cdfs_init(EncSyntaxCdfs *cdfs);
uint16_t *enc_syntax_do_split_cdf(
    EncSyntaxCdfs *cdfs, size_t partition_structure, size_t context);
uint16_t *enc_syntax_rect_type_cdf(
    EncSyntaxCdfs *cdfs, size_t partition_structure, size_t context);
uint16_t *enc_syntax_do_ext_partition_cdf(
    EncSyntaxCdfs *cdfs, size_t partition_structure, size_t context);
uint16_t *enc_syntax_delta_q_cdf(EncSyntaxCdfs *cdfs);
uint16_t *enc_syntax_y_mode_set_cdf(EncSyntaxCdfs *cdfs);
uint16_t *enc_syntax_y_mode_index_cdf(EncSyntaxCdfs *cdfs, size_t context);
uint16_t *enc_syntax_fsc_mode_cdf(
    EncSyntaxCdfs *cdfs, size_t context, size_t block_size_context);
uint16_t *enc_syntax_tx_do_partition_cdf(
    EncSyntaxCdfs *cdfs, size_t fsc_mode, size_t is_inter,
    size_t size_group);
uint16_t *enc_syntax_tx_partition_type_cdf(
    EncSyntaxCdfs *cdfs, size_t fsc_mode, size_t is_inter,
    size_t size_group);
uint16_t *enc_syntax_palette_y_mode_cdf(EncSyntaxCdfs *cdfs);
uint16_t *enc_syntax_txb_skip_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t plane_type,
    size_t tx_size, size_t skip_context);
uint16_t *enc_syntax_eob_pt_64_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t plane_context);
uint16_t *enc_syntax_coeff_base_eob_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t tx_size, size_t context);
uint16_t *enc_syntax_coeff_base_lf_eob_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t tx_size, size_t context);
uint16_t *enc_syntax_coeff_base_lf_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t tx_size, size_t context,
    size_t tcq_state);
uint16_t *enc_syntax_coeff_br_lf_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t context);
uint16_t *enc_syntax_dc_sign_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t plane_type,
    size_t sign_group, size_t context);
uint16_t *enc_syntax_intra_tx_type_set1_cdf(
    EncSyntaxCdfs *cdfs, size_t context);
uint16_t *enc_syntax_intra_tx_type_set2_cdf(
    EncSyntaxCdfs *cdfs, size_t context);
uint16_t *enc_syntax_intra_tx_type_long_cdf(
    EncSyntaxCdfs *cdfs, size_t tx_size_context);

EncStatus enc_m1_sequence_header(void *data, size_t capacity, size_t *size);
EncStatus enc_m1_frame_header(void *data, size_t capacity, size_t *size);
EncStatus enc_m1_tile(void *data, size_t capacity, size_t *size);
EncStatus enc_m3_sequence_header(void *data, size_t capacity, size_t *size,
                                 size_t width, size_t height);
EncStatus enc_m4_sequence_header(void *data, size_t capacity, size_t *size);
EncStatus enc_m3_frame_header(void *data, size_t capacity, size_t *size);
EncStatus enc_m3_frame_header_q(void *data, size_t capacity, size_t *size,
                                unsigned int qindex);
EncStatus enc_m3_frame_header_q_for_size(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int qindex);
EncStatus enc_m4_frame_header(void *data, size_t capacity, size_t *size);
EncStatus enc_m4_ac_tile(void *data, size_t capacity, size_t *size,
                         int16_t dc_level, int16_t ac_level);
EncStatus enc_m3_dc_tile(void *data, size_t capacity, size_t *size,
                         unsigned int quant_level, unsigned int sign);
EncStatus enc_m3_dc_grid_tile(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count);
EncStatus enc_m3_dc_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count,
    unsigned int qindex);

#endif
