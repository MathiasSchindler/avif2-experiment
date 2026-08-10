#include "internal/coeff_modules.h"

uint8_t av2h_coeff_q_context(uint16_t base_q_index) {
    return base_q_index <= 90U ? 0U :
           base_q_index <= 140U ? 1U :
           base_q_index <= 190U ? 2U : 3U;
}

int32_t av2h_coeff_clamp_dequantized_8bit(
    uint64_t magnitude,
    uint8_t sign) {
    if (sign) {
        return magnitude > 32768U ? -32768 : -(int32_t)magnitude;
    }
    return magnitude > 32767U ? 32767 : (int32_t)magnitude;
}

uint32_t av2h_coeff_dequant_step_8bit(uint16_t base_q_index, int delta) {
    static const uint16_t lookup[25U] = {
        64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U, 51U, 52U, 54U,
        55U, 57U, 59U, 60U, 62U, 64U, 66U, 68U, 70U, 72U, 74U, 76U,
        78U
    };
    int q_index = (int)base_q_index + delta;
    if (base_q_index == 0U && delta <= 0) return lookup[0];
    if (q_index < 1) q_index = 1;
    if (q_index > 255) q_index = 255;
    if (q_index < 25) return lookup[q_index];
    return (uint32_t)lookup[((q_index - 1) % 24) + 1] <<
           ((q_index - 1) / 24);
}

int32_t av2h_coeff_dequantize_level(
    uint32_t level,
    uint8_t sign,
    uint32_t dequant_step,
    uint8_t shift) {
    uint64_t high_precision =
        ((uint64_t)level * dequant_step) & (uint64_t)0xffffffU;
    uint64_t magnitude = (high_precision + 4U) >> 3U;
    magnitude >>= shift;
    return av2h_coeff_clamp_dequantized_8bit(magnitude, sign);
}

uint8_t av2h_coeff_finalize_cul_level(
    uint32_t summed_level,
    int32_t dc_value) {
    uint32_t cul_level = summed_level > 7U ? 7U : summed_level;
    if (dc_value < 0) {
        cul_level |= 8U;
    } else if (dc_value > 0) {
        cul_level += 16U;
    }
    return (uint8_t)cul_level;
}

static uint8_t coeff_dim_log2(uint16_t dimension) {
    uint8_t log2 = 0U;
    while ((dimension >> log2) > 1) ++log2;
    return log2;
}

Av2hStatus av2h_coeff_tx_geometry_from_dims(
    uint16_t tx_width,
    uint16_t tx_height,
    Av2hCoeffTxGeometry *geometry) {
    uint32_t pels;
    uint8_t low_log2;
    uint8_t high_log2;
    if (geometry == 0) return AV2H_INVALID_ARGUMENT;
    if (tx_width < 4U || tx_width > 64U || tx_height < 4U ||
        tx_height > 64U ||
        (tx_width & (tx_width - 1U)) != 0U ||
        (tx_height & (tx_height - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry->tx_width = tx_width;
    geometry->tx_height = tx_height;
    geometry->width = tx_width > 32U ? 32U : tx_width;
    geometry->height = tx_height > 32U ? 32U : tx_height;
    geometry->coefficient_count =
        (uint32_t)geometry->width * geometry->height;
    pels = (uint32_t)tx_width * tx_height;
    geometry->dequant_shift =
        (uint8_t)((pels > 256U ? 1U : 0U) + (pels > 1024U ? 1U : 0U));
    low_log2 = coeff_dim_log2(
        tx_width < tx_height ? tx_width : tx_height);
    high_log2 = coeff_dim_log2(
        tx_width > tx_height ? tx_width : tx_height);
    geometry->tx_context = (uint8_t)(
        ((low_log2 - 2U) + (high_log2 - 2U) + 1U) >> 1U);
    return AV2H_OK;
}

Av2hStatus av2h_coeff_tx_geometry(
    Av2hFirstTxSize tx_size,
    Av2hCoeffTxGeometry *geometry) {
    uint16_t tx_width;
    uint16_t tx_height;
    Av2hStatus status;
    if (geometry == 0) return AV2H_INVALID_ARGUMENT;
    switch (tx_size) {
        case AV2H_TX_4X4: tx_width = 4U; tx_height = 4U; break;
        case AV2H_TX_8X8: tx_width = 8U; tx_height = 8U; break;
        case AV2H_TX_16X16: tx_width = 16U; tx_height = 16U; break;
        case AV2H_TX_32X32: tx_width = 32U; tx_height = 32U; break;
        case AV2H_TX_64X64: tx_width = 64U; tx_height = 64U; break;
        case AV2H_TX_4X8: tx_width = 4U; tx_height = 8U; break;
        case AV2H_TX_8X4: tx_width = 8U; tx_height = 4U; break;
        case AV2H_TX_8X16: tx_width = 8U; tx_height = 16U; break;
        case AV2H_TX_16X8: tx_width = 16U; tx_height = 8U; break;
        case AV2H_TX_16X32: tx_width = 16U; tx_height = 32U; break;
        case AV2H_TX_32X16: tx_width = 32U; tx_height = 16U; break;
        case AV2H_TX_64X32: tx_width = 64U; tx_height = 32U; break;
        case AV2H_TX_32X64: tx_width = 32U; tx_height = 64U; break;
        case AV2H_TX_4X16: tx_width = 4U; tx_height = 16U; break;
        case AV2H_TX_16X4: tx_width = 16U; tx_height = 4U; break;
        case AV2H_TX_8X32: tx_width = 8U; tx_height = 32U; break;
        case AV2H_TX_32X8: tx_width = 32U; tx_height = 8U; break;
        case AV2H_TX_16X64: tx_width = 16U; tx_height = 64U; break;
        case AV2H_TX_64X16: tx_width = 64U; tx_height = 16U; break;
        case AV2H_TX_4X32: tx_width = 4U; tx_height = 32U; break;
        case AV2H_TX_32X4: tx_width = 32U; tx_height = 4U; break;
        case AV2H_TX_8X64: tx_width = 8U; tx_height = 64U; break;
        case AV2H_TX_64X8: tx_width = 64U; tx_height = 8U; break;
        case AV2H_TX_4X64: tx_width = 4U; tx_height = 64U; break;
        case AV2H_TX_64X4: tx_width = 64U; tx_height = 4U; break;
        default: return AV2H_UNSUPPORTED;
    }
    status = av2h_coeff_tx_geometry_from_dims(tx_width, tx_height, geometry);
    if (status != AV2H_OK) return status;
    return status;
}