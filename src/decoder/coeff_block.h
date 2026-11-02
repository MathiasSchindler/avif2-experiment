#ifndef AV2H_DECODER_COEFF_BLOCK_H
#define AV2H_DECODER_COEFF_BLOCK_H

#include "base.h"
#include "quantizer.h"
#include "tx_geometry.h"

#define AV2H_FIRST_COEFF_CAPACITY 1024U

typedef int32_t Av2hDequantizedCoefficient;

typedef int64_t Av2hDequantProduct;

typedef struct {
    uint32_t dc;
    uint32_t ac;
} Av2hCoeffDequantSteps;

typedef struct {
    Av2hFirstTxSize tx_size;
    uint8_t tx_size_context;
    uint8_t txb_skip_context;
    uint8_t all_zero;
    Av2hQIndex q_index;
    uint8_t eob_point;
    uint8_t eob_offset_bits;
    uint16_t eob_extra;
    uint16_t eob;
    size_t before_all_zero_bits;
    size_t after_all_zero_bits;
    size_t after_eob_point_bits;
    size_t after_eob_extra_bit;
    size_t after_eob_bits;
} Av2hFirstCoeffPrefix;

typedef struct {
    uint8_t base_levels[AV2H_FIRST_COEFF_CAPACITY];
    int32_t quantized[AV2H_FIRST_COEFF_CAPACITY];
    Av2hDequantizedCoefficient dequantized[AV2H_FIRST_COEFF_CAPACITY];
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

Av2hDequantizedCoefficient av2h_coeff_clamp_dequantized_10bit(
    uint64_t magnitude,
    uint8_t sign);

Av2hStatus av2h_coeff_tx_geometry_from_dims(
    uint16_t tx_width,
    uint16_t tx_height,
    Av2hCoeffTxGeometry *geometry);

Av2hStatus av2h_coeff_tx_geometry(
    Av2hFirstTxSize tx_size,
    Av2hCoeffTxGeometry *geometry);

uint32_t av2h_coeff_dequant_step_8bit(Av2hQIndex base_q_index, int delta);

uint32_t av2h_coeff_dequant_step_10bit(Av2hQIndex base_q_index, int delta);

int32_t av2h_coeff_dequantize_level(
    uint32_t level,
    uint8_t sign,
    uint32_t dequant_step,
    uint8_t shift);

Av2hDequantizedCoefficient av2h_coeff_dequantize_level_10bit(
    uint32_t level,
    uint8_t sign,
    uint32_t dequant_step,
    uint8_t shift);

uint8_t av2h_coeff_finalize_cul_level(
    uint32_t summed_level,
    int32_t dc_value);

uint8_t av2h_tcq_quantizer(uint8_t state);

uint8_t av2h_tcq_next_state(uint8_t state, uint32_t absolute_level);

uint16_t av2h_coeff_scan_position(
    uint16_t scan_index, uint16_t width, uint16_t height,
    uint8_t vertical, uint8_t column_scan);

#endif
