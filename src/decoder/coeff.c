#include "coeff.h"

Av2hStatus av2h_first_coeff_cdfs_init(Av2hFirstCoeffCdfs *cdfs) {
    static const uint16_t eob_pt_16[] = {
        1839U, 2995U, 6796U, 15126U
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
    static const uint16_t eob_pt_1024[] = {
        634U, 1777U, 3873U, 6293U, 12653U, 23825U, 24712U
    };
    Av2hStatus status;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs0, (Av2hBinaryCdfSpec){ 30296U, 5U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_chroma_txs0_ctx6,
        (Av2hBinaryCdfSpec){ 8778U, 31U, 0U });
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
        cdfs->txb_skip_txs1, (Av2hBinaryCdfSpec){ 31530U, 30U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs3, (Av2hBinaryCdfSpec){ 27230U, 62U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16, 5U + 4U, eob_pt_16,
                           5U, 30U, 0U);
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
    } else if (block_size == AV2H_BLOCK_8X4) {
        *tx_size = AV2H_TX_8X4;
        *tx_size_context = 1U;
    } else if (block_size == AV2H_BLOCK_8X8) {
        *tx_size = AV2H_TX_8X8;
        *tx_size_context = 1U;
    } else if (block_size == AV2H_BLOCK_32X32) {
        *tx_size = AV2H_TX_32X32;
        *tx_size_context = 3U;
    } else {
        return AV2H_UNSUPPORTED;
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
    } else if (tx_size == AV2H_TX_8X4) {
        *point_cdf = cdfs->eob_pt_32;
        *point_symbols = 6U;
        *capacity = 32U;
    } else if (tx_size == AV2H_TX_8X8) {
        *point_cdf = cdfs->eob_pt_64;
        *point_symbols = 7U;
        *capacity = 64U;
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
         (leaf->block_size != AV2H_BLOCK_4X4 || txb_skip_context != 9U) :
         txb_skip_context != 0U) ||
        frame->base_q_index != 128U || frame->tx_mode != 1U ||
        frame->disable_cdf_update || mode->use_intrabc) {
        return AV2H_UNSUPPORTED;
    }
    status = derive_tx_size(leaf->block_size, &prefix->tx_size,
                            &prefix->tx_size_context);
    if (status != AV2H_OK) return status;
    prefix->txb_skip_context = txb_skip_context;
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
        cdf = mode->fsc_mode ? cdfs->txb_skip_fsc_txs0_ctx9 :
            prefix->tx_size_context == 0U ? cdfs->txb_skip_txs0 :
          prefix->tx_size_context == 1U ? cdfs->txb_skip_txs1 :
                                          cdfs->txb_skip_txs3;
    value = av2h_range_read_adaptive(decoder, cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    if (value != 0U) return AV2H_UNSUPPORTED;
    return decode_eob(decoder, cdfs, 0, prefix);
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

Av2hStatus av2h_first_chroma_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    Av2hFirstCoeffPrefix *prefix) {
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || frame == 0 || leaf == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) ||
        leaf->block_size != AV2H_BLOCK_8X8 ||
        frame->base_q_index != 128U || frame->tx_mode != 1U ||
        frame->disable_cdf_update) {
        return AV2H_UNSUPPORTED;
    }
    prefix->tx_size = AV2H_TX_4X4;
    prefix->tx_size_context = 0U;
    prefix->txb_skip_context = plane == 1U ? 6U : 9U;
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
    value = av2h_range_read_adaptive(
        decoder, plane == 1U ? cdfs->txb_skip_chroma_txs0_ctx6 :
                              cdfs->txb_skip_chroma_v_ctx9, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    if (value != 0U) return AV2H_UNSUPPORTED;
    return decode_eob(decoder, cdfs, cdfs->eob_pt_16_chroma, prefix);
}