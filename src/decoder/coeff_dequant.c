#include "internal/coeff_modules.h"

#include "shared/bit_math.h"

#include <stddef.h>

#include "predefined_qmatrix.inc"

uint8_t av2h_coeff_q_context(Av2hQIndex base_q_index) {
    return base_q_index <= 90U ? 0U : base_q_index <= 140U ? 1U : base_q_index <= 190U ? 2U : 3U;
}

int32_t av2h_coeff_clamp_dequantized_8bit(uint64_t magnitude, uint8_t sign) {
    if (sign) {
        return magnitude > 32768U ? -32768 : -(int32_t)magnitude;
    }
    return magnitude > 32767U ? 32767 : (int32_t)magnitude;
}

Av2hDequantizedCoefficient av2h_coeff_clamp_dequantized_10bit(uint64_t magnitude, uint8_t sign) {
    if (sign) {
        return magnitude > 131072U ? -131072 : -(int32_t)magnitude;
    }
    return magnitude > 131071U ? 131071 : (int32_t)magnitude;
}

static uint32_t coeff_dequant_step(Av2hQIndex base_q_index, int delta, int64_t maximum_q_index) {
    static const uint16_t lookup[25U] = {64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U,
                                         51U, 52U, 54U, 55U, 57U, 59U, 60U, 62U, 64U,
                                         66U, 68U, 70U, 72U, 74U, 76U, 78U};
    int64_t q_index = (int64_t)base_q_index + delta;
    if (base_q_index == 0U && delta <= 0)
        return lookup[0];
    if (q_index < 1)
        q_index = 1;
    if (q_index > maximum_q_index)
        q_index = maximum_q_index;
    if (q_index < 25)
        return lookup[(size_t)q_index];
    return (uint32_t)lookup[(size_t)((q_index - 1) % 24) + 1U]
           << (unsigned int)((q_index - 1) / 24);
}

uint32_t av2h_coeff_dequant_step_8bit(Av2hQIndex base_q_index, int delta) {
    return coeff_dequant_step(base_q_index, delta, 255);
}

uint32_t av2h_coeff_dequant_step_10bit(Av2hQIndex base_q_index, int delta) {
    return coeff_dequant_step(base_q_index, delta, 303);
}

static Av2hDequantizedCoefficient coeff_dequantize_level(uint32_t level, uint8_t sign,
                                                         uint32_t dequant_step, uint8_t shift,
                                                         uint64_t maximum_magnitude) {
    const uint64_t maximum_product = ((uint64_t)-1) >> 1U;
    Av2hDequantProduct product;
    if (dequant_step != 0U && (uint64_t)level > maximum_product / dequant_step) {
        product = (Av2hDequantProduct)maximum_product;
    } else {
        product = (Av2hDequantProduct)level * (Av2hDequantProduct)dequant_step;
    }
    uint64_t magnitude = ((uint64_t)product & (uint64_t)0xffffffU) + 4U;
    magnitude >>= 3U;
    magnitude >>= shift;
    if (sign) {
        return magnitude > maximum_magnitude ? -(int32_t)maximum_magnitude : -(int32_t)magnitude;
    }
    return magnitude >= maximum_magnitude ? (int32_t)(maximum_magnitude - 1U) : (int32_t)magnitude;
}

int32_t av2h_coeff_dequantize_level(uint32_t level, uint8_t sign, uint32_t dequant_step,
                                    uint8_t shift) {
    return coeff_dequantize_level(level, sign, dequant_step, shift, 32768U);
}

Av2hDequantizedCoefficient av2h_coeff_dequantize_level_10bit(uint32_t level, uint8_t sign,
                                                             uint32_t dequant_step, uint8_t shift) {
    return coeff_dequantize_level(level, sign, dequant_step, shift, 131072U);
}

Av2hStatus av2h_coeff_dequant_steps(const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                    Av2hQIndex q_index, uint8_t plane,
                                    Av2hCoeffDequantSteps *steps) {
    int dc_delta;
    int ac_delta;
    if (sequence == NULL || frame == NULL || steps == NULL || plane >= 3U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->info.bit_depth_idc != 0U && sequence->info.bit_depth_idc != 1U) {
        return AV2H_UNSUPPORTED;
    }
    if (plane == 0U) {
        dc_delta = frame->y_dc_delta_q + sequence->base_y_dc_delta_q;
        ac_delta = 0;
    } else if (plane == 1U) {
        dc_delta = frame->u_dc_delta_q + sequence->base_uv_dc_delta_q;
        ac_delta = frame->u_ac_delta_q + sequence->base_uv_ac_delta_q;
    } else {
        dc_delta = frame->v_dc_delta_q + sequence->base_uv_dc_delta_q;
        ac_delta = frame->v_ac_delta_q + sequence->base_uv_ac_delta_q;
    }
    if (sequence->info.bit_depth_idc == 0U) {
        steps->dc = av2h_coeff_dequant_step_10bit(q_index, dc_delta);
        steps->ac = av2h_coeff_dequant_step_10bit(q_index, ac_delta);
    } else {
        steps->dc = av2h_coeff_dequant_step_8bit(q_index, dc_delta);
        steps->ac = av2h_coeff_dequant_step_8bit(q_index, ac_delta);
    }
    return AV2H_OK;
}

Av2hDequantizedCoefficient
av2h_coeff_dequantize_level_for_sequence(const Av2hSequenceState *sequence, uint32_t level,
                                         uint8_t sign, uint32_t dequant_step, uint8_t shift) {
    if (sequence != NULL && sequence->info.bit_depth_idc == 0U) {
        return av2h_coeff_dequantize_level_10bit(level, sign, dequant_step, shift);
    }
    return av2h_coeff_dequantize_level(level, sign, dequant_step, shift);
}

int av2h_quantizer_is_lossless(const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                               Av2hQIndex q_index) {
    if (sequence == NULL || frame == NULL || q_index != 0U || frame->delta_q_present != 0U) {
        return 0;
    }
    return frame->y_dc_delta_q + sequence->base_y_dc_delta_q <= 0 &&
           frame->u_dc_delta_q + sequence->base_uv_dc_delta_q <= 0 &&
           frame->v_dc_delta_q + sequence->base_uv_dc_delta_q <= 0 &&
           frame->u_ac_delta_q + sequence->base_uv_ac_delta_q <= 0 &&
           frame->v_ac_delta_q + sequence->base_uv_ac_delta_q <= 0;
}

static size_t qmatrix_offset(uint16_t width, uint16_t height) {
    if (width == 4U && height == 4U)
        return 0U;
    if (width == 8U && height == 8U)
        return 16U;
    if (width == 16U && height == 16U)
        return 80U;
    if (width == 32U && height == 32U)
        return 336U;
    if (width == 4U && height == 8U)
        return 1360U;
    if (width == 8U && height == 4U)
        return 1392U;
    if (width == 8U && height == 16U)
        return 1424U;
    if (width == 16U && height == 8U)
        return 1552U;
    if (width == 16U && height == 32U)
        return 1680U;
    if (width == 32U && height == 16U)
        return 2192U;
    if (width == 4U && height == 16U)
        return 2704U;
    if (width == 16U && height == 4U)
        return 2768U;
    if (width == 8U && height == 32U)
        return 2832U;
    if (width == 32U && height == 8U)
        return 3088U;
    if (width == 4U && height == 32U)
        return 3344U;
    if (width == 32U && height == 4U)
        return 3472U;
    return SIZE_MAX;
}

static const uint8_t *custom_qmatrix(const Av2hFrameState *frame, uint8_t id, uint8_t plane,
                                     uint16_t width, uint16_t height, uint16_t position) {
    const uint8_t *fundamental = frame->custom_qm[id][plane];
    uint16_t row;
    uint16_t column;
    if (position >= (uint32_t)width * height)
        return NULL;
    if (!((width == 4U && height == 4U) || (width == 8U && height == 8U) ||
          (width == 8U && height == 4U) || (width == 4U && height == 8U))) {
        return NULL;
    }
    row = position / width;
    column = position % width;
    if (width == 4U && height == 4U) {
        return &fundamental[(row * 2U) * 8U + column * 2U];
    }
    if (width == 8U && height == 8U)
        return &fundamental[position];
    if (width == 8U && height == 4U)
        return &fundamental[64U + position];
    if (width == 4U && height == 8U)
        return &fundamental[96U + position];
    return NULL;
}

Av2hStatus av2h_coeff_qmatrix_dequant_step(const Av2hSequenceState *sequence,
                                           const Av2hFrameState *frame, Av2hQIndex q_index,
                                           uint8_t segment_id, uint8_t plane, uint16_t tx_width,
                                           uint16_t tx_height, uint8_t primary_type,
                                           uint16_t position, uint32_t dequant_step,
                                           uint32_t *weighted_step) {
    uint8_t level;
    size_t offset;
    uint64_t product;
    uint8_t matrix;
    const uint8_t *custom_weight;
    if (sequence == NULL || frame == NULL || weighted_step == NULL || plane >= 3U ||
        segment_id >= av2h_sequence_segment_count(sequence)) {
        return AV2H_INVALID_ARGUMENT;
    }
    *weighted_step = dequant_step;
    if (frame->using_qmatrix == 0U || primary_type >= AV2H_TX_IDTX ||
        av2h_quantizer_is_lossless(sequence, frame, q_index)) {
        return AV2H_OK;
    }
    matrix = frame->segment_qm_index[segment_id];
    if (matrix >= frame->picture_qm_count)
        return AV2H_INVALID_DATA;
    level = plane == 0U   ? frame->qm_y[matrix]
            : plane == 1U ? frame->qm_u[matrix]
                          : frame->qm_v[matrix];
    if (level == 15U)
        return AV2H_OK;
    if ((frame->custom_qm_bitmap & (uint16_t)(1U << level)) != 0U &&
        plane >= frame->custom_qm_plane_count[level]) {
        return AV2H_INVALID_DATA;
    }
    custom_weight = (frame->custom_qm_bitmap & (uint16_t)(1U << level)) != 0U
                        ? custom_qmatrix(frame, level, plane, tx_width, tx_height, position)
                        : NULL;
    if (custom_weight != NULL) {
        product = (uint64_t)dequant_step * *custom_weight;
        product = (product + 16U) >> 5U;
        if (product > UINT32_MAX)
            return AV2H_OVERFLOW;
        *weighted_step = (uint32_t)product;
        return AV2H_OK;
    }
    offset = qmatrix_offset(tx_width, tx_height);
    if (offset == SIZE_MAX || position >= (uint32_t)tx_width * tx_height ||
        offset + position >= 3600U) {
        return AV2H_UNSUPPORTED;
    }
    product =
        (uint64_t)dequant_step * av2h_predefined_iqmatrix[level][plane != 0U][offset + position];
    product = (product + 16U) >> 5U;
    if (product > UINT32_MAX)
        return AV2H_OVERFLOW;
    *weighted_step = (uint32_t)product;
    return AV2H_OK;
}

uint8_t av2h_coeff_finalize_cul_level(uint32_t summed_level, int32_t dc_value) {
    uint32_t cul_level = summed_level > 7U ? 7U : summed_level;
    if (dc_value < 0) {
        cul_level |= 8U;
    } else if (dc_value > 0) {
        cul_level += 16U;
    }
    return (uint8_t)cul_level;
}

Av2hStatus av2h_coeff_tx_geometry_from_dims(uint16_t tx_width, uint16_t tx_height,
                                            Av2hCoeffTxGeometry *geometry) {
    uint32_t pels;
    uint8_t low_log2;
    uint8_t high_log2;
    if (geometry == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (tx_width < 4U || tx_width > 64U || tx_height < 4U || tx_height > 64U ||
        (tx_width & (tx_width - 1U)) != 0U || (tx_height & (tx_height - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry->tx_width = tx_width;
    geometry->tx_height = tx_height;
    geometry->width = tx_width > 32U ? 32U : tx_width;
    geometry->height = tx_height > 32U ? 32U : tx_height;
    geometry->coefficient_count = (uint32_t)geometry->width * geometry->height;
    pels = (uint32_t)tx_width * tx_height;
    geometry->dequant_shift = (uint8_t)((pels > 256U ? 1U : 0U) + (pels > 1024U ? 1U : 0U));
    low_log2 = (uint8_t)av2h_floor_log2_u32(tx_width < tx_height ? tx_width : tx_height);
    high_log2 = (uint8_t)av2h_floor_log2_u32(tx_width > tx_height ? tx_width : tx_height);
    geometry->tx_context = (uint8_t)(((low_log2 - 2U) + (high_log2 - 2U) + 1U) >> 1U);
    return AV2H_OK;
}

Av2hStatus av2h_coeff_tx_geometry(Av2hTxSize tx_size, Av2hCoeffTxGeometry *geometry) {
    uint16_t tx_width;
    uint16_t tx_height;
    Av2hStatus status;
    if (geometry == NULL)
        return AV2H_INVALID_ARGUMENT;
    switch (tx_size) {
    case AV2H_TX_4X4:
        tx_width = 4U;
        tx_height = 4U;
        break;
    case AV2H_TX_8X8:
        tx_width = 8U;
        tx_height = 8U;
        break;
    case AV2H_TX_16X16:
        tx_width = 16U;
        tx_height = 16U;
        break;
    case AV2H_TX_32X32:
        tx_width = 32U;
        tx_height = 32U;
        break;
    case AV2H_TX_64X64:
        tx_width = 64U;
        tx_height = 64U;
        break;
    case AV2H_TX_4X8:
        tx_width = 4U;
        tx_height = 8U;
        break;
    case AV2H_TX_8X4:
        tx_width = 8U;
        tx_height = 4U;
        break;
    case AV2H_TX_8X16:
        tx_width = 8U;
        tx_height = 16U;
        break;
    case AV2H_TX_16X8:
        tx_width = 16U;
        tx_height = 8U;
        break;
    case AV2H_TX_16X32:
        tx_width = 16U;
        tx_height = 32U;
        break;
    case AV2H_TX_32X16:
        tx_width = 32U;
        tx_height = 16U;
        break;
    case AV2H_TX_64X32:
        tx_width = 64U;
        tx_height = 32U;
        break;
    case AV2H_TX_32X64:
        tx_width = 32U;
        tx_height = 64U;
        break;
    case AV2H_TX_4X16:
        tx_width = 4U;
        tx_height = 16U;
        break;
    case AV2H_TX_16X4:
        tx_width = 16U;
        tx_height = 4U;
        break;
    case AV2H_TX_8X32:
        tx_width = 8U;
        tx_height = 32U;
        break;
    case AV2H_TX_32X8:
        tx_width = 32U;
        tx_height = 8U;
        break;
    case AV2H_TX_16X64:
        tx_width = 16U;
        tx_height = 64U;
        break;
    case AV2H_TX_64X16:
        tx_width = 64U;
        tx_height = 16U;
        break;
    case AV2H_TX_4X32:
        tx_width = 4U;
        tx_height = 32U;
        break;
    case AV2H_TX_32X4:
        tx_width = 32U;
        tx_height = 4U;
        break;
    case AV2H_TX_8X64:
        tx_width = 8U;
        tx_height = 64U;
        break;
    case AV2H_TX_64X8:
        tx_width = 64U;
        tx_height = 8U;
        break;
    case AV2H_TX_4X64:
        tx_width = 4U;
        tx_height = 64U;
        break;
    case AV2H_TX_64X4:
        tx_width = 64U;
        tx_height = 4U;
        break;
    default:
        return AV2H_UNSUPPORTED;
    }
    status = av2h_coeff_tx_geometry_from_dims(tx_width, tx_height, geometry);
    if (status != AV2H_OK)
        return status;
    return status;
}