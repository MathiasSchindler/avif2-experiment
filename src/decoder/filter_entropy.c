#include "leaf.h"

#include "internal/leaf_modules.h"

#include <stddef.h>

Av2hStatus av2h_gdf_entropy_state_init(
    Av2hGdfEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *block_flags,
    size_t block_flag_capacity) {
    size_t index;
    Av2hStatus status;
    if (state == NULL || sequence == NULL || frame == NULL) {
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
    if (block_flags == NULL ||
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

Av2hStatus av2h_leaf_read_gdf_entropy(
    Av2hRangeDecoder *decoder,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hGdfEntropyState *state,
    Av2hLeafMode *mode) {
    size_t mi_rows = ((size_t)frame->height + 3U) / 4U;
    size_t mi_columns = ((size_t)frame->width + 3U) / 4U;
    size_t mi_row;
    size_t mi_column;
    mode->before_gdf_bits = av2h_range_tell(decoder);
    mode->after_gdf_bits = mode->before_gdf_bits;
    mode->gdf_flags_read = 0U;
    if (frame->gdf_mode < 2U) return AV2H_OK;
    if (state == NULL || state->mode != frame->gdf_mode ||
        state->block_flags == NULL ||
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

static Av2hStatus cdef_entropy_state_init(
    Av2hCdefEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *unit_strengths,
    size_t unit_strength_capacity,
    uint8_t preserve_controls) {
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
    if (state == NULL || sequence == NULL || frame == NULL) {
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
    if (unit_strengths == NULL ||
        unit_strength_capacity < state->unit_count) {
        return AV2H_OUT_OF_MEMORY;
    }
    state->unit_strengths = unit_strengths;
    state->unit_strength_capacity = unit_strength_capacity;
    if (preserve_controls == 0U) {
        for (index = 0U; index < state->unit_count; ++index) {
            unit_strengths[index] = AV2H_CDEF_UNTRANSMITTED;
        }
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

Av2hStatus av2h_cdef_entropy_state_init(
    Av2hCdefEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *unit_strengths,
    size_t unit_strength_capacity) {
    return cdef_entropy_state_init(
        state, sequence, frame, unit_strengths, unit_strength_capacity, 0U);
}

Av2hStatus av2h_cdef_entropy_state_init_preserving_controls(
    Av2hCdefEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *unit_strengths,
    size_t unit_strength_capacity) {
    return cdef_entropy_state_init(
        state, sequence, frame, unit_strengths, unit_strength_capacity, 1U);
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
    if (state == NULL || leaf == NULL || context == NULL ||
        unit_row >= state->unit_rows ||
        unit_column >= state->unit_columns) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (unit_column != 0U &&
        unit_column * 16U > state->tile_start_column) {
        left = state->unit_strengths[
            unit_row * state->unit_columns + unit_column - 1U];
    }
    if (unit_row != 0U && unit_row * 16U > state->tile_start_row &&
        leaf->row >= 16U &&
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

static Av2hStatus cdef_span_strength(
    Av2hCdefEntropyState *state,
    const Av2hPartitionNode *leaf,
    size_t width,
    size_t height,
    uint8_t strength) {
    size_t first_row = leaf->row / 16U;
    size_t first_column = leaf->column / 16U;
    size_t last_row = ((size_t)leaf->row + height - 1U) / 16U;
    size_t last_column = ((size_t)leaf->column + width - 1U) / 16U;
    size_t row;
    size_t column;
    if (first_row >= state->unit_rows ||
        first_column >= state->unit_columns ||
        strength >= state->strength_count) {
        return AV2H_INVALID_DATA;
    }
    if (last_row >= state->unit_rows) last_row = state->unit_rows - 1U;
    if (last_column >= state->unit_columns) {
        last_column = state->unit_columns - 1U;
    }
    for (row = first_row; row <= last_row; ++row) {
        for (column = first_column; column <= last_column; ++column) {
            size_t index = row * state->unit_columns + column;
            uint8_t current = state->unit_strengths[index];
            if (current != AV2H_CDEF_UNTRANSMITTED &&
                current != strength) {
                return AV2H_INVALID_DATA;
            }
            state->unit_strengths[index] = strength;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_leaf_read_cdef_entropy(
    Av2hRangeDecoder *decoder,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hCdefEntropyState *state,
    Av2hLeafMode *mode) {
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
    if (state == NULL ||
        state->strength_count != frame->cdef_strength_count ||
        (state->superblock_mi != 16U && state->superblock_mi != 32U) ||
        state->unit_columns != expected_columns ||
        state->unit_rows != expected_rows ||
        state->unit_count != expected_count ||
        state->unit_strengths == NULL ||
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
        return cdef_span_strength(
            state, leaf, width, height, mode->cdef_strength_index);
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
    status = cdef_span_strength(
        state, leaf, width, height, (uint8_t)value);
    if (status != AV2H_OK) return status;
    mode->cdef_strength_index = (uint8_t)value;
    mode->cdef_strengths_read = 1U;
    mode->after_cdef_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

static Av2hStatus ccso_entropy_state_init(
    Av2hCcsoEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *const unit_controls[AV2H_FRAME_PLANE_COUNT],
    size_t unit_control_capacity,
    uint8_t preserve_controls) {
    size_t plane;
    size_t index;
    if (state == NULL || sequence == NULL || frame == NULL) {
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
    state->unit_size = frame->ccso_unit_size != 0U ?
        frame->ccso_unit_size :
        (sequence->ccso_unit_matches_superblock_size ?
            sequence->superblock_size : AV2H_CCSO_MAX_UNIT_SIZE);
    if (state->unit_size != 64U && state->unit_size != 128U &&
        state->unit_size != 256U) {
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
    if (unit_controls == NULL) {
        return AV2H_OUT_OF_MEMORY;
    }
    if (unit_control_capacity < state->unit_count) {
        return AV2H_OUT_OF_MEMORY;
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (unit_controls[plane] == NULL) {
            return AV2H_OUT_OF_MEMORY;
        }
        state->unit_controls[plane] = unit_controls[plane];
        if (preserve_controls == 0U) {
            for (index = 0U; index < state->unit_count; ++index) {
                unit_controls[plane][index] = AV2H_CCSO_UNTRANSMITTED;
            }
        }
    }
    state->unit_control_capacity = unit_control_capacity;
    return AV2H_OK;
}

Av2hStatus av2h_ccso_entropy_state_init(
    Av2hCcsoEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *const unit_controls[AV2H_FRAME_PLANE_COUNT],
    size_t unit_control_capacity) {
    return ccso_entropy_state_init(
        state, sequence, frame, unit_controls, unit_control_capacity, 0U);
}

Av2hStatus av2h_ccso_entropy_state_init_preserving_controls(
    Av2hCcsoEntropyState *state,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    uint8_t *const unit_controls[AV2H_FRAME_PLANE_COUNT],
    size_t unit_control_capacity) {
    return ccso_entropy_state_init(
        state, sequence, frame, unit_controls, unit_control_capacity, 1U);
}

static Av2hStatus ccso_context(
    const Av2hCcsoEntropyState *state,
    const Av2hLumaModeNeighbors *neighbors,
    size_t plane,
    size_t unit_row,
    size_t unit_column,
    uint8_t above_available_at_leaf,
    size_t *context) {
    uint8_t left = AV2H_CCSO_UNTRANSMITTED;
    uint8_t above = AV2H_CCSO_UNTRANSMITTED;
    uint8_t left_available;
    uint8_t above_available;
    if (state == NULL || context == NULL ||
        plane >= AV2H_FRAME_PLANE_COUNT ||
        unit_row >= state->unit_rows ||
        unit_column >= state->unit_columns ||
        state->unit_controls[plane] == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (unit_column != 0U && neighbors != NULL &&
        neighbors->ccso_present[0] != 0U &&
        unit_column * state->unit_size_mi > state->tile_start_column) {
        left = state->unit_controls[plane][
            unit_row * state->unit_columns + unit_column - 1U];
    }
    if (unit_row != 0U && neighbors != NULL &&
        neighbors->ccso_present[1] != 0U &&
        unit_row * state->unit_size_mi > state->tile_start_row &&
        above_available_at_leaf) {
        above = state->unit_controls[plane][
            (unit_row - 1U) * state->unit_columns + unit_column];
    }
    left_available = left != AV2H_CCSO_UNTRANSMITTED;
    above_available = above != AV2H_CCSO_UNTRANSMITTED;
    if ((left_available && left > 1U) ||
        (above_available && above > 1U)) {
        return AV2H_INVALID_DATA;
    }
    if (left_available && above_available && neighbors != NULL &&
        (neighbors->ccso_owner_row[0] / state->unit_size_mi) ==
            (neighbors->ccso_owner_row[1] / state->unit_size_mi) &&
        (neighbors->ccso_owner_column[0] / state->unit_size_mi) ==
            (neighbors->ccso_owner_column[1] / state->unit_size_mi)) {
        *context = left != 0U ? 2U : 0U;
    } else if (left_available && above_available) {
        *context = left && above ? 3U : (size_t)(left || above);
    } else if (left_available || above_available) {
        uint8_t neighbor = left_available ? left : above;
        *context = neighbor ? 2U : 0U;
    } else {
        *context = 0U;
    }
    return AV2H_OK;
}

Av2hStatus av2h_leaf_read_ccso_entropy(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLumaModeNeighbors *neighbors,
    Av2hCcsoEntropyState *state,
    Av2hLeafMode *mode) {
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
    if (state == NULL || state->enabled_mask == 0U) return AV2H_OK;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (frame->ccso[plane].enabled) {
            expected_mask |= (uint8_t)(1U << plane);
        }
    }
    expected_size = frame->ccso_unit_size != 0U ?
        frame->ccso_unit_size :
        (sequence->ccso_unit_matches_superblock_size ?
            sequence->superblock_size : AV2H_CCSO_MAX_UNIT_SIZE);
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
        size_t last_unit_row;
        size_t last_unit_column;
        size_t span_row;
        uint32_t value;
        if ((state->enabled_mask & (uint8_t)(1U << plane)) == 0U) {
            continue;
        }
        if (state->unit_controls[plane] == NULL ||
            state->unit_controls[plane][unit_index] !=
                AV2H_CCSO_UNTRANSMITTED) {
            return AV2H_INVALID_DATA;
        }
        status = ccso_context(
            state, neighbors, plane, unit_row, unit_column,
            (uint8_t)(leaf->row % (sequence->superblock_size / 4U) != 0U),
            &context);
        if (status != AV2H_OK) return status;
        value = av2h_range_read_adaptive(
            decoder, cdfs->ccso[plane][context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        last_unit_row = ((size_t)leaf->row + height - 1U) /
            state->unit_size_mi;
        last_unit_column = ((size_t)leaf->column + width - 1U) /
            state->unit_size_mi;
        if (last_unit_row >= state->unit_rows) {
            last_unit_row = state->unit_rows - 1U;
        }
        if (last_unit_column >= state->unit_columns) {
            last_unit_column = state->unit_columns - 1U;
        }
        for (span_row = unit_row; span_row <= last_unit_row; ++span_row) {
            size_t span_column;
            for (span_column = unit_column;
                 span_column <= last_unit_column; ++span_column) {
                size_t span_index =
                    span_row * state->unit_columns + span_column;
                uint8_t current = state->unit_controls[plane][span_index];
                if (current != AV2H_CCSO_UNTRANSMITTED && current != value) {
                    return AV2H_INVALID_DATA;
                }
                state->unit_controls[plane][span_index] = (uint8_t)value;
            }
        }
        mode->ccso_present_mask |= (uint8_t)(1U << plane);
        mode->ccso_block_control[plane] = (uint8_t)value;
        ++mode->ccso_controls_read;
    }
    mode->after_ccso_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}

Av2hStatus av2h_leaf_read_first_ccso_without_state(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hLeafMode *mode) {
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