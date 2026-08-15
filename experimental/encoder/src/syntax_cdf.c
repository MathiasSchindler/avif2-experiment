#include "encoder/cdf.h"
#include "encoder/syntax.h"

#include "normative_cdfs.inc"

#define MATERIALIZE_TABLE(member, source, rows, symbols)                     \
    do {                                                                      \
        size_t row_;                                                          \
        for (row_ = 0U; row_ < (rows); ++row_) {                             \
            EncStatus status_ = enc_cdf_materialize(                          \
                cdfs->member[row_], sizeof(cdfs->member[row_]) /              \
                    sizeof(cdfs->member[row_][0]),                            \
                source[row_], symbols, source[row_][(symbols) - 1U],          \
                (uint8_t)source[row_][symbols]);                              \
            if (status_ != ENC_OK) return status_;                            \
        }                                                                     \
    } while (0)

EncStatus enc_syntax_cdfs_init(EncSyntaxCdfs *cdfs) {
    if (cdfs == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    MATERIALIZE_TABLE(do_split, enc_norm_do_split, ENC_DO_SPLIT_ROWS, 2U);
    MATERIALIZE_TABLE(rect_type, enc_norm_rect_type, ENC_RECT_TYPE_ROWS, 2U);
    MATERIALIZE_TABLE(
        do_ext_partition, enc_norm_do_ext_partition,
        ENC_DO_EXT_PARTITION_ROWS, 2U);
    MATERIALIZE_TABLE(delta_q, enc_norm_delta_q, ENC_DELTA_Q_ROWS, 8U);
    MATERIALIZE_TABLE(y_mode_set, enc_norm_y_mode_set, ENC_Y_MODE_SET_ROWS, 4U);
    MATERIALIZE_TABLE(
        y_mode_index, enc_norm_y_mode_index, ENC_Y_MODE_INDEX_ROWS, 8U);
    MATERIALIZE_TABLE(fsc_mode, enc_norm_fsc_mode, ENC_FSC_MODE_ROWS, 2U);
    MATERIALIZE_TABLE(
        tx_do_partition, enc_norm_tx_do_partition,
        ENC_TX_DO_PARTITION_ROWS, 2U);
    MATERIALIZE_TABLE(
        tx_partition_type, enc_norm_tx_partition_type,
        ENC_TX_PARTITION_TYPE_ROWS, 7U);
    MATERIALIZE_TABLE(
        palette_y_mode, enc_norm_palette_y_mode, ENC_PALETTE_Y_MODE_ROWS, 2U);
    MATERIALIZE_TABLE(txb_skip, enc_norm_txb_skip, ENC_TXB_SKIP_ROWS, 2U);
    MATERIALIZE_TABLE(eob_pt_64, enc_norm_eob_pt_64, ENC_EOB_PT_64_ROWS, 7U);
    MATERIALIZE_TABLE(
        coeff_base_eob, enc_norm_coeff_base_eob,
        ENC_COEFF_BASE_EOB_ROWS, 3U);
    MATERIALIZE_TABLE(
        coeff_base_lf_eob, enc_norm_coeff_base_lf_eob,
        ENC_COEFF_BASE_LF_EOB_ROWS, 5U);
    MATERIALIZE_TABLE(
        coeff_base_lf, enc_norm_coeff_base_lf,
        ENC_COEFF_BASE_LF_ROWS, 6U);
    MATERIALIZE_TABLE(
        coeff_br_lf, enc_norm_coeff_br_lf, ENC_COEFF_BR_LF_ROWS, 4U);
    MATERIALIZE_TABLE(dc_sign, enc_norm_dc_sign, ENC_DC_SIGN_ROWS, 2U);
    MATERIALIZE_TABLE(
        intra_tx_type_set1, enc_norm_intra_tx_type_set1,
        ENC_INTRA_TX_TYPE_SET1_ROWS, 7U);
    MATERIALIZE_TABLE(
        intra_tx_type_set2, enc_norm_intra_tx_type_set2,
        ENC_INTRA_TX_TYPE_SET2_ROWS, 2U);
    MATERIALIZE_TABLE(
        intra_tx_type_long, enc_norm_intra_tx_type_long,
        ENC_INTRA_TX_TYPE_LONG_ROWS, 4U);
    return ENC_OK;
}

#undef MATERIALIZE_TABLE

uint16_t *enc_syntax_do_split_cdf(
    EncSyntaxCdfs *cdfs, size_t partition_structure, size_t context) {
    size_t row = partition_structure * 64U + context;
    return cdfs != 0 && partition_structure < 2U && context < 64U
        ? cdfs->do_split[row] : 0;
}

uint16_t *enc_syntax_rect_type_cdf(
    EncSyntaxCdfs *cdfs, size_t partition_structure, size_t context) {
    size_t row = partition_structure * 64U + context;
    return cdfs != 0 && partition_structure < 2U && context < 64U
        ? cdfs->rect_type[row] : 0;
}

uint16_t *enc_syntax_do_ext_partition_cdf(
    EncSyntaxCdfs *cdfs, size_t partition_structure, size_t context) {
    size_t row = partition_structure * 64U + context;
    return cdfs != 0 && partition_structure < 2U && context < 64U
        ? cdfs->do_ext_partition[row] : 0;
}

uint16_t *enc_syntax_delta_q_cdf(EncSyntaxCdfs *cdfs) {
    return cdfs != 0 ? cdfs->delta_q[0] : 0;
}

uint16_t *enc_syntax_y_mode_set_cdf(EncSyntaxCdfs *cdfs) {
    return cdfs != 0 ? cdfs->y_mode_set[0] : 0;
}

uint16_t *enc_syntax_y_mode_index_cdf(EncSyntaxCdfs *cdfs, size_t context) {
    return cdfs != 0 && context < 3U ? cdfs->y_mode_index[context] : 0;
}

uint16_t *enc_syntax_fsc_mode_cdf(
    EncSyntaxCdfs *cdfs, size_t context, size_t block_size_context) {
    size_t row = context * 6U + block_size_context;
    return cdfs != 0 && context < 4U && block_size_context < 6U
        ? cdfs->fsc_mode[row] : 0;
}

uint16_t *enc_syntax_tx_do_partition_cdf(
    EncSyntaxCdfs *cdfs, size_t fsc_mode, size_t is_inter,
    size_t size_group) {
    size_t row = (fsc_mode * 2U + is_inter) * 9U + size_group;
    return cdfs != 0 && fsc_mode < 2U && is_inter < 2U &&
            size_group < 9U
        ? cdfs->tx_do_partition[row] : 0;
}

uint16_t *enc_syntax_tx_partition_type_cdf(
    EncSyntaxCdfs *cdfs, size_t fsc_mode, size_t is_inter,
    size_t size_group) {
    size_t row = (fsc_mode * 2U + is_inter) * 14U + size_group;
    return cdfs != 0 && fsc_mode < 2U && is_inter < 2U &&
            size_group < 14U
        ? cdfs->tx_partition_type[row] : 0;
}

uint16_t *enc_syntax_palette_y_mode_cdf(EncSyntaxCdfs *cdfs) {
    return cdfs != 0 ? cdfs->palette_y_mode[0] : 0;
}

uint16_t *enc_syntax_txb_skip_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t plane_type,
    size_t tx_size, size_t skip_context) {
    size_t row = (((q_context * 2U + plane_type) * 5U + tx_size) * 10U +
                  skip_context);
    return cdfs != 0 && q_context < 4U && plane_type < 2U &&
            tx_size < 5U && skip_context < 10U
        ? cdfs->txb_skip[row] : 0;
}

uint16_t *enc_syntax_eob_pt_64_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t plane_context) {
    size_t row = q_context * 3U + plane_context;
    return cdfs != 0 && q_context < 4U && plane_context < 3U
        ? cdfs->eob_pt_64[row] : 0;
}

uint16_t *enc_syntax_coeff_base_eob_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t tx_size, size_t context) {
    size_t row = (q_context * 5U + tx_size) * 4U + context;
    return cdfs != 0 && q_context < 4U && tx_size < 5U && context < 4U
        ? cdfs->coeff_base_eob[row] : 0;
}

uint16_t *enc_syntax_coeff_base_lf_eob_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t tx_size, size_t context) {
    size_t row = (q_context * 5U + tx_size) * 4U + context;
    return cdfs != 0 && q_context < 4U && tx_size < 5U && context < 4U
        ? cdfs->coeff_base_lf_eob[row] : 0;
}

uint16_t *enc_syntax_coeff_base_lf_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t tx_size, size_t context,
    size_t tcq_state) {
    size_t row = ((q_context * 5U + tx_size) * 33U + context) * 2U +
                 tcq_state;
    return cdfs != 0 && q_context < 4U && tx_size < 5U &&
            context < 33U && tcq_state < 2U
        ? cdfs->coeff_base_lf[row] : 0;
}

uint16_t *enc_syntax_coeff_br_lf_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t context) {
    size_t row = q_context * 14U + context;
    return cdfs != 0 && q_context < 4U && context < 14U
        ? cdfs->coeff_br_lf[row] : 0;
}

uint16_t *enc_syntax_dc_sign_cdf(
    EncSyntaxCdfs *cdfs, size_t q_context, size_t plane_type,
    size_t sign_group, size_t context) {
    size_t row = (((q_context * 2U + plane_type) * 2U + sign_group) * 3U +
                  context);
    return cdfs != 0 && q_context < 4U && plane_type < 2U &&
            sign_group < 2U && context < 3U
        ? cdfs->dc_sign[row] : 0;
}

uint16_t *enc_syntax_intra_tx_type_set1_cdf(
    EncSyntaxCdfs *cdfs, size_t context) {
    return cdfs != 0 && context < 3U
        ? cdfs->intra_tx_type_set1[context] : 0;
}

uint16_t *enc_syntax_intra_tx_type_set2_cdf(
    EncSyntaxCdfs *cdfs, size_t context) {
    return cdfs != 0 && context < 3U
        ? cdfs->intra_tx_type_set2[context] : 0;
}

uint16_t *enc_syntax_intra_tx_type_long_cdf(
    EncSyntaxCdfs *cdfs, size_t tx_size_context) {
    return cdfs != 0 && tx_size_context < 4U
        ? cdfs->intra_tx_type_long[tx_size_context] : 0;
}
