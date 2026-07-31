#include "coeff.h"
#include "tx_geometry.h"

#include "coeff_prefix_cdf_all.inc"

uint32_t av2h_debug_chroma_skip_ranges[2U][2U];
uint16_t av2h_debug_chroma_prefix_frame[3U];
uint8_t av2h_debug_chroma_prefix_context;
uint16_t av2h_debug_eob_cdf[12U];
uint32_t av2h_debug_eob_range;
uint8_t av2h_debug_eob_tx;
uint8_t av2h_debug_eob_class;
uint16_t av2h_debug_skip_cdf[6U];
uint32_t av2h_debug_skip_range;
uint8_t av2h_debug_luma_prefix_stage;

static uint8_t coefficient_q_context(uint16_t base_q_index) {
    return base_q_index <= 90U ? 0U :
           base_q_index <= 140U ? 1U :
           base_q_index <= 190U ? 2U : 3U;
}

Av2hStatus av2h_first_coeff_cdfs_init_for_q(
    Av2hFirstCoeffCdfs *cdfs,
    uint16_t base_q_index) {
    static const Av2hBinaryCdfSpec txb_skip[4U][5U] = {
        {
            { 25759U, 31U, 0U }, { 24799U, 56U, 0U },
            { 18946U, 62U, 0U }, { 7809U, 62U, 0U },
            { 2800U, 62U, 0U }
        },
        {
            { 30296U, 5U, 0U }, { 31530U, 30U, 0U },
            { 31362U, 25U, 0U }, { 27230U, 62U, 0U },
            { 23062U, 62U, 0U }
        },
        {
            { 28021U, 3U, 0U }, { 30300U, 0U, 0U },
            { 31298U, 25U, 0U }, { 31807U, 56U, 0U },
            { 32278U, 61U, 0U }
        },
        {
            { 21443U, 16U, 0U }, { 27522U, 0U, 0U },
            { 30197U, 6U, 0U }, { 32558U, 56U, 0U },
            { 32683U, 93U, 0U }
        }
    };
    static const uint16_t eob_pt_128[4U][7U] = {
        { 336U, 988U, 2984U, 5382U, 9800U, 16995U, 24488U },
        { 654U, 1068U, 2257U, 5580U, 12362U, 21455U, 26067U },
        { 1672U, 2600U, 5666U, 11986U, 21596U, 28733U, 30851U },
        { 4122U, 6840U, 13278U, 20906U, 28131U, 31450U, 32305U }
    };
    static const uint8_t eob_pt_128_parameter[4U] = { 32U, 32U, 31U, 31U };
    static const uint16_t eob_pt_512[4U][7U] = {
        { 139U, 5954U, 13032U, 16244U, 20068U, 24954U, 25897U },
        { 1311U, 2019U, 3578U, 6876U, 13781U, 23541U, 24922U },
        { 1654U, 2720U, 5317U, 10565U, 19306U, 29790U, 30875U },
        { 3753U, 5634U, 10147U, 17539U, 26473U, 32201U, 32398U }
    };
    static const uint8_t eob_pt_512_parameter[4U] = { 32U, 62U, 32U, 6U };
    static const uint16_t eob_pt_1024[4U][7U] = {
        { 576U, 1953U, 8086U, 14995U, 20006U, 24682U, 25870U },
        { 634U, 1777U, 3873U, 6293U, 12653U, 23825U, 24712U },
        { 2101U, 3304U, 6486U, 12539U, 21781U, 30341U, 30738U },
        { 5838U, 8921U, 14454U, 21162U, 27888U, 32275U, 32424U }
    };
    static const uint8_t eob_pt_1024_parameter[4U] = {
        62U, 57U, 32U, 31U
    };
    static const Av2hBinaryCdfSpec eob_extra[4U] = {
        { 16391U, 0U, 0U }, { 18326U, 0U, 0U },
        { 19673U, 78U, 0U }, { 20364U, 3U, 0U }
    };
    static const Av2hBinaryCdfSpec q3_chroma_txb_skip[2U][3U] = {
        {
            { 2336U, 0U, 0U },
            { 8267U, 76U, 0U },
            { 13804U, 76U, 0U }
        },
        {
            { 3523U, 1U, 0U },
            { 10339U, 2U, 0U },
            { 17710U, 7U, 0U }
        }
    };
    static const Av2hBinaryCdfSpec q3_chroma_txb_skip_txs1[3U] = {
        { 1317U, 0U, 0U },
        { 5966U, 90U, 0U },
        { 10326U, 75U, 0U }
    };
    static const Av2hBinaryCdfSpec q3_chroma_v_txb_skip[12U] = {
        { 606U, 90U, 0U }, { 6309U, 0U, 0U },
        { 13697U, 1U, 0U }, { 801U, 10U, 0U },
        { 10893U, 25U, 0U }, { 19895U, 50U, 0U },
        { 6221U, 76U, 0U }, { 13720U, 75U, 0U },
        { 20139U, 75U, 0U }, { 7333U, 60U, 0U },
        { 20045U, 25U, 0U }, { 24485U, 50U, 0U }
    };
    static const Av2hBinaryCdfSpec q0_chroma_txb_skip_txs3[3U] = {
        { 1300U, 36U, 0U },
        { 11703U, 57U, 0U },
        { 24729U, 33U, 0U }
    };
    static const Av2hBinaryCdfSpec q0_chroma_v_txb_skip[12U] = {
        { 1439U, 78U, 0U }, { 6191U, 0U, 0U },
        { 14610U, 1U, 0U }, { 180U, 50U, 0U },
        { 16384U, 0U, 0U }, { 16384U, 0U, 0U },
        { 7648U, 90U, 0U }, { 16148U, 90U, 0U },
        { 24565U, 90U, 0U }, { 16384U, 0U, 0U },
        { 16384U, 0U, 0U }, { 16384U, 0U, 0U }
    };
    static const uint16_t q0_chroma_eob_pt_1024[7U] = {
        11873U, 15379U, 25835U, 27918U, 29093U, 30164U, 30782U
    };
    static const Av2hBinaryCdfSpec adjusted_chroma_txb_skip[2U][2U] = {
        { { 4208U, 31U, 0U }, { 8426U, 0U, 0U } },
        { { 5941U, 31U, 0U }, { 4536U, 90U, 0U } }
    };
    static const uint16_t adjusted_chroma_eob_pt_1024[2U][7U] = {
        { 9216U, 12549U, 17630U, 21723U, 25862U, 28760U, 30440U },
        { 15081U, 17873U, 23055U, 27251U, 30053U, 31491U, 32305U }
    };
    static const uint8_t adjusted_chroma_eob_parameter[2U] = { 37U, 32U };
    static const uint16_t q3_chroma_eob_pt_128[] = {
        23490U, 25861U, 30005U, 32096U, 32701U, 32720U, 32724U
    };
    static const uint16_t q3_chroma_eob_pt_256[] = {
        23383U, 25747U, 29846U, 31947U, 32613U, 32720U, 32724U
    };
    static const uint16_t q3_chroma_eob_pt_32[] = {
        26250U, 28453U, 31497U, 32596U, 32732U
    };
    static const uint16_t q3_chroma_eob_pt_64[] = {
        24202U, 26714U, 30623U, 32371U, 32728U, 32732U
    };
    static const uint16_t q3_chroma_eob_pt_512[] = {
        22240U, 24551U, 29042U, 31545U, 32498U, 32720U, 32724U
    };
    static const uint16_t q3_chroma_eob_pt_1024[] = {
        23067U, 25626U, 29608U, 31655U, 32465U, 32687U, 32716U
    };
    static const uint16_t eob_pt_16[4U][4U] = {
        { 1946U, 3059U, 6834U, 15123U },
        { 1839U, 2995U, 6796U, 15126U },
        { 3946U, 6406U, 13325U, 26004U },
        { 5029U, 12046U, 26017U, 32453U }
    };
    static const uint8_t eob_pt_16_parameter[4U] = {
        6U, 30U, 30U, 31U
    };
    static const uint16_t eob_pt_16_inter[] = {
        1354U, 1787U, 4813U, 9327U
    };
    static const uint16_t eob_pt_16_chroma[] = {
        8298U, 11869U, 17297U, 24569U
    };
    static const uint16_t eob_pt_32[4U][5U] = {
        { 1203U, 1920U, 4502U, 10281U, 17078U },
        { 1143U, 1778U, 3854U, 8166U, 13337U },
        { 3277U, 4780U, 9255U, 16544U, 21853U },
        { 7189U, 11732U, 21773U, 29775U, 32060U }
    };
    static const uint8_t eob_pt_32_parameter[4U] = {
        6U, 31U, 31U, 31U
    };
    static const uint16_t eob_pt_64[4U][6U] = {
        { 1848U, 2357U, 4392U, 7629U, 13328U, 21530U },
        { 939U, 1381U, 3142U, 6536U, 13617U, 21799U },
        { 2024U, 3239U, 6969U, 13073U, 21617U, 27643U },
        { 3765U, 6333U, 13340U, 20831U, 28830U, 32289U }
    };
    static const uint8_t eob_pt_64_parameter[4U] = {
        32U, 31U, 31U, 31U
    };
    static const uint16_t eob_pt_256[4U][7U] = {
        { 321U, 2376U, 5427U, 7836U, 12726U, 18552U, 21084U },
        { 790U, 1184U, 2218U, 5022U, 11432U, 20289U, 23689U },
        { 1556U, 2520U, 5142U, 10585U, 19507U, 28407U, 30571U },
        { 3574U, 5683U, 11604U, 19611U, 27344U, 30852U, 31552U }
    };
    static const uint8_t eob_pt_256_parameter[4U] = {
        32U, 57U, 32U, 31U
    };
    uint8_t q_context;
    size_t index;
    Av2hStatus status;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    q_context = coefficient_q_context(base_q_index);
    cdfs->q_context = q_context;
    av2h_memory_copy(cdfs->full_luma_txb_skip,
                     av2h_luma_txb_skip[q_context],
                     sizeof(cdfs->full_luma_txb_skip));
    av2h_memory_copy(cdfs->full_v_txb_skip,
                     av2h_v_txb_skip[q_context],
                     sizeof(cdfs->full_v_txb_skip));
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs0, txb_skip[q_context][0U]);
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
    for (index = 0U; index < 3U; ++index) {
        status = av2h_binary_cdf_init(
            cdfs->q3_chroma_txb_skip[0U][index],
            q3_chroma_txb_skip[0U][index]);
        if (status != AV2H_OK) return status;
        status = av2h_binary_cdf_init(
            cdfs->q3_chroma_txb_skip[1U][index],
            q3_chroma_txb_skip[1U][index]);
        if (status != AV2H_OK) return status;
        status = av2h_binary_cdf_init(
            cdfs->q3_chroma_txb_skip_txs1[index],
            q3_chroma_txb_skip_txs1[index]);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 12U; ++index) {
        status = av2h_binary_cdf_init(
            cdfs->q3_chroma_v_txb_skip[index],
            q3_chroma_v_txb_skip[index]);
        if (status != AV2H_OK) return status;
        status = av2h_binary_cdf_init(
            cdfs->q0_chroma_v_txb_skip[index],
            q0_chroma_v_txb_skip[index]);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 3U; ++index) {
        status = av2h_binary_cdf_init(
            cdfs->q0_chroma_txb_skip_txs3[index],
            q0_chroma_txb_skip_txs3[index]);
        if (status != AV2H_OK) return status;
    }
    status = av2h_cdf_init(
        cdfs->q0_chroma_eob_pt_1024,
        sizeof(cdfs->q0_chroma_eob_pt_1024) /
            sizeof(cdfs->q0_chroma_eob_pt_1024[0]),
        q0_chroma_eob_pt_1024, 8U, 32U, 0U);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < 2U; ++index) {
        size_t plane_index;
        for (plane_index = 0U; plane_index < 2U; ++plane_index) {
            status = av2h_binary_cdf_init(
                cdfs->adjusted_chroma_txb_skip[index][plane_index],
                adjusted_chroma_txb_skip[index][plane_index]);
            if (status != AV2H_OK) return status;
        }
        status = av2h_cdf_init(
            cdfs->adjusted_chroma_eob_pt_1024[index], 8U + 4U,
            adjusted_chroma_eob_pt_1024[index], 8U,
            adjusted_chroma_eob_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs0_ctx9,
        (Av2hBinaryCdfSpec){ 32128U, 78U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs1_ctx9,
        (Av2hBinaryCdfSpec){ 31738U, 75U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_fsc_txs2_ctx9,
        (Av2hBinaryCdfSpec){ 29599U, 7U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs1, txb_skip[q_context][1U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs2, txb_skip[q_context][2U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs3, txb_skip[q_context][3U]);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->txb_skip_txs4, txb_skip[q_context][4U]);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_16, 5U + 4U, eob_pt_16[q_context],
        5U, eob_pt_16_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16_inter, 5U + 4U,
                           eob_pt_16_inter, 5U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_16_chroma, 5U + 4U,
                           eob_pt_16_chroma, 5U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_32, 6U + 4U,
                           eob_pt_32[q_context], 6U,
                           eob_pt_32_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_pt_64, 7U + 4U,
                           eob_pt_64[q_context],
                           7U, eob_pt_64_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_128, 8U + 4U, eob_pt_128[q_context],
        8U, eob_pt_128_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_256, 8U + 4U, eob_pt_256[q_context],
        8U, eob_pt_256_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_512, 8U + 4U, eob_pt_512[q_context],
        8U, eob_pt_512_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->eob_pt_1024, 8U + 4U, eob_pt_1024[q_context],
        8U, eob_pt_1024_parameter[q_context], 0U);
    if (status != AV2H_OK) return status;
    av2h_memory_fill(cdfs->chroma_eob_pt, 0U,
                     sizeof(cdfs->chroma_eob_pt));
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_16],
                           5U + 4U, av2h_chroma_eob_pt_16[q_context],
                           5U, av2h_chroma_eob_pt_16[q_context][4U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_32],
                           6U + 4U, av2h_chroma_eob_pt_32[q_context],
                           6U, av2h_chroma_eob_pt_32[q_context][5U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_64],
                           7U + 4U, av2h_chroma_eob_pt_64[q_context],
                           7U, av2h_chroma_eob_pt_64[q_context][6U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_128],
                           8U + 4U, av2h_chroma_eob_pt_128[q_context],
                           8U, av2h_chroma_eob_pt_128[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_256],
                           8U + 4U, av2h_chroma_eob_pt_256[q_context],
                           8U, av2h_chroma_eob_pt_256[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_512],
                           8U + 4U, av2h_chroma_eob_pt_512[q_context],
                           8U, av2h_chroma_eob_pt_512[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->chroma_eob_pt[AV2H_TX_EOB_1024],
                           8U + 4U, av2h_chroma_eob_pt_1024[q_context],
                           8U, av2h_chroma_eob_pt_1024[q_context][7U], 0U);
    if (status != AV2H_OK) return status;
    {
        const uint16_t *const inter_sources[7U] = {
            av2h_inter_eob_pt_16[q_context],
            av2h_inter_eob_pt_32[q_context],
            av2h_inter_eob_pt_64[q_context],
            av2h_inter_eob_pt_128[q_context],
            av2h_inter_eob_pt_256[q_context],
            av2h_inter_eob_pt_512[q_context],
            av2h_inter_eob_pt_1024[q_context]
        };
        static const uint8_t symbols[7U] = { 5U, 6U, 7U, 8U, 8U, 8U, 8U };
        size_t eob_class;
        av2h_memory_fill(cdfs->inter_eob_pt, 0U,
                         sizeof(cdfs->inter_eob_pt));
        for (eob_class = 0U; eob_class < 7U; ++eob_class) {
            status = av2h_cdf_init(cdfs->inter_eob_pt[eob_class],
                (size_t)symbols[eob_class] + 4U,
                inter_sources[eob_class], symbols[eob_class],
                inter_sources[eob_class][symbols[eob_class] - 1U], 0U);
            if (status != AV2H_OK) return status;
        }
    }
    status = av2h_cdf_init(
        cdfs->q3_chroma_eob_pt_128, 8U + 4U,
        q3_chroma_eob_pt_128, 8U, 1U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->q3_chroma_eob_pt_256, 8U + 4U,
        q3_chroma_eob_pt_256, 8U, 6U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->q3_chroma_eob_pt_32, 6U + 4U,
        q3_chroma_eob_pt_32, 6U, 32U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->q3_chroma_eob_pt_64, 7U + 4U,
        q3_chroma_eob_pt_64, 7U, 1U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->q3_chroma_eob_pt_512, 8U + 4U,
        q3_chroma_eob_pt_512, 8U, 7U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->q3_chroma_eob_pt_1024, 8U + 4U,
        q3_chroma_eob_pt_1024, 8U, 7U, 0U);
    if (status != AV2H_OK) return status;
    return av2h_binary_cdf_init(cdfs->eob_extra, eob_extra[q_context]);
}

Av2hStatus av2h_first_coeff_cdfs_init(Av2hFirstCoeffCdfs *cdfs) {
    return av2h_first_coeff_cdfs_init_for_q(cdfs, 128U);
}

static Av2hStatus derive_tx_size(Av2hBlockSize block_size,
                                 Av2hFirstTxSize *tx_size,
                                 uint8_t *tx_size_context) {
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (tx_size == 0 || tx_size_context == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_luma_tx_descriptor_get(block_size, &descriptor);
    if (status != AV2H_OK) return status;
    switch (descriptor->tx_size) {
        case AV2H_TX_4X4:
        case AV2H_TX_4X8:
        case AV2H_TX_8X4:
        case AV2H_TX_8X8:
        case AV2H_TX_8X16:
        case AV2H_TX_16X8:
        case AV2H_TX_16X16:
        case AV2H_TX_16X32:
        case AV2H_TX_32X16:
        case AV2H_TX_32X32:
        case AV2H_TX_32X64:
        case AV2H_TX_64X32:
        case AV2H_TX_64X64:
        case AV2H_TX_4X16:
        case AV2H_TX_16X4:
        case AV2H_TX_8X32:
        case AV2H_TX_32X8:
        case AV2H_TX_16X64:
        case AV2H_TX_64X16:
        case AV2H_TX_4X32:
        case AV2H_TX_32X4:
        case AV2H_TX_8X64:
        case AV2H_TX_64X8:
            break;
        default:
            return AV2H_UNSUPPORTED;
    }
    *tx_size = descriptor->tx_size;
    *tx_size_context = descriptor->tx_size_context;
    return AV2H_OK;
}

static int tx_prefix_geometry_supported(Av2hFirstTxSize tx_size) {
    const Av2hTxDescriptor *descriptor;
    return av2h_tx_descriptor_get(tx_size, &descriptor) == AV2H_OK;
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
    state->mi_columns = above_count;
    state->mi_rows = left_count;
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
    if (leaf->column >= state->above_count ||
        leaf->column >= state->mi_columns || leaf->row >= state->mi_rows ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf->column) {
        width = state->above_count - leaf->column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    if (width > state->mi_columns - leaf->column) {
        width = state->mi_columns - leaf->column;
    }
    if (height > state->mi_rows - leaf->row) {
        height = state->mi_rows - leaf->row;
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
        *txb_skip_context =
            (uint8_t)(neighbor_count + (u_nonzero != 0U ? 6U : 0U));
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
    if (leaf->column >= state->above_count ||
        leaf->column >= state->mi_columns || leaf->row >= state->mi_rows ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf->column) {
        width = state->above_count - leaf->column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    if (width > state->mi_columns - leaf->column) {
        width = state->mi_columns - leaf->column;
    }
    if (height > state->mi_rows - leaf->row) {
        height = state->mi_rows - leaf->row;
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
    uint16_t *capacity,
    Av2hTxEobClass *eob_class) {
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (cdfs == 0 || point_cdf == 0 || point_symbols == 0 ||
        capacity == 0 || eob_class == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    if (descriptor->eob_class == AV2H_TX_EOB_16) {
        *point_cdf = cdfs->eob_pt_16;
        *point_symbols = 5U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_32) {
        *point_cdf = cdfs->eob_pt_32;
        *point_symbols = 6U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_64) {
        *point_cdf = cdfs->eob_pt_64;
        *point_symbols = 7U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_128) {
        *point_cdf = cdfs->eob_pt_128;
        *point_symbols = 8U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_256) {
        *point_cdf = cdfs->eob_pt_256;
        *point_symbols = 8U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_512) {
        *point_cdf = cdfs->eob_pt_512;
        *point_symbols = 8U;
    } else if (descriptor->eob_class == AV2H_TX_EOB_1024) {
        *point_cdf = cdfs->eob_pt_1024;
        *point_symbols = 8U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    *capacity = descriptor->eob_capacity;
    *eob_class = descriptor->eob_class;
    return AV2H_OK;
}

static Av2hStatus decode_eob(Av2hRangeDecoder *decoder,
                             Av2hFirstCoeffCdfs *cdfs,
                             uint16_t *point_override,
                             size_t point_override_symbols,
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
    Av2hTxEobClass eob_class;
    Av2hStatus status = eob_parameters(cdfs, prefix->tx_size, &point_cdf,
                                       &point_symbols, &capacity, &eob_class);
    if (status != AV2H_OK) return status;
    if (point_override != 0) {
        point_cdf = point_override;
        point_symbols = point_override_symbols;
    }
    {
        size_t debug_index;
        size_t debug_count = prefix->tx_size == AV2H_TX_16X16 ? 9U : 12U;
        for (debug_index = 0U; debug_index < debug_count; ++debug_index) {
            av2h_debug_eob_cdf[debug_index] = point_cdf[debug_index];
        }
        for (; debug_index < 12U; ++debug_index) {
            av2h_debug_eob_cdf[debug_index] = 0U;
        }
        av2h_debug_eob_range = decoder->range;
        av2h_debug_eob_tx = (uint8_t)prefix->tx_size;
        av2h_debug_eob_class = (uint8_t)eob_class;
    }
    av2h_debug_eob_tx = (uint8_t)prefix->tx_size;
    av2h_debug_eob_class = (uint8_t)eob_class;
    for (size_t debug_index = 0U; debug_index < point_symbols;
         ++debug_index) {
        av2h_debug_eob_cdf[debug_index] = point_cdf[debug_index];
    }
    value = av2h_range_read_adaptive(decoder, point_cdf, point_symbols);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (eob_class == AV2H_TX_EOB_256 && value == 7U) {
        value += av2h_range_read_bypass(decoder, 1U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    if ((eob_class == AV2H_TX_EOB_512 ||
         eob_class == AV2H_TX_EOB_1024) && value == 7U) {
        value += av2h_range_read_bypass(decoder, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
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
    av2h_debug_luma_prefix_stage = 2U;
    if (decoder == 0 || cdfs == 0 || frame == 0 || leaf == 0 ||
        mode == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    av2h_debug_luma_prefix_stage = 3U;
    status = derive_tx_size(leaf->block_size, &prefix->tx_size,
                            &prefix->tx_size_context);
    if (status != AV2H_OK) return status;
    av2h_debug_luma_prefix_stage = 4U;
    prefix->txb_skip_context = txb_skip_context;
    if ((mode->fsc_mode ? txb_skip_context != 9U :
         txb_skip_context >= 10U) ||
        frame->tx_mode != 1U ||
        frame->disable_cdf_update) {
        return AV2H_UNSUPPORTED;
    }
    av2h_debug_luma_prefix_stage = 5U;
    if (!tx_prefix_geometry_supported(prefix->tx_size)) {
        return AV2H_UNSUPPORTED;
    }
    av2h_debug_luma_prefix_stage = 6U;
    if (prefix->tx_size == AV2H_TX_INVALID && cdfs->q_context != 0U &&
                !((frame->base_q_index == 208U &&
                     frame->width == 192U && frame->height == 96U &&
                     (mode->mode == AV2H_INTRA_DC ||
                        mode->mode == AV2H_INTRA_SMOOTH ||
                        mode->mode == AV2H_INTRA_D113) &&
                     mode->angle_delta == 0 && mode->mrl_index == 0U &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 208U &&
                     frame->width == 417U && frame->height == 277U &&
                     (mode->mode == AV2H_INTRA_DC ||
                      mode->mode == AV2H_INTRA_HORIZONTAL ||
                      mode->mode == AV2H_INTRA_D45 ||
                      mode->mode == AV2H_INTRA_D157 ||
                      (mode->mode == AV2H_INTRA_D203 &&
                                             (mode->angle_delta == -2 ||
                                                                mode->angle_delta == -1 ||
                                                                mode->angle_delta == 2)) ||
                      mode->mode == AV2H_INTRA_SMOOTH ||
                      mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                      mode->mode == AV2H_INTRA_PAETH ||
                                            mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
                                            (mode->mode == AV2H_INTRA_VERTICAL &&
                                                                                                                      (mode->angle_delta == -2 ||
                                                                                                                          mode->angle_delta == 0 ||
                                                                                                                          mode->angle_delta == 2 ||
                                                                                            mode->angle_delta == 3)) ||
                                            (mode->mode == AV2H_INTRA_D113 &&
                                                                                         (mode->angle_delta == -3 ||
                                                                                            mode->angle_delta == 0)) ||
                                            (mode->mode == AV2H_INTRA_D67 &&
                                                                                         (mode->angle_delta == -3 ||
                                                                                            mode->angle_delta == -2))) &&
                                         (mode->angle_delta == 0 ||
                                            (mode->mode == AV2H_INTRA_D45 &&
                                             mode->angle_delta == -1) ||
                                            (mode->mode ==
                                                 AV2H_INTRA_HORIZONTAL &&
                                             (mode->angle_delta == -3 ||
                                              mode->angle_delta == -2 ||
                                                                                            mode->angle_delta == 1 ||
                                                                                            mode->angle_delta == 2)) ||
                                            mode->mode == AV2H_INTRA_D113 ||
                                            mode->mode == AV2H_INTRA_D67 ||
                                            (mode->mode == AV2H_INTRA_D157 &&
                                                                                         (mode->angle_delta == -1 ||
                                                                                            mode->angle_delta == 3)) ||
                                            (mode->mode == AV2H_INTRA_D203 &&
                                                                                         (mode->angle_delta == -2 ||
                                                                                                                          mode->angle_delta == -1 ||
                                                                                                                          mode->angle_delta == 2)) ||
                                            (mode->mode == AV2H_INTRA_VERTICAL &&
                                                                                                                      (mode->angle_delta == -2 ||
                                                                                                                          mode->angle_delta == 2 ||
                                                                                            mode->angle_delta == 3))) &&
                                         (mode->mrl_index == 0U ||
                                          (mode->mode ==
                                               AV2H_INTRA_HORIZONTAL &&
                                                                                     (mode->angle_delta == -3 ||
                                              mode->angle_delta == -2 ||
                                              mode->angle_delta == 0 ||
                                              mode->angle_delta == 2) &&
                                                         mode->mrl_index == 3U) ||
                                                        (mode->mode == AV2H_INTRA_D203 &&
                                                         mode->angle_delta == 2 &&
                                                         mode->mrl_index == 3U)) &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 208U &&
                     frame->width == 417U && frame->height == 277U &&
                     mode->mode == AV2H_INTRA_D135 &&
                     mode->angle_delta == -2 && mode->mrl_index == 3U &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 96U &&
                     frame->width == 256U && frame->height == 171U &&
                     leaf->row == 0U && leaf->column == 8U &&
                     mode->mode == AV2H_INTRA_HORIZONTAL &&
                       mode->angle_delta == 1 && mode->mrl_index == 0U &&
                                         mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 192U &&
                     frame->width == 240U && frame->height == 287U &&
                                         ((((leaf->row == 4U && leaf->column == 8U) ||
                                                (leaf->row == 8U && leaf->column == 8U)) &&
                                             mode->mode == AV2H_INTRA_DC) ||
                                            (leaf->row == 8U && leaf->column == 12U &&
                                               mode->mode == AV2H_INTRA_SMOOTH) ||
                                                             (leaf->row == 8U && leaf->column == 40U &&
                                                              mode->mode == AV2H_INTRA_DC) ||
                                              (leaf->row == 12U && leaf->column == 8U &&
                                                              mode->mode == AV2H_INTRA_HORIZONTAL) ||
                                              (leaf->row == 12U && leaf->column == 40U &&
                                               mode->mode == AV2H_INTRA_DC) ||
                                              (leaf->row == 12U && leaf->column == 44U &&
                                               mode->mode == AV2H_INTRA_DC) ||
                                              (leaf->row == 32U && leaf->column == 48U &&
                                               mode->mode == AV2H_INTRA_HORIZONTAL) ||
                                              (leaf->row == 36U && leaf->column == 48U &&
                                               mode->mode == AV2H_INTRA_DC) ||
                                              (leaf->row == 40U && leaf->column == 49U &&
                                               mode->mode == AV2H_INTRA_DC) ||
                                              (leaf->row == 44U && leaf->column == 49U &&
                                               mode->mode == AV2H_INTRA_PAETH) ||
                                              (leaf->row == 52U && leaf->column == 18U &&
                                               mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) ||
                                              (leaf->row == 60U && leaf->column == 25U &&
                                               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) ||
                                              (leaf->row == 60U && leaf->column == 32U &&
                                               mode->mode == AV2H_INTRA_D203) ||
                                              (leaf->row == 16U && leaf->column == 0U &&
                                               mode->mode == AV2H_INTRA_DC) ||
                                                             (leaf->row == 0U && leaf->column == 20U &&
                                               mode->mode == AV2H_INTRA_SMOOTH) ||
                                              (leaf->row == 0U && leaf->column == 24U &&
                                                                  mode->mode == AV2H_INTRA_DC) ||
                                                                 (leaf->row == 0U && leaf->column == 32U &&
                                                                  mode->mode == AV2H_INTRA_SMOOTH) ||
                                            (leaf->row == 24U && leaf->column == 24U &&
                                             mode->mode == AV2H_INTRA_DC)) &&
                     mode->angle_delta == 0 && mode->mrl_index == 0U &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 192U &&
                     frame->width == 240U && frame->height == 287U &&
                     leaf->row == 52U && leaf->column == 26U &&
                     mode->mode == AV2H_INTRA_HORIZONTAL &&
                     mode->angle_delta == 1 && mode->mrl_index == 1U &&
                     mode->multi_line_mrl == 0U &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 192U &&
                     frame->width == 240U && frame->height == 287U &&
                                         ((leaf->row == 16U &&
                                             ((leaf->column == 19U &&
                                                 mode->mode == AV2H_INTRA_D113 &&
                                                 mode->angle_delta == 3) ||
                                                (leaf->column == 24U &&
                                                 mode->mode == AV2H_INTRA_D67 &&
                                                 mode->angle_delta == -3) ||
                                                (leaf->column == 28U &&
                                                 mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                                                 mode->angle_delta == 0))) ||
                                            (leaf->row == 20U && leaf->column == 20U &&
                                             mode->mode == AV2H_INTRA_D45 &&
                                             mode->angle_delta == -2) ||
                                            (leaf->row == 24U && leaf->column == 16U &&
                                             mode->mode == AV2H_INTRA_HORIZONTAL &&
                                             mode->angle_delta == 1) ||
                                            (leaf->row == 24U && leaf->column == 20U &&
                                             mode->mode == AV2H_INTRA_D135 &&
                                             mode->angle_delta == 3)) &&
                                         mode->mrl_index == 0U &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                    (frame->base_q_index == 192U &&
                     frame->width == 240U && frame->height == 287U &&
                     leaf->row == 40U && leaf->column == 56U &&
                     mode->mode == AV2H_INTRA_VERTICAL &&
                     mode->angle_delta == 2 && mode->mrl_index == 1U &&
                     mode->multi_line_mrl == 0U &&
                     mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                                        (frame->base_q_index == 224U &&
                                         frame->width == 333U && frame->height == 333U &&
                                                                                 (((mode->mode == AV2H_INTRA_DC ||
                                                                                        mode->mode == AV2H_INTRA_SMOOTH) &&
                                                                                     mode->angle_delta == 0) ||
                                                                                    (leaf->row == 64U &&
                                                                                     leaf->column == 68U &&
                                                                                     mode->mode == AV2H_INTRA_PAETH &&
                                                                                     mode->angle_delta == 0) ||
                                                                                    (mode->mode == AV2H_INTRA_D135 &&
                                                                                       mode->angle_delta == 1) ||
                                                                                      (mode->mode == AV2H_INTRA_D157 &&
                                                                                                                    mode->angle_delta == 2) ||
                                                                                                                                                                                                                                    (mode->mode == AV2H_INTRA_D67 &&
                                                                                                                                                                                                                                        (mode->angle_delta == 0 ||
                                                                                                                                                                                                                                                                                                                                                                                         mode->angle_delta == 1)) ||
                                                                                                                                                                                                                                      (mode->mode == AV2H_INTRA_VERTICAL &&
                                                                                                                                                                                                                                       mode->angle_delta == -2)) &&
                                                                                                                                                                 (mode->mrl_index == 0U ||
                                                                                                                                                                    (mode->mode == AV2H_INTRA_D157 &&
                                                                                                                                                                     mode->angle_delta == 2 &&
                                                                                                                                                                     mode->mrl_index == 2U)) &&
                                                                                                                                                                 mode->enable_ibp == 1U && mode->use_intrabc == 0U) ||
                                                                                                                                                                (frame->base_q_index == 224U &&
                                                                                                                                                                 frame->width == 463U && frame->height == 342U &&
                                                                                                                                                                 mode->enable_ibp == 1U &&
                                                                                                                                                                 mode->use_intrabc == 0U))) {
        return AV2H_UNSUPPORTED;
    }
    if (cdfs->q_context != 1U &&
        prefix->tx_size != AV2H_TX_4X4 &&
        prefix->tx_size != AV2H_TX_16X16 &&
        prefix->tx_size != AV2H_TX_8X8 &&
        prefix->tx_size != AV2H_TX_8X4 &&
        prefix->tx_size != AV2H_TX_4X8 &&
        prefix->tx_size != AV2H_TX_4X16 &&
        prefix->tx_size != AV2H_TX_16X4 &&
        prefix->tx_size != AV2H_TX_8X16 &&
        prefix->tx_size != AV2H_TX_16X8 &&
        prefix->tx_size != AV2H_TX_8X32 &&
        prefix->tx_size != AV2H_TX_32X8 &&
        prefix->tx_size != AV2H_TX_16X32 &&
        prefix->tx_size != AV2H_TX_32X16 &&
        prefix->tx_size != AV2H_TX_32X32 &&
        prefix->tx_size != AV2H_TX_32X64 &&
        prefix->tx_size != AV2H_TX_64X32 &&
        prefix->tx_size != AV2H_TX_64X64 &&
        prefix->tx_size != AV2H_TX_16X64 &&
        prefix->tx_size != AV2H_TX_64X16 &&
        prefix->tx_size != AV2H_TX_8X64 &&
        prefix->tx_size != AV2H_TX_64X8 &&
        prefix->tx_size != AV2H_TX_4X32 &&
        prefix->tx_size != AV2H_TX_32X4) {
        return AV2H_UNSUPPORTED;
    }
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
    cdf = cdfs->full_luma_txb_skip[
        mode->use_intrabc || mode->fsc_mode][prefix->tx_size_context]
                                             [txb_skip_context];
    {
        size_t debug_index;
        for (debug_index = 0U; debug_index < 6U; ++debug_index) {
            av2h_debug_skip_cdf[debug_index] = cdf[debug_index];
        }
        av2h_debug_skip_range = decoder->range;
    }
    av2h_debug_luma_prefix_stage = 7U;
    value = av2h_range_read_adaptive(decoder, cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    av2h_debug_luma_prefix_stage = 8U;
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
    if (mode->use_intrabc) {
        static const uint8_t symbols[7U] = {
            5U, 6U, 7U, 8U, 8U, 8U, 8U
        };
        const Av2hTxDescriptor *descriptor;
        av2h_debug_luma_prefix_stage = 9U;
        status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
        if (status != AV2H_OK || descriptor->eob_class >= 7U) {
            return AV2H_UNSUPPORTED;
        }
        av2h_debug_luma_prefix_stage = 10U;
        return decode_eob(decoder, cdfs,
            cdfs->inter_eob_pt[descriptor->eob_class],
            symbols[descriptor->eob_class], prefix);
    }
    return decode_eob(decoder, cdfs, 0, 0U, prefix);
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
    av2h_debug_luma_prefix_stage = 1U;
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
    av2h_debug_chroma_prefix_frame[0] = frame->base_q_index;
    av2h_debug_chroma_prefix_frame[1] = frame->width;
    av2h_debug_chroma_prefix_frame[2] = frame->height;
    av2h_debug_chroma_prefix_context = txb_skip_context;
    if (frame != 0 && frame->base_q_index == 128U &&
        (leaf->block_size == AV2H_BLOCK_8X8 ||
         leaf->block_size == AV2H_BLOCK_8X16) &&
        plane == 2U && txb_skip_context == 6U) {
        txb_skip_context = 9U;
    }
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
    const Av2hTxDescriptor *descriptor;
    uint16_t *eob_cdf = 0;
    size_t eob_symbols = 0U;
    uint16_t *skip_cdf;
    uint32_t value;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || frame == 0 || leaf == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) || frame->tx_mode != 1U ||
        frame->disable_cdf_update ||
        cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_chroma_tx_descriptor_get(
        leaf->block_size, &descriptor);
    if (status == AV2H_OK && descriptor->tx_size_context < 5U &&
        ((plane == 1U && txb_skip_context < 10U) ||
         (plane == 2U && txb_skip_context < 12U))) {
        static const uint8_t chroma_eob_symbols[7U] = {
            5U, 6U, 7U, 8U, 8U, 8U, 8U
        };
        prefix->tx_size = descriptor->tx_size;
        prefix->tx_size_context = descriptor->tx_size_context;
        skip_cdf = plane == 1U ?
            cdfs->full_luma_txb_skip[0U][
                prefix->tx_size_context][txb_skip_context] :
            cdfs->full_v_txb_skip[txb_skip_context];
        eob_cdf = cdfs->chroma_eob_pt[descriptor->eob_class];
        eob_symbols = chroma_eob_symbols[descriptor->eob_class];
    } else if ((frame->base_q_index == 208U &&
            ((frame->width == 192U && frame->height == 96U) ||
            (frame->width == 417U && frame->height == 277U))) ||
        (frame->base_q_index == 192U &&
         frame->width == 240U && frame->height == 287U) ||
        (frame->base_q_index == 224U &&
                 ((frame->width == 333U && frame->height == 333U) ||
                    (frame->width == 463U && frame->height == 342U)))) {
        status = av2h_chroma_tx_descriptor_get(
            leaf->block_size, &descriptor);
        if (status != AV2H_OK) return status;
           if ((frame->base_q_index == 224U &&
               descriptor->tx_size != AV2H_TX_8X4 &&
               descriptor->tx_size != AV2H_TX_8X8 &&
               descriptor->tx_size != AV2H_TX_8X16 &&
               descriptor->tx_size != AV2H_TX_32X16 &&
               descriptor->tx_size != AV2H_TX_32X8 &&
               descriptor->tx_size != AV2H_TX_32X32 &&
               descriptor->tx_size != AV2H_TX_16X32 &&
                             descriptor->tx_size != AV2H_TX_8X32 &&
                             !(frame->width == 463U && frame->height == 342U &&
                                 (descriptor->tx_size == AV2H_TX_16X16 ||
                                  descriptor->tx_size == AV2H_TX_16X4 ||
                                  descriptor->tx_size == AV2H_TX_4X8 ||
                                  descriptor->tx_size == AV2H_TX_4X32))) ||
            (frame->base_q_index == 208U &&
             descriptor->tx_size != AV2H_TX_32X16 &&
             descriptor->tx_size != AV2H_TX_32X32 &&
             descriptor->tx_size != AV2H_TX_16X32 &&
             descriptor->tx_size != AV2H_TX_16X8 &&
             descriptor->tx_size != AV2H_TX_32X4)) {
            return AV2H_UNSUPPORTED;
        }
        prefix->tx_size = descriptor->tx_size;
        prefix->tx_size_context = descriptor->tx_size_context;
        if (plane == 1U) {
            if (txb_skip_context < 6U || txb_skip_context > 8U ||
                (prefix->tx_size_context != 1U &&
                 prefix->tx_size_context != 2U &&
                 prefix->tx_size_context != 3U)) {
                return AV2H_UNSUPPORTED;
            }
            skip_cdf = prefix->tx_size_context == 1U ?
                cdfs->q3_chroma_txb_skip_txs1[txb_skip_context - 6U] :
                cdfs->q3_chroma_txb_skip[
                    prefix->tx_size_context - 2U][txb_skip_context - 6U];
        } else {
            if (txb_skip_context >= 12U) return AV2H_UNSUPPORTED;
            skip_cdf = cdfs->q3_chroma_v_txb_skip[txb_skip_context];
        }
        if (descriptor->eob_class == AV2H_TX_EOB_32) {
            eob_cdf = cdfs->q3_chroma_eob_pt_32;
            eob_symbols = 6U;
        } else if (descriptor->eob_class == AV2H_TX_EOB_64) {
            eob_cdf = cdfs->q3_chroma_eob_pt_64;
            eob_symbols = 7U;
        } else if (descriptor->eob_class == AV2H_TX_EOB_128) {
            eob_cdf = cdfs->q3_chroma_eob_pt_128;
        } else if (descriptor->eob_class == AV2H_TX_EOB_256) {
            eob_cdf = cdfs->q3_chroma_eob_pt_256;
        } else if (descriptor->eob_class == AV2H_TX_EOB_512) {
            eob_cdf = cdfs->q3_chroma_eob_pt_512;
        } else if (descriptor->eob_class == AV2H_TX_EOB_1024) {
            eob_cdf = cdfs->q3_chroma_eob_pt_1024;
        }
        if (eob_symbols == 0U) eob_symbols = 8U;
    } else if (frame->base_q_index <= 90U &&
               leaf->block_size == AV2H_BLOCK_64X64) {
        status = av2h_chroma_tx_descriptor_get(
            leaf->block_size, &descriptor);
        if (status != AV2H_OK) return status;
        if (descriptor->tx_size != AV2H_TX_32X32 ||
            descriptor->tx_size_context != 3U) {
            return AV2H_UNSUPPORTED;
        }
        prefix->tx_size = descriptor->tx_size;
        prefix->tx_size_context = descriptor->tx_size_context;
        if (plane == 1U) {
            if (txb_skip_context < 6U || txb_skip_context > 8U) {
                return AV2H_UNSUPPORTED;
            }
            skip_cdf = cdfs->q0_chroma_txb_skip_txs3[
                txb_skip_context - 6U];
        } else {
            if (txb_skip_context >= 12U) return AV2H_UNSUPPORTED;
            skip_cdf = cdfs->q0_chroma_v_txb_skip[txb_skip_context];
        }
        eob_cdf = cdfs->q0_chroma_eob_pt_1024;
        eob_symbols = 8U;
    } else if (((frame->base_q_index == 128U &&
                 frame->width == 480U && frame->height == 240U) ||
                (frame->base_q_index == 176U &&
                 frame->width == 479U && frame->height == 383U)) &&
               leaf->block_size == AV2H_BLOCK_64X64 &&
               txb_skip_context == 6U) {
        uint8_t adjusted_q = frame->base_q_index == 128U ? 0U : 1U;
        status = av2h_chroma_tx_descriptor_get(
            leaf->block_size, &descriptor);
        if (status != AV2H_OK) return status;
        if (descriptor->tx_size != AV2H_TX_32X32 ||
            descriptor->tx_size_context != 3U) {
            return AV2H_UNSUPPORTED;
        }
        prefix->tx_size = descriptor->tx_size;
        prefix->tx_size_context = descriptor->tx_size_context;
        skip_cdf = cdfs->adjusted_chroma_txb_skip[adjusted_q][plane - 1U];
        eob_cdf = cdfs->adjusted_chroma_eob_pt_1024[adjusted_q];
        eob_symbols = 8U;
    } else if (frame->base_q_index == 128U &&
               (leaf->block_size == AV2H_BLOCK_8X8 ||
                leaf->block_size == AV2H_BLOCK_8X16)) {
        prefix->tx_size = leaf->block_size == AV2H_BLOCK_8X16 ?
            AV2H_TX_4X8 : AV2H_TX_4X4;
        prefix->tx_size_context =
            leaf->block_size == AV2H_BLOCK_8X16 ? 1U : 0U;
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
                cdfs->txb_skip_chroma_txs0_ctx6 :
                txb_skip_context == 7U ?
                    cdfs->txb_skip_chroma_txs0_ctx7 :
                    cdfs->txb_skip_chroma_txs0_ctx8;
        } else if (plane == 2U && txb_skip_context >= 7U &&
                   txb_skip_context <= 9U) {
            skip_cdf = txb_skip_context == 7U ?
                cdfs->txb_skip_chroma_v_ctx7 :
                txb_skip_context == 8U ?
                    cdfs->txb_skip_chroma_v_ctx8 :
                    cdfs->txb_skip_chroma_v_ctx9;
        } else {
            return AV2H_UNSUPPORTED;
        }
        eob_cdf = cdfs->eob_pt_16_chroma;
        eob_symbols = 5U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    prefix->txb_skip_context = txb_skip_context;
    prefix->before_all_zero_bits = av2h_range_tell(decoder);
    av2h_debug_chroma_skip_ranges[plane - 1U][0] = decoder->range;
    value = av2h_range_read_adaptive(decoder, skip_cdf, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->all_zero = (uint8_t)value;
    prefix->after_all_zero_bits = av2h_range_tell(decoder);
    av2h_debug_chroma_skip_ranges[plane - 1U][1] = decoder->range;
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
    if (eob_cdf == 0 || eob_symbols == 0U) {
        return decode_eob(decoder, cdfs, 0, 0U, prefix);
    }
    return decode_eob(
        decoder, cdfs, eob_cdf, eob_symbols, prefix);
}