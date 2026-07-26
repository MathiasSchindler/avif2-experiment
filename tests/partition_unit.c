#include "decoder/partition.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void check_prefix(const char *name,
                         const unsigned char *bytes,
                         size_t size,
                         uint16_t width,
                         uint16_t height,
                         size_t expected_forced_steps,
                         Av2hBlockSize expected_node,
                         size_t expected_split_context,
                         size_t expected_rect_type_context,
                         size_t expected_extension_context,
                         Av2hPartitionAxis expected_axis) {
    Av2hSpan span = { bytes, size };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs cdfs;
    Av2hFirstPartitionPrefix prefix;
    CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    {
        Av2hStatus status =
            av2h_partition_first_prefix_read(
                &decoder, &cdfs, width, height, &prefix);
        if (status != AV2H_OK) {
            fprintf(stderr, "%s: status=%d axis=%d tell=%zu\n", name,
                    status, (int)prefix.ordinary.axis,
                    av2h_range_tell(&decoder));
            ++failures;
            return;
        }
    }
    CHECK(prefix.forced.step_count == expected_forced_steps);
    CHECK(prefix.forced.next.block_size == expected_node);
    CHECK(prefix.split_context == expected_split_context);
    CHECK(prefix.rect_type_context == expected_rect_type_context);
    CHECK(prefix.extension_context == expected_extension_context);
    CHECK(prefix.ordinary.axis == expected_axis);
    CHECK(prefix.ordinary.after_split_bits <= prefix.ordinary.after_axis_bits);
    CHECK(prefix.ordinary.after_axis_bits <=
          prefix.ordinary.after_extension_bits);
    CHECK(cdfs.do_split[0][expected_split_context][2] == 1U);
    CHECK(cdfs.rect_type[0][expected_rect_type_context][2] == 1U);
    CHECK(cdfs.do_ext_partition[0][expected_extension_context][2] == 1U);
}

static void test_fixture_prefixes(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U
    };
    static const unsigned char odd[] = {
        0xf3U, 0x99U, 0x9cU, 0x59U, 0x42U, 0xfbU, 0xe6U, 0x50U,
        0x15U, 0x7dU, 0x6eU, 0x92U, 0x8aU, 0x57U, 0x4aU, 0x49U
    };
    static const unsigned char ordinary[] = {
        0xcdU, 0x77U, 0xc5U, 0x58U, 0x5fU, 0x4dU, 0x54U, 0x7fU,
        0xf3U, 0x9eU, 0xfcU, 0x9cU, 0x09U, 0x95U, 0xb7U, 0x37U
    };
    static const unsigned char detail[] = {
        0xc7U, 0x43U, 0xf5U, 0xb7U, 0xcbU, 0xdeU, 0xd0U, 0x78U,
        0xe8U, 0xfdU, 0x2dU, 0x87U, 0x60U, 0xdaU, 0x52U, 0x31U
    };
    check_prefix("tiny", tiny, sizeof(tiny), 16U, 16U, 4U,
                 AV2H_BLOCK_16X16, 4U, 0U, 4U,
                 AV2H_PARTITION_HORIZONTAL);
    check_prefix("odd", odd, sizeof(odd), 17U, 15U, 3U,
                 AV2H_BLOCK_32X16, 8U, 8U, 8U,
                 AV2H_PARTITION_VERTICAL);
    check_prefix("ordinary", ordinary, sizeof(ordinary), 160U, 120U, 0U,
                 AV2H_BLOCK_64X64, 12U, 24U, 12U,
                 AV2H_PARTITION_HORIZONTAL);
    check_prefix("high-detail", detail, sizeof(detail), 96U, 80U, 0U,
                 AV2H_BLOCK_64X64, 12U, 24U, 12U,
                 AV2H_PARTITION_HORIZONTAL);
}

static void check_forced_path(uint16_t mi_rows,
                              uint16_t mi_columns,
                              const Av2hBlockSize *blocks,
                              const Av2hPartitionType *partitions,
                              size_t count,
                              Av2hBlockSize next) {
    Av2hForcedPartitionPath path;
    size_t index;
    CHECK(av2h_partition_forced_path(
              mi_rows, mi_columns,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 },
              &path) == AV2H_OK);
    CHECK(path.step_count == count);
    CHECK(path.next.row == 0U && path.next.column == 0U);
    CHECK(path.next.block_size == next);
    for (index = 0U; index < count && index < path.step_count; ++index) {
        CHECK(path.steps[index].node.block_size == blocks[index]);
        CHECK(path.steps[index].partition == partitions[index]);
    }
}

static void test_forced_geometry(void) {
    static const Av2hBlockSize tiny_blocks[] = {
        AV2H_BLOCK_64X64, AV2H_BLOCK_64X32,
        AV2H_BLOCK_32X32, AV2H_BLOCK_32X16
    };
    static const Av2hPartitionType tiny_partitions[] = {
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT,
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT
    };
    static const Av2hBlockSize odd_blocks[] = {
        AV2H_BLOCK_64X64, AV2H_BLOCK_64X32, AV2H_BLOCK_32X32
    };
    static const Av2hPartitionType odd_partitions[] = {
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT, AV2H_PARTITION_HORZ
    };
    Av2hForcedPartitionPath right_edge_path;
    size_t split_context;
    size_t rect_context;
    size_t extension_context;
    check_forced_path(4U, 4U, tiny_blocks, tiny_partitions, 4U,
                      AV2H_BLOCK_16X16);
    check_forced_path(4U, 6U, odd_blocks, odd_partitions, 3U,
                      AV2H_BLOCK_32X16);
    check_forced_path(30U, 40U, 0, 0, 0U, AV2H_BLOCK_64X64);
    check_forced_path(20U, 24U, 0, 0, 0U, AV2H_BLOCK_64X64);

        CHECK(av2h_partition_forced_path(
              84U, 84U,
              (Av2hPartitionNode){ 0U, 80U, AV2H_BLOCK_64X64 },
              &right_edge_path) == AV2H_OK);
        CHECK(right_edge_path.step_count == 1U);
        CHECK(right_edge_path.next.row == 0U);
        CHECK(right_edge_path.next.column == 80U);
            CHECK(right_edge_path.next.block_size == AV2H_BLOCK_32X64);
            CHECK(right_edge_path.steps[0].node.block_size == AV2H_BLOCK_64X64);
            CHECK(right_edge_path.steps[0].partition == AV2H_PARTITION_VERT);
    {
        Av2hPartitionContextState contexts;
        uint8_t luma_above[84U];
        uint8_t luma_left[84U];
        uint8_t chroma_above[84U];
        uint8_t chroma_left[84U];
        CHECK(av2h_partition_context_state_init(
                  &contexts, luma_above, luma_left,
                  chroma_above, chroma_left, 84U, 84U) == AV2H_OK);
        CHECK(av2h_partition_contexts_from_state(
                  &contexts, 0U,
                  (Av2hPartitionNode){ 0U, 80U, AV2H_BLOCK_32X64 },
                  AV2H_PARTITION_VERTICAL, &split_context, &rect_context,
                  &extension_context) == AV2H_OK);
        CHECK(split_context == 12U && rect_context == 16U &&
              extension_context == 12U);
    }

    CHECK(av2h_partition_contexts(AV2H_BLOCK_16X16, 0U, 0U, 0U, 0U,
              &split_context, &rect_context, &extension_context) == AV2H_OK);
    CHECK(split_context == 4U && rect_context == 0U &&
          extension_context == 4U);
    CHECK(av2h_partition_contexts(AV2H_BLOCK_32X16, 0U, 0U, 0U, 0U,
              &split_context, &rect_context, &extension_context) == AV2H_OK);
    CHECK(split_context == 8U && rect_context == 8U &&
          extension_context == 8U);
    CHECK(av2h_partition_contexts(AV2H_BLOCK_64X64, 0U, 0U, 0U, 0U,
              &split_context, &rect_context, &extension_context) == AV2H_OK);
    CHECK(split_context == 12U && rect_context == 24U &&
          extension_context == 12U);
    {
        Av2hForcedPartitionPath path;
        CHECK(av2h_partition_forced_path(
                  16U, 2U,
                  (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_8X64 },
                  &path) == AV2H_OK);
        CHECK(path.step_count == 0U);
        CHECK(path.next.block_size == AV2H_BLOCK_8X64);
    }
}

static void check_first_leaf(const unsigned char *bytes,
                             size_t size,
                             uint16_t width,
                             uint16_t height,
                             const Av2hBlockSize *blocks,
                             const Av2hPartitionType *partitions,
                             const uint8_t *forced,
                             const uint8_t *axis_implied,
                             const uint8_t *extension_implied,
                             const size_t *after_bits,
                             size_t count,
                             Av2hBlockSize leaf) {
    Av2hSpan span = { bytes, size };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs cdfs;
    Av2hFirstLeafPath path;
    Av2hPartitionTraversal traversal;
    Av2hPartitionContextState contexts;
    uint8_t luma_above[64U];
    uint8_t luma_left[64U];
    uint8_t chroma_above[64U];
    uint8_t chroma_left[64U];
    uint8_t has_leaf;
    uint16_t mi_rows;
    uint16_t mi_columns;
    size_t index;
    CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_first_leaf_read(
              &decoder, &cdfs, width, height, 1U, &path) == AV2H_OK);
    CHECK(path.decision_count == count);
    CHECK(path.leaf.row == 0U && path.leaf.column == 0U);
    CHECK(path.leaf.block_size == leaf);
    for (index = 0U; index < count && index < path.decision_count; ++index) {
        CHECK(path.decisions[index].node.block_size == blocks[index]);
        CHECK(path.decisions[index].partition == partitions[index]);
        CHECK(path.decisions[index].forced == forced[index]);
          CHECK(path.decisions[index].axis_implied == axis_implied[index]);
          CHECK(path.decisions[index].extension_implied ==
              extension_implied[index]);
        CHECK(path.decisions[index].after_bits == after_bits[index]);
        if (index != 0U) {
            CHECK(path.decisions[index - 1U].after_bits <=
                  path.decisions[index].after_bits);
        }
    }

    mi_columns = (uint16_t)(2U * (((uint32_t)width + 7U) / 8U));
    mi_rows = (uint16_t)(2U * (((uint32_t)height + 7U) / 8U));
    CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_context_state_init(
              &contexts, luma_above, luma_left, chroma_above, chroma_left,
              64U, 64U) == AV2H_OK);
    CHECK(av2h_partition_traversal_init(
              &traversal, mi_rows, mi_columns,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 }, 0U, 1U, 0U) ==
          AV2H_OK);
    {
        Av2hStatus traversal_status = av2h_partition_traversal_next(
            &traversal, &decoder, &cdfs, &contexts, &path, &has_leaf);
        if (traversal_status != AV2H_OK) {
                fprintf(stderr,
                    "partition fixture %ux%u status=%d node=%u/%u/%u tell=%lu\n",
                    width, height, traversal_status,
                    traversal.diagnostic_node.row,
                    traversal.diagnostic_node.column,
                    traversal.diagnostic_node.block_size,
                    (unsigned long)traversal.diagnostic_before_bits);
        }
        CHECK(traversal_status == AV2H_OK);
    }
    CHECK(has_leaf == 1U);
    CHECK(path.decision_count == count);
    CHECK(path.leaf.row == 0U && path.leaf.column == 0U);
    CHECK(path.leaf.block_size == leaf);
}

static void test_first_leaf_paths(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U
    };
    static const unsigned char odd[] = {
        0xf3U, 0x99U, 0x9cU, 0x59U, 0x42U, 0xfbU, 0xe6U, 0x50U,
        0x15U, 0x7dU, 0x6eU, 0x92U, 0x8aU, 0x57U, 0x4aU, 0x49U
    };
    static const unsigned char ordinary[] = {
        0xcdU, 0x77U, 0xc5U, 0x58U, 0x5fU, 0x4dU, 0x54U, 0x7fU,
        0xf3U, 0x9eU, 0xfcU, 0x9cU, 0x09U, 0x95U, 0xb7U, 0x37U
    };
    static const unsigned char detail[] = {
        0xc7U, 0x43U, 0xf5U, 0xb7U, 0xcbU, 0xdeU, 0xd0U, 0x78U,
        0xe8U, 0xfdU, 0x2dU, 0x87U, 0x60U, 0xdaU, 0x52U, 0x31U
    };
    static const Av2hBlockSize tiny_blocks[] = {
        AV2H_BLOCK_64X64, AV2H_BLOCK_64X32, AV2H_BLOCK_32X32,
        AV2H_BLOCK_32X16, AV2H_BLOCK_16X16, AV2H_BLOCK_16X8,
        AV2H_BLOCK_16X4, AV2H_BLOCK_8X4
    };
    static const Av2hPartitionType tiny_parts[] = {
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT, AV2H_PARTITION_HORZ,
        AV2H_PARTITION_VERT, AV2H_PARTITION_HORZ, AV2H_PARTITION_HORZ,
        AV2H_PARTITION_VERT, AV2H_PARTITION_NONE
    };
    static const uint8_t tiny_forced[] = { 1U, 1U, 1U, 1U, 0U, 0U, 0U, 0U };
    static const uint8_t tiny_axis_implied[] = {
        1U, 1U, 1U, 1U, 0U, 0U, 1U, 1U
    };
    static const uint8_t tiny_extension_implied[] = {
        1U, 1U, 1U, 1U, 0U, 1U, 1U, 1U
    };
    static const size_t tiny_after_bits[] = {
        1U, 1U, 1U, 1U, 5U, 8U, 11U, 11U
    };
    static const Av2hBlockSize odd_blocks[] = {
        AV2H_BLOCK_64X64, AV2H_BLOCK_64X32, AV2H_BLOCK_32X32,
        AV2H_BLOCK_32X16, AV2H_BLOCK_16X16, AV2H_BLOCK_16X8,
        AV2H_BLOCK_16X4, AV2H_BLOCK_8X4
    };
    static const Av2hPartitionType odd_parts[] = {
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT, AV2H_PARTITION_HORZ,
        AV2H_PARTITION_VERT, AV2H_PARTITION_HORZ, AV2H_PARTITION_HORZ,
        AV2H_PARTITION_VERT, AV2H_PARTITION_NONE
    };
    static const uint8_t odd_forced[] = { 1U, 1U, 1U, 0U, 0U, 0U, 0U, 0U };
    static const uint8_t odd_axis_implied[] = {
        1U, 1U, 1U, 0U, 0U, 0U, 1U, 1U
    };
    static const uint8_t odd_extension_implied[] = {
        1U, 1U, 1U, 0U, 0U, 1U, 1U, 1U
    };
    static const size_t odd_after_bits[] = {
        1U, 1U, 1U, 4U, 7U, 11U, 13U, 13U
    };
    static const Av2hBlockSize ordinary_blocks[] = {
        AV2H_BLOCK_64X64, AV2H_BLOCK_64X32, AV2H_BLOCK_32X32
    };
    static const Av2hPartitionType ordinary_parts[] = {
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT, AV2H_PARTITION_NONE
    };
    static const uint8_t ordinary_forced[] = { 0U, 0U, 0U };
    static const uint8_t ordinary_axis_implied[] = { 0U, 0U, 1U };
    static const uint8_t ordinary_extension_implied[] = { 0U, 0U, 1U };
    static const size_t ordinary_after_bits[] = { 4U, 6U, 7U };
    static const Av2hBlockSize detail_blocks[] = {
        AV2H_BLOCK_64X64, AV2H_BLOCK_64X32, AV2H_BLOCK_64X16,
        AV2H_BLOCK_32X16, AV2H_BLOCK_32X8, AV2H_BLOCK_16X8,
        AV2H_BLOCK_8X8
    };
    static const Av2hPartitionType detail_parts[] = {
        AV2H_PARTITION_HORZ, AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT,
        AV2H_PARTITION_HORZ, AV2H_PARTITION_VERT, AV2H_PARTITION_VERT,
        AV2H_PARTITION_NONE
    };
    static const uint8_t detail_forced[] = { 0U, 0U, 0U, 0U, 0U, 0U, 0U };
    static const uint8_t detail_axis_implied[] = {
        0U, 0U, 0U, 0U, 0U, 0U, 1U
    };
    static const uint8_t detail_extension_implied[] = {
        0U, 0U, 0U, 0U, 0U, 0U, 1U
    };
    static const size_t detail_after_bits[] = {
        4U, 6U, 8U, 12U, 14U, 16U, 17U
    };
    check_first_leaf(tiny, sizeof(tiny), 16U, 16U, tiny_blocks, tiny_parts,
                     tiny_forced, tiny_axis_implied, tiny_extension_implied,
                     tiny_after_bits, 8U, AV2H_BLOCK_8X4);
    check_first_leaf(odd, sizeof(odd), 17U, 15U, odd_blocks, odd_parts,
                     odd_forced, odd_axis_implied, odd_extension_implied,
                     odd_after_bits, 8U, AV2H_BLOCK_8X4);
    check_first_leaf(ordinary, sizeof(ordinary), 160U, 120U,
                     ordinary_blocks, ordinary_parts, ordinary_forced,
                     ordinary_axis_implied, ordinary_extension_implied,
                     ordinary_after_bits, 3U, AV2H_BLOCK_32X32);
    check_first_leaf(detail, sizeof(detail), 96U, 80U, detail_blocks,
                     detail_parts, detail_forced, detail_axis_implied,
                     detail_extension_implied, detail_after_bits, 7U,
                     AV2H_BLOCK_8X8);
}

static void test_context_state(void) {
    uint8_t luma_above[16U];
    uint8_t luma_left[16U];
    uint8_t chroma_above[16U];
    uint8_t chroma_left[16U];
    Av2hPartitionContextState state;
    size_t split_context;
    size_t rect_context;
    size_t extension_context;
    CHECK(av2h_partition_context_state_init(
              &state, luma_above, luma_left, chroma_above, chroma_left,
              16U, 16U) == AV2H_OK);
    CHECK(av2h_partition_context_state_update(
              &state, 0U,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_8X4 }) == AV2H_OK);
    CHECK(av2h_partition_contexts_from_state(
              &state, 0U,
              (Av2hPartitionNode){ 0U, 2U, AV2H_BLOCK_8X4 },
              AV2H_PARTITION_VERTICAL, &split_context, &rect_context,
              &extension_context) == AV2H_OK);
    CHECK(split_context == 2U);
    CHECK(rect_context == 2U);
    CHECK(extension_context == 0U);

    CHECK(av2h_partition_context_state_update(
              &state, 0U,
              (Av2hPartitionNode){ 0U, 2U, AV2H_BLOCK_4X4 }) == AV2H_OK);
    CHECK(av2h_partition_context_state_update(
              &state, 0U,
              (Av2hPartitionNode){ 0U, 3U, AV2H_BLOCK_4X4 }) == AV2H_OK);
    CHECK(av2h_partition_contexts_from_state(
              &state, 0U,
              (Av2hPartitionNode){ 1U, 0U, AV2H_BLOCK_16X4 },
              AV2H_PARTITION_VERTICAL, &split_context, &rect_context,
              &extension_context) == AV2H_OK);
    CHECK(split_context == 45U);
    CHECK(rect_context == 57U);
    CHECK(extension_context == 45U);

    CHECK(av2h_partition_context_state_update(
              &state, 0U,
              (Av2hPartitionNode){ 1U, 0U, AV2H_BLOCK_8X4 }) == AV2H_OK);
    CHECK(av2h_partition_contexts_from_state(
              &state, 0U,
              (Av2hPartitionNode){ 1U, 2U, AV2H_BLOCK_8X4 },
              AV2H_PARTITION_VERTICAL, &split_context, &rect_context,
              &extension_context) == AV2H_OK);
    CHECK(split_context == 3U);
    CHECK(rect_context == 3U);
    CHECK(extension_context == 3U);
}

static void test_traversal(void) {
    static const unsigned char bytes[] = { 0U, 0U, 0U, 0U };
    uint8_t luma_above[4U];
    uint8_t luma_left[4U];
    uint8_t chroma_above[4U];
    uint8_t chroma_left[4U];
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs cdfs;
    Av2hPartitionContextState contexts;
    Av2hPartitionTraversal traversal;
    Av2hFirstLeafPath path;
    uint8_t has_leaf;

    CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_partition_context_state_init(
              &contexts, luma_above, luma_left, chroma_above, chroma_left,
              4U, 4U) == AV2H_OK);
    CHECK(av2h_partition_traversal_init(
              &traversal, 1U, 1U,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_16X16 }, 0U, 1U, 0U) ==
          AV2H_OK);
    CHECK(av2h_partition_traversal_next(
              &traversal, &decoder, &cdfs, &contexts, &path, &has_leaf) ==
          AV2H_OK);
    CHECK(has_leaf == 1U);
    CHECK(path.decision_count == 4U);
    CHECK(path.leaf.row == 0U && path.leaf.column == 0U);
    CHECK(path.leaf.block_size == AV2H_BLOCK_4X4);
    CHECK(av2h_partition_traversal_next(
              &traversal, &decoder, &cdfs, &contexts, &path, &has_leaf) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_partition_traversal_commit_leaf(
              &traversal, &contexts,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_8X4 }) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_partition_traversal_commit_leaf(
              &traversal, &contexts, path.leaf) == AV2H_OK);
    CHECK(luma_above[0] == 63U);
    CHECK(luma_left[0] == 63U);
    CHECK(av2h_partition_traversal_next(
              &traversal, &decoder, &cdfs, &contexts, &path, &has_leaf) ==
          AV2H_OK);
    CHECK(has_leaf == 0U);
    CHECK(av2h_partition_traversal_init(
              &traversal, 0U, 1U,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_4X4 }, 0U, 1U, 0U) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_superblock_roots(void) {
    static const uint16_t rows[] = { 0U, 0U, 0U, 16U, 16U, 16U };
    static const uint16_t columns[] = { 0U, 16U, 32U, 0U, 16U, 32U };
    Av2hPartitionTraversal traversal;
    Av2hForcedPartitionPath path;
    size_t index;
    CHECK(av2h_partition_traversal_init(
              &traversal, 24U, 48U,
              (Av2hPartitionNode){ rows[0], columns[0],
                                   AV2H_BLOCK_64X64 },
              0U, 1U, 0U) == AV2H_OK);
    for (index = 0U; index < 6U; ++index) {
        Av2hPartitionNode root = {
            rows[index], columns[index], AV2H_BLOCK_64X64
        };
        if (index != 0U) {
            CHECK(av2h_partition_traversal_reset_root(
                      &traversal, root) == AV2H_OK);
        }
        CHECK(traversal.stack_count == 1U);
        CHECK(traversal.stack[0].row == rows[index]);
        CHECK(traversal.stack[0].column == columns[index]);
        CHECK(traversal.diagnostic_node.row == rows[index]);
        CHECK(traversal.diagnostic_node.column == columns[index]);
        CHECK(traversal.diagnostic_before_bits == 0U);
        CHECK(traversal.mi_rows == 24U);
        CHECK(traversal.mi_columns == 48U);
        CHECK(traversal.structure == 0U);
        CHECK(traversal.enable_ext_partitions == 1U);
        CHECK(traversal.enable_uneven_4way_partitions == 0U);
        CHECK(av2h_partition_forced_path(
                  24U, 48U, root, &path) == AV2H_OK);
        CHECK(path.next.row == rows[index]);
        CHECK(path.next.column == columns[index]);
        if (rows[index] == 16U) {
            CHECK(path.step_count == 1U);
            CHECK(path.steps[0].partition == AV2H_PARTITION_HORZ);
            CHECK(path.next.block_size == AV2H_BLOCK_64X32);
        } else {
            CHECK(path.step_count == 0U);
            CHECK(path.next.block_size == AV2H_BLOCK_64X64);
        }
        traversal.stack_count = 0U;
    }
    traversal.has_pending_leaf = 1U;
    CHECK(av2h_partition_traversal_reset_root(
              &traversal,
              (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 }) ==
          AV2H_INVALID_ARGUMENT);
}

static void test_arguments(void) {
    static const unsigned char empty_storage[] = { 0U };
    Av2hSpan empty = { empty_storage, 0U };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs cdfs;
        Av2hOrdinaryPartitionPrefix prefix;
    Av2hFirstPartitionPrefix first;
        CHECK(av2h_partition_ordinary_prefix_read(
              0, &cdfs, 0U, 0U, 0U, &prefix) ==
          AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_ordinary_prefix_read(
              &decoder, 0, 0U, 0U, 0U, &prefix) ==
          AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_ordinary_prefix_read(
              &decoder, &cdfs, 0U, 0U, 0U, 0) ==
          AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_ordinary_prefix_read(
              &decoder, &cdfs, AV2H_PARTITION_CONTEXT_COUNT, 0U, 0U,
              &prefix) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_forced_path(
                  0U, 1U, (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_64X64 },
                  (Av2hForcedPartitionPath *)&prefix) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_contexts(
                  AV2H_BLOCK_INVALID, 0U, 0U, 0U, 0U,
                  &prefix.after_split_bits, &prefix.after_axis_bits,
                  &prefix.after_extension_bits) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_first_prefix_read(
                  &decoder, &cdfs, 0U, 1U, &first) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
        CHECK(av2h_range_init(&decoder, &empty, 1U) == AV2H_TRUNCATED);
        CHECK(av2h_partition_ordinary_prefix_read(
                  &decoder, &cdfs, 4U, 0U, 4U, &prefix) == AV2H_TRUNCATED);
}

int main(void) {
    test_fixture_prefixes();
    test_forced_geometry();
    test_first_leaf_paths();
    test_context_state();
    test_traversal();
    test_superblock_roots();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d partition checks failed\n", failures);
        return 1;
    }
    puts("partition unit: pass");
    return 0;
}