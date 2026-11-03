#include "coeff_level.h"

#include "internal/coeff_cdf_specs.h"
#include "internal/coeff_common.h"
#include "internal/coeff_modules.h"

#include <stddef.h>

#include "coeff_cdf_all.inc"
#include "coeff_cdf_q0_txs012.inc"
#include "coeff_cdf_q0_txs3.inc"
#include "coeff_cdf_q0_txs4.inc"
#include "coeff_cdf_q0_uv.inc"
#include "fsc_cdf_data.inc"

static void initialize_full_level_cdfs(
    Av2hCoeffLevelCdfs *cdfs, uint8_t q_context) {
    av2h_memory_copy(cdfs->full_eob_base,
                     av2h_coeff_eob_base[q_context],
                     sizeof(cdfs->full_eob_base));
    av2h_memory_copy(cdfs->full_eob_base_lf,
                     av2h_coeff_eob_base_lf[q_context],
                     sizeof(cdfs->full_eob_base_lf));
    av2h_memory_copy(cdfs->full_base, av2h_coeff_base[q_context],
                     sizeof(cdfs->full_base));
    av2h_memory_copy(cdfs->full_base_lf, av2h_coeff_base_lf[q_context],
                     sizeof(cdfs->full_base_lf));
    av2h_memory_copy(cdfs->full_br, av2h_coeff_br[q_context],
                     sizeof(cdfs->full_br));
    av2h_memory_copy(cdfs->full_br_lf, av2h_coeff_br_lf[q_context],
                     sizeof(cdfs->full_br_lf));
    av2h_memory_copy(cdfs->full_uv_eob_base,
                     av2h_coeff_eob_base_uv[q_context],
                     sizeof(cdfs->full_uv_eob_base));
    av2h_memory_copy(cdfs->full_uv_eob_base_lf,
                     av2h_coeff_eob_base_lf_uv[q_context],
                     sizeof(cdfs->full_uv_eob_base_lf));
    av2h_memory_copy(cdfs->full_uv_base,
                     av2h_coeff_base_uv[q_context],
                     sizeof(cdfs->full_uv_base));
    av2h_memory_copy(cdfs->full_uv_base_lf,
                     av2h_coeff_base_lf_uv[q_context],
                     sizeof(cdfs->full_uv_base_lf));
    av2h_memory_copy(cdfs->full_uv_br,
                     av2h_coeff_br_uv[q_context],
                     sizeof(cdfs->full_uv_br));
}

static Av2hStatus initialize_fsc_cdfs(
    Av2hCoeffLevelCdfs *cdfs, uint8_t q_context) {
    size_t tx_context;
    size_t context;
    Av2hStatus status;
    if (q_context >= 4U) return AV2H_INVALID_DATA;
    for (tx_context = 0U; tx_context < 3U; ++tx_context) {
        size_t bank = (size_t)q_context * 3U + tx_context;
        for (context = 0U; context < 3U; ++context) {
            const Av2hFscCdfSpec *spec =
                &fsc_bob_specs[bank * 3U + context];
            status = av2h_cdf_init(
                cdfs->fsc_bob_all[tx_context][context], 7U,
                spec->threshold, 3U, spec->parameter, 0U);
            if (status != AV2H_OK) return status;
        }
        for (context = 0U; context < 7U; ++context) {
            const Av2hFscCdfSpec *base_spec =
                &fsc_base_specs[bank * 7U + context];
            const Av2hFscCdfSpec *br_spec =
                &fsc_br_specs[bank * 7U + context];
            status = av2h_cdf_init(
                cdfs->fsc_base_all[tx_context][context], 8U,
                base_spec->threshold, 4U, base_spec->parameter, 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->fsc_br_all[tx_context][context], 8U,
                br_spec->threshold, 4U, br_spec->parameter, 0U);
            if (status != AV2H_OK) return status;
        }
        for (context = 0U; context < 9U; ++context) {
            const Av2hFscCdfSpec *spec =
                &fsc_sign_specs[bank * 9U + context];
            status = av2h_cdf_init(
                cdfs->fsc_sign_all[tx_context][context],
                AV2H_BINARY_CDF_SIZE, spec->threshold, 2U,
                spec->parameter, 0U);
            if (status != AV2H_OK) return status;
        }
    }
    return AV2H_OK;
}

static const Av2hCoeffKey eob_base_keys[] = {
    { 0U, 3U }, { 1U, 3U }, { 3U, 1U }, { 1U, 2U }, { 2U, 3U },
    { 2U, 2U }, { 2U, 1U }
};
static const Av2hCoeffKey base_keys[] = {
    { 0U, 15U }, { 0U, 16U }, { 0U, 17U },
    { 1U, 1U }, { 1U, 2U }, { 1U, 3U }, { 1U, 4U }, { 1U, 5U },
    { 1U, 6U },
    { 1U, 7U },
    { 1U, 8U },
    { 1U, 9U },
    { 1U, 10U },
    { 1U, 11U },
    { 1U, 12U },
    { 1U, 13U },
    { 1U, 15U },
    { 1U, 16U },
    { 1U, 17U },
    { 1U, 18U },
    { 1U, 19U },
    { 3U, 0U }, { 3U, 1U }, { 3U, 4U }, { 3U, 5U }, { 3U, 6U },
    { 3U, 8U }, { 3U, 9U }, { 3U, 10U }, { 3U, 11U }, { 3U, 12U },
    { 3U, 13U }, { 3U, 14U },
    { 1U, 0U }, { 2U, 0U }, { 2U, 1U }, { 2U, 2U }, { 2U, 5U }, { 2U, 15U },
    { 2U, 16U }
};
static const Av2hCoeffKey base_lf_keys[] = {
    { 0U, 1U }, { 0U, 9U }, { 0U, 10U }, { 0U, 16U }, { 0U, 17U },
    { 0U, 21U },
    { 0U, 22U }, { 0U, 23U },
    { 0U, 24U }, { 0U, 25U }, { 0U, 26U },
    { 0U, 28U }, { 0U, 29U }, { 0U, 30U }, { 0U, 31U },
    { 1U, 1U }, { 1U, 3U }, { 1U, 4U }, { 1U, 5U }, { 1U, 8U },
    { 1U, 9U },
    { 1U, 10U },
    { 1U, 11U }, { 1U, 12U },
    { 1U, 13U }, { 1U, 14U },
    { 1U, 15U }, { 1U, 16U }, { 1U, 17U }, { 1U, 18U }, { 1U, 19U },
    { 1U, 20U },
    { 1U, 25U }, { 1U, 26U }, { 1U, 27U }, { 1U, 31U }, { 1U, 32U },
    { 3U, 2U }, { 3U, 5U }, { 3U, 8U }, { 3U, 9U }, { 3U, 10U },
    { 3U, 11U }, { 3U, 15U }, { 3U, 16U }, { 3U, 17U }, { 3U, 18U },
    { 3U, 6U }, { 3U, 13U }, { 3U, 20U },
    { 2U, 0U }, { 2U, 1U }, { 2U, 2U }, { 2U, 7U }, { 2U, 9U }, { 2U, 10U },
    { 2U, 13U },
    { 2U, 16U },
    { 2U, 17U }, { 2U, 18U }, { 2U, 19U }, { 2U, 20U }, { 2U, 21U },
    { 2U, 22U }, { 2U, 23U },
    { 2U, 28U }, { 2U, 29U }
};
static const uint8_t br_keys[] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U };
static const uint8_t br_lf_keys[] = {
    0U, 1U, 2U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U
};

static uint16_t *find_key(uint16_t *rows, size_t row_words,
                          const Av2hCoeffKey *keys, size_t count,
                          uint8_t tx_context, uint8_t context);

static const Av2hBaseSpec base_specs[] = {
    { 0U, 15U, { 27562U, 31970U, 32298U }, 75U },
    { 0U, 16U, { 11965U, 23262U, 28482U }, 75U },
    { 0U, 17U, { 6254U, 14791U, 20469U }, 78U },
    { 1U, 1U, { 18907U, 30472U, 32314U }, 90U },
    { 1U, 2U, { 13516U, 26908U, 30904U }, 75U },
    { 1U, 3U, { 8297U, 20991U, 27437U }, 78U },
    { 1U, 4U, { 3980U, 11626U, 17598U }, 3U },
    { 1U, 5U, { 26391U, 32035U, 32595U }, 75U },
    { 1U, 6U, { 19580U, 30651U, 32319U }, 75U },
    { 1U, 7U, { 13734U, 27193U, 31043U }, 0U },
    { 1U, 8U, { 8311U, 21239U, 27850U }, 3U },
    { 1U, 9U, { 4217U, 12744U, 19511U }, 3U },
    { 1U, 10U, { 25094U, 32069U, 32616U }, 5U },
    { 1U, 11U, { 19672U, 30814U, 32312U }, 75U },
    { 1U, 12U, { 14800U, 28188U, 31442U }, 0U },
    { 1U, 13U, { 8995U, 22820U, 28992U }, 3U },
    { 1U, 15U, { 30845U, 32589U, 32713U }, 0U },
    { 1U, 16U, { 19409U, 29802U, 31864U }, 75U },
    { 1U, 17U, { 11114U, 22871U, 27848U }, 0U },
    { 1U, 18U, { 6092U, 16202U, 22429U }, 5U },
    { 1U, 19U, { 3247U, 9679U, 15027U }, 90U },
    { 3U, 0U, { 28880U, 32680U, 32748U }, 0U },
    { 3U, 1U, { 24025U, 32081U, 32704U }, 75U },
    { 3U, 4U, { 3997U, 11545U, 16952U }, 75U },
    { 3U, 5U, { 30905U, 32733U, 32748U }, 75U },
    { 3U, 6U, { 26894U, 32528U, 32731U }, 76U },
    { 3U, 8U, { 10948U, 25238U, 30225U }, 0U },
    { 3U, 9U, { 3995U, 12602U, 18654U }, 0U },
    { 3U, 10U, { 31712U, 32750U, 32754U }, 75U },
    { 3U, 11U, { 28091U, 32642U, 32748U }, 75U },
    { 3U, 12U, { 22886U, 31816U, 32647U }, 0U },
    { 3U, 13U, { 13616U, 27730U, 31451U }, 0U },
    { 3U, 14U, { 6026U, 17154U, 23829U }, 75U },
    { 1U, 0U, { 25334U, 31967U, 32621U }, 90U },
    { 2U, 0U, { 27264U, 32622U, 32748U }, 75U },
    { 2U, 1U, { 22849U, 32019U, 32682U }, 75U },
    { 2U, 2U, { 17618U, 30067U, 32261U }, 75U },
    { 2U, 5U, { 29595U, 32710U, 32748U }, 75U },
    { 2U, 15U, { 32282U, 32726U, 32748U }, 15U },
    { 2U, 16U, { 23211U, 31638U, 32504U }, 90U }
};
static const Av2hLfSpec base_lf_specs[] = {
    { 0U, 1U, { 549U, 13587U, 23108U, 28767U, 30410U }, 10U },
    { 0U, 9U, { 13961U, 27727U, 31548U, 32432U, 32640U }, 30U },
    { 0U, 10U, { 8468U, 21640U, 27998U, 30901U, 32015U }, 90U },
    { 0U, 16U, { 18092U, 29844U, 32040U, 32571U, 32719U }, 75U },
    { 0U, 17U, { 11343U, 24700U, 29814U, 31603U, 32215U }, 93U },
    { 0U, 21U, { 21153U, 29831U, 31499U, 32049U, 32276U }, 75U },
    { 0U, 22U, { 11827U, 24672U, 29069U, 30619U, 31334U }, 93U },
    { 0U, 23U, { 6970U, 16794U, 23049U, 26961U, 28915U }, 75U },
    { 0U, 24U, { 6107U, 13557U, 19154U, 23110U, 25619U }, 75U },
    { 0U, 25U, { 2702U, 7985U, 12910U, 17340U, 20552U }, 78U },
    { 0U, 26U, { 1993U, 5600U, 9439U, 13043U, 16257U }, 90U },
    { 0U, 28U, { 26630U, 32076U, 32532U, 32672U, 32731U }, 75U },
    { 0U, 29U, { 13939U, 25333U, 29988U, 31536U, 32142U }, 90U },
    { 0U, 30U, { 6772U, 15540U, 22058U, 26174U, 28801U }, 75U },
    { 0U, 31U, { 5996U, 13722U, 19150U, 22923U, 25872U }, 78U },
    { 1U, 1U, { 1845U, 17089U, 26116U, 30050U, 31620U }, 75U },
    { 1U, 3U, { 5542U, 13743U, 19899U, 24736U, 27946U }, 75U },
    { 1U, 4U, { 3700U, 10363U, 15560U, 20281U, 23726U }, 75U },
    { 1U, 5U, { 2418U, 7197U, 12060U, 15605U, 19277U }, 93U },
    { 1U, 8U, { 1072U, 3013U, 4878U, 6783U, 8518U }, 93U },
    { 1U, 9U, { 18024U, 30489U, 32251U, 32626U, 32732U }, 90U },
    { 1U, 10U, { 11660U, 26630U, 30824U, 32187U, 32598U }, 75U },
    { 1U, 11U, { 12486U, 24964U, 29580U, 31503U, 32248U }, 15U },
    { 1U, 12U, { 6927U, 17619U, 24441U, 28334U, 30439U }, 78U },
    { 1U, 13U, { 4541U, 12600U, 19151U, 24114U, 27363U }, 93U },
    { 1U, 14U, { 3464U, 9852U, 15685U, 20588U, 24106U }, 78U },
    { 1U, 15U, { 2085U, 5889U, 9302U, 12624U, 15262U }, 93U },
    { 1U, 16U, { 23748U, 31870U, 32594U, 32721U, 32732U }, 90U },
    { 1U, 17U, { 17733U, 30083U, 32212U, 32605U, 32725U }, 75U },
    { 1U, 18U, { 12827U, 26254U, 30707U, 32112U, 32556U }, 75U },
    { 1U, 19U, { 7651U, 19635U, 26664U, 30009U, 31616U }, 78U },
    { 1U, 20U, { 3603U, 10277U, 15911U, 20395U, 23870U }, 78U },
    { 1U, 25U, { 3604U, 10158U, 15795U, 20231U, 23348U }, 0U },
    { 1U, 26U, { 2312U, 7277U, 11902U, 16059U, 19358U }, 75U },
    { 1U, 27U, { 1616U, 4713U, 7922U, 10951U, 13675U }, 93U },
    { 1U, 31U, { 7718U, 18010U, 24057U, 27609U, 29598U }, 75U },
    { 1U, 32U, { 3560U, 10218U, 15775U, 20026U, 23024U }, 75U },
    { 3U, 2U, { 9152U, 21002U, 26411U, 29197U, 30838U }, 0U },
    { 3U, 5U, { 2381U, 7283U, 11060U, 13175U, 15657U }, 78U },
    { 3U, 8U, { 1166U, 3234U, 5698U, 8017U, 9616U }, 93U },
    { 3U, 9U, { 26289U, 31974U, 32589U, 32707U, 32732U }, 0U },
    { 3U, 10U, { 14722U, 28032U, 31407U, 32465U, 32660U }, 0U },
    { 3U, 11U, { 14143U, 25751U, 29446U, 31254U, 32093U }, 75U },
    { 3U, 15U, { 1563U, 4884U, 8005U, 10967U, 13252U }, 90U },
    { 3U, 16U, { 28318U, 32551U, 32737U, 32741U, 32745U }, 75U },
    { 3U, 17U, { 21238U, 31149U, 32509U, 32707U, 32732U }, 75U },
    { 3U, 18U, { 16317U, 28616U, 31700U, 32471U, 32706U }, 1U },
    { 3U, 6U, { 1314U, 5852U, 7870U, 11057U, 13558U }, 75U },
    { 3U, 13U, { 4022U, 11170U, 17442U, 21682U, 24925U }, 75U },
    { 3U, 20U, { 3393U, 9936U, 15268U, 19487U, 22467U }, 76U }
    , { 2U, 1U, { 4579U, 20789U, 27200U, 30357U, 31565U }, 0U }
    , { 2U, 2U, { 10553U, 22962U, 27982U, 30214U, 31394U }, 1U }
    , { 2U, 7U, { 1665U, 4813U, 7575U, 10457U, 12877U }, 25U }
    , { 2U, 9U, { 25676U, 31984U, 32605U, 32712U, 32732U }, 75U }
    , { 2U, 10U, { 13894U, 27451U, 31145U, 32228U, 32639U }, 15U }
    , { 2U, 13U, { 4205U, 12741U, 19447U, 23962U, 26806U }, 76U }
    , { 2U, 16U, { 26506U, 32458U, 32715U, 32736U, 32740U }, 90U }
    , { 2U, 17U, { 20516U, 31118U, 32508U, 32699U, 32732U }, 75U }
    , { 2U, 18U, { 15612U, 28327U, 31610U, 32454U, 32687U }, 35U }
    , { 2U, 19U, { 8459U, 21222U, 27807U, 30732U, 31943U }, 76U }
    , { 2U, 20U, { 4168U, 12043U, 18257U, 22879U, 26034U }, 75U }
    , { 2U, 21U, { 30650U, 32519U, 32673U, 32723U, 32732U }, 75U }
    , { 2U, 22U, { 17529U, 29628U, 31828U, 32359U, 32545U }, 0U }
    , { 2U, 23U, { 10542U, 21115U, 26318U, 29161U, 30805U }, 7U }
    , { 2U, 28U, { 31955U, 32716U, 32740U, 32744U, 32748U }, 75U }
    , { 2U, 29U, { 22693U, 31709U, 32467U, 32593U, 32659U }, 75U }
};
static const Av2hBrSpec br_specs[] = {
    { 0U, { 24190U, 30560U, 31576U }, 30U },
    { 1U, { 24366U, 30921U, 31758U }, 78U },
    { 2U, { 21555U, 29858U, 31096U }, 3U },
    { 3U, { 16661U, 26574U, 28953U }, 78U },
    { 4U, { 11988U, 21738U, 25479U }, 93U },
    { 5U, { 8920U, 17446U, 22214U }, 93U },
    { 6U, { 5802U, 12294U, 17499U }, 78U }
};
static const Av2hBrSpec br_lf_specs[] = {
    { 0U, { 11992U, 17977U, 23847U }, 37U },
    { 1U, { 14229U, 22329U, 26075U }, 90U },
    { 2U, { 11443U, 20390U, 22820U }, 76U },
    { 4U, { 5827U, 11883U, 14653U }, 90U },
    { 5U, { 4416U, 9159U, 11664U }, 90U },
    { 6U, { 2812U, 6024U, 7386U }, 93U },
    { 7U, { 16755U, 23759U, 26700U }, 5U },
    { 8U, { 17293U, 25940U, 28518U }, 93U },
    { 9U, { 15547U, 25297U, 27304U }, 93U },
    { 10U, { 12584U, 22225U, 24378U }, 93U },
    { 11U, { 9185U, 17517U, 19977U }, 93U },
    { 13U, { 4509U, 9609U, 11858U }, 93U }
};

Av2hStatus av2h_coeff_level_cdfs_init(Av2hCoeffLevelCdfs *cdfs) {
    static const uint16_t eob_base_txs0[] = { 26028U, 30728U };
    static const uint16_t eob_lf_txs0[] = {
        28506U, 31820U, 32509U, 32686U
    };
    static const uint16_t eob_lf_txs0_ctx3[] = {
        27591U, 32110U, 32578U, 32712U
    };
    static const uint16_t eob_base_txs1[] = { 31276U, 32538U };
    static const uint16_t eob_base_txs1_ctx2[] = { 31889U, 32651U };
    static const uint16_t eob_base_txs2_ctx3[] = { 32695U, 32756U };
    static const uint16_t eob_base_txs2_ctx2[] = { 32688U, 32756U };
    static const uint16_t eob_base_txs2_ctx1[] = { 32624U, 32752U };
    static const uint16_t eob_base_txs3[] = { 32677U, 32743U };
    static const uint16_t eob_lf_txs1[] = {
        31747U, 32600U, 32712U, 32740U
    };
    static const uint16_t eob_lf_txs1_ctx0[] = {
        28550U, 32010U, 32465U, 32593U
    };
    static const uint16_t eob_lf_txs1_ctx2[] = {
        31600U, 32549U, 32688U, 32735U
    };
    static const uint16_t eob_lf_txs1_ctx1[] = {
        31254U, 32502U, 32660U, 32713U
    };
    static const uint16_t eob_lf_txs2_ctx1[] = {
        31836U, 32524U, 32640U, 32686U
    };
    static const uint16_t eob_lf_txs2_ctx0[] = {
        27239U, 31085U, 31883U, 32247U
    };
    static const uint16_t eob_lf_txs3_ctx1[] = {
        32054U, 32674U, 32735U, 32740U
    };
    static const uint16_t eob_lf_txs3_ctx0[] = {
        22871U, 30729U, 31682U, 32000U
    };
    static const uint16_t dc_sign[3U] = { 16540U, 14804U, 18312U };
    static const uint16_t uv_eob_ctx1[] = { 31595U, 32587U };
    static const uint16_t uv_eob_ctx3[] = { 32287U, 32698U };
    static const uint16_t uv_eob_lf_ctx0[] = {
        29843U, 32183U, 32562U, 32675U
    };
    static const uint16_t uv_base[][3U] = {
        { 26264U, 32282U, 32667U },
        { 27171U, 32473U, 32714U }
    };
    static const uint16_t uv_base_lf[][5U] = {
        { 5238U, 14626U, 21567U, 26015U, 28727U },
        { 6211U, 16772U, 23701U, 27732U, 29961U }
    };
    static const uint16_t uv_br_ctx0[] = { 24442U, 29827U, 31535U };
    static const uint16_t uv_base_ctx5[] = { 18547U, 30679U, 32297U };
    static const uint16_t uv_base_ctx6[] = { 9723U, 23037U, 28561U };
    static const uint16_t uv_base_ctx7[] = { 4310U, 12399U, 18745U };
    static const uint16_t uv_base_lf_ctx1[] = {
        9556U, 22999U, 28733U, 31004U, 31942U
    };
    static const uint16_t uv_base_ctx1[] = { 16468U, 29532U, 31912U };
    static const uint16_t uv_base_ctx3[] = { 4341U, 12415U, 18782U };
    static const uint16_t uv_base_lf_ctx3[] = {
        2196U, 6512U, 10780U, 14847U, 18294U
    };
    static const uint16_t uv_base_lf_ctx5[] = {
        11506U, 25824U, 30517U, 31948U, 32443U
    };
    static const uint16_t uv_base_lf_ctx7[] = {
        2461U, 7239U, 11755U, 15869U, 19210U
    };
    static const uint16_t uv_br_ctx1[] = { 22490U, 28954U, 31256U };
    static const uint16_t uv_br_ctx2[] = { 17882U, 25670U, 29377U };
    static const uint16_t uv_br_ctx3[] = { 10664U, 17735U, 22437U };
    static const uint16_t parity_thresholds[3U] = {
        8192U, 16384U, 24576U
    };
    size_t index;
    Av2hStatus status;
    if (cdfs == NULL) return AV2H_INVALID_ARGUMENT;
    cdfs->q_context = 1U;
    initialize_full_level_cdfs(cdfs, cdfs->q_context);
    status = av2h_cdf_init(cdfs->eob_base[0], 7U, eob_base_txs0, 3U, 3U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_base[1], 7U, eob_base_txs1, 3U, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_base[2], 7U, eob_base_txs3, 3U, 68U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_base[3], 7U, eob_base_txs1_ctx2,
                           3U, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_base[4], 7U, eob_base_txs2_ctx3,
                           3U, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_base[5], 7U, eob_base_txs2_ctx2,
                           3U, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_base[6], 7U, eob_base_txs2_ctx1,
                           3U, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[0], 9U, eob_lf_txs0, 5U, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[1], 9U, eob_lf_txs1, 5U, 78U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[2], 9U, eob_lf_txs1_ctx2, 5U,
                           75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[3], 9U, eob_lf_txs1_ctx1, 5U,
                           75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[4], 9U, eob_lf_txs0_ctx3, 5U,
                           75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[5], 9U, eob_lf_txs2_ctx1, 5U,
                           0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[6], 9U, eob_lf_txs1_ctx0, 5U,
                           1U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[7], 9U, eob_lf_txs3_ctx1, 5U,
                           0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[8], 9U, eob_lf_txs3_ctx0, 5U,
                           37U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->eob_lf[9], 9U, eob_lf_txs2_ctx0, 5U,
                           7U, 0U);
    if (status != AV2H_OK) return status;
    for (index = 0U;
         index < sizeof(base_specs) / sizeof(base_specs[0]); ++index) {
        uint16_t *base = find_key(
            &cdfs->base[0][0], 8U, base_keys,
            sizeof(base_keys) / sizeof(base_keys[0]),
            base_specs[index].tx_context, base_specs[index].context);
        if (base == NULL) return AV2H_UNSUPPORTED;
        status = av2h_cdf_init(base, 8U,
                               base_specs[index].threshold, 4U,
                               base_specs[index].parameter, 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U;
         index < sizeof(base_lf_specs) / sizeof(base_lf_specs[0]); ++index) {
        uint16_t *base_lf = find_key(
            &cdfs->base_lf[0][0], 10U, base_lf_keys,
            sizeof(base_lf_keys) / sizeof(base_lf_keys[0]),
            base_lf_specs[index].tx_context,
            base_lf_specs[index].context);
        if (base_lf == NULL) return AV2H_UNSUPPORTED;
        status = av2h_cdf_init(base_lf, 10U,
                               base_lf_specs[index].threshold, 6U,
                               base_lf_specs[index].parameter, 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < sizeof(br_specs) / sizeof(br_specs[0]); ++index) {
        status = av2h_cdf_init(cdfs->br[index], 8U,
                               br_specs[index].threshold, 4U,
                               br_specs[index].parameter, 0U);
        if (status != AV2H_OK) return status;
    }
        for (index = 0U;
            index < sizeof(br_lf_specs) / sizeof(br_lf_specs[0]); ++index) {
        status = av2h_cdf_init(cdfs->br_lf[index], 8U,
                               br_lf_specs[index].threshold, 4U,
                               br_lf_specs[index].parameter, 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 3U; ++index) {
        status = av2h_cdf_init(cdfs->dc_sign[index], AV2H_BINARY_CDF_SIZE,
                               &dc_sign[index], 2U, 93U, 0U);
        if (status != AV2H_OK) return status;
    }
    status = av2h_cdf_init(cdfs->uv_eob_lf_ctx0, 9U, uv_eob_lf_ctx0, 5U,
                           75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_eob_ctx1, 7U, uv_eob_ctx1, 3U,
                           93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_eob_ctx3, 7U, uv_eob_ctx3, 3U,
                           93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base[0], 8U, uv_base[0], 4U, 75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base[1], 8U, uv_base[1], 4U, 90U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_ctx1, 8U, uv_base_ctx1, 4U, 78U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_ctx3, 8U, uv_base_ctx3, 4U, 75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_ctx5, 8U, uv_base_ctx5, 4U, 75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_ctx6, 8U, uv_base_ctx6, 4U, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_ctx7, 8U, uv_base_ctx7, 4U, 75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_lf[0], 10U, uv_base_lf[0], 6U,
                           78U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_lf[1], 10U, uv_base_lf[1], 6U,
                           90U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_lf_ctx1, 10U, uv_base_lf_ctx1, 6U,
                           75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_lf_ctx3, 10U, uv_base_lf_ctx3, 6U,
                           93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_lf_ctx5, 10U, uv_base_lf_ctx5, 6U,
                           75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_base_lf_ctx7, 10U, uv_base_lf_ctx7, 6U,
                           90U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_br_ctx0, 8U, uv_br_ctx0, 4U, 75U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_br_ctx1, 8U, uv_br_ctx1, 4U, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_br_ctx2, 8U, uv_br_ctx2, 4U, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->uv_br_ctx3, 8U, uv_br_ctx3, 4U, 75U, 0U);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < 5U; ++index) {
        status = av2h_cdf_init(
            cdfs->parity_base[index], 8U, parity_thresholds,
            4U, 0U, 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 3U; ++index) {
        static const uint16_t sign_threshold[1U] = { 16384U };
        status = av2h_cdf_init(
            cdfs->parity_dc_sign[index], AV2H_BINARY_CDF_SIZE,
            sign_threshold, 2U, 0U, 0U);
        if (status != AV2H_OK) return status;
    }
    return initialize_fsc_cdfs(cdfs, 1U);
}

static uint16_t *find_key(uint16_t *rows, size_t row_words,
                          const Av2hCoeffKey *keys, size_t key_count,
                          uint8_t tx_context, uint8_t context) {
    size_t index;
    for (index = 0U; index < key_count; ++index) {
        if (keys[index].tx_context == tx_context &&
            keys[index].context == context) {
            return rows + index * row_words;
        }
    }
    return NULL;
}

static uint16_t *find_context(uint16_t *rows, size_t row_words,
                              const uint8_t *keys, size_t key_count,
                              uint8_t context) {
    size_t index;
    for (index = 0U; index < key_count; ++index) {
        if (keys[index] == context) return rows + index * row_words;
    }
    return NULL;
}

uint16_t *av2h_coeff_level_base_row(
    Av2hCoeffLevelCdfs *cdfs, uint8_t tx_context,
    uint8_t context, uint8_t low) {
    return low ?
        find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                 sizeof(base_lf_keys) / sizeof(base_lf_keys[0]),
                 tx_context, context) :
        find_key(&cdfs->base[0][0], 8U, base_keys,
                 sizeof(base_keys) / sizeof(base_keys[0]),
                 tx_context, context);
}

uint16_t *av2h_coeff_level_br_row(
    Av2hCoeffLevelCdfs *cdfs, uint8_t context, uint8_t low) {
    return low ?
        find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                     sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), context) :
        find_context(&cdfs->br[0][0], 8U, br_keys,
                     sizeof(br_keys) / sizeof(br_keys[0]), context);
}

uint16_t *av2h_coeff_level_eob_row(
    Av2hCoeffLevelCdfs *cdfs, uint8_t tx_context, uint8_t context) {
    return find_key(&cdfs->eob_base[0][0], 7U, eob_base_keys,
                    sizeof(eob_base_keys) / sizeof(eob_base_keys[0]),
                    tx_context, context);
}

static Av2hStatus initialize_level_row(
    uint16_t *row,
    size_t row_words,
    const uint16_t *thresholds,
    size_t symbol_count,
    uint8_t parameter) {
    if (row == NULL) return AV2H_UNSUPPORTED;
    return av2h_cdf_init(
        row, row_words, thresholds, symbol_count, parameter, 0U);
}

Av2hStatus av2h_coeff_level_cdfs_init_for_q(
    Av2hCoeffLevelCdfs *cdfs,
    Av2hQIndex base_q_index) {
    static const uint16_t q1_eob_base_txs4_ctx1[] = {
        32710U, 32756U
    };
    static const uint16_t q1_base_txs4_ctx5[] = {
        29793U, 32715U, 32748U
    };
    static const uint16_t q1_base_txs4_ctx1[] = {
        20773U, 31428U, 32627U
    };
    static const uint16_t q1_base_txs4_ctx0[] = {
        27869U, 32588U, 32732U
    };
    static const uint16_t q1_base_txs4_ctx2[] = {
        16679U, 28979U, 31835U
    };
    static const uint16_t q1_base_lf_txs4_ctx8[] = {
        575U, 2082U, 3357U, 4723U, 5504U
    };
    static const uint16_t q1_base_lf_txs4_ctx15[] = {
        1193U, 3568U, 6140U, 8638U, 10114U
    };
    static const uint16_t q1_base_lf_txs4_ctx17[] = {
        17896U, 30207U, 32257U, 32653U, 32732U
    };
    static const uint16_t q1_base_lf_txs4_ctx19[] = {
        6432U, 18636U, 25684U, 29741U, 31417U
    };
    static const uint16_t q1_base_lf_txs4_ctx20[] = {
        2460U, 7366U, 11813U, 15406U, 18038U
    };
    static const uint16_t q1_br_lf_ctx12[] = {
        7015U, 13961U, 16744U
    };
    static const uint16_t q2_eob_base_txs3_ctx1[] = { 32684U, 32756U };
    static const uint16_t q3_eob_lf_txs0_ctx0[] = {
        30609U, 32413U, 32637U, 32716U
    };
    static const uint16_t q3_eob_lf_txs0_ctx1[] = {
        30333U, 32610U, 32744U, 32748U
    };
    static const uint16_t q3_eob_lf_txs0_ctx2[] = {
        31067U, 32497U, 32676U, 32711U
    };
    static const uint16_t q3_eob_lf_txs1_ctx3[] = {
        32449U, 32740U, 32744U, 32748U
    };
    static const uint16_t q2_eob_lf_txs3_ctx1[] = {
        32167U, 32698U, 32744U, 32748U
    };
    static const uint16_t q2_eob_lf_txs4_ctx1[] = {
        32052U, 32690U, 32737U, 32741U
    };
    static const uint16_t q2_base_lf_txs4_ctx9[] = {
        23459U, 31826U, 32523U, 32706U, 32732U
    };
    static const uint16_t q2_base_lf_txs4_ctx1[] = {
        4322U, 17878U, 25258U, 29337U, 30946U
    };
    static const uint16_t q2_base_ctx0[] = {
        27184U, 32672U, 32748U
    };
    static const uint16_t q2_base_lf_txs3_ctx1[] = {
        6204U, 18887U, 26106U, 29994U, 31527U
    };
    static const uint16_t q3_base_lf_txs3_ctx6[] = {
        123U, 5065U, 11340U, 15150U, 17222U
    };
    static const uint16_t q2_base_lf_ctx2[] = {
        10730U, 22881U, 27929U, 30084U, 31344U
    };
    static const uint16_t q2_base_lf_ctx6[] = {
        1055U, 5756U, 9129U, 12574U, 14838U
    };
    static const uint16_t q2_base_lf_ctx9[] = {
        24889U, 31894U, 32576U, 32692U, 32732U
    };
    static const uint16_t q2_base_lf_ctx10[] = {
        13420U, 28082U, 31481U, 32420U, 32693U
    };
    static const uint16_t q2_base_lf_ctx13[] = {
        4611U, 12681U, 19425U, 24119U, 27368U
    };
    static const uint16_t q2_base_lf_ctx16[] = {
        26166U, 32502U, 32728U, 32736U, 32740U
    };
    static const uint16_t q2_base_lf_ctx17[] = {
        19676U, 30981U, 32490U, 32712U, 32732U
    };
    static const uint16_t q2_base_lf_ctx18[] = {
        15573U, 28642U, 31918U, 32572U, 32718U
    };
    static const uint16_t q2_br_lf_ctx1[] = {
        15058U, 23563U, 27312U
    };
    static const uint16_t q2_br_lf_ctx5[] = {
        5580U, 11627U, 15411U
    };
    static const uint16_t q2_dc_sign[] = { 17575U, 14224U, 19801U };
    static const uint8_t q2_dc_sign_parameter[] = { 75U, 78U, 93U };
    static const uint16_t q3_eob_lf_txs3_ctx0[] = {
        30132U, 32214U, 32554U, 32667U
    };
    static const uint16_t q3_eob_lf_txs1_ctx0[] = {
        31289U, 32570U, 32714U, 32740U
    };
    static const uint16_t q3_eob_lf_txs3_ctx1[] = {
        32297U, 32711U, 32744U, 32748U
    };
    static const uint16_t q3_eob_lf_txs3_ctx2[] = {
        6554U, 13107U, 19661U, 26214U
    };
    static const uint16_t q3_eob_base_txs3_ctx1[] = {
        32700U, 32756U
    };
    static const uint16_t q3_eob_base_txs1_ctx1[] = {
        32389U, 32626U
    };
    static const uint16_t q3_eob_base_txs1_ctx2[] = {
        32642U, 32756U
    };
    static const uint16_t q3_eob_base_txs1_ctx3[] = {
        32629U, 32756U
    };
    static const uint16_t q3_eob_base_txs2_ctx1[] = {
        32686U, 32756U
    };
    static const uint16_t q3_eob_base_txs2_ctx2[] = {
        32726U, 32756U
    };
    static const uint16_t q3_eob_base_txs4_ctx1[] = {
        32708U, 32756U
    };
    static const uint16_t q3_base_txs1_ctx17[] = {
        17095U, 29686U, 32489U
    };
    static const uint16_t q3_base_txs1_ctx18[] = {
        13280U, 23529U, 32046U
    };
    static const uint16_t q3_base_txs1_ctx0[] = {
        26034U, 32672U, 32748U
    };
    static const uint16_t q3_base_txs1_ctx1[] = {
        22551U, 32372U, 32748U
    };
    static const uint16_t q3_base_txs1_ctx2[] = {
        17805U, 31280U, 32648U
    };
    static const uint16_t q3_base_txs1_ctx5[] = {
        29142U, 32726U, 32748U
    };
    static const uint16_t q3_base_txs1_ctx10[] = {
        29915U, 32732U, 32748U
    };
    static const uint16_t q3_base_txs1_ctx6[] = {
        24984U, 32622U, 32748U
    };
    static const uint16_t q3_base_txs1_ctx7[] = {
        18241U, 32005U, 32740U
    };
    static const uint16_t q3_base_txs1_ctx11[] = {
        25072U, 32653U, 32747U
    };
    static const uint16_t q3_base_txs1_ctx12[] = {
        19064U, 32047U, 32688U
    };
    static const uint16_t q3_base_txs1_ctx16[] = {
        20616U, 31528U, 32613U
    };
    static const uint16_t q3_base_txs1_ctx15[] = {
        32165U, 32689U, 32748U
    };
    static const uint16_t q3_base_txs2_ctx11[] = {
        25750U, 32644U, 32748U
    };
    static const uint16_t q3_base_lf_txs1_ctx1[] = {
        86U, 20785U, 28528U, 31103U, 32087U
    };
    static const uint16_t q3_base_lf_txs1_ctx2[] = {
        10869U, 20589U, 26954U, 30327U, 31505U
    };
    static const uint16_t q3_base_lf_txs0_ctx1[] = {
        100U, 16721U, 28276U, 31260U, 32398U
    };
    static const uint16_t q3_base_lf_txs0_ctx2[] = {
        6124U, 20746U, 26659U, 30297U, 32090U
    };
    static const uint16_t q3_base_lf_txs0_ctx9[] = {
        18225U, 31062U, 32659U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs0_ctx11[] = {
        14256U, 29803U, 32043U, 32616U, 32732U
    };
    static const uint16_t q3_base_lf_txs0_ctx22[] = {
        16090U, 27467U, 31184U, 32157U, 32704U
    };
    static const uint16_t q3_base_lf_txs0_ctx21[] = {
        29595U, 31985U, 32553U, 32665U, 32732U
    };
    static const uint16_t q3_base_lf_txs1_ctx0[] = {
        10519U, 30400U, 31391U, 32327U, 32713U
    };
    static const uint16_t q3_base_lf_txs1_ctx3[] = {
        4914U, 15045U, 23040U, 27717U, 30490U
    };
    static const uint16_t q3_base_lf_txs1_ctx4[] = {
        2819U, 10379U, 17295U, 23231U, 27208U
    };
    static const uint16_t q3_base_lf_txs1_ctx13[] = {
        7348U, 19414U, 26844U, 30566U, 31941U
    };
    static const uint16_t q3_base_lf_txs1_ctx5[] = {
        2510U, 8572U, 13721U, 19650U, 24615U
    };
    static const uint16_t q3_base_lf_txs1_ctx28[] = {
        32033U, 32744U, 32748U, 32752U, 32756U
    };
    static const uint16_t q3_base_lf_txs1_ctx29[] = {
        21905U, 31682U, 32689U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs1_ctx30[] = {
        16223U, 28310U, 31943U, 32665U, 32704U
    };
    static const uint16_t q3_base_lf_txs1_ctx21[] = {
        30851U, 32572U, 32702U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs1_ctx22[] = {
        15898U, 29692U, 32156U, 32573U, 32722U
    };
    static const uint16_t q3_base_lf_txs1_ctx23[] = {
        9275U, 21031U, 27592U, 31095U, 32259U
    };
    static const uint16_t q3_base_lf_txs3_ctx12[] = {
        7549U, 20488U, 27158U, 30433U, 31903U
    };
    static const uint16_t q3_base_lf_txs3_ctx13[] = {
        4919U, 15070U, 22336U, 27218U, 29792U
    };
    static const uint16_t q3_base_lf_txs3_ctx19[] = {
        10133U, 24671U, 30659U, 32294U, 32704U
    };
    static const uint16_t q3_base_lf_txs1_ctx9[] = {
        21042U, 31954U, 32667U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs1_ctx10[] = {
        7851U, 28094U, 31782U, 32677U, 32732U
    };
    static const uint16_t q3_base_lf_txs1_ctx11[] = {
        12762U, 26468U, 31109U, 32414U, 32693U
    };
    static const uint16_t q3_base_lf_txs1_ctx12[] = {
        10020U, 22866U, 29258U, 31456U, 32395U
    };
    static const uint16_t q3_base_lf_txs1_ctx16[] = {
        24491U, 32563U, 32740U, 32744U, 32748U
    };
    static const uint16_t q3_base_lf_txs1_ctx17[] = {
        18023U, 31654U, 32683U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs1_ctx18[] = {
        15126U, 29669U, 32385U, 32717U, 32732U
    };
    static const uint16_t q3_base_lf_txs1_ctx19[] = {
        11803U, 26851U, 31619U, 32578U, 32732U
    };
    static const uint16_t q3_base_lf_txs2_ctx3[] = {
        5931U, 14893U, 21314U, 26301U, 29130U
    };
    static const uint16_t q3_base_lf_txs2_ctx4[] = {
        4511U, 11376U, 17124U, 22328U, 26530U
    };
    static const uint16_t q3_base_lf_txs2_ctx5[] = {
        2402U, 9192U, 14384U, 19305U, 22990U
    };
    static const uint16_t q3_base_lf_txs2_ctx0[] = {
        20744U, 31573U, 32432U, 32575U, 32732U
    };
    static const uint16_t q3_eob_lf_txs2_ctx0[] = {
        31348U, 32506U, 32654U, 32724U
    };
    static const uint16_t q3_eob_lf_txs1_ctx1[] = {
        32248U, 32709U, 32744U, 32748U
    };
    static const uint16_t q3_eob_lf_txs1_ctx2[] = {
        32234U, 32719U, 32744U, 32748U
    };
    static const uint16_t q3_eob_lf_txs2_ctx1[] = {
        32301U, 32703U, 32744U, 32748U
    };
    static const uint16_t q3_eob_lf_txs4_ctx0[] = {
        28223U, 31648U, 32311U, 32527U
    };
    static const uint16_t q3_eob_lf_txs4_ctx1[] = {
        32184U, 32695U, 32744U, 32748U
    };
    static const uint16_t q3_base_lf_txs2_ctx1[] = {
        1512U, 20783U, 28537U, 31233U, 32030U
    };
    static const uint16_t q3_base_lf_txs2_ctx2[] = {
        11418U, 22879U, 28443U, 30589U, 31691U
    };
    static const uint16_t q3_base_lf_txs2_ctx9[] = {
        22323U, 32120U, 32586U, 32723U, 32732U
    };
    static const uint16_t q3_base_lf_txs2_ctx10[] = {
        9067U, 28461U, 31803U, 32646U, 32732U
    };
    static const uint16_t q3_base_lf_txs2_ctx11[] = {
        14440U, 27462U, 31270U, 32400U, 32671U
    };
    static const uint16_t q3_base_lf_txs2_ctx12[] = {
        8061U, 20968U, 27850U, 30659U, 31948U
    };
    static const uint16_t q3_base_lf_txs2_ctx21[] = {
        31243U, 32617U, 32724U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs2_ctx22[] = {
        15771U, 29814U, 32176U, 32618U, 32719U
    };
    static const uint16_t q3_base_lf_txs2_ctx28[] = {
        32375U, 32744U, 32748U, 32752U, 32756U
    };
    static const uint16_t q3_base_lf_txs2_ctx29[] = {
        21853U, 32148U, 32719U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs2_ctx16[] = {
        24757U, 32619U, 32740U, 32744U, 32748U
    };
    static const uint16_t q3_base_lf_txs2_ctx17[] = {
        19775U, 31747U, 32667U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs2_ctx19[] = {
        10631U, 24891U, 30498U, 32167U, 32638U
    };
    static const uint16_t q3_base_lf_txs2_ctx18[] = {
        17105U, 29988U, 32352U, 32704U, 32732U
    };
    static const uint16_t q3_base_lf_ctx2[] = {
        11218U, 23056U, 28975U, 31289U, 32045U
    };
    static const uint16_t q3_base_lf_ctx10[] = {
        10345U, 28211U, 31805U, 32644U, 32732U
    };
    static const uint16_t q3_base_lf_ctx16[] = {
        23376U, 32614U, 32740U, 32744U, 32748U
    };
    static const uint16_t q3_base_lf_ctx17[] = {
        18958U, 31560U, 32655U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_ctx18[] = {
        15525U, 29433U, 32224U, 32682U, 32732U
    };
    static const uint16_t q3_base_lf_ctx11[] = {
        13559U, 26348U, 31030U, 32344U, 32653U
    };
    static const uint16_t q3_base_lf_ctx9[] = {
        19675U, 31285U, 32524U, 32698U, 32732U
    };
    static const uint16_t q3_base_lf_txs4_ctx16[] = {
        24009U, 32537U, 32740U, 32744U, 32748U
    };
    static const uint16_t q3_base_lf_txs4_ctx10[] = {
        12348U, 28499U, 31976U, 32606U, 32720U
    };
    static const uint16_t q3_base_lf_txs4_ctx9[] = {
        20741U, 31938U, 32628U, 32728U, 32732U
    };
    static const uint16_t q3_base_lf_txs4_ctx1[] = {
        6387U, 21131U, 28651U, 31386U, 32236U
    };
    static const uint16_t q3_base_lf_txs4_ctx18[] = {
        15897U, 29451U, 32143U, 32648U, 32732U
    };
    static const uint16_t q3_base_lf_txs4_ctx17[] = {
        19318U, 31691U, 32676U, 32736U, 32740U
    };
    static const uint16_t q3_base_lf_txs4_ctx19[] = {
        10046U, 24716U, 30457U, 32055U, 32572U
    };
    static const uint16_t q3_base_lf_txs4_ctx12[] = {
        7379U, 21206U, 27561U, 30526U, 31713U
    };
    static const uint16_t q3_base_lf_txs4_ctx13[] = {
        5216U, 15396U, 22184U, 26885U, 29262U
    };
    static const uint16_t q3_base_lf_txs4_ctx11[] = {
        13275U, 26774U, 30839U, 32177U, 32535U
    };
    static const uint16_t q3_base_lf_txs4_ctx3[] = {
        7250U, 18857U, 23745U, 26494U, 29185U
    };
    static const uint16_t q3_base_lf_txs4_ctx2[] = {
        10794U, 23651U, 28414U, 30514U, 31623U
    };
    static const uint16_t q3_base_lf_txs3_ctx3[] = {
        4973U, 15646U, 22529U, 27020U, 30234U
    };
    static const uint16_t q3_base_lf_txs3_ctx0[] = {
        17586U, 29249U, 30954U, 31637U, 31943U
    };
    static const uint16_t q3_base_lf_txs3_ctx1[] = {
        6607U, 19220U, 27249U, 31067U, 32015U
    };
    static const uint16_t q3_base_txs4_ctx10[] = {
        31236U, 32752U, 32756U
    };
    static const uint16_t q3_base_txs4_ctx11[] = {
        27068U, 32675U, 32748U
    };
    static const uint16_t q3_base_txs4_ctx5[] = {
        29384U, 32732U, 32748U
    };
    static const uint16_t q3_base_txs4_ctx6[] = {
        24520U, 32525U, 32747U
    };
    static const uint16_t q3_base_txs4_ctx0[] = {
        27339U, 32708U, 32748U
    };
    static const uint16_t q3_base_txs4_ctx1[] = {
        23878U, 32548U, 32744U
    };
    static const uint16_t q3_base_txs4_ctx2[] = {
        19326U, 31492U, 32668U
    };
    static const uint16_t q3_base_txs3_ctx0[] = {
        26428U, 32701U, 32748U
    };
    static const uint16_t q3_base_txs3_ctx1[] = {
        22687U, 32407U, 32740U
    };
    static const uint16_t q3_base_txs3_ctx2[] = {
        18394U, 31398U, 32670U
    };
    static const uint16_t q3_base_txs2_ctx3[] = {
        12030U, 27356U, 31773U
    };
    static const uint16_t q3_base_txs2_ctx10[] = {
        31096U, 32751U, 32755U
    };
    static const uint16_t q3_base_txs2_ctx5[] = {
        29639U, 32739U, 32748U
    };
    static const uint16_t q3_base_txs2_ctx1[] = {
        22953U, 32345U, 32732U
    };
    static const uint16_t q3_base_txs2_ctx0[] = {
        27252U, 32687U, 32748U
    };
    static const uint16_t q3_base_txs2_ctx2[] = {
        18289U, 31130U, 32616U
    };
    static const uint16_t q3_base_txs2_ctx6[] = {
        24256U, 32629U, 32748U
    };
    static const uint16_t q3_base_txs2_ctx7[] = {
        18917U, 31583U, 32708U
    };
    static const uint16_t q3_base_txs3_ctx10[] = {
        31332U, 32752U, 32756U
    };
    static const uint16_t q3_base_txs3_ctx11[] = {
        26024U, 32656U, 32746U
    };
    static const uint16_t q3_base_txs3_ctx5[] = {
        28826U, 32741U, 32748U
    };
    static const uint16_t q3_base_txs3_ctx6[] = {
        24880U, 32674U, 32744U
    };
    static const uint16_t q3_br_lf_ctx0[] = {
        13965U, 20979U, 26518U
    };
    static const uint16_t q3_br_lf_ctx1[] = {
        17417U, 26343U, 29434U
    };
    static const uint16_t q3_br_lf_ctx2[] = {
        14569U, 24464U, 27796U
    };
    static const uint16_t q3_br_lf_ctx3[] = {
        12532U, 22823U, 25671U
    };
    static const uint16_t q3_br_lf_ctx4[] = {
        8586U, 17291U, 21825U
    };
    static const uint16_t q3_br_lf_ctx9[] = {
        19961U, 29271U, 31158U
    };
    static const uint16_t q3_dc_sign[] = { 20108U, 14675U, 20547U };
    static const uint8_t q3_dc_sign_parameter[] = { 1U, 0U, 93U };
    static const uint16_t q3_uv_eob_lf[4U][5U] = {
        { 31345U, 32511U, 32676U, 32725U },
        { 31940U, 32413U, 32531U, 32650U },
        { 6554U, 13107U, 19661U, 26214U },
        { 6554U, 13107U, 19661U, 26214U }
    };
    static const uint8_t q3_uv_eob_lf_parameter[4U] = {
        76U, 50U, 0U, 0U
    };
    static const uint16_t q3_uv_eob[4U][3U] = {
        { 10923U, 21845U },
        { 31987U, 32697U },
        { 32662U, 32756U },
        { 32487U, 32698U }
    };
    static const uint8_t q3_uv_eob_parameter[4U] = {
        0U, 93U, 90U, 60U
    };
    static const uint16_t q3_uv_base_lf[12U][5U] = {
        { 7986U, 24484U, 30470U, 32123U, 32478U },
        { 6385U, 22268U, 29781U, 31884U, 32479U },
        { 4563U, 14337U, 22377U, 27527U, 30631U },
        { 1768U, 5903U, 10730U, 16633U, 21440U },
        { 7554U, 22427U, 29258U, 31475U, 32386U },
        { 6209U, 20694U, 28786U, 31444U, 32330U },
        { 4185U, 12361U, 19887U, 25739U, 29491U },
        { 1583U, 5456U, 9702U, 15370U, 20474U },
        { 30183U, 32316U, 32574U, 32639U, 32703U },
        { 15147U, 31729U, 32521U, 32669U, 32719U },
        { 5461U, 10923U, 16384U, 21845U, 27307U },
        { 5461U, 10923U, 16384U, 21845U, 27307U }
    };
    static const uint8_t q3_uv_base_lf_parameter[12U] = {
        12U, 6U, 75U, 90U, 5U, 90U, 93U, 90U, 90U, 35U, 0U, 0U
    };
    static const uint16_t q3_uv_base[12U][3U] = {
        { 26130U, 32392U, 32726U },
        { 13730U, 29626U, 32253U },
        { 9442U, 23537U, 29252U },
        { 4255U, 13986U, 21188U },
        { 24285U, 32033U, 32659U },
        { 11954U, 28100U, 31763U },
        { 7399U, 19439U, 26435U },
        { 4096U, 12081U, 19339U },
        { 30583U, 31894U, 32331U },
        { 18905U, 31508U, 32138U },
        { 8192U, 16384U, 24576U },
        { 8192U, 16384U, 24576U }
    };
    static const uint8_t q3_uv_base_parameter[12U] = {
        75U, 75U, 6U, 35U, 0U, 75U, 5U, 77U, 90U, 25U, 0U, 0U
    };
    static const uint16_t q3_uv_br[4U][3U] = {
        { 26977U, 31598U, 32364U },
        { 24721U, 30575U, 32124U },
        { 18803U, 26659U, 30610U },
        { 11757U, 20385U, 25284U }
    };
    static const uint8_t q3_uv_br_parameter[4U] = {
        50U, 93U, 90U, 5U
    };
    static const uint16_t adjusted_uv_eob[2U][2U] = {
        { 31595U, 32587U }, { 32086U, 32691U }
    };
    static const uint16_t adjusted_uv_base_q1[3U][3U] = {
        { 26264U, 32282U, 32667U },
        { 16468U, 29532U, 31912U },
        { 8915U, 21647U, 27531U }
    };
    static const uint16_t adjusted_uv_base_q1_parameter[3U] = {
        75U, 78U, 0U
    };
    static const uint16_t adjusted_uv_base_q2[] = {
        26928U, 32457U, 32720U
    };
    static const uint16_t adjusted_uv_base_lf_q1[] = {
        2196U, 6512U, 10780U, 14847U, 18294U
    };
    static const uint16_t adjusted_uv_base_lf_q2[] = {
        10499U, 24975U, 30136U, 31803U, 32399U
    };
    static const uint16_t adjusted_uv_eob_lf_q2[] = {
        30964U, 32463U, 32673U, 32733U
    };
    static const uint16_t adjusted_uv_base_v_q1[3U][3U] = {
        { 27171U, 32473U, 32714U },
        { 18547U, 30679U, 32297U },
        { 9723U, 23037U, 28561U }
    };
    static const uint16_t adjusted_uv_base_v_q1_parameter[3U] = {
        90U, 75U, 0U
    };
    static const uint16_t adjusted_uv_base_lf_v_q1[] = {
        2461U, 7239U, 11755U, 15869U, 19210U
    };
    static const uint16_t adjusted_uv_br_q1[2U][3U] = {
        { 22490U, 28954U, 31256U },
        { 17882U, 25670U, 29377U }
    };
    uint8_t q_context;
    size_t index;
    Av2hStatus status = av2h_coeff_level_cdfs_init(cdfs);
    uint16_t *row;
    if (status != AV2H_OK) return status;
    q_context = av2h_coeff_q_context(base_q_index);
    cdfs->q_context = q_context;
    initialize_full_level_cdfs(cdfs, q_context);
    status = initialize_fsc_cdfs(cdfs, q_context);
    if (status != AV2H_OK) return status;
    if (q_context == 0U) {
        av2h_memory_copy(cdfs->q0_txs4_eob_base,
                         av2h_q0_txs4_eob_base,
                         sizeof(cdfs->q0_txs4_eob_base));
        av2h_memory_copy(cdfs->q0_txs4_eob_base_lf,
                         av2h_q0_txs4_eob_base_lf,
                         sizeof(cdfs->q0_txs4_eob_base_lf));
        av2h_memory_copy(cdfs->q0_txs4_base, av2h_q0_txs4_base,
                         sizeof(cdfs->q0_txs4_base));
        av2h_memory_copy(cdfs->q0_txs4_base_lf, av2h_q0_txs4_base_lf,
                         sizeof(cdfs->q0_txs4_base_lf));
        av2h_memory_copy(cdfs->q0_txs3_eob_base,
                 av2h_q0_txs3_eob_base,
                 sizeof(cdfs->q0_txs3_eob_base));
        av2h_memory_copy(cdfs->q0_txs3_eob_base_lf,
                 av2h_q0_txs3_eob_base_lf,
                 sizeof(cdfs->q0_txs3_eob_base_lf));
        av2h_memory_copy(cdfs->q0_txs3_base, av2h_q0_txs3_base,
                 sizeof(cdfs->q0_txs3_base));
        av2h_memory_copy(cdfs->q0_txs3_base_lf, av2h_q0_txs3_base_lf,
                 sizeof(cdfs->q0_txs3_base_lf));
        av2h_memory_copy(cdfs->q0_txs2_eob_base,
                 av2h_q0_txs2_eob_base,
                 sizeof(cdfs->q0_txs2_eob_base));
        av2h_memory_copy(cdfs->q0_txs2_eob_base_lf,
                 av2h_q0_txs2_eob_base_lf,
                 sizeof(cdfs->q0_txs2_eob_base_lf));
        av2h_memory_copy(cdfs->q0_txs2_base, av2h_q0_txs2_base,
                 sizeof(cdfs->q0_txs2_base));
        av2h_memory_copy(cdfs->q0_txs2_base_lf, av2h_q0_txs2_base_lf,
                 sizeof(cdfs->q0_txs2_base_lf));
        av2h_memory_copy(cdfs->q0_txs1_eob_base,
                 av2h_q0_txs1_eob_base,
                 sizeof(cdfs->q0_txs1_eob_base));
        av2h_memory_copy(cdfs->q0_txs1_eob_base_lf,
                 av2h_q0_txs1_eob_base_lf,
                 sizeof(cdfs->q0_txs1_eob_base_lf));
        av2h_memory_copy(cdfs->q0_txs1_base, av2h_q0_txs1_base,
                 sizeof(cdfs->q0_txs1_base));
        av2h_memory_copy(cdfs->q0_txs1_base_lf, av2h_q0_txs1_base_lf,
                 sizeof(cdfs->q0_txs1_base_lf));
        av2h_memory_copy(cdfs->q0_txs0_eob_base,
                 av2h_q0_txs0_eob_base,
                 sizeof(cdfs->q0_txs0_eob_base));
        av2h_memory_copy(cdfs->q0_txs0_eob_base_lf,
                 av2h_q0_txs0_eob_base_lf,
                 sizeof(cdfs->q0_txs0_eob_base_lf));
        av2h_memory_copy(cdfs->q0_txs0_base, av2h_q0_txs0_base,
                 sizeof(cdfs->q0_txs0_base));
        av2h_memory_copy(cdfs->q0_txs0_base_lf, av2h_q0_txs0_base_lf,
                 sizeof(cdfs->q0_txs0_base_lf));
        av2h_memory_copy(cdfs->q0_br, av2h_q0_br,
                         sizeof(cdfs->q0_br));
        av2h_memory_copy(cdfs->q0_br_lf, av2h_q0_br_lf,
                         sizeof(cdfs->q0_br_lf));
        av2h_memory_copy(cdfs->dc_sign, av2h_q0_dc_sign,
                         sizeof(cdfs->dc_sign));
        av2h_memory_copy(cdfs->q0_uv_eob, av2h_q0_uv_eob,
                 sizeof(cdfs->q0_uv_eob));
        av2h_memory_copy(cdfs->q0_uv_eob_lf, av2h_q0_uv_eob_lf,
                 sizeof(cdfs->q0_uv_eob_lf));
        av2h_memory_copy(cdfs->q0_uv_base, av2h_q0_uv_base,
                 sizeof(cdfs->q0_uv_base));
        av2h_memory_copy(cdfs->q0_uv_base_lf, av2h_q0_uv_base_lf,
                 sizeof(cdfs->q0_uv_base_lf));
        av2h_memory_copy(cdfs->q0_uv_br, av2h_q0_uv_br,
                 sizeof(cdfs->q0_uv_br));
        return AV2H_OK;
    }
    if (q_context == 1U) {
        status = av2h_cdf_init(
            cdfs->adjusted_uv_eob[0U], 7U, adjusted_uv_eob[0U],
            3U, 93U, 0U);
        if (status != AV2H_OK) return status;
        for (index = 0U; index < 3U; ++index) {
            status = av2h_cdf_init(
                cdfs->adjusted_uv_base[0U][index], 8U,
                adjusted_uv_base_q1[index], 4U,
                adjusted_uv_base_q1_parameter[index], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->adjusted_uv_base[0U][index + 4U], 8U,
                adjusted_uv_base_v_q1[index], 4U,
                adjusted_uv_base_v_q1_parameter[index], 0U);
            if (status != AV2H_OK) return status;
        }
        status = av2h_cdf_init(
            cdfs->adjusted_uv_base_lf[0U][3U], 10U,
            adjusted_uv_base_lf_q1, 6U, 93U, 0U);
        if (status != AV2H_OK) return status;
        status = av2h_cdf_init(
            cdfs->adjusted_uv_base_lf[0U][7U], 10U,
            adjusted_uv_base_lf_v_q1, 6U, 90U, 0U);
        if (status != AV2H_OK) return status;
        for (index = 0U; index < 2U; ++index) {
            status = av2h_cdf_init(
                cdfs->adjusted_uv_br[0U][index + 1U], 8U,
                adjusted_uv_br_q1[index], 4U, 93U, 0U);
            if (status != AV2H_OK) return status;
        }
        status = initialize_level_row(
            cdfs->q1_eob_base_txs4_ctx1, 7U,
            q1_eob_base_txs4_ctx1, 3U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q1_base_txs4_ctx0, 8U,
            q1_base_txs4_ctx0, 4U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q1_base_txs4_ctx1, 8U,
            q1_base_txs4_ctx1, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q1_base_txs4_ctx2, 8U,
            q1_base_txs4_ctx2, 4U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q1_base_txs4_ctx5, 8U,
            q1_base_txs4_ctx5, 4U, 31U);
        if (status != AV2H_OK) return status;
#define AV2H_INIT_Q1_BASE_LF(context, thresholds, parameter)                  \
        status = initialize_level_row(                                       \
            cdfs->q1_base_lf_txs4_ctx##context, 10U,                         \
            thresholds, 6U, parameter);                                      \
        if (status != AV2H_OK) return status
        AV2H_INIT_Q1_BASE_LF(8, q1_base_lf_txs4_ctx8, 93U);
        AV2H_INIT_Q1_BASE_LF(15, q1_base_lf_txs4_ctx15, 93U);
        AV2H_INIT_Q1_BASE_LF(17, q1_base_lf_txs4_ctx17, 75U);
        AV2H_INIT_Q1_BASE_LF(19, q1_base_lf_txs4_ctx19, 0U);
        AV2H_INIT_Q1_BASE_LF(20, q1_base_lf_txs4_ctx20, 90U);
#undef AV2H_INIT_Q1_BASE_LF
        return initialize_level_row(
            cdfs->q1_br_lf_ctx12, 8U,
            q1_br_lf_ctx12, 4U, 93U);
    }
    if (q_context == 2U) {
        status = av2h_cdf_init(
            cdfs->adjusted_uv_eob[1U], 7U, adjusted_uv_eob[1U],
            3U, 93U, 0U);
        if (status != AV2H_OK) return status;
        status = av2h_cdf_init(
            cdfs->adjusted_uv_eob_lf[1U], 9U,
            adjusted_uv_eob_lf_q2, 5U, 75U, 0U);
        if (status != AV2H_OK) return status;
        status = av2h_cdf_init(
            cdfs->adjusted_uv_base[1U][0U], 8U,
            adjusted_uv_base_q2, 4U, 75U, 0U);
        if (status != AV2H_OK) return status;
        status = av2h_cdf_init(
            cdfs->adjusted_uv_base_lf[1U][1U], 10U,
            adjusted_uv_base_lf_q2, 6U, 75U, 0U);
        if (status != AV2H_OK) return status;
        row = find_key(&cdfs->eob_base[0][0], 7U, eob_base_keys,
                       sizeof(eob_base_keys) / sizeof(eob_base_keys[0]),
                       3U, 1U);
        status = initialize_level_row(
            row, 7U, q2_eob_base_txs3_ctx1, 3U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->eob_lf[7], 9U, q2_eob_lf_txs3_ctx1, 5U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q2_eob_lf_txs4_ctx1, 9U,
            q2_eob_lf_txs4_ctx1, 5U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q2_base_lf_txs4_ctx9, 10U,
            q2_base_lf_txs4_ctx9, 6U, 32U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q2_base_lf_txs4_ctx1, 10U,
            q2_base_lf_txs4_ctx1, 6U, 31U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 0U),
            8U, q2_base_ctx0, 4U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q2_base_lf_txs3_ctx1, 10U,
            q2_base_lf_txs3_ctx1, 6U, 1U);
        if (status != AV2H_OK) return status;
#define AV2H_INIT_Q2_BASE_LF(context, thresholds, parameter)                  \
        status = initialize_level_row(                                       \
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,               \
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]),          \
                     3U, context),                                            \
            10U, thresholds, 6U, parameter);                                  \
        if (status != AV2H_OK) return status
        AV2H_INIT_Q2_BASE_LF(2U, q2_base_lf_ctx2, 25U);
        AV2H_INIT_Q2_BASE_LF(6U, q2_base_lf_ctx6, 26U);
        AV2H_INIT_Q2_BASE_LF(9U, q2_base_lf_ctx9, 6U);
        AV2H_INIT_Q2_BASE_LF(10U, q2_base_lf_ctx10, 2U);
        AV2H_INIT_Q2_BASE_LF(13U, q2_base_lf_ctx13, 75U);
        AV2H_INIT_Q2_BASE_LF(16U, q2_base_lf_ctx16, 0U);
        AV2H_INIT_Q2_BASE_LF(17U, q2_base_lf_ctx17, 75U);
        AV2H_INIT_Q2_BASE_LF(18U, q2_base_lf_ctx18, 1U);
#undef AV2H_INIT_Q2_BASE_LF
        status = initialize_level_row(
            find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                         sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), 1U),
            8U, q2_br_lf_ctx1, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                         sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), 5U),
            8U, q2_br_lf_ctx5, 4U, 75U);
        if (status != AV2H_OK) return status;
        for (index = 0U; index < 3U; ++index) {
            status = av2h_binary_cdf_init(
                cdfs->dc_sign[index],
                (Av2hBinaryCdfSpec){
                    q2_dc_sign[index], q2_dc_sign_parameter[index], 0U
                });
            if (status != AV2H_OK) return status;
        }
        return AV2H_OK;
    }
    if (q_context == 3U) {
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs0_ctx0, 9U,
            q3_eob_lf_txs0_ctx0, 5U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs0_ctx1, 9U,
            q3_eob_lf_txs0_ctx1, 5U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs0_ctx2, 9U,
            q3_eob_lf_txs0_ctx2, 5U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs1_ctx3, 9U,
            q3_eob_lf_txs1_ctx3, 5U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 1U, 15U),
            8U, q3_base_txs1_ctx15, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs1_ctx0, 9U,
            q3_eob_lf_txs1_ctx0, 5U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs1_ctx1, 9U,
            q3_eob_lf_txs1_ctx1, 5U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs1_ctx2, 9U,
            q3_eob_lf_txs1_ctx2, 5U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->eob_lf[9], 9U, q3_eob_lf_txs2_ctx0, 5U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->eob_lf[5], 9U, q3_eob_lf_txs2_ctx1, 5U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs4_ctx0, 9U,
            q3_eob_lf_txs4_ctx0, 5U, 37U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs4_ctx1, 9U,
            q3_eob_lf_txs4_ctx1, 5U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 0U),
            10U, q3_base_lf_txs2_ctx0, 6U, 37U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 1U),
            10U, q3_base_lf_txs2_ctx1, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 2U),
            10U, q3_base_lf_txs2_ctx2, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 9U),
            10U, q3_base_lf_txs2_ctx9, 6U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 10U),
            10U, q3_base_lf_txs2_ctx10, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx11,
            10U, q3_base_lf_txs2_ctx11, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx12,
            10U, q3_base_lf_txs2_ctx12, 6U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx5,
            10U, q3_base_lf_txs2_ctx5, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx21,
            10U, q3_base_lf_txs2_ctx21, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx22,
            10U, q3_base_lf_txs2_ctx22, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx28,
            10U, q3_base_lf_txs2_ctx28, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 16U),
            10U, q3_base_lf_txs2_ctx16, 6U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 17U),
            10U, q3_base_lf_txs2_ctx17, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 19U),
            10U, q3_base_lf_txs2_ctx19, 6U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 2U, 18U),
            10U, q3_base_lf_txs2_ctx18, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->eob_lf[8], 9U, q3_eob_lf_txs3_ctx0, 5U, 7U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->eob_lf[7], 9U, q3_eob_lf_txs3_ctx1, 5U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_lf_txs3_ctx2, 9U,
            q3_eob_lf_txs3_ctx2, 5U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->eob_base[0][0], 7U, eob_base_keys,
                     sizeof(eob_base_keys) / sizeof(eob_base_keys[0]),
                     3U, 1U),
            7U, q3_eob_base_txs3_ctx1, 3U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_base_txs1_ctx1, 7U,
            q3_eob_base_txs1_ctx1, 3U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_base_txs1_ctx2, 7U,
            q3_eob_base_txs1_ctx2, 3U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_base_txs1_ctx3, 7U,
            q3_eob_base_txs1_ctx3, 3U, 18U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_base_txs2_ctx1, 7U,
            q3_eob_base_txs2_ctx1, 3U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_base_txs2_ctx2, 7U,
            q3_eob_base_txs2_ctx2, 3U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_eob_base_txs4_ctx1, 7U,
            q3_eob_base_txs4_ctx1, 3U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 1U, 17U),
            8U, q3_base_txs1_ctx17, 4U, 18U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 1U, 18U),
            8U, q3_base_txs1_ctx18, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx0, 8U,
            q3_base_txs1_ctx0, 4U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx1, 8U,
            q3_base_txs1_ctx1, 4U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx2, 8U,
            q3_base_txs1_ctx2, 4U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx5, 8U,
            q3_base_txs1_ctx5, 4U, 15U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx6, 8U,
            q3_base_txs1_ctx6, 4U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx7, 8U,
            q3_base_txs1_ctx7, 4U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx10, 8U,
            q3_base_txs1_ctx10, 4U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx11, 8U,
            q3_base_txs1_ctx11, 4U, 15U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs1_ctx12, 8U,
            q3_base_txs1_ctx12, 4U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 1U, 16U),
            8U, q3_base_txs1_ctx16, 4U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs2_ctx11, 8U,
            q3_base_txs2_ctx11, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx0, 10U,
            q3_base_lf_txs1_ctx0, 6U, 18U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs0_ctx11, 10U,
            q3_base_lf_txs0_ctx11, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs0_ctx1, 10U,
            q3_base_lf_txs0_ctx1, 6U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs0_ctx2, 10U,
            q3_base_lf_txs0_ctx2, 6U, 80U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs0_ctx22, 10U,
            q3_base_lf_txs0_ctx22, 6U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs0_ctx21, 10U,
            q3_base_lf_txs0_ctx21, 6U, 4U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx1, 10U,
            q3_base_lf_txs1_ctx1, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx29,
            10U, q3_base_lf_txs2_ctx29, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx21, 10U,
            q3_base_lf_txs1_ctx21, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx22, 10U,
            q3_base_lf_txs1_ctx22, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx23, 10U,
            q3_base_lf_txs1_ctx23, 6U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx28, 10U,
            q3_base_lf_txs1_ctx28, 6U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx29, 10U,
            q3_base_lf_txs1_ctx29, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx30, 10U,
            q3_base_lf_txs1_ctx30, 6U, 2U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx2, 10U,
            q3_base_lf_txs1_ctx2, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx3, 10U,
            q3_base_lf_txs1_ctx3, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx4, 10U,
            q3_base_lf_txs1_ctx4, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx5, 10U,
            q3_base_lf_txs1_ctx5, 6U, 91U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx13, 10U,
            q3_base_lf_txs1_ctx13, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs0_ctx9, 10U,
            q3_base_lf_txs0_ctx9, 6U, 31U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx12, 10U,
            q3_base_lf_txs3_ctx12, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx13, 10U,
            q3_base_lf_txs3_ctx13, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx19, 10U,
            q3_base_lf_txs3_ctx19, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx9, 10U,
            q3_base_lf_txs1_ctx9, 6U, 18U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx10, 10U,
            q3_base_lf_txs1_ctx10, 6U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx11, 10U,
            q3_base_lf_txs1_ctx11, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx12, 10U,
            q3_base_lf_txs1_ctx12, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx16, 10U,
            q3_base_lf_txs1_ctx16, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx17, 10U,
            q3_base_lf_txs1_ctx17, 6U, 78U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx18, 10U,
            q3_base_lf_txs1_ctx18, 6U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs1_ctx19, 10U,
            q3_base_lf_txs1_ctx19, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx3, 10U,
            q3_base_lf_txs2_ctx3, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs2_ctx4, 10U,
            q3_base_lf_txs2_ctx4, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 2U),
            10U, q3_base_lf_ctx2, 6U, 5U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 10U),
            10U, q3_base_lf_ctx10, 6U, 3U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 16U),
            10U, q3_base_lf_ctx16, 6U, 5U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 17U),
            10U, q3_base_lf_ctx17, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 18U),
            10U, q3_base_lf_ctx18, 6U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 11U),
            10U, q3_base_lf_ctx11, 6U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]), 3U, 9U),
            10U, q3_base_lf_ctx9, 6U, 11U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx16, 10U,
            q3_base_lf_txs4_ctx16, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx10, 10U,
            q3_base_lf_txs4_ctx10, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx9, 10U,
            q3_base_lf_txs4_ctx9, 6U, 35U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx1, 10U,
            q3_base_lf_txs4_ctx1, 6U, 51U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx18, 10U,
            q3_base_lf_txs4_ctx18, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx17, 10U,
            q3_base_lf_txs4_ctx17, 6U, 80U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx19, 10U,
            q3_base_lf_txs4_ctx19, 6U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx12, 10U,
            q3_base_lf_txs4_ctx12, 6U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx13, 10U,
            q3_base_lf_txs4_ctx13, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx11, 10U,
            q3_base_lf_txs4_ctx11, 6U, 16U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx3, 10U,
            q3_base_lf_txs4_ctx3, 6U, 31U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs4_ctx2, 10U,
            q3_base_lf_txs4_ctx2, 6U, 26U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx3, 10U,
            q3_base_lf_txs3_ctx3, 6U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx0, 10U,
            q3_base_lf_txs3_ctx0, 6U, 60U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx1, 10U,
            q3_base_lf_txs3_ctx1, 6U, 15U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_lf_txs3_ctx6, 10U,
            q3_base_lf_txs3_ctx6, 6U, 16U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx10, 8U,
            q3_base_txs4_ctx10, 4U, 80U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx11, 8U,
            q3_base_txs4_ctx11, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx5, 8U,
            q3_base_txs4_ctx5, 4U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx6, 8U,
            q3_base_txs4_ctx6, 4U, 76U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx0, 8U,
            q3_base_txs4_ctx0, 4U, 81U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx1, 8U,
            q3_base_txs4_ctx1, 4U, 5U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs4_ctx2, 8U,
            q3_base_txs4_ctx2, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 0U),
            8U, q3_base_txs3_ctx0, 4U, 5U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs2_ctx10,
            8U, q3_base_txs2_ctx10, 4U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs2_ctx5,
            8U, q3_base_txs2_ctx5, 4U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 2U, 1U),
            8U, q3_base_txs2_ctx1, 4U, 93U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 2U, 0U),
            8U, q3_base_txs2_ctx0, 4U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 2U, 2U),
            8U, q3_base_txs2_ctx2, 4U, 90U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs2_ctx6,
            8U, q3_base_txs2_ctx6, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs2_ctx7,
            8U, q3_base_txs2_ctx7, 4U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 1U),
            8U, q3_base_txs3_ctx1, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs3_ctx2, 8U,
            q3_base_txs3_ctx2, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_base_txs2_ctx3, 8U,
            q3_base_txs2_ctx3, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 10U),
            8U, q3_base_txs3_ctx10, 4U, 0U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 11U),
            8U, q3_base_txs3_ctx11, 4U, 75U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 5U),
            8U, q3_base_txs3_ctx5, 4U, 31U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 6U),
            8U, q3_base_txs3_ctx6, 4U, 76U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                         sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), 0U),
            8U, q3_br_lf_ctx0, 4U, 62U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                         sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), 1U),
            8U, q3_br_lf_ctx1, 4U, 1U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                         sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), 2U),
            8U, q3_br_lf_ctx2, 4U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_br_lf_ctx3, 8U, q3_br_lf_ctx3, 4U, 11U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_br_lf_ctx4, 8U, q3_br_lf_ctx4, 4U, 6U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->q3_br_lf_ctx9, 8U, q3_br_lf_ctx9, 4U, 6U);
        if (status != AV2H_OK) return status;
        for (index = 0U; index < 3U; ++index) {
            status = av2h_binary_cdf_init(
                cdfs->dc_sign[index],
                (Av2hBinaryCdfSpec){
                    q3_dc_sign[index], q3_dc_sign_parameter[index], 0U
                });
            if (status != AV2H_OK) return status;
        }
        for (index = 0U; index < 4U; ++index) {
            status = av2h_cdf_init(
                cdfs->q3_uv_eob_lf[index], 9U, q3_uv_eob_lf[index],
                5U, q3_uv_eob_lf_parameter[index], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->q3_uv_eob[index], 7U, q3_uv_eob[index],
                3U, q3_uv_eob_parameter[index], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->q3_uv_br[index], 8U, q3_uv_br[index],
                4U, q3_uv_br_parameter[index], 0U);
            if (status != AV2H_OK) return status;
        }
        for (index = 0U; index < 12U; ++index) {
            status = av2h_cdf_init(
                cdfs->q3_uv_base_lf[index], 10U, q3_uv_base_lf[index],
                6U, q3_uv_base_lf_parameter[index], 0U);
            if (status != AV2H_OK) return status;
            status = av2h_cdf_init(
                cdfs->q3_uv_base[index], 8U, q3_uv_base[index],
                4U, q3_uv_base_parameter[index], 0U);
            if (status != AV2H_OK) return status;
        }
        return AV2H_OK;
    }
    return AV2H_OK;
}
