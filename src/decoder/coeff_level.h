#ifndef AV2H_DECODER_COEFF_LEVEL_H
#define AV2H_DECODER_COEFF_LEVEL_H

#include "coeff.h"
#include "coeff_block.h"
#include "transform_type.h"

typedef struct {
    uint16_t full_eob_base[5U][4U][3U + 4U];
    uint16_t full_eob_base_lf[5U][4U][5U + 4U];
    uint16_t full_base[5U][20U][2U][4U + 4U];
    uint16_t full_base_lf[5U][33U][2U][6U + 4U];
    uint16_t full_br[7U][4U + 4U];
    uint16_t full_br_lf[14U][4U + 4U];
    uint16_t full_uv_eob_base[4U][3U + 4U];
    uint16_t full_uv_eob_base_lf[4U][5U + 4U];
    uint16_t full_uv_base[12U][4U + 4U];
    uint16_t full_uv_base_lf[12U][6U + 4U];
    uint16_t full_uv_br[4U][4U + 4U];
    uint16_t q0_txs4_eob_base[4U][3U + 4U];
    uint16_t q0_txs4_eob_base_lf[4U][5U + 4U];
    uint16_t q0_txs4_base[20U][4U + 4U];
    uint16_t q0_txs4_base_lf[33U][6U + 4U];
    uint16_t q0_txs3_eob_base[4U][3U + 4U];
    uint16_t q0_txs3_eob_base_lf[4U][5U + 4U];
    uint16_t q0_txs3_base[20U][4U + 4U];
    uint16_t q0_txs3_base_lf[33U][6U + 4U];
    uint16_t q0_txs2_eob_base[4U][3U + 4U];
    uint16_t q0_txs2_eob_base_lf[4U][5U + 4U];
    uint16_t q0_txs2_base[20U][4U + 4U];
    uint16_t q0_txs2_base_lf[33U][6U + 4U];
    uint16_t q0_txs1_eob_base[4U][3U + 4U];
    uint16_t q0_txs1_eob_base_lf[4U][5U + 4U];
    uint16_t q0_txs1_base[20U][4U + 4U];
    uint16_t q0_txs1_base_lf[33U][6U + 4U];
    uint16_t q0_txs0_eob_base[4U][3U + 4U];
    uint16_t q0_txs0_eob_base_lf[4U][5U + 4U];
    uint16_t q0_txs0_base[20U][4U + 4U];
    uint16_t q0_txs0_base_lf[33U][6U + 4U];
    uint16_t q0_br[7U][4U + 4U];
    uint16_t q0_br_lf[14U][4U + 4U];
    uint16_t eob_base[7U][3U + 4U];
    uint16_t q3_eob_base_txs1_ctx1[3U + 4U];
    uint16_t q3_eob_base_txs1_ctx2[3U + 4U];
    uint16_t q3_eob_base_txs1_ctx3[3U + 4U];
    uint16_t q3_eob_base_txs2_ctx1[3U + 4U];
    uint16_t q3_eob_base_txs2_ctx2[3U + 4U];
    uint16_t q3_eob_base_txs4_ctx1[3U + 4U];
    uint16_t q1_eob_base_txs4_ctx1[3U + 4U];
    uint16_t eob_lf[10U][5U + 4U];
    uint16_t q3_eob_lf_txs1_ctx0[5U + 4U];
    uint16_t q3_eob_lf_txs1_ctx1[5U + 4U];
    uint16_t q3_eob_lf_txs1_ctx2[5U + 4U];
    uint16_t q3_eob_lf_txs1_ctx3[5U + 4U];
    uint16_t q3_eob_lf_txs3_ctx2[5U + 4U];
    uint16_t q3_eob_lf_txs4_ctx0[5U + 4U];
    uint16_t q3_eob_lf_txs4_ctx1[5U + 4U];
    uint16_t q3_eob_lf_txs0_ctx0[5U + 4U];
    uint16_t q3_eob_lf_txs0_ctx1[5U + 4U];
    uint16_t q3_eob_lf_txs0_ctx2[5U + 4U];
    uint16_t q2_eob_lf_txs4_ctx1[5U + 4U];
    uint16_t base[40U][4U + 4U];
    uint16_t q3_base_txs1_ctx0[4U + 4U];
    uint16_t q3_base_txs1_ctx1[4U + 4U];
    uint16_t q3_base_txs1_ctx2[4U + 4U];
    uint16_t q3_base_txs1_ctx5[4U + 4U];
    uint16_t q3_base_txs1_ctx6[4U + 4U];
    uint16_t q3_base_txs1_ctx7[4U + 4U];
    uint16_t q3_base_txs1_ctx10[4U + 4U];
    uint16_t q3_base_txs1_ctx11[4U + 4U];
    uint16_t q3_base_txs1_ctx12[4U + 4U];
    uint16_t q3_base_txs2_ctx5[4U + 4U];
    uint16_t q3_base_txs2_ctx6[4U + 4U];
    uint16_t q3_base_txs2_ctx7[4U + 4U];
    uint16_t q3_base_txs2_ctx10[4U + 4U];
    uint16_t q3_base_txs2_ctx11[4U + 4U];
    uint16_t q3_base_txs2_ctx3[4U + 4U];
    uint16_t q3_base_txs3_ctx2[4U + 4U];
    uint16_t q3_base_txs4_ctx0[4U + 4U];
    uint16_t q3_base_txs4_ctx1[4U + 4U];
    uint16_t q3_base_txs4_ctx2[4U + 4U];
    uint16_t q3_base_txs4_ctx5[4U + 4U];
    uint16_t q3_base_txs4_ctx6[4U + 4U];
    uint16_t q3_base_txs4_ctx10[4U + 4U];
    uint16_t q3_base_txs4_ctx11[4U + 4U];
    uint16_t q1_base_txs4_ctx0[4U + 4U];
    uint16_t q1_base_txs4_ctx1[4U + 4U];
    uint16_t q1_base_txs4_ctx2[4U + 4U];
    uint16_t q1_base_txs4_ctx5[4U + 4U];
    uint16_t base_lf[67U][6U + 4U];
    uint16_t q3_base_lf_txs1_ctx21[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx22[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx23[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx28[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx29[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx30[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx1[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx2[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx9[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx21[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx22[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx6[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx11[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx0[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx1[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx2[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx4[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx5[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx29[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx3[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx9[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx10[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx11[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx12[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx13[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx22[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx16[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx17[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx18[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx19[6U + 4U];
    uint16_t q2_base_lf_txs3_ctx1[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx11[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx12[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx0[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx1[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx3[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx5[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx21[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx28[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx12[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx13[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx19[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx3[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx4[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx5[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx3[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx1[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx2[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx9[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx10[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx11[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx12[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx13[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx16[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx17[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx18[6U + 4U];
    uint16_t q3_base_lf_txs4_ctx19[6U + 4U];
    uint16_t q2_base_lf_txs4_ctx1[6U + 4U];
    uint16_t q2_base_lf_txs4_ctx9[6U + 4U];
    uint16_t q1_base_lf_txs4_ctx8[6U + 4U];
    uint16_t q1_base_lf_txs4_ctx15[6U + 4U];
    uint16_t q1_base_lf_txs4_ctx17[6U + 4U];
    uint16_t q1_base_lf_txs4_ctx19[6U + 4U];
    uint16_t q1_base_lf_txs4_ctx20[6U + 4U];
    uint16_t br[7U][4U + 4U];
    uint16_t br_lf[12U][4U + 4U];
    uint16_t q3_br_lf_ctx3[4U + 4U];
    uint16_t q3_br_lf_ctx4[4U + 4U];
    uint16_t q3_br_lf_ctx9[4U + 4U];
    uint16_t q1_br_lf_ctx12[4U + 4U];
    uint16_t dc_sign[3U][AV2H_BINARY_CDF_SIZE];
    uint16_t parity_dc_sign[3U][AV2H_BINARY_CDF_SIZE];
    uint16_t parity_base[5U][4U + 4U];
    uint16_t fsc_bob_all[3U][3U][3U + 4U];
    uint16_t fsc_base_all[3U][7U][4U + 4U];
    uint16_t fsc_br_all[3U][7U][4U + 4U];
    uint16_t fsc_sign_all[3U][9U][AV2H_BINARY_CDF_SIZE];
    uint16_t uv_eob_lf_ctx0[5U + 4U];
    uint16_t uv_eob_ctx1[3U + 4U];
    uint16_t uv_eob_ctx3[3U + 4U];
    uint16_t uv_base[2U][4U + 4U];
    uint16_t uv_base_ctx1[4U + 4U];
    uint16_t uv_base_ctx3[4U + 4U];
    uint16_t uv_base_ctx5[4U + 4U];
    uint16_t uv_base_ctx6[4U + 4U];
    uint16_t uv_base_ctx7[4U + 4U];
    uint16_t uv_base_lf[2U][6U + 4U];
    uint16_t uv_base_lf_ctx1[6U + 4U];
    uint16_t uv_base_lf_ctx3[6U + 4U];
    uint16_t uv_base_lf_ctx5[6U + 4U];
    uint16_t uv_base_lf_ctx7[6U + 4U];
    uint16_t uv_br_ctx0[4U + 4U];
    uint16_t uv_br_ctx1[4U + 4U];
    uint16_t uv_br_ctx2[4U + 4U];
    uint16_t uv_br_ctx3[4U + 4U];
    uint16_t q3_uv_eob_lf[4U][5U + 4U];
    uint16_t q3_uv_eob[4U][3U + 4U];
    uint16_t q3_uv_base_lf[12U][6U + 4U];
    uint16_t q3_uv_base[12U][4U + 4U];
    uint16_t q3_uv_br[4U][4U + 4U];
    uint16_t q0_uv_eob_lf[4U][5U + 4U];
    uint16_t q0_uv_eob[4U][3U + 4U];
    uint16_t q0_uv_base_lf[12U][6U + 4U];
    uint16_t q0_uv_base[12U][4U + 4U];
    uint16_t q0_uv_br[4U][4U + 4U];
    uint16_t adjusted_uv_eob[2U][3U + 4U];
    uint16_t adjusted_uv_eob_lf[2U][5U + 4U];
    uint16_t adjusted_uv_base[2U][7U][4U + 4U];
    uint16_t adjusted_uv_base_lf[2U][8U][6U + 4U];
    uint16_t adjusted_uv_br[2U][3U][4U + 4U];
    uint8_t q_context;
} Av2hFirstCoeffLevelCdfs;

Av2hStatus av2h_coeff_dequant_steps(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hQIndex q_index,
    uint8_t plane,
    Av2hCoeffDequantSteps *steps);
Av2hDequantizedCoefficient av2h_coeff_dequantize_level_for_sequence(
    const Av2hSequenceState *sequence,
    uint32_t level,
    uint8_t sign,
    uint32_t dequant_step,
    uint8_t shift);
int av2h_quantizer_is_lossless(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hQIndex q_index);
Av2hStatus av2h_coeff_qmatrix_dequant_step(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hQIndex q_index,
    uint8_t segment_id,
    uint8_t plane,
    uint16_t tx_width,
    uint16_t tx_height,
    uint8_t primary_type,
    uint16_t position,
    uint32_t dequant_step,
    uint32_t *weighted_step);
Av2hStatus av2h_first_coeff_level_cdfs_init(Av2hFirstCoeffLevelCdfs *cdfs);
Av2hStatus av2h_first_coeff_level_cdfs_init_for_q(
    Av2hFirstCoeffLevelCdfs *cdfs,
    Av2hQIndex base_q_index);
Av2hStatus av2h_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    uint8_t dc_sign_context,
    Av2hFirstCoeffBlock *block);
Av2hStatus av2h_coeff_block_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    Av2hCoeffContextState *state,
    Av2hFirstCoeffBlock *block);
Av2hStatus av2h_first_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    Av2hFirstCoeffBlock *block);
Av2hStatus av2h_first_chroma_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t segment_id,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hFirstCoeffBlock *block);
Av2hStatus av2h_chroma_coeff_block_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t segment_id,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    Av2hCoeffContextState *state,
    Av2hFirstCoeffBlock *block);

#endif