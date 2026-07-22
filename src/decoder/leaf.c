#include "leaf.h"

static const Av2hBinaryCdfSpec intrabc_defaults[] = {
    { 32085U, 5U, 0U }, { 15172U, 30U, 0U }, { 4503U, 0U, 0U }
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
    Av2hStatus status;
    size_t context;
    if (cdfs == 0) return AV2H_INVALID_ARGUMENT;
    status = initialize_binary_table(cdfs->intrabc, intrabc_defaults,
                                     AV2H_INTRABC_CONTEXT_COUNT);
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
    return AV2H_OK;
}

static Av2hStatus map_first_mode(uint8_t mode_index,
                                 Av2hFirstLeafIntraMode *mode) {
    static const Av2hFirstLeafIntraMode modes[] = {
        AV2H_INTRA_DC, AV2H_INTRA_SMOOTH, AV2H_INTRA_SMOOTH_VERTICAL,
        AV2H_INTRA_SMOOTH_HORIZONTAL, AV2H_INTRA_PAETH
    };
    if (mode_index >= sizeof(modes) / sizeof(modes[0])) {
        return AV2H_UNSUPPORTED;
    }
    *mode = modes[mode_index];
    return AV2H_OK;
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

Av2hStatus av2h_first_leaf_mode_read(Av2hRangeDecoder *decoder,
                                     Av2hLeafCdfs *cdfs,
                                     const Av2hSequenceState *sequence,
                                     const Av2hFrameState *frame,
                                     const Av2hPartitionNode *leaf,
                                     Av2hFirstLeafMode *mode) {
    size_t plane;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        leaf == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (leaf->row != 0U || leaf->column != 0U ||
        leaf->block_size >= AV2H_BLOCK_SIZE_COUNT ||
        frame->base_q_index != 128U ||
        frame->disable_cdf_update || frame->segmentation_enabled ||
        frame->delta_q_present || frame->gdf_mode >= 2U ||
        frame->cdef_strength_count > 1U || !sequence->enable_sdp ||
        sequence->enable_intra_dip || frame->allow_screen_content_tools) {
        return AV2H_UNSUPPORTED;
    }
    mode->use_intrabc = 0U;
    mode->ccso_present_mask = 0U;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        mode->ccso_block_control[plane] = 0U;
    }
    if (frame->allow_intrabc && block_allows_intrabc(leaf->block_size)) {
        value = av2h_range_read_adaptive(decoder, cdfs->intrabc[0], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->use_intrabc = (uint8_t)value;
        if (value != 0U) return AV2H_UNSUPPORTED;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (!frame->ccso[plane].enabled) continue;
        value = av2h_range_read_adaptive(decoder, cdfs->ccso[plane][0], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->ccso_present_mask |= (uint8_t)(1U << plane);
        mode->ccso_block_control[plane] = (uint8_t)value;
    }
    mode->before_mode_bits = av2h_range_tell(decoder);
    value = av2h_range_read_adaptive(
        decoder, cdfs->y_mode_set, AV2H_Y_MODE_SET_SYMBOLS);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->mode_set = (uint8_t)value;
    mode->after_mode_set_bits = av2h_range_tell(decoder);
    if (value != 0U) return AV2H_UNSUPPORTED;
    value = av2h_range_read_adaptive(
        decoder, cdfs->y_mode_index[0], AV2H_Y_MODE_INDEX_SYMBOLS);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->mode_index = (uint8_t)value;
    if (value == AV2H_Y_MODE_INDEX_SYMBOLS - 1U) {
        value += av2h_range_read_adaptive(
            decoder, cdfs->y_mode_offset[0], AV2H_Y_MODE_OFFSET_SYMBOLS);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->mode_index = (uint8_t)value;
    }
    mode->after_mode_index_bits = av2h_range_tell(decoder);
    if (map_first_mode(mode->mode_index, &mode->mode) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    mode->fsc_mode = 0U;
    mode->fsc_block_context = fsc_block_context(leaf->block_size);
    if (sequence->enable_idtx_intra) {
        if (mode->fsc_block_context >= AV2H_FSC_BLOCK_CONTEXT_COUNT) {
            return AV2H_UNSUPPORTED;
        }
        value = av2h_range_read_adaptive(
            decoder, cdfs->fsc_mode[0][mode->fsc_block_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->fsc_mode = (uint8_t)value;
        if (value != 0U) return AV2H_UNSUPPORTED;
    }
    mode->after_fsc_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}