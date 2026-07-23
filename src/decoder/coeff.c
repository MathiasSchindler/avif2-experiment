#include "coeff.h"

Av2hStatus av2h_first_coeff_cdfs_init(Av2hFirstCoeffCdfs *cdfs) {
    static const uint16_t eob_pt_16[] = {
        1839U, 2995U, 6796U, 15126U
    };
    static const uint16_t eob_pt_16_inter[] = {
        1354U, 1787U, 4813U, 9327U
    };
    static const uint16_t eob_pt_16_chroma[] = {
        8298U, 11869U, 17297U, 24569U
    };
    static const uint16_t eob_pt_32[] = {
        1143U, 1778U, 3854U, 8166U, 13337U
    };
    static const uint16_t eob_pt_64[] = {
        939U, 1381U, 3142U, 6536U, 13617U, 21799U
    };
    static const uint16_t eob_pt_128[] = {
        654U, 1068U, 2257U, 5580U, 12362U, 21455U, 26067U
    };
    static const uint16_t eob_pt_256[] = {
        790U, 1184U, 2218U, 5022U, 11432U, 20289U, 23689U
    };
    static const uint16_t eob_pt_512[] = {
        1311U, 2019U, 3578U, 6876U, 13781U, 23541U, 24922U
    };
    static const uint16_t eob_pt_1024[] = {
        634U, 1777U, 3873U, 6293U, 12653U, 23825U, 24712U
    };
    Av2hStatus status;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs0, (Av2hBinaryCdfSpec){ 30296U, 5U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_inter_txs0,
        (Av2hBinaryCdfSpec){ 32764U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_txs0_ctx6,
        (Av2hBinaryCdfSpec){ 8778U, 31U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_txs0_ctx7,
        (Av2hBinaryCdfSpec){ 17506U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_txs0_ctx8,
        (Av2hBinaryCdfSpec){ 26467U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_txs1_ctx7,
        (Av2hBinaryCdfSpec){ 16054U, 31U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_v_ctx1,
        (Av2hBinaryCdfSpec){ 7768U, 75U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_v_ctx7,
        (Av2hBinaryCdfSpec){ 16919U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_v_ctx8,
        (Av2hBinaryCdfSpec){ 25248U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_v_ctx9,
        (Av2hBinaryCdfSpec){ 8448U, 50U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs0_ctx9,
        (Av2hBinaryCdfSpec){ 32128U, 78U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs2_ctx9,
        (Av2hBinaryCdfSpec){ 29599U, 7U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs1, (Av2hBinaryCdfSpec){ 31530U, 30U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs2, (Av2hBinaryCdfSpec){ 31362U, 25U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs3, (Av2hBinaryCdfSpec){ 27230U, 62U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16, 5U + 4U, eob_pt_16,
                           5U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16_inter, 5U + 4U,
                           eob_pt_16_inter, 5U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16_chroma, 5U + 4U,
                           eob_pt_16_chroma, 5U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_32, 6U + 4U, eob_pt_32,
                           6U, 31U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_64, 7U + 4U, eob_pt_64,
                           7U, 31U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_128, 8U + 4U, eob_pt_128,
                           8U, 32U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_256, 8U + 4U, eob_pt_256,
                           8U, 57U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_512, 8U + 4U, eob_pt_512,
                           8U, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_1024, 8U + 4U, eob_pt_1024,
                           8U, 57U, 0U);
    if (status != AV2H_OK) return status;
    return av2h_binary_cdf_init(
        cdfs->eob_extra, (Av2hBinaryCdfSpec){ 18326U, 0U, 0U });
}

static Av2hStatus derive_tx_size(Av2hBlockSize block_size,
                                 Av2hFirstTxSize *tx_size,
                                 uint8_t *tx_size_context) {
    if (block_size == AV2H_BLOCK_4X4) {
        *tx_size = AV2H_TX_4X4;
        *tx_size_context = 0U;
    } else if (block_size == AV2H_BLOCK_4X8) {
        *tx_size = AV2H_TX_4X8;
        *tx_size_context = 1U;
    } else if (block_size == AV2H_BLOCK_8X4) {
        *tx_size = AV2H_TX_8X4;
        *tx_size_context = 1U;
    } else if (block_size == AV2H_BLOCK_8X8) {
        *tx_size = AV2H_TX_8X8;
        *tx_size_context = 1U;
    } else if (block_size == AV2H_BLOCK_8X16) {
        *tx_size = AV2H_TX_8X16;
        *tx_size_context = 2U;
    } else if (block_size == AV2H_BLOCK_16X8) {
        *tx_size = AV2H_TX_16X8;
        *tx_size_context = 2U;
    } else if (block_size == AV2H_BLOCK_16X4) {
        *tx_size = AV2H_TX_16X4;
        *tx_size_context = 1U;
    } else if (block_size == AV2H_BLOCK_8X32) {
        *tx_size = AV2H_TX_8X32;
        *tx_size_context = 2U;
    } else if (block_size == AV2H_BLOCK_32X8) {
        *tx_size = AV2H_TX_32X8;
        *tx_size_context = 2U;
    } else if (block_size == AV2H_BLOCK_16X32) {
        *tx_size = AV2H_TX_16X32;
        *tx_size_context = 3U;
    } else if (block_size == AV2H_BLOCK_32X32) {
        *tx_size = AV2H_TX_32X32;
        *tx_size_context = 3U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

static uint8_t *coefficient_plane(uint8_t *contexts, size_t count,
                                  uint8_t plane) {
    return contexts + (size_t)plane * count;
}

Av2hStatus av2h_coeff_context_state_init(
    Av2hCoeffContextState *state,
    uint8_t *above,
    size_t above_count,
    uint8_t *left,
    size_t left_count) {
    size_t index;
    if (state == 0 || above == 0 || left == 0 || above_count == 0U ||
        left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    state->above = above;
    state->left = left;
    state->above_count = above_count;
    state->left_count = left_count;
    for (index = 0U; index < AV2H_FRAME_PLANE_COUNT * above_count; ++index) {
        above[index] = 0U;
    }
    for (index = 0U; index < AV2H_FRAME_PLANE_COUNT * left_count; ++index) {
        left[index] = 0U;
    }
    return AV2H_OK;
}

Av2hStatus av2h_coeff_contexts_from_state(
    const Av2hCoeffContextState *state,
    uint8_t plane,
    const Av2hPartitionNode *leaf,
    uint8_t fsc_mode,
    uint8_t *txb_skip_context,
    uint8_t *dc_sign_context) {
    const uint8_t *above;
    const uint8_t *left;
    const uint8_t *u_above;
    const uint8_t *u_left;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    int dc_sign = 0;
    unsigned int above_nonzero = 0U;
    unsigned int left_nonzero = 0U;
    Av2hStatus status;
    if (state == 0 || state->above == 0 || state->left == 0 ||
        state->above_count == 0U || state->left_count == 0U || leaf == 0 ||
        plane >= AV2H_FRAME_PLANE_COUNT || fsc_mode > 1U ||
        txb_skip_context == 0 || dc_sign_context == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_block_dimensions_mi(leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    left_index = leaf->row % state->left_count;
    if ((size_t)leaf->column + width > state->above_count ||
        left_index + height > state->left_count) {
        return AV2H_OVERFLOW;
    }
    above = coefficient_plane(state->above, state->above_count, plane);
    left = coefficient_plane(state->left, state->left_count, plane);
    u_above = coefficient_plane(state->above, state->above_count, 1U);
    u_left = coefficient_plane(state->left, state->left_count, 1U);
    for (index = 0U; index < width; ++index) {
        uint8_t context = above[leaf->column + index];
        above_nonzero |= context & 7U;
        dc_sign += (context & 16U) != 0U ? 1 :
                   (context & 8U) != 0U ? -1 : 0;
    }
    for (index = 0U; index < height; ++index) {
        uint8_t context = left[left_index + index];
        left_nonzero |= context & 7U;
        dc_sign += (context & 16U) != 0U ? 1 :
                   (context & 8U) != 0U ? -1 : 0;
    }
    if (plane == 0U) {
        *txb_skip_context = fsc_mode ? 9U : 0U;
    } else if (plane == 1U) {
        *txb_skip_context = (uint8_t)(6U + (above_nonzero != 0U) +
                                     (left_nonzero != 0U));
    } else {
        unsigned int u_nonzero =
            (u_above[leaf->column] & 7U) | (u_left[left_index] & 7U);
        unsigned int neighbor_count =
            (above_nonzero != 0U) + (left_nonzero != 0U);
        *txb_skip_context = (uint8_t)(u_nonzero != 0U ?
            (neighbor_count == 0U ? 9U : 6U + neighbor_count) :
            neighbor_count);
    }
    *dc_sign_context = dc_sign < 0 ? 1U : dc_sign > 0 ? 2U : 0U;
    return AV2H_OK;
}

Av2hStatus av2h_coeff_context_state_update(
    Av2hCoeffContextState *state,
    uint8_t plane,
    const Av2hPartitionNode *leaf,
    uint8_t cul_level) {
    uint8_t *above;
    uint8_t *left;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    Av2hStatus status;
    if (state == 0 || state->above == 0 || state->left == 0 ||
        state->above_count == 0U || state->left_count == 0U || leaf == 0 ||
        plane >= AV2H_FRAME_PLANE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_block_dimensions_mi(leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    left_index = leaf->row % state->left_count;
    if ((size_t)leaf->column + width > state->above_count ||
        left_index + height > state->left_count) {
        return AV2H_OVERFLOW;
    }
    above = coefficient_plane(state->above, state->above_count, plane);
    left = coefficient_plane(state->left, state->left_count, plane);
    for (index = 0U; index < width; ++index) {
        above[leaf->column + index] = cul_level;
    }
    for (index = 0U; index < height; ++index) {
        left[left_index + index] = cul_level;
    }
    return AV2H_OK;
}

static Av2hStatus eob_parameters(
    Av2hFirstCoeffCdfs *cdfs,
    Av2hFirstTxSize tx_size,
    uint16_t **point_cdf,
    size_t *point_symbols,
    uint16_t *capacity) {
    if (tx_size == AV2H_TX_4X4) {
        *point_cdf = cdfs->eob_pt_16;
        *point_symbols = 5U;
        *capacity = 16U;
    } else if (tx_size == AV2H_TX_4X8 || tx_size == AV2H_TX_8X4) {
        *point_cdf = cdfs->eob_pt_32;
        *point_symbols = 6U;
        *capacity = 32U;
    } else if (tx_size == AV2H_TX_8X8 || tx_size == AV2H_TX_16X4) {
        *point_cdf = cdfs->eob_pt_64;
        *point_symbols = 7U;
        *capacity = 64U;
    } else if (tx_size == AV2H_TX_8X16 || tx_size == AV2H_TX_16X8) {
        *point_cdf = cdfs->eob_pt_128;
        *point_symbols = 8U;
        *capacity = 128U;
    } else if (tx_size == AV2H_TX_8X32 || tx_size == AV2H_TX_32X8) {
        *point_cdf = cdfs->eob_pt_256;
        *point_symbols = 8U;
        *capacity = 256U;
    } else if (tx_size == AV2H_TX_16X32) {
        *point_cdf = cdfs->eob_pt_512;
        *point_symbols = 8U;
        *capacity = 512U;
    } else if (tx_size == AV2H_TX_32X32) {
        *point_cdf = cdfs->eob_pt_1024;
        *point_symbols = 8U;
        *capacity = 1024U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

static Av2hStatus decode_eob(Av2hRangeDecoder *decoder,
                             Av2hFirstCoeffCdfs *cdfs,
                             uint16_t *eob_pt_16_override,
                             Av2hFirstCoeffPrefix *prefix) {
    static const uint16_t group_start[] = {
        0U, 1U, 2U, 3U, 5U, 9U, 17U, 33U, 65U, 129U, 257U, 513U
    };
    static const uint8_t offset_bits[] = {
        0U, 0U, 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U
    };
    uint16_t *point_cdf;
    uint16_t capacity;
    size_t point_symbols;
    uint32_t value;
    Av2hStatus status = eob_parameters(cdfs, prefix->tx_size, &point_cdf,
                                       &point_symbols, &capacity);
    if (status != AV2H_OK) return status;
    if (prefix->tx_size == AV2H_TX_4X4 && eob_pt_16_override != 0) {
        point_cdf = eob_pt_16_override;
    }
    value = av2h_range_read_adaptive(decoder, point_cdf, point_symbols);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (prefix->tx_size == AV2H_TX_8X32 && value == 7U) {
        value += av2h_range_read_bypass(decoder, 1U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    if (prefix->tx_size == AV2H_TX_32X32 && value == 7U) {
        return AV2H_UNSUPPORTED;
    }
    prefix->eob_point = (uint8_t)(value + 1U);
    prefix->after_eob_point_bits = av2h_range_tell(decoder);
    if (prefix->eob_point >= sizeof(offset_bits) / sizeof(offset_bits[0])) {
        return AV2H_UNSUPPORTED;
    }
    prefix->eob_offset_bits = offset_bits[prefix->eob_point];
    prefix->eob_extra = 0U;
    prefix->after_eob_extra_bit = prefix->after_eob_point_bits;
    if (prefix->eob_offset_bits != 0U) {
        value = av2h_range_read_adaptive(decoder, cdfs->eob_extra, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        prefix->eob_extra =
            (uint16_t)(value << (prefix->eob_offset_bits - 1U));
        prefix->after_eob_extra_bit = av2h_range_tell(decoder);
        if (prefix->eob_offset_bits > 1U) {
            value = av2h_range_read_bypass(
                decoder, (unsigned int)(prefix->eob_offset_bits - 1U));
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            prefix->eob_extra = (uint16_t)(prefix->eob_extra + value);
        }
    }
    prefix->eob =
        (uint16_t)(group_start[prefix->eob_point] + prefix->eob_extra);
    prefix->after_eob_bits = av2h_range_tell(decoder);
    if (prefix->eob == 0U || prefix->eob > capacity) {
        return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

Av2hStatus av2h_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    uint8_t txb_skip_context,
    Av2hFirstCoeffPrefix *prefix) {
    uint16_t *cdf;
    uint32_t value;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || frame == 0 || leaf == 0 ||
        mode == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((mode->fsc_mode ?
                 ((leaf->block_size != AV2H_BLOCK_4X4 &&
                     leaf->block_size != AV2H_BLOCK_8X16) ||
                    txb_skip_context != 9U) :
         txb_skip_context != 0U) ||
        frame->base_q_index != 128U || frame->tx_mode != 1U ||
        frame->disable_cdf_update) {
        return AV2H_UNSUPPORTED;
    }
    status = derive_tx_size(leaf->block_size, &prefix->tx_size,
                            &prefix->tx_size_context);
    if (status != AV2H_OK) return status;
    prefix->txb_skip_context = txb_skip_context;
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
        cdf = mode->use_intrabc ? cdfs->txb_skip_inter_txs0 :
            mode->fsc_mode && prefix->tx_size_context == 0U ?
                cdfs->txb_skip_fsc_txs0_ctx9 :
            mode->fsc_mode && prefix->tx_size_context == 2U ?
                cdfs->txb_skip_fsc_txs2_ctx9 :
            prefix->tx_size_context == 0U ? cdfs->txb_skip_txs0 :
          prefix->tx_size_context == 1U ? cdfs->txb_skip_txs1 :
                    prefix->tx_size_context == 2U ? cdfs->txb_skip_txs2 :
                                                                                    cdfs->txb_skip_txs3;
    value = av2h_range_read_adaptive(decoder, cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    if (value != 0U) return AV2H_UNSUPPORTED;
    return decode_eob(decoder, cdfs,
                      mode->use_intrabc ? cdfs->eob_pt_16_inter : 0,
                      prefix);
}

Av2hStatus av2h_first_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    Av2hFirstCoeffPrefix *prefix) {
    if (leaf == 0) return AV2H_INVALID_ARGUMENT;
    if (leaf->row != 0U || leaf->column != 0U) return AV2H_UNSUPPORTED;
    return av2h_coeff_prefix_read(decoder, cdfs, frame, leaf, mode, 0U,
                                  prefix);
}

Av2hStatus av2h_coeff_prefix_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hCoeffContextState *state,
    Av2hFirstCoeffPrefix *prefix,
    uint8_t *dc_sign_context) {
    uint8_t txb_skip_context;
    Av2hStatus status = av2h_coeff_contexts_from_state(
        state, 0U, leaf, mode != 0 ? mode->fsc_mode : 0U,
        &txb_skip_context, dc_sign_context);
    if (status != AV2H_OK) return status;
    return av2h_coeff_prefix_read(decoder, cdfs, frame, leaf, mode,
                                  txb_skip_context, prefix);
}

Av2hStatus av2h_chroma_coeff_prefix_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    const Av2hCoeffContextState *state,
    Av2hFirstCoeffPrefix *prefix) {
    uint8_t txb_skip_context;
    uint8_t dc_sign_context;
    Av2hStatus status = av2h_coeff_contexts_from_state(
        state, plane, leaf, 0U, &txb_skip_context, &dc_sign_context);
    if (status != AV2H_OK) return status;
    return av2h_chroma_coeff_prefix_read(
        decoder, cdfs, frame, leaf, plane, txb_skip_context, prefix);
}

Av2hStatus av2h_first_chroma_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    Av2hFirstCoeffPrefix *prefix) {
    return av2h_chroma_coeff_prefix_read(
        decoder, cdfs, frame, leaf, plane, plane == 1U ? 6U : 9U, prefix);
}

Av2hStatus av2h_chroma_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    uint8_t txb_skip_context,
    Av2hFirstCoeffPrefix *prefix) {
    uint16_t *skip_cdf;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || frame == 0 || leaf == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) ||
        (leaf->block_size != AV2H_BLOCK_8X8 &&
         leaf->block_size != AV2H_BLOCK_8X16) ||
        frame->base_q_index != 128U || frame->tx_mode != 1U ||
        frame->disable_cdf_update) {
        return AV2H_UNSUPPORTED;
    }
    prefix->tx_size = leaf->block_size == AV2H_BLOCK_8X16 ?
        AV2H_TX_4X8 : AV2H_TX_4X4;
    prefix->tx_size_context = leaf->block_size == AV2H_BLOCK_8X16 ? 1U : 0U;
    prefix->txb_skip_context = txb_skip_context;
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
    if (prefix->tx_size == AV2H_TX_4X8 && plane == 1U &&
        txb_skip_context == 7U) {
        skip_cdf = cdfs->txb_skip_chroma_txs1_ctx7;
    } else if (prefix->tx_size == AV2H_TX_4X8 && plane == 2U &&
               txb_skip_context == 1U) {
        skip_cdf = cdfs->txb_skip_chroma_v_ctx1;
    } else if (prefix->tx_size != AV2H_TX_4X4) {
        return AV2H_UNSUPPORTED;
    } else if (plane == 1U && txb_skip_context >= 6U &&
               txb_skip_context <= 8U) {
        skip_cdf = txb_skip_context == 6U ?
            cdfs->txb_skip_chroma_txs0_ctx6 : txb_skip_context == 7U ?
            cdfs->txb_skip_chroma_txs0_ctx7 :
            cdfs->txb_skip_chroma_txs0_ctx8;
    } else if (plane == 2U && txb_skip_context >= 7U &&
               txb_skip_context <= 9U) {
        skip_cdf = txb_skip_context == 7U ? cdfs->txb_skip_chroma_v_ctx7 :
            txb_skip_context == 8U ? cdfs->txb_skip_chroma_v_ctx8 :
                                    cdfs->txb_skip_chroma_v_ctx9;
    } else {
        return AV2H_UNSUPPORTED;
    }
    value = av2h_range_read_adaptive(decoder, skip_cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    if (value != 0U) {
        prefix->eob_point = 0U;
        prefix->eob_offset_bits = 0U;
        prefix->eob_extra = 0U;
        prefix->eob = 0U;
        prefix->after_eob_point_bits = prefix->after_all_zero_bits;
        prefix->after_eob_extra_bit = prefix->after_all_zero_bits;
        prefix->after_eob_bits = prefix->after_all_zero_bits;
        return AV2H_OK;
    }
    return decode_eob(decoder, cdfs, cdfs->eob_pt_16_chroma, prefix);
}