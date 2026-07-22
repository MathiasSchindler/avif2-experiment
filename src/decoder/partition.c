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
                                Av2hPartitionAxis axis) {
    if (block_size >= AV2H_BLOCK_SIZE_COUNT ||
        block_size <= AV2H_BLOCK_8X8 ||
        (block_size > AV2H_BLOCK_64X64 &&
         block_size <= AV2H_BLOCK_256X256) ||
        block_size == AV2H_BLOCK_4X16 ||
        block_size == AV2H_BLOCK_16X4) {
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
    mi_columns = (uint16_t)(2U * (((uint32_t)frame_width + 7U) / 8U));
    mi_rows = (uint16_t)(2U * (((uint32_t)frame_height + 7U) / 8U));
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
    mi_columns = (uint16_t)(2U * (((uint32_t)frame_width + 7U) / 8U));
    mi_rows = (uint16_t)(2U * (((uint32_t)frame_height + 7U) / 8U));
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
            !extension_is_allowed(node.block_size, axis);
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