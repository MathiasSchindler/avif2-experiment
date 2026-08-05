#include "leaf.h"
#include "internal/decode_diagnostics.h"

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

Av2hStatus av2h_leaf_read_first_intrabc(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeContextState *state,
    const Av2hLumaModeNeighbors *neighbors,
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
    av2h_leaf_diagnostics_set_luma_stage(20U);
    mode->before_mode_bits = av2h_range_tell(decoder);
    mode->intrabc_mode = (uint8_t)av2h_range_read_adaptive(
        decoder, cdfs->intrabc_mode, 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    mode->after_mode_set_bits = av2h_range_tell(decoder);
    av2h_leaf_diagnostics_set_luma_stage(21U);
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
    av2h_leaf_diagnostics_set_luma_stage(22U);
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
    av2h_leaf_diagnostics_set_luma_stage(23U);
    if (mode->intrabc_mode == 0U) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->intrabc_shell_set, 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (mode->mv_precision != 3U && mode->mv_precision != 5U) {
            av2h_leaf_diagnostics_set_luma_stage(
                value == 0U ? 231U : 233U);
            return AV2H_UNSUPPORTED;
        }
        av2h_leaf_diagnostics_set_luma_stage(24U);
        mode->after_mrl_bits = av2h_range_tell(decoder);
        if (value != 0U) {
            shell_class = (mode->mv_precision == 5U ? 8U : 7U) +
                av2h_range_read_adaptive(
                decoder,
                mode->mv_precision == 5U ?
                    cdfs->intrabc_qtr_shell_class_set1 :
                    cdfs->intrabc_shell_class_set1,
                mode->mv_precision == 5U ?
                    AV2H_INTRABC_QTR_SHELL_CLASS_SET1_SYMBOLS :
                    AV2H_INTRABC_SHELL_CLASS_SET1_SYMBOLS);
        } else {
            shell_class = av2h_range_read_adaptive(
                decoder,
                mode->mv_precision == 5U ? cdfs->intrabc_qtr_shell_class :
                                           cdfs->intrabc_shell_class,
                mode->mv_precision == 5U ?
                    AV2H_INTRABC_QTR_SHELL_CLASS_SYMBOLS :
                    AV2H_INTRABC_SHELL_CLASS_SYMBOLS);
        }
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (shell_class < AV2H_INTRABC_LOW_CLASS_COUNT) {
            shell_offset = av2h_range_read_adaptive(
                decoder, cdfs->intrabc_shell_offset[shell_class], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
        } else if (shell_class == 2U) {
            shell_offset = 0U;
            for (index = 0U; index < 3U; ++index) {
                value = index == 0U ? av2h_range_read_adaptive(
                    decoder, cdfs->intrabc_shell_offset_class2, 2U) :
                    av2h_range_read_bypass(decoder, 1U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                shell_offset = index + value;
                if (value == 0U) break;
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
        difference_x <<= mode->mv_precision == 5U ? 1U : 3U;
        difference_y <<= mode->mv_precision == 5U ? 1U : 3U;
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
    mode->morph_pred = 0U;
    if (sequence->enable_bawp && frame->allow_screen_content_tools) {
        uint8_t morph_context = 0U;
        for (index = 0U; index < 2U; ++index) {
            if (neighbors->spatial_present[index] &&
                neighbors->spatial_use_intrabc[index] &&
                neighbors->spatial_morph_pred[index]) {
                ++morph_context;
            }
        }
        value = av2h_range_read_adaptive(
            decoder, cdfs->morph_pred[morph_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        mode->morph_pred = (uint8_t)value;
    }
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