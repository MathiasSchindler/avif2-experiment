#include "decoder/tile.h"

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

static void check_vector(const unsigned char *header,
                         size_t header_size,
                         size_t uncompressed_header_bits) {
    unsigned char payload[32];
    Av2hTileGroup group;
    size_t index;
    CHECK(header_size + 1U <= sizeof(payload));
    for (index = 0U; index < header_size; ++index) payload[index] = header[index];
    payload[header_size] = 0x5aU;
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ payload, header_size + 1U },
              uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(group.header_bits == uncompressed_header_bits + 1U);
    CHECK(group.header_size == header_size);
    CHECK(group.entropy.data == payload + header_size);
    CHECK(group.entropy.size == 1U && group.entropy.data[0] == 0x5aU);
}

static void test_fixture_offsets(void) {
    static const unsigned char tiny[] = {
        0xe3U, 0x00U, 0x33U, 0xd0U, 0xb8U, 0x70U, 0x00U
    };
    static const unsigned char odd[] = {
        0xeeU, 0xc0U, 0x0cU, 0xf7U, 0xc2U, 0x89U, 0xc2U, 0x93U,
        0xcfU, 0xffU, 0xc1U, 0x00U, 0x00U, 0xffU, 0x2aU, 0x52U,
        0x67U, 0xd6U, 0x80U
    };
    static const unsigned char ordinary[] = {
        0xeeU, 0x90U, 0x03U, 0xe9U, 0x55U, 0x0aU, 0x27U,
        0x50U, 0x61U, 0xf7U, 0x27U, 0x9bU, 0x00U
    };
    static const unsigned char detail[] = {
        0xeeU, 0x90U, 0x03U, 0xc2U, 0x2aU,
        0x16U, 0xbcU, 0xc0U, 0x00U
    };
    check_vector(tiny, sizeof(tiny), 49U);
    check_vector(odd, sizeof(odd), 148U);
    check_vector(ordinary, sizeof(ordinary), 101U);
    check_vector(detail, sizeof(detail), 65U);
}

static void test_failures(void) {
    static const unsigned char not_first[] = { 0x00U, 0x80U };
    static const unsigned char header_only[] = { 0x80U };
    Av2hTileGroup group;
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ not_first, sizeof(not_first) }, 0U,
              &group) == AV2H_UNSUPPORTED);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ header_only, sizeof(header_only) }, 0U,
              &group) == AV2H_TRUNCATED);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ header_only, sizeof(header_only) }, SIZE_MAX,
              &group) == AV2H_OVERFLOW);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ 0, 1U }, 0U, &group) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ header_only, sizeof(header_only) }, 0U,
              0) == AV2H_INVALID_ARGUMENT);
}

int main(void) {
    test_fixture_offsets();
    test_failures();
    if (failures != 0) {
        fprintf(stderr, "%d tile checks failed\n", failures);
        return 1;
    }
    puts("tile unit: pass");
    return 0;
}