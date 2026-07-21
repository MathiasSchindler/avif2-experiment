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
                         size_t split_context,
                         size_t rect_type_context,
                         size_t extension_context,
                         Av2hPartitionAxis expected_axis) {
    Av2hSpan span = { bytes, size };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs cdfs;
    Av2hOrdinaryPartitionPrefix prefix;
    CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    {
        Av2hStatus status =
            av2h_partition_ordinary_prefix_read(
                &decoder, &cdfs, split_context, rect_type_context,
                extension_context, &prefix);
        if (status != AV2H_OK) {
            fprintf(stderr, "%s: status=%d axis=%d tell=%zu\n", name,
                    status, (int)prefix.axis, av2h_range_tell(&decoder));
            ++failures;
            return;
        }
    }
    CHECK(prefix.axis == expected_axis);
    CHECK(prefix.after_split_bits <= prefix.after_axis_bits);
    CHECK(prefix.after_axis_bits <= prefix.after_extension_bits);
    CHECK(cdfs.do_split[0][split_context][2] == 1U);
    CHECK(cdfs.rect_type[0][rect_type_context][2] == 1U);
    CHECK(cdfs.do_ext_partition[0][extension_context][2] == 1U);
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
    check_prefix("tiny", tiny, sizeof(tiny), 4U, 0U, 4U,
                 AV2H_PARTITION_HORIZONTAL);
    check_prefix("odd", odd, sizeof(odd), 8U, 8U, 8U,
                 AV2H_PARTITION_VERTICAL);
    check_prefix("ordinary", ordinary, sizeof(ordinary), 12U, 24U, 12U,
                 AV2H_PARTITION_HORIZONTAL);
    check_prefix("high-detail", detail, sizeof(detail), 12U, 24U, 12U,
                 AV2H_PARTITION_HORIZONTAL);
}

static void test_arguments(void) {
    static const unsigned char empty_storage[] = { 0U };
    Av2hSpan empty = { empty_storage, 0U };
    Av2hRangeDecoder decoder;
    Av2hPartitionCdfs cdfs;
        Av2hOrdinaryPartitionPrefix prefix;
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
        CHECK(av2h_partition_cdfs_init(&cdfs) == AV2H_OK);
        CHECK(av2h_range_init(&decoder, &empty, 1U) == AV2H_TRUNCATED);
        CHECK(av2h_partition_ordinary_prefix_read(
                  &decoder, &cdfs, 4U, 0U, 4U, &prefix) == AV2H_TRUNCATED);
}

int main(void) {
    test_fixture_prefixes();
    test_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d partition checks failed\n", failures);
        return 1;
    }
    puts("partition unit: pass");
    return 0;
}