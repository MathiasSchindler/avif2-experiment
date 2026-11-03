#include "decoder/internal/partition_modules.h"

#include <stddef.h>

const Av2hBlockGeometry av2h_block_geometry[AV2H_BLOCK_SIZE_COUNT] = {
    { 1U, 1U, 0U, 0U }, { 1U, 2U, 0U, 0U },
    { 2U, 1U, 0U, 0U }, { 2U, 2U, 0U, 0U },
    { 2U, 4U, 1U, 1U }, { 4U, 2U, 1U, 2U },
    { 4U, 4U, 1U, 0U }, { 4U, 8U, 2U, 1U },
    { 8U, 4U, 2U, 2U }, { 8U, 8U, 2U, 3U },
    { 8U, 16U, 3U, 4U }, { 16U, 8U, 3U, 5U },
    { 16U, 16U, 3U, 6U }, { 16U, 32U, 4U, 7U },
    { 32U, 16U, 5U, 8U }, { 32U, 32U, 6U, 9U },
    { 32U, 64U, 7U, 10U }, { 64U, 32U, 8U, 11U },
    { 64U, 64U, 9U, 12U }, { 1U, 4U, 10U, 13U },
    { 4U, 1U, 11U, 14U }, { 2U, 8U, 12U, 13U },
    { 8U, 2U, 13U, 14U }, { 4U, 16U, 14U, 13U },
    { 16U, 4U, 15U, 14U }, { 1U, 8U, 0U, 0U },
    { 8U, 1U, 0U, 0U }, { 2U, 16U, 0U, 0U },
    { 16U, 2U, 0U, 0U }, { 1U, 16U, 0U, 0U },
    { 16U, 1U, 0U, 0U }
};

Av2hStatus av2h_block_dimensions_mi(Av2hBlockSize block_size,
                                     size_t *width,
                                     size_t *height) {
    if (block_size >= AV2H_BLOCK_SIZE_COUNT || width == NULL || height == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    *width = av2h_block_geometry[block_size].mi_width;
    *height = av2h_block_geometry[block_size].mi_height;
    return AV2H_OK;
}

Av2hStatus av2h_partition_contexts(Av2hBlockSize block_size,
                                    unsigned int left_smaller,
                                    unsigned int above_smaller,
                                    unsigned int first_half_smaller,
                                    unsigned int second_half_smaller,
                                    size_t *split_context,
                                    size_t *rect_type_context,
                                    size_t *extension_context) {
    const Av2hBlockGeometry *geometry;
    if (block_size >= AV2H_BLOCK_SIZE_COUNT || split_context == NULL ||
        rect_type_context == NULL || extension_context == NULL ||
        left_smaller > 1U || above_smaller > 1U ||
        first_half_smaller > 1U || second_half_smaller > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry = &av2h_block_geometry[block_size];
    *split_context = geometry->split_size_adjust * 4U +
                     left_smaller * 2U + above_smaller;
    *rect_type_context = geometry->rect_size_adjust * 4U +
                         left_smaller * 2U + above_smaller;
    *extension_context = geometry->split_size_adjust * 4U +
                         first_half_smaller * 2U + second_half_smaller;
    if (*split_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        *rect_type_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        *extension_context >= AV2H_PARTITION_CONTEXT_COUNT) {
        return AV2H_INVALID_DATA;
    }
    return AV2H_OK;
}

static unsigned int context_bit(uint8_t context, uint8_t dimension,
                                unsigned int extension) {
    uint8_t shift = 0U;
    while (dimension > 1U) {
        dimension >>= 1U;
        ++shift;
    }
    if (shift > extension) shift = (uint8_t)(shift - extension);
    else shift = 0U;
    return ((unsigned int)context >> shift) & 1U;
}

Av2hStatus av2h_partition_context_state_init(
    Av2hPartitionContextState *state,
    uint8_t *luma_above,
    uint8_t *luma_left,
    uint8_t *chroma_above,
    uint8_t *chroma_left,
    size_t above_count,
    size_t left_count) {
    size_t index;
    if (state == NULL || luma_above == NULL || luma_left == NULL ||
        chroma_above == NULL || chroma_left == NULL || above_count == 0U ||
        left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    state->above[0] = luma_above;
    state->left[0] = luma_left;
    state->above[1] = chroma_above;
    state->left[1] = chroma_left;
    state->above_count = above_count;
    state->left_count = left_count;
    for (index = 0U; index < above_count; ++index) {
        luma_above[index] = 0U;
        chroma_above[index] = 0U;
    }
    for (index = 0U; index < left_count; ++index) {
        luma_left[index] = 0U;
        chroma_left[index] = 0U;
    }
    return AV2H_OK;
}

Av2hStatus av2h_partition_contexts_from_state(
    const Av2hPartitionContextState *state,
    size_t structure,
    Av2hPartitionNode node,
    Av2hPartitionAxis axis,
    size_t *split_context,
    size_t *rect_type_context,
    size_t *extension_context) {
    const Av2hBlockGeometry *geometry;
    size_t left_index;
    size_t second_index;
    unsigned int left_smaller;
    unsigned int above_smaller;
    unsigned int first_half_smaller;
    unsigned int second_half_smaller;
    if (state == NULL || structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        node.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        axis > AV2H_PARTITION_VERTICAL || split_context == NULL ||
        rect_type_context == NULL || extension_context == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry = &av2h_block_geometry[node.block_size];
    left_index = node.row % state->left_count;
    if (node.column >= state->above_count ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    left_smaller = context_bit(state->left[structure][left_index],
                               geometry->mi_height, 1U);
    above_smaller = context_bit(state->above[structure][node.column],
                                geometry->mi_width, 1U);
    if (axis == AV2H_PARTITION_HORIZONTAL) {
        second_index = left_index + geometry->mi_height / 2U;
        first_half_smaller = context_bit(
            state->left[structure][left_index], geometry->mi_height, 2U);
        second_half_smaller = second_index < state->left_count ?
            context_bit(state->left[structure][second_index],
                        geometry->mi_height, 2U) : 0U;
    } else {
        second_index = (size_t)node.column + geometry->mi_width / 2U;
        first_half_smaller = context_bit(
            state->above[structure][node.column], geometry->mi_width, 2U);
        second_half_smaller = second_index < state->above_count ?
            context_bit(state->above[structure][second_index],
                        geometry->mi_width, 2U) : 0U;
    }
    return av2h_partition_contexts(
        node.block_size, left_smaller, above_smaller, first_half_smaller,
        second_half_smaller, split_context, rect_type_context,
        extension_context);
}

Av2hStatus av2h_partition_context_state_update(
    Av2hPartitionContextState *state,
    size_t structure,
    Av2hPartitionNode leaf) {
    const Av2hBlockGeometry *geometry;
    size_t width;
    size_t height;
    size_t index;
    size_t left_index;
    if (state == NULL || structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        leaf.block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry = &av2h_block_geometry[leaf.block_size];
    width = geometry->mi_width;
    height = geometry->mi_height;
    left_index = leaf.row % state->left_count;
    if (leaf.column >= state->above_count ||
        left_index >= state->left_count) {
        return AV2H_OVERFLOW;
    }
    if (width > state->above_count - leaf.column) {
        width = state->above_count - leaf.column;
    }
    if (height > state->left_count - left_index) {
        height = state->left_count - left_index;
    }
    for (index = 0U; index < width; ++index) {
        state->above[structure][leaf.column + index] =
            (uint8_t)(64U - geometry->mi_width);
    }
    for (index = 0U; index < height; ++index) {
        state->left[structure][left_index + index] =
            (uint8_t)(64U - geometry->mi_height);
    }
    return AV2H_OK;
}