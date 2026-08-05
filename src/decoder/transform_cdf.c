#include "transform.h"

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
    static const Av2hBinaryCdfSpec intrabc_32x32_type = {
        16384U, 0U, 0U
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
    static const uint16_t intrabc_secondary[5U][3U] = {
        { 8192U, 16384U, 24576U },
        { 8192U, 16384U, 24576U },
        { 10490U, 15394U, 22206U },
        { 13967U, 16953U, 23109U },
        { 20191U, 21727U, 25818U }
    };
    static const uint8_t intrabc_secondary_parameter[5U] = {
        0U, 0U, 31U, 31U, 32U
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
    status = av2h_binary_cdf_init(
        cdfs->intrabc_32x32_type, intrabc_32x32_type);
    if (status != AV2H_OK) return status;
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
    for (size_t index = 0U; index < 5U; ++index) {
        status = av2h_cdf_init(
            cdfs->intrabc_secondary[index],
            AV2H_SECONDARY_TX_TYPE_SYMBOLS + 4U,
            intrabc_secondary[index], AV2H_SECONDARY_TX_TYPE_SYMBOLS,
            intrabc_secondary_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
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