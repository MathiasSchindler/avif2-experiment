#include "leaf.h"
#include "internal/decode_diagnostics.h"
#include "internal/leaf_modules.h"

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static Av2hLeafDiagnostics leaf_diagnostics;
#endif

const Av2hLeafDiagnostics *av2h_leaf_diagnostics_get(void) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return &leaf_diagnostics;
#else
    return 0;
#endif
}

void av2h_leaf_diagnostics_set_luma_stage(uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    leaf_diagnostics.luma_mode_stage = value;
#else
    (void)value;
#endif
}

void av2h_leaf_diagnostics_set_chroma_stage(uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    leaf_diagnostics.chroma_mode_stage = value;
#else
    (void)value;
#endif
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
        colocated_luma_delta, block_size, cfl_context, 1U, 0U, mode);
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
    uint8_t lossless,
    Av2hFirstChromaMode *mode) {
    uint8_t mode_context;
    uint8_t mhccp_allowed = cfl_allowed;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_leaf_diagnostics_set_chroma_stage(1U);
    if (cfl_context >= AV2H_CFL_CONTEXT_COUNT ||
        colocated_luma_mode > AV2H_INTRA_PAETH || cfl_allowed > 1U) {
        return AV2H_UNSUPPORTED;
    }
    cfl_allowed = (uint8_t)(cfl_allowed && sequence->enable_cfl_intra);
    if (block_size < AV2H_BLOCK_SIZE_COUNT) {
        size_t block_width_mi;
        size_t block_height_mi;
        size_t max_dimension_mi =
            sequence->info.chroma_format_idc == 2U ? 8U : 16U;
        size_t max_cfl_dimension_mi =
            sequence->info.chroma_format_idc == 2U ? 16U : 32U;
        size_t lossless_dimension_mi =
            sequence->info.chroma_format_idc == 2U ? 1U : 2U;
        uint8_t lossless_chroma_4x4;
        uint8_t mhccp_too_small;
        if (av2h_block_dimensions_mi(
                block_size, &block_width_mi, &block_height_mi) != AV2H_OK) {
            return AV2H_UNSUPPORTED;
        }
        lossless_chroma_4x4 = (uint8_t)(
            block_width_mi == lossless_dimension_mi &&
            block_height_mi == lossless_dimension_mi);
        mhccp_too_small = lossless ? (uint8_t)!lossless_chroma_4x4 :
            sequence->info.chroma_format_idc == 2U ?
                (uint8_t)(block_width_mi == 1U && block_height_mi == 1U) :
                (uint8_t)(block_width_mi <= 2U && block_height_mi <= 2U);
        cfl_allowed = (uint8_t)(cfl_allowed &&
            (!lossless || lossless_chroma_4x4) &&
            block_width_mi <= max_cfl_dimension_mi &&
            block_height_mi <= max_cfl_dimension_mi);
        mhccp_allowed = (uint8_t)(mhccp_allowed &&
            !mhccp_too_small &&
            block_width_mi <= max_dimension_mi &&
            block_height_mi <= max_dimension_mi);
    }
    mode->before_cfl_bits = av2h_range_tell(decoder);
    value = 0U;
    if (cfl_allowed) {
        av2h_leaf_diagnostics_set_chroma_stage(2U);
        value = av2h_range_read_adaptive(
            decoder, cdfs->is_cfl[cfl_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    mode->is_cfl = (uint8_t)value;
    mode->after_cfl_bits = av2h_range_tell(decoder);
    mode->uses_cfl = 0U;
    mode->edge_ibp = sequence->enable_ibp;
    mode->cfl_index = 0U;
    mode->cfl_mhccp_switch = 0U;
    mode->mhccp_direction = 0U;
    mode->cfl_downsample_filter_index =
        sequence->cfl_downsample_filter_index;
    mode->chroma_format_idc = sequence->info.chroma_format_idc;
    mode->superblock_size = sequence->superblock_size;
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
        if (sequence->enable_mhccp && mhccp_allowed) {
            av2h_leaf_diagnostics_set_chroma_stage(3U);
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
                av2h_leaf_diagnostics_set_chroma_stage(4U);
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
        av2h_leaf_diagnostics_set_chroma_stage(5U);
        value = av2h_range_read_adaptive(decoder, cdfs->cfl_index, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->cfl_index = (uint8_t)value;
        mode->after_cfl_index_bits = av2h_range_tell(decoder);
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
        av2h_leaf_diagnostics_set_chroma_stage(6U);
        value = av2h_range_read_adaptive(decoder, cdfs->cfl_sign, 8U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->cfl_alpha_signs = (uint8_t)value;
        {
            uint8_t sign_u = (uint8_t)(((value + 1U) * 11U) >> 5U);
            uint8_t sign_v = (uint8_t)(value + 1U - 3U * sign_u);
            uint8_t alpha_idx = 0U;
            av2h_leaf_diagnostics_set_chroma_stage(7U);
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
    av2h_leaf_diagnostics_set_chroma_stage(8U);
    value = av2h_range_read_adaptive(
        decoder, cdfs->uv_mode[mode_context], AV2H_UV_MODE_SYMBOLS);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (value == AV2H_UV_MODE_SYMBOLS - 1U) {
        av2h_leaf_diagnostics_set_chroma_stage(9U);
        value += av2h_range_read_bypass(decoder, 3U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    mode->mode_index = (uint8_t)value;
    av2h_leaf_diagnostics_set_chroma_stage(10U);
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
    Av2hQIndex *current_q_index,
    uint8_t defer_palette_and_dip,
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
        cdef_state, ccso_state, 0, 0U, mode);
    return status;
}

Av2hStatus av2h_luma_mode_read_with_state_and_delta_q(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hGdfEntropyState *gdf_state,
    Av2hCdefEntropyState *cdef_state,
    Av2hCcsoEntropyState *ccso_state,
    Av2hQIndex *current_q_index,
    uint8_t defer_palette_and_dip,
    Av2hFirstLeafMode *mode) {
    Av2hLumaModeNeighbors neighbors;
    Av2hStatus status = av2h_luma_mode_neighbors_from_state(
        state, leaf, &neighbors);
    if (status != AV2H_OK) return status;
    return luma_mode_read_internal(
        decoder, cdfs, sequence, frame, leaf, &neighbors, state, gdf_state,
        cdef_state, ccso_state, current_q_index, defer_palette_and_dip,
        mode);
}

static Av2hStatus read_delta_q(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    uint8_t skip_transform,
    Av2hQIndex *current_q_index) {
    Av2hBlockSize superblock_size;
    uint16_t superblock_mi;
    uint32_t magnitude;
    int q_index;
    if (frame->delta_q_present == 0U) return AV2H_OK;
    if (current_q_index == 0 || frame->delta_q_res == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    superblock_mi = (uint16_t)(sequence->superblock_size / 4U);
    superblock_size = sequence->superblock_size == 128U ?
        AV2H_BLOCK_128X128 : AV2H_BLOCK_64X64;
    if (superblock_mi == 0U ||
        leaf->row % superblock_mi != 0U ||
        leaf->column % superblock_mi != 0U ||
        (leaf->block_size == superblock_size && skip_transform != 0U)) {
        return AV2H_OK;
    }
    magnitude = av2h_range_read_adaptive(
        decoder, cdfs->delta_q, AV2H_DELTA_Q_SYMBOLS);
    if (magnitude >= 7U) {
        uint32_t remainder_bits =
            av2h_range_read_bypass(decoder, 3U) + 1U;
        magnitude = av2h_range_read_bypass(decoder, remainder_bits) +
            ((uint32_t)1U << remainder_bits) + 5U;
    }
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    q_index = (int)*current_q_index;
    if (magnitude != 0U && av2h_range_read_bypass(decoder, 1U) != 0U) {
        q_index -= (int)magnitude * frame->delta_q_res;
    } else {
        q_index += (int)magnitude * frame->delta_q_res;
    }
    *current_q_index = (Av2hQIndex)av2h_sequence_clamp_delta_q_index(
        sequence, q_index);
    return av2h_range_status(decoder);
}

static const Av2hLumaModeContext *segment_spatial_context(
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
        state->spatial + index * state->spatial_stride_bytes);
}

static uint8_t negative_deinterleave(
    uint8_t difference, uint8_t reference, uint8_t maximum) {
    if (reference == 0U) return difference;
    if (reference >= maximum - 1U) {
        return (uint8_t)(maximum - difference - 1U);
    }
    if ((uint16_t)2U * reference < maximum) {
        if (difference <= (uint16_t)2U * reference) {
            if ((difference & 1U) != 0U) {
                return (uint8_t)(
                    reference + ((difference + 1U) >> 1U));
            }
            return (uint8_t)(reference - (difference >> 1U));
        }
        return difference;
    }
    if (difference <= (uint16_t)2U * (maximum - reference - 1U)) {
        if ((difference & 1U) != 0U) {
            return (uint8_t)(reference + ((difference + 1U) >> 1U));
        }
        return (uint8_t)(reference - (difference >> 1U));
    }
    return (uint8_t)(maximum - difference - 1U);
}

static Av2hStatus read_segment_id(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    Av2hFirstLeafMode *mode) {
    const Av2hLumaModeContext *upper_left = segment_spatial_context(
        state, (int32_t)leaf->row - 1, (int32_t)leaf->column - 1);
    const Av2hLumaModeContext *above = segment_spatial_context(
        state, (int32_t)leaf->row - 1, leaf->column);
    const Av2hLumaModeContext *left = segment_spatial_context(
        state, leaf->row, (int32_t)leaf->column - 1);
    int upper_left_id = upper_left != 0 && upper_left->present ?
        upper_left->segment_id : -1;
    int above_id = above != 0 && above->present ? above->segment_id : -1;
    int left_id = left != 0 && left->present ? left->segment_id : -1;
    uint8_t context;
    uint8_t predictor;
    uint8_t coded_id;
    uint8_t maximum = (uint8_t)(frame->seg_last_active_id + 1U);
    if (frame->segmentation_enabled == 0U) {
        mode->segment_id = 0U;
        return AV2H_OK;
    }
    context = upper_left_id < 0 ? 0U :
        upper_left_id == above_id && upper_left_id == left_id ? 2U :
        upper_left_id == above_id || upper_left_id == left_id ||
            above_id == left_id ? 1U : 0U;
    predictor = above_id < 0 ? (uint8_t)(left_id < 0 ? 0 : left_id) :
        left_id < 0 ? (uint8_t)above_id :
        upper_left_id == above_id ? (uint8_t)above_id : (uint8_t)left_id;
    coded_id = (uint8_t)av2h_range_read_adaptive(
        decoder, cdfs->segment_id[context], AV2H_SEGMENT_COUNT);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->segment_id = negative_deinterleave(coded_id, predictor, maximum);
    return mode->segment_id <= frame->seg_last_active_id ?
        AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hQIndex segment_q_index(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hQIndex base_q_index,
    uint8_t segment_id) {
    int q_index;
    int maximum;
    if (frame->segmentation_enabled == 0U ||
        (frame->seg_feature_mask[segment_id] & 1U) == 0U) {
        return base_q_index;
    }
    q_index = (int)base_q_index + frame->seg_alt_q[segment_id];
    maximum = (int)av2h_sequence_max_q_index(sequence);
    if (q_index < 0) return 0U;
    if (q_index > maximum) return (Av2hQIndex)maximum;
    return (Av2hQIndex)q_index;
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
    Av2hQIndex *current_q_index,
    uint8_t defer_palette_and_dip,
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
        frame->disable_cdf_update ||
        (!sequence->enable_sdp && !av2h_sequence_is_monochrome(sequence)) ||
        (frame->gdf_mode >= 2U && gdf_state == 0) ||
        (frame->cdef_strength_count > 1U && cdef_state == 0)) {
        return AV2H_UNSUPPORTED;
    }
    mode->enable_ibp = sequence->enable_ibp;
    mode->left_smooth = neighbors->edge_smooth[0];
    mode->above_smooth = neighbors->edge_smooth[1];
    mode->superblock_size = sequence->superblock_size;
    has_spatial_neighbors = neighbors->spatial_valid;
    for (neighbor = 0U; neighbor < 2U; ++neighbor) {
        uint8_t skip_transform = neighbors->line_valid ?
            neighbors->line_skip_transform[neighbor] :
            neighbors->skip_transform[neighbor];
        if (neighbors->present[neighbor]) {
            if (neighbors->joint_mode[neighbor] >= 61U) {
                return AV2H_INVALID_ARGUMENT;
            }
            mode_context += is_directional_joint_mode(
                neighbors->joint_mode[neighbor]);
        }
        if (neighbors->line_valid && neighbors->line_present[neighbor]) {
            skip_transform_context += skip_transform;
        } else if (!neighbors->line_valid) {
            skip_transform_context += skip_transform;
        }
    }
    for (neighbor = 0U; neighbor < 2U; ++neighbor) {
        uint8_t present = has_spatial_neighbors ?
            neighbors->spatial_present[neighbor] : neighbors->present[neighbor];
        uint8_t use_intrabc = has_spatial_neighbors ?
            neighbors->spatial_use_intrabc[neighbor] :
            neighbors->use_intrabc[neighbor];
        uint8_t spatial_mrl = has_spatial_neighbors ?
            neighbors->spatial_mrl_index[neighbor] :
            neighbors->mrl_index[neighbor];
        uint8_t spatial_multi = has_spatial_neighbors ?
            neighbors->spatial_multi_line_mrl[neighbor] :
            neighbors->multi_line_mrl[neighbor];
        if (!present) continue;
           if ((has_spatial_neighbors &&
               neighbors->spatial_fsc_mode[neighbor] > 1U) ||
            spatial_mrl >= AV2H_MRL_INDEX_SYMBOLS ||
            spatial_multi > 1U) {
            return AV2H_INVALID_ARGUMENT;
        }
        intrabc_context += use_intrabc;
        if (has_spatial_neighbors &&
            neighbors->spatial_present[neighbor]) {
            fsc_context += neighbors->spatial_fsc_mode[neighbor];
        }
        mrl_context += spatial_mrl != 0U;
        secondary_context += spatial_multi;
    }
    av2h_leaf_diagnostics_set_luma_stage(1U);
    mode->use_intrabc = 0U;
    mode->segment_id = 0U;
    mode->q_index = frame->base_q_index;
    mode->use_dpcm_y = 0U;
    mode->dpcm_mode_y = 0U;
    mode->lossless_tx_size_large = 0U;
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
    mode->use_intra_dip = 0U;
    mode->intra_dip_mode = 0U;
    mode->before_palette_bits = mode->before_gdf_bits;
    mode->after_palette_bits = mode->before_gdf_bits;
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
        av2h_leaf_diagnostics_set_luma_stage(2U);
        if (value != 0U) {
            Av2hStatus status;
            size_t block_width_mi;
            size_t block_height_mi;
            if (av2h_block_dimensions_mi(
                    leaf->block_size, &block_width_mi,
                    &block_height_mi) != AV2H_OK) {
                return AV2H_UNSUPPORTED;
            }
            mode->block_origin_row = leaf->row;
            mode->block_origin_column = leaf->column;
            mode->block_width = (uint16_t)(block_width_mi * 4U);
            mode->block_height = (uint16_t)(block_height_mi * 4U);
            if (skip_transform_context >=
                AV2H_SKIP_TRANSFORM_CONTEXT_COUNT) {
                return AV2H_INVALID_DATA;
            }
            mode->skip_transform = (uint8_t)av2h_range_read_adaptive(
                decoder, cdfs->skip_transform[skip_transform_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            status = read_segment_id(
                decoder, cdfs, frame, leaf, state, mode);
            if (status != AV2H_OK) return status;
            status = av2h_leaf_read_gdf_entropy(
                decoder, frame, leaf, gdf_state, mode);
            if (status != AV2H_OK) return status;
            status = av2h_leaf_read_cdef_entropy(
                decoder, frame, leaf, cdef_state, mode);
            if (status != AV2H_OK) return status;
            if (ccso_state != 0 && ccso_state->enabled_mask != 0U) {
                status = av2h_leaf_read_ccso_entropy(
                    decoder, cdfs, sequence, frame, leaf,
                    ccso_state, mode);
            } else {
                status = av2h_leaf_read_first_ccso_without_state(
                    decoder, cdfs, sequence, frame, leaf, mode);
            }
            if (status != AV2H_OK) return status;
            status = read_delta_q(
                decoder, cdfs, sequence, frame, leaf,
                mode->skip_transform, current_q_index);
            if (status != AV2H_OK) return status;
            mode->q_index = segment_q_index(
                sequence, frame,
                current_q_index != 0 ? *current_q_index : frame->base_q_index,
                mode->segment_id);
            if (frame->segmentation_enabled != 0U && mode->q_index == 0U) {
                return AV2H_UNSUPPORTED;
            }
            av2h_leaf_diagnostics_set_luma_stage(3U);
            status = av2h_leaf_read_first_intrabc(
                decoder, cdfs, sequence, frame, leaf, state, neighbors, mode);
            return status;
        }
    }
    {
        Av2hStatus status = read_segment_id(
            decoder, cdfs, frame, leaf, state, mode);
        if (status != AV2H_OK) return status;
    }
    {
        Av2hStatus status = av2h_leaf_read_gdf_entropy(
            decoder, frame, leaf, gdf_state, mode);
        if (status != AV2H_OK) return status;
    }
    {
        Av2hStatus status = av2h_leaf_read_cdef_entropy(
            decoder, frame, leaf, cdef_state, mode);
        if (status != AV2H_OK) return status;
    }
    if (ccso_state != 0 && ccso_state->enabled_mask != 0U) {
        Av2hStatus status = av2h_leaf_read_ccso_entropy(
            decoder, cdfs, sequence, frame, leaf, ccso_state, mode);
        if (status != AV2H_OK) return status;
    } else {
        Av2hStatus status = av2h_leaf_read_first_ccso_without_state(
            decoder, cdfs, sequence, frame, leaf, mode);
        if (status != AV2H_OK) return status;
    }
    {
        Av2hStatus status = read_delta_q(
            decoder, cdfs, sequence, frame, leaf,
            mode->skip_transform, current_q_index);
        if (status != AV2H_OK) return status;
    }
    mode->q_index = segment_q_index(
        sequence, frame,
        current_q_index != 0 ? *current_q_index : frame->base_q_index,
        mode->segment_id);
    if (frame->segmentation_enabled != 0U && mode->q_index == 0U) {
        return AV2H_UNSUPPORTED;
    }
    mode->before_mode_bits = av2h_range_tell(decoder);
    if (frame->base_q_index == 0U) {
        mode->use_dpcm_y = (uint8_t)av2h_range_read_adaptive(
            decoder, cdfs->dpcm, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
    }
    if (mode->use_dpcm_y != 0U) {
        mode->dpcm_mode_y = (uint8_t)av2h_range_read_adaptive(
            decoder, cdfs->dpcm_vert_horz, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->mode_set = 0U;
        mode->mode_index = mode->dpcm_mode_y != 0U ? 6U : 1U;
        mode->joint_mode = mode->dpcm_mode_y != 0U ? 50U : 22U;
        mode->mode = mode->dpcm_mode_y != 0U ?
            AV2H_INTRA_HORIZONTAL : AV2H_INTRA_VERTICAL;
        mode->angle_delta = 0;
        mode->after_mode_set_bits = av2h_range_tell(decoder);
        mode->after_mode_index_bits = mode->after_mode_set_bits;
    } else {
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
        av2h_leaf_diagnostics_set_luma_stage(4U);
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
        if (value != 0U) {
            size_t fsc_width;
            size_t fsc_height;
            if (av2h_block_dimensions_mi(
                    leaf->block_size, &fsc_width, &fsc_height) != AV2H_OK ||
                fsc_width > 8U || fsc_height > 8U) {
                return AV2H_UNSUPPORTED;
            }
        }
    }
    mode->after_fsc_bits = av2h_range_tell(decoder);
    av2h_leaf_diagnostics_set_luma_stage(5U);
    mode->mrl_index = 0U;
    mode->multi_line_mrl = 0U;
    if (mode->use_dpcm_y == 0U && sequence->enable_mrls &&
        is_directional_joint_mode(mode->joint_mode)) {
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
    av2h_leaf_diagnostics_set_luma_stage(6U);
    if (defer_palette_and_dip) {
        mode->palette_mode = 0U;
        mode->palette_size = 0U;
        mode->before_palette_bits = mode->after_mrl_bits;
        mode->after_palette_bits = mode->before_palette_bits;
        mode->after_palette_colors_bits = mode->before_palette_bits;
        mode->after_palette_map_bits = mode->before_palette_bits;
        return AV2H_OK;
    }
    return av2h_luma_mode_read_palette_and_dip(
        decoder, cdfs, sequence, frame, leaf, neighbors, mode);
    return AV2H_OK;
}

Av2hStatus av2h_luma_mode_read_palette_and_dip(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeNeighbors *neighbors,
    Av2hFirstLeafMode *mode) {
    uint32_t value;
    size_t neighbor;
    uint8_t has_spatial_neighbors;
    if (decoder == 0 || cdfs == 0 || sequence == 0 || frame == 0 ||
        leaf == 0 || neighbors == 0 || mode == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    has_spatial_neighbors = neighbors->spatial_valid;
    mode->palette_mode = 0U;
    mode->palette_size = 0U;
    mode->before_palette_bits = mode->after_mrl_bits;
    mode->after_palette_bits = mode->before_palette_bits;
    mode->after_palette_colors_bits = mode->before_palette_bits;
    mode->after_palette_map_bits = mode->before_palette_bits;
    if (frame->allow_screen_content_tools &&
        av2h_leaf_block_allows_palette(leaf->block_size) &&
        mode->mode == AV2H_INTRA_DC) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->palette_y_mode, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->palette_mode = (uint8_t)value;
        av2h_leaf_diagnostics_set_luma_stage(7U);
        mode->after_palette_bits = av2h_range_tell(decoder);
        if (value != 0U) {
            Av2hStatus status;
            value = av2h_range_read_adaptive(
                decoder, cdfs->palette_y_size, 7U) + 2U;
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            mode->palette_size = (uint8_t)value;
            status = av2h_leaf_palette_read_colors(
                decoder, sequence, neighbors, mode);
            if (status != AV2H_OK) return status;
            mode->after_palette_colors_bits = av2h_range_tell(decoder);
        }
    }
    if (sequence->enable_intra_dip != 0U &&
        mode->mode == AV2H_INTRA_DC && mode->mrl_index == 0U &&
        mode->palette_size == 0U) {
        size_t width_mi;
        size_t height_mi;
        Av2hStatus status = av2h_block_dimensions_mi(
            leaf->block_size, &width_mi, &height_mi);
        if (status != AV2H_OK) return status;
        if (width_mi > 1U && height_mi > 1U &&
            width_mi * height_mi >= 8U) {
            size_t context = 0U;
            for (neighbor = 0U; neighbor < 2U; ++neighbor) {
                uint8_t present = has_spatial_neighbors ?
                    neighbors->spatial_present[neighbor] :
                    neighbors->present[neighbor];
                uint8_t use_intra_dip = has_spatial_neighbors ?
                    neighbors->spatial_use_intra_dip[neighbor] :
                    neighbors->use_intra_dip[neighbor];
                if (present != 0U) {
                    context += use_intra_dip != 0U;
                }
            }
            if (context >= AV2H_INTRA_DIP_CONTEXTS) {
                return AV2H_INVALID_DATA;
            }
            mode->use_intra_dip = (uint8_t)av2h_range_read_adaptive(
                decoder, cdfs->intra_dip[context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            if (mode->use_intra_dip != 0U) {
                mode->intra_dip_mode = (uint8_t)(
                    av2h_range_read_bypass(decoder, 1U) << 4U);
                mode->intra_dip_mode = (uint8_t)(
                    mode->intra_dip_mode + av2h_range_read_adaptive(
                        decoder, cdfs->intra_dip_mode,
                        AV2H_INTRA_DIP_MODES));
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
            }
        }
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
        decoder, cdfs, sequence, frame, leaf, neighbors, 0, 0, 0, 0, 0, 0U,
        mode);
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