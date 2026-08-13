#include "leaf.h"

static const Av2hBinaryCdfSpec intrabc_defaults[] = {
    { 32085U, 5U, 0U }, { 15172U, 30U, 0U }, { 4503U, 0U, 0U }
};

static const Av2hBinaryCdfSpec intrabc_mode_default = {
    29993U, 6U, 0U
};

static const Av2hBinaryCdfSpec intrabc_precision_default = {
    19778U, 31U, 0U
};

static const Av2hBinaryCdfSpec intrabc_shell_set_default = {
    31579U, 25U, 0U
};

static const Av2hBinaryCdfSpec intrabc_shell_offset_defaults[] = {
    { 14587U, 36U, 0U }, { 20966U, 75U, 0U }
};

static const Av2hBinaryCdfSpec intrabc_shell_offset_class2_default = {
    13189U, 0U, 0U
};

static const Av2hBinaryCdfSpec intrabc_shell_offset_other_defaults[] = {
    { 17943U, 93U, 0U }, { 18934U, 93U, 0U },
    { 18928U, 93U, 0U }, { 18696U, 93U, 0U },
    { 19044U, 93U, 0U }, { 20362U, 93U, 0U },
    { 20426U, 93U, 0U }, { 22563U, 93U, 0U },
    { 22190U, 93U, 0U }, { 23458U, 90U, 0U },
    { 26227U, 2U, 0U }, { 30765U, 50U, 0U },
    { 16384U, 0U, 0U }, { 16384U, 0U, 0U },
    { 16384U, 0U, 0U }, { 16384U, 0U, 0U }
};

static const Av2hBinaryCdfSpec intrabc_col_greater_defaults[] = {
    { 5663U, 25U, 0U }, { 4856U, 90U, 0U }
};

static const Av2hBinaryCdfSpec intrabc_col_index_defaults[] = {
    { 13445U, 1U, 0U }, { 13541U, 1U, 0U },
    { 14045U, 1U, 0U }, { 12888U, 31U, 0U }
};

static const Av2hBinaryCdfSpec skip_transform_defaults[] = {
    { 25865U, 25U, 0U }, { 14316U, 0U, 0U },
    { 4598U, 0U, 0U }, { 25612U, 6U, 0U },
    { 12366U, 1U, 0U }, { 3320U, 90U, 0U }
};

static const Av2hBinaryCdfSpec ccso_defaults[] = {
    { 18469U, 62U, 0U }, { 16384U, 0U, 0U },
    { 4949U, 37U, 0U }, { 16384U, 0U, 0U },
    { 23470U, 37U, 0U }, { 16384U, 0U, 0U },
    { 6666U, 37U, 0U }, { 16384U, 0U, 0U },
    { 22914U, 37U, 0U }, { 16384U, 0U, 0U },
    { 6993U, 37U, 0U }, { 16384U, 0U, 0U }
};

static const Av2hBinaryCdfSpec fsc_defaults[] = {
    { 30503U, 3U, 0U }, { 31244U, 3U, 0U },
    { 32254U, 78U, 0U }, { 32324U, 93U, 0U },
    { 32582U, 93U, 0U }, { 32691U, 93U, 0U },
    { 27437U, 0U, 0U }, { 27242U, 90U, 0U },
    { 28040U, 76U, 0U }, { 27589U, 76U, 0U },
    { 27234U, 7U, 0U }, { 23583U, 62U, 0U },
    { 26068U, 75U, 0U }, { 22635U, 75U, 0U },
    { 22069U, 6U, 0U }, { 19218U, 32U, 0U },
    { 13701U, 31U, 0U }, { 4636U, 38U, 0U },
    { 16384U, 0U, 0U }, { 16384U, 0U, 0U },
    { 32016U, 15U, 0U }, { 32403U, 93U, 0U },
    { 32583U, 15U, 0U }, { 32683U, 76U, 0U }
};

static const Av2hBinaryCdfSpec palette_y_mode_default = {
    30045U, 62U, 0U
};

static Av2hStatus initialize_binary_table(
    uint16_t (*destination)[AV2H_BINARY_CDF_SIZE],
    const Av2hBinaryCdfSpec *source,
    size_t row_count) {
    size_t index;
    for (index = 0U; index < row_count; ++index) {
        Av2hStatus status =
            av2h_binary_cdf_init(destination[index], source[index]);
        if (status != AV2H_OK) return status;
    }
    return AV2H_OK;
}

Av2hStatus av2h_leaf_cdfs_init(Av2hLeafCdfs *cdfs) {
    static const uint16_t delta_q[] = {
        16594U, 23325U, 26424U, 28225U, 29358U, 30099U, 30613U
    };
    static const uint16_t segment_id[AV2H_SEGMENT_ID_CONTEXTS][7U] = {
        { 5622U, 7893U, 16093U, 18233U, 27809U, 28373U, 32533U },
        { 14274U, 18230U, 22557U, 24935U, 29980U, 30851U, 32344U },
        { 27527U, 28487U, 28723U, 28890U, 32397U, 32647U, 32679U }
    };
    static const uint16_t intrabc_shell_class[] = {
        8680U, 13723U, 18208U, 22686U, 26722U, 30020U
    };
    static const uint16_t intrabc_qtr_shell_class[] = {
        7497U, 17301U, 23848U, 27438U, 29395U, 30879U, 32003U
    };
    static const uint16_t intrabc_shell_class_set1[] = {
        19978U, 30160U, 32564U, 32732U, 32736U, 32740U
    };
    static const uint16_t intrabc_qtr_shell_class_set1[] = {
        18469U, 27427U, 31562U, 32652U, 32724U, 32728U, 32732U
    };
    static const uint16_t mode_set[] = { 28863U, 31022U, 31724U };
    static const uint16_t mode_index[AV2H_Y_MODE_CONTEXT_COUNT][7] = {
        { 15175U, 20075U, 21728U, 24098U, 26405U, 27655U, 28860U },
        { 10114U, 14957U, 16815U, 19127U, 20147U, 25583U, 27169U },
        { 5636U, 9004U, 10456U, 12122U, 12744U, 20325U, 25607U }
    };
    static const uint16_t mode_offset[AV2H_Y_MODE_CONTEXT_COUNT][5] = {
        { 12743U, 18172U, 20194U, 23648U, 26419U },
        { 8976U, 16084U, 20827U, 24595U, 28496U },
        { 8784U, 14556U, 19710U, 24903U, 28724U }
    };
    static const uint8_t index_parameter[] = { 5U, 0U, 0U };
    static const uint8_t offset_parameter[] = { 6U, 75U, 75U };
    static const uint16_t mrl_index[AV2H_MRL_CONTEXT_COUNT][3] = {
        { 29573U, 31193U, 32023U },
        { 21812U, 27066U, 30279U },
        { 16076U, 23806U, 28762U }
    };
    static const uint8_t mrl_parameter[] = { 78U, 75U, 1U };
    static const Av2hBinaryCdfSpec mrl_secondary[] = {
        { 19678U, 6U, 0U }, { 12287U, 6U, 0U }, { 9574U, 6U, 0U }
    };
    static const Av2hBinaryCdfSpec morph_pred[] = {
        { 31715U, 55U, 0U }, { 19667U, 1U, 0U }, { 10555U, 91U, 0U }
    };
    static const Av2hBinaryCdfSpec is_cfl[] = {
        { 20441U, 30U, 0U }, { 11610U, 6U, 0U }, { 4643U, 0U, 0U }
    };
    static const uint16_t cfl_sign[] = {
        2421U, 4332U, 11256U, 12766U, 21386U, 28725U, 32087U
    };
    static const uint16_t cfl_alpha[6U][7U] = {
        { 21679U, 25305U, 30646U, 31512U, 32537U, 32646U, 32696U },
        { 8262U, 16302U, 24082U, 29422U, 31398U, 32286U, 32525U },
        { 17235U, 26166U, 30378U, 31305U, 32373U, 32549U, 32668U },
        { 17618U, 25732U, 27865U, 30338U, 31125U, 31522U, 32238U },
        { 17542U, 23066U, 27907U, 28728U, 30702U, 31165U, 31435U },
        { 17675U, 24802U, 30468U, 30783U, 31841U, 32264U, 32422U }
    };
    static const uint16_t mhccp_direction
        [AV2H_MHCCP_CONTEXT_GROUP_COUNT][AV2H_MHCCP_DIRECTION_SYMBOLS - 1U] = {
        { 10923U, 21845U },
        { 8795U, 15105U },
        { 10433U, 15974U },
        { 17085U, 21689U }
    };
    static const uint8_t mhccp_direction_parameter
        [AV2H_MHCCP_CONTEXT_GROUP_COUNT] = { 0U, 6U, 32U, 32U };
    static const uint16_t uv_mode[AV2H_UV_MODE_CONTEXT_COUNT][7U] = {
        { 9363U, 20957U, 22865U, 24753U, 26411U, 27983U, 30428U },
        { 21282U, 23610U, 28208U, 29311U, 30348U, 31158U, 31491U }
    };
    static const uint8_t uv_mode_parameter[] = { 31U, 30U };
    static const uint16_t palette_size[] = {
        8779U, 15095U, 20777U, 24903U, 27923U, 30403U
    };
    static const uint16_t palette_identity
        [AV2H_PALETTE_ROW_CONTEXT_COUNT][2U] = {
        { 22515U, 25751U }, { 4014U, 5233U },
        { 3548U, 4163U }, { 12999U, 32756U }
    };
    static const uint8_t palette_identity_parameter
        [AV2H_PALETTE_ROW_CONTEXT_COUNT] = { 25U, 31U, 33U, 56U };
    static const uint16_t palette_index[AV2H_PALETTE_SIZE_COUNT]
        [AV2H_PALETTE_COLOR_CONTEXT_COUNT][AV2H_PALETTE_MAX_SIZE - 1U] = {
        {
            { 28140U }, { 16384U }, { 8582U }, { 27413U }, { 30429U }
        },
        {
            { 25350U, 29026U }, { 11363U, 25273U },
            { 6841U, 28579U }, { 21350U, 26012U },
            { 30573U, 31646U }
        },
        {
            { 23706U, 26962U, 29060U },
            { 9976U, 22516U, 27382U },
            { 6691U, 25460U, 29234U },
            { 18909U, 23925U, 28403U },
            { 30308U, 31076U, 31818U }
        },
        {
            { 24116U, 26957U, 28486U, 29941U },
            { 9568U, 20472U, 24294U, 28942U },
            { 5706U, 25243U, 28040U, 30406U },
            { 20105U, 22982U, 27024U, 28911U },
            { 30897U, 31342U, 31766U, 32199U }
        },
        {
            { 20824U, 24227U, 25926U, 27459U, 29266U },
            { 8141U, 18989U, 21599U, 26182U, 28576U },
            { 5252U, 24340U, 26450U, 28438U, 30625U },
            { 19519U, 22695U, 25587U, 26972U, 28423U },
            { 30383U, 30890U, 31247U, 31653U, 32150U }
        },
        {
            { 21628U, 24512U, 25873U, 27054U, 28131U, 29539U },
            { 8028U, 18264U, 20613U, 25424U, 27112U, 28906U },
            { 6489U, 22242U, 24461U, 26394U, 28350U, 30510U },
            { 22048U, 24429U, 26990U, 27944U, 28417U, 29574U },
            { 30801U, 31205U, 31472U, 31728U, 32005U, 32305U }
        },
        {
            { 22471U, 25083U, 25984U, 26893U, 27654U, 28750U, 29903U },
            { 7542U, 17057U, 19151U, 23550U, 25459U, 27066U, 28804U },
            { 7582U, 20437U, 22728U, 24622U, 26515U, 28579U, 30632U },
            { 22102U, 24144U, 26916U, 28151U, 28846U, 29212U, 30153U },
            { 30524U, 30887U, 31156U, 31393U, 31626U, 31911U, 32281U }
        }
    };
    static const uint8_t palette_index_parameter[AV2H_PALETTE_SIZE_COUNT]
        [AV2H_PALETTE_COLOR_CONTEXT_COUNT] = {
        { 90U, 0U, 6U, 32U, 93U },
        { 90U, 7U, 1U, 6U, 93U },
        { 0U, 1U, 6U, 31U, 93U },
        { 0U, 81U, 76U, 31U, 93U },
        { 75U, 75U, 75U, 6U, 78U },
        { 80U, 90U, 75U, 76U, 93U },
        { 93U, 90U, 90U, 0U, 93U }
    };
    Av2hStatus status;
    size_t context;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_cdf_init(
        cdfs->delta_q, AV2H_DELTA_Q_SYMBOLS + 4U,
        delta_q, AV2H_DELTA_Q_SYMBOLS, 56U, 0U);
    if (status != AV2H_OK) return status;
    for (context = 0U; context < AV2H_SEGMENT_ID_CONTEXTS; ++context) {
        status = av2h_cdf_init(
            cdfs->segment_id[context], AV2H_SEGMENT_COUNT + 4U,
            segment_id[context], AV2H_SEGMENT_COUNT, 0U, 0U);
        if (status != AV2H_OK) return status;
    }
    status = initialize_binary_table(cdfs->intrabc, intrabc_defaults,
                                     AV2H_INTRABC_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->intrabc_qtr_shell_class,
        AV2H_INTRABC_QTR_SHELL_CLASS_SYMBOLS + 4U,
        intrabc_qtr_shell_class, AV2H_INTRABC_QTR_SHELL_CLASS_SYMBOLS,
        31U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(cdfs->intrabc_mode,
                                  intrabc_mode_default);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(cdfs->intrabc_precision,
                                  intrabc_precision_default);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(cdfs->morph_pred, morph_pred,
                                     AV2H_MORPH_PRED_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(cdfs->intrabc_shell_set,
                                  intrabc_shell_set_default);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->intrabc_shell_class,
        AV2H_INTRABC_SHELL_CLASS_SYMBOLS + 4U, intrabc_shell_class,
        AV2H_INTRABC_SHELL_CLASS_SYMBOLS, 5U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->intrabc_shell_class_set1,
        AV2H_INTRABC_SHELL_CLASS_SET1_SYMBOLS + 4U,
        intrabc_shell_class_set1,
        AV2H_INTRABC_SHELL_CLASS_SET1_SYMBOLS, 1U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->intrabc_qtr_shell_class_set1,
        AV2H_INTRABC_QTR_SHELL_CLASS_SET1_SYMBOLS + 4U,
        intrabc_qtr_shell_class_set1,
        AV2H_INTRABC_QTR_SHELL_CLASS_SET1_SYMBOLS, 31U, 0U);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        cdfs->intrabc_shell_offset, intrabc_shell_offset_defaults,
        AV2H_INTRABC_LOW_CLASS_COUNT);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->intrabc_shell_offset_class2,
        intrabc_shell_offset_class2_default);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        cdfs->intrabc_shell_offset_other,
        intrabc_shell_offset_other_defaults,
        AV2H_INTRABC_OTHER_CLASS_OFFSET_COUNT);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        cdfs->intrabc_col_greater, intrabc_col_greater_defaults,
        AV2H_INTRABC_GREATER_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        cdfs->intrabc_col_index, intrabc_col_index_defaults,
        AV2H_INTRABC_INDEX_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        cdfs->skip_transform, skip_transform_defaults,
        AV2H_SKIP_TRANSFORM_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(&cdfs->ccso[0][0], ccso_defaults,
                                     AV2H_FRAME_PLANE_COUNT *
                                         AV2H_CCSO_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        &cdfs->fsc_mode[0][0], fsc_defaults,
        AV2H_FSC_MODE_CONTEXT_COUNT * AV2H_FSC_BLOCK_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->palette_y_mode, palette_y_mode_default);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->palette_y_size, 11U, palette_size, 7U, 32U, 0U);
    if (status != AV2H_OK) return status;
    for (context = 0U; context < AV2H_PALETTE_ROW_CONTEXT_COUNT; ++context) {
        status = av2h_cdf_init(
            cdfs->palette_y_identity[context], 7U,
            palette_identity[context], 3U,
            palette_identity_parameter[context], 0U);
        if (status != AV2H_OK) return status;
    }
    for (context = 0U; context < AV2H_PALETTE_COLOR_CONTEXT_COUNT;
         ++context) {
        size_t palette_index_value;
        for (palette_index_value = 0U;
             palette_index_value < AV2H_PALETTE_SIZE_COUNT;
             ++palette_index_value) {
            size_t symbol_count = palette_index_value + 2U;
            status = av2h_cdf_init(
                cdfs->palette_y_color_index[palette_index_value][context],
                AV2H_PALETTE_MAX_SIZE + 4U,
                palette_index[palette_index_value][context], symbol_count,
                palette_index_parameter[palette_index_value][context], 0U);
            if (status != AV2H_OK) return status;
        }
    }
    status = av2h_cdf_init(cdfs->y_mode_set,
                           AV2H_Y_MODE_SET_SYMBOLS + 4U, mode_set,
                           AV2H_Y_MODE_SET_SYMBOLS, 93U, 0U);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->dpcm, (Av2hBinaryCdfSpec){ 16384U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->dpcm_vert_horz,
        (Av2hBinaryCdfSpec){ 16384U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->dpcm_uv, (Av2hBinaryCdfSpec){ 16384U, 0U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->dpcm_uv_vert_horz,
        (Av2hBinaryCdfSpec){ 16384U, 0U, 0U });
    if (status != AV2H_OK) return status;
    for (context = 0U; context < 4U; ++context) {
        size_t inter;
        for (inter = 0U; inter < 2U; ++inter) {
            status = av2h_binary_cdf_init(
                cdfs->lossless_tx_size[context][inter],
                (Av2hBinaryCdfSpec){
                    16384U, context == 0U ? 1U : 75U, 0U
                });
            if (status != AV2H_OK) return status;
        }
    }
    status = av2h_binary_cdf_init(
        cdfs->lossless_inter_tx_type,
        (Av2hBinaryCdfSpec){ 16384U, 0U, 0U });
    if (status != AV2H_OK) return status;
    for (context = 0U; context < AV2H_Y_MODE_CONTEXT_COUNT; ++context) {
        status = av2h_cdf_init(
            cdfs->y_mode_index[context], AV2H_Y_MODE_INDEX_SYMBOLS + 4U,
            mode_index[context], AV2H_Y_MODE_INDEX_SYMBOLS,
            index_parameter[context], 0U);
        if (status != AV2H_OK) return status;
        status = av2h_cdf_init(
            cdfs->y_mode_offset[context], AV2H_Y_MODE_OFFSET_SYMBOLS + 4U,
            mode_offset[context], AV2H_Y_MODE_OFFSET_SYMBOLS,
            offset_parameter[context], 0U);
        if (status != AV2H_OK) return status;
    }
    for (context = 0U; context < AV2H_MRL_CONTEXT_COUNT; ++context) {
        status = av2h_cdf_init(
            cdfs->mrl_index[context], AV2H_MRL_INDEX_SYMBOLS + 4U,
            mrl_index[context], AV2H_MRL_INDEX_SYMBOLS,
            mrl_parameter[context], 0U);
        if (status != AV2H_OK) return status;
        status = av2h_binary_cdf_init(cdfs->mrl_secondary[context],
                                      mrl_secondary[context]);
        if (status != AV2H_OK) return status;
    }
    status = initialize_binary_table(cdfs->is_cfl, is_cfl,
                                     AV2H_CFL_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(
        cdfs->cfl_mhccp_switch,
        (Av2hBinaryCdfSpec){ 15499U, 30U, 0U });
    if (status != AV2H_OK) return status;
    for (context = 0U; context < AV2H_MHCCP_CONTEXT_GROUP_COUNT; ++context) {
        status = av2h_cdf_init(
            cdfs->mhccp_direction[context],
            AV2H_MHCCP_DIRECTION_SYMBOLS + 4U,
            mhccp_direction[context], AV2H_MHCCP_DIRECTION_SYMBOLS,
            mhccp_direction_parameter[context], 0U);
        if (status != AV2H_OK) return status;
    }
    status = av2h_binary_cdf_init(
        cdfs->cfl_index, (Av2hBinaryCdfSpec){ 12507U, 30U, 0U });
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->cfl_sign, 12U, cfl_sign, 8U, 62U, 0U);
    if (status != AV2H_OK) return status;
    for (context = 0U; context < 6U; ++context) {
        status = av2h_cdf_init(
            cdfs->cfl_alpha[context], 12U,
            cfl_alpha[context], 8U, 62U, 0U);
        if (status != AV2H_OK) return status;
    }
    for (context = 0U; context < AV2H_UV_MODE_CONTEXT_COUNT; ++context) {
        status = av2h_cdf_init(
            cdfs->uv_mode[context], AV2H_UV_MODE_SYMBOLS + 4U,
            uv_mode[context], AV2H_UV_MODE_SYMBOLS,
            uv_mode_parameter[context], 0U);
        if (status != AV2H_OK) return status;
    }
    return AV2H_OK;
}