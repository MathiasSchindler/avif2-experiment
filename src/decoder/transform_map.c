#include "internal/transform_modules.h"

#include "tx_geometry.h"

#include <stddef.h>

Av2hIntraMode av2h_transform_mode(
    Av2hIntraMode mode, int8_t angle_delta,
    uint8_t mrl_index,
    const Av2hTxDescriptor *descriptor) {
    static const int mrl_delta[4U] = { 0, 1, -1, 0 };
    int angle;
    switch (mode) {
        case AV2H_INTRA_VERTICAL: angle = 90; break;
        case AV2H_INTRA_HORIZONTAL: angle = 180; break;
        case AV2H_INTRA_D45: angle = 45; break;
        case AV2H_INTRA_D67: angle = 67; break;
        case AV2H_INTRA_D113: angle = 113; break;
        case AV2H_INTRA_D135: angle = 135; break;
        case AV2H_INTRA_D157: angle = 157; break;
        case AV2H_INTRA_D203: angle = 203; break;
        default: return mode;
    }
    angle += angle_delta * 3;
    if (mrl_index < 4U) angle += mrl_delta[mrl_index];
    if ((descriptor->output_height == 2U * descriptor->output_width &&
         angle < 61) ||
        (descriptor->output_height == 4U * descriptor->output_width &&
         angle < 73) ||
        (descriptor->output_height == 8U * descriptor->output_width &&
         angle < 82) ||
        (descriptor->output_height == 16U * descriptor->output_width &&
         angle < 86)) {
        return AV2H_INTRA_D203;
    }
    if ((descriptor->output_width == 2U * descriptor->output_height &&
         angle > 209) ||
        (descriptor->output_width == 4U * descriptor->output_height &&
         angle > 197) ||
        (descriptor->output_width == 8U * descriptor->output_height &&
         angle > 188) ||
        (descriptor->output_width == 16U * descriptor->output_height &&
         angle > 184)) {
        return AV2H_INTRA_D45;
    }
    return mode;
}

Av2hStatus av2h_chroma_primary_type_map(
    const Av2hChromaLeafMode *mode,
    const Av2hTxDescriptor *descriptor,
    uint8_t reduced_tx_set,
    Av2hTxType *tx_type) {
    static const Av2hTxType mode_types[13U] = {
        AV2H_TX_DCT_DCT,
        AV2H_TX_ADST_DCT,
        AV2H_TX_DCT_ADST,
        AV2H_TX_DCT_DCT,
        AV2H_TX_ADST_ADST,
        AV2H_TX_ADST_DCT,
        AV2H_TX_DCT_ADST,
        AV2H_TX_DCT_ADST,
        AV2H_TX_ADST_DCT,
        AV2H_TX_ADST_ADST,
        AV2H_TX_ADST_DCT,
        AV2H_TX_DCT_ADST,
        AV2H_TX_ADST_ADST
    };
    if (mode == NULL || descriptor == NULL || tx_type == NULL ||
        (uint8_t)mode->mode >= 13U) {
        return AV2H_INVALID_ARGUMENT;
    }
    *tx_type = mode_types[(uint8_t)av2h_transform_mode(
        mode->mode, mode->angle_delta, 0U, descriptor)];
    if (reduced_tx_set ||
        descriptor->signaling_class == AV2H_TX_SIGNAL_DCT_ONLY ||
                ((descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_32 ||
                    descriptor->signaling_class == AV2H_TX_SIGNAL_HIGH_32 ||
                    descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_64 ||
                    descriptor->signaling_class == AV2H_TX_SIGNAL_HIGH_64) &&
                 *tx_type == AV2H_TX_ADST_ADST) ||
        (descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_32 &&
         *tx_type == AV2H_TX_DCT_ADST) ||
        (descriptor->signaling_class == AV2H_TX_SIGNAL_HIGH_32 &&
         *tx_type == AV2H_TX_ADST_DCT) ||
        (descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_64 &&
         *tx_type != AV2H_TX_ADST_DCT) ||
        (descriptor->signaling_class == AV2H_TX_SIGNAL_HIGH_64 &&
         *tx_type != AV2H_TX_DCT_ADST)) {
        *tx_type = AV2H_TX_DCT_DCT;
    }
    return AV2H_OK;
}

Av2hStatus av2h_secondary_tx_set_map(
    Av2hIntraMode mode, uint8_t symbol, uint8_t *set) {
    static const uint8_t inverse_mapping[12U][7U] = {
        { 6U, 1U, 0U, 5U, 4U, 3U, 2U },
        { 1U, 6U, 0U, 4U, 2U, 5U, 3U },
        { 1U, 6U, 0U, 4U, 2U, 5U, 3U },
        { 2U, 6U, 0U, 5U, 1U, 4U, 3U },
        { 3U, 4U, 6U, 1U, 0U, 2U, 5U },
        { 4U, 1U, 3U, 6U, 0U, 5U, 2U },
        { 4U, 1U, 3U, 6U, 0U, 5U, 2U },
        { 5U, 0U, 6U, 2U, 1U, 4U, 3U },
        { 5U, 0U, 6U, 2U, 1U, 4U, 3U },
        { 6U, 1U, 0U, 5U, 4U, 3U, 2U },
        { 1U, 6U, 0U, 4U, 2U, 5U, 3U },
        { 1U, 6U, 0U, 4U, 2U, 5U, 3U }
    };
    if (set == NULL) return AV2H_INVALID_ARGUMENT;
    if ((uint8_t)mode >= 12U || symbol >= AV2H_SECONDARY_TX_SET_SYMBOLS) {
        return AV2H_UNSUPPORTED;
    }
    *set = inverse_mapping[(uint8_t)mode][symbol];
    return AV2H_OK;
}

Av2hStatus av2h_secondary_adst_tx_set_map(
    Av2hIntraMode mode, uint8_t symbol, uint8_t *set) {
    static const uint8_t inverse_mapping[12U][4U] = {
        { 3U, 1U, 0U, 2U },
        { 1U, 3U, 0U, 2U },
        { 1U, 3U, 0U, 2U },
        { 1U, 3U, 0U, 2U },
        { 0U, 2U, 3U, 1U },
        { 2U, 1U, 0U, 3U },
        { 2U, 1U, 0U, 3U },
        { 1U, 0U, 3U, 2U },
        { 1U, 0U, 3U, 2U },
        { 3U, 1U, 0U, 2U },
        { 1U, 3U, 0U, 2U },
        { 1U, 3U, 0U, 2U }
    };
    if (set == NULL) return AV2H_INVALID_ARGUMENT;
    if ((uint8_t)mode >= 12U ||
        symbol >= AV2H_SECONDARY_ADST_TX_SET_SYMBOLS) {
        return AV2H_UNSUPPORTED;
    }
    *set = inverse_mapping[(uint8_t)mode][symbol];
    return AV2H_OK;
}

Av2hStatus av2h_primary_tx_type_map(
    Av2hTxSize tx_size,
    Av2hIntraMode mode,
    uint8_t long_side_symbol,
    uint8_t primary_symbol,
    Av2hTxType *tx_type) {
    static const uint8_t intra_types[3U][13U][7U] = {
        {
            { 0U, 3U, 1U, 2U, 7U, 8U, 13U },
            { 0U, 3U, 1U, 2U, 7U, 10U, 12U },
            { 0U, 3U, 1U, 2U, 8U, 11U, 13U },
            { 0U, 3U, 1U, 2U, 6U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 7U, 8U, 13U },
            { 0U, 3U, 1U, 2U, 7U, 12U, 14U },
            { 0U, 3U, 1U, 2U, 7U, 8U, 13U },
            { 0U, 3U, 1U, 2U, 8U, 11U, 13U },
            { 0U, 3U, 1U, 2U, 7U, 10U, 12U },
            { 0U, 3U, 1U, 2U, 6U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 7U, 8U, 12U },
            { 0U, 3U, 1U, 2U, 7U, 8U, 13U },
            { 0U, 3U, 2U, 10U, 11U, 12U, 13U }
        },
        {
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 7U, 6U, 8U },
            { 0U, 3U, 1U, 2U, 8U, 4U, 7U },
            { 0U, 3U, 1U, 2U, 6U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 8U, 7U },
            { 0U, 3U, 1U, 2U, 5U, 6U, 7U },
            { 0U, 3U, 1U, 2U, 6U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 10U, 11U, 13U }
        },
        {
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 8U, 7U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 6U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 4U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 5U, 7U, 8U },
            { 0U, 3U, 1U, 2U, 10U, 11U, 12U }
        }
    };
        static const uint8_t adst_flipadst_symbols[3][13] = {
                { 4U, 4U, 0xffU, 5U, 4U, 4U, 4U,
                    0xffU, 4U, 5U, 4U, 4U, 0xffU },
                { 5U, 4U, 6U, 5U, 5U, 5U, 5U,
                    4U, 6U, 5U, 5U, 5U, 0xffU },
                { 5U, 5U, 6U, 5U, 5U, 5U, 5U,
                    0xffU, 5U, 5U, 5U, 5U, 0xffU }
        };
    const Av2hTxDescriptor *descriptor;
        uint8_t size_class;
    Av2hStatus status;
    if (tx_type == NULL) return AV2H_INVALID_ARGUMENT;
    *tx_type = AV2H_TX_TYPE_INVALID;
    status = av2h_tx_descriptor_get(tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    if (descriptor->signaling_class == AV2H_TX_SIGNAL_DCT_ONLY) {
        if (long_side_symbol != 0U || primary_symbol != 0U) {
            return AV2H_UNSUPPORTED;
        }
        *tx_type = AV2H_TX_DCT_DCT;
        return AV2H_OK;
    }
    size_class =
        tx_size == AV2H_TX_4X4 || tx_size == AV2H_TX_4X8 ||
        tx_size == AV2H_TX_8X4 ? 0U :
        tx_size == AV2H_TX_4X16 || tx_size == AV2H_TX_16X4 ||
        tx_size == AV2H_TX_8X8 || tx_size == AV2H_TX_8X16 ||
        tx_size == AV2H_TX_16X8 ? 1U :
        tx_size == AV2H_TX_16X16 ? 2U : 0xffU;
    if (size_class < 3U &&
        (uint8_t)mode <= (uint8_t)AV2H_INTRA_PAETH &&
        long_side_symbol == 0U &&
        primary_symbol < AV2H_PRIMARY_TX_TYPE_SYMBOLS) {
        *tx_type = (Av2hTxType)
            intra_types[size_class][(uint8_t)mode][primary_symbol];
        return AV2H_OK;
    }
    if (size_class != 0xffU &&
        (uint8_t)mode <= (uint8_t)AV2H_INTRA_PAETH &&
        long_side_symbol == 0U &&
        primary_symbol == 0U) {
        *tx_type = AV2H_TX_DCT_DCT;
        return AV2H_OK;
    }
    if (size_class != 0xffU &&
        (uint8_t)mode <= (uint8_t)AV2H_INTRA_PAETH &&
        long_side_symbol == 0U &&
        primary_symbol == adst_flipadst_symbols[size_class][(uint8_t)mode]) {
        *tx_type = AV2H_TX_ADST_FLIPADST;
        return AV2H_OK;
    }
    if (size_class == 0U && mode == AV2H_INTRA_D113 &&
        long_side_symbol == 0U && primary_symbol == 6U) {
        *tx_type = AV2H_TX_V_FLIPADST;
        return AV2H_OK;
    }
    if ((descriptor->signaling_class == AV2H_TX_SIGNAL_INTRA_4 ||
         descriptor->signaling_class == AV2H_TX_SIGNAL_INTRA_8 ||
         descriptor->signaling_class == AV2H_TX_SIGNAL_INTRA_16) &&
        (uint8_t)mode <= (uint8_t)AV2H_INTRA_PAETH &&
        long_side_symbol == 0U &&
        primary_symbol == 2U &&
        !(descriptor->square_class == AV2H_TX_SQUARE_4 &&
          mode == AV2H_INTRA_PAETH)) {
        *tx_type = AV2H_TX_ADST_DCT;
        return AV2H_OK;
        }
        if ((descriptor->signaling_class == AV2H_TX_SIGNAL_INTRA_4 ||
         descriptor->signaling_class == AV2H_TX_SIGNAL_INTRA_8 ||
         descriptor->signaling_class == AV2H_TX_SIGNAL_INTRA_16) &&
        (uint8_t)mode <= (uint8_t)AV2H_INTRA_PAETH &&
        long_side_symbol == 0U &&
        primary_symbol ==
            (descriptor->square_class == AV2H_TX_SQUARE_4 &&
             mode == AV2H_INTRA_PAETH ? 2U : 3U)) {
        *tx_type = AV2H_TX_DCT_ADST;
        return AV2H_OK;
    }
    if (tx_size == AV2H_TX_16X16 && mode == AV2H_INTRA_SMOOTH &&
        long_side_symbol == 0U && primary_symbol == 1U) {
        *tx_type = AV2H_TX_ADST_ADST;
        return AV2H_OK;
    }
    if (descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_32 ||
        descriptor->signaling_class == AV2H_TX_SIGNAL_HIGH_32 ||
        descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_64 ||
        descriptor->signaling_class == AV2H_TX_SIGNAL_HIGH_64) {
        int wide =
            descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_32 ||
            descriptor->signaling_class == AV2H_TX_SIGNAL_WIDE_64;
        if (long_side_symbol > 1U || primary_symbol > 3U) {
            return AV2H_UNSUPPORTED;
        }
        if (long_side_symbol != 0U) {
            if (wide) {
                *tx_type = primary_symbol == 0U ? AV2H_TX_DCT_DCT :
                    primary_symbol == 1U ? AV2H_TX_ADST_DCT :
                    primary_symbol == 2U ? AV2H_TX_FLIPADST_DCT :
                                           AV2H_TX_H_DCT;
            } else {
                *tx_type = primary_symbol == 0U ? AV2H_TX_DCT_DCT :
                    primary_symbol == 1U ? AV2H_TX_DCT_ADST :
                    primary_symbol == 2U ? AV2H_TX_DCT_FLIPADST :
                                           AV2H_TX_V_DCT;
            }
            return AV2H_OK;
        }
        if (wide) {
            *tx_type = primary_symbol == 0U ? AV2H_TX_V_DCT :
                primary_symbol == 1U ? AV2H_TX_V_ADST :
                primary_symbol == 2U ? AV2H_TX_V_FLIPADST : AV2H_TX_IDTX;
        } else {
            *tx_type = primary_symbol == 0U ? AV2H_TX_H_DCT :
                primary_symbol == 1U ? AV2H_TX_H_ADST :
                primary_symbol == 2U ? AV2H_TX_H_FLIPADST :
                primary_symbol == 3U ? AV2H_TX_IDTX :
                                       AV2H_TX_TYPE_INVALID;
            if (*tx_type == AV2H_TX_TYPE_INVALID) return AV2H_UNSUPPORTED;
        }
        return AV2H_OK;
    }
    return AV2H_UNSUPPORTED;
}