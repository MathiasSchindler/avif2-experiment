#ifndef AV2H_DECODER_INTERNAL_COEFF_MODULES_H
#define AV2H_DECODER_INTERNAL_COEFF_MODULES_H

#include "decoder/coeff_level.h"

uint8_t av2h_coeff_q_context(uint16_t base_q_index);
uint16_t av2h_coeff_scan_position(
    uint16_t scan_index, uint16_t width, uint16_t height,
    uint8_t vertical, uint8_t column_scan);
uint8_t av2h_coeff_lf_context(
    const Av2hFirstCoeffBlock *block, uint16_t position,
    uint8_t vertical, uint8_t horizontal);
uint8_t av2h_coeff_base_context(
    const Av2hFirstCoeffBlock *block, uint16_t position,
    uint8_t vertical, uint8_t horizontal);
uint8_t av2h_coeff_br_context(
    const Av2hFirstCoeffBlock *block, uint16_t position,
    uint8_t vertical, uint8_t low, uint8_t horizontal);
uint8_t av2h_coeff_is_low_frequency(
    uint16_t position, uint16_t width, uint8_t vertical,
    uint8_t horizontal);
uint8_t av2h_coeff_eob_context(uint16_t scan_index, uint16_t capacity);
uint8_t av2h_coeff_uv_level_context(
    const Av2hFirstCoeffBlock *block, uint16_t position,
    uint8_t plane, uint8_t low);
uint8_t av2h_coeff_fsc_base_context(
    const Av2hFirstCoeffBlock *block, uint16_t position);
uint8_t av2h_coeff_fsc_br_context(
    const Av2hFirstCoeffBlock *block, uint16_t position);
uint8_t av2h_coeff_fsc_sign_context(
    const Av2hFirstCoeffBlock *block, uint16_t position);
uint16_t *av2h_coeff_level_base_row(
    Av2hFirstCoeffLevelCdfs *cdfs, uint8_t tx_context,
    uint8_t context, uint8_t low);
uint16_t *av2h_coeff_level_br_row(
    Av2hFirstCoeffLevelCdfs *cdfs, uint8_t context, uint8_t low);
uint16_t *av2h_coeff_level_eob_row(
    Av2hFirstCoeffLevelCdfs *cdfs, uint8_t tx_context, uint8_t context);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
const uint16_t *av2h_coeff_level_diagnostic_cdf(void);
const uint64_t *av2h_coeff_level_diagnostic_range(void);
#endif

#endif