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
        decoder, cdfs, sequence, colocated_luma_mode, 0, cfl_context, mode);
}

Av2hStatus av2h_chroma_mode_read_with_delta(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    Av2hFirstLeafIntraMode colocated_luma_mode,
    int8_t colocated_luma_delta,
    uint8_t cfl_context,
    Av2hFirstChromaMode *mode) {
    uint8_t mode_context;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (!sequence->enable_cfl_intra || cfl_context >= AV2H_CFL_CONTEXT_COUNT ||
        colocated_luma_mode > AV2H_INTRA_PAETH) {
        return AV2H_UNSUPPORTED;
    }
    mode->before_cfl_bits = av2h_range_tell(decoder);
    value = av2h_range_read_adaptive(decoder, cdfs->is_cfl[cfl_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->is_cfl = (uint8_t)value;
    mode->after_cfl_bits = av2h_range_tell(decoder);
    if (value != 0U) return AV2H_UNSUPPORTED;
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

static Av2hStatus read_first_intrabc(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
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
    int32_t reference_x;
    int32_t reference_y;
    int32_t difference_x = 0;
    int32_t difference_y = 0;
    Av2hStatus status = av2h_block_dimensions_mi(
        leaf->block_size, &width_mi, &height_mi);
    if (status != AV2H_OK) return status;
    mode->before_mode_bits = av2h_range_tell(decoder);
    mode->intrabc_mode = (uint8_t)av2h_range_read_adaptive(
        decoder, cdfs->intrabc_mode, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->after_mode_set_bits = av2h_range_tell(decoder);
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
    if (mode->intrabc_drl_index > 3U) return AV2H_UNSUPPORTED;
    if (mode->intrabc_drl_index == 0U) {
        reference_x = 0;
        reference_y = -(int32_t)sequence->superblock_size * 8;
    } else if (mode->intrabc_drl_index == 1U) {
        reference_x = -((int32_t)sequence->superblock_size + 256) * 8;
        reference_y = 0;
    } else if (mode->intrabc_drl_index == 2U) {
        reference_x = 0;
        reference_y = -(int32_t)height_mi * 32;
    } else {
        reference_x = -(int32_t)width_mi * 32;
        reference_y = 0;
    }
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
        if (shell_class >= AV2H_INTRABC_LOW_CLASS_COUNT) {
            return AV2H_UNSUPPORTED;
        }
        shell_offset = av2h_range_read_adaptive(
            decoder, cdfs->intrabc_shell_offset[shell_class], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        shell_index = (shell_class == 0U ? 0U : 1U << shell_class) +
                      shell_offset;
        maximum_pair_index = shell_index >> 1U;
        if (maximum_pair_index > AV2H_INTRABC_GREATER_CONTEXT_COUNT) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < maximum_pair_index; ++index) {
            value = av2h_range_read_adaptive(
                decoder, cdfs->intrabc_col_greater[index], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            pair_index = index + value;
            if (value == 0U) break;
        }
        if (!(pair_index == maximum_pair_index &&
              (shell_index & 1U) == 0U)) {
            value = av2h_range_read_adaptive(
                decoder, cdfs->intrabc_col_index[shell_class], 2U);
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
    state->above_count = above_count;
    state->left_count = left_count;
    state->has_intrabc = 0U;
    for (index = 0U; index < above_count; ++index) {
        above[index] = (Av2hLumaModeContext){ 0U, 0U, 0U, 0U, 0U, 0U };
    }
    for (index = 0U; index < left_count; ++index) {
        left[index] = (Av2hLumaModeContext){ 0U, 0U, 0U, 0U, 0U, 0U };
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_neighbors_from_state(
    const Av2hLumaModeContextState *state,
    const Av2hPartitionNode *leaf,
    Av2hLumaModeNeighbors *neighbors) {
    const Av2hLumaModeContext *selected[2U] = { 0, 0 };
    size_t width;
    size_t height;
    size_t left_index;
    size_t index;
    if (state == 0 || leaf == 0 || neighbors == 0 ||
        leaf->block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (av2h_block_dimensions_mi(
            leaf->block_size, &width, &height) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    left_index = leaf->row % state->left_count;
    if (leaf->column >= state->above_count ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (leaf->column != 0U) {
        const Av2hLumaModeContext *bottom_left =
            &state->left[(leaf->row + height - 1U) % state->left_count];
        const Av2hLumaModeContext *left = &state->left[left_index];
        selected[0] = bottom_left->present ? bottom_left : left;
    }
    if ((leaf->row & 15U) != 0U &&
        (size_t)leaf->column + width - 1U < state->above_count) {
        const Av2hLumaModeContext *above_right =
            &state->above[leaf->column + width - 1U];
        const Av2hLumaModeContext *above = &state->above[leaf->column];
        selected[1] = above_right->present ? above_right : above;
    }
    for (index = 0U; index < 2U; ++index) {
        const Av2hLumaModeContext *context = selected[index];
        neighbors->present[index] =
            (uint8_t)(context != 0 && context->present);
        neighbors->use_intrabc[index] = context != 0 ? context->use_intrabc : 0U;
        neighbors->joint_mode[index] = context != 0 ? context->joint_mode : 0U;
        neighbors->fsc_mode[index] = context != 0 ? context->fsc_mode : 0U;
        neighbors->mrl_index[index] = context != 0 ? context->mrl_index : 0U;
        neighbors->multi_line_mrl[index] =
            context != 0 ? context->multi_line_mrl : 0U;
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
    if ((size_t)leaf->column + width > state->above_count ||
        left_index + height > state->left_count) {
        return AV2H_OVERFLOW;
    }
    context.present = 1U;
    context.use_intrabc = mode->use_intrabc;
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
    if (mode->use_intrabc) state->has_intrabc = 1U;
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hFirstLeafMode *mode) {
    Av2hLumaModeNeighbors neighbors;
    Av2hStatus status = av2h_luma_mode_neighbors_from_state(
        state, leaf, &neighbors);
    if (status != AV2H_OK) return status;
    status = av2h_luma_mode_read(decoder, cdfs, sequence, frame, leaf,
                                 &neighbors, mode);
    if (status == AV2H_OK && mode->use_intrabc && state->has_intrabc) {
        return AV2H_UNSUPPORTED;
    }
    return status;
}

Av2hStatus av2h_luma_mode_read(Av2hRangeDecoder *decoder,
                               Av2hLeafCdfs *cdfs,
                               const Av2hSequenceState *sequence,
                               const Av2hFrameState *frame,
                               const Av2hPartitionNode *leaf,
                               const Av2hLumaModeNeighbors *neighbors,
                               Av2hFirstLeafMode *mode) {
    size_t plane;
    size_t neighbor;
    size_t mode_context = 0U;
    size_t intrabc_context = 0U;
    size_t fsc_context = 0U;
    size_t mrl_context = 0U;
    size_t secondary_context = 0U;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        leaf == 0 || neighbors == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (leaf->block_size >= AV2H_BLOCK_SIZE_COUNT ||
        frame->base_q_index != 128U ||
        frame->disable_cdf_update || frame->segmentation_enabled ||
        frame->delta_q_present || frame->gdf_mode >= 2U ||
        frame->cdef_strength_count > 1U || !sequence->enable_sdp ||
        sequence->enable_intra_dip || frame->allow_screen_content_tools) {
        return AV2H_UNSUPPORTED;
    }
    for (neighbor = 0U; neighbor < 2U; ++neighbor) {
        if (!neighbors->present[neighbor]) continue;
        if (neighbors->joint_mode[neighbor] >= 61U ||
            neighbors->fsc_mode[neighbor] > 1U ||
            neighbors->mrl_index[neighbor] >= AV2H_MRL_INDEX_SYMBOLS ||
            neighbors->multi_line_mrl[neighbor] > 1U) {
            return AV2H_INVALID_ARGUMENT;
        }
        mode_context += is_directional_joint_mode(
            neighbors->joint_mode[neighbor]);
        intrabc_context += neighbors->use_intrabc[neighbor];
        fsc_context += neighbors->fsc_mode[neighbor];
        mrl_context += neighbors->mrl_index[neighbor] != 0U;
        secondary_context += neighbors->multi_line_mrl[neighbor];
    }
    mode->use_intrabc = 0U;
    mode->intrabc_mode = 0U;
    mode->skip_transform = 0U;
    mode->intrabc_drl_index = 0U;
    mode->mv_precision = 0U;
    mode->displacement_x = 0;
    mode->displacement_y = 0;
    mode->ccso_present_mask = 0U;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        mode->ccso_block_control[plane] = 0U;
    }
    if (frame->allow_intrabc && block_allows_intrabc(leaf->block_size)) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->intrabc[intrabc_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->use_intrabc = (uint8_t)value;
        if (value != 0U) {
            mode->skip_transform = (uint8_t)av2h_range_read_adaptive(
                decoder, cdfs->skip_transform[0], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            if (mode->skip_transform != 0U) return AV2H_UNSUPPORTED;
            return read_first_intrabc(
                decoder, cdfs, sequence, frame, leaf, mode);
        }
    }
    if ((leaf->row & 15U) == 0U && (leaf->column & 15U) == 0U) {
        for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            if (!frame->ccso[plane].enabled) continue;
            value = av2h_range_read_adaptive(
                decoder, cdfs->ccso[plane][0], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            mode->ccso_present_mask |= (uint8_t)(1U << plane);
            mode->ccso_block_control[plane] = (uint8_t)value;
        }
    }
    mode->before_mode_bits = av2h_range_tell(decoder);
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
    if (sequence->enable_idtx_intra) {
        if (mode->fsc_block_context >= AV2H_FSC_BLOCK_CONTEXT_COUNT) {
            return AV2H_UNSUPPORTED;
        }
        value = av2h_range_read_adaptive(
            decoder, cdfs->fsc_mode[fsc_context][mode->fsc_block_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->fsc_mode = (uint8_t)value;
        if (value != 0U && leaf->block_size != AV2H_BLOCK_4X4 &&
            leaf->block_size != AV2H_BLOCK_8X16 &&
            leaf->block_size != AV2H_BLOCK_16X4) {
            return AV2H_UNSUPPORTED;
        }
    }
    mode->after_fsc_bits = av2h_range_tell(decoder);
    mode->mrl_index = 0U;
    mode->multi_line_mrl = 0U;
    if (sequence->enable_mrls && is_directional_joint_mode(mode->joint_mode)) {
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
    return AV2H_OK;
}

Av2hStatus av2h_first_leaf_mode_read(Av2hRangeDecoder *decoder,
                                     Av2hLeafCdfs *cdfs,
                                     const Av2hSequenceState *sequence,
                                     const Av2hFrameState *frame,
                                     const Av2hPartitionNode *leaf,
                                     Av2hFirstLeafMode *mode) {
    const Av2hLumaModeNeighbors neighbors = { { 0U, 0U }, { 0U, 0U },
        { 0U, 0U }, { 0U, 0U }, { 0U, 0U }, { 0U, 0U } };
    if (leaf == 0) return AV2H_INVALID_ARGUMENT;
    if (leaf->row != 0U || leaf->column != 0U) return AV2H_UNSUPPORTED;
    return av2h_luma_mode_read(decoder, cdfs, sequence, frame, leaf,
                               &neighbors, mode);
}