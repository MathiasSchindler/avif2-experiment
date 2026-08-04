#include "transform.h"

uint8_t av2h_debug_transform_stage;
#include "tx_geometry.h"

Av2hStatus av2h_first_transform_cdfs_init(Av2hFirstTransformCdfs *cdfs) {
    static const uint16_t primary_size0[] = {
        5026U, 16816U, 19974U, 23404U, 26845U, 30499U
    };
    static const uint16_t primary_size1[] = {
        14910U, 25257U, 26964U, 29323U, 30237U, 31535U
    };
    static const uint16_t primary_size2[] = {
        13759U, 26108U, 27688U, 29793U, 30265U, 31576U
    };
    static const uint16_t primary_short_side_4[] = {
        10692U, 26586U, 29231U
    };
    static const uint16_t primary_short_side_8[] = {
        26700U, 32160U, 32748U
    };
    static const uint16_t primary_short_side_16[] = {
        26915U, 32411U, 32748U
    };
    static const uint16_t intrabc_short_side[3U][3U][3U] = {
        {
            { 8347U, 20254U, 24536U },
            { 15994U, 26294U, 32748U },
            { 21212U, 27810U, 32748U }
        },
        {
            { 21815U, 25084U, 26230U },
            { 29354U, 31747U, 32748U },
            { 31614U, 32529U, 32748U }
        },
        {
            { 10501U, 20590U, 24181U },
            { 17596U, 26388U, 32748U },
            { 15407U, 26475U, 32732U }
        }
    };
    static const uint8_t intrabc_short_side_parameter[3U][3U] = {
        { 31U, 1U, 1U },
        { 62U, 37U, 36U },
        { 31U, 37U, 60U }
    };
    static const Av2hBinaryCdfSpec intrabc_type_set[3U][2U] = {
        { { 14122U, 25U, 0U }, { 8962U, 31U, 0U } },
        { { 16423U, 26U, 0U }, { 23446U, 0U, 0U } },
        { { 23352U, 25U, 0U }, { 17069U, 56U, 0U } }
    };
    static const Av2hBinaryCdfSpec intrabc_type_set2[3U] = {
        { 20835U, 0U, 0U }, { 30720U, 3U, 0U }, { 28857U, 17U, 0U }
    };
    static const uint16_t intrabc_type_index[3U][7U] = {
        { 8914U, 10732U, 12270U, 14822U, 17128U, 19586U, 21964U },
        { 1160U, 1555U, 1972U, 3414U, 3962U, 5151U, 5908U },
        { 22819U, 24338U, 25592U, 27001U, 28395U, 29648U, 30990U }
    };
    static const uint8_t intrabc_type_index_parameter[3U] = {
        25U, 31U, 25U
    };
    static const uint16_t intrabc_type_index2[3U][7U] = {
        { 1140U, 1725U, 2324U, 14653U, 19072U, 23618U, 28109U },
        { 58U, 261U, 587U, 32317U, 32556U, 32626U, 32708U },
        { 17404U, 17669U, 18403U, 24052U, 26393U, 28506U, 30676U }
    };
    static const uint8_t intrabc_type_index2_parameter[3U] = {
        31U, 2U, 57U
    };
    static const uint16_t intrabc_type_offset[3U][7U] = {
        { 3121U, 6470U, 9191U, 12280U, 17811U, 22588U, 27697U },
        { 338U, 377U, 571U, 743U, 7932U, 11860U, 17524U },
        { 3314U, 7625U, 10639U, 14206U, 19363U, 23456U, 28033U }
    };
    static const uint8_t intrabc_type_offset_parameter[3U] = {
        93U, 1U, 93U
    };
    static const uint16_t intrabc_type_offset2[3U][3U] = {
        { 8669U, 16533U, 24855U },
        { 9441U, 16413U, 25276U },
        { 8767U, 17611U, 24876U }
    };
    static const uint8_t intrabc_type_offset2_parameter[3U] = {
        93U, 90U, 10U
    };
    static const uint16_t secondary_32x32[] = {
        2472U, 8725U, 13853U
    };
    static const uint16_t secondary_64x64[] = {
        7523U, 11681U, 14783U
    };
    static const uint16_t secondary_8x8[] = {
        2357U, 9504U, 16641U
    };
    static const uint16_t secondary_16x16[] = {
        1364U, 8034U, 14431U
    };
    static const uint16_t secondary_4x4[] = {
        4486U, 15589U, 26440U
    };
    static const uint16_t secondary_set[] = {
        20712U, 26263U, 30623U, 32732U, 32736U, 32740U
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
        cdfs->primary_size2, AV2H_PRIMARY_TX_TYPE_SYMBOLS + 4U,
        primary_size2, AV2H_PRIMARY_TX_TYPE_SYMBOLS, 35U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->primary_long_side_32,
        (Av2hBinaryCdfSpec){ 36U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->primary_short_side_4, 4U + 4U, primary_short_side_4,
        4U, 31U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->primary_short_side_8, 4U + 4U, primary_short_side_8,
        4U, 6U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->primary_short_side_16, 4U + 4U, primary_short_side_16,
        4U, 6U, 0U);
    if (status != AV2H_OK) return status;
    {
        size_t eob_context;
        size_t square_context;
        for (eob_context = 0U; eob_context < 3U; ++eob_context) {
            for (square_context = 0U; square_context < 3U;
                 ++square_context) {
                status = av2h_cdf_init(
                    cdfs->intrabc_short_side[eob_context][square_context],
                    4U + 4U,
                    intrabc_short_side[eob_context][square_context], 4U,
                    intrabc_short_side_parameter[eob_context][square_context],
                    0U);
                if (status != AV2H_OK) return status;
            }
        }
    }
    {
        size_t eob_context;
        size_t square_context;
        for (eob_context = 0U; eob_context < 3U; ++eob_context) {
            for (square_context = 0U; square_context < 2U;
                 ++square_context) {
                status = av2h_binary_cdf_init(
                    cdfs->intrabc_type_set[eob_context][square_context],
                    intrabc_type_set[eob_context][square_context]);
                if (status != AV2H_OK) return status;
            }
            status = av2h_binary_cdf_init(
                cdfs->intrabc_type_set2[eob_context],
                intrabc_type_set2[eob_context]);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->intrabc_type_index[eob_context],
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS + 4U,
                intrabc_type_index[eob_context],
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS,
                intrabc_type_index_parameter[eob_context], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->intrabc_type_index2[eob_context],
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS + 4U,
                intrabc_type_index2[eob_context],
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS,
                intrabc_type_index2_parameter[eob_context], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->intrabc_type_offset[eob_context],
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS + 4U,
                intrabc_type_offset[eob_context],
                AV2H_INTER_TX_TYPE_INDEX_SYMBOLS,
                intrabc_type_offset_parameter[eob_context], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->intrabc_type_offset2[eob_context], 4U + 4U,
                intrabc_type_offset2[eob_context], 4U,
                intrabc_type_offset2_parameter[eob_context], 0U);
            if (status != AV2H_OK) return status;
        }
    }
    status = av2h_cdf_init(
        cdfs->secondary_4x4, AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
        secondary_4x4, AV2H_SECONDARY_TX_TYPE_SYMBOLS, 75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->secondary_32x32, AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
        secondary_32x32, AV2H_SECONDARY_TX_TYPE_SYMBOLS, 76U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->secondary_64x64, AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
        secondary_64x64, AV2H_SECONDARY_TX_TYPE_SYMBOLS, 1U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->secondary_8x8, AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
        secondary_8x8, AV2H_SECONDARY_TX_TYPE_SYMBOLS, 5U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->secondary_16x16, AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
        secondary_16x16, AV2H_SECONDARY_TX_TYPE_SYMBOLS, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->secondary_set, AV2H_SECONDARY_TX_SET_SYMBOLS + 4U,
        secondary_set, AV2H_SECONDARY_TX_SET_SYMBOLS, 55U, 0U);
    if (status != AV2H_OK) return status;
    {
        static const uint16_t secondary_adst_set[3U] = {
            15897U, 26144U, 30010U
        };
        status = av2h_cdf_init(
            cdfs->secondary_adst_set,
            AV2H_SECONDARY_ADST_TX_SET_SYMBOLS + 4U,
            secondary_adst_set, AV2H_SECONDARY_ADST_TX_SET_SYMBOLS, 0U, 0U);
        if (status != AV2H_OK) return status;
    }
    return av2h_cdf_init(cdfs->chroma_cross_type, 7U + 4U,
                         chroma_cross_type, 7U, 62U, 0U);
}

static Av2hStatus secondary_tx_set_map(
    Av2hFirstLeafIntraMode mode, uint8_t symbol, uint8_t *set) {
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
    if (set == 0) return AV2H_INVALID_ARGUMENT;
    if ((uint8_t)mode >= 12U || symbol >= AV2H_SECONDARY_TX_SET_SYMBOLS) {
        return AV2H_UNSUPPORTED;
    }
    *set = inverse_mapping[(uint8_t)mode][symbol];
    return AV2H_OK;
}

static Av2hStatus secondary_adst_tx_set_map(
    Av2hFirstLeafIntraMode mode, uint8_t symbol, uint8_t *set) {
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
    if (set == 0) return AV2H_INVALID_ARGUMENT;
    if ((uint8_t)mode >= 12U ||
        symbol >= AV2H_SECONDARY_ADST_TX_SET_SYMBOLS) {
        return AV2H_UNSUPPORTED;
    }
    *set = inverse_mapping[(uint8_t)mode][symbol];
    return AV2H_OK;
}

Av2hStatus av2h_primary_tx_type_map(
    Av2hFirstTxSize tx_size,
    Av2hFirstLeafIntraMode mode,
    uint8_t long_side_symbol,
    uint8_t primary_symbol,
    Av2hFirstTxType *tx_type) {
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
    if (tx_type == 0) return AV2H_INVALID_ARGUMENT;
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
        *tx_type = (Av2hFirstTxType)
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
        if (primary_symbol == 2U) return AV2H_UNSUPPORTED;
        if (wide) {
            *tx_type = primary_symbol == 0U ? AV2H_TX_V_DCT :
                primary_symbol == 1U ? AV2H_TX_V_ADST : AV2H_TX_IDTX;
        } else {
            *tx_type = primary_symbol == 0U ? AV2H_TX_H_DCT :
                primary_symbol == 1U ? AV2H_TX_H_ADST :
                primary_symbol == 3U ? AV2H_TX_IDTX :
                                       AV2H_TX_TYPE_INVALID;
            if (*tx_type == AV2H_TX_TYPE_INVALID) return AV2H_UNSUPPORTED;
        }
        return AV2H_OK;
    }
    return AV2H_UNSUPPORTED;
}

Av2hStatus av2h_secondary_tx_type_signal(
    const Av2hSequenceState *sequence,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTxType primary_type,
    uint8_t *signaled) {
    const Av2hTxDescriptor *descriptor;
    uint8_t size_class;
    uint16_t eob_limit;
    Av2hStatus status;
    if (sequence == 0 || mode == 0 || coeff == 0 || signaled == 0) {
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

static Av2hFirstLeafIntraMode transform_mode(
    Av2hFirstLeafIntraMode mode, int8_t angle_delta,
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

static Av2hStatus read_primary(Av2hRangeDecoder *decoder,
                               Av2hFirstTransformCdfs *cdfs,
                               const Av2hFirstLeafMode *mode,
                               const Av2hFirstCoeffPrefix *coeff,
                               Av2hFirstTransformType *transform) {
    const Av2hTxDescriptor *descriptor;
    Av2hFirstLeafMode mapped_mode;
    uint32_t symbol;
    Av2hStatus status;
    transform->primary_signaled = 0U;
    transform->primary_symbol = 0U;
    transform->primary_long_side_symbol = 0U;
    transform->primary_type = AV2H_TX_DCT_DCT;
    av2h_debug_transform_stage = 2U;
    status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    mapped_mode = *mode;
    mapped_mode.mode = transform_mode(
        mode->mode, mode->angle_delta, mode->mrl_index, descriptor);
    mode = &mapped_mode;
    if (descriptor->signaling_class == AV2H_TX_SIGNAL_DCT_ONLY) {
        return av2h_primary_tx_type_map(
            coeff->tx_size, mode->mode, 0U, 0U, &transform->primary_type);
    }
    if (coeff->tx_size == AV2H_TX_8X8 &&
        mode->mode == AV2H_INTRA_D203) {
        static const Av2hFirstTxType d203_size1[] = {
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
        static const Av2hFirstTxType d203_size1[] = {
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
            decoder, cdfs->primary_long_side_32, 2U);
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
            av2h_debug_transform_stage = 3U;
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
        static const Av2hFirstTxType intrabc_types[] = {
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
        av2h_debug_transform_stage = 32U;
        transform->primary_long_side_symbol = (uint8_t)set;
        transform->primary_symbol = (uint8_t)symbol;
        if (signaling_set2) {
            static const Av2hFirstTxType set2_types[12U] = {
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
            av2h_debug_transform_stage = 33U;
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
            static const Av2hFirstTxType dc_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = dc_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_D67) {
            static const Av2hFirstTxType d67_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_FLIPADST_FLIPADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d67_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_D45) {
            static const Av2hFirstTxType d45_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_PAETH) {
            static const Av2hFirstTxType paeth_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_V_DCT, AV2H_TX_H_DCT, AV2H_TX_H_ADST
            };
            transform->primary_type = paeth_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
            mode->mode == AV2H_INTRA_D135) {
            static const Av2hFirstTxType d135_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d135_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL) {
            static const Av2hFirstTxType horizontal_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_ADST, AV2H_TX_H_DCT,
                AV2H_TX_H_ADST
            };
            transform->primary_type = horizontal_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16 &&
                   mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
            static const Av2hFirstTxType smooth_horizontal_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = smooth_horizontal_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X16) {
            static const Av2hFirstTxType d203_4x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_DCT, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d203_4x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8 &&
                   mode->mode == AV2H_INTRA_VERTICAL) {
            static const Av2hFirstTxType vertical_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_V_DCT,
                AV2H_TX_V_ADST
            };
            transform->primary_type = vertical_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL) {
            static const Av2hFirstTxType horizontal_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_ADST, AV2H_TX_H_DCT,
                AV2H_TX_H_ADST
            };
            transform->primary_type = horizontal_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8 &&
                   mode->mode == AV2H_INTRA_D45) {
            static const Av2hFirstTxType d45_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X8) {
            static const Av2hFirstTxType dc_4x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = dc_4x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X4 &&
                   mode->mode == AV2H_INTRA_D135) {
            static const Av2hFirstTxType d135_8x4[] = {
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
            static const Av2hFirstTxType d67_16x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_DCT_FLIPADST, AV2H_TX_FLIPADST_FLIPADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d67_16x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_D67) {
            static const Av2hFirstTxType d67_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_V_DCT,
                AV2H_TX_V_ADST
            };
            transform->primary_type = d67_4x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_VERTICAL) {
            static const Av2hFirstTxType vertical_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_V_DCT,
                AV2H_TX_V_ADST
            };
            transform->primary_type = vertical_4x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_D157) {
            static const Av2hFirstTxType d157_4x4[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_ADST_FLIPADST, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_H_ADST
            };
            transform->primary_type = d157_4x4[symbol];
        } else if (coeff->tx_size == AV2H_TX_4X4 &&
                   mode->mode == AV2H_INTRA_HORIZONTAL) {
            static const Av2hFirstTxType horizontal_4x4[] = {
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
            static const Av2hFirstTxType d203_8x16[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_DCT, AV2H_TX_FLIPADST_ADST,
                AV2H_TX_ADST_FLIPADST
            };
            transform->primary_type = d203_8x16[symbol];
        } else if (coeff->tx_size == AV2H_TX_16X8 &&
            mode->mode == AV2H_INTRA_D45) {
            static const Av2hFirstTxType d45_16x8[] = {
                AV2H_TX_DCT_DCT, AV2H_TX_ADST_ADST,
                AV2H_TX_ADST_DCT, AV2H_TX_DCT_ADST,
                AV2H_TX_FLIPADST_FLIPADST, AV2H_TX_ADST_FLIPADST,
                AV2H_TX_FLIPADST_ADST
            };
            transform->primary_type = d45_16x8[symbol];
        } else if (coeff->tx_size == AV2H_TX_8X16 &&
            mode->mode == AV2H_INTRA_D45) {
            static const Av2hFirstTxType d45_8x16[] = {
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
            static const Av2hFirstTxType d135_8x8[] = {
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

Av2hStatus av2h_first_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform) {
    uint32_t symbol;
    const Av2hTxDescriptor *descriptor;
    uint8_t secondary_signaled;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        mode == 0 || coeff == 0 || transform == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_debug_transform_stage = 1U;
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
    av2h_debug_transform_stage = 4U;
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
    av2h_debug_transform_stage = 5U;
    if (secondary_signaled) {
        symbol = av2h_range_read_adaptive(
            decoder,
            descriptor->square_class == AV2H_TX_SQUARE_4 ?
                cdfs->secondary_4x4 :
            descriptor->square_class == AV2H_TX_SQUARE_8 ?
                cdfs->secondary_8x8 :
            descriptor->square_class == AV2H_TX_SQUARE_16 ?
                cdfs->secondary_16x16 :
            descriptor->square_class == AV2H_TX_SQUARE_32 ?
                cdfs->secondary_32x32 : cdfs->secondary_64x64,
            AV2H_SECONDARY_TX_TYPE_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        transform->secondary_signaled = 1U;
        transform->secondary_symbol = (uint8_t)symbol;
        transform->after_secondary_type_bits = av2h_range_tell(decoder);
        if (symbol != 0U) {
            uint8_t reduced_adst_set = (uint8_t)(
                transform->primary_type == AV2H_TX_ADST_ADST &&
                descriptor->output_width >= 8U &&
                descriptor->output_height >= 8U);
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
            status = reduced_adst_set ? secondary_adst_tx_set_map(
                mode->mode, (uint8_t)symbol, &transform->secondary_set) :
                secondary_tx_set_map(
                    mode->mode, (uint8_t)symbol, &transform->secondary_set);
            if (status != AV2H_OK) return status;
            if (transform->primary_type == AV2H_TX_ADST_ADST) {
                transform->secondary_set += 7U;
            }
        }
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

static Av2hFirstLeafIntraMode chroma_transform_mode(
    const Av2hFirstChromaMode *mode,
    const Av2hTxDescriptor *descriptor) {
    return transform_mode(mode->mode, mode->angle_delta, 0U, descriptor);
}

static Av2hStatus chroma_primary_type(
    const Av2hFirstChromaMode *mode,
    const Av2hTxDescriptor *descriptor,
    uint8_t reduced_tx_set,
    Av2hFirstTxType *tx_type) {
    static const Av2hFirstTxType mode_types[13U] = {
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
    if (mode == 0 || descriptor == 0 || tx_type == 0 ||
        (uint8_t)mode->mode >= 13U) {
        return AV2H_INVALID_ARGUMENT;
    }
    *tx_type = mode_types[(uint8_t)chroma_transform_mode(mode, descriptor)];
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

Av2hStatus av2h_chroma_transform_type_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstTransformCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hFirstChromaMode *mode,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *coeff,
    Av2hFirstTransformType *transform) {
    const Av2hTxDescriptor *descriptor;
    uint32_t symbol = 0U;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || frame == 0 || mode == 0 || coeff == 0 ||
        transform == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(coeff->tx_size, &descriptor);
    if ((plane == 1U || plane == 2U) && status == AV2H_OK) {
        transform->before_primary_bits = av2h_range_tell(decoder);
        status = chroma_primary_type(
            mode, descriptor, frame->reduced_tx_set,
            &transform->primary_type);
        if (status != AV2H_OK) return status;
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
            if (plane == 1U && coeff->eob != 1U) {
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