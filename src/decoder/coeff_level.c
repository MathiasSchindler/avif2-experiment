#include "coeff_level.h"

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

static const Av2hCoeffKey eob_base_keys[] = {
    { 0U, 3U }, { 1U, 3U }, { 3U, 1U }, { 1U, 2U }, { 2U, 3U },
    { 2U, 2U }
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
    { 3U, 0U }, { 3U, 1U }, { 3U, 5U }, { 3U, 6U },
    { 1U, 0U }, { 2U, 0U }, { 2U, 1U }, { 2U, 5U }, { 2U, 15U },
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
    { 3U, 2U }, { 3U, 5U }, { 3U, 9U }, { 3U, 10U }, { 3U, 11U },
    { 3U, 16U }, { 3U, 17U }, { 3U, 18U }, { 3U, 6U }, { 3U, 13U },
    { 2U, 1U }, { 2U, 2U }, { 2U, 9U }, { 2U, 10U }, { 2U, 16U },
    { 2U, 17U }, { 2U, 21U }, { 2U, 22U }, { 2U, 23U },
    { 2U, 28U }, { 2U, 29U }
};
static const uint8_t br_keys[] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U };
static const uint8_t br_lf_keys[] = {
    0U, 1U, 2U, 4U, 5U, 6U, 7U, 9U, 10U, 11U, 13U
};
static const uint8_t fsc_base_keys[] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U };
static const uint8_t fsc_bob_keys[] = { 0U, 1U, 2U };
static const uint8_t fsc_br_keys[] = { 0U, 1U, 5U };
static const uint8_t fsc_sign_keys[] = { 0U, 1U, 2U, 3U, 4U };

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
    { 3U, 5U, { 30905U, 32733U, 32748U }, 75U },
    { 3U, 6U, { 26894U, 32528U, 32731U }, 76U },
    { 1U, 0U, { 25334U, 31967U, 32621U }, 90U },
    { 2U, 0U, { 27264U, 32622U, 32748U }, 75U },
    { 2U, 1U, { 22849U, 32019U, 32682U }, 75U },
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
    { 3U, 9U, { 26289U, 31974U, 32589U, 32707U, 32732U }, 0U },
    { 3U, 10U, { 14722U, 28032U, 31407U, 32465U, 32660U }, 0U },
    { 3U, 11U, { 14143U, 25751U, 29446U, 31254U, 32093U }, 75U },
    { 3U, 16U, { 28318U, 32551U, 32737U, 32741U, 32745U }, 75U },
    { 3U, 17U, { 21238U, 31149U, 32509U, 32707U, 32732U }, 75U },
    { 3U, 18U, { 16317U, 28616U, 31700U, 32471U, 32706U }, 1U },
    { 3U, 6U, { 1314U, 5852U, 7870U, 11057U, 13558U }, 75U },
    { 3U, 13U, { 4022U, 11170U, 17442U, 21682U, 24925U }, 75U }
    , { 2U, 1U, { 4579U, 20789U, 27200U, 30357U, 31565U }, 0U }
    , { 2U, 2U, { 10553U, 22962U, 27982U, 30214U, 31394U }, 1U }
    , { 2U, 9U, { 25676U, 31984U, 32605U, 32712U, 32732U }, 75U }
    , { 2U, 10U, { 13894U, 27451U, 31145U, 32228U, 32639U }, 15U }
    , { 2U, 16U, { 26506U, 32458U, 32715U, 32736U, 32740U }, 90U }
    , { 2U, 17U, { 20516U, 31118U, 32508U, 32699U, 32732U }, 75U }
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
    static const uint16_t eob_lf_txs3_ctx1[] = {
        32054U, 32674U, 32735U, 32740U
    };
    static const uint16_t eob_lf_txs3_ctx0[] = {
        22871U, 30729U, 31682U, 32000U
    };
    static const uint16_t dc_sign[3U] = { 16540U, 14804U, 18312U };
    static const uint16_t fsc_bob[][2U] = {
        { 16673U, 23002U }, { 19890U, 25933U }, { 18355U, 25595U }
    };
    static const uint8_t fsc_bob_parameter[] = { 75U, 90U, 75U };
    static const uint16_t fsc_base[][3U] = {
        { 28035U, 30893U, 31756U },
        { 18097U, 26957U, 29476U },
        { 15075U, 22557U, 26893U },
        { 11632U, 16975U, 20254U },
        { 5659U, 12007U, 16445U },
        { 3855U, 7854U, 12411U },
        { 2360U, 4192U, 5868U }
    };
    static const uint8_t fsc_base_parameter[] = {
        78U, 75U, 0U, 0U, 0U, 1U, 0U
    };
    static const uint16_t fsc_br[][3U] = {
        { 11825U, 18784U, 22974U },
        { 10844U, 17662U, 22290U },
        { 5350U, 9801U, 14462U }
    };
    static const uint8_t fsc_br_parameter[] = { 0U, 0U, 5U };
    static const uint16_t fsc_sign[] = {
        17010U, 25832U, 7718U, 29537U, 8925U
    };
    static const uint8_t fsc_sign_parameter[] = { 0U, 0U, 3U, 1U, 31U };
    static const uint16_t fsc_bob_txs1_ctx2[] = { 22018U, 27887U };
    static const uint16_t fsc_base_txs1_ctx0[] = {
        31009U, 32323U, 32574U
    };
    static const uint16_t fsc_base_txs1_ctx3[] = {
        12426U, 17800U, 21533U
    };
    static const uint16_t fsc_base_txs1_ctx6[] = {
        2473U, 4631U, 6923U
    };
    static const uint16_t fsc_br_txs1_ctx0[] = {
        13939U, 20762U, 24451U
    };
    static const uint16_t fsc_br_txs1_ctx5[] = {
        5645U, 10671U, 15100U
    };
    static const uint16_t fsc_br_txs1_ctx6[] = {
        4335U, 8679U, 12492U
    };
    static const uint16_t fsc_bob_txs2[][2U] = {
        { 22395U, 27670U }, { 24275U, 28656U }, { 22595U, 28026U }
    };
    static const uint8_t fsc_bob_txs2_parameter[] = { 31U, 31U, 1U };
    static const uint16_t fsc_base_txs2[][3U] = {
        { 32079U, 32614U, 32706U },
        { 22634U, 30655U, 31942U },
        { 16704U, 25079U, 29917U },
        { 12795U, 17039U, 20729U },
        { 5362U, 11212U, 18340U },
        { 3513U, 6916U, 12694U },
        { 1883U, 3540U, 5856U }
    };
    static const uint8_t fsc_base_txs2_parameter[] = {
        75U, 0U, 31U, 6U, 37U, 31U, 6U
    };
    static const uint16_t fsc_br_txs2[][3U] = {
        { 14250U, 21163U, 24941U },
        { 17217U, 23934U, 27433U },
        { 17090U, 23993U, 27574U },
        { 19725U, 25425U, 28697U },
        { 12700U, 23765U, 28251U },
        { 6542U, 11855U, 17108U },
        { 5101U, 10468U, 15443U }
    };
    static const uint8_t fsc_br_txs2_parameter[] = {
        37U, 32U, 37U, 37U, 37U, 37U, 37U
    };
    static const uint16_t fsc_sign_txs2[] = {
        19157U, 30551U, 2788U, 30752U, 701U,
        32565U, 359U, 32515U, 223U
    };
    static const uint8_t fsc_sign_txs2_parameter[] = {
        31U, 31U, 31U, 37U, 26U, 15U, 8U, 87U, 37U
    };
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
        for (index = 0U;
            index < sizeof(base_specs) / sizeof(base_specs[0]); ++index) {
        status = av2h_cdf_init(cdfs->base[index], 8U,
                               base_specs[index].threshold, 4U,
                               base_specs[index].parameter, 0U);
        if (status != AV2H_OK) return status;
    }
        for (index = 0U;
            index < sizeof(base_lf_specs) / sizeof(base_lf_specs[0]); ++index) {
        status = av2h_cdf_init(cdfs->base_lf[index], 10U,
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
    for (index = 0U; index < 3U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_bob[index], 7U, fsc_bob[index],
                               3U, fsc_bob_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 7U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_base[index], 8U, fsc_base[index],
                               4U, fsc_base_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 3U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_br[index], 8U, fsc_br[index], 4U,
                               fsc_br_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 5U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_sign[index], AV2H_BINARY_CDF_SIZE,
                               &fsc_sign[index], 2U,
                               fsc_sign_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    status = av2h_cdf_init(cdfs->fsc_bob_txs1_ctx2, 7U,
                           fsc_bob_txs1_ctx2, 3U, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->fsc_base_txs1_ctx0, 8U,
                           fsc_base_txs1_ctx0, 4U, 78U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->fsc_base_txs1_ctx3, 8U,
                           fsc_base_txs1_ctx3, 4U, 0U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->fsc_base_txs1_ctx6, 8U,
                           fsc_base_txs1_ctx6, 4U, 5U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->fsc_br_txs1_ctx0, 8U,
                           fsc_br_txs1_ctx0, 4U, 31U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->fsc_br_txs1_ctx5, 8U,
                           fsc_br_txs1_ctx5, 4U, 30U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(cdfs->fsc_br_txs1_ctx6, 8U,
                           fsc_br_txs1_ctx6, 4U, 31U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->fsc_sign_txs1_ctx0,
        (Av2hBinaryCdfSpec){ 17528U, 25U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->fsc_sign_txs1_ctx4,
        (Av2hBinaryCdfSpec){ 2848U, 31U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->fsc_sign_txs1_ctx8,
        (Av2hBinaryCdfSpec){ 474U, 18U, 0U });
    if (status != AV2H_OK) return status;
    for (index = 0U; index < 3U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_bob_txs2[index], 7U,
                               fsc_bob_txs2[index], 3U,
                               fsc_bob_txs2_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 7U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_base_txs2[index], 8U,
                               fsc_base_txs2[index], 4U,
                               fsc_base_txs2_parameter[index], 0U);
        if (status != AV2H_OK) return status;
        status = av2h_cdf_init(cdfs->fsc_br_txs2[index], 8U,
                               fsc_br_txs2[index], 4U,
                               fsc_br_txs2_parameter[index], 0U);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < 9U; ++index) {
        status = av2h_cdf_init(cdfs->fsc_sign_txs2[index],
                               AV2H_BINARY_CDF_SIZE,
                               &fsc_sign_txs2[index], 2U,
                               fsc_sign_txs2_parameter[index], 0U);
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
    return av2h_cdf_init(cdfs->uv_br_ctx3, 8U, uv_br_ctx3, 4U, 75U, 0U);
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
    static const uint16_t q2_eob_base_txs3_ctx1[] = { 32684U, 32756U };
    static const uint16_t q2_eob_lf_txs3_ctx1[] = {
        32167U, 32698U, 32744U, 32748U
    };
    static const uint16_t q2_base_ctx0[] = {
        27184U, 32672U, 32748U
    };
    static const uint16_t q2_base_lf_ctx2[] = {
        10730U, 22881U, 27929U, 30084U, 31344U
    };
    static const uint16_t q2_base_lf_ctx6[] = {
        1055U, 5756U, 9129U, 12574U, 14838U
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
    static const uint16_t q3_eob_lf_txs3_ctx1[] = {
        32297U, 32711U, 32744U, 32748U
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
    static const uint16_t q3_br_lf_ctx0[] = {
        13965U, 20979U, 26518U
    };
    static const uint16_t q3_dc_sign[] = { 20108U, 14675U, 20547U };
    static const uint8_t q3_dc_sign_parameter[] = { 1U, 0U, 93U };
    uint8_t q_context;
    size_t index;
    Av2hStatus status = av2h_first_coeff_level_cdfs_init(cdfs);
    uint16_t *row;
    if (status != AV2H_OK) return status;
    q_context = coefficient_q_context(base_q_index);
    cdfs->q_context = q_context;
    if (q_context == 1U) return AV2H_OK;
    if (q_context == 2U) {
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
            find_key(&cdfs->base[0][0], 8U, base_keys,
                     sizeof(base_keys) / sizeof(base_keys[0]), 3U, 0U),
            8U, q2_base_ctx0, 4U, 1U);
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
            cdfs->eob_lf[8], 9U, q3_eob_lf_txs3_ctx0, 5U, 7U);
        if (status != AV2H_OK) return status;
        status = initialize_level_row(
            cdfs->eob_lf[7], 9U, q3_eob_lf_txs3_ctx1, 5U, 90U);
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
            find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
                         sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), 0U),
            8U, q3_br_lf_ctx0, 4U, 62U);
        if (status != AV2H_OK) return status;
        for (index = 0U; index < 3U; ++index) {
            status = av2h_binary_cdf_init(
                cdfs->dc_sign[index],
                (Av2hBinaryCdfSpec){
                    q3_dc_sign[index], q3_dc_sign_parameter[index], 0U
                });
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

static int q_level_profile_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform) {
    if (cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
        return 0;
    }
    if (frame->base_q_index == 128U) {
        return !(prefix->tx_size == AV2H_TX_8X16 &&
                 prefix->eob == 6U &&
                 transform->primary_type == AV2H_TX_DCT_DCT &&
                 transform->secondary_symbol == 0U);
    }
    if (cdfs->q_context == 2U && prefix->tx_size == AV2H_TX_16X32) {
        return (prefix->eob == 5U &&
                transform->primary_type == AV2H_TX_DCT_DCT) ||
               (prefix->eob == 13U &&
                transform->primary_type == AV2H_TX_DCT_FLIPADST);
    }
    return cdfs->q_context == 3U &&
           prefix->tx_size == AV2H_TX_32X32 &&
           (prefix->eob == 1U || prefix->eob == 5U) &&
           transform->primary_type == AV2H_TX_DCT_DCT;
}

static int q_eob_context_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    uint8_t tx_context,
    uint8_t context,
    uint8_t low) {
    if (cdfs->q_context == 1U) return 1;
    if (cdfs->q_context == 2U) {
        return tx_context == 3U && context == 1U;
    }
    return cdfs->q_context == 3U && low && tx_context == 3U &&
           (context == 0U || context == 1U);
}

static int q_base_context_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    uint8_t tx_context,
    uint8_t context,
    uint8_t low) {
    if (cdfs->q_context == 1U) return 1;
    if (tx_context != 3U) return 0;
    if (cdfs->q_context == 2U) {
        if (!low) return context == 0U;
        return context == 2U || context == 6U || context == 10U ||
               context == 13U || context == 16U || context == 17U ||
               context == 18U;
    }
    return cdfs->q_context == 3U && low &&
           (context == 2U || context == 10U || context == 16U);
}

static int q_br_context_supported(
    const Av2hFirstCoeffLevelCdfs *cdfs,
    uint8_t context,
    uint8_t low) {
    if (cdfs->q_context == 1U) return 1;
    if (!low) return 0;
    if (cdfs->q_context == 2U) {
        return context == 1U || context == 5U;
    }
    return cdfs->q_context == 3U && context == 0U;
}

static uint32_t qtx_8bit(uint16_t base_q_index, int delta) {
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

static uint16_t *uv_base_cdf(Av2hFirstCoeffLevelCdfs *cdfs,
                             uint8_t context, uint8_t low) {
    if (low) {
        return context == 1U ? cdfs->uv_base_lf_ctx1 :
            context == 2U ? cdfs->uv_base_lf[0] :
            context == 3U ? cdfs->uv_base_lf_ctx3 :
            context == 5U ? cdfs->uv_base_lf_ctx5 :
            context == 6U ? cdfs->uv_base_lf[1] :
            context == 7U ? cdfs->uv_base_lf_ctx7 : 0;
    }
    return context == 0U ? cdfs->uv_base[0] :
        context == 1U ? cdfs->uv_base_ctx1 :
        context == 3U ? cdfs->uv_base_ctx3 :
        context == 4U ? cdfs->uv_base[1] :
        context == 5U ? cdfs->uv_base_ctx5 :
        context == 6U ? cdfs->uv_base_ctx6 :
        context == 7U ? cdfs->uv_base_ctx7 : 0;
}

static uint16_t *uv_br_cdf(Av2hFirstCoeffLevelCdfs *cdfs,
                           const Av2hFirstCoeffBlock *block,
                           uint16_t position) {
    uint8_t context = uv_level_context(block, position, 1U, 1U);
    return context == 0U ? cdfs->uv_br_ctx0 :
        context == 1U ? cdfs->uv_br_ctx1 :
        context == 2U ? cdfs->uv_br_ctx2 :
        context == 3U ? cdfs->uv_br_ctx3 : 0;
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
    uint32_t symbol;
    block->diagnostic_position = position;
    block->diagnostic_context = context;
    block->diagnostic_kind = low ? 1U : 0U;
    if (!q_base_context_supported(cdfs, tx_context, context, low)) {
        return AV2H_UNSUPPORTED;
    }
    cdf = low ?
        find_key(&cdfs->base_lf[0][0], 10U, base_lf_keys,
                 sizeof(base_lf_keys) / sizeof(base_lf_keys[0]),
                 tx_context, context) :
        find_key(&cdfs->base[0][0], 8U, base_keys,
                 sizeof(base_keys) / sizeof(base_keys[0]),
                 tx_context, context);
    if (cdf == 0) return AV2H_UNSUPPORTED;
    symbol = av2h_range_read_adaptive(decoder, cdf, low ? 6U : 4U);
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
        cdf = low ? find_context(
            &cdfs->br_lf[0][0], 8U, br_lf_keys,
            sizeof(br_lf_keys) / sizeof(br_lf_keys[0]), context_br) :
                    find_context(&cdfs->br[0][0], 8U, br_keys, 7U,
                                 context_br);
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
    uint8_t txs1;
    uint8_t txs2;
    Av2hStatus status;
    if (sequence->enable_tcq || sequence->enable_parity_hiding ||
        frame->base_q_index != 128U || frame->using_qmatrix ||
        frame->y_dc_delta_q != 0 ||
        (prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_8X16 &&
         prefix->tx_size != AV2H_TX_16X4) ||
        prefix->all_zero ||
        transform->primary_type != AV2H_TX_IDTX ||
        transform->secondary_symbol != 0U) {
        return AV2H_UNSUPPORTED;
    }
    txs1 = (uint8_t)(prefix->tx_size == AV2H_TX_16X4);
    txs2 = (uint8_t)(prefix->tx_size == AV2H_TX_8X16);
    block->width = prefix->tx_size == AV2H_TX_16X4 ? 16U :
        txs2 ? 8U : 4U;
    block->height = prefix->tx_size == AV2H_TX_16X4 ? 4U :
        txs2 ? 16U : 4U;
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
        uint16_t position = scan_position(
            bob, block->width, block->height, 0U, 0U);
        uint8_t context = bob <= 2U ? 0U : bob <= 4U ? 1U : 2U;
        uint16_t *cdf = txs1 ?
            (context == 2U ? cdfs->fsc_bob_txs1_ctx2 : 0) :
            txs2 ? cdfs->fsc_bob_txs2[context] :
            find_context(&cdfs->fsc_bob[0][0], 7U,
                         fsc_bob_keys, 3U, context);
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
            context = fsc_br_context(block, position);
            block->diagnostic_context = context;
            block->diagnostic_kind = 14U;
            cdf = txs1 ?
                (context == 0U ? cdfs->fsc_br_txs1_ctx0 : 0) :
                find_context(&cdfs->fsc_br[0][0], 8U, fsc_br_keys, 3U,
                             context);
            if (cdf == 0) return AV2H_UNSUPPORTED;
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
        uint16_t *cdf = txs1 ?
            (context == 0U ? cdfs->fsc_base_txs1_ctx0 :
             context == 3U ? cdfs->fsc_base_txs1_ctx3 :
             context == 6U ? cdfs->fsc_base_txs1_ctx6 : 0) :
            txs2 ? cdfs->fsc_base_txs2[context] :
            find_context(&cdfs->fsc_base[0][0], 8U,
                         fsc_base_keys, 7U, context);
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
            cdf = txs1 ?
                (context == 0U ? cdfs->fsc_br_txs1_ctx0 :
                 context == 5U ? cdfs->fsc_br_txs1_ctx5 :
                 context == 6U ? cdfs->fsc_br_txs1_ctx6 : 0) :
                txs2 ? cdfs->fsc_br_txs2[context] :
                find_context(&cdfs->fsc_br[0][0], 8U, fsc_br_keys, 3U,
                             context);
            if (cdf == 0) return AV2H_UNSUPPORTED;
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
        context = fsc_sign_context(block, position);
        block->diagnostic_position = position;
        block->diagnostic_context = context;
        block->diagnostic_kind = 17U;
        cdf = txs1 ?
            (context == 0U ? cdfs->fsc_sign_txs1_ctx0 :
             context == 4U ? cdfs->fsc_sign_txs1_ctx4 :
             context == 8U ? cdfs->fsc_sign_txs1_ctx8 : 0) :
            txs2 ? cdfs->fsc_sign_txs2[context] :
            find_context(&cdfs->fsc_sign[0][0], AV2H_BINARY_CDF_SIZE,
                         fsc_sign_keys, 5U, context);
        if (cdf == 0) return AV2H_UNSUPPORTED;
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
        dequantized_magnitude =
            ((uint64_t)final_level * 1568U + 4U) >> 3U;
        block->dequantized[position] =
            av2h_coeff_clamp_dequantized_8bit(
                dequantized_magnitude, sign);
        ++block->nonzero_count;
        cul_level += final_level;
    }
    block->cul_level = (uint8_t)minimum(cul_level, 7U);
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
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        mode == 0 || prefix == 0 || transform == 0 || block == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cdfs->q_context != coefficient_q_context(frame->base_q_index)) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->fsc_mode ||
        (mode->use_intrabc &&
         transform->primary_type == AV2H_TX_IDTX)) {
        return read_fsc_block(decoder, cdfs, sequence, frame, prefix,
                              transform, block);
    }
    if (sequence->enable_tcq || sequence->enable_parity_hiding ||
        frame->using_qmatrix || mode->fsc_mode ||
        transform->secondary_symbol != 0U || prefix->all_zero ||
        prefix->eob == 0U || dc_sign_context >= 3U ||
        !q_level_profile_supported(cdfs, frame, prefix, transform)) {
        return AV2H_UNSUPPORTED;
    }
    if (prefix->tx_size == AV2H_TX_4X4) {
        block->width = 4U;
        block->height = 4U;
        tx_context = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8) {
        block->width = 4U;
        block->height = 8U;
        tx_context = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X4) {
        block->width = 8U;
        block->height = 4U;
        tx_context = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8) {
        block->width = 8U;
        block->height = 8U;
        tx_context = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X16) {
        block->width = 8U;
        block->height = 16U;
        tx_context = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8) {
        block->width = 16U;
        block->height = 8U;
        tx_context = 2U;
    } else if (prefix->tx_size == AV2H_TX_16X4) {
        block->width = 16U;
        block->height = 4U;
        tx_context = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X32) {
        block->width = 8U;
        block->height = 32U;
        tx_context = 2U;
    } else if (prefix->tx_size == AV2H_TX_32X8) {
        block->width = 32U;
        block->height = 8U;
        tx_context = 2U;
    } else if (prefix->tx_size == AV2H_TX_16X32) {
        block->width = 16U;
        block->height = 32U;
        tx_context = 3U;
    } else if (prefix->tx_size == AV2H_TX_32X32) {
        block->width = 32U;
        block->height = 32U;
        tx_context = 3U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    vertical = (uint8_t)(transform->primary_type == AV2H_TX_V_DCT ||
                         transform->primary_type == AV2H_TX_V_ADST);
    column_scan = (uint8_t)(transform->primary_type == AV2H_TX_H_DCT ||
                            transform->primary_type == AV2H_TX_H_ADST ||
                            transform->primary_type ==
                                AV2H_TX_H_FLIPADST);
    if ((transform->primary_type != AV2H_TX_DCT_DCT &&
               transform->primary_type != AV2H_TX_ADST_DCT &&
               transform->primary_type != AV2H_TX_DCT_ADST &&
               transform->primary_type != AV2H_TX_DCT_FLIPADST &&
               transform->primary_type != AV2H_TX_ADST_ADST &&
               transform->primary_type != AV2H_TX_ADST_FLIPADST &&
               transform->primary_type != AV2H_TX_FLIPADST_ADST &&
               transform->primary_type != AV2H_TX_IDTX &&
               transform->primary_type != AV2H_TX_V_DCT &&
               transform->primary_type != AV2H_TX_V_ADST &&
               transform->primary_type != AV2H_TX_H_DCT &&
               transform->primary_type != AV2H_TX_H_ADST &&
               transform->primary_type != AV2H_TX_H_FLIPADST) ||
        (transform->primary_type == AV2H_TX_V_DCT &&
         prefix->tx_size != AV2H_TX_8X8) ||
        (transform->primary_type == AV2H_TX_H_ADST &&
         prefix->tx_size != AV2H_TX_4X4 &&
         prefix->tx_size != AV2H_TX_8X32) ||
        (transform->primary_type == AV2H_TX_H_DCT &&
         prefix->tx_size != AV2H_TX_8X32) ||
        (transform->primary_type == AV2H_TX_H_FLIPADST &&
         prefix->tx_size != AV2H_TX_4X4)) {
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
            if ((tx_context == 0U && context == 1U)) {
                cdf = cdfs->eob_lf[0];
            } else if (tx_context == 0U && context == 3U) {
                cdf = cdfs->eob_lf[4];
            } else if (tx_context == 1U && context == 3U) {
                cdf = cdfs->eob_lf[1];
            } else if (tx_context == 1U && context == 2U) {
                cdf = cdfs->eob_lf[2];
            } else if (tx_context == 1U && context == 1U) {
                cdf = cdfs->eob_lf[3];
            } else if (tx_context == 1U && context == 0U) {
                cdf = cdfs->eob_lf[6];
            } else if (tx_context == 2U && context == 1U) {
                cdf = cdfs->eob_lf[5];
            } else if (tx_context == 3U && context == 1U) {
                cdf = cdfs->eob_lf[7];
            } else if (tx_context == 3U && context == 0U) {
                cdf = cdfs->eob_lf[8];
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
                context = br_context(block, position, vertical, low,
                                     column_scan);
                block->diagnostic_context = context;
                block->diagnostic_kind = 9U;
                if (!q_br_context_supported(cdfs, context, low)) {
                    return AV2H_UNSUPPORTED;
                }
                cdf = find_context(&cdfs->br_lf[0][0], 8U, br_lf_keys,
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
            cdf = find_key(&cdfs->eob_base[0][0], 7U, eob_base_keys,
                           sizeof(eob_base_keys) / sizeof(eob_base_keys[0]),
                           tx_context, context);
            if (cdf == 0) return AV2H_UNSUPPORTED;
            symbol = av2h_range_read_adaptive(decoder, cdf, 3U);
            level = (uint8_t)(symbol + 1U);
            if (level > 2U) {
                if (!q_br_context_supported(cdfs, 0U, low)) {
                    return AV2H_UNSUPPORTED;
                }
                cdf = find_context(&cdfs->br[0][0], 8U, br_keys, 7U, 0U);
                if (cdf == 0) return AV2H_UNSUPPORTED;
                symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                level = (uint8_t)(level + symbol);
                if (symbol == 3U) return AV2H_UNSUPPORTED;
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
        uint64_t dequantized_magnitude;
        uint32_t dequant;
        --scan_index;
        position = scan_position(scan_index, block->width, block->height,
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
        if (final_level > 32767U) return AV2H_INVALID_DATA;
        block->quantized[position] = sign ? -(int32_t)final_level :
                                             (int32_t)final_level;
        dequant = qtx_8bit(
            frame->base_q_index,
            scan_index == 0U ?
                frame->y_dc_delta_q + sequence->base_y_dc_delta_q : 0);
        dequantized_magnitude =
            ((uint64_t)final_level * dequant + 4U) >> 3U;
        if (block->coefficient_count > 256U) dequantized_magnitude >>= 1U;
        block->dequantized[position] =
            av2h_coeff_clamp_dequantized_8bit(
                dequantized_magnitude, sign);
        ++block->nonzero_count;
        cul_level += final_level;
        if (scan_index == 0U) {
            block->dc_value = block->quantized[position];
        }
    }
    cul_level = minimum(cul_level, 7U);
    if (block->dc_value < 0) {
        cul_level |= 8U;
    } else if (block->dc_value > 0) {
        cul_level += 16U;
    }
    block->cul_level = (uint8_t)cul_level;
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

Av2hStatus av2h_first_chroma_coeff_block_read(
    Av2hRangeDecoder *decoder,
    Av2hFirstCoeffLevelCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t plane,
    const Av2hFirstCoeffPrefix *prefix,
    Av2hFirstCoeffBlock *block) {
    uint16_t scan_index;
    uint32_t high_average = 0U;
    uint32_t cul_level = 0U;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        prefix == 0 || block == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((plane != 1U && plane != 2U) || sequence->enable_tcq ||
        sequence->enable_parity_hiding || frame->base_q_index != 128U ||
        frame->using_qmatrix || frame->u_dc_delta_q != 0 ||
        frame->u_ac_delta_q != 0 || frame->v_dc_delta_q != 0 ||
        frame->v_ac_delta_q != 0) {
        return AV2H_UNSUPPORTED;
    }
    if (prefix->all_zero && prefix->eob == 0U &&
        (prefix->tx_size == AV2H_TX_4X4 ||
         prefix->tx_size == AV2H_TX_4X8)) {
        block->width = 4U;
        block->height = prefix->tx_size == AV2H_TX_4X8 ? 8U : 4U;
        block->coefficient_count =
            (uint16_t)(block->width * block->height);
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
    if (prefix->tx_size != AV2H_TX_4X4 || prefix->all_zero ||
        prefix->eob == 0U || prefix->eob > 16U) {
        return AV2H_UNSUPPORTED;
    }
    block->width = 4U;
    block->height = 4U;
    block->coefficient_count = 16U;
    for (scan_index = 0U; scan_index < AV2H_FIRST_COEFF_CAPACITY;
         ++scan_index) {
        block->base_levels[scan_index] = 0U;
        block->quantized[scan_index] = 0;
        block->dequantized[scan_index] = 0;
    }
    block->nonzero_count = 0U;
    block->dc_value = 0;
    block->before_levels_bits = av2h_range_tell(decoder);
    {
        uint16_t position = scan_position(
            (uint16_t)(prefix->eob - 1U), 4U, 4U, 0U, 0U);
        uint8_t context = eob_context(
            (uint16_t)(prefix->eob - 1U), 16U);
        uint8_t low = (uint8_t)(position == 0U);
        uint16_t *cdf = low ? cdfs->uv_eob_lf_ctx0 :
            context == 1U ? cdfs->uv_eob_ctx1 :
            context == 3U ? cdfs->uv_eob_ctx3 : 0;
        uint32_t symbol;
        if (cdf == 0) return AV2H_UNSUPPORTED;
        symbol = av2h_range_read_adaptive(decoder, cdf, low ? 5U : 3U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)(symbol + 1U);
        if (!low && symbol + 1U > 2U) {
            symbol = av2h_range_read_adaptive(decoder, cdfs->uv_br_ctx0, 4U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            block->base_levels[position] =
                (uint8_t)(block->base_levels[position] + symbol);
        }
    }
    for (scan_index = (uint16_t)(prefix->eob - 1U); scan_index != 0U;) {
        uint16_t position;
        uint8_t low;
        uint8_t context;
        uint16_t *cdf;
        uint32_t symbol;
        --scan_index;
        position = scan_position(scan_index, 4U, 4U, 0U, 0U);
        low = (uint8_t)(position == 0U);
        context = uv_level_context(block, position, plane, low);
        cdf = uv_base_cdf(cdfs, context, low);
        if (cdf == 0) return AV2H_UNSUPPORTED;
        symbol = av2h_range_read_adaptive(decoder, cdf, low ? 6U : 4U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        block->base_levels[position] = (uint8_t)symbol;
        if (!low && symbol > 2U) {
            cdf = uv_br_cdf(cdfs, block, position);
            if (cdf == 0) return AV2H_UNSUPPORTED;
            symbol = av2h_range_read_adaptive(decoder, cdf, 4U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            block->base_levels[position] =
                (uint8_t)(block->base_levels[position] + symbol);
        }
    }
    block->after_levels_bits = av2h_range_tell(decoder);
    for (scan_index = prefix->eob; scan_index != 0U;) {
        uint16_t position;
        uint32_t final_level;
        uint8_t sign;
        int32_t dequantized;
        --scan_index;
        position = scan_position(scan_index, 4U, 4U, 0U, 0U);
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
        block->quantized[position] = sign ? -(int32_t)final_level :
                                             (int32_t)final_level;
        dequantized = (int32_t)((final_level * 1568U + 4U) >> 3U);
        block->dequantized[position] = sign ? -dequantized : dequantized;
        ++block->nonzero_count;
        cul_level += final_level;
        if (position == 0U) block->dc_value = block->quantized[position];
    }
    cul_level = minimum(cul_level, 7U);
    if (block->dc_value < 0) {
        cul_level |= 8U;
    } else if (block->dc_value > 0) {
        cul_level += 16U;
    }
    block->cul_level = (uint8_t)cul_level;
    block->after_coefficients_bits = av2h_range_tell(decoder);
    return AV2H_OK;
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