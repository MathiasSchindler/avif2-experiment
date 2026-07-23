#ifndef AV2H_DECODER_COEFF_LEVEL_H
#define AV2H_DECODER_COEFF_LEVEL_H

#include "transform.h"

#define AV2H_FIRST_COEFF_CAPACITY 1024U

typedef struct {
    uint16_t eob_base[6U][3U + 4U];
    uint16_t eob_lf[8U][5U + 4U];
    uint16_t base[31U][4U + 4U];
    uint16_t base_lf[56U][6U + 4U];
    uint16_t br[7U][4U + 4U];
    uint16_t br_lf[11U][4U + 4U];
    uint16_t dc_sign[3U][AV2H_BINARY_CDF_SIZE];
    uint16_t fsc_bob[3U][3U + 4U];
    uint16_t fsc_base[6U][4U + 4U];
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

Av2hStatus av2h_first_coeff_level_cdfs_init(Av2hFirstCoeffLevelCdfs *cdfs);
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