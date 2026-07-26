#include "partition.h"

typedef struct {
    uint8_t mi_width;
    uint8_t mi_height;
    uint8_t split_size_adjust;
    uint8_t rect_size_adjust;
} Av2hBlockGeometry;

static const Av2hBlockGeometry block_geometry[AV2H_BLOCK_SIZE_COUNT] = {
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
    { 16U, 2U, 0U, 0U }
};

Av2hStatus av2h_block_dimensions_mi(Av2hBlockSize block_size,
                                     size_t *width,
                                     size_t *height) {
    if (block_size >= AV2H_BLOCK_SIZE_COUNT || width == 0 || height == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    *width = block_geometry[block_size].mi_width;
    *height = block_geometry[block_size].mi_height;
    return AV2H_OK;
}

static int is_partition_point(Av2hBlockSize block_size) {
    return block_size != AV2H_BLOCK_4X4 &&
           block_size <= AV2H_BLOCK_64X16;
}

static Av2hBlockSize ordinary_subsize(Av2hBlockSize block_size,
                                      Av2hPartitionType partition) {
    size_t index;
    uint8_t width;
    uint8_t height;
    if (block_size >= AV2H_BLOCK_SIZE_COUNT) return AV2H_BLOCK_INVALID;
    width = block_geometry[block_size].mi_width;
    height = block_geometry[block_size].mi_height;
    if (partition == AV2H_PARTITION_HORZ) {
        if (height < 2U) return AV2H_BLOCK_INVALID;
        height >>= 1U;
    } else if (partition == AV2H_PARTITION_VERT) {
        if (width < 2U) return AV2H_BLOCK_INVALID;
        width >>= 1U;
    } else {
        return AV2H_BLOCK_INVALID;
    }
    for (index = 0U; index < AV2H_BLOCK_SIZE_COUNT; ++index) {
        if (block_geometry[index].mi_width == width &&
            block_geometry[index].mi_height == height) {
            return (Av2hBlockSize)index;
        }
    }
    return AV2H_BLOCK_INVALID;
}

static Av2hBlockSize block_size_from_dimensions(uint8_t width,
                                                uint8_t height) {
    size_t index;
    for (index = 0U; index < AV2H_BLOCK_SIZE_COUNT; ++index) {
        if (block_geometry[index].mi_width == width &&
            block_geometry[index].mi_height == height) {
            return (Av2hBlockSize)index;
        }
    }
    return AV2H_BLOCK_INVALID;
}

static Av2hPartitionAxis implied_axis(Av2hBlockSize block_size) {
    if (block_size == AV2H_BLOCK_128X256 ||
        block_size == AV2H_BLOCK_4X8 ||
        block_size == AV2H_BLOCK_64X128 ||
        block_size == AV2H_BLOCK_4X16) {
        return AV2H_PARTITION_HORIZONTAL;
    }
    if (block_size == AV2H_BLOCK_256X128 ||
        block_size == AV2H_BLOCK_8X4 ||
        block_size == AV2H_BLOCK_128X64 ||
        block_size == AV2H_BLOCK_16X4) {
        return AV2H_PARTITION_VERTICAL;
    }
    return (Av2hPartitionAxis)255;
}

static int extension_is_allowed(Av2hBlockSize block_size,
                                Av2hPartitionAxis axis,
                                size_t structure) {
    if (block_size >= AV2H_BLOCK_SIZE_COUNT ||
        block_size <= AV2H_BLOCK_8X8 ||
        (block_size > AV2H_BLOCK_64X64 &&
         block_size <= AV2H_BLOCK_256X256) ||
        block_size == AV2H_BLOCK_4X16 ||
        block_size == AV2H_BLOCK_16X4) {
        return 0;
    }
    if (structure == 1U &&
        (block_size <= AV2H_BLOCK_16X16 ||
         block_size == AV2H_BLOCK_8X32 ||
         block_size == AV2H_BLOCK_32X8 ||
         (block_size == AV2H_BLOCK_32X16 &&
          axis == AV2H_PARTITION_HORIZONTAL) ||
         (block_size == AV2H_BLOCK_16X32 &&
          axis == AV2H_PARTITION_VERTICAL))) {
        return 0;
    }
    if ((block_size == AV2H_BLOCK_16X8 &&
         axis == AV2H_PARTITION_HORIZONTAL) ||
        (block_size == AV2H_BLOCK_8X16 &&
         axis == AV2H_PARTITION_VERTICAL) ||
        (block_size == AV2H_BLOCK_32X8 &&
         axis == AV2H_PARTITION_HORIZONTAL) ||
        (block_size == AV2H_BLOCK_8X32 &&
         axis == AV2H_PARTITION_VERTICAL) ||
        (block_size == AV2H_BLOCK_64X16 &&
         axis == AV2H_PARTITION_HORIZONTAL) ||
        (block_size == AV2H_BLOCK_16X64 &&
         axis == AV2H_PARTITION_VERTICAL)) {
        return 0;
    }
    return 1;
}

static int uneven_4way_is_allowed(Av2hBlockSize block_size,
                                  Av2hPartitionAxis axis,
                                  size_t structure) {
    const Av2hBlockGeometry *geometry;
    int size_allowed;
    if (!extension_is_allowed(block_size, axis, structure)) return 0;
    if (structure == 1U && block_size == AV2H_BLOCK_64X32 &&
        axis == AV2H_PARTITION_HORIZONTAL) {
        return 0;
    }
    geometry = &block_geometry[block_size];
    if (block_size <= AV2H_BLOCK_64X64) {
        size_allowed = structure == 0U
            ? block_size >= AV2H_BLOCK_16X32
            : block_size >= AV2H_BLOCK_32X64;
    } else {
        size_allowed =
            block_size >= AV2H_BLOCK_8X32 &&
            block_size <= AV2H_BLOCK_64X16 &&
            (structure == 0U || block_size >= AV2H_BLOCK_16X64);
    }
    if (!size_allowed) return 0;
    return axis == AV2H_PARTITION_HORIZONTAL
        ? geometry->mi_height >= 8U
        : geometry->mi_width >= 8U;
}

static Av2hPartitionType forced_at_boundary(uint16_t mi_rows,
                                             uint16_t mi_columns,
                                             Av2hPartitionNode node) {
    const Av2hBlockGeometry *geometry;
    unsigned int has_rows;
    unsigned int has_columns;
    if (node.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        !is_partition_point(node.block_size)) {
        return AV2H_PARTITION_NONE;
    }
    geometry = &block_geometry[node.block_size];
    has_rows = (size_t)node.row + geometry->mi_height / 2U < mi_rows;
    has_columns =
        (size_t)node.column + geometry->mi_width / 2U < mi_columns;
    if (has_rows && has_columns) return AV2H_PARTITION_INVALID;
    if (geometry->mi_width == geometry->mi_height) {
        return has_rows && !has_columns ? AV2H_PARTITION_VERT
                                        : AV2H_PARTITION_HORZ;
    }
    if (geometry->mi_height > geometry->mi_width) {
        if (!has_rows) return AV2H_PARTITION_HORZ;
        if (!has_columns && geometry->mi_width >= 4U &&
            (size_t)node.column + geometry->mi_width / 4U >= mi_columns) {
            return AV2H_PARTITION_HORZ;
        }
    } else {
        if (!has_columns) return AV2H_PARTITION_VERT;
        if (!has_rows && geometry->mi_height >= 4U &&
            (size_t)node.row + geometry->mi_height / 4U >= mi_rows) {
            return AV2H_PARTITION_VERT;
        }
    }
    return AV2H_PARTITION_INVALID;
}

Av2hStatus av2h_partition_forced_path(uint16_t mi_rows,
                                       uint16_t mi_columns,
                                       Av2hPartitionNode root,
                                       Av2hForcedPartitionPath *path) {
    Av2hPartitionNode node = root;
    size_t count = 0U;
    if (path == 0 || mi_rows == 0U || mi_columns == 0U ||
        root.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        root.row >= mi_rows || root.column >= mi_columns) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (;;) {
        Av2hPartitionType partition =
            forced_at_boundary(mi_rows, mi_columns, node);
        Av2hBlockSize child;
        if (partition == AV2H_PARTITION_INVALID ||
            partition == AV2H_PARTITION_NONE) {
            path->step_count = count;
            path->next = node;
            return AV2H_OK;
        }
        if (count >= AV2H_MAX_FORCED_PARTITION_ANCESTORS) {
            return AV2H_OVERFLOW;
        }
        path->steps[count].node = node;
        path->steps[count].partition = partition;
        ++count;
        child = ordinary_subsize(node.block_size, partition);
        if (child == AV2H_BLOCK_INVALID) return AV2H_UNSUPPORTED;
        node.block_size = child;
    }
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
    if (block_size >= AV2H_BLOCK_SIZE_COUNT || split_context == 0 ||
        rect_type_context == 0 || extension_context == 0 ||
        left_smaller > 1U || above_smaller > 1U ||
        first_half_smaller > 1U || second_half_smaller > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry = &block_geometry[block_size];
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
    if (state == 0 || luma_above == 0 || luma_left == 0 ||
        chroma_above == 0 || chroma_left == 0 || above_count == 0U ||
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
    if (state == 0 || structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        node.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        axis > AV2H_PARTITION_VERTICAL || split_context == 0 ||
        rect_type_context == 0 || extension_context == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry = &block_geometry[node.block_size];
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
        if (second_index >= state->left_count) return AV2H_OVERFLOW;
        first_half_smaller = context_bit(
            state->left[structure][left_index], geometry->mi_height, 2U);
        second_half_smaller = context_bit(
            state->left[structure][second_index], geometry->mi_height, 2U);
    } else {
        second_index = (size_t)node.column + geometry->mi_width / 2U;
        if (second_index >= state->above_count) return AV2H_OVERFLOW;
        first_half_smaller = context_bit(
            state->above[structure][node.column], geometry->mi_width, 2U);
        second_half_smaller = context_bit(
            state->above[structure][second_index], geometry->mi_width, 2U);
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
    if (state == 0 || structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        leaf.block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    geometry = &block_geometry[leaf.block_size];
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

Av2hStatus av2h_partition_first_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    uint16_t frame_width,
    uint16_t frame_height,
    Av2hFirstPartitionPrefix *prefix) {
    uint16_t mi_rows;
    uint16_t mi_columns;
    Av2hStatus status;
    if (decoder == 0 || cdfs == 0 || prefix == 0 ||
        frame_width == 0U || frame_height == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    mi_columns = (uint16_t)(((uint32_t)frame_width + 3U) / 4U);
    mi_rows = (uint16_t)(((uint32_t)frame_height + 3U) / 4U);
    status = av2h_partition_forced_path(
        mi_rows, mi_columns,
        (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 },
        &prefix->forced);
    if (status != AV2H_OK) return status;
    status = av2h_partition_contexts(
        prefix->forced.next.block_size, 0U, 0U, 0U, 0U,
        &prefix->split_context, &prefix->rect_type_context,
        &prefix->extension_context);
    if (status != AV2H_OK) return status;
    return av2h_partition_ordinary_prefix_read(
        decoder, cdfs, prefix->split_context, prefix->rect_type_context,
        prefix->extension_context, &prefix->ordinary);
}

static Av2hStatus append_decision(Av2hFirstLeafPath *path,
                                  Av2hPartitionNode node,
                                  Av2hPartitionType partition,
                                  unsigned int forced,
                                  unsigned int axis_implied,
                                  unsigned int extension_implied,
                                  size_t after_bits) {
    Av2hPartitionDecision *decision;
    if (path->decision_count >= AV2H_MAX_FIRST_LEAF_PARTITIONS) {
        return AV2H_OVERFLOW;
    }
    decision = &path->decisions[path->decision_count++];
    decision->node = node;
    decision->partition = partition;
    decision->forced = (uint8_t)forced;
    decision->axis_implied = (uint8_t)axis_implied;
    decision->extension_implied = (uint8_t)extension_implied;
    decision->after_bits = after_bits;
    return AV2H_OK;
}

Av2hStatus av2h_partition_first_leaf_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    uint16_t frame_width,
    uint16_t frame_height,
    unsigned int enable_ext_partitions,
    Av2hFirstLeafPath *path) {
    Av2hPartitionNode node = { 0U, 0U, AV2H_BLOCK_64X64 };
    uint16_t mi_rows;
    uint16_t mi_columns;
    if (decoder == 0 || cdfs == 0 || path == 0 || frame_width == 0U ||
        frame_height == 0U || enable_ext_partitions > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    mi_columns = (uint16_t)(((uint32_t)frame_width + 3U) / 4U);
    mi_rows = (uint16_t)(((uint32_t)frame_height + 3U) / 4U);
    path->decision_count = 0U;
    for (;;) {
        Av2hPartitionType partition =
            forced_at_boundary(mi_rows, mi_columns, node);
        Av2hPartitionAxis axis;
        Av2hBlockSize child;
        Av2hStatus status;
        size_t split_context;
        size_t rect_context;
        size_t extension_context;
        unsigned int axis_was_implied;
        unsigned int extension_was_implied;
        uint32_t value;
        if (partition != AV2H_PARTITION_INVALID &&
            partition != AV2H_PARTITION_NONE) {
            status = append_decision(path, node, partition, 1U, 1U, 1U,
                                     av2h_range_tell(decoder));
            if (status != AV2H_OK) return status;
            child = ordinary_subsize(node.block_size, partition);
            if (child == AV2H_BLOCK_INVALID) return AV2H_UNSUPPORTED;
            node.block_size = child;
            continue;
        }
        if (!is_partition_point(node.block_size)) {
            path->leaf = node;
            return AV2H_OK;
        }
        status = av2h_partition_contexts(
            node.block_size, 0U, 0U, 0U, 0U, &split_context,
            &rect_context, &extension_context);
        if (status != AV2H_OK) return status;
        value = av2h_range_read_adaptive(
            decoder, cdfs->do_split[0][split_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (value == 0U) {
            status = append_decision(path, node, AV2H_PARTITION_NONE, 0U,
                                     1U, 1U, av2h_range_tell(decoder));
            if (status != AV2H_OK) return status;
            path->leaf = node;
            return AV2H_OK;
        }

        axis = implied_axis(node.block_size);
        axis_was_implied = axis != (Av2hPartitionAxis)255;
        if (!axis_was_implied) {
            value = av2h_range_read_adaptive(
                decoder, cdfs->rect_type[0][rect_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            axis = value == 0U ? AV2H_PARTITION_HORIZONTAL
                               : AV2H_PARTITION_VERTICAL;
        }
        extension_was_implied = !enable_ext_partitions ||
            !extension_is_allowed(node.block_size, axis, 0U);
        if (!extension_was_implied) {
            value = av2h_range_read_adaptive(
                decoder, cdfs->do_ext_partition[0][extension_context], 2U);
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
            if (value != 0U) return AV2H_UNSUPPORTED;
        }
        partition = axis == AV2H_PARTITION_HORIZONTAL
            ? AV2H_PARTITION_HORZ : AV2H_PARTITION_VERT;
        status = append_decision(path, node, partition, 0U,
                                 axis_was_implied, extension_was_implied,
                                 av2h_range_tell(decoder));
        if (status != AV2H_OK) return status;
        child = ordinary_subsize(node.block_size, partition);
        if (child == AV2H_BLOCK_INVALID) return AV2H_UNSUPPORTED;
        node.block_size = child;
    }
}

Av2hStatus av2h_partition_ordinary_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    Av2hOrdinaryPartitionPrefix *prefix) {
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (split_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        rect_type_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        extension_context >= AV2H_PARTITION_CONTEXT_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    value = av2h_range_read_adaptive(
        decoder, cdfs->do_split[0][split_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->after_split_bits = av2h_range_tell(decoder);
    if (value == 0U) return AV2H_UNSUPPORTED;

    value = av2h_range_read_adaptive(
        decoder, cdfs->rect_type[0][rect_type_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->axis = value == 0U ? AV2H_PARTITION_HORIZONTAL
                               : AV2H_PARTITION_VERTICAL;
    prefix->after_axis_bits = av2h_range_tell(decoder);

    value = av2h_range_read_adaptive(
        decoder,
        cdfs->do_ext_partition[0][extension_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->after_extension_bits = av2h_range_tell(decoder);
    return value == 0U ? AV2H_OK : AV2H_UNSUPPORTED;
}

Av2hStatus av2h_partition_node_read_for_structure(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    size_t structure,
    Av2hPartitionNode node,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    unsigned int enable_ext_partitions,
    Av2hPartitionDecision *decision,
    Av2hPartitionNode *child) {
    Av2hPartitionAxis axis;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || decision == 0 || child == 0 ||
        structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        node.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        split_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        rect_type_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        extension_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        enable_ext_partitions > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (!is_partition_point(node.block_size)) return AV2H_UNSUPPORTED;
    decision->node = node;
    decision->forced = 0U;
    decision->extension_context = 0U;
    if (structure == 1U && node.block_size == AV2H_BLOCK_8X8) {
        decision->partition = AV2H_PARTITION_NONE;
        decision->forced = 1U;
        decision->axis_implied = 1U;
        decision->extension_implied = 1U;
        decision->after_bits = av2h_range_tell(decoder);
        *child = node;
        return AV2H_OK;
    }
    value = av2h_range_read_adaptive(
        decoder, cdfs->do_split[structure][split_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (value == 0U) {
        decision->partition = AV2H_PARTITION_NONE;
        decision->axis_implied = 1U;
        decision->extension_implied = 1U;
        decision->after_bits = av2h_range_tell(decoder);
        *child = node;
        return AV2H_OK;
    }
    axis = implied_axis(node.block_size);
    if (structure == 1U && node.block_size == AV2H_BLOCK_16X8) {
        axis = AV2H_PARTITION_VERTICAL;
    } else if (structure == 1U && node.block_size == AV2H_BLOCK_8X16) {
        axis = AV2H_PARTITION_HORIZONTAL;
    }
    decision->axis_implied = (uint8_t)(axis != (Av2hPartitionAxis)255);
    if (!decision->axis_implied) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->rect_type[structure][rect_type_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        axis = value == 0U ? AV2H_PARTITION_HORIZONTAL
                           : AV2H_PARTITION_VERTICAL;
    }
    decision->extension_implied = (uint8_t)(structure == 1U ||
        !enable_ext_partitions ||
        !extension_is_allowed(node.block_size, axis, structure));
    if (!decision->extension_implied) {
        value = av2h_range_read_adaptive(
            decoder,
            cdfs->do_ext_partition[structure][extension_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (value != 0U) return AV2H_UNSUPPORTED;
    }
    decision->partition = axis == AV2H_PARTITION_HORIZONTAL
        ? AV2H_PARTITION_HORZ : AV2H_PARTITION_VERT;
    decision->after_bits = av2h_range_tell(decoder);
    *child = node;
    child->block_size = ordinary_subsize(node.block_size,
                                         decision->partition);
    if (child->block_size == AV2H_BLOCK_INVALID) return AV2H_UNSUPPORTED;
    return AV2H_OK;
}

Av2hStatus av2h_partition_node_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    Av2hPartitionNode node,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    unsigned int enable_ext_partitions,
    Av2hPartitionDecision *decision,
    Av2hPartitionNode *child) {
    return av2h_partition_node_read_for_structure(
        decoder, cdfs, 0U, node, split_context, rect_type_context,
        extension_context, enable_ext_partitions, decision, child);
}

Av2hStatus av2h_partition_node_read_with_state(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    const Av2hPartitionContextState *state,
    size_t structure,
    Av2hPartitionNode node,
    unsigned int enable_ext_partitions,
    unsigned int enable_uneven_4way_partitions,
    Av2hPartitionDecision *decision,
    Av2hPartitionNode *child) {
    Av2hPartitionAxis axis = implied_axis(node.block_size);
    size_t split_context;
    size_t rect_type_context;
    size_t extension_context;
    Av2hStatus status;
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || state == 0 || decision == 0 ||
        child == 0 || structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        node.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        enable_ext_partitions > 1U ||
        enable_uneven_4way_partitions > 1U ||
        !is_partition_point(node.block_size)) {
        return AV2H_INVALID_ARGUMENT;
    }
    decision->node = node;
    decision->forced = 0U;
    if (structure == 1U && node.block_size == AV2H_BLOCK_8X8) {
        decision->partition = AV2H_PARTITION_NONE;
        decision->forced = 1U;
        decision->axis_implied = 1U;
        decision->extension_implied = 1U;
        decision->after_bits = av2h_range_tell(decoder);
        *child = node;
        return AV2H_OK;
    }
    status = av2h_partition_contexts_from_state(
        state, structure, node, AV2H_PARTITION_HORIZONTAL, &split_context,
        &rect_type_context, &extension_context);
    if (status != AV2H_OK) return status;
    value = av2h_range_read_adaptive(
        decoder, cdfs->do_split[structure][split_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (value == 0U) {
        decision->partition = AV2H_PARTITION_NONE;
        decision->axis_implied = 1U;
        decision->extension_implied = 1U;
        decision->after_bits = av2h_range_tell(decoder);
        *child = node;
        return AV2H_OK;
    }
    if (structure == 1U && node.block_size == AV2H_BLOCK_16X8) {
        axis = AV2H_PARTITION_VERTICAL;
    } else if (structure == 1U && node.block_size == AV2H_BLOCK_8X16) {
        axis = AV2H_PARTITION_HORIZONTAL;
    }
    decision->axis_implied = (uint8_t)(axis != (Av2hPartitionAxis)255);
    if (!decision->axis_implied) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->rect_type[structure][rect_type_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        axis = value == 0U ? AV2H_PARTITION_HORIZONTAL
                           : AV2H_PARTITION_VERTICAL;
    }
    status = av2h_partition_contexts_from_state(
        state, structure, node, axis, &split_context, &rect_type_context,
        &extension_context);
    if (status != AV2H_OK) return status;
    decision->extension_context = extension_context;
    decision->extension_implied = (uint8_t)(!enable_ext_partitions ||
        !extension_is_allowed(node.block_size, axis, structure));
    if (!decision->extension_implied) {
        value = av2h_range_read_adaptive(
            decoder, cdfs->do_ext_partition[structure][extension_context], 2U);
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (value != 0U) {
            if (enable_uneven_4way_partitions &&
                uneven_4way_is_allowed(node.block_size, axis, structure)) {
                value = av2h_range_read_adaptive(
                    decoder,
                    cdfs->do_uneven_4way_partition[structure]
                                                       [extension_context],
                    2U);
                if (av2h_range_status(decoder) != AV2H_OK) {
                    return av2h_range_status(decoder);
                }
                if (value != 0U) return AV2H_UNSUPPORTED;
            }
            decision->partition = axis == AV2H_PARTITION_HORIZONTAL
                ? AV2H_PARTITION_HORZ_3 : AV2H_PARTITION_VERT_3;
            decision->after_bits = av2h_range_tell(decoder);
            *child = node;
            return AV2H_OK;
        }
    }
    decision->partition = axis == AV2H_PARTITION_HORIZONTAL
        ? AV2H_PARTITION_HORZ : AV2H_PARTITION_VERT;
    decision->after_bits = av2h_range_tell(decoder);
    *child = node;
    child->block_size = ordinary_subsize(node.block_size,
                                         decision->partition);
    if (child->block_size == AV2H_BLOCK_INVALID) return AV2H_UNSUPPORTED;
    return AV2H_OK;
}

static Av2hStatus traversal_push_children(
    Av2hPartitionTraversal *traversal,
    Av2hPartitionNode node,
    Av2hPartitionType partition) {
    Av2hPartitionNode first = node;
    Av2hPartitionNode second = node;
    size_t width;
    size_t height;
    if (partition == AV2H_PARTITION_HORZ_3 ||
        partition == AV2H_PARTITION_VERT_3) {
        Av2hPartitionNode children[4U];
        uint8_t width = block_geometry[node.block_size].mi_width;
        uint8_t height = block_geometry[node.block_size].mi_height;
        Av2hBlockSize outer;
        Av2hBlockSize middle;
        size_t index;
        if (traversal->stack_count + 4U >
            AV2H_MAX_PARTITION_TRAVERSAL_NODES) {
            return AV2H_OVERFLOW;
        }
        outer = partition == AV2H_PARTITION_HORZ_3 ?
            block_size_from_dimensions(width, (uint8_t)(height / 4U)) :
            block_size_from_dimensions((uint8_t)(width / 4U), height);
        middle = block_size_from_dimensions(
            (uint8_t)(width / 2U), (uint8_t)(height / 2U));
        if (outer == AV2H_BLOCK_INVALID || middle == AV2H_BLOCK_INVALID) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < 4U; ++index) children[index] = node;
        children[0].block_size = outer;
        children[3].block_size = outer;
        children[1].block_size = middle;
        children[2].block_size = middle;
        if (partition == AV2H_PARTITION_HORZ_3) {
            children[1].row = (uint16_t)(node.row + height / 4U);
            children[2].row = children[1].row;
            children[2].column = (uint16_t)(node.column + width / 2U);
            children[3].row = (uint16_t)(node.row + 3U * height / 4U);
        } else {
            children[1].column = (uint16_t)(node.column + width / 4U);
            children[2].column = children[1].column;
            children[2].row = (uint16_t)(node.row + height / 2U);
            children[3].column = (uint16_t)(node.column + 3U * width / 4U);
        }
        for (index = 4U; index != 0U; --index) {
            traversal->stack[traversal->stack_count++] = children[index - 1U];
        }
        return AV2H_OK;
    }
    first.block_size = ordinary_subsize(node.block_size, partition);
    if (first.block_size == AV2H_BLOCK_INVALID ||
        traversal->stack_count + 2U > AV2H_MAX_PARTITION_TRAVERSAL_NODES) {
        return first.block_size == AV2H_BLOCK_INVALID ? AV2H_UNSUPPORTED
                                                     : AV2H_OVERFLOW;
    }
    if (av2h_block_dimensions_mi(first.block_size, &width, &height) !=
        AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    second.block_size = first.block_size;
    if (partition == AV2H_PARTITION_HORZ) {
        if ((size_t)second.row + height > 65535U) return AV2H_OVERFLOW;
        second.row = (uint16_t)((size_t)second.row + height);
    } else if (partition == AV2H_PARTITION_VERT) {
        if ((size_t)second.column + width > 65535U) return AV2H_OVERFLOW;
        second.column = (uint16_t)((size_t)second.column + width);
    } else {
        return AV2H_UNSUPPORTED;
    }
    traversal->stack[traversal->stack_count++] = second;
    traversal->stack[traversal->stack_count++] = first;
    return AV2H_OK;
}

Av2hStatus av2h_partition_traversal_init(
    Av2hPartitionTraversal *traversal,
    uint16_t mi_rows,
    uint16_t mi_columns,
    Av2hPartitionNode root,
    size_t structure,
    unsigned int enable_ext_partitions,
    unsigned int enable_uneven_4way_partitions) {
    if (traversal == 0 || mi_rows == 0U || mi_columns == 0U ||
        root.row >= mi_rows || root.column >= mi_columns ||
        root.block_size >= AV2H_BLOCK_SIZE_COUNT ||
        structure >= AV2H_PARTITION_STRUCTURE_COUNT ||
        enable_ext_partitions > 1U ||
        enable_uneven_4way_partitions > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    traversal->stack[0] = root;
    traversal->pending_leaf = root;
    traversal->stack_count = 1U;
    traversal->mi_rows = mi_rows;
    traversal->mi_columns = mi_columns;
    traversal->structure = (uint8_t)structure;
    traversal->enable_ext_partitions = (uint8_t)enable_ext_partitions;
    traversal->enable_uneven_4way_partitions =
        (uint8_t)enable_uneven_4way_partitions;
    traversal->has_pending_leaf = 0U;
    traversal->diagnostic_node = root;
    traversal->diagnostic_before_bits = 0U;
    return AV2H_OK;
}

Av2hStatus av2h_partition_traversal_reset_root(
    Av2hPartitionTraversal *traversal,
    Av2hPartitionNode root) {
    if (traversal == 0 || traversal->mi_rows == 0U ||
        traversal->mi_columns == 0U || traversal->has_pending_leaf ||
        root.row >= traversal->mi_rows ||
        root.column >= traversal->mi_columns ||
        root.block_size >= AV2H_BLOCK_SIZE_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    traversal->stack[0] = root;
    traversal->pending_leaf = root;
    traversal->stack_count = 1U;
    traversal->has_pending_leaf = 0U;
    traversal->diagnostic_node = root;
    traversal->diagnostic_before_bits = 0U;
    return AV2H_OK;
}

Av2hStatus av2h_partition_traversal_next(
    Av2hPartitionTraversal *traversal,
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    const Av2hPartitionContextState *state,
    Av2hFirstLeafPath *path,
    uint8_t *has_leaf) {
    if (traversal == 0 || decoder == 0 || cdfs == 0 || state == 0 ||
        path == 0 || has_leaf == 0 || traversal->has_pending_leaf) {
        return AV2H_INVALID_ARGUMENT;
    }
    path->decision_count = 0U;
    *has_leaf = 0U;
    while (traversal->stack_count != 0U) {
        Av2hPartitionNode node =
            traversal->stack[--traversal->stack_count];
        Av2hPartitionType forced;
        Av2hPartitionDecision decision;
        Av2hPartitionNode child;
        Av2hStatus status;
        if (node.row >= traversal->mi_rows ||
            node.column >= traversal->mi_columns) {
            continue;
        }
        traversal->diagnostic_node = node;
        traversal->diagnostic_before_bits = av2h_range_tell(decoder);
        forced = forced_at_boundary(
            traversal->mi_rows, traversal->mi_columns, node);
        if (forced != AV2H_PARTITION_INVALID &&
            forced != AV2H_PARTITION_NONE) {
            status = append_decision(
                path, node, forced, 1U, 1U, 1U,
                av2h_range_tell(decoder));
            if (status != AV2H_OK) return status;
            status = traversal_push_children(traversal, node, forced);
            if (status != AV2H_OK) return status;
            continue;
        }
        if (!is_partition_point(node.block_size)) {
            path->leaf = node;
            traversal->pending_leaf = node;
            traversal->has_pending_leaf = 1U;
            *has_leaf = 1U;
            return AV2H_OK;
        }
        status = av2h_partition_node_read_with_state(
            decoder, cdfs, state, traversal->structure, node,
            traversal->enable_ext_partitions,
            traversal->enable_uneven_4way_partitions, &decision, &child);
        if (status != AV2H_OK) return status;
        status = append_decision(
            path, decision.node, decision.partition, decision.forced,
            decision.axis_implied, decision.extension_implied,
            decision.after_bits);
        if (status != AV2H_OK) return status;
        if (decision.partition == AV2H_PARTITION_NONE) {
            path->leaf = child;
            traversal->pending_leaf = child;
            traversal->has_pending_leaf = 1U;
            *has_leaf = 1U;
            return AV2H_OK;
        }
        status = traversal_push_children(
            traversal, node, decision.partition);
        if (status != AV2H_OK) return status;
    }
    return AV2H_OK;
}

Av2hStatus av2h_partition_traversal_commit_leaf(
    Av2hPartitionTraversal *traversal,
    Av2hPartitionContextState *state,
    Av2hPartitionNode leaf) {
    Av2hStatus status;
    if (traversal == 0 || state == 0 || !traversal->has_pending_leaf ||
        traversal->pending_leaf.row != leaf.row ||
        traversal->pending_leaf.column != leaf.column ||
        traversal->pending_leaf.block_size != leaf.block_size) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_partition_context_state_update(
        state, traversal->structure, leaf);
    if (status != AV2H_OK) return status;
    traversal->has_pending_leaf = 0U;
    return AV2H_OK;
}