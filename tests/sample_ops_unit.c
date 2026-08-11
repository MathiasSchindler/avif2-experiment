#include "decoder/internal/sample_ops.h"

#include <stdio.h>

static int failures;

#define CHECK(expression)                                      \
    do {                                                       \
        if (!(expression)) {                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n",     \
                    __FILE__, __LINE__, #expression);           \
            ++failures;                                        \
        }                                                      \
    } while (0)

static int row_padding_is(
    const uint8_t *plane, size_t stride_samples,
    size_t width, size_t height, uint8_t value) {
    size_t row;
    for (row = 0U; row < height; ++row) {
        size_t column;
        for (column = width; column < stride_samples; ++column) {
            if (plane[row * stride_samples + column] != value) return 0;
        }
    }
    return 1;
}

static int row_padding_is_u16(
    const uint16_t *plane, size_t stride_samples,
    size_t width, size_t height, uint16_t value) {
    size_t row;
    for (row = 0U; row < height; ++row) {
        size_t column;
        for (column = width; column < stride_samples; ++column) {
            if (plane[row * stride_samples + column] != value) return 0;
        }
    }
    return 1;
}

static void test_fill_copy_and_crop(void) {
    uint8_t source[3U * 6U];
    uint8_t copy[3U * 7U];
    uint8_t crop[3U * 4U + 2U];
    size_t row;
    size_t column;
    av2h_memory_fill(source, 0xa5U, sizeof(source));
    av2h_memory_fill(copy, 0x5aU, sizeof(copy));
    av2h_memory_fill(crop, 0xccU, sizeof(crop));
    av2h_plane_fill_u8(source, 6U, 0U, 3U, 7U);
    av2h_plane_fill_u8(source, 6U, 4U, 0U, 7U);
    CHECK(row_padding_is(source, 6U, 0U, 3U, 0xa5U));
    av2h_plane_fill_u8(source, 6U, 4U, 3U, 19U);
    for (row = 0U; row < 3U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            CHECK(source[row * 6U + column] == 19U);
        }
    }
    CHECK(row_padding_is(source, 6U, 4U, 3U, 0xa5U));
    av2h_plane_copy_u8(copy, 7U, source, 6U, 4U, 0U);
    CHECK(row_padding_is(copy, 7U, 0U, 3U, 0x5aU));
    av2h_plane_copy_u8(copy, 7U, source, 6U, 4U, 3U);
    for (row = 0U; row < 3U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            CHECK(copy[row * 7U + column] == 19U);
        }
    }
    CHECK(row_padding_is(copy, 7U, 4U, 3U, 0x5aU));
    av2h_visible_crop_u8(&crop[1U], source, 6U, 4U, 3U);
    CHECK(crop[0U] == 0xccU);
    CHECK(crop[sizeof(crop) - 1U] == 0xccU);
    for (row = 0U; row < 12U; ++row) CHECK(crop[row + 1U] == 19U);
}

static void test_load_commit_and_store(void) {
    uint8_t source[3U * 5U] = {
        1U, 2U, 3U, 0xeeU, 0xeeU,
        4U, 5U, 6U, 0xeeU, 0xeeU,
        7U, 8U, 9U, 0xeeU, 0xeeU
    };
    uint8_t destination[3U * 6U];
    av2h_memory_fill(destination, 0xa5U, sizeof(destination));
    CHECK(av2h_sample_load_u8(source, 5U, 1U, 2U) == 8U);
    CHECK(av2h_reconstructed_sample_load_u8(source, 5U, 2U, 1U) == 6U);
    CHECK(av2h_reference_load_u8(source, 5U, 3U, 3U, 99U, 0U) == 3U);
    CHECK(av2h_reference_load_u8(source, 5U, 3U, 3U, 0U, 99U) == 7U);
    av2h_predictor_commit_u8(destination, 6U, source, 5U, 3U, 3U);
    CHECK(av2h_sample_load_u8(destination, 6U, 2U, 2U) == 9U);
    CHECK(row_padding_is(destination, 6U, 3U, 3U, 0xa5U));
    av2h_sample_clip_store_u8(destination, 6U, 0U, 1U, -9);
    av2h_sample_clip_store_u8(destination, 6U, 1U, 1U, 300);
    av2h_sample_store_u8(destination, 6U, 2U, 1U, 73U);
    CHECK(av2h_sample_load_u8(destination, 6U, 0U, 1U) == 0U);
    CHECK(av2h_sample_load_u8(destination, 6U, 1U, 1U) == 255U);
    CHECK(av2h_sample_load_u8(destination, 6U, 2U, 1U) == 73U);
}

static void test_u16_operations(void) {
    const Av2hSampleDomain domain = { 10U, 2U, 512U, 1023U };
    uint16_t source[3U * 6U];
    uint16_t copy[3U * 7U];
    uint16_t crop[3U * 4U + 2U];
    size_t row;
    size_t column;
    for (row = 0U; row < sizeof(source) / sizeof(source[0]); ++row) {
        source[row] = 0xa5a5U;
    }
    for (row = 0U; row < sizeof(copy) / sizeof(copy[0]); ++row) {
        copy[row] = 0x5a5aU;
    }
    for (row = 0U; row < sizeof(crop) / sizeof(crop[0]); ++row) {
        crop[row] = 0xccccU;
    }
    av2h_plane_fill_u16(source, 6U, 4U, 3U, 700U);
    for (row = 0U; row < 3U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            CHECK(source[row * 6U + column] == 700U);
        }
    }
    CHECK(row_padding_is_u16(source, 6U, 4U, 3U, 0xa5a5U));
    av2h_plane_copy_u16(copy, 7U, source, 6U, 4U, 3U);
    CHECK(row_padding_is_u16(copy, 7U, 4U, 3U, 0x5a5aU));
    av2h_visible_crop_u16(&crop[1U], source, 6U, 4U, 3U);
    CHECK(crop[0U] == 0xccccU);
    CHECK(crop[sizeof(crop) / sizeof(crop[0]) - 1U] == 0xccccU);
    for (row = 0U; row < 12U; ++row) CHECK(crop[row + 1U] == 700U);

    CHECK(av2h_sample_load_u16(source, 6U, 2U, 1U) == 700U);
    CHECK(av2h_reconstructed_sample_load_u16(source, 6U, 3U, 2U) == 700U);
    CHECK(av2h_reference_load_u16(source, 6U, 4U, 3U, 99U, 0U) == 700U);
    CHECK(av2h_reference_load_u16(source, 6U, 4U, 3U, 0U, 99U) == 700U);
    av2h_predictor_commit_u16(copy, 7U, source, 6U, 4U, 3U);
    av2h_sample_clip_store_u16(copy, 7U, 0U, 1U, -1, &domain);
    av2h_sample_clip_store_u16(copy, 7U, 1U, 1U, 2048, &domain);
    av2h_sample_store_u16(copy, 7U, 2U, 1U, 512U);
    CHECK(av2h_sample_load_u16(copy, 7U, 0U, 1U) == 0U);
    CHECK(av2h_sample_load_u16(copy, 7U, 1U, 1U) == 1023U);
    CHECK(av2h_sample_load_u16(copy, 7U, 2U, 1U) == 512U);
    CHECK(row_padding_is_u16(copy, 7U, 4U, 3U, 0x5a5aU));
}

int main(void) {
    test_fill_copy_and_crop();
    test_load_commit_and_store();
    test_u16_operations();
    if (failures != 0) return 1;
    puts("sample ops unit: pass");
    return 0;
}