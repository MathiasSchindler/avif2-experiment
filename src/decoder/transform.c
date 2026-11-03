#include "transform.h"

#include "internal/decode_diagnostics.h"
#include "internal/transform_modules.h"
#include "tx_geometry.h"

#include <stddef.h>

const Av2hTransformDiagnostics *av2h_transform_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return diagnostics != NULL ? &diagnostics->transform : NULL;
#else
    (void)diagnostics;
    return NULL;
#endif
}

void av2h_transform_diagnostics_set_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) diagnostics->transform.transform_stage = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}
Av2hStatus av2h_secondary_tx_type_signal(
    const Av2hSequenceState *sequence,
    const Av2hLeafMode *mode,
    const Av2hCoeffPrefix *coeff,
    Av2hTxType primary_type,
    uint8_t *signaled) {
    const Av2hTxDescriptor *descriptor;
    uint8_t size_class;
    uint16_t eob_limit;
    Av2hStatus status;
    if (sequence == NULL || mode == NULL || coeff == NULL || signaled == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    *signaled = 0U;
    status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    if (mode->use_intrabc) {
        if (!sequence->enable_inter_ist || coeff->eob <= 3U ||
            primary_type != AV2H_TX_DCT_DCT ||
            descriptor->output_width < 16U ||
            descriptor->output_height < 16U) {
            return AV2H_OK;
        }
    } else if (!sequence->enable_ist || coeff->eob <= 1U ||
               mode->mode >= AV2H_INTRA_PAETH ||
               (primary_type != AV2H_TX_DCT_DCT &&
                primary_type != AV2H_TX_ADST_ADST)) {
        return AV2H_OK;
    }
    if (descriptor->output_width == 8U &&
        descriptor->output_height == 8U &&
        primary_type == AV2H_TX_DCT_DCT) {
        size_class = 1U;
    } else if (descriptor->output_width >= 8U &&
               descriptor->output_height >= 8U) {
        size_class = primary_type == AV2H_TX_DCT_DCT ? 2U : 3U;
    } else {
        size_class = 0U;
    }
    eob_limit = size_class == 0U ? 8U :
        size_class == 1U || size_class == 3U ? 20U : 32U;
    *signaled = (uint8_t)(coeff->eob <= eob_limit);
    return AV2H_OK;
}

static Av2hStatus read_primary(Av2hRangeDecoder *decoder,
                               Av2hTransformCdfs *cdfs,
                               const Av2hLeafMode *mode,
                               const Av2hCoeffPrefix *coeff,
                               Av2hTransformType *transform) {
    const Av2hTxDescriptor *descriptor;
    Av2hLeafMode mapped_mode;
    uint32_t symbol;
    Av2hStatus status;
    transform->primary_signaled = 0U;
    transform->primary_symbol = 0U;
    transform->primary_long_side_symbol = 0U;
    transform->primary_type = AV2H_TX_DCT_DCT;
    av2h_transform_diagnostics_set_stage(decoder->diagnostics, 2U);
    status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    mapped_mode = *mode;
    mapped_mode.mode = av2h_transform_mode(
        mode->mode, mode->angle_delta, mode->mrl_index, descriptor);
    mode = &mapped_mode;
    if (descriptor->signaling_class == AV2H_TX_SIGNAL_DCT_ONLY &&
        !(mode->use_intrabc && coeff->tx_size == AV2H_TX_32X32)) {
        return av2h_primary_tx_type_map(
            coeff->tx_size, mode->mode, 0U, 0U, &transform->primary_type);
    }
    if (coeff->tx_size == AV2H_TX_8X8 &&
        mode->mode == AV2H_INTRA_D203) {
        static const Av2hTxType d203_size1[] = {
            AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
            AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
            AV2H_TX_FLIPADST_DCT, AV2H_TX_FLIPADST_ADST,
            AV2H_TX_ADST_FLIPADST
        };
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->primary_size1, AV2H_PRIMARY_TX_TYPE_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->primary_signaled = 1U;
        transform->primary_symbol = (uint8_t)symbol;
        transform->primary_type = d203_size1[symbol];
        return AV2H_OK;
    }
    if (coeff->tx_size == AV2H_TX_16X4 &&
        mode->mode == AV2H_INTRA_D203) {
        static const Av2hTxType d203_size1[] = {
            AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
            AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
            AV2H_TX_FLIPADST_DCT, AV2H_TX_FLIPADST_ADST,
            AV2H_TX_ADST_FLIPADST
        };
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->primary_size0, AV2H_PRIMARY_TX_TYPE_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->primary_signaled = 1U;
        transform->primary_symbol = (uint8_t)symbol;
        transform->primary_type = d203_size1[symbol];
        return AV2H_OK;
    }
    if (coeff->tx_size == AV2H_TX_64X16 ||
        coeff->tx_size == AV2H_TX_16X64 ||
        coeff->tx_size == AV2H_TX_8X64 ||
        coeff->tx_size == AV2H_TX_64X8 ||
        coeff->tx_size == AV2H_TX_4X64 ||
        coeff->tx_size == AV2H_TX_64X4) {
        uint16_t *short_side_cdf;
        if (mode->use_intrabc) {
            uint16_t last = (uint16_t)(coeff->eob - 1U);
            uint16_t diagonal =
                (uint16_t)(last / descriptor->coefficient_width +
                           last % descriptor->coefficient_width);
            uint16_t max_diagonal =
                (uint16_t)(descriptor->coefficient_width +
                           descriptor->coefficient_height - 2U);
            size_t eob_context = diagonal < 2U ? 1U :
                diagonal > max_diagonal - 2U ? 2U : 0U;
            size_t square_context =
                descriptor->square_class == AV2H_TX_SQUARE_4 ? 0U :
                descriptor->square_class == AV2H_TX_SQUARE_8 ? 1U : 2U;
            short_side_cdf =
                cdfs->intrabc_short_side[eob_context][square_context];
        } else {
            short_side_cdf =
                (coeff->tx_size == AV2H_TX_4X64 ||
                 coeff->tx_size == AV2H_TX_64X4) ?
                    cdfs->primary_short_side_4 :
                (coeff->tx_size == AV2H_TX_8X64 ||
                 coeff->tx_size == AV2H_TX_64X8) ?
                    cdfs->primary_short_side_8 :
                    cdfs->primary_short_side_16;
        }
        symbol = av2h_range_read_adaptive(
            decoder, short_side_cdf, 4U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->primary_signaled = 1U;
        transform->primary_long_side_symbol = 1U;
        transform->primary_symbol = (uint8_t)symbol;
        return av2h_primary_tx_type_map(
            coeff->tx_size, mode->mode, 1U, (uint8_t)symbol,
            &transform->primary_type);
    }
    if (coeff->tx_size == AV2H_TX_4X32 ||
        coeff->tx_size == AV2H_TX_32X4 ||
        coeff->tx_size == AV2H_TX_8X32 ||
        coeff->tx_size == AV2H_TX_16X32 ||
        coeff->tx_size == AV2H_TX_32X16 ||
        coeff->tx_size == AV2H_TX_32X8) {
        uint16_t *short_side_cdf;
        uint32_t long_side = av2h_range_read_adaptive(
            decoder, mode->use_intrabc ? cdfs->intrabc_long_side_32 :
                                         cdfs->primary_long_side_32, 2U);
        if (mode->use_intrabc) {
            uint16_t last = (uint16_t)(coeff->eob - 1U);
            uint16_t diagonal =
                (uint16_t)(last / descriptor->coefficient_width +
                           last % descriptor->coefficient_width);
            uint16_t max_diagonal =
                (uint16_t)(descriptor->coefficient_width +
                           descriptor->coefficient_height - 2U);
            size_t eob_context = diagonal < 2U ? 1U :
                diagonal > max_diagonal - 2U ? 2U : 0U;
            size_t square_context =
                descriptor->square_class == AV2H_TX_SQUARE_4 ? 0U :
                descriptor->square_class == AV2H_TX_SQUARE_8 ? 1U : 2U;
            short_side_cdf =
                cdfs->intrabc_short_side[eob_context][square_context];
        } else {
            short_side_cdf =
                (coeff->tx_size == AV2H_TX_4X32 ||
                 coeff->tx_size == AV2H_TX_32X4) ?
                    cdfs->primary_short_side_4 :
                (coeff->tx_size == AV2H_TX_8X32 ||
                 coeff->tx_size == AV2H_TX_32X8) ?
                    cdfs->primary_short_side_8 :
                    cdfs->primary_short_side_16;
        }
        symbol = av2h_range_read_adaptive(
            decoder, short_side_cdf, 4U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->primary_signaled = 1U;
        transform->primary_long_side_symbol = (uint8_t)long_side;
        transform->primary_symbol = (uint8_t)symbol;
        return av2h_primary_tx_type_map(
            coeff->tx_size,
            mode->use_intrabc ? AV2H_INTRA_DC : mode->mode,
            (uint8_t)long_side,
            (uint8_t)symbol, &transform->primary_type);
    }
            av2h_transform_diagnostics_set_stage(decoder->diagnostics, 3U);
            if (coeff->tx_size == AV2H_TX_32X32 && mode->use_intrabc) {
        static const Av2hTxType intrabc_32x32_types[] = {
            AV2H_TX_IDTX, AV2H_TX_DCT_DCT
        };
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->intrabc_32x32_type, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->primary_signaled = 1U;
        transform->primary_symbol = (uint8_t)symbol;
        transform->primary_type = intrabc_32x32_types[symbol];
        return AV2H_OK;
    }
            if ((coeff->tx_size == AV2H_TX_4X4 ||
                coeff->tx_size == AV2H_TX_4X8 ||
                coeff->tx_size == AV2H_TX_8X4 ||
                coeff->tx_size == AV2H_TX_8X8 ||
                coeff->tx_size == AV2H_TX_8X16 ||
                coeff->tx_size == AV2H_TX_16X8 ||
                coeff->tx_size == AV2H_TX_4X16 ||
                coeff->tx_size == AV2H_TX_16X4 ||
                coeff->tx_size == AV2H_TX_16X16) && mode->use_intrabc) {
        uint16_t last = (uint16_t)(coeff->eob - 1U);
        uint16_t diagonal =
            (uint16_t)(last / descriptor->coefficient_width +
                       last % descriptor->coefficient_width);
        uint16_t max_diagonal =
            (uint16_t)(descriptor->coefficient_width +
                       descriptor->coefficient_height - 2U);
        uint8_t eob_context =
            diagonal < 2U ? 1U :
            diagonal > max_diagonal - 2U ? 2U : 0U;
        size_t square_context = descriptor->square_class == AV2H_TX_SQUARE_4 ?
            0U : descriptor->square_class == AV2H_TX_SQUARE_8 ? 1U : 2U;
        uint16_t *set_cdf;
        uint16_t *type_cdf;
        uint8_t signaling_set2 = square_context == 2U;
        static const Av2hTxType intrabc_types[] = {
            AV2H_TX_IDTX,
            AV2H_TX_V_DCT,
            AV2H_TX_H_DCT,
            AV2H_TX_V_ADST,
            AV2H_TX_H_ADST,
            AV2H_TX_V_FLIPADST,
            AV2H_TX_H_FLIPADST,
            AV2H_TX_DCT_DCT,
            AV2H_TX_ADST_DCT,
            AV2H_TX_DCT_ADST,
            AV2H_TX_FLIPADST_DCT,
            AV2H_TX_DCT_FLIPADST,
            AV2H_TX_ADST_ADST,
            AV2H_TX_FLIPADST_FLIPADST,
            AV2H_TX_ADST_FLIPADST,
            AV2H_TX_FLIPADST_ADST
        };
        set_cdf = signaling_set2 ?
            cdfs->intrabc_type_set2[eob_context] :
            cdfs->intrabc_type_set[eob_context][square_context];
        uint32_t set = av2h_range_read_adaptive(
            decoder, set_cdf, 2U);
        type_cdf = set == 0U ?
            (signaling_set2 ? cdfs->intrabc_type_index2[eob_context] :
                              cdfs->intrabc_type_index[eob_context]) :
            (signaling_set2 ? cdfs->intrabc_type_offset2[eob_context] :
                              cdfs->intrabc_type_offset[eob_context]);
        symbol = av2h_range_read_adaptive(
            decoder, type_cdf,
            signaling_set2 && set != 0U ? 4U :
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS);
        av2h_transform_diagnostics_set_stage(decoder->diagnostics, 32U);
        transform->primary_long_side_symbol = (uint8_t)set;
        transform->primary_symbol = (uint8_t)symbol;
        if (signaling_set2) {
            static const Av2hTxType set2_types[12U] = {
                AV2H_TX_IDTX, AV2H_TX_V_DCT, AV2H_TX_H_DCT,
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_DCT, AV2H_TX_DCT_FLIPADST,
                AV2H_TX_ADST_ADST, AV2H_TX_FLIPADST_FLIPADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = set2_types[set * 8U + symbol];
        } else if (symbol < AV2H_INTER_TX_TYPE_INDEX_SYMBOLS) {
            transform->primary_type = intrabc_types[set * 8U + symbol];
        } else {
            av2h_transform_diagnostics_set_stage(decoder->diagnostics, 33U);
            return AV2H_UNSUPPORTED;
        }
         } else if ((coeff->tx_size == AV2H_TX_4X4 ||
                     coeff->tx_size == AV2H_TX_4X8 ||
                     coeff->tx_size == AV2H_TX_8X4 ||
                     coeff->tx_size == AV2H_TX_4X16 ||
                     coeff->tx_size == AV2H_TX_16X4) &&
                    (uint8_t)mode->mode <= (uint8_t)AV2H_INTRA_PAETH) {
        symbol = av2h_range_read_adaptive(
            decoder, cdfs->primary_size0, AV2H_PRIMARY_TX_TYPE_SYMBOLS);
        transform->primary_symbol = (uint8_t)symbol;
        status = av2h_primary_tx_type_map(
            coeff->tx_size, mode->mode, 0U, (uint8_t)symbol,
            &transform->primary_type);
        if (status == AV2H_OK) {
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_DC) {
            static const Av2hTxType dc_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = dc_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_D67) {
            static const Av2hTxType d67_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_FLIPADST_FLIPADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d67_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_D45) {
            static const Av2hTxType d45_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_PAETH) {
            static const Av2hTxType paeth_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_V_DCT, AV2H_TX_H_DCT, AV2H_TX_H_ADST
            };
            transform->primary_type = paeth_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_D135) {
            static const Av2hTxType d135_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d135_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL) {
            static const Av2hTxType horizontal_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_ADST, AV2H_TX_H_DCT,
                AV2H_TX_H_ADST
            };
            transform->primary_type = horizontal_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
            static const Av2hTxType smooth_horizontal_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = smooth_horizontal_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16) {
            static const Av2hTxType d203_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_DCT, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d203_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8 &&
                   mode->mode == AV2H_INTRA_VERTICAL) {
            static const Av2hTxType vertical_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_V_DCT,
                AV2H_TX_V_ADST
            };
            transform->primary_type = vertical_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL) {
            static const Av2hTxType horizontal_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_ADST, AV2H_TX_H_DCT,
                AV2H_TX_H_ADST
            };
            transform->primary_type = horizontal_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8 &&
                   mode->mode == AV2H_INTRA_D45) {
            static const Av2hTxType d45_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8) {
            static const Av2hTxType dc_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = dc_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X4 &&
                   mode->mode == AV2H_INTRA_D135) {
            static const Av2hTxType d135_8x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = d135_8x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X4) {
            if ((mode->mode == AV2H_INTRA_DC ||
                 mode->mode == AV2H_INTRA_D203) && symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_DC && symbol == 2U) {
                transform->primary_type = AV2H_TX_ADST_DCT;
            } else if (mode->mode == AV2H_INTRA_D67 && symbol == 1U) {
                transform->primary_type = AV2H_TX_ADST_ADST;
            } else if (mode->mode == AV2H_INTRA_VERTICAL && symbol == 1U) {
                transform->primary_type = AV2H_TX_ADST_ADST;
            } else if (mode->mode == AV2H_INTRA_D157 && symbol == 1U) {
                transform->primary_type = AV2H_TX_ADST_ADST;
            } else if (mode->mode == AV2H_INTRA_D157 && symbol == 3U) {
                transform->primary_type = AV2H_TX_DCT_ADST;
            } else if (mode->mode == AV2H_INTRA_D45 && symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_D45 && symbol == 5U) {
                transform->primary_type = AV2H_TX_ADST_FLIPADST;
            } else if (mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 2U) {
                transform->primary_type = AV2H_TX_ADST_DCT;
            } else if (mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 6U) {
                transform->primary_type = AV2H_TX_H_ADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                       symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                       symbol == 3U) {
                transform->primary_type = AV2H_TX_DCT_ADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                       symbol == 6U) {
                transform->primary_type = AV2H_TX_H_ADST;
            } else {
                return AV2H_UNSUPPORTED;
            }
        } else if (coeff->tx_size == AV2H_TX_16X4 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 6U) {
            transform->primary_type = AV2H_TX_H_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X4 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X4 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X4 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X4 &&
                   mode->mode == AV2H_INTRA_D67) {
            static const Av2hTxType d67_16x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_FLIPADST_FLIPADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d67_16x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_D67) {
            static const Av2hTxType d67_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_V_DCT,
                AV2H_TX_V_ADST
            };
            transform->primary_type = d67_4x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_VERTICAL) {
            static const Av2hTxType vertical_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_V_DCT,
                AV2H_TX_V_ADST
            };
            transform->primary_type = vertical_4x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_D157) {
            static const Av2hTxType d157_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = d157_4x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL) {
            static const Av2hTxType horizontal_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_ADST, AV2H_TX_H_DCT,
                AV2H_TX_H_ADST
            };
            transform->primary_type = horizontal_4x4[symbol];
        } else {
              if ((mode->mode == AV2H_INTRA_DC ||
                  mode->mode == AV2H_INTRA_PAETH) && symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_DC && symbol == 1U) {
                transform->primary_type = AV2H_TX_ADST_ADST;
            } else if (mode->mode == AV2H_INTRA_D203 && symbol == 3U) {
                transform->primary_type = AV2H_TX_DCT_ADST;
            } else if (mode->mode == AV2H_INTRA_D203 && symbol == 6U) {
                transform->primary_type = AV2H_TX_H_ADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH && symbol == 0U) {
                transform->primary_type = AV2H_TX_DCT_DCT;
            } else if (mode->mode == AV2H_INTRA_SMOOTH && symbol == 3U) {
                transform->primary_type = AV2H_TX_DCT_ADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH && symbol == 4U) {
                transform->primary_type = AV2H_TX_FLIPADST_FLIPADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH && symbol == 5U) {
                transform->primary_type = AV2H_TX_ADST_FLIPADST;
            } else if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                       symbol == 6U) {
                transform->primary_type = AV2H_TX_V_ADST;
            } else {
                return AV2H_UNSUPPORTED;
            }
        }
    } else if ((coeff->tx_size == AV2H_TX_8X8 ||
                coeff->tx_size == AV2H_TX_16X16 ||
                coeff->tx_size == AV2H_TX_8X16 ||
                coeff->tx_size == AV2H_TX_16X8) &&
               (mode->mode == AV2H_INTRA_PAETH ||
                mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_HORIZONTAL ||
                mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D135 ||
                mode->mode == AV2H_INTRA_D113 ||
                mode->mode == AV2H_INTRA_D157 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D45)) {
        symbol = av2h_range_read_adaptive(
            decoder,
            coeff->tx_size == AV2H_TX_16X16 ?
                cdfs->primary_size2 : cdfs->primary_size1,
            AV2H_PRIMARY_TX_TYPE_SYMBOLS);
        transform->primary_symbol = (uint8_t)symbol;
        status = av2h_primary_tx_type_map(
            coeff->tx_size, mode->mode, 0U, (uint8_t)symbol,
            &transform->primary_type);
        if (status == AV2H_OK) {
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
            mode->mode == AV2H_INTRA_SMOOTH && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D135 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D135 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_PAETH && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_PAETH && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D203 && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D203 && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D45 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 4U) {
            transform->primary_type = AV2H_TX_DCT_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                   symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                   symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                   symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
            mode->mode == AV2H_INTRA_SMOOTH && symbol == 1U) {
            status = av2h_primary_tx_type_map(
                coeff->tx_size, mode->mode, 0U, (uint8_t)symbol,
                &transform->primary_type);
            if (status != AV2H_OK) return status;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X16 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
         } else if (coeff->tx_size == AV2H_TX_16X16 &&
                 mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                 symbol == 3U) {
             transform->primary_type = AV2H_TX_DCT_ADST;
         } else if (coeff->tx_size == AV2H_TX_16X16 &&
                 mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 0U) {
             transform->primary_type = AV2H_TX_DCT_DCT;
         } else if (coeff->tx_size == AV2H_TX_16X16 &&
                 mode->mode == AV2H_INTRA_DC && symbol == 0U) {
             transform->primary_type = AV2H_TX_DCT_DCT;
         } else if (coeff->tx_size == AV2H_TX_16X16 &&
                 mode->mode == AV2H_INTRA_DC && symbol == 1U) {
             transform->primary_type = AV2H_TX_ADST_ADST;
         } else if (coeff->tx_size == AV2H_TX_16X16 &&
                 mode->mode == AV2H_INTRA_DC && symbol == 3U) {
             transform->primary_type = AV2H_TX_DCT_ADST;
         } else if (coeff->tx_size == AV2H_TX_16X16 &&
                 mode->mode == AV2H_INTRA_DC && symbol == 6U) {
             transform->primary_type = AV2H_TX_FLIPADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
            mode->mode == AV2H_INTRA_D203) {
            static const Av2hTxType d203_8x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_DCT, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d203_8x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
            mode->mode == AV2H_INTRA_D45) {
            static const Av2hTxType d45_16x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_16x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
            mode->mode == AV2H_INTRA_D45) {
            static const Av2hTxType d45_8x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_8x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
            mode->mode == AV2H_INTRA_DC && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_PAETH && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
            mode->mode == AV2H_INTRA_DC && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 5U) {
            transform->primary_type = AV2H_TX_ADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                   symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D45 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 5U) {
            transform->primary_type = AV2H_TX_FLIPADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_D135 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 4U) {
            transform->primary_type = AV2H_TX_FLIPADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 5U) {
            transform->primary_type = AV2H_TX_FLIPADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 5U) {
            transform->primary_type = AV2H_TX_ADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_D157 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                   symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                   symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_PAETH && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                   symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                   symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                   symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 5U) {
            transform->primary_type = AV2H_TX_FLIPADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D113 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D135) {
            static const Av2hTxType d135_8x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d135_8x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D67 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                 mode->mode == AV2H_INTRA_D157 &&
                 (symbol == 0U || symbol == 2U)) {
             transform->primary_type = symbol == 0U ?
              AV2H_TX_DCT_DCT : AV2H_TX_ADST_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 5U) {
            transform->primary_type = AV2H_TX_ADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                   symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
                   symbol == 5U) {
            transform->primary_type = AV2H_TX_ADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 5U) {
            transform->primary_type = AV2H_TX_FLIPADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
                   mode->mode == AV2H_INTRA_VERTICAL && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 6U) {
            transform->primary_type = AV2H_TX_FLIPADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D45 && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D45 && symbol == 6U) {
            transform->primary_type = AV2H_TX_FLIPADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_D45 && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_PAETH && symbol == 1U) {
            transform->primary_type = AV2H_TX_ADST_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 0U) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 3U) {
            transform->primary_type = AV2H_TX_DCT_ADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL && symbol == 6U) {
            transform->primary_type = AV2H_TX_ADST_FLIPADST;
        } else if (coeff->tx_size == AV2H_TX_8X8 &&
                   mode->mode == AV2H_INTRA_DC && symbol == 5U) {
            transform->primary_type = AV2H_TX_ADST_FLIPADST;
        } else if (mode->mode == AV2H_INTRA_PAETH && symbol == 4U) {
            transform->primary_type = AV2H_TX_V_DCT;
        } else if (mode->mode == AV2H_INTRA_D67 && symbol == 2U) {
            transform->primary_type = AV2H_TX_ADST_DCT;
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

Av2hStatus av2h_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hTransformCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hLeafMode *mode,
    const Av2hCoeffPrefix *coeff,
    Av2hTransformType *transform) {
    uint32_t symbol;
    const Av2hTxDescriptor *descriptor;
    uint8_t secondary_signaled;
    Av2hStatus status;
    if (decoder == NULL || cdfs == NULL || sequence == NULL || frame == NULL ||
        mode == NULL || coeff == NULL || transform == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_transform_diagnostics_set_stage(
        decoder != NULL ? decoder->diagnostics : NULL, 1U);
    if (frame->reduced_tx_set || coeff->all_zero || coeff->eob == 0U) {
        return AV2H_UNSUPPORTED;
    }
    transform->before_primary_bits = av2h_range_tell(decoder);
    if (mode->fsc_mode) {
        status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
        if (status != AV2H_OK) return status;
        if (descriptor->output_width > 32U ||
            descriptor->output_height > 32U) {
            return AV2H_UNSUPPORTED;
        }
        transform->primary_type = AV2H_TX_IDTX;
        transform->primary_signaled = 0U;
        transform->primary_symbol = 0U;
        transform->after_primary_bits = transform->before_primary_bits;
        transform->secondary_signaled = 0U;
        transform->secondary_symbol = 0U;
        transform->cctx_type = 0U;
        transform->secondary_set_symbol = 0U;
        transform->secondary_set = 0U;
        transform->after_secondary_type_bits = transform->before_primary_bits;
        transform->after_secondary_bits = transform->before_primary_bits;
        return AV2H_OK;
    }
    if (coeff->eob == 1U && !mode->use_intrabc) {
        transform->primary_type = AV2H_TX_DCT_DCT;
        transform->primary_signaled = 0U;
        transform->primary_symbol = 0U;
        transform->secondary_signaled = 0U;
        transform->secondary_symbol = 0U;
        transform->secondary_set_symbol = 0U;
        transform->secondary_set = 0U;
        transform->after_primary_bits = transform->before_primary_bits;
        transform->after_secondary_type_bits = transform->before_primary_bits;
        transform->after_secondary_bits = transform->before_primary_bits;
        return AV2H_OK;
    }
    status = read_primary(decoder, cdfs, mode, coeff, transform);
    if (status != AV2H_OK) return status;
    av2h_transform_diagnostics_set_stage(decoder->diagnostics, 4U);
    transform->after_primary_bits = av2h_range_tell(decoder);
    status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    transform->secondary_signaled = 0U;
    transform->secondary_symbol = 0U;
    transform->secondary_set_symbol = 0U;
    transform->secondary_set = 0U;
    transform->after_secondary_type_bits = transform->after_primary_bits;
    status = av2h_secondary_tx_type_signal(
        sequence, mode, coeff, transform->primary_type, &secondary_signaled);
    if (status != AV2H_OK) return status;
    av2h_transform_diagnostics_set_stage(decoder->diagnostics, 5U);
    if (secondary_signaled) {
        uint16_t *secondary_cdf = mode->use_intrabc ?
            cdfs->intrabc_secondary[descriptor->square_class] :
            descriptor->square_class == AV2H_TX_SQUARE_4 ?
                cdfs->secondary_4x4 :
            descriptor->square_class == AV2H_TX_SQUARE_8 ?
                cdfs->secondary_8x8 :
            descriptor->square_class == AV2H_TX_SQUARE_16 ?
                cdfs->secondary_16x16 :
            descriptor->square_class == AV2H_TX_SQUARE_32 ?
                cdfs->secondary_32x32 : cdfs->secondary_64x64;
        symbol = av2h_range_read_adaptive(
            decoder, secondary_cdf,
            AV2H_SECONDARY_TX_TYPE_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->secondary_signaled = 1U;
        transform->secondary_symbol = (uint8_t)symbol;
        transform->after_secondary_type_bits = av2h_range_tell(decoder);
        if (symbol != 0U && !mode->use_intrabc) {
            uint8_t reduced_adst_set = (uint8_t)(
                transform->primary_type == AV2H_TX_ADST_ADST &&
                descriptor->output_width >= 8U &&
                descriptor->output_height >= 8U);
            Av2hIntraMode secondary_mode = av2h_transform_mode(
                mode->mode, mode->angle_delta, mode->mrl_index, descriptor);
            symbol = av2h_range_read_adaptive(
                decoder,
                reduced_adst_set ?
                    cdfs->secondary_adst_set : cdfs->secondary_set,
                reduced_adst_set ? AV2H_SECONDARY_ADST_TX_SET_SYMBOLS :
                    AV2H_SECONDARY_TX_SET_SYMBOLS);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            transform->secondary_set_symbol = (uint8_t)symbol;
            status = reduced_adst_set ? av2h_secondary_adst_tx_set_map(
                secondary_mode, (uint8_t)symbol, &transform->secondary_set) :
                av2h_secondary_tx_set_map(
                    secondary_mode, (uint8_t)symbol,
                    &transform->secondary_set);
            if (status != AV2H_OK) return status;
            if (transform->primary_type == AV2H_TX_ADST_ADST) {
                transform->secondary_set += 7U;
            }
        }
    }
    transform->after_secondary_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_chroma_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hTransformCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hChromaLeafMode *mode,
    uint8_t plane,
    uint8_t cctx_allowed,
    const Av2hCoeffPrefix *coeff,
    Av2hTransformType *transform) {
    const Av2hTxDescriptor *descriptor;
    uint32_t symbol = 0U;
    Av2hStatus status;
    if (decoder == NULL || cdfs == NULL || frame == NULL || mode == NULL || coeff == NULL ||
        transform == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
    if ((plane == 1U || plane == 2U) && status == AV2H_OK) {
        transform->before_primary_bits = av2h_range_tell(decoder);
        if (coeff->all_zero) {
            transform->primary_type = AV2H_TX_DCT_DCT;
        } else {
            status = av2h_chroma_primary_type_map(
                mode, descriptor, frame->reduced_tx_set,
                &transform->primary_type);
            if (status != AV2H_OK) return status;
        }
        transform->primary_signaled = 0U;
        transform->primary_symbol = 0U;
        transform->secondary_signaled = 0U;
        transform->secondary_symbol = 0U;
        transform->secondary_set_symbol = 0U;
        transform->secondary_set = 0U;
        transform->cctx_type = 0U;
        if (coeff->all_zero) {
            if (coeff->eob != 0U) return AV2H_UNSUPPORTED;
        } else {
            if (coeff->eob == 0U || coeff->eob > descriptor->eob_capacity) {
                return AV2H_UNSUPPORTED;
            }
            if (plane == 1U && cctx_allowed && coeff->eob != 1U) {
                symbol = av2h_range_read_adaptive(
                    decoder, cdfs->chroma_cross_type, 7U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                transform->cctx_type = (uint8_t)symbol;
            }
        }
        transform->after_primary_bits = av2h_range_tell(decoder);
        transform->after_secondary_type_bits =
            transform->after_primary_bits;
        transform->after_secondary_bits = transform->after_primary_bits;
        return AV2H_OK;
    }
    return status != AV2H_OK ? status : AV2H_UNSUPPORTED;
}
