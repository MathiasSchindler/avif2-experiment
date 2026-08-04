#include "leaf.h"

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
        const Av2hLumaModeContext *left = &state->left[left_index];
        size_t bottom_row = (size_t)leaf->row + height - 1U;
        candidates[2] = left;
        selected[0] = left;
        if (state->spatial != 0 && state->spatial_rows != 0U &&
            bottom_row >= state->spatial_rows) {
            selected[0] = 0;
        } else if (state->spatial_rows == 0U ||
                   bottom_row < state->spatial_rows) {
            const Av2hLumaModeContext *bottom_left =
                &state->left[bottom_row % state->left_count];
            candidates[0] = bottom_left;
            selected[0] = bottom_left;
        }
    }
    if (leaf->row != 0U) {
        const Av2hLumaModeContext *above = &state->above[leaf->column];
        candidates[3] = above;
        if (state->superblock_mi != 0U &&
            state->spatial_rows <= state->superblock_mi) {
            selected[1] = above;
        }
        if ((size_t)leaf->column + width - 1U < state->above_count) {
            const Av2hLumaModeContext *above_right =
                &state->above[leaf->column + width - 1U];
            candidates[1] = above_right;
            selected[1] = above_right;
        }
    }
    for (index = 0U; index < 4U && spatial_count < 2U; ++index) {
        if (!not_at_superblock_top && (index == 1U || index == 3U)) {
            continue;
        }
        if (candidates[index] != 0 && candidates[index]->present) {
            spatial[spatial_count++] = candidates[index];
        }
    }
    neighbors->line_valid = state->superblock_mi != 0U;
    {
        size_t line_count = 0U;
        for (index = 0U; index < 4U && line_count < 2U; ++index) {
            if (candidates[index] != 0 && candidates[index]->present) {
                neighbors->line_present[line_count] = 1U;
                neighbors->line_skip_transform[line_count] =
                    candidates[index]->skip_transform;
                ++line_count;
            }
        }
    }
    neighbors->spatial_valid = state->superblock_mi != 0U;
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
        neighbors->morph_pred[index] =
            context != 0 ? context->morph_pred : 0U;
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
        neighbors->spatial_morph_pred[index] =
            spatial_context != 0 ? spatial_context->morph_pred : 0U;
    }
    neighbors->edge_smooth[0] = (uint8_t)(
        candidates[2] != 0 && candidates[2]->present &&
        candidates[2]->joint_mode >= 1U && candidates[2]->joint_mode <= 3U);
    neighbors->edge_smooth[1] = (uint8_t)(
        candidates[3] != 0 && candidates[3]->present &&
        candidates[3]->joint_mode >= 1U && candidates[3]->joint_mode <= 3U);
    neighbors->palette_size[0] = candidates[2] != 0 ?
        candidates[2]->palette_size : 0U;
    neighbors->palette_size[1] =
        candidates[3] != 0 && leaf->row % 16U != 0U ?
            candidates[3]->palette_size : 0U;
    for (index = 0U; index < AV2H_PALETTE_MAX_SIZE; ++index) {
        neighbors->palette_colors[0][index] = candidates[2] != 0 ?
            candidates[2]->palette_colors[index] : 0U;
        neighbors->palette_colors[1][index] = candidates[3] != 0 ?
            candidates[3]->palette_colors[index] : 0U;
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
    context.morph_pred = mode->morph_pred;
    context.displacement_x = mode->displacement_x;
    context.displacement_y = mode->displacement_y;
    context.skip_transform = mode->skip_transform;
    context.joint_mode = mode->joint_mode;
    context.fsc_mode = mode->fsc_mode;
    context.mrl_index = mode->mrl_index;
    context.multi_line_mrl = mode->multi_line_mrl;
    context.palette_size = mode->palette_size;
    for (index = 0U; index < AV2H_PALETTE_MAX_SIZE; ++index) {
        context.palette_colors[index] = mode->palette_colors[index];
    }
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