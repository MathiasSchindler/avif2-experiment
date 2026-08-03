#include "coeff_level.h"

#include "coeff_cdf_all.inc"

uint32_t av2h_debug_target_base[5U][10U];
uint8_t av2h_debug_target_base_count;
uint32_t av2h_debug_target_eob[10U];
uint32_t av2h_debug_target_sign[2U][7U];
uint8_t av2h_debug_target_sign_count;
uint32_t av2h_debug_fsc_coeff[64U][16U];
uint8_t av2h_debug_fsc_coeff_count;
uint32_t av2h_debug_fsc_bob[12U];
uint32_t av2h_debug_chimp_base[64U][18U];
uint8_t av2h_debug_chimp_base_count;
uint32_t av2h_debug_chimp_eob[18U];
static uint8_t debug_target_base_active;

static void initialize_full_level_cdfs(
    Av2hFirstCoeffLevelCdfs *cdfs, uint8_t q_context) {
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

typedef struct {
    uint8_t tx_context;
    uint8_t context;
} Av2hCoeffKey;

typedef struct {
    uint8_t tx_context;
    uint8_t context;
    uint16_t threshold[3U];
    uint8_t parameter;
} Av2hBaseSpec;

typedef struct {
    uint8_t tx_context;
    uint8_t context;
    uint16_t threshold[5U];
    uint8_t parameter;
} Av2hLfSpec;

typedef struct {
    uint8_t context;
    uint16_t threshold[3U];
    uint8_t parameter;
} Av2hBrSpec;

typedef struct {
    uint16_t threshold[3U];
    uint8_t parameter;
} Av2hFscCdfSpec;

#include "fsc_cdf_data.inc"
#include "coeff_cdf_q0_txs4.inc"
#include "coeff_cdf_q0_txs3.inc"
#include "coeff_cdf_q0_txs012.inc"
#include "coeff_cdf_q0_uv.inc"

static Av2hStatus initialize_fsc_cdfs(
    Av2hFirstCoeffLevelCdfs *cdfs, uint8_t q_context) {
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

static uint8_t coefficient_q_context(uint16_t base_q_index) {
    return base_q_index <= 90U ? 0U :
           base_q_index <= 140U ? 1U :
           base_q_index <= 190U ? 2U : 3U;
}

Av2hStatus av2h_first_coeff_level_cdfs_init(Av2hFirstCoeffLevelCdfs *cdfs) {
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
    size_t index;
    Av2hStatus status;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
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
        if (base == 0) return AV2H_UNSUPPORTED;
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
        if (base_lf == 0) return AV2H_UNSUPPORTED;
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
    return 0;
}

static uint16_t *find_context(uint16_t *rows, size_t row_words,
                              const uint8_t *keys, size_t key_count,
                              uint8_t context) {
    size_t index;
    for (index = 0U; index < key_count; ++index) {
        if (keys[index] == context) return rows + index * row_words;
    }
    return 0;
}

static Av2hStatus initialize_level_row(
    uint16_t *row,
    size_t row_words,
    const uint16_t *thresholds,
    size_t symbol_count,
    uint8_t parameter) {
    if (row == 0) return AV2H_UNSUPPORTED;
    return av2h_cdf_init(
        row, row_words, thresholds, symbol_count, parameter, 0U);
}

Av2hStatus av2h_first_coeff_level_cdfs_init_for_q(
    Av2hFirstCoeffLevelCdfs *cdfs,
    uint16_t base_q_index) {
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
    static const uint8_t adjusted_uv_base_q1_parameter[3U] = {
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
    static const uint8_t adjusted_uv_base_v_q1_parameter[3U] = {
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
    Av2hStatus status = av2h_first_coeff_level_cdfs_init(cdfs);
    uint16_t *row;
    if (status != AV2H_OK) return status;
    q_context = coefficient_q_context(base_q_index);
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

static unsigned int minimum(unsigned int left, unsigned int right) {
    return left < right ? left : right;
}

int32_t av2h_coeff_clamp_dequantized_8bit(
    uint64_t magnitude,
    uint8_t sign) {
    if (sign) {
        return magnitude > 32768U ? -32768 : -(int32_t)magnitude;
    }
    return magnitude > 32767U ? 32767 : (int32_t)magnitude;
}

uint32_t av2h_coeff_dequant_step_8bit(uint16_t base_q_index, int delta) {
    static const uint16_t lookup[25U] = {
        64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U, 51U, 52U, 54U,
        55U, 57U, 59U, 60U, 62U, 64U, 66U, 68U, 70U, 72U, 74U, 76U,
        78U
    };
    int q_index = (int)base_q_index + delta;
    if (base_q_index == 0U && delta <= 0) return lookup[0];
    if (q_index < 1) q_index = 1;
    if (q_index > 255) q_index = 255;
    if (q_index < 25) return lookup[q_index];
    return (uint32_t)lookup[((q_index - 1) % 24) + 1] <<
           ((q_index - 1) / 24);
}

/*
 * Exact AVM 8-bit inverse quantization for a single decoded coefficient level.
 * Mirrors av2_read_coeffs_txb() / get_dqv():
 *   dq_hp = (level * dequant_step) & 0xffffff        (24-bit working range)
 *   dq    = ROUND_POWER_OF_TWO(dq_hp, QUANT_TABLE_BITS=3)
 *   dq  >>= shift                                    (av2_get_tx_scale)
 * then the signed clamp to the 8-bit dequantized range.
 */
int32_t av2h_coeff_dequantize_level(
    uint32_t level,
    uint8_t sign,
    uint32_t dequant_step,
    uint8_t shift) {
    uint64_t high_precision =
        ((uint64_t)level * dequant_step) & (uint64_t)0xffffffU;
    uint64_t magnitude = (high_precision + 4U) >> 3U;
    magnitude >>= shift;
    return av2h_coeff_clamp_dequantized_8bit(magnitude, sign);
}

/*
 * Normative running coefficient level for neighbour context, matching
 * set_dc_sign() with COEFF_CONTEXT_BITS=3 / COEFF_CONTEXT_MASK=7.  Note this is
 * distinct from the trace's informational "cul_level_capped" field
 * (sum(min(|level|,15))) which does not carry the DC-sign flags.
 */
uint8_t av2h_coeff_finalize_cul_level(
    uint32_t summed_level,
    int32_t dc_value) {
    uint32_t cul_level = summed_level > 7U ? 7U : summed_level;
    if (dc_value < 0) {
        cul_level |= 8U;
    } else if (dc_value > 0) {
        cul_level += 16U;
    }
    return (uint8_t)cul_level;
}

static uint8_t coeff_dim_log2(uint16_t dimension) {
    uint8_t log2 = 0U;
    while ((dimension >> log2) > 1U) ++log2;
    return log2;
}

Av2hStatus av2h_coeff_tx_geometry_from_dims(
    uint16_t tx_width,
    uint16_t tx_height,
    Av2hCoeffTxGeometry *geometry) {
    uint32_t pels;
    uint8_t low_log2;
    uint8_t high_log2;
    if (geometry == 0) return AV2H_INVALID_ARGUMENT;
    if (tx_width < 4U || tx_width > 64U || tx_height < 4U ||
        tx_height > 64U ||
        (tx_width & (tx_width - 1U)) != 0U ||
        (tx_height & (tx_height - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry->tx_width = tx_width;
    geometry->tx_height = tx_height;
    geometry->width = tx_width > 32U ? 32U : tx_width;
    geometry->height = tx_height > 32U ? 32U : tx_height;
    geometry->coefficient_count =
        (uint32_t)geometry->width * geometry->height;
    pels = (uint32_t)tx_width * tx_height;
    geometry->dequant_shift =
        (uint8_t)((pels > 256U ? 1U : 0U) + (pels > 1024U ? 1U : 0U));
    /* get_txsize_entropy_ctx: (sqr + sqr_up + 1) >> 1, squares indexed by
     * log2(side) - 2 for the minimum and maximum transform sides. */
    low_log2 = coeff_dim_log2(
        tx_width < tx_height ? tx_width : tx_height);
    high_log2 = coeff_dim_log2(
        tx_width > tx_height ? tx_width : tx_height);
    geometry->tx_context = (uint8_t)(
        ((low_log2 - 2U) + (high_log2 - 2U) + 1U) >> 1U);
    return AV2H_OK;
}

Av2hStatus av2h_coeff_tx_geometry(
    Av2hFirstTxSize tx_size,
    Av2hCoeffTxGeometry *geometry) {
    uint16_t tx_width;
    uint16_t tx_height;
    Av2hStatus status;
    if (geometry == 0) return AV2H_INVALID_ARGUMENT;
    switch (tx_size) {
        case AV2H_TX_4X4: tx_width = 4U; tx_height = 4U; break;
        case AV2H_TX_8X8: tx_width = 8U; tx_height = 8U; break;
        case AV2H_TX_16X16: tx_width = 16U; tx_height = 16U; break;
        case AV2H_TX_32X32: tx_width = 32U; tx_height = 32U; break;
        case AV2H_TX_64X64: tx_width = 64U; tx_height = 64U; break;
        case AV2H_TX_4X8: tx_width = 4U; tx_height = 8U; break;
        case AV2H_TX_8X4: tx_width = 8U; tx_height = 4U; break;
        case AV2H_TX_8X16: tx_width = 8U; tx_height = 16U; break;
        case AV2H_TX_16X8: tx_width = 16U; tx_height = 8U; break;
        case AV2H_TX_16X32: tx_width = 16U; tx_height = 32U; break;
        case AV2H_TX_32X16: tx_width = 32U; tx_height = 16U; break;
        case AV2H_TX_64X32: tx_width = 64U; tx_height = 32U; break;
        case AV2H_TX_32X64: tx_width = 32U; tx_height = 64U; break;
        case AV2H_TX_4X16: tx_width = 4U; tx_height = 16U; break;
        case AV2H_TX_16X4: tx_width = 16U; tx_height = 4U; break;
        case AV2H_TX_8X32: tx_width = 8U; tx_height = 32U; break;
        case AV2H_TX_32X8: tx_width = 32U; tx_height = 8U; break;
        case AV2H_TX_16X64: tx_width = 16U; tx_height = 64U; break;
        case AV2H_TX_64X16: tx_width = 64U; tx_height = 16U; break;
        case AV2H_TX_4X32: tx_width = 4U; tx_height = 32U; break;
        case AV2H_TX_32X4: tx_width = 32U; tx_height = 4U; break;
        case AV2H_TX_8X64: tx_width = 8U; tx_height = 64U; break;
        case AV2H_TX_64X8: tx_width = 64U; tx_height = 8U; break;
        case AV2H_TX_4X64: tx_width = 4U; tx_height = 64U; break;
        case AV2H_TX_64X4: tx_width = 64U; tx_height = 4U; break;
        default: return AV2H_UNSUPPORTED;
    }
    status = av2h_coeff_tx_geometry_from_dims(tx_width, tx_height, geometry);
    if (status != AV2H_OK) return status;
    return status;
}

static int q_level_profile_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform) {
    if (cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
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

static unsigned int level_at(const Av2hFirstCoeffBlock *block,
                             int row, int column) {
    if (row < 0 || column < 0 || row >= (int)block->height ||
        column >= (int)block->width) {
        return 0U;
    }
    return block->base_levels[(size_t)row * block->width + (size_t)column];
}

static uint16_t scan_position(uint16_t scan_index, uint16_t width,
                              uint16_t height, uint8_t vertical,
                              uint8_t column_scan) {
    unsigned int diagonal;
    unsigned int remaining = scan_index;
    if (column_scan) {
        return (uint16_t)((scan_index % height) * width + scan_index / height);
    }
    if (vertical) return scan_index;
    for (diagonal = 0U; diagonal < (unsigned int)width + height - 1U;
         ++diagonal) {
        int row = (int)minimum(diagonal, height - 1U);
        int last_row = diagonal >= width ? (int)(diagonal - width + 1U) : 0;
        unsigned int diagonal_count = (unsigned int)(row - last_row + 1);
        if (remaining < diagonal_count) {
            row -= (int)remaining;
            return (uint16_t)((unsigned int)row * width + diagonal -
                              (unsigned int)row);
        }
        remaining -= diagonal_count;
    }
    return AV2H_FIRST_COEFF_CAPACITY;
}

static uint8_t lf_context(const Av2hFirstCoeffBlock *block, uint16_t position,
                          uint8_t vertical, uint8_t horizontal) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude = minimum(level_at(block, row, column + 1), 5U) +
        minimum(level_at(block, row + 1, column), 5U);
    unsigned int context;
    if (vertical) {
        magnitude += minimum(level_at(block, row + 2, column), 3U);
        magnitude += minimum(level_at(block, row + 3, column), 3U);
        magnitude += minimum(level_at(block, row + 4, column), 3U);
        context = (magnitude + 1U) >> 1U;
        if (row == 0) return (uint8_t)(21U + minimum(context, 6U));
        return (uint8_t)(28U + minimum(context, 4U));
    }
    if (horizontal) {
        magnitude += minimum(level_at(block, row, column + 2), 3U);
        magnitude += minimum(level_at(block, row, column + 3), 3U);
        magnitude += minimum(level_at(block, row, column + 4), 3U);
        context = (magnitude + 1U) >> 1U;
        if (column == 0) return (uint8_t)(21U + minimum(context, 6U));
        return (uint8_t)(28U + minimum(context, 4U));
    }
    magnitude += minimum(level_at(block, row + 1, column + 1), 5U);
    magnitude += minimum(level_at(block, row, column + 2), 5U);
    magnitude += minimum(level_at(block, row + 2, column), 5U);
    context = (magnitude + 1U) >> 1U;
    if (position == 0U) return (uint8_t)minimum(context, 8U);
    if (row + column < 2) return (uint8_t)(9U + minimum(context, 6U));
    return (uint8_t)(16U + minimum(context, 4U));
}

static uint8_t base_context(const Av2hFirstCoeffBlock *block,
                            uint16_t position, uint8_t vertical,
                            uint8_t horizontal) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude = minimum(level_at(block, row, column + 1), 3U) +
        minimum(level_at(block, row + 1, column), 3U);
    unsigned int context;
    if (vertical) {
        magnitude += minimum(level_at(block, row + 2, column), 3U);
        magnitude += minimum(level_at(block, row + 3, column), 3U);
        magnitude += minimum(level_at(block, row + 4, column), 3U);
        return (uint8_t)(15U + minimum((magnitude + 1U) >> 1U, 4U));
    }
    if (horizontal) {
        magnitude += minimum(level_at(block, row, column + 2), 3U);
        magnitude += minimum(level_at(block, row, column + 3), 3U);
        magnitude += minimum(level_at(block, row, column + 4), 3U);
        return (uint8_t)(15U + minimum((magnitude + 1U) >> 1U, 4U));
    }
    magnitude += minimum(level_at(block, row + 1, column + 1), 3U);
    magnitude += minimum(level_at(block, row, column + 2), 3U);
    magnitude += minimum(level_at(block, row + 2, column), 3U);
    context = minimum((magnitude + 1U) >> 1U, 4U);
    if (row + column < 6) return (uint8_t)context;
    if (row + column < 8) return (uint8_t)(5U + context);
    return (uint8_t)(10U + context);
}

static uint8_t br_context(const Av2hFirstCoeffBlock *block,
                          uint16_t position, uint8_t vertical, uint8_t low,
                          uint8_t horizontal) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude = minimum(level_at(block, row, column + 1), 5U) +
        minimum(level_at(block, row + 1, column), 5U);
    if (vertical) {
        magnitude += minimum(level_at(block, row + 2, column), 5U);
    } else if (horizontal) {
        magnitude += minimum(level_at(block, row, column + 2), 5U);
    } else {
        magnitude += minimum(level_at(block, row + 1, column + 1), 5U);
    }
    magnitude = minimum((magnitude + 1U) >> 1U, 6U);
    if (!low) return (uint8_t)magnitude;
    if (!vertical && !horizontal && position == 0U) return (uint8_t)magnitude;
    return (uint8_t)(magnitude + 7U);
}

static uint8_t is_low_frequency(uint16_t position, uint16_t width,
                                uint8_t vertical, uint8_t horizontal) {
    unsigned int row = position / width;
    unsigned int column = position % width;
    return (uint8_t)(vertical ? row < 2U :
                     horizontal ? column < 2U : row + column < 4U);
}

static uint8_t eob_context(uint16_t scan_index, uint16_t capacity) {
    if (scan_index == 0U) return 0U;
    if (scan_index <= capacity / 8U) return 1U;
    if (scan_index <= capacity / 4U) return 2U;
    return 3U;
}

static uint8_t uv_level_context(const Av2hFirstCoeffBlock *block,
                                uint16_t position, uint8_t plane,
                                uint8_t low) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int limit = low ? 5U : 3U;
    unsigned int magnitude = minimum(
        level_at(block, row, column + 1), limit) + minimum(
        level_at(block, row + 1, column), limit) + minimum(
        level_at(block, row + 1, column + 1), limit);
    uint8_t context = (uint8_t)minimum((magnitude + 1U) >> 1U, 3U);
    return plane == 2U ? (uint8_t)(context + 4U) : context;
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
    uint8_t context = low ? lf_context(block, position, vertical, transpose) :
                            base_context(block, position, vertical, transpose);
    uint16_t *cdf;
    uint32_t *debug_chimp = 0;
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
        cdf = low ?
            find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                     sizeof(base_lf_keys) / sizeof(base_lf_keys[0]),
                     tx_context, context) :
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]),
                     tx_context, context);
    }
    if (cdf == 0) return AV2H_UNSUPPORTED;
    if (debug_target_base_active) {
        size_t debug_index;
        size_t symbols = low ? 6U : 4U;
        if (av2h_debug_chimp_base_count < 64U) {
            debug_chimp = av2h_debug_chimp_base[
                av2h_debug_chimp_base_count++];
            debug_chimp[0] = position;
            debug_chimp[1] = context;
            debug_chimp[2] = low;
            debug_chimp[3] = (uint32_t)av2h_range_tell(decoder);
            debug_chimp[4] = decoder->range;
            for (debug_index = 0U; debug_index < symbols + 4U;
                 ++debug_index) {
                debug_chimp[5U + debug_index] = cdf[debug_index];
            }
        }
    }
    if (debug_target_base_active && av2h_debug_target_base_count < 5U) {
        size_t debug_index;
        uint32_t *debug =
            av2h_debug_target_base[av2h_debug_target_base_count++];
        debug[0] = position;
        debug[1] = context;
        debug[2] = low;
        debug[3] = decoder->range;
        for (debug_index = 0U; debug_index < 6U; ++debug_index) {
            debug[4U + debug_index] = cdf[debug_index];
        }
    }
    symbol = av2h_range_read_adaptive(decoder, cdf, low ? 6U : 4U);
    if (debug_chimp != 0) {
        debug_chimp[15] = symbol;
        debug_chimp[16] = (uint32_t)av2h_range_tell(decoder);
        debug_chimp[17] = decoder->range;
    }
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    *level = (uint8_t)symbol;
    if (symbol > (low ? 4U : 2U)) {
        uint8_t context_br = br_context(
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
                find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                    sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), context_br)) :
                    (cdfs->q_context < 4U ? cdfs->full_br[context_br] :
                     find_context(&cdfs->br[0][0], 8U, br_keys, 7U,
                                  context_br));
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
    maximum = (uint8_t)minimum((unsigned int)rice + 4U, 6U);
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

static uint8_t fsc_base_context(const Av2hFirstCoeffBlock *block,
                                uint16_t position) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude = minimum(level_at(block, row, column - 1), 3U) +
        minimum(level_at(block, row - 1, column), 3U);
    return (uint8_t)minimum(magnitude, 6U);
}

static uint8_t fsc_br_context(const Av2hFirstCoeffBlock *block,
                              uint16_t position) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude = minimum(level_at(block, row, column - 1), 5U) +
        minimum(level_at(block, row - 1, column), 5U);
    return (uint8_t)minimum(magnitude, 6U);
}

static int fsc_sign_at(const Av2hFirstCoeffBlock *block, int row, int column) {
    int32_t value;
    if (row < 0 || column < 0 || row >= (int)block->height ||
        column >= (int)block->width) {
        return 0;
    }
    value = block->quantized[(size_t)row * block->width + (size_t)column];
    return value < 0 ? -1 : value > 0 ? 1 : 0;
}

static uint8_t fsc_sign_context(const Av2hFirstCoeffBlock *block,
                                uint16_t position) {
    int row = position / block->width;
    int column = position % block->width;
    int sum = fsc_sign_at(block, row, column - 1) +
        fsc_sign_at(block, row - 1, column) +
        fsc_sign_at(block, row - 1, column - 1);
    uint8_t context = sum > 2 ? 5U : sum < -2 ? 6U :
        sum > 0 ? 1U : sum < 0 ? 2U : 0U;
    if (block->base_levels[position] > 3U && context != 0U) context += 2U;
    return context;
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
    av2h_debug_fsc_coeff_count = 0U;
    block->before_levels_bits = av2h_range_tell(decoder);
    bob = (uint16_t)(block->coefficient_count - prefix->eob);
    {
        uint16_t position = scan_position(
            bob, block->width, block->height, 0U, 0U);
        uint8_t context = bob <= block->coefficient_count / 8U ? 0U :
            bob <= block->coefficient_count / 4U ? 1U : 2U;
        uint16_t *cdf = cdfs->fsc_bob_all[tx_context][context];
        uint32_t symbol;
        uint8_t debug_bob = (uint8_t)(
            frame->width == 93U && frame->height == 111U &&
            av2h_range_tell(decoder) > 38000U &&
            av2h_range_tell(decoder) < 38300U);
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 13U;
        if (cdf == 0) return AV2H_UNSUPPORTED;
        if (debug_bob) {
            av2h_debug_fsc_bob[0] = (uint32_t)av2h_range_tell(decoder);
            av2h_debug_fsc_bob[1] = decoder->range;
            av2h_debug_fsc_bob[2] = context;
            for (uint8_t debug_index = 0U; debug_index < 7U; ++debug_index) {
                av2h_debug_fsc_bob[3U + debug_index] = cdf[debug_index];
            }
        }
        symbol = av2h_range_read_adaptive(decoder, cdf, 3U);
        if (debug_bob) {
            av2h_debug_fsc_bob[10] = symbol;
            av2h_debug_fsc_bob[11] = decoder->range;
        }
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)(symbol + 1U);
        if (block->base_levels[position] > 2U) {
            context = fsc_br_context(block, position);
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
        uint16_t position = scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        uint8_t context = fsc_base_context(block, position);
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
            context = fsc_br_context(block, position);
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
        uint16_t position = scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        uint32_t final_level = block->base_levels[position];
        uint8_t context;
        uint16_t *cdf;
        uint8_t sign;
        uint64_t dequantized_magnitude;
        if (final_level == 0U) continue;
        uint32_t *debug = av2h_debug_fsc_coeff[63U];
        if (av2h_debug_fsc_coeff_count < 64U) {
            debug = av2h_debug_fsc_coeff[av2h_debug_fsc_coeff_count++];
        }
        debug[0] = scan_index;
        debug[1] = position;
        debug[2] = final_level;
        debug[3] = (uint32_t)av2h_range_tell(decoder);
        debug[4] = decoder->range;
        context = fsc_sign_context(block, position);
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 17U;
        cdf = cdfs->fsc_sign_all[tx_context][context];
        sign = (uint8_t)av2h_range_read_adaptive(decoder, cdf, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        debug[5] = context;
        debug[6] = sign;
        debug[7] = (uint32_t)av2h_range_tell(decoder);
        debug[8] = decoder->range;
        if (final_level >= 6U) {
            uint32_t high;
            block->diagnostic_context = (uint8_t)high_average;
            block->diagnostic_kind = 18U;
            status = read_high_range(decoder, high_average, &high);
            if (status != AV2H_OK) return status;
            final_level += high;
            high_average = (high_average + high) >> 1U;
        }
        debug[9] = decoder->range;
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
    if (cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
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
    av2h_debug_fsc_coeff_count = 0U;
    debug_target_base_active = (uint8_t)(
        (cdfs->q_context == 0U && tx_context == 4U) ||
        (frame->base_q_index == 192U && frame->width == 240U &&
                 frame->height == 287U &&
                 (tx_context == 2U ||
                    (prefix->tx_size == AV2H_TX_8X8 && prefix->eob == 10U))) ||
        (prefix->tx_size == AV2H_TX_16X16 && prefix->eob == 6U &&
         transform->primary_type == AV2H_TX_ADST_ADST) ||
        (prefix->tx_size == AV2H_TX_64X32 && prefix->eob == 3U &&
         transform->primary_type == AV2H_TX_DCT_DCT) ||
        (prefix->tx_size == AV2H_TX_16X8 && prefix->eob == 6U &&
         transform->primary_type == AV2H_TX_DCT_DCT) ||
        (prefix->tx_size == AV2H_TX_16X8 && prefix->eob == 4U &&
         transform->primary_type == AV2H_TX_DCT_DCT));
    if (debug_target_base_active) av2h_debug_target_base_count = 0U;
    if (debug_target_base_active) av2h_debug_target_sign_count = 0U;
    if (debug_target_base_active) av2h_debug_chimp_base_count = 0U;
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
    if ((transform->primary_type != AV2H_TX_DCT_DCT &&
               transform->primary_type != AV2H_TX_ADST_DCT &&
               transform->primary_type != AV2H_TX_DCT_ADST &&
               transform->primary_type != AV2H_TX_FLIPADST_DCT &&
               transform->primary_type != AV2H_TX_DCT_FLIPADST &&
               transform->primary_type != AV2H_TX_ADST_ADST &&
               transform->primary_type != AV2H_TX_FLIPADST_FLIPADST &&
               transform->primary_type != AV2H_TX_ADST_FLIPADST &&
               transform->primary_type != AV2H_TX_FLIPADST_ADST &&
               transform->primary_type != AV2H_TX_IDTX &&
               transform->primary_type != AV2H_TX_V_DCT &&
               transform->primary_type != AV2H_TX_V_ADST &&
               transform->primary_type != AV2H_TX_H_DCT &&
               transform->primary_type != AV2H_TX_H_ADST &&
               transform->primary_type != AV2H_TX_V_FLIPADST &&
               transform->primary_type != AV2H_TX_H_FLIPADST) ||
        (transform->primary_type == AV2H_TX_V_DCT &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_4X8 &&
         prefix->tx_size != AV2H_TX_8X4 &&
         prefix->tx_size != AV2H_TX_8X8 &&
         prefix->tx_size != AV2H_TX_16X16 &&
         prefix->tx_size != AV2H_TX_4X16 &&
         prefix->tx_size != AV2H_TX_8X16 &&
         prefix->tx_size != AV2H_TX_16X4 &&
         prefix->tx_size != AV2H_TX_4X32) ||
        (transform->primary_type == AV2H_TX_H_ADST &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_4X8 &&
         prefix->tx_size != AV2H_TX_8X4 &&
         prefix->tx_size != AV2H_TX_8X8 &&
         prefix->tx_size != AV2H_TX_16X4 &&
         prefix->tx_size != AV2H_TX_16X8) ||
        (transform->primary_type == AV2H_TX_V_ADST &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_4X8 &&
         prefix->tx_size != AV2H_TX_8X4 &&
         prefix->tx_size != AV2H_TX_16X4 &&
         prefix->tx_size != AV2H_TX_16X8) ||
        (transform->primary_type == AV2H_TX_H_DCT &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_4X8 &&
         prefix->tx_size != AV2H_TX_8X4 &&
         prefix->tx_size != AV2H_TX_8X8 &&
         prefix->tx_size != AV2H_TX_4X16 &&
         prefix->tx_size != AV2H_TX_16X4 &&
         prefix->tx_size != AV2H_TX_32X4) ||
        (transform->primary_type == AV2H_TX_V_FLIPADST &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_4X8 &&
         prefix->tx_size != AV2H_TX_8X4) ||
        (transform->primary_type == AV2H_TX_H_FLIPADST &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_4X8 &&
         prefix->tx_size != AV2H_TX_8X4 &&
         prefix->tx_size != AV2H_TX_8X8 &&
         prefix->tx_size != AV2H_TX_16X8)) {
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
        uint16_t position = scan_position(scan_index, block->width,
                                          block->height, vertical,
                                          column_scan);
        uint8_t low = is_low_frequency(
            position, block->width, vertical, column_scan);
        uint8_t level;
        context = eob_context(scan_index, block->coefficient_count);
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
            if (debug_target_base_active) {
                size_t debug_index;
                av2h_debug_target_eob[0] = position;
                av2h_debug_target_eob[1] = context;
                av2h_debug_target_eob[2] = decoder->range;
                for (debug_index = 0U; debug_index < 7U; ++debug_index) {
                    av2h_debug_target_eob[3U + debug_index] = cdf[debug_index];
                }
                av2h_debug_chimp_eob[0] = position;
                av2h_debug_chimp_eob[1] = context;
                av2h_debug_chimp_eob[2] = low;
                av2h_debug_chimp_eob[3] =
                    (uint32_t)av2h_range_tell(decoder);
                av2h_debug_chimp_eob[4] = decoder->range;
                for (debug_index = 0U; debug_index < 9U; ++debug_index) {
                    av2h_debug_chimp_eob[5U + debug_index] = cdf[debug_index];
                }
            }
            symbol = av2h_range_read_adaptive(decoder, cdf, 5U);
            if (debug_target_base_active) {
                av2h_debug_chimp_eob[15] = symbol;
                av2h_debug_chimp_eob[16] =
                    (uint32_t)av2h_range_tell(decoder);
                av2h_debug_chimp_eob[17] = decoder->range;
            }
            level = (uint8_t)(symbol + 1U);
            block->diagnostic_kind = 8U;
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            if (level > 4U) {
                context = br_context(block, position, vertical, low,
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
                    find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                                 sizeof(br_lf_keys) /
                                     sizeof(br_lf_keys[0]),
                                 context);
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
                cdf = find_key(&cdfs->eob_base[0][0], 7U, eob_base_keys,
                               sizeof(eob_base_keys) /
                                   sizeof(eob_base_keys[0]),
                               tx_context, context);
            }
            if (cdf == 0) return AV2H_UNSUPPORTED;
            if (debug_target_base_active) {
                size_t debug_index;
                av2h_debug_chimp_eob[0] = position;
                av2h_debug_chimp_eob[1] = context;
                av2h_debug_chimp_eob[2] = low;
                av2h_debug_chimp_eob[3] =
                    (uint32_t)av2h_range_tell(decoder);
                av2h_debug_chimp_eob[4] = decoder->range;
                for (debug_index = 0U; debug_index < 7U; ++debug_index) {
                    av2h_debug_chimp_eob[5U + debug_index] = cdf[debug_index];
                }
            }
            symbol = av2h_range_read_adaptive(decoder, cdf, 3U);
            if (debug_target_base_active) {
                av2h_debug_chimp_eob[15] = symbol;
                av2h_debug_chimp_eob[16] =
                    (uint32_t)av2h_range_tell(decoder);
                av2h_debug_chimp_eob[17] = decoder->range;
            }
            level = (uint8_t)(symbol + 1U);
            if (level > 2U) {
                if (!q_br_context_supported(cdfs, 0U, low)) {
                    return AV2H_UNSUPPORTED;
                }
                cdf = cdfs->q_context < 4U ? cdfs->full_br[0U] :
                    find_context(&cdfs->br[0][0], 8U, br_keys, 7U, 0U);
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
        position = scan_position(scan_index, block->width, block->height,
                                 vertical, column_scan);
        low = is_low_frequency(position, block->width, vertical, column_scan);
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
        position = scan_position(scan_index, block->width, block->height,
                                 vertical, column_scan);
        level = block->base_levels[position];
        if (level == 0U) continue;
        uint32_t *debug = av2h_debug_fsc_coeff[63U];
        if (av2h_debug_fsc_coeff_count < 64U) {
            debug = av2h_debug_fsc_coeff[av2h_debug_fsc_coeff_count++];
        }
        debug[0] = scan_index;
        debug[1] = position;
        debug[2] = level;
        debug[3] = (uint32_t)av2h_range_tell(decoder);
        debug[4] = decoder->range;
        uint8_t adaptive_sign = (uint8_t)(
            scan_index == 0U ||
            (vertical && position / block->width == 0U) ||
            (column_scan && position % block->width == 0U));
        uint8_t sign_context = scan_index == 0U ? dc_sign_context : 0U;
        block->diagnostic_position = position;
        block->diagnostic_context = sign_context;
        block->diagnostic_kind = adaptive_sign ? 4U : 5U;
        if (debug_target_base_active && av2h_debug_target_sign_count < 2U) {
            uint32_t *debug =
                av2h_debug_target_sign[av2h_debug_target_sign_count++];
            debug[0] = scan_index;
            debug[1] = position;
            debug[2] = adaptive_sign ? sign_context : 255U;
            debug[3] = decoder->range;
            debug[4] = adaptive_sign ? cdfs->dc_sign[sign_context][0] : 0U;
            debug[5] = adaptive_sign ? cdfs->dc_sign[sign_context][1] : 0U;
            debug[6] = adaptive_sign ? cdfs->dc_sign[sign_context][2] : 0U;
        }
        if (adaptive_sign) {
            sign = (uint8_t)av2h_range_read_adaptive(
                decoder, cdfs->dc_sign[sign_context], 2U);
        } else {
            sign = (uint8_t)av2h_range_read_bypass(decoder, 1U);
        }
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        debug[5] = adaptive_sign ? sign_context : 255U;
        if (adaptive_sign) {
            size_t cdf_index;
            for (cdf_index = 0U; cdf_index < 6U; ++cdf_index) {
                debug[10U + cdf_index] =
                    cdfs->dc_sign[sign_context][cdf_index];
            }
        }
        debug[6] = sign;
        debug[7] = (uint32_t)av2h_range_tell(decoder);
        debug[8] = decoder->range;
        final_level = level;
        low = is_low_frequency(position, block->width, vertical, column_scan);
        if (final_level >= (low ? 8U : 6U)) {
            uint32_t high;
            block->diagnostic_context = (uint8_t)high_average;
            block->diagnostic_kind = 6U;
            status = read_high_range(decoder, high_average, &high);
            if (status != AV2H_OK) return status;
            final_level += high;
            high_average = (high_average + high) >> 1U;
        }
        debug[9] = decoder->range;
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
    if (cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
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
        uint16_t position = scan_position(
            (uint16_t)(prefix->eob - 1U), block->width, block->height,
            0U, 0U);
        uint8_t context = eob_context(
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
        position = scan_position(
            scan_index, block->width, block->height, 0U, 0U);
        low = (uint8_t)(position == 0U);
        context = uv_level_context(block, position, plane, low);
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
                uv_level_context(block, position, 1U, 1U);
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
        position = scan_position(
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