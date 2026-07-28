#ifndef AV2H_DECODER_COEFF_LEVEL_H
#define AV2H_DECODER_COEFF_LEVEL_H

#include "transform.h"

extern uint32_t av2h_debug_target_base[5U][10U];
extern uint8_t av2h_debug_target_base_count;
extern uint32_t av2h_debug_target_eob[10U];

#define AV2H_FIRST_COEFF_CAPACITY 1024U

typedef struct {
    uint16_t eob_base[7U][3U + 4U];
    uint16_t q3_eob_base_txs1_ctx1[3U + 4U];
    uint16_t q3_eob_base_txs1_ctx3[3U + 4U];
    uint16_t q3_eob_base_txs2_ctx1[3U + 4U];
    uint16_t q3_eob_base_txs2_ctx2[3U + 4U];
    uint16_t q3_eob_base_txs4_ctx1[3U + 4U];
    uint16_t eob_lf[10U][5U + 4U];
    uint16_t q3_eob_lf_txs1_ctx0[5U + 4U];
    uint16_t q3_eob_lf_txs1_ctx1[5U + 4U];
    uint16_t q3_eob_lf_txs3_ctx2[5U + 4U];
    uint16_t q3_eob_lf_txs4_ctx0[5U + 4U];
    uint16_t q3_eob_lf_txs4_ctx1[5U + 4U];
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
    uint16_t q3_base_txs2_ctx10[4U + 4U];
    uint16_t q3_base_txs2_ctx11[4U + 4U];
    uint16_t q3_base_txs3_ctx2[4U + 4U];
    uint16_t q3_base_txs4_ctx0[4U + 4U];
    uint16_t q3_base_txs4_ctx1[4U + 4U];
    uint16_t q3_base_txs4_ctx2[4U + 4U];
    uint16_t q3_base_txs4_ctx5[4U + 4U];
    uint16_t q3_base_txs4_ctx6[4U + 4U];
    uint16_t q3_base_txs4_ctx10[4U + 4U];
    uint16_t q3_base_txs4_ctx11[4U + 4U];
    uint16_t base_lf[67U][6U + 4U];
    uint16_t q3_base_lf_txs1_ctx21[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx22[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx28[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx29[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx2[6U + 4U];
    uint16_t q3_base_lf_txs0_ctx11[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx0[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx1[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx2[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx29[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx3[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx9[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx10[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx11[6U + 4U];
    uint16_t q3_base_lf_txs1_ctx12[6U + 4U];
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
    uint16_t q3_base_lf_txs2_ctx21[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx28[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx12[6U + 4U];
    uint16_t q3_base_lf_txs3_ctx19[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx3[6U + 4U];
    uint16_t q3_base_lf_txs2_ctx4[6U + 4U];
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
    uint16_t br[7U][4U + 4U];
    uint16_t br_lf[12U][4U + 4U];
    uint16_t q3_br_lf_ctx3[4U + 4U];
    uint16_t q3_br_lf_ctx9[4U + 4U];
    uint16_t dc_sign[3U][AV2H_BINARY_CDF_SIZE];
    uint16_t fsc_bob[3U][3U + 4U];
    uint16_t fsc_base[7U][4U + 4U];
    uint16_t fsc_br[3U][4U + 4U];
    uint16_t fsc_sign[5U][AV2H_BINARY_CDF_SIZE];
    uint16_t fsc_bob_txs1_ctx2[3U + 4U];
    uint16_t fsc_base_txs1_ctx0[4U + 4U];
    uint16_t fsc_base_txs1_ctx3[4U + 4U];
    uint16_t fsc_base_txs1_ctx6[4U + 4U];
    uint16_t fsc_br_txs1_ctx0[4U + 4U];
    uint16_t fsc_br_txs1_ctx5[4U + 4U];
    uint16_t fsc_br_txs1_ctx6[4U + 4U];
    uint16_t fsc_sign_txs1_ctx0[AV2H_BINARY_CDF_SIZE];
    uint16_t fsc_sign_txs1_ctx4[AV2H_BINARY_CDF_SIZE];
    uint16_t fsc_sign_txs1_ctx8[AV2H_BINARY_CDF_SIZE];
    uint16_t fsc_bob_txs2[3U][3U + 4U];
    uint16_t fsc_base_txs2[7U][4U + 4U];
    uint16_t fsc_br_txs2[7U][4U + 4U];
    uint16_t fsc_sign_txs2[9U][AV2H_BINARY_CDF_SIZE];
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
    uint8_t q_context;
} Av2hFirstCoeffLevelCdfs;

typedef struct {
    uint8_t base_levels[AV2H_FIRST_COEFF_CAPACITY];
    int32_t quantized[AV2H_FIRST_COEFF_CAPACITY];
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY];
    uint16_t width;
    uint16_t height;
    uint16_t coefficient_count;
    uint16_t nonzero_count;
    uint8_t cul_level;
    int32_t dc_value;
    uint16_t diagnostic_position;
    uint8_t diagnostic_context;
    uint8_t diagnostic_kind;
    size_t before_levels_bits;
    size_t after_levels_bits;
    uint32_t after_levels_range;
    uint32_t after_levels_value;
    size_t after_coefficients_bits;
} Av2hFirstCoeffBlock;

/*
 * Coefficient-level transform geometry, derived purely from transform
 * dimensions.  This is intentionally enum-independent so later integration can
 * select the coefficient context/geometry for any transform size, including
 * sizes whose enum name is not yet present in this build (the parallel
 * transform-geometry package still owes AV2H_TX_32X16, AV2H_TX_16X64 and
 * AV2H_TX_64X32 to src/decoder/coeff.h).  Integration for those three sizes is
 * a one-line addition: map the new enum to its (tx_width, tx_height) and call
 * av2h_coeff_tx_geometry_from_dims().  The dimension table for the Emerald
 * Q-context-3 remaining-luma set is:
 *   TX_32X8  -> 32x8   ctx 2 shift 0 (256 coeffs)
 *   TX_32X32 -> 32x32  ctx 3 shift 1 (1024 coeffs)
 *   TX_64X32 -> 32x32  ctx 4 shift 2 (1024 coeffs, tx pels 2048)
 *   TX_64X16 -> 32x16  ctx 3 shift 1 (512 coeffs)
 *   TX_16X16 -> 16x16  ctx 2 shift 0 (256 coeffs)
 *   TX_32X16 -> 32x16  ctx 3 shift 1 (512 coeffs)
 *   TX_16X64 -> 16x32  ctx 3 shift 1 (512 coeffs, tx pels 1024)
 *   TX_32X64 -> 32x32  ctx 4 shift 2 (1024 coeffs, tx pels 2048)
 */
typedef struct {
    uint16_t tx_width;
    uint16_t tx_height;
    uint16_t width;
    uint16_t height;
    uint32_t coefficient_count;
    uint8_t tx_context;
    uint8_t dequant_shift;
} Av2hCoeffTxGeometry;

int32_t av2h_coeff_clamp_dequantized_8bit(
    uint64_t magnitude,
    uint8_t sign);
Av2hStatus av2h_coeff_tx_geometry_from_dims(
    uint16_t tx_width,
    uint16_t tx_height,
    Av2hCoeffTxGeometry *geometry);
Av2hStatus av2h_coeff_tx_geometry(
    Av2hFirstTxSize tx_size,
    Av2hCoeffTxGeometry *geometry);
uint32_t av2h_coeff_dequant_step_8bit(uint16_t base_q_index, int delta);
int32_t av2h_coeff_dequantize_level(
    uint32_t level,
    uint8_t sign,
    uint32_t dequant_step,
    uint8_t shift);
uint8_t av2h_coeff_finalize_cul_level(
    uint32_t summed_level,
    int32_t dc_value);
Av2hStatus av2h_first_coeff_level_cdfs_init(Av2hFirstCoeffLevelCdfs *cdfs);
Av2hStatus av2h_first_coeff_level_cdfs_init_for_q(
    Av2hFirstCoeffLevelCdfs *cdfs,
    uint16_t base_q_index);
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
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hFirstCoeffBlock *block);
Av2hStatus av2h_chroma_coeff_block_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hCoeffContextState *state,
    Av2hFirstCoeffBlock *block);

#endif