#include "transform.h"

Av2hStatus av2h_first_transform_cdfs_init(Av2hFirstTransformCdfs *cdfs) {
    static const uint16_t primary_size0[] = {
        5026U, 16816U, 19974U, 23404U, 26845U, 30499U
    };
    static const uint16_t primary_size1[] = {
        14910U, 25257U, 26964U, 29323U, 30237U, 31535U
    };
    static const uint16_t secondary_32x32[] = {
        2472U, 8725U, 13853U
    };
    static const uint16_t chroma_cross_type[] = {
        14350U, 14836U, 16054U, 17075U, 19408U, 28530U
    };
    Av2hStatus status;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_cdf_init(
        cdfs->primary_size0, AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U,
        primary_size0, AV2H_PRIMARY_TX_TYPE_SYMBOLS, 8U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->primary_size1, AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U,
        primary_size1, AV2H_PRIMARY_TX_TYPE_SYMBOLS, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->secondary_32x32, AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
        secondary_32x32, AV2H_SECONDARY_TX_TYPE_SYMBOLS, 76U, 0U);
    if (status != AV2H_OK) return status;
    return av2h_cdf_init(cdfs->chroma_cross_type, 7U + 4U,
                         chroma_cross_type, 7U, 62U, 0U);
}

static Av2hStatus read_primary(Av2hRangeDecoder *decoder,
                               Av2hFirstTransformCdfs *cdfs,
                               const Av2hFirstLeafMode *mode,
                               const Av2hFirstCoeffPrefix *coeff,
                               Av2hFirstTransformType *transform) {
    uint32_t symbol;
    transform->primary_signaled = 0U;
    transform->primary_symbol = 0U;
    transform->primary_type = AV2H_TX_DCT_DCT;
    if (coeff->tx_size == AV2H_TX_32X32) return AV2H_OK;
    if ((coeff->tx_size == AV2H_TX_8X4 &&
         (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D45)) ||
                (coeff->tx_size == AV2H_TX_4X4 &&
                 (mode->mode == AV2H_INTRA_D203 ||
                    mode->mode == AV2H_INTRA_PAETH ||
                    mode->mode == AV2H_INTRA_SMOOTH))) {
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->primary_size0, AV2H_PRIMARY_TX_TYPE_SYMBOLS);
        if (coeff->tx_size == AV2H_TX_8X4) {
            if (mode->mode == AV2H_INTRA_DC && symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_D45 && symbol == 5U) {
                transform->primary_type = AV2H_TX_ADST_FLIPADST;
            } else {
                return AV2H_UNSUPPORTED;
            }
        } else {
            if (mode->mode == AV2H_INTRA_PAETH && symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_D203 && symbol == 3U) {
                transform->primary_type = AV2H_TX_DCT_ADST;
            } else if (mode->mode == AV2H_INTRA_D203 && symbol == 6U) {
                transform->primary_type = AV2H_TX_H_ADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH && symbol == 3U) {
                transform->primary_type = AV2H_TX_DCT_ADST;
            } else {
                return AV2H_UNSUPPORTED;
            }
        }
    } else if (coeff->tx_size == AV2H_TX_8X8 &&
               (mode->mode == AV2H_INTRA_PAETH ||
                mode->mode == AV2H_INTRA_D67)) {
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->primary_size1, AV2H_PRIMARY_TX_TYPE_SYMBOLS);
        if (mode->mode == AV2H_INTRA_PAETH && symbol == 4U) {
            transform->primary_type = AV2H_TX_V_DCT;
        } else if (mode->mode == AV2H_INTRA_D67 && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else {
            return AV2H_UNSUPPORTED;
        }
    } else {
        return AV2H_UNSUPPORTED;
    }
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    transform->primary_signaled = 1U;
    transform->primary_symbol = (uint8_t)symbol;
    return AV2H_OK;
}

Av2hStatus av2h_first_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform) {
    uint32_t symbol;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        mode == 0 || coeff == 0 || transform == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (frame->reduced_tx_set || frame->base_q_index != 128U ||
        mode->use_intrabc || coeff->all_zero ||
        coeff->eob <= 1U) {
        return AV2H_UNSUPPORTED;
    }
    transform->before_primary_bits = av2h_range_tell(decoder);
    if (mode->fsc_mode) {
        if (coeff->tx_size != AV2H_TX_4X4 ||
            (coeff->eob != 15U && coeff->eob != 13U && coeff->eob != 8U)) {
            return AV2H_UNSUPPORTED;
        }
        transform->primary_type = AV2H_TX_IDTX;
        transform->primary_signaled = 0U;
        transform->primary_symbol = 0U;
        transform->after_primary_bits = transform->before_primary_bits;
        transform->secondary_signaled = 0U;
        transform->secondary_symbol = 0U;
        transform->after_secondary_bits = transform->before_primary_bits;
        return AV2H_OK;
    }
    status = read_primary(decoder, cdfs, mode, coeff, transform);
    if (status != AV2H_OK) return status;
    transform->after_primary_bits = av2h_range_tell(decoder);
    transform->secondary_signaled = 0U;
    transform->secondary_symbol = 0U;
    if (sequence->enable_ist && coeff->tx_size == AV2H_TX_32X32 &&
        transform->primary_type == AV2H_TX_DCT_DCT &&
        mode->mode < AV2H_INTRA_PAETH && coeff->eob <= 32U) {
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->secondary_32x32,
            AV2H_SECONDARY_TX_TYPE_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->secondary_signaled = 1U;
        transform->secondary_symbol = (uint8_t)symbol;
        if (symbol != 0U) return AV2H_UNSUPPORTED;
    }
    transform->after_secondary_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_first_chroma_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hFrameState *frame,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform) {
    Av2hFirstChromaMode mode = { 0 };
    mode.mode = AV2H_INTRA_DC;
    return av2h_chroma_transform_type_read(
        decoder, cdfs, frame, &mode, plane, coeff, transform);
}

Av2hStatus av2h_chroma_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hFirstChromaMode *mode,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform) {
    uint32_t symbol = 0U;
    if (decoder == 0 || cdfs == 0 || frame == 0 || mode == 0 || coeff == 0 ||
        transform == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) || frame->reduced_tx_set ||
        frame->base_q_index != 128U || coeff->tx_size != AV2H_TX_4X4 ||
        coeff->all_zero || coeff->eob == 0U || coeff->eob > 16U) {
        return AV2H_UNSUPPORTED;
    }
    transform->before_primary_bits = av2h_range_tell(decoder);
    transform->primary_type = mode->mode == AV2H_INTRA_D203 ?
        AV2H_TX_DCT_ADST : mode->mode == AV2H_INTRA_D67 ?
        AV2H_TX_ADST_DCT : AV2H_TX_DCT_DCT;
    transform->primary_signaled = 0U;
    transform->primary_symbol = 0U;
    transform->secondary_signaled = 0U;
    transform->secondary_symbol = 0U;
    if (plane == 1U && coeff->eob != 1U) {
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->chroma_cross_type, 7U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (symbol != 0U) return AV2H_UNSUPPORTED;
    }
    transform->after_primary_bits = av2h_range_tell(decoder);
    transform->after_secondary_bits = transform->after_primary_bits;
    return AV2H_OK;
}