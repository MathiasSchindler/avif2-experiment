#include "coeff_level.h"

#include "internal/coeff_common.h"
#include "internal/coeff_modules.h"

static int q_level_profile_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform) {
    if (cdfs->q_context != av2h_coeff_q_context(frame->base_q_index)) {
        return 0;
    }
    {
        Av2hCoeffTxGeometry geometry;
        (void)transform;
        return av2h_coeff_tx_geometry(prefix->tx_size, &geometry) == AV2H_OK;
    }
}

static int q_eob_context_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    uint8_t tx_context,
    uint8_t context,
    uint8_t low) {
    if (cdfs->q_context < 4U)
        return tx_context < 5U && context < 4U;
    if (cdfs->q_context == 1U) return 1;
    if (cdfs->q_context == 2U) {
        return (tx_context == 3U || tx_context == 4U) && context == 1U;
    }
    return cdfs->q_context == 3U &&
                            ((low && ((tx_context == 0U &&
                                (context == 0U || context == 1U ||
                                 context == 2U || context == 3U)) ||
                                        (tx_context == 3U &&
                 (context == 0U || context == 1U || context == 2U)) ||
                     (tx_context == 1U &&
                          (context == 0U || context == 1U ||
                           context == 2U || context == 3U)) ||
                     (tx_context == 2U &&
                      (context == 0U || context == 1U)) ||
                     (tx_context == 4U &&
                      (context == 0U || context == 1U)))) ||
            (!low && ((tx_context == 2U &&
                       (context == 1U || context == 2U)) ||
                      (tx_context == 1U && context == 1U) ||
                      (tx_context == 1U && context == 2U) ||
                      (tx_context == 1U && context == 3U) ||
                      (context == 1U &&
                       (tx_context == 3U || tx_context == 4U)))));
}

static int q_base_context_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    uint8_t tx_context,
    uint8_t context,
    uint8_t low) {
    if (cdfs->q_context < 4U)
        return tx_context < 5U && context < (low ? 33U : 20U);
    if (cdfs->q_context == 1U) return 1;
    if (cdfs->q_context == 2U) {
        if (tx_context == 4U) {
            return low && (context == 1U || context == 9U);
        }
        if (tx_context != 3U) return 0;
        if (!low) return context == 0U;
         return context == 1U || context == 2U || context == 6U ||
             context == 9U || context == 10U ||
               context == 13U || context == 16U || context == 17U ||
               context == 18U;
    }
    return cdfs->q_context == 3U &&
           ((low &&
                                 ((tx_context == 0U &&
                                    (context == 1U || context == 2U ||
                                     context == 9U ||
                                     context == 10U ||
                                     context == 11U ||
                                     context == 21U ||
                                             context == 22U ||
                                     context == 16U ||
                                      context == 17U)) ||
                          (tx_context == 1U &&
                                      (context == 0U || context == 1U || context == 2U || context == 3U || context == 4U || context == 5U || context == 9U || context == 10U || context == 11U || context == 12U || context == 13U || context == 16U ||
                                          context == 17U || context == 18U || context == 19U || context == 21U || context == 22U || context == 23U || context == 28U || context == 29U || context == 30U)) ||
                               (tx_context == 3U &&
                               (context == 0U || context == 1U || context == 2U || context == 3U ||
                                                context == 5U || context == 6U || context == 10U || context == 13U ||
                context == 9U || context == 12U || context == 16U || context == 19U)) ||
               (tx_context == 3U &&
                (context == 11U || context == 17U || context == 18U)) ||
               (tx_context == 2U &&
                                (context == 0U || context == 1U || context == 2U || context == 3U || context == 4U || context == 5U || context == 9U || context == 10U ||
                                 context == 11U || context == 12U ||
                                 context == 21U ||
                                 context == 22U ||
                                 context == 28U ||
                                 context == 29U ||
                 context == 16U || context == 17U || context == 18U ||
                 context == 19U)) ||
               (tx_context == 4U &&
                (context == 1U || context == 2U || context == 9U ||
                  context == 10U || context == 3U || context == 11U ||
                                    context == 12U || context == 13U ||
                  context == 16U ||
                  context == 17U || context == 18U ||
                 context == 19U)))) ||
            (!low &&
               ((tx_context == 1U &&
                                                                                                     (context == 0U || context == 1U || context == 2U || context == 5U || context == 6U || context == 7U || context == 10U || context == 17U || context == 18U ||
                                                                                                                                          context == 11U || context == 12U || context == 15U || context == 16U)) ||
                                                     (tx_context == 2U &&
                             (context == 0U || context == 1U || context == 2U || context == 3U ||
                                          context == 5U || context == 6U || context == 7U ||
                                                context == 10U || context == 11U)) ||
                            (tx_context == 3U &&
              (context == 0U || context == 1U || context == 2U || context == 10U ||
               context == 11U || context == 5U || context == 6U)) ||
              (tx_context == 4U &&
               (context == 0U || context == 1U || context == 2U ||
                context == 5U ||
                context == 10U ||
                context == 6U || context == 11U)))));
}

static int q_br_context_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    uint8_t context,
    uint8_t low) {
    if (cdfs->q_context < 4U) return low ? context < 14U : context < 7U;
    if (cdfs->q_context == 1U) return 1;
    if (!low) return 0;
    if (cdfs->q_context == 2U) {
        return context == 1U || context == 5U;
    }
    return cdfs->q_context == 3U &&
            (context == 0U || context == 1U || context == 2U || context == 3U ||
             context == 4U || context == 9U);
}

static uint16_t *q0_base_cdf(Av2hFirstCoeffLevelCdfs *cdfs,
                             uint8_t tx_context, uint8_t context,
                             uint8_t low) {
    switch (tx_context) {
        case 0U:
            return low ? cdfs->q0_txs0_base_lf[context] :
                         cdfs->q0_txs0_base[context];
        case 1U:
            return low ? cdfs->q0_txs1_base_lf[context] :
                         cdfs->q0_txs1_base[context];
        case 2U:
            return low ? cdfs->q0_txs2_base_lf[context] :
                         cdfs->q0_txs2_base[context];
        case 3U:
            return low ? cdfs->q0_txs3_base_lf[context] :
                         cdfs->q0_txs3_base[context];
        case 4U:
            return low ? cdfs->q0_txs4_base_lf[context] :
                         cdfs->q0_txs4_base[context];
        default:
            return 0;
    }
}

static uint16_t *q0_eob_cdf(Av2hFirstCoeffLevelCdfs *cdfs,
                            uint8_t tx_context, uint8_t context,
                            uint8_t low) {
    switch (tx_context) {
        case 0U:
            return low ? cdfs->q0_txs0_eob_base_lf[context] :
                         cdfs->q0_txs0_eob_base[context];
        case 1U:
            return low ? cdfs->q0_txs1_eob_base_lf[context] :
                         cdfs->q0_txs1_eob_base[context];
        case 2U:
            return low ? cdfs->q0_txs2_eob_base_lf[context] :
                         cdfs->q0_txs2_eob_base[context];
        case 3U:
            return low ? cdfs->q0_txs3_eob_base_lf[context] :
                         cdfs->q0_txs3_eob_base[context];
        case 4U:
            return low ? cdfs->q0_txs4_eob_base_lf[context] :
                         cdfs->q0_txs4_eob_base[context];
        default:
            return 0;
    }
}

static Av2hStatus read_base_level(Av2hRangeDecoder *decoder,
                                  Av2hFirstCoeffLevelCdfs *cdfs,
                                  Av2hFirstCoeffBlock *block,
                                  uint8_t tx_context, uint16_t position,
                                  uint8_t vertical, uint8_t low,
                                  uint8_t transpose,
                                  uint8_t *level) {
    uint8_t context = low ?
        av2h_coeff_lf_context(block, position, vertical, transpose) :
        av2h_coeff_base_context(block, position, vertical, transpose);
    uint16_t *cdf;
    uint32_t symbol;
    block->diagnostic_position = position;
    block->diagnostic_context = context;
    block->diagnostic_kind = low ? 1U : 0U;
    if (!q_base_context_supported(cdfs, tx_context, context, low)) {
        return AV2H_UNSUPPORTED;
    }
    if (cdfs->q_context == 0U) {
        cdf = q0_base_cdf(cdfs, tx_context, context, low);
    } else if (cdfs->q_context < 4U) {
        cdf = low ? cdfs->full_base_lf[tx_context][context] :
                    cdfs->full_base[tx_context][context];
    } else if (low && cdfs->q_context == 2U && tx_context == 4U &&
        (context == 1U || context == 9U)) {
        cdf = context == 1U ? cdfs->q2_base_lf_txs4_ctx1 :
                              cdfs->q2_base_lf_txs4_ctx9;
    } else if (low && cdfs->q_context == 1U && tx_context == 4U &&
               (context == 8U || context == 15U || context == 17U ||
                context == 19U || context == 20U)) {
        cdf = context == 8U ? cdfs->q1_base_lf_txs4_ctx8 :
              context == 15U ? cdfs->q1_base_lf_txs4_ctx15 :
              context == 17U ? cdfs->q1_base_lf_txs4_ctx17 :
              context == 19U ? cdfs->q1_base_lf_txs4_ctx19 :
                               cdfs->q1_base_lf_txs4_ctx20;
    } else if (!low && cdfs->q_context == 1U && tx_context == 4U &&
               context == 0U) {
        cdf = cdfs->q1_base_txs4_ctx0;
    } else if (!low && cdfs->q_context == 1U && tx_context == 4U &&
               context == 1U) {
        cdf = cdfs->q1_base_txs4_ctx1;
    } else if (!low && cdfs->q_context == 1U && tx_context == 4U &&
               context == 2U) {
        cdf = cdfs->q1_base_txs4_ctx2;
    } else if (!low && cdfs->q_context == 1U && tx_context == 4U &&
               context == 5U) {
        cdf = cdfs->q1_base_txs4_ctx5;
    } else if (low && cdfs->q_context == 2U && tx_context == 3U &&
        context == 1U) {
        cdf = cdfs->q2_base_lf_txs3_ctx1;
           } else if (low && cdfs->q_context == 3U && tx_context == 1U &&
                         (context == 21U || context == 22U ||
                                     context == 23U ||
                                     context == 28U || context == 29U ||
                                     context == 30U)) {
         cdf = context == 21U ? cdfs->q3_base_lf_txs1_ctx21 :
                             context == 22U ? cdfs->q3_base_lf_txs1_ctx22 :
                     context == 23U ? cdfs->q3_base_lf_txs1_ctx23 :
                context == 28U ? cdfs->q3_base_lf_txs1_ctx28 :
                     context == 29U ? cdfs->q3_base_lf_txs1_ctx29 :
                                            cdfs->q3_base_lf_txs1_ctx30;
                } else if (low && cdfs->q_context == 3U && tx_context == 2U &&
                             (context == 21U || context == 22U ||
                                context == 28U || context == 29U)) {
                 cdf = context == 21U ? cdfs->q3_base_lf_txs2_ctx21 :
                            context == 22U ? cdfs->q3_base_lf_txs2_ctx22 :
                    context == 28U ? cdfs->q3_base_lf_txs2_ctx28 :
                               cdfs->q3_base_lf_txs2_ctx29;
           } else if (low && cdfs->q_context == 3U && tx_context == 0U &&
               (context == 1U || context == 2U || context == 9U ||
                context == 11U || context == 21U || context == 22U)) {
         cdf = context == 1U ? cdfs->q3_base_lf_txs0_ctx1 :
               context == 2U ? cdfs->q3_base_lf_txs0_ctx2 :
                context == 9U ? cdfs->q3_base_lf_txs0_ctx9 :
               context == 11U ? cdfs->q3_base_lf_txs0_ctx11 :
               context == 21U ? cdfs->q3_base_lf_txs0_ctx21 :
                                cdfs->q3_base_lf_txs0_ctx22;
    } else if (low && cdfs->q_context == 3U && tx_context == 1U &&
                                (context == 0U || context == 1U || context == 2U || context == 3U || context == 4U || context == 5U || context == 9U || context == 10U || context == 11U || context == 12U || context == 13U || context == 16U ||
           context == 17U || context == 18U || context == 19U)) {
                    cdf = context == 0U ? cdfs->q3_base_lf_txs1_ctx0 :
                            context == 1U ? cdfs->q3_base_lf_txs1_ctx1 :
                            context == 2U ? cdfs->q3_base_lf_txs1_ctx2 :
              context == 3U ? cdfs->q3_base_lf_txs1_ctx3 :
                              context == 4U ? cdfs->q3_base_lf_txs1_ctx4 :
              context == 5U ? cdfs->q3_base_lf_txs1_ctx5 :
              context == 9U ? cdfs->q3_base_lf_txs1_ctx9 :
              context == 10U ? cdfs->q3_base_lf_txs1_ctx10 :
              context == 11U ? cdfs->q3_base_lf_txs1_ctx11 :
              context == 12U ? cdfs->q3_base_lf_txs1_ctx12 :
              context == 13U ? cdfs->q3_base_lf_txs1_ctx13 :
              context == 16U ? cdfs->q3_base_lf_txs1_ctx16 :
              context == 17U ? cdfs->q3_base_lf_txs1_ctx17 :
              context == 18U ? cdfs->q3_base_lf_txs1_ctx18 :
                               cdfs->q3_base_lf_txs1_ctx19;
    } else if (!low && cdfs->q_context == 3U && tx_context == 1U &&
                        (context == 0U || context == 1U || context == 2U || context == 5U || context == 6U || context == 7U || context == 10U || context == 11U || context == 12U)) {
          cdf = context == 0U ? cdfs->q3_base_txs1_ctx0 :
              context == 1U ? cdfs->q3_base_txs1_ctx1 :
                            context == 2U ? cdfs->q3_base_txs1_ctx2 :
              context == 5U ? cdfs->q3_base_txs1_ctx5 :
              context == 6U ? cdfs->q3_base_txs1_ctx6 :
              context == 7U ? cdfs->q3_base_txs1_ctx7 :
              context == 10U ? cdfs->q3_base_txs1_ctx10 :
                            context == 11U ? cdfs->q3_base_txs1_ctx11 :
                                                             cdfs->q3_base_txs1_ctx12;
    } else if (!low && cdfs->q_context == 3U && tx_context == 2U &&
            (context == 3U || context == 5U || context == 6U || context == 7U || context == 10U ||
         context == 11U)) {
          cdf = context == 3U ? cdfs->q3_base_txs2_ctx3 :
              context == 5U ? cdfs->q3_base_txs2_ctx5 :
              context == 6U ? cdfs->q3_base_txs2_ctx6 :
              context == 7U ? cdfs->q3_base_txs2_ctx7 :
              context == 10U ? cdfs->q3_base_txs2_ctx10 :
                               cdfs->q3_base_txs2_ctx11;
        } else if (!low && cdfs->q_context == 3U && tx_context == 3U &&
             context == 2U) {
         cdf = cdfs->q3_base_txs3_ctx2;
    } else if (!low && cdfs->q_context == 3U && tx_context == 4U &&
        (context == 0U || context == 1U || context == 2U || context == 5U ||
         context == 6U || context == 10U || context == 11U)) {
        cdf = context == 0U ? cdfs->q3_base_txs4_ctx0 :
              context == 1U ? cdfs->q3_base_txs4_ctx1 :
              context == 2U ? cdfs->q3_base_txs4_ctx2 :
              context == 5U ? cdfs->q3_base_txs4_ctx5 :
              context == 6U ? cdfs->q3_base_txs4_ctx6 :
              context == 10U ? cdfs->q3_base_txs4_ctx10 :
                               cdfs->q3_base_txs4_ctx11;
    } else if (low && cdfs->q_context == 3U && tx_context == 2U &&
          (context == 3U || context == 4U || context == 5U || context == 11U ||
         context == 12U)) {
        cdf = context == 3U ? cdfs->q3_base_lf_txs2_ctx3 :
              context == 4U ? cdfs->q3_base_lf_txs2_ctx4 :
              context == 5U ? cdfs->q3_base_lf_txs2_ctx5 :
              context == 11U ? cdfs->q3_base_lf_txs2_ctx11 :
                               cdfs->q3_base_lf_txs2_ctx12;
    } else if (low && cdfs->q_context == 3U && tx_context == 3U &&
        (context == 0U || context == 1U || context == 3U || context == 5U || context == 6U || context == 13U ||
         context == 12U || context == 19U)) {
        cdf = context == 0U ? cdfs->q3_base_lf_txs3_ctx0 :
              context == 1U ? cdfs->q3_base_lf_txs3_ctx1 :
              context == 3U ? cdfs->q3_base_lf_txs3_ctx3 :
              context == 5U ? cdfs->q3_base_lf_txs3_ctx5 :
              context == 6U ? cdfs->q3_base_lf_txs3_ctx6 :
              context == 13U ? cdfs->q3_base_lf_txs3_ctx13 :
              context == 12U ? cdfs->q3_base_lf_txs3_ctx12 :
                              cdfs->q3_base_lf_txs3_ctx19;
    } else if (low && cdfs->q_context == 3U && tx_context == 4U &&
        (context == 1U || context == 2U || context == 9U ||
         context == 10U || context == 3U || context == 11U ||
         context == 12U || context == 13U ||
         context == 16U ||
         context == 17U || context == 18U ||
         context == 19U)) {
        cdf = context == 1U ? cdfs->q3_base_lf_txs4_ctx1 :
              context == 2U ? cdfs->q3_base_lf_txs4_ctx2 :
              context == 3U ? cdfs->q3_base_lf_txs4_ctx3 :
              context == 9U ? cdfs->q3_base_lf_txs4_ctx9 :
              context == 10U ? cdfs->q3_base_lf_txs4_ctx10 :
              context == 11U ? cdfs->q3_base_lf_txs4_ctx11 :
              context == 12U ? cdfs->q3_base_lf_txs4_ctx12 :
              context == 13U ? cdfs->q3_base_lf_txs4_ctx13 :
              context == 16U ? cdfs->q3_base_lf_txs4_ctx16 :
              context == 17U ? cdfs->q3_base_lf_txs4_ctx17 :
              context == 18U ? cdfs->q3_base_lf_txs4_ctx18 :
                               cdfs->q3_base_lf_txs4_ctx19;
    } else {
        cdf = av2h_coeff_level_base_row(
            cdfs, tx_context, context, low);
    }
    if (cdf == 0) return AV2H_UNSUPPORTED;
    symbol = av2h_range_read_adaptive(decoder, cdf, low ? 6U : 4U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    *level = (uint8_t)symbol;
    if (symbol > (low ? 4U : 2U)) {
        uint8_t context_br = av2h_coeff_br_context(
            block, position, vertical, low, transpose);
        block->diagnostic_context = context_br;
        block->diagnostic_kind = low ? 3U : 2U;
        if (!q_br_context_supported(cdfs, context_br, low)) {
            return AV2H_UNSUPPORTED;
        }
        cdf = low ?
            (cdfs->q_context < 4U ? cdfs->full_br_lf[context_br] :
             cdfs->q_context == 1U && context_br == 12U ?
                cdfs->q1_br_lf_ctx12 :
             cdfs->q_context == 3U && context_br == 3U ?
                cdfs->q3_br_lf_ctx3 :
                 cdfs->q_context == 3U && context_br == 4U ?
                     cdfs->q3_br_lf_ctx4 :
             cdfs->q_context == 3U && context_br == 9U ?
                cdfs->q3_br_lf_ctx9 :
                av2h_coeff_level_br_row(cdfs, context_br, 1U)) :
                    (cdfs->q_context < 4U ? cdfs->full_br[context_br] :
                     av2h_coeff_level_br_row(cdfs, context_br, 0U));
        if (cdf == 0) return AV2H_UNSUPPORTED;
        symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        *level = (uint8_t)(*level + symbol);
    }
    return AV2H_OK;
}

static Av2hStatus read_unary(Av2hRangeDecoder *decoder, uint8_t maximum,
                             uint8_t *value) {
    uint8_t count;
    for (count = 0U; count < maximum; ++count) {
        uint32_t bit = av2h_range_read_bypass(decoder, 1U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (bit != 0U) break;
    }
    *value = count;
    return AV2H_OK;
}

static Av2hStatus read_high_range(Av2hRangeDecoder *decoder,
                                  uint32_t context, uint32_t *value) {
    uint8_t rice = 1U;
    uint8_t quotient;
    uint8_t maximum;
    uint32_t remainder;
    Av2hStatus status;
    while (rice < 6U && context >= (4U << (rice - 1U))) ++rice;
    maximum = (uint8_t)av2h_coeff_minimum((unsigned int)rice + 4U, 6U);
    status = read_unary(decoder, maximum, &quotient);
    if (status != AV2H_OK) return status;
    if (quotient == maximum) {
        uint8_t length;
        status = read_unary(decoder, 21U, &length);
        if (status != AV2H_OK || length > 20U) return AV2H_INVALID_DATA;
        length = (uint8_t)(length + rice + 1U);
        if (length >= 32U) return AV2H_INVALID_DATA;
        remainder = (1U << length) +
            av2h_range_read_bypass(decoder, length) -
            (1U << (rice + 1U));
    } else {
        remainder = av2h_range_read_bypass(decoder, rice);
    }
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    *value = remainder + ((uint32_t)quotient << rice);
    return AV2H_OK;
}

static Av2hStatus read_fsc_block(
    Av2hRangeDecoder *decoder, Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence, const Av2hFrameState *frame,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform, Av2hFirstCoeffBlock *block) {
    uint16_t scan_index;
    uint16_t bob;
    uint32_t high_average = 0U;
    uint32_t cul_level = 0U;
    uint8_t tx_context;
    Av2hCoeffTxGeometry geometry;
    Av2hStatus status;
    if (sequence->enable_tcq || sequence->enable_parity_hiding ||
        frame->using_qmatrix ||
        prefix->all_zero ||
        transform->primary_type != AV2H_TX_IDTX ||
        transform->secondary_symbol != 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (av2h_coeff_tx_geometry(prefix->tx_size, &geometry) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    tx_context = geometry.tx_context > 2U ? 2U : geometry.tx_context;
    block->width = geometry.width;
    block->height = geometry.height;
    block->coefficient_count = (uint16_t)(block->width * block->height);
    for (scan_index = 0U; scan_index < AV2H_FIRST_COEFF_CAPACITY;
         ++scan_index) {
        block->base_levels[scan_index] = 0U;
        block->quantized[scan_index] = 0;
        block->dequantized[scan_index] = 0;
    }
    block->nonzero_count = 0U;
    block->dc_value = 0;
    block->before_levels_bits = av2h_range_tell(decoder);
    bob = (uint16_t)(block->coefficient_count - prefix->eob);
    {
        uint16_t position = av2h_coeff_scan_position(
            bob, block->width, block->height, 0U, 0U);
        uint8_t context = bob <= block->coefficient_count / 8U ? 0U :
            bob <= block->coefficient_count / 4U ? 1U : 2U;
        uint16_t *cdf = cdfs->fsc_bob_all[tx_context][context];
        uint32_t symbol;
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 13U;
        if (cdf == 0) return AV2H_UNSUPPORTED;
        symbol = av2h_range_read_adaptive(decoder, cdf, 3U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)(symbol + 1U);
        if (block->base_levels[position] > 2U) {
            context = av2h_coeff_fsc_br_context(block, position);
            block->diagnostic_context = context;
            block->diagnostic_kind = 14U;
            cdf = cdfs->fsc_br_all[tx_context][context];
            symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            block->base_levels[position] =
                (uint8_t)(block->base_levels[position] + symbol);
        }
    }
    for (scan_index = (uint16_t)(bob + 1U);
         scan_index < block->coefficient_count; ++scan_index) {
        uint16_t position = av2h_coeff_scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        uint8_t context = av2h_coeff_fsc_base_context(block, position);
        uint16_t *cdf = cdfs->fsc_base_all[tx_context][context];
        uint32_t symbol;
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 15U;
        if (cdf == 0) return AV2H_UNSUPPORTED;
        symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)symbol;
        if (symbol > 2U) {
            context = av2h_coeff_fsc_br_context(block, position);
            block->diagnostic_context = context;
            block->diagnostic_kind = 16U;
            cdf = cdfs->fsc_br_all[tx_context][context];
            symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            block->base_levels[position] =
                (uint8_t)(block->base_levels[position] + symbol);
        }
    }
    block->after_levels_bits = av2h_range_tell(decoder);
    for (scan_index = bob; scan_index < block->coefficient_count;
         ++scan_index) {
        uint16_t position = av2h_coeff_scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        uint32_t final_level = block->base_levels[position];
        uint8_t context;
        uint16_t *cdf;
        uint8_t sign;
        uint64_t dequantized_magnitude;
        if (final_level == 0U) continue;
        context = av2h_coeff_fsc_sign_context(block, position);
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 17U;
        cdf = cdfs->fsc_sign_all[tx_context][context];
        sign = (uint8_t)av2h_range_read_adaptive(decoder, cdf, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (final_level >= 6U) {
            uint32_t high;
            block->diagnostic_context = (uint8_t)high_average;
            block->diagnostic_kind = 18U;
            status = read_high_range(decoder, high_average, &high);
            if (status != AV2H_OK) return status;
            final_level += high;
            high_average = (high_average + high) >> 1U;
        }
        if (final_level > 32767U) return AV2H_INVALID_DATA;
        block->quantized[position] = sign ? -(int32_t)final_level :
                                             (int32_t)final_level;
        if (position == 0U) block->dc_value = block->quantized[position];
        dequantized_magnitude = av2h_coeff_dequant_step_8bit(
            frame->base_q_index,
            position == 0U ? frame->y_dc_delta_q +
                sequence->base_y_dc_delta_q : 0);
        block->dequantized[position] = av2h_coeff_dequantize_level(
            final_level, sign, (uint32_t)dequantized_magnitude,
            geometry.dequant_shift);
        ++block->nonzero_count;
        cul_level += final_level;
    }
    block->cul_level =
        av2h_coeff_finalize_cul_level(cul_level, block->dc_value);
    block->after_coefficients_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    uint8_t dc_sign_context,
    Av2hFirstCoeffBlock *block) {
    uint16_t scan_index;
    uint8_t tx_context;
    uint8_t vertical;
    uint8_t column_scan;
    uint8_t context;
    uint16_t *cdf;
    uint32_t symbol;
    uint32_t high_average = 0U;
    uint32_t cul_level = 0U;
    Av2hCoeffTxGeometry geometry;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        mode == 0 || prefix == 0 || transform == 0 || block == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cdfs->q_context != av2h_coeff_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->fsc_mode || transform->primary_type == AV2H_TX_IDTX) {
        return read_fsc_block(decoder, cdfs, sequence, frame, prefix,
                              transform, block);
    }
    if (sequence->enable_tcq || sequence->enable_parity_hiding ||
        frame->using_qmatrix || mode->fsc_mode ||
        prefix->all_zero || prefix->eob == 0U || dc_sign_context >= 3U ||
        !q_level_profile_supported(cdfs, frame, prefix, transform)) {
        return AV2H_UNSUPPORTED;
    }
    if (av2h_coeff_tx_geometry(prefix->tx_size, &geometry) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    block->width = geometry.width;
    block->height = geometry.height;
    tx_context = geometry.tx_context;
    if (frame->base_q_index == 224U &&
        prefix->tx_size == AV2H_TX_8X16) {
        tx_context = 2U;
    }
    vertical = (uint8_t)(transform->primary_type == AV2H_TX_V_DCT ||
                         transform->primary_type == AV2H_TX_V_ADST ||
                         transform->primary_type == AV2H_TX_V_FLIPADST);
    column_scan = (uint8_t)(transform->primary_type == AV2H_TX_H_DCT ||
                            transform->primary_type == AV2H_TX_H_ADST ||
                            transform->primary_type ==
                                AV2H_TX_H_FLIPADST);
    if (transform->primary_type > AV2H_TX_H_FLIPADST) {
        return AV2H_UNSUPPORTED;
    }
    block->coefficient_count = (uint16_t)(block->width * block->height);
    if (prefix->eob > block->coefficient_count) return AV2H_INVALID_DATA;
    for (scan_index = 0U; scan_index < AV2H_FIRST_COEFF_CAPACITY;
         ++scan_index) {
        block->base_levels[scan_index] = 0U;
        block->quantized[scan_index] = 0;
        block->dequantized[scan_index] = 0;
    }
    block->nonzero_count = 0U;
    block->before_levels_bits = av2h_range_tell(decoder);
    scan_index = (uint16_t)(prefix->eob - 1U);
    {
        uint16_t position = av2h_coeff_scan_position(scan_index, block->width,
                                          block->height, vertical,
                                          column_scan);
        uint8_t low = av2h_coeff_is_low_frequency(
            position, block->width, vertical, column_scan);
        uint8_t level;
        context = av2h_coeff_eob_context(
            scan_index, block->coefficient_count);
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 7U;
        if (!q_eob_context_supported(
                cdfs, tx_context, context, low)) {
            return AV2H_UNSUPPORTED;
        }
        if (low) {
            if (cdfs->q_context == 0U) {
                cdf = q0_eob_cdf(
                    cdfs, tx_context, context, 1U);
            } else if (cdfs->q_context < 4U) {
                cdf = cdfs->full_eob_base_lf[tx_context][context];
            } else if (tx_context == 0U && context == 0U) {
                cdf = cdfs->q3_eob_lf_txs0_ctx0;
            } else if (tx_context == 0U && context == 1U) {
                cdf = cdfs->q_context == 3U ?
                    cdfs->q3_eob_lf_txs0_ctx1 : cdfs->eob_lf[0];
            } else if (tx_context == 0U && context == 2U) {
                cdf = cdfs->q3_eob_lf_txs0_ctx2;
            } else if (tx_context == 0U && context == 3U) {
                cdf = cdfs->eob_lf[4];
            } else if (tx_context == 1U && context == 3U) {
                cdf = cdfs->q_context == 3U ?
                    cdfs->q3_eob_lf_txs1_ctx3 : cdfs->eob_lf[1];
            } else if (tx_context == 1U && context == 2U) {
                cdf = cdfs->q_context == 3U ?
                    cdfs->q3_eob_lf_txs1_ctx2 : cdfs->eob_lf[2];
            } else if (tx_context == 1U && context == 1U) {
                cdf = cdfs->q_context == 3U ?
                    cdfs->q3_eob_lf_txs1_ctx1 : cdfs->eob_lf[3];
            } else if (tx_context == 1U && context == 0U) {
                cdf = cdfs->q_context == 3U ?
                    cdfs->q3_eob_lf_txs1_ctx0 : cdfs->eob_lf[6];
            } else if (tx_context == 2U && context == 1U) {
                cdf = cdfs->eob_lf[5];
            } else if (tx_context == 2U && context == 0U) {
                cdf = cdfs->eob_lf[9];
            } else if (tx_context == 3U && context == 1U) {
                cdf = cdfs->eob_lf[7];
            } else if (tx_context == 3U && context == 2U) {
                cdf = cdfs->q3_eob_lf_txs3_ctx2;
            } else if (tx_context == 3U && context == 0U) {
                cdf = cdfs->eob_lf[8];
            } else if (tx_context == 4U && context == 0U) {
                cdf = cdfs->q3_eob_lf_txs4_ctx0;
            } else if (tx_context == 4U && context == 1U) {
                cdf = cdfs->q_context == 2U ?
                    cdfs->q2_eob_lf_txs4_ctx1 :
                    cdfs->q3_eob_lf_txs4_ctx1;
            } else {
                return AV2H_UNSUPPORTED;
            }
            symbol = av2h_range_read_adaptive(decoder, cdf, 5U);
            level = (uint8_t)(symbol + 1U);
            block->diagnostic_kind = 8U;
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            if (level > 4U) {
                context = av2h_coeff_br_context(
                    block, position, vertical, low,
                                     column_scan);
                block->diagnostic_context = context;
                block->diagnostic_kind = 9U;
                if (!q_br_context_supported(cdfs, context, low)) {
                    return AV2H_UNSUPPORTED;
                }
                cdf = cdfs->q_context < 4U ? cdfs->full_br_lf[context] :
                    cdfs->q_context == 3U && context == 3U ?
                    cdfs->q3_br_lf_ctx3 :
                    cdfs->q_context == 3U && context == 4U ?
                    cdfs->q3_br_lf_ctx4 :
                    cdfs->q_context == 3U && context == 9U ?
                    cdfs->q3_br_lf_ctx9 :
                    av2h_coeff_level_br_row(cdfs, context, 1U);
                if (cdf == 0) return AV2H_UNSUPPORTED;
                symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                level = (uint8_t)(level + symbol);
                block->diagnostic_kind = 10U;
            }
        } else {
            if (cdfs->q_context == 0U) {
                cdf = q0_eob_cdf(
                    cdfs, tx_context, context, 0U);
            } else if (cdfs->q_context < 4U) {
                cdf = cdfs->full_eob_base[tx_context][context];
            } else if (cdfs->q_context == 3U && tx_context == 1U &&
                context == 1U) {
                cdf = cdfs->q3_eob_base_txs1_ctx1;
            } else if (cdfs->q_context == 3U && tx_context == 1U &&
                context == 2U) {
                cdf = cdfs->q3_eob_base_txs1_ctx2;
            } else if (cdfs->q_context == 3U && tx_context == 1U &&
                context == 3U) {
                cdf = cdfs->q3_eob_base_txs1_ctx3;
            } else if (cdfs->q_context == 3U && tx_context == 2U &&
                (context == 1U || context == 2U)) {
                cdf = context == 1U ? cdfs->q3_eob_base_txs2_ctx1 :
                                      cdfs->q3_eob_base_txs2_ctx2;
                } else if (tx_context == 4U && context == 1U &&
                       (cdfs->q_context == 1U ||
                    cdfs->q_context == 3U)) {
                cdf = cdfs->q_context == 1U ?
                    cdfs->q1_eob_base_txs4_ctx1 :
                    cdfs->q3_eob_base_txs4_ctx1;
            } else {
                cdf = av2h_coeff_level_eob_row(
                    cdfs, tx_context, context);
            }
            if (cdf == 0) return AV2H_UNSUPPORTED;
            symbol = av2h_range_read_adaptive(decoder, cdf, 3U);
            level = (uint8_t)(symbol + 1U);
            if (level > 2U) {
                if (!q_br_context_supported(cdfs, 0U, low)) {
                    return AV2H_UNSUPPORTED;
                }
                cdf = cdfs->q_context < 4U ? cdfs->full_br[0U] :
                    av2h_coeff_level_br_row(cdfs, 0U, 0U);
                if (cdf == 0) return AV2H_UNSUPPORTED;
                symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                level = (uint8_t)(level + symbol);
            }
        }
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = level;
    }
    while (scan_index != 0U) {
        uint16_t position;
        uint8_t low;
        uint8_t level;
        --scan_index;
        position = av2h_coeff_scan_position(
            scan_index, block->width, block->height,
                                 vertical, column_scan);
        low = av2h_coeff_is_low_frequency(
            position, block->width, vertical, column_scan);
        status = read_base_level(decoder, cdfs, block, tx_context, position,
                     vertical, low, column_scan, &level);
        if (status != AV2H_OK) return status;
        block->base_levels[position] = level;
    }
    block->after_levels_bits = av2h_range_tell(decoder);
    block->after_levels_range = decoder->range;
    block->after_levels_value = decoder->value;
    for (scan_index = prefix->eob; scan_index != 0U;) {
        uint16_t position;
        uint8_t level;
        uint8_t low;
        uint8_t sign;
        uint32_t final_level;
        uint32_t dequant;
        --scan_index;
        position = av2h_coeff_scan_position(
            scan_index, block->width, block->height,
                                 vertical, column_scan);
        level = block->base_levels[position];
        if (level == 0U) continue;
        uint8_t adaptive_sign = (uint8_t)(
            scan_index == 0U ||
            (vertical && position / block->width == 0U) ||
            (column_scan && position % block->width == 0U));
        uint8_t sign_context = scan_index == 0U ? dc_sign_context : 0U;
        block->diagnostic_position = position;
        block->diagnostic_context = sign_context;
        block->diagnostic_kind = adaptive_sign ? 4U : 5U;
        if (adaptive_sign) {
            sign = (uint8_t)av2h_range_read_adaptive(
                decoder, cdfs->dc_sign[sign_context], 2U);
        } else {
            sign = (uint8_t)av2h_range_read_bypass(decoder, 1U);
        }
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        final_level = level;
        low = av2h_coeff_is_low_frequency(
            position, block->width, vertical, column_scan);
        if (final_level >= (low ? 8U : 6U)) {
            uint32_t high;
            block->diagnostic_context = (uint8_t)high_average;
            block->diagnostic_kind = 6U;
            status = read_high_range(decoder, high_average, &high);
            if (status != AV2H_OK) return status;
            final_level += high;
            high_average = (high_average + high) >> 1U;
        }
        if (final_level > 32767U) return AV2H_INVALID_DATA;
        block->quantized[position] = sign ? -(int32_t)final_level :
                                             (int32_t)final_level;
        dequant = av2h_coeff_dequant_step_8bit(
            frame->base_q_index,
            scan_index == 0U ?
                frame->y_dc_delta_q + sequence->base_y_dc_delta_q : 0);
        block->dequantized[position] = av2h_coeff_dequantize_level(
            final_level, sign, dequant, geometry.dequant_shift);
        ++block->nonzero_count;
        cul_level += final_level;
        if (scan_index == 0U) {
            block->dc_value = block->quantized[position];
        }
    }
    block->cul_level =
        av2h_coeff_finalize_cul_level(cul_level, block->dc_value);
    block->after_coefficients_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_first_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    Av2hFirstCoeffBlock *block) {
    return av2h_coeff_block_read(decoder, cdfs, sequence, frame, mode, prefix,
                                 transform, 0U, block);
}

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
    Av2hFirstCoeffBlock *block) {
    uint8_t txb_skip_context;
    uint8_t dc_sign_context;
    block->diagnostic_position = leaf != 0 ? leaf->block_size : 0U;
    block->diagnostic_context = 0U;
    block->diagnostic_kind = 11U;
    Av2hStatus status = av2h_coeff_contexts_from_state(
        state, 0U, leaf, mode != 0 ? mode->fsc_mode : 0U,
        &txb_skip_context, &dc_sign_context);
    if (status != AV2H_OK) return status;
    block->diagnostic_context = dc_sign_context;
    block->diagnostic_kind = 12U;
    status = av2h_coeff_block_read(decoder, cdfs, sequence, frame, mode,
                                   prefix, transform, dc_sign_context, block);
    if (status != AV2H_OK) return status;
    return av2h_coeff_context_state_update(state, 0U, leaf, block->cul_level);
}

static Av2hStatus read_chroma_block(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hFirstCoeffBlock *block) {
    Av2hCoeffTxGeometry geometry;
    uint16_t scan_index;
    uint32_t high_average = 0U;
    uint32_t cul_level = 0U;
    uint32_t dc_step;
    uint32_t ac_step;
    Av2hStatus status;
    if (cdfs->q_context != av2h_coeff_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    if (sequence->enable_tcq || sequence->enable_parity_hiding ||
        frame->using_qmatrix || frame->u_dc_delta_q != 0 ||
        frame->u_ac_delta_q != 0 || frame->v_dc_delta_q != 0 ||
        frame->v_ac_delta_q != 0) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_coeff_tx_geometry(prefix->tx_size, &geometry);
    if (status != AV2H_OK) return status;
    if (geometry.coefficient_count > AV2H_FIRST_COEFF_CAPACITY) {
        return AV2H_OVERFLOW;
    }
    block->width = geometry.width;
    block->height = geometry.height;
    block->coefficient_count = (uint16_t)geometry.coefficient_count;
    for (scan_index = 0U; scan_index < AV2H_FIRST_COEFF_CAPACITY;
         ++scan_index) {
        block->base_levels[scan_index] = 0U;
        block->quantized[scan_index] = 0;
        block->dequantized[scan_index] = 0;
    }
    block->nonzero_count = 0U;
    block->cul_level = 0U;
    block->dc_value = 0;
    block->diagnostic_position = 0U;
    block->diagnostic_context = 0U;
    block->diagnostic_kind = 0U;
    block->before_levels_bits = av2h_range_tell(decoder);
    block->after_levels_bits = block->before_levels_bits;
    block->after_levels_range = decoder->range;
    block->after_levels_value = decoder->value;
    block->after_coefficients_bits = block->before_levels_bits;
    if (prefix->all_zero && prefix->eob == 0U) return AV2H_OK;
    if (prefix->all_zero || prefix->eob == 0U ||
        prefix->eob > geometry.coefficient_count) {
        return AV2H_UNSUPPORTED;
    }
    {
        uint16_t position = av2h_coeff_scan_position(
            (uint16_t)(prefix->eob - 1U), block->width, block->height,
            0U, 0U);
        uint8_t context = av2h_coeff_eob_context(
            (uint16_t)(prefix->eob - 1U), block->coefficient_count);
        uint8_t low = (uint8_t)(position == 0U);
        uint16_t *cdf;
        cdf = low ? cdfs->full_uv_eob_base_lf[context] :
                    cdfs->full_uv_eob_base[context];
        uint32_t symbol = av2h_range_read_adaptive(
            decoder, cdf, low ? 5U : 3U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)(symbol + 1U);
        if (!low && symbol + 1U > 2U) {
            symbol = av2h_range_read_adaptive(
                decoder, cdfs->full_uv_br[0U], 4U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            block->base_levels[position] =
                (uint8_t)(block->base_levels[position] + symbol);
        }
    }
    for (scan_index = (uint16_t)(prefix->eob - 1U);
         scan_index != 0U;) {
        uint16_t position;
        uint8_t low;
        uint8_t context;
        uint16_t *cdf;
        uint32_t symbol;
        --scan_index;
        position = av2h_coeff_scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        low = (uint8_t)(position == 0U);
        context = av2h_coeff_uv_level_context(block, position, plane, low);
        if (context >= 12U) return AV2H_UNSUPPORTED;
        cdf = low ? cdfs->full_uv_base_lf[context] :
                    cdfs->full_uv_base[context];
        symbol = av2h_range_read_adaptive(
            decoder, cdf, low ? 6U : 4U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)symbol;
        if (!low && symbol > 2U) {
            uint8_t br_context =
                av2h_coeff_uv_level_context(block, position, 1U, 1U);
            if (br_context >= 4U) return AV2H_UNSUPPORTED;
            symbol = av2h_range_read_adaptive(
                decoder, cdfs->full_uv_br[br_context], 4U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            block->base_levels[position] =
                (uint8_t)(block->base_levels[position] + symbol);
        }
    }
    block->after_levels_bits = av2h_range_tell(decoder);
    block->after_levels_range = decoder->range;
    block->after_levels_value = decoder->value;
    dc_step = av2h_coeff_dequant_step_8bit(
        frame->base_q_index,
        plane == 1U ? frame->u_dc_delta_q : frame->v_dc_delta_q);
    ac_step = av2h_coeff_dequant_step_8bit(
        frame->base_q_index,
        plane == 1U ? frame->u_ac_delta_q : frame->v_ac_delta_q);
    for (scan_index = prefix->eob; scan_index != 0U;) {
        uint16_t position;
        uint32_t final_level;
        uint8_t sign;
        uint32_t step;
        --scan_index;
        position = av2h_coeff_scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        final_level = block->base_levels[position];
        if (final_level == 0U) continue;
        sign = (uint8_t)av2h_range_read_bypass(decoder, 1U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (final_level >= (position == 0U ? 5U : 6U)) {
            uint32_t high;
            status = read_high_range(decoder, high_average, &high);
            if (status != AV2H_OK) return status;
            final_level += high;
            high_average = (high_average + high) >> 1U;
        }
        if (final_level > 32767U) return AV2H_INVALID_DATA;
        block->quantized[position] =
            sign ? -(int32_t)final_level : (int32_t)final_level;
        step = position == 0U ? dc_step : ac_step;
        block->dequantized[position] = av2h_coeff_dequantize_level(
            final_level, sign, step, geometry.dequant_shift);
        ++block->nonzero_count;
        cul_level += final_level;
        if (position == 0U) block->dc_value = block->quantized[position];
    }
    block->cul_level =
        av2h_coeff_finalize_cul_level(cul_level, block->dc_value);
    block->after_coefficients_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_first_chroma_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hFirstCoeffBlock *block) {
    uint16_t scan_index;
    Av2hCoeffTxGeometry geometry;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        prefix == 0 || block == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) || sequence->enable_tcq ||
        sequence->enable_parity_hiding ||
        frame->using_qmatrix || frame->u_dc_delta_q != 0 ||
        frame->u_ac_delta_q != 0 || frame->v_dc_delta_q != 0 ||
        frame->v_ac_delta_q != 0) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_coeff_tx_geometry(prefix->tx_size, &geometry);
    if (status != AV2H_OK) return status;
    if (geometry.coefficient_count > AV2H_FIRST_COEFF_CAPACITY) {
        return AV2H_OVERFLOW;
    }
    if (prefix->all_zero && prefix->eob == 0U) {
        block->width = geometry.width;
        block->height = geometry.height;
        block->coefficient_count = (uint16_t)geometry.coefficient_count;
        for (scan_index = 0U; scan_index < AV2H_FIRST_COEFF_CAPACITY;
             ++scan_index) {
            block->base_levels[scan_index] = 0U;
            block->quantized[scan_index] = 0;
            block->dequantized[scan_index] = 0;
        }
        block->nonzero_count = 0U;
        block->cul_level = 0U;
        block->dc_value = 0;
        block->diagnostic_position = 0U;
        block->diagnostic_context = 0U;
        block->diagnostic_kind = 0U;
        block->before_levels_bits = av2h_range_tell(decoder);
        block->after_levels_bits = block->before_levels_bits;
        block->after_levels_range = decoder->range;
        block->after_levels_value = decoder->value;
        block->after_coefficients_bits = block->before_levels_bits;
        return AV2H_OK;
    }
    return read_chroma_block(
        decoder, cdfs, sequence, frame, plane, prefix, block);
}

Av2hStatus av2h_chroma_coeff_block_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hCoeffContextState *state,
    Av2hFirstCoeffBlock *block) {
    Av2hStatus status = av2h_first_chroma_coeff_block_read(
        decoder, cdfs, sequence, frame, plane, prefix, block);
    if (status != AV2H_OK) return status;
    return av2h_coeff_context_state_update(state, plane, leaf,
                                           block->cul_level);
}