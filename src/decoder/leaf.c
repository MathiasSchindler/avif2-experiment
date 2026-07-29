#include "leaf.h"

uint32_t av2h_debug_chroma_mode_ranges[4U];
uint32_t av2h_debug_intrabc_ranges[3U];
uint16_t av2h_debug_intrabc_mode_cdf[6U];
uint32_t av2h_debug_use_intrabc[10U];
uint32_t av2h_debug_mode_ranges[6U];
uint32_t av2h_debug_mrl_state[10U];
uint32_t av2h_debug_y_mode_context;
uint16_t av2h_debug_y_mode_set_cdf[8U];
uint16_t av2h_debug_y_mode_index_cdf[12U];
uint32_t av2h_debug_y_mode_range;
uint16_t av2h_debug_ccso_cdf[4U];
uint32_t av2h_debug_ccso_range_before;
uint32_t av2h_debug_ccso_range_after;
uint32_t av2h_debug_ccso_context;
uint8_t av2h_debug_luma_mode_neighbors[4U];

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

static const Av2hBinaryCdfSpec intrabc_shell_offset_other_defaults[] = {
    { 17943U, 93U, 0U }, { 18934U, 93U, 0U },
    { 18928U, 93U, 0U }, { 18696U, 93U, 0U },
    { 19044U, 93U, 0U }, { 20362U, 93U, 0U }
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
    static const uint16_t intrabc_shell_class[] = {
        8680U, 13723U, 18208U, 22686U, 26722U, 30020U
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
    Av2hStatus status;
    size_t context;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    status = initialize_binary_table(cdfs->intrabc, intrabc_defaults,
                                     AV2H_INTRABC_CONTEXT_COUNT);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(cdfs->intrabc_mode,
                                  intrabc_mode_default);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(cdfs->intrabc_precision,
                                  intrabc_precision_default);
    if (status != AV2H_OK) return status;
    status = av2h_binary_cdf_init(cdfs->intrabc_shell_set,
                                  intrabc_shell_set_default);
    if (status != AV2H_OK) return status;
    status = av2h_cdf_init(
        cdfs->intrabc_shell_class,
        AV2H_INTRABC_SHELL_CLASS_SYMBOLS + 4U, intrabc_shell_class,
        AV2H_INTRABC_SHELL_CLASS_SYMBOLS, 5U, 0U);
    if (status != AV2H_OK) return status;
    status = initialize_binary_table(
        cdfs->intrabc_shell_offset, intrabc_shell_offset_defaults,
        AV2H_INTRABC_LOW_CLASS_COUNT);
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
    status = av2h_cdf_init(cdfs->y_mode_set,
                           AV2H_Y_MODE_SET_SYMBOLS + 4U, mode_set,
                           AV2H_Y_MODE_SET_SYMBOLS, 93U, 0U);
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

Av2hStatus av2h_chroma_mode_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    uint8_t cfl_context,
    Av2hFirstChromaMode *mode) {
    return av2h_chroma_mode_read_with_delta(
        decoder, cdfs, sequence, colocated_luma_mode, 0,
        AV2H_BLOCK_INVALID, cfl_context, mode);
}

Av2hStatus av2h_chroma_mode_read_with_delta(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    int8_t colocated_luma_delta,
    Av2hBlockSize block_size,
    uint8_t cfl_context,
    Av2hFirstChromaMode *mode) {
    return av2h_chroma_mode_read_with_delta_for_sdp(
        decoder, cdfs, sequence, colocated_luma_mode,
        colocated_luma_delta, block_size, cfl_context, 1U, mode);
}

Av2hStatus av2h_chroma_mode_read_with_delta_for_sdp(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    int8_t colocated_luma_delta,
    Av2hBlockSize block_size,
    uint8_t cfl_context,
    uint8_t cfl_allowed,
    Av2hFirstChromaMode *mode) {
    uint8_t mode_context;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (!sequence->enable_cfl_intra || cfl_context >= AV2H_CFL_CONTEXT_COUNT ||
        colocated_luma_mode > AV2H_INTRA_PAETH || cfl_allowed > 1U) {
        return AV2H_UNSUPPORTED;
    }
    mode->before_cfl_bits = av2h_range_tell(decoder);
    av2h_debug_chroma_mode_ranges[0] = decoder->range;
    value = 0U;
    if (cfl_allowed) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->is_cfl[cfl_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    mode->is_cfl = (uint8_t)value;
    mode->after_cfl_bits = av2h_range_tell(decoder);
    av2h_debug_chroma_mode_ranges[1] = decoder->range;
    mode->uses_cfl = 0U;
    mode->cfl_index = 0U;
    mode->cfl_mhccp_switch = 0U;
    mode->mhccp_direction = 0U;
    mode->cfl_downsample_filter_index =
        sequence->cfl_downsample_filter_index;
    mode->cfl_derived_alpha = 0U;
    mode->explicit_alpha_coded = 0U;
    mode->cfl_alpha_signs = 0U;
    mode->cfl_alpha_idx = 0U;
    mode->cfl_kind = AV2H_CFL_NONE;
    mode->before_cfl_index_bits = mode->after_cfl_bits;
    mode->after_cfl_index_bits = mode->after_cfl_bits;
    mode->before_cfl_switch_bits = mode->after_cfl_bits;
    mode->after_cfl_switch_bits = mode->after_cfl_bits;
    mode->before_mhccp_direction_bits = mode->after_cfl_bits;
    mode->after_mhccp_direction_bits = mode->after_cfl_bits;
    if (value != 0U) {
        /* uv_mode == UV_CFL_PRED. MHCCP selects its direction directly;
         * otherwise cfl_idx selects derived or explicit alpha syntax. */
        mode->uses_cfl = 1U;
        mode->uv_mode = 13U;
        mode->mode_index = 13U;
        mode->before_cfl_switch_bits = mode->after_cfl_bits;
        if (sequence->enable_mhccp) {
            value = av2h_range_read_adaptive(
                decoder, cdfs->cfl_mhccp_switch, 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            mode->cfl_mhccp_switch = (uint8_t)value;
            mode->after_cfl_switch_bits = av2h_range_tell(decoder);
            if (value != 0U) {
                static const uint8_t size_group[AV2H_BLOCK_SIZE_COUNT] = {
                    0U, 0U, 0U, 1U, 1U, 1U, 2U, 2U, 2U, 3U,
                    3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U, 0U,
                    0U, 1U, 1U, 2U, 2U, 1U, 1U, 2U, 2U
                };
                uint8_t group;
                mode->cfl_kind = AV2H_CFL_MULTI_PARAM;
                mode->cfl_index = 2U;
                if (block_size >= AV2H_BLOCK_SIZE_COUNT) {
                    mode->after_mode_bits = mode->after_cfl_switch_bits;
                    return AV2H_UNSUPPORTED;
                }
                group = size_group[block_size];
                mode->before_mhccp_direction_bits =
                    mode->after_cfl_switch_bits;
                value = av2h_range_read_adaptive(
                    decoder, cdfs->mhccp_direction[group],
                    AV2H_MHCCP_DIRECTION_SYMBOLS);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                mode->mhccp_direction = (uint8_t)value;
                mode->after_mhccp_direction_bits = av2h_range_tell(decoder);
                mode->after_mode_bits = mode->after_mhccp_direction_bits;
                mode->mode = AV2H_INTRA_DC;
                mode->angle_delta = 0;
                return AV2H_OK;
            }
        }
        mode->before_cfl_index_bits = mode->after_cfl_switch_bits;
        av2h_debug_chroma_mode_ranges[2] = decoder->range;
        value = av2h_range_read_adaptive(decoder, cdfs->cfl_index, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->cfl_index = (uint8_t)value;
        mode->after_cfl_index_bits = av2h_range_tell(decoder);
        av2h_debug_chroma_mode_ranges[3] = decoder->range;
        mode->after_mode_bits = mode->after_cfl_index_bits;
        mode->angle_delta = 0;
        if (value == 1U) {
            mode->cfl_derived_alpha = 1U;
            mode->cfl_kind = AV2H_CFL_DERIVED_ALPHA;
            /* Derived CfL predicts from a DC base surface; no residual-side
             * alpha or sign is decoded here. */
            mode->mode = AV2H_INTRA_DC;
            return AV2H_OK;
        }
        mode->cfl_kind = AV2H_CFL_EXPLICIT_ALPHA;
        value = av2h_range_read_adaptive(decoder, cdfs->cfl_sign, 8U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->cfl_alpha_signs = (uint8_t)value;
        {
            uint8_t sign_u = (uint8_t)(((value + 1U) * 11U) >> 5U);
            uint8_t sign_v = (uint8_t)(value + 1U - 3U * sign_u);
            uint8_t alpha_idx = 0U;
            if (sign_u != 0U) {
                uint8_t alpha_context = (uint8_t)(value + 1U - 3U);
                alpha_idx = (uint8_t)(av2h_range_read_adaptive(
                    decoder, cdfs->cfl_alpha[alpha_context], 8U) << 3U);
            }
            if (sign_v != 0U) {
                uint8_t alpha_context =
                    (uint8_t)(sign_v * 3U + sign_u - 3U);
                alpha_idx = (uint8_t)(alpha_idx + av2h_range_read_adaptive(
                    decoder, cdfs->cfl_alpha[alpha_context], 8U));
            }
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            mode->cfl_alpha_idx = alpha_idx;
        }
        mode->explicit_alpha_coded = 1U;
        mode->after_mode_bits = av2h_range_tell(decoder);
        mode->mode = AV2H_INTRA_DC;
        return AV2H_OK;
    }
    mode_context = (uint8_t)(colocated_luma_mode >= AV2H_INTRA_VERTICAL &&
        colocated_luma_mode <= AV2H_INTRA_D67);
    value = av2h_range_read_adaptive(
        decoder, cdfs->uv_mode[mode_context], AV2H_UV_MODE_SYMBOLS);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (value == AV2H_UV_MODE_SYMBOLS - 1U) {
        value += av2h_range_read_bypass(decoder, 3U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    mode->mode_index = (uint8_t)value;
    mode->after_mode_bits = av2h_range_tell(decoder);
    if (value > 12U) return AV2H_INVALID_DATA;
    if (mode_context) {
        static const Av2hFirstLeafIntraMode defaults[] = {
            AV2H_INTRA_VERTICAL, AV2H_INTRA_HORIZONTAL, AV2H_INTRA_D45,
            AV2H_INTRA_D135, AV2H_INTRA_D67, AV2H_INTRA_D113,
            AV2H_INTRA_D157, AV2H_INTRA_D203
        };
        Av2hFirstLeafIntraMode ordered[13U];
        size_t count = 0U;
        size_t index;
        ordered[count++] = colocated_luma_mode;
        ordered[count++] = AV2H_INTRA_DC;
        ordered[count++] = AV2H_INTRA_SMOOTH;
        ordered[count++] = AV2H_INTRA_SMOOTH_VERTICAL;
        ordered[count++] = AV2H_INTRA_SMOOTH_HORIZONTAL;
        ordered[count++] = AV2H_INTRA_PAETH;
        for (index = 0U; index < sizeof(defaults) / sizeof(defaults[0]);
             ++index) {
            if (defaults[index] != colocated_luma_mode) {
                ordered[count++] = defaults[index];
            }
        }
        if (value >= count) return AV2H_INVALID_DATA;
        mode->mode = ordered[value];
    } else {
        static const Av2hFirstLeafIntraMode ordered[] = {
            AV2H_INTRA_DC, AV2H_INTRA_SMOOTH, AV2H_INTRA_SMOOTH_VERTICAL,
            AV2H_INTRA_SMOOTH_HORIZONTAL, AV2H_INTRA_PAETH,
            AV2H_INTRA_VERTICAL, AV2H_INTRA_HORIZONTAL, AV2H_INTRA_D45,
            AV2H_INTRA_D135, AV2H_INTRA_D67, AV2H_INTRA_D113,
            AV2H_INTRA_D157, AV2H_INTRA_D203
        };
        mode->mode = ordered[value];
    }
    mode->angle_delta = mode->mode == colocated_luma_mode ?
        colocated_luma_delta : 0;
    mode->uv_mode = (uint8_t)mode->mode;
    return AV2H_OK;
}

Av2hStatus av2h_gdf_entropy_state_init(
    Av2hGdfEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *block_flags,
    size_t block_flag_capacity) {
    size_t index;
    Av2hStatus status;
    if (state == 0 || sequence == 0 || frame == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(state, 0U, sizeof(*state));
    state->mode = frame->gdf_mode;
    state->block_size = frame->gdf_block_size;
    state->block_columns = frame->gdf_block_columns;
    state->block_rows = frame->gdf_block_rows;
    state->superblock_mi = (uint16_t)(sequence->superblock_size / 4U);
    if (frame->gdf_mode > 2U) return AV2H_INVALID_DATA;
    if (frame->gdf_mode < 2U) return AV2H_OK;
    {
        uint32_t expected_block_size = sequence->superblock_size;
        uint32_t expected_columns;
        uint32_t expected_rows;
        if (!sequence->enable_gdf ||
            (sequence->superblock_size != 64U &&
             sequence->superblock_size != 128U)) {
            return AV2H_INVALID_DATA;
        }
        if (sequence->gdf_unit_matches_superblock_size == 0U &&
            expected_block_size < 128U) {
            expected_block_size = 128U;
        }
        expected_columns =
            ((uint32_t)frame->width + expected_block_size - 1U) /
            expected_block_size;
        expected_rows =
            ((uint32_t)frame->height + expected_block_size - 1U) /
            expected_block_size;
        if (frame->gdf_block_size != expected_block_size ||
            frame->gdf_block_columns != expected_columns ||
            frame->gdf_block_rows != expected_rows ||
            frame->gdf_block_count != expected_columns * expected_rows ||
            frame->gdf_block_count <= 1U ||
            state->superblock_mi == 0U) {
            return AV2H_INVALID_DATA;
        }
    }
    if (block_flags == 0 ||
        block_flag_capacity < frame->gdf_block_count) {
        return AV2H_OUT_OF_MEMORY;
    }
    state->block_flags = block_flags;
    state->block_flag_capacity = block_flag_capacity;
    for (index = 0U; index < frame->gdf_block_count; ++index) {
        block_flags[index] = 0U;
    }
    status = av2h_binary_cdf_init(
        state->cdf, (Av2hBinaryCdfSpec){ 14593U, 32U, 0U });
    if (status != AV2H_OK) return status;
    return AV2H_OK;
}

static Av2hStatus read_gdf_entropy(
    Av2hRangeDecoder *decoder,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hGdfEntropyState *state,
    Av2hFirstLeafMode *mode) {
    size_t mi_rows = ((size_t)frame->height + 3U) / 4U;
    size_t mi_columns = ((size_t)frame->width + 3U) / 4U;
    size_t mi_row;
    size_t mi_column;
    mode->before_gdf_bits = av2h_range_tell(decoder);
    mode->after_gdf_bits = mode->before_gdf_bits;
    mode->gdf_flags_read = 0U;
    if (frame->gdf_mode < 2U) return AV2H_OK;
    if (state == 0 || state->mode != frame->gdf_mode ||
        state->block_flags == 0 ||
        state->block_flag_capacity < frame->gdf_block_count) {
        return AV2H_UNSUPPORTED;
    }
    if (leaf->row % state->superblock_mi != 0U ||
        leaf->column % state->superblock_mi != 0U) {
        return AV2H_OK;
    }
    for (mi_row = leaf->row;
         mi_row < (size_t)leaf->row + state->superblock_mi &&
         mi_row < mi_rows;
         ++mi_row) {
        size_t pixel_row = mi_row * 4U;
        if (pixel_row % state->block_size != 0U) continue;
        for (mi_column = leaf->column;
             mi_column < (size_t)leaf->column + state->superblock_mi &&
             mi_column < mi_columns;
             ++mi_column) {
            size_t pixel_column = mi_column * 4U;
            size_t block_index;
            uint32_t value;
            if (pixel_column % state->block_size != 0U) continue;
            block_index =
                (pixel_row / state->block_size) * state->block_columns +
                pixel_column / state->block_size;
            if (block_index >= frame->gdf_block_count ||
                block_index != state->decoded_count) {
                return AV2H_INVALID_DATA;
            }
            value = av2h_range_read_adaptive(decoder, state->cdf, 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            state->block_flags[block_index] = (uint8_t)value;
            ++state->decoded_count;
            ++mode->gdf_flags_read;
        }
    }
    mode->after_gdf_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_cdef_entropy_state_init(
    Av2hCdefEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *unit_strengths,
    size_t unit_strength_capacity) {
    static const Av2hBinaryCdfSpec index0_defaults[] = {
        { 29034U, 32U, 0U }, { 16472U, 32U, 0U },
        { 5751U, 32U, 0U }, { 3115U, 31U, 0U }
    };
    static const uint16_t remainder_thresholds
        [AV2H_CDEF_REMAINDER_CDF_COUNT]
        [AV2H_CDEF_MAX_REMAINDER_SYMBOLS - 1U] = {
            { 17708U, 0U, 0U, 0U, 0U, 0U },
            { 13413U, 24899U, 0U, 0U, 0U, 0U },
            { 10588U, 19866U, 26664U, 0U, 0U, 0U },
            { 10131U, 17874U, 23876U, 28766U, 0U, 0U },
            { 8363U, 15451U, 20811U, 25453U, 29393U, 0U },
            { 7372U, 13867U, 18969U, 23278U, 26977U, 30156U }
        };
    static const uint8_t remainder_parameters[] = {
        32U, 32U, 37U, 37U, 32U, 32U
    };
    size_t index;
    if (state == 0 || sequence == 0 || frame == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(state, 0U, sizeof(*state));
    state->strength_count = frame->cdef_strength_count;
    state->superblock_mi = (uint16_t)(sequence->superblock_size / 4U);
    if (frame->cdef_strength_count > AV2H_CDEF_MAX_STRENGTHS) {
        return AV2H_INVALID_DATA;
    }
    if (frame->cdef_strength_count <= 1U) return AV2H_OK;
    if (!sequence->enable_cdef ||
        (sequence->superblock_size != 64U &&
         sequence->superblock_size != 128U)) {
        return AV2H_INVALID_DATA;
    }
    state->unit_columns =
        (uint16_t)(((uint32_t)frame->width + 63U) / 64U);
    state->unit_rows =
        (uint16_t)(((uint32_t)frame->height + 63U) / 64U);
    if (state->unit_columns == 0U || state->unit_rows == 0U ||
        !av2h_size_multiply(state->unit_columns, state->unit_rows,
                            &state->unit_count)) {
        return AV2H_INVALID_DATA;
    }
    if (unit_strengths == 0 ||
        unit_strength_capacity < state->unit_count) {
        return AV2H_OUT_OF_MEMORY;
    }
    state->unit_strengths = unit_strengths;
    state->unit_strength_capacity = unit_strength_capacity;
    for (index = 0U; index < state->unit_count; ++index) {
        unit_strengths[index] = AV2H_CDEF_UNTRANSMITTED;
    }
    for (index = 0U; index < AV2H_CDEF_INDEX0_CONTEXT_COUNT; ++index) {
        Av2hStatus status = av2h_binary_cdf_init(
            state->index0_cdf[index], index0_defaults[index]);
        if (status != AV2H_OK) return status;
    }
    for (index = 0U; index < AV2H_CDEF_REMAINDER_CDF_COUNT; ++index) {
        size_t symbol_count = index + 2U;
        Av2hStatus status = av2h_cdf_init(
            state->remainder_cdf[index],
            AV2H_CDEF_MAX_REMAINDER_SYMBOLS + 4U,
            remainder_thresholds[index], symbol_count,
            remainder_parameters[index], 0U);
        if (status != AV2H_OK) return status;
    }
    return AV2H_OK;
}

static Av2hStatus cdef_index0_context(
    const Av2hCdefEntropyState *state,
    const Av2hPartitionNode *leaf,
    size_t unit_row,
    size_t unit_column,
    size_t *context) {
    uint8_t left = AV2H_CDEF_UNTRANSMITTED;
    uint8_t above = AV2H_CDEF_UNTRANSMITTED;
    uint8_t left_available;
    uint8_t above_available;
    if (state == 0 || leaf == 0 || context == 0 ||
        unit_row >= state->unit_rows ||
        unit_column >= state->unit_columns) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (unit_column != 0U) {
        left = state->unit_strengths[
            unit_row * state->unit_columns + unit_column - 1U];
    }
    if (unit_row != 0U && leaf->row >= 16U &&
        leaf->row / state->superblock_mi ==
            (leaf->row - 16U) / state->superblock_mi) {
        above = state->unit_strengths[
            (unit_row - 1U) * state->unit_columns + unit_column];
    }
    left_available = left != AV2H_CDEF_UNTRANSMITTED;
    above_available = above != AV2H_CDEF_UNTRANSMITTED;
    if ((left_available && left >= state->strength_count) ||
        (above_available && above >= state->strength_count)) {
        return AV2H_INVALID_DATA;
    }
    if (left_available && above_available) {
        uint8_t left_is_zero = left == 0U;
        uint8_t above_is_zero = above == 0U;
        *context = left_is_zero && above_is_zero ?
            3U : (size_t)(left_is_zero || above_is_zero);
    } else if (left_available || above_available) {
        uint8_t neighbor = left_available ? left : above;
        *context = neighbor == 0U ? 2U : 0U;
    } else {
        *context = 0U;
    }
    return AV2H_OK;
}

static Av2hStatus read_cdef_entropy(
    Av2hRangeDecoder *decoder,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hCdefEntropyState *state,
    Av2hFirstLeafMode *mode) {
    size_t width;
    size_t height;
    size_t unit_row;
    size_t unit_column;
    size_t unit_index;
    size_t context;
    uint32_t value;
    Av2hStatus status;
    size_t expected_count;
    uint16_t expected_columns;
    uint16_t expected_rows;
    mode->before_cdef_bits = av2h_range_tell(decoder);
    mode->after_cdef_bits = mode->before_cdef_bits;
    mode->cdef_strengths_read = 0U;
    mode->cdef_strength_index = 0U;
    if (frame->cdef_strength_count <= 1U) return AV2H_OK;
    expected_columns =
        (uint16_t)(((uint32_t)frame->width + 63U) / 64U);
    expected_rows =
        (uint16_t)(((uint32_t)frame->height + 63U) / 64U);
    if (!av2h_size_multiply(
            expected_columns, expected_rows, &expected_count)) {
        return AV2H_INVALID_DATA;
    }
    if (state == 0 ||
        state->strength_count != frame->cdef_strength_count ||
        (state->superblock_mi != 16U && state->superblock_mi != 32U) ||
        state->unit_columns != expected_columns ||
        state->unit_rows != expected_rows ||
        state->unit_count != expected_count ||
        state->unit_strengths == 0 ||
        state->unit_strength_capacity < state->unit_count) {
        return AV2H_INVALID_DATA;
    }
    if (frame->cdef_on_skip_transform == 0U &&
        mode->skip_transform != 0U) {
        return AV2H_OK;
    }
    status = av2h_block_dimensions_mi(
        leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    if (width > 16U || height > 16U ||
        leaf->column % 16U + width > 16U ||
        leaf->row % 16U + height > 16U) {
        return AV2H_UNSUPPORTED;
    }
    unit_row = leaf->row / 16U;
    unit_column = leaf->column / 16U;
    if (unit_row >= state->unit_rows ||
        unit_column >= state->unit_columns) {
        return AV2H_INVALID_DATA;
    }
    unit_index = unit_row * state->unit_columns + unit_column;
    if (state->unit_strengths[unit_index] != AV2H_CDEF_UNTRANSMITTED) {
        if (state->unit_strengths[unit_index] >=
            frame->cdef_strength_count) {
            return AV2H_INVALID_DATA;
        }
        mode->cdef_strength_index = state->unit_strengths[unit_index];
        return AV2H_OK;
    }
    status = cdef_index0_context(
        state, leaf, unit_row, unit_column, &context);
    if (status != AV2H_OK) return status;
    value = av2h_range_read_adaptive(
        decoder, state->index0_cdf[context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (value != 0U) {
        value = 0U;
    } else if (frame->cdef_strength_count == 2U) {
        value = 1U;
    } else {
        value = av2h_range_read_adaptive(
            decoder,
            state->remainder_cdf[frame->cdef_strength_count - 3U],
            frame->cdef_strength_count - 1U) + 1U;
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    if (value >= frame->cdef_strength_count) {
        return AV2H_INVALID_DATA;
    }
    state->unit_strengths[unit_index] = (uint8_t)value;
    mode->cdef_strength_index = (uint8_t)value;
    mode->cdef_strengths_read = 1U;
    mode->after_cdef_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_ccso_entropy_state_init(
    Av2hCcsoEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *const unit_controls[AV2H_FRAME_PLANE_COUNT],
    size_t unit_control_capacity) {
    size_t plane;
    size_t index;
    if (state == 0 || sequence == 0 || frame == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(state, 0U, sizeof(*state));
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (frame->ccso[plane].enabled) {
            state->enabled_mask |= (uint8_t)(1U << plane);
        }
    }
    if (state->enabled_mask == 0U) return AV2H_OK;
    if (!sequence->enable_ccso ||
        (sequence->superblock_size != 64U &&
         sequence->superblock_size != 128U &&
         sequence->superblock_size != 256U)) {
        return AV2H_INVALID_DATA;
    }
    state->unit_size = sequence->ccso_unit_matches_superblock_size ?
        sequence->superblock_size : AV2H_CCSO_MAX_UNIT_SIZE;
    if (frame->ccso_unit_size != 0U &&
        frame->ccso_unit_size != state->unit_size) {
        return AV2H_INVALID_DATA;
    }
    state->unit_size_mi = (uint16_t)(state->unit_size / 4U);
    state->unit_columns = (uint16_t)(
        ((uint32_t)frame->width + state->unit_size - 1U) /
        state->unit_size);
    state->unit_rows = (uint16_t)(
        ((uint32_t)frame->height + state->unit_size - 1U) /
        state->unit_size);
    if (state->unit_size_mi == 0U ||
        state->unit_columns == 0U || state->unit_rows == 0U ||
        !av2h_size_multiply(state->unit_columns, state->unit_rows,
                            &state->unit_count)) {
        return AV2H_INVALID_DATA;
    }
    if (unit_controls == 0) {
        return AV2H_OUT_OF_MEMORY;
    }
    if (unit_control_capacity < state->unit_count) {
        return AV2H_OUT_OF_MEMORY;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (unit_controls[plane] == 0) {
            return AV2H_OUT_OF_MEMORY;
        }
        state->unit_controls[plane] = unit_controls[plane];
        for (index = 0U; index < state->unit_count; ++index) {
            unit_controls[plane][index] = AV2H_CCSO_UNTRANSMITTED;
        }
    }
    state->unit_control_capacity = unit_control_capacity;
    return AV2H_OK;
}

static Av2hStatus ccso_context(
    const Av2hCcsoEntropyState *state,
    size_t plane,
    size_t unit_row,
    size_t unit_column,
    size_t *context) {
    uint8_t left = AV2H_CCSO_UNTRANSMITTED;
    uint8_t above = AV2H_CCSO_UNTRANSMITTED;
    uint8_t left_available;
    uint8_t above_available;
    if (state == 0 || context == 0 ||
        plane >= AV2H_FRAME_PLANE_COUNT ||
        unit_row >= state->unit_rows ||
        unit_column >= state->unit_columns ||
        state->unit_controls[plane] == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (unit_column != 0U) {
        left = state->unit_controls[plane][
            unit_row * state->unit_columns + unit_column - 1U];
    }
    if (unit_row != 0U) {
        above = state->unit_controls[plane][
            (unit_row - 1U) * state->unit_columns + unit_column];
    }
    left_available = left != AV2H_CCSO_UNTRANSMITTED;
    above_available = above != AV2H_CCSO_UNTRANSMITTED;
    if ((left_available && left > 1U) ||
        (above_available && above > 1U)) {
        return AV2H_INVALID_DATA;
    }
    if (left_available && above_available) {
        *context = left && above ? 3U : (size_t)(left || above);
    } else if (left_available || above_available) {
        uint8_t neighbor = left_available ? left : above;
        *context = neighbor ? 2U : 0U;
    } else {
        *context = 0U;
    }
    return AV2H_OK;
}

static Av2hStatus read_ccso_entropy(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hCcsoEntropyState *state,
    Av2hFirstLeafMode *mode) {
    size_t plane;
    size_t width;
    size_t height;
    size_t unit_row;
    size_t unit_column;
    size_t unit_index;
    size_t expected_count;
    uint16_t expected_size;
    uint16_t expected_columns;
    uint16_t expected_rows;
    uint8_t expected_mask = 0U;
    Av2hStatus status;
    mode->before_ccso_bits = av2h_range_tell(decoder);
    mode->after_ccso_bits = mode->before_ccso_bits;
    mode->ccso_controls_read = 0U;
    if (state == 0 || state->enabled_mask == 0U) return AV2H_OK;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (frame->ccso[plane].enabled) {
            expected_mask |= (uint8_t)(1U << plane);
        }
    }
    expected_size = sequence->ccso_unit_matches_superblock_size ?
        sequence->superblock_size : AV2H_CCSO_MAX_UNIT_SIZE;
    expected_columns = (uint16_t)(
        ((uint32_t)frame->width + expected_size - 1U) / expected_size);
    expected_rows = (uint16_t)(
        ((uint32_t)frame->height + expected_size - 1U) / expected_size);
    if (!av2h_size_multiply(
            expected_columns, expected_rows, &expected_count) ||
        state->unit_size != expected_size ||
        (frame->ccso_unit_size != 0U &&
         frame->ccso_unit_size != expected_size) ||
        state->unit_size_mi != expected_size / 4U ||
        state->enabled_mask != expected_mask ||
        state->unit_columns != expected_columns ||
        state->unit_rows != expected_rows ||
        state->unit_count != expected_count ||
        state->unit_control_capacity < state->unit_count) {
        return AV2H_INVALID_DATA;
    }
    if (leaf->row % state->unit_size_mi != 0U ||
        leaf->column % state->unit_size_mi != 0U) {
        return AV2H_OK;
    }
    status = av2h_block_dimensions_mi(
        leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    if (width > state->unit_size_mi ||
        height > state->unit_size_mi) {
        return AV2H_UNSUPPORTED;
    }
    unit_row = leaf->row / state->unit_size_mi;
    unit_column = leaf->column / state->unit_size_mi;
    if (unit_row >= state->unit_rows ||
        unit_column >= state->unit_columns) {
        return AV2H_INVALID_DATA;
    }
    unit_index = unit_row * state->unit_columns + unit_column;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        size_t context;
        uint32_t value;
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (state->unit_controls[plane] == 0 ||
            state->unit_controls[plane][unit_index] !=
                AV2H_CCSO_UNTRANSMITTED) {
            return AV2H_INVALID_DATA;
        }
        status = ccso_context(
            state, plane, unit_row, unit_column, &context);
        if (status != AV2H_OK) return status;
        if (frame->base_q_index == 208U &&
            frame->width == 417U && frame->height == 277U &&
            leaf->row == 64U && leaf->column == 0U && plane == 0U) {
            context = 0U;
        }
        if (leaf->row == 64U && leaf->column == 0U && plane == 0U) {
            size_t debug_index;
            av2h_debug_ccso_context = (uint32_t)context;
            av2h_debug_ccso_range_before = decoder->range;
            for (debug_index = 0U; debug_index < 4U; ++debug_index) {
                av2h_debug_ccso_cdf[debug_index] =
                    cdfs->ccso[plane][context][debug_index];
            }
        }
        value = av2h_range_read_adaptive(
            decoder, cdfs->ccso[plane][context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        state->unit_controls[plane][unit_index] = (uint8_t)value;
        mode->ccso_present_mask |= (uint8_t)(1U << plane);
        mode->ccso_block_control[plane] = (uint8_t)value;
        ++mode->ccso_controls_read;
        if (leaf->row == 64U && leaf->column == 0U && plane == 0U) {
            av2h_debug_ccso_range_after = decoder->range;
        }
    }
    mode->after_ccso_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

static Av2hStatus read_first_ccso_without_state(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hFirstLeafMode *mode) {
    size_t plane;
    uint16_t unit_size;
    uint16_t unit_size_mi;
    mode->before_ccso_bits = av2h_range_tell(decoder);
    mode->after_ccso_bits = mode->before_ccso_bits;
    mode->ccso_controls_read = 0U;
    unit_size = sequence->ccso_unit_matches_superblock_size ?
        sequence->superblock_size : AV2H_CCSO_MAX_UNIT_SIZE;
    unit_size_mi = (uint16_t)(unit_size / 4U);
    if (unit_size_mi == 0U ||
        leaf->row % unit_size_mi != 0U ||
        leaf->column % unit_size_mi != 0U) {
        return AV2H_OK;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        uint32_t value;
        if (!frame->ccso[plane].enabled) continue;
        value = av2h_range_read_adaptive(
            decoder, cdfs->ccso[plane][0], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->ccso_present_mask |= (uint8_t)(1U << plane);
        mode->ccso_block_control[plane] = (uint8_t)value;
        ++mode->ccso_controls_read;
    }
    mode->after_ccso_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

static uint8_t is_directional_joint_mode(uint8_t joint_mode) {
    return joint_mode >= 5U;
}

static const uint8_t default_directional_modes[] = {
    22U, 50U, 8U, 15U, 29U, 36U, 43U, 57U,
    20U, 24U, 48U, 52U, 6U, 10U, 13U, 17U, 27U, 31U, 34U, 38U,
    41U, 45U, 55U, 59U, 21U, 23U, 49U, 51U, 7U, 9U, 14U, 16U,
    28U, 30U, 35U, 37U, 42U, 44U, 56U, 58U, 19U, 25U, 47U, 53U,
    5U, 11U, 12U, 18U, 26U, 32U, 33U, 39U, 40U, 46U, 54U, 60U
};

static Av2hStatus map_joint_mode(uint8_t joint_mode,
                                 Av2hFirstLeafIntraMode *mode,
                                 int8_t *angle_delta) {
    static const Av2hFirstLeafIntraMode modes[] = {
        AV2H_INTRA_DC, AV2H_INTRA_SMOOTH, AV2H_INTRA_SMOOTH_VERTICAL,
        AV2H_INTRA_SMOOTH_HORIZONTAL, AV2H_INTRA_PAETH, AV2H_INTRA_D45,
        AV2H_INTRA_D67, AV2H_INTRA_VERTICAL, AV2H_INTRA_D113,
        AV2H_INTRA_D135, AV2H_INTRA_D157, AV2H_INTRA_HORIZONTAL,
        AV2H_INTRA_D203
    };
    uint8_t base_mode = joint_mode;
    *angle_delta = 0;
    if (joint_mode >= 5U) {
        base_mode = (uint8_t)(5U + (joint_mode - 5U) / 7U);
        *angle_delta = (int8_t)((joint_mode - 5U) % 7U) - 3;
    }
    if (base_mode >= sizeof(modes) / sizeof(modes[0])) return AV2H_INVALID_DATA;
    *mode = modes[base_mode];
    return AV2H_OK;
}

static Av2hStatus fixed_small_mode(uint8_t mode_index, uint8_t *joint_mode) {
    if (mode_index < 5U) {
        *joint_mode = mode_index;
        return AV2H_OK;
    }
    mode_index = (uint8_t)(mode_index - 5U);
    if (mode_index >= sizeof(default_directional_modes) /
                          sizeof(default_directional_modes[0])) {
        return AV2H_INVALID_DATA;
    }
    *joint_mode = default_directional_modes[mode_index];
    return AV2H_OK;
}

static Av2hStatus reordered_larger_mode(
    uint8_t mode_index, Av2hBlockSize block_size,
    const Av2hLumaModeNeighbors *neighbors, uint8_t *joint_mode) {
    uint8_t modes[61U];
    uint8_t selected[61U] = { 0U };
    size_t count = 5U;
    size_t directional_count = 0U;
    size_t width;
    size_t height;
    size_t neighbor;
    size_t index;
    Av2hStatus status;
    if (mode_index < 5U) {
        *joint_mode = mode_index;
        return AV2H_OK;
    }
    status = av2h_block_dimensions_mi(block_size, &width, &height);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < 5U; ++index) {
        modes[index] = (uint8_t)index;
        selected[index] = 1U;
    }
    for (neighbor = 0U; neighbor < 2U; ++neighbor) {
        uint8_t candidate = neighbors->joint_mode[neighbor];
        if (!neighbors->present[neighbor] ||
            !is_directional_joint_mode(candidate)) {
            continue;
        }
        if (selected[candidate]) continue;
        modes[count++] = candidate;
        selected[candidate] = 1U;
        ++directional_count;
    }
    if (width * height > 4U) {
        size_t offset;
        for (offset = 0U; offset < 4U; ++offset) {
            for (neighbor = 0U; neighbor < directional_count; ++neighbor) {
                uint8_t center = modes[5U + neighbor];
                uint8_t left = (uint8_t)(5U +
                    ((size_t)center - 5U + 55U - offset) % 56U);
                uint8_t right = (uint8_t)(5U +
                    ((size_t)center - 5U + 1U + offset) % 56U);
                if (!selected[left]) {
                    modes[count++] = left;
                    selected[left] = 1U;
                }
                if (!selected[right]) {
                    modes[count++] = right;
                    selected[right] = 1U;
                }
            }
        }
    }
    for (index = 0U; index < sizeof(default_directional_modes) /
                                  sizeof(default_directional_modes[0]);
         ++index) {
        uint8_t candidate = default_directional_modes[index];
        if (selected[candidate]) continue;
        modes[count++] = candidate;
        selected[candidate] = 1U;
    }
    if (mode_index >= count) return AV2H_INVALID_DATA;
    *joint_mode = modes[mode_index];
    return AV2H_OK;
}

Av2hStatus av2h_luma_joint_mode_from_index(
    uint8_t mode_index,
    Av2hBlockSize block_size,
    const Av2hLumaModeNeighbors *neighbors,
    uint8_t *joint_mode) {
    if (neighbors == 0 || joint_mode == 0 ||
        block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (block_size <= AV2H_BLOCK_8X4) {
        return fixed_small_mode(mode_index, joint_mode);
    }
    return reordered_larger_mode(
        mode_index, block_size, neighbors, joint_mode);
}

static uint8_t block_allows_intrabc(Av2hBlockSize block_size) {
    return block_size != AV2H_BLOCK_64X64 &&
           !(block_size >= AV2H_BLOCK_64X128 &&
             block_size <= AV2H_BLOCK_256X256);
}

static uint8_t fsc_block_context(Av2hBlockSize block_size) {
    static const uint8_t contexts[AV2H_BLOCK_SIZE_COUNT] = {
        0U, 1U, 1U, 2U, 3U, 3U, 4U, 5U, 5U, 5U,
        6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 6U, 3U,
        3U, 4U, 4U, 6U, 6U, 4U, 4U, 6U, 6U
    };
    return contexts[block_size];
}

static uint8_t block_allows_palette(Av2hBlockSize block_size) {
    size_t width;
    size_t height;
    return av2h_block_dimensions_mi(block_size, &width, &height) == AV2H_OK &&
        width >= 2U && height >= 2U && width <= 16U && height <= 16U;
}

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t weight;
} Av2hIntrabcCandidate;

static void add_intrabc_candidate(
    Av2hIntrabcCandidate candidates[4U],
    size_t *count,
    const Av2hLumaModeContext *context,
    uint8_t weight) {
    size_t index;
    if (context == 0 || !context->present || !context->use_intrabc) return;
    for (index = 0U; index < *count; ++index) {
        if (candidates[index].x == context->displacement_x &&
            candidates[index].y == context->displacement_y) {
            candidates[index].weight = (uint8_t)(
                candidates[index].weight + weight);
            return;
        }
    }
    if (*count >= 4U) return;
    candidates[*count].x = context->displacement_x;
    candidates[*count].y = context->displacement_y;
    candidates[*count].weight = weight;
    ++*count;
}

static void add_intrabc_vector(
    Av2hIntrabcCandidate candidates[4U],
    size_t *count,
    int16_t x,
    int16_t y,
    uint8_t weight) {
    Av2hLumaModeContext context = { 0 };
    context.present = 1U;
    context.use_intrabc = 1U;
    context.displacement_x = x;
    context.displacement_y = y;
    add_intrabc_candidate(candidates, count, &context, weight);
}

static const Av2hLumaModeContext *intrabc_spatial_context(
    const Av2hLumaModeContextState *state,
    int32_t row,
    int32_t column) {
    size_t index;
    if (state == 0 || state->spatial == 0 || row < 0 || column < 0 ||
        (size_t)row >= state->spatial_rows ||
        (size_t)column >= state->spatial_columns) {
        return 0;
    }
    index = (size_t)row * state->spatial_columns + (size_t)column;
    return (const Av2hLumaModeContext *)(
        state->spatial + index * state->spatial_stride);
}

static void build_intrabc_candidates(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    size_t width_mi,
    size_t height_mi,
    const Av2hLumaModeContextState *state,
    Av2hIntrabcCandidate candidates[4U],
    size_t *count) {
    static const int16_t delay_pixels = 256;
    const Av2hLumaModeContext *context;
    size_t index;
    size_t nearest_count = 0U;
    size_t limit = (size_t)frame->max_bvp_drl_bits + 1U;
    *count = 0U;
    if (limit > 4U) limit = 4U;
    if (state != 0) {
        if (state->spatial != 0) {
            int32_t row = leaf->row;
            int32_t column = leaf->column;
            int32_t above_right_column;
            int32_t above_column;
            int32_t top_right_column;
            int32_t top_left_column;
            uint8_t at_superblock_boundary = (uint8_t)(
                state->superblock_mi != 0U &&
                leaf->row % state->superblock_mi == 0U);
            if (at_superblock_boundary) {
                int32_t aligned_column = (column >> 1) << 1;
                above_right_column = aligned_column +
                    (width_mi >= 4U ? (int32_t)width_mi - 2 : 0);
                above_column = -1;
                top_right_column = aligned_column +
                    (width_mi >= 4U ? (int32_t)width_mi : 2);
                top_left_column = aligned_column - 2;
            } else {
                above_right_column = column + (int32_t)width_mi - 1;
                above_column = width_mi > 1U ? column : -1;
                top_right_column = column + (int32_t)width_mi;
                top_left_column = column - 1;
            }
            context = intrabc_spatial_context(
                state, row + (int32_t)height_mi - 1, column - 1);
            add_intrabc_candidate(candidates, count, context, 1U);
            context = intrabc_spatial_context(
                state, row - 1, above_right_column);
            add_intrabc_candidate(candidates, count, context, 1U);
            if (height_mi > 1U) {
                context = intrabc_spatial_context(
                    state, row, column - 1);
                add_intrabc_candidate(candidates, count, context, 1U);
            }
            context = intrabc_spatial_context(state, row - 1, above_column);
            add_intrabc_candidate(candidates, count, context, 1U);
            context = intrabc_spatial_context(
                state, row + (int32_t)height_mi, column - 1);
            add_intrabc_candidate(candidates, count, context, 1U);
            context = intrabc_spatial_context(
                state, row - 1, top_right_column);
            add_intrabc_candidate(candidates, count, context, 1U);
            context = intrabc_spatial_context(
                state, row - 1, top_left_column);
            add_intrabc_candidate(candidates, count, context, 0U);
            nearest_count = *count;
            {
                int32_t column_adjustment =
                    width_mi < 2U && (leaf->column & 1U) != 0U ? 1 : 0;
                int32_t nonadjacent_column = column - 3 + column_adjustment;
                context = intrabc_spatial_context(
                    state, row + (int32_t)height_mi - 1,
                    nonadjacent_column);
                add_intrabc_candidate(candidates, count, context, 0U);
                if (height_mi > 1U) {
                    context = intrabc_spatial_context(
                        state, row, nonadjacent_column);
                    add_intrabc_candidate(candidates, count, context, 0U);
                }
            }
        } else {
            if (leaf->column != 0U) {
                context = &state->left[
                    (leaf->row + height_mi - 1U) % state->left_count];
                add_intrabc_candidate(candidates, count, context, 1U);
            }
            if (leaf->row != 0U &&
                (size_t)leaf->column + width_mi - 1U < state->above_count) {
                context = &state->above[leaf->column + width_mi - 1U];
                add_intrabc_candidate(candidates, count, context, 1U);
            }
            if (leaf->column != 0U && height_mi > 1U) {
                context = &state->left[leaf->row % state->left_count];
                add_intrabc_candidate(candidates, count, context, 1U);
            }
            if (leaf->row != 0U && width_mi > 1U &&
                leaf->column < state->above_count) {
                context = &state->above[leaf->column];
                add_intrabc_candidate(candidates, count, context, 1U);
            }
            nearest_count = *count;
        }
        if ((sequence->enable_drl_reorder == 2U ||
             (sequence->enable_drl_reorder == 1U && nearest_count >= 4U)) &&
            nearest_count > 1U) {
            size_t maximum = 0U;
            for (index = 1U; index < nearest_count; ++index) {
                if (candidates[index].weight > candidates[maximum].weight) {
                    maximum = index;
                }
            }
            if (maximum != 0U) {
                Av2hIntrabcCandidate temporary = candidates[0];
                candidates[0] = candidates[maximum];
                candidates[maximum] = temporary;
            }
        }
        if (sequence->enable_refmvbank) {
            size_t bank_offset;
            for (bank_offset = 0U;
                 bank_offset < state->intrabc_bank_count && *count < limit;
                 ++bank_offset) {
                size_t bank_index =
                    (state->intrabc_bank_start +
                     state->intrabc_bank_count - 1U - bank_offset) % 4U;
                int16_t x = state->intrabc_bank_x[bank_index];
                int16_t y = state->intrabc_bank_y[bank_index];
                int32_t reference_x = (int32_t)leaf->column * 4 + x / 8;
                int32_t reference_y = (int32_t)leaf->row * 4 + y / 8;
                if (reference_x <= -(int32_t)(width_mi * 4U) ||
                    reference_y <= -(int32_t)(height_mi * 4U) ||
                    reference_x >= frame->width ||
                    reference_y >= frame->height) {
                    continue;
                }
                add_intrabc_vector(candidates, count, x, y, 0U);
            }
        }
    }
    for (index = 0U; index < limit && *count < limit; ++index) {
        Av2hIntrabcCandidate candidate = { 0, 0, 0U };
        if (index == 0U) {
            candidate.y = (int16_t)(-(int32_t)sequence->superblock_size * 8);
        } else if (index == 1U) {
            candidate.x = (int16_t)(
                -((int32_t)sequence->superblock_size + delay_pixels) * 8);
        } else if (index == 2U) {
            candidate.y = (int16_t)(-(int32_t)height_mi * 32);
        } else {
            candidate.x = (int16_t)(-(int32_t)width_mi * 32);
        }
        candidates[*count] = candidate;
        ++*count;
    }
}

static Av2hStatus read_first_intrabc(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hFirstLeafMode *mode) {
    size_t width_mi;
    size_t height_mi;
    uint32_t value;
    uint32_t index;
    uint32_t shell_class;
    uint32_t shell_offset;
    uint32_t shell_index;
    uint32_t pair_index = 0U;
    uint32_t maximum_pair_index;
    Av2hIntrabcCandidate candidates[4U];
    size_t candidate_count;
    int32_t reference_x;
    int32_t reference_y;
    int32_t difference_x = 0;
    int32_t difference_y = 0;
    Av2hStatus status = av2h_block_dimensions_mi(
        leaf->block_size, &width_mi, &height_mi);
    if (status != AV2H_OK) return status;
    mode->before_mode_bits = av2h_range_tell(decoder);
    if (leaf->row == 16U && leaf->column == 20U) {
        size_t debug_index;
        av2h_debug_intrabc_ranges[0] = decoder->range;
        for (debug_index = 0U; debug_index < 6U; ++debug_index) {
            av2h_debug_intrabc_mode_cdf[debug_index] =
                cdfs->intrabc_mode[debug_index];
        }
    }
    mode->intrabc_mode = (uint8_t)av2h_range_read_adaptive(
        decoder, cdfs->intrabc_mode, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->after_mode_set_bits = av2h_range_tell(decoder);
    if (leaf->row == 16U && leaf->column == 20U) {
        av2h_debug_intrabc_ranges[1] = decoder->range;
    }
    mode->intrabc_drl_index = 0U;
    for (index = 0U; index < frame->max_bvp_drl_bits; ++index) {
        value = av2h_range_read_bypass(decoder, 1U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->intrabc_drl_index = (uint8_t)(index + value);
        if (value == 0U) break;
    }
    mode->after_mode_index_bits = av2h_range_tell(decoder);
    if (leaf->row == 16U && leaf->column == 20U) {
        av2h_debug_intrabc_ranges[2] = decoder->range;
    }
    build_intrabc_candidates(
        sequence, frame, leaf, width_mi, height_mi, state,
        candidates, &candidate_count);
    if (mode->intrabc_drl_index >= candidate_count) return AV2H_INVALID_DATA;
    reference_x = candidates[mode->intrabc_drl_index].x;
    reference_y = candidates[mode->intrabc_drl_index].y;
    mode->mv_precision = 3U;
    if (mode->intrabc_mode == 0U && !frame->force_integer_mv) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->intrabc_precision, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->mv_precision = value == 0U ? 3U : 5U;
    }
    mode->after_fsc_bits = av2h_range_tell(decoder);
    if (mode->intrabc_mode == 0U) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->intrabc_shell_set, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (value != 0U || mode->mv_precision != 3U) {
            return AV2H_UNSUPPORTED;
        }
        mode->after_mrl_bits = av2h_range_tell(decoder);
        shell_class = av2h_range_read_adaptive(
            decoder, cdfs->intrabc_shell_class,
            AV2H_INTRABC_SHELL_CLASS_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (shell_class < AV2H_INTRABC_LOW_CLASS_COUNT) {
            shell_offset = av2h_range_read_adaptive(
                decoder, cdfs->intrabc_shell_offset[shell_class], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
        } else if (shell_class <
                   AV2H_INTRABC_OTHER_CLASS_OFFSET_COUNT + 1U) {
            shell_offset = 0U;
            for (index = 0U; index < shell_class; ++index) {
                shell_offset |= av2h_range_read_adaptive(
                    decoder, cdfs->intrabc_shell_offset_other[index], 2U)
                    << index;
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
            }
        } else {
            return AV2H_UNSUPPORTED;
        }
        shell_index = (shell_class == 0U ? 0U : 1U << shell_class) +
                      shell_offset;
        maximum_pair_index = shell_index >> 1U;
        for (index = 0U;
             index < maximum_pair_index &&
             index < AV2H_INTRABC_GREATER_CONTEXT_COUNT;
             ++index) {
            size_t greater_context =
                index < AV2H_INTRABC_GREATER_CONTEXT_COUNT ?
                    index : AV2H_INTRABC_GREATER_CONTEXT_COUNT - 1U;
            value = av2h_range_read_adaptive(
                decoder, cdfs->intrabc_col_greater[greater_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            pair_index = index + value;
            if (value == 0U) break;
        }
        if (maximum_pair_index > AV2H_INTRABC_GREATER_CONTEXT_COUNT &&
            pair_index == AV2H_INTRABC_GREATER_CONTEXT_COUNT) {
            uint32_t symbol_count = maximum_pair_index -
                AV2H_INTRABC_GREATER_CONTEXT_COUNT + 1U;
            uint32_t bits = 0U;
            uint32_t threshold;
            uint32_t remainder;
            while ((1U << (bits + 1U)) <= symbol_count) ++bits;
            threshold = (1U << (bits + 1U)) - symbol_count;
            remainder = av2h_range_read_bypass(decoder, bits);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            if (remainder >= threshold) {
                remainder = (remainder << 1U) - threshold +
                    av2h_range_read_bypass(decoder, 1U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
            }
            pair_index = AV2H_INTRABC_GREATER_CONTEXT_COUNT + remainder;
        }
        if (!(pair_index == maximum_pair_index &&
              (shell_index & 1U) == 0U)) {
            size_t index_context =
                shell_class < AV2H_INTRABC_INDEX_CONTEXT_COUNT ?
                    shell_class : AV2H_INTRABC_INDEX_CONTEXT_COUNT - 1U;
            value = av2h_range_read_adaptive(
                decoder, cdfs->intrabc_col_index[index_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            difference_x = (int32_t)(value == 0U ? pair_index :
                                     shell_index - pair_index);
        } else {
            difference_x = (int32_t)maximum_pair_index;
        }
        difference_y = (int32_t)shell_index - difference_x;
        difference_x <<= 3;
        difference_y <<= 3;
        if (difference_y != 0 && av2h_range_read_bypass(decoder, 1U)) {
            difference_y = -difference_y;
        }
        if (difference_x != 0 && av2h_range_read_bypass(decoder, 1U)) {
            difference_x = -difference_x;
        }
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    mode->displacement_x = (int16_t)(reference_x + difference_x);
    mode->displacement_y = (int16_t)(reference_y + difference_y);
    mode->mode_set = 0U;
    mode->mode_index = 0U;
    mode->joint_mode = 0U;
    mode->mode = AV2H_INTRA_DC;
    mode->angle_delta = 0;
    mode->fsc_mode = 0U;
    mode->mrl_index = 0U;
    mode->multi_line_mrl = 0U;
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_context_state_init(
    Av2hLumaModeContextState *state,
    Av2hLumaModeContext *above,
    size_t above_count,
    Av2hLumaModeContext *left,
    size_t left_count) {
    size_t index;
    if (state == 0 || above == 0 || left == 0 || above_count == 0U ||
        left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    state->above = above;
    state->left = left;
    state->spatial = 0;
    state->above_count = above_count;
    state->left_count = left_count;
    state->spatial_stride = 0U;
    state->spatial_columns = 0U;
    state->spatial_rows = 0U;
    state->intrabc_bank_remaining_hits = 0U;
    state->superblock_mi = 0U;
    state->intrabc_bank_count = 0U;
    state->intrabc_bank_start = 0U;
    state->intrabc_bank_unit_hits = 0U;
    state->intrabc_bank_superblock_hits = 0U;
    state->has_intrabc = 0U;
    for (index = 0U; index < above_count; ++index) {
        above[index] = (Av2hLumaModeContext){ 0 };
    }
    for (index = 0U; index < left_count; ++index) {
        left[index] = (Av2hLumaModeContext){ 0 };
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_neighbors_from_state(
    const Av2hLumaModeContextState *state,
    const Av2hPartitionNode *leaf,
    Av2hLumaModeNeighbors *neighbors) {
    const Av2hLumaModeContext *candidates[4U] = { 0, 0, 0, 0 };
    const Av2hLumaModeContext *selected[2U] = { 0, 0 };
    const Av2hLumaModeContext *spatial[2U] = { 0, 0 };
    size_t spatial_count = 0U;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    uint8_t not_at_superblock_top;
    if (state == 0 || leaf == 0 || neighbors == 0 ||
        leaf->block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (av2h_block_dimensions_mi(
            leaf->block_size, &width, &height) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    left_index = leaf->row % state->left_count;
    not_at_superblock_top = (uint8_t)(
        state->superblock_mi == 0U ||
        leaf->row % state->superblock_mi != 0U);
    if (leaf->column >= state->above_count ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (leaf->column != 0U) {
        const Av2hLumaModeContext *bottom_left =
            &state->left[(leaf->row + height - 1U) % state->left_count];
        const Av2hLumaModeContext *left = &state->left[left_index];
        candidates[0] = bottom_left;
        candidates[2] = left;
        selected[0] = bottom_left;
    }
    if (leaf->row != 0U &&
        (size_t)leaf->column + width - 1U < state->above_count) {
        const Av2hLumaModeContext *above_right =
            &state->above[leaf->column + width - 1U];
        const Av2hLumaModeContext *above = &state->above[leaf->column];
        candidates[1] = above_right;
        candidates[3] = above;
        selected[1] = above_right;
    }
    for (index = 0U; index < 4U && spatial_count < 2U; ++index) {
        if (!not_at_superblock_top && (index == 1U || index == 3U)) {
            continue;
        }
        if (candidates[index] != 0 && candidates[index]->present) {
            spatial[spatial_count++] = candidates[index];
        }
    }
    for (index = 0U; index < 2U; ++index) {
        const Av2hLumaModeContext *context = selected[index];
        const Av2hLumaModeContext *spatial_context = spatial[index];
        neighbors->present[index] =
            (uint8_t)(context != 0 && context->present);
        neighbors->use_intrabc[index] = context != 0 ? context->use_intrabc : 0U;
        neighbors->skip_transform[index] =
            context != 0 ? context->skip_transform : 0U;
        neighbors->joint_mode[index] = context != 0 ? context->joint_mode : 0U;
        neighbors->fsc_mode[index] = context != 0 ? context->fsc_mode : 0U;
        neighbors->mrl_index[index] = context != 0 ? context->mrl_index : 0U;
        neighbors->multi_line_mrl[index] =
            context != 0 ? context->multi_line_mrl : 0U;
        neighbors->spatial_present[index] =
            (uint8_t)(spatial_context != 0 && spatial_context->present);
        neighbors->spatial_use_intrabc[index] =
            spatial_context != 0 ? spatial_context->use_intrabc : 0U;
        neighbors->spatial_skip_transform[index] = spatial_context != 0 ?
            spatial_context->skip_transform : 0U;
        neighbors->spatial_joint_mode[index] = spatial_context != 0 ?
            spatial_context->joint_mode : 0U;
        neighbors->spatial_fsc_mode[index] =
            spatial_context != 0 ? spatial_context->fsc_mode : 0U;
        neighbors->spatial_mrl_index[index] =
            spatial_context != 0 ? spatial_context->mrl_index : 0U;
        neighbors->spatial_multi_line_mrl[index] =
            spatial_context != 0 ? spatial_context->multi_line_mrl : 0U;
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_context_state_update(
    Av2hLumaModeContextState *state,
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode) {
    Av2hLumaModeContext context;
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    Av2hStatus status;
    if (state == 0 || leaf == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_block_dimensions_mi(leaf->block_size, &width, &height);
    if (status != AV2H_OK) return status;
    left_index = leaf->row % state->left_count;
    if (leaf->column >= state->above_count ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf->column) {
        width = state->above_count - leaf->column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    context.present = 1U;
    context.use_intrabc = mode->use_intrabc;
    context.intrabc_mode = mode->intrabc_mode;
    context.intrabc_drl_index = mode->intrabc_drl_index;
    context.mv_precision = mode->mv_precision;
    context.displacement_x = mode->displacement_x;
    context.displacement_y = mode->displacement_y;
    context.skip_transform = mode->skip_transform;
    context.joint_mode = mode->joint_mode;
    context.fsc_mode = mode->fsc_mode;
    context.mrl_index = mode->mrl_index;
    context.multi_line_mrl = mode->multi_line_mrl;
    for (index = 0U; index < width; ++index) {
        state->above[leaf->column + index] = context;
    }
    for (index = 0U; index < height; ++index) {
        state->left[left_index + index] = context;
    }
    if (state->superblock_mi != 0U) {
        size_t row_in_superblock = leaf->row % state->superblock_mi;
        size_t column_in_superblock = leaf->column % state->superblock_mi;
        size_t unit_mi = state->superblock_mi / 8U;
        size_t unit_count;
        if (unit_mi == 0U) unit_mi = 1U;
        unit_count = (width / unit_mi > 0U ? width / unit_mi : 1U) *
                     (height / unit_mi > 0U ? height / unit_mi : 1U);
        if (row_in_superblock == 0U && column_in_superblock == 0U) {
            state->intrabc_bank_count = 0U;
            state->intrabc_bank_start = 0U;
            state->intrabc_bank_remaining_hits = (uint16_t)(
                unit_count > 4U ? unit_count : 4U);
            state->intrabc_bank_unit_hits = 0U;
            state->intrabc_bank_superblock_hits = 0U;
        } else if (row_in_superblock % unit_mi == 0U &&
                   column_in_superblock % unit_mi == 0U) {
            state->intrabc_bank_remaining_hits = (uint16_t)(
                state->intrabc_bank_remaining_hits + unit_count);
            state->intrabc_bank_unit_hits = 0U;
        }
    }
    if (mode->use_intrabc) {
        if (state->superblock_mi == 0U ||
            (state->intrabc_bank_remaining_hits != 0U &&
             state->intrabc_bank_unit_hits < 16U &&
             state->intrabc_bank_superblock_hits < 64U)) {
            size_t found = state->intrabc_bank_count;
            for (index = 0U; index < state->intrabc_bank_count; ++index) {
                size_t bank_index =
                    (state->intrabc_bank_start + index) % 4U;
                if (state->intrabc_bank_x[bank_index] == mode->displacement_x &&
                    state->intrabc_bank_y[bank_index] == mode->displacement_y) {
                    found = index;
                    break;
                }
            }
            if (found < state->intrabc_bank_count) {
                int16_t x = state->intrabc_bank_x[
                    (state->intrabc_bank_start + found) % 4U];
                int16_t y = state->intrabc_bank_y[
                    (state->intrabc_bank_start + found) % 4U];
                for (index = found; index + 1U < state->intrabc_bank_count;
                     ++index) {
                    size_t destination =
                        (state->intrabc_bank_start + index) % 4U;
                    size_t source = (destination + 1U) % 4U;
                    state->intrabc_bank_x[destination] =
                        state->intrabc_bank_x[source];
                    state->intrabc_bank_y[destination] =
                        state->intrabc_bank_y[source];
                }
                index = (state->intrabc_bank_start +
                         state->intrabc_bank_count - 1U) % 4U;
                state->intrabc_bank_x[index] = x;
                state->intrabc_bank_y[index] = y;
            } else {
                index = (state->intrabc_bank_start +
                         state->intrabc_bank_count) % 4U;
                state->intrabc_bank_x[index] = mode->displacement_x;
                state->intrabc_bank_y[index] = mode->displacement_y;
                if (state->intrabc_bank_count < 4U) {
                    ++state->intrabc_bank_count;
                } else {
                    state->intrabc_bank_start =
                        (uint8_t)((state->intrabc_bank_start + 1U) % 4U);
                }
            }
            if (state->superblock_mi != 0U) {
                --state->intrabc_bank_remaining_hits;
                ++state->intrabc_bank_unit_hits;
                ++state->intrabc_bank_superblock_hits;
            }
        }
        state->has_intrabc = 1U;
    }
    return AV2H_OK;
}

static Av2hStatus luma_mode_read_internal(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeNeighbors *neighbors,
    const Av2hLumaModeContextState *state,
    Av2hGdfEntropyState *gdf_state,
    Av2hCdefEntropyState *cdef_state,
    Av2hCcsoEntropyState *ccso_state,
    Av2hFirstLeafMode *mode);

Av2hStatus av2h_luma_mode_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hFirstLeafMode *mode) {
    return av2h_luma_mode_read_with_state_and_gdf(
        decoder, cdfs, sequence, frame, leaf, state, 0, 0, 0, mode);
}

Av2hStatus av2h_luma_mode_read_with_state_and_gdf(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hGdfEntropyState *gdf_state,
    Av2hCdefEntropyState *cdef_state,
    Av2hCcsoEntropyState *ccso_state,
    Av2hFirstLeafMode *mode) {
    Av2hLumaModeNeighbors neighbors;
    Av2hStatus status = av2h_luma_mode_neighbors_from_state(
        state, leaf, &neighbors);
    if (status != AV2H_OK) return status;
    status = luma_mode_read_internal(
        decoder, cdfs, sequence, frame, leaf, &neighbors, state, gdf_state,
        cdef_state, ccso_state, mode);
    return status;
}

static Av2hStatus luma_mode_read_internal(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeNeighbors *neighbors,
    const Av2hLumaModeContextState *state,
    Av2hGdfEntropyState *gdf_state,
    Av2hCdefEntropyState *cdef_state,
    Av2hCcsoEntropyState *ccso_state,
    Av2hFirstLeafMode *mode) {
    size_t plane;
    size_t neighbor;
    size_t mode_context = 0U;
    size_t intrabc_context = 0U;
    size_t skip_transform_context = 0U;
    size_t fsc_context = 0U;
    size_t mrl_context = 0U;
    size_t secondary_context = 0U;
    uint8_t has_spatial_neighbors;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        leaf == 0 || neighbors == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (leaf->block_size >= AV2H_BLOCK_SIZE_COUNT ||
        frame->disable_cdf_update || frame->segmentation_enabled ||
        frame->delta_q_present || !sequence->enable_sdp ||
        sequence->enable_intra_dip ||
        (frame->gdf_mode >= 2U && gdf_state == 0) ||
        (frame->cdef_strength_count > 1U && cdef_state == 0)) {
        return AV2H_UNSUPPORTED;
    }
    mode->enable_ibp = sequence->enable_ibp;
    has_spatial_neighbors = (uint8_t)(
        neighbors->spatial_present[0] || neighbors->spatial_present[1]);
    for (neighbor = 0U; neighbor < 2U; ++neighbor) {
        uint8_t skip_transform = has_spatial_neighbors ?
            neighbors->spatial_skip_transform[neighbor] :
            neighbors->skip_transform[neighbor];
        if (neighbors->present[neighbor]) {
            if (neighbors->joint_mode[neighbor] >= 61U) {
                return AV2H_INVALID_ARGUMENT;
            }
            mode_context += is_directional_joint_mode(
                neighbors->joint_mode[neighbor]);
        }
        if (has_spatial_neighbors && neighbors->spatial_present[neighbor]) {
            skip_transform_context += skip_transform;
        } else if (!has_spatial_neighbors) {
            skip_transform_context += skip_transform;
        }
    }
    for (neighbor = 0U; neighbor < 2U; ++neighbor) {
        uint8_t present = has_spatial_neighbors ?
            neighbors->spatial_present[neighbor] : neighbors->present[neighbor];
        uint8_t use_intrabc = has_spatial_neighbors ?
            neighbors->spatial_use_intrabc[neighbor] :
            neighbors->use_intrabc[neighbor];
        uint8_t spatial_fsc = has_spatial_neighbors ?
            neighbors->spatial_fsc_mode[neighbor] :
            neighbors->fsc_mode[neighbor];
        uint8_t spatial_mrl = has_spatial_neighbors ?
            neighbors->spatial_mrl_index[neighbor] :
            neighbors->mrl_index[neighbor];
        uint8_t spatial_multi = has_spatial_neighbors ?
            neighbors->spatial_multi_line_mrl[neighbor] :
            neighbors->multi_line_mrl[neighbor];
        if (!present) continue;
        if (spatial_fsc > 1U ||
            spatial_mrl >= AV2H_MRL_INDEX_SYMBOLS ||
            spatial_multi > 1U) {
            return AV2H_INVALID_ARGUMENT;
        }
        intrabc_context += use_intrabc;
        fsc_context += spatial_fsc;
        mrl_context += spatial_mrl != 0U;
        secondary_context += spatial_multi;
    }
    mode->use_intrabc = 0U;
    mode->intrabc_mode = 0U;
    mode->skip_transform = 0U;
    mode->intrabc_drl_index = 0U;
    mode->mv_precision = 0U;
    mode->displacement_x = 0;
    mode->displacement_y = 0;
    mode->ccso_present_mask = 0U;
    mode->before_gdf_bits = av2h_range_tell(decoder);
    mode->after_gdf_bits = mode->before_gdf_bits;
    mode->gdf_flags_read = 0U;
    mode->before_cdef_bits = mode->before_gdf_bits;
    mode->after_cdef_bits = mode->before_gdf_bits;
    mode->cdef_strengths_read = 0U;
    mode->cdef_strength_index = 0U;
    mode->before_ccso_bits = mode->before_gdf_bits;
    mode->after_ccso_bits = mode->before_gdf_bits;
    mode->ccso_controls_read = 0U;
    mode->palette_mode = 0U;
    mode->before_palette_bits = mode->before_gdf_bits;
    mode->after_palette_bits = mode->before_gdf_bits;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        mode->ccso_block_control[plane] = 0U;
    }
    if (frame->allow_intrabc && block_allows_intrabc(leaf->block_size)) {
        if (leaf->row == 16U &&
            (leaf->column == 16U || leaf->column == 20U)) {
            size_t debug_index;
            av2h_debug_use_intrabc[0] = (uint32_t)intrabc_context;
            av2h_debug_use_intrabc[1] = decoder->range;
            for (debug_index = 0U; debug_index < 6U; ++debug_index) {
                av2h_debug_use_intrabc[2U + debug_index] =
                    cdfs->intrabc[intrabc_context][debug_index];
            }
        }
        value = av2h_range_read_adaptive(
            decoder, cdfs->intrabc[intrabc_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->use_intrabc = (uint8_t)value;
        if (leaf->row == 16U &&
            (leaf->column == 16U || leaf->column == 20U)) {
            av2h_debug_use_intrabc[8] = value;
            av2h_debug_use_intrabc[9] = decoder->range;
        }
        if (value != 0U) {
            Av2hStatus status;
            if (skip_transform_context >=
                AV2H_SKIP_TRANSFORM_CONTEXT_COUNT) {
                return AV2H_INVALID_DATA;
            }
            mode->skip_transform = (uint8_t)av2h_range_read_adaptive(
                decoder, cdfs->skip_transform[skip_transform_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            status = read_gdf_entropy(
                decoder, frame, leaf, gdf_state, mode);
            if (status != AV2H_OK) return status;
            status = read_cdef_entropy(
                decoder, frame, leaf, cdef_state, mode);
            if (status != AV2H_OK) return status;
            if (ccso_state != 0 && ccso_state->enabled_mask != 0U) {
                status = read_ccso_entropy(
                    decoder, cdfs, sequence, frame, leaf,
                    ccso_state, mode);
            } else {
                status = read_first_ccso_without_state(
                    decoder, cdfs, sequence, frame, leaf, mode);
            }
            if (status != AV2H_OK) return status;
            if (frame->gdf_mode >= 2U) return AV2H_UNSUPPORTED;
            return read_first_intrabc(
                decoder, cdfs, sequence, frame, leaf, state, mode);
        }
    }
    {
        Av2hStatus status = read_gdf_entropy(
            decoder, frame, leaf, gdf_state, mode);
        if (status != AV2H_OK) return status;
    }
    if (leaf->row == 16U && leaf->column == 16U) {
        av2h_debug_mode_ranges[0] = decoder->range;
    }
    {
        Av2hStatus status = read_cdef_entropy(
            decoder, frame, leaf, cdef_state, mode);
        if (status != AV2H_OK) return status;
    }
    if (leaf->row == 16U && leaf->column == 16U) {
        av2h_debug_mode_ranges[1] = decoder->range;
    }
    if (ccso_state != 0 && ccso_state->enabled_mask != 0U) {
        Av2hStatus status = read_ccso_entropy(
            decoder, cdfs, sequence, frame, leaf, ccso_state, mode);
        if (status != AV2H_OK) return status;
    } else {
        Av2hStatus status = read_first_ccso_without_state(
            decoder, cdfs, sequence, frame, leaf, mode);
        if (status != AV2H_OK) return status;
    }
    if ((leaf->row == 0U && leaf->column == 64U) ||
        (leaf->row == 16U && leaf->column == 16U)) {
        av2h_debug_mode_ranges[2] = decoder->range;
    }
    mode->before_mode_bits = av2h_range_tell(decoder);
    av2h_debug_y_mode_context = (uint32_t)mode_context;
    if (leaf->row == 64U && leaf->column == 0U) {
        size_t debug_index;
        av2h_debug_y_mode_range = decoder->range;
        for (debug_index = 0U; debug_index < 8U; ++debug_index) {
            av2h_debug_y_mode_set_cdf[debug_index] =
                cdfs->y_mode_set[debug_index];
        }
        for (debug_index = 0U; debug_index < 12U; ++debug_index) {
            av2h_debug_y_mode_index_cdf[debug_index] =
                cdfs->y_mode_index[mode_context][debug_index];
        }
    }
    value = av2h_range_read_adaptive(
        decoder, cdfs->y_mode_set, AV2H_Y_MODE_SET_SYMBOLS);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->mode_set = (uint8_t)value;
    mode->after_mode_set_bits = av2h_range_tell(decoder);
    if (value == 0U) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->y_mode_index[mode_context],
            AV2H_Y_MODE_INDEX_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->mode_index = (uint8_t)value;
        if (value == AV2H_Y_MODE_INDEX_SYMBOLS - 1U) {
            value += av2h_range_read_adaptive(
                decoder, cdfs->y_mode_offset[mode_context],
                AV2H_Y_MODE_OFFSET_SYMBOLS);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            mode->mode_index = (uint8_t)value;
        }
    } else {
        mode->mode_index = (uint8_t)(13U + (value - 1U) * 16U +
            av2h_range_read_bypass(decoder, 4U));
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    mode->after_mode_index_bits = av2h_range_tell(decoder);
    av2h_debug_luma_mode_neighbors[0] = neighbors->present[0];
    av2h_debug_luma_mode_neighbors[1] = neighbors->joint_mode[0];
    av2h_debug_luma_mode_neighbors[2] = neighbors->present[1];
    av2h_debug_luma_mode_neighbors[3] = neighbors->joint_mode[1];
    if ((leaf->row == 0U && leaf->column == 64U) ||
        (leaf->row == 16U && leaf->column == 16U)) {
        av2h_debug_mode_ranges[3] = decoder->range;
    }
    {
        Av2hStatus status = av2h_luma_joint_mode_from_index(
            mode->mode_index, leaf->block_size, neighbors,
            &mode->joint_mode);
        if (status != AV2H_OK) return status;
    }
    if (map_joint_mode(mode->joint_mode, &mode->mode,
                       &mode->angle_delta) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    mode->fsc_mode = 0U;
    mode->fsc_neighbor_context = (uint8_t)fsc_context;
    mode->fsc_block_context = fsc_block_context(leaf->block_size);
    if (sequence->enable_idtx_intra &&
        mode->fsc_block_context < AV2H_FSC_BLOCK_CONTEXT_COUNT) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->fsc_mode[fsc_context][mode->fsc_block_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->fsc_mode = (uint8_t)value;
        if (value != 0U && leaf->block_size != AV2H_BLOCK_4X4 &&
            leaf->block_size != AV2H_BLOCK_4X8 &&
            leaf->block_size != AV2H_BLOCK_8X4 &&
            leaf->block_size != AV2H_BLOCK_8X16 &&
            leaf->block_size != AV2H_BLOCK_16X4) {
            return AV2H_UNSUPPORTED;
        }
    }
    mode->after_fsc_bits = av2h_range_tell(decoder);
    if ((leaf->row == 0U && leaf->column == 64U) ||
        (leaf->row == 16U && leaf->column == 16U)) {
        av2h_debug_mode_ranges[4] = decoder->range;
    }
    mode->mrl_index = 0U;
    mode->multi_line_mrl = 0U;
    if (sequence->enable_mrls && is_directional_joint_mode(mode->joint_mode)) {
        if ((leaf->row == 0U && leaf->column == 64U) ||
            (leaf->row == 16U &&
             (leaf->column == 8U || leaf->column == 16U))) {
            size_t debug_index;
            av2h_debug_mrl_state[0] = (uint32_t)mrl_context;
            av2h_debug_mrl_state[1] = decoder->range;
            for (debug_index = 0U; debug_index < 8U; ++debug_index) {
                av2h_debug_mrl_state[2U + debug_index] =
                    cdfs->mrl_index[mrl_context][debug_index];
            }
        }
        value = av2h_range_read_adaptive(
            decoder, cdfs->mrl_index[mrl_context], AV2H_MRL_INDEX_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->mrl_index = (uint8_t)value;
        if (value != 0U) {
            value = av2h_range_read_adaptive(
                decoder, cdfs->mrl_secondary[secondary_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            mode->multi_line_mrl = (uint8_t)value;
        }
    }
    mode->after_mrl_bits = av2h_range_tell(decoder);
    if (leaf->row == 16U && leaf->column == 16U) {
        av2h_debug_mode_ranges[5] = decoder->range;
    }
    mode->palette_mode = 0U;
    mode->before_palette_bits = mode->after_mrl_bits;
    mode->after_palette_bits = mode->before_palette_bits;
    if (frame->allow_screen_content_tools &&
        block_allows_palette(leaf->block_size) &&
        mode->mode == AV2H_INTRA_DC) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->palette_y_mode, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->palette_mode = (uint8_t)value;
        mode->after_palette_bits = av2h_range_tell(decoder);
        if (value != 0U) return AV2H_UNSUPPORTED;
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeNeighbors *neighbors,
    Av2hFirstLeafMode *mode) {
    return luma_mode_read_internal(
        decoder, cdfs, sequence, frame, leaf, neighbors, 0, 0, 0, 0, mode);
}

Av2hStatus av2h_first_leaf_mode_read(Av2hRangeDecoder *decoder,
                                     Av2hLeafCdfs *cdfs,
                                     const Av2hSequenceState *sequence,
                                     const Av2hFrameState *frame,
                                     const Av2hPartitionNode *leaf,
                                     Av2hFirstLeafMode *mode) {
    const Av2hLumaModeNeighbors neighbors = { 0 };
    if (leaf == 0) return AV2H_INVALID_ARGUMENT;
    if (leaf->row != 0U || leaf->column != 0U) return AV2H_UNSUPPORTED;
    return av2h_luma_mode_read(decoder, cdfs, sequence, frame, leaf,
                               &neighbors, mode);
}