#include "output_layout.h"

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

static void check_plane(const Av2hPlaneLayout *plane,
                        uint32_t width,
                        uint32_t height,
                        size_t row_bytes,
                        size_t offset,
                        size_t size) {
    CHECK(plane->width == width);
    CHECK(plane->height == height);
    CHECK(plane->row_bytes == row_bytes);
    CHECK(plane->offset == offset);
    CHECK(plane->size == size);
}

static void check_odd_layout(uint8_t chroma_format_idc,
                             size_t bytes_per_sample) {
    Av2hPlaneLayout planes[3];
    Av2hError error;
    size_t output_size = 0U;
    size_t luma_size = 17U * 15U * bytes_per_sample;
    size_t chroma_size = 9U * 8U * bytes_per_sample;
    CHECK(av2h_output_layout_calculate(
              17U, 15U, chroma_format_idc, bytes_per_sample,
              planes, &output_size, &error) == AV2H_OK);
    check_plane(&planes[0], 17U, 15U, 17U * bytes_per_sample,
                0U, luma_size);
    if (chroma_format_idc == 1U) {
        check_plane(&planes[1], 0U, 0U, 0U, 0U, 0U);
        check_plane(&planes[2], 0U, 0U, 0U, 0U, 0U);
        CHECK(output_size == luma_size);
    } else if (chroma_format_idc == 0U) {
        check_plane(&planes[1], 9U, 8U, 9U * bytes_per_sample,
                    luma_size, chroma_size);
        check_plane(&planes[2], 9U, 8U, 9U * bytes_per_sample,
                    luma_size + chroma_size, chroma_size);
        CHECK(output_size == luma_size + 2U * chroma_size);
    } else {
        check_plane(&planes[1], 17U, 15U, 17U * bytes_per_sample,
                    luma_size, luma_size);
        check_plane(&planes[2], 17U, 15U, 17U * bytes_per_sample,
                    2U * luma_size, luma_size);
        CHECK(output_size == 3U * luma_size);
    }
}

static void test_one_and_two_byte_layouts(void) {
    uint8_t chroma_format_idc;
    for (chroma_format_idc = 0U; chroma_format_idc <= 2U;
         ++chroma_format_idc) {
        check_odd_layout(chroma_format_idc, 1U);
        check_odd_layout(chroma_format_idc, 2U);
    }
}

static void test_minimum_dimensions(void) {
    static const size_t expected_sizes[3] = { 3U, 1U, 3U };
    Av2hPlaneLayout planes[3];
    Av2hError error;
    size_t output_size;
    uint8_t chroma_format_idc;
    for (chroma_format_idc = 0U; chroma_format_idc <= 2U;
         ++chroma_format_idc) {
        CHECK(av2h_output_layout_calculate(
                  1U, 1U, chroma_format_idc, 1U,
                  planes, &output_size, &error) == AV2H_OK);
        check_plane(&planes[0], 1U, 1U, 1U, 0U, 1U);
        CHECK(output_size == expected_sizes[chroma_format_idc]);
        if (chroma_format_idc == 1U) {
            check_plane(&planes[1], 0U, 0U, 0U, 0U, 0U);
            check_plane(&planes[2], 0U, 0U, 0U, 0U, 0U);
        } else {
            check_plane(&planes[1], 1U, 1U, 1U, 1U, 1U);
            check_plane(&planes[2], 1U, 1U, 1U, 2U, 1U);
        }
    }
}

static void test_wp0_10bit_420_layout(void) {
    Av2hPlaneLayout planes[3];
    size_t output_size = 0U;
    CHECK(av2h_output_layout_calculate(
              16U, 16U, 0U, 2U, planes, &output_size, 0) == AV2H_OK);
    check_plane(&planes[0], 16U, 16U, 32U, 0U, 512U);
    check_plane(&planes[1], 8U, 8U, 16U, 512U, 128U);
    check_plane(&planes[2], 8U, 8U, 16U, 640U, 128U);
    CHECK(output_size == 768U);
}

static void test_maximum_dimensions(void) {
    static const size_t expected_sizes[3] = {
        805306368U, 536870912U, 1610612736U
    };
    Av2hPlaneLayout planes[3];
    size_t output_size;
    uint8_t chroma_format_idc;
    for (chroma_format_idc = 0U; chroma_format_idc <= 2U;
         ++chroma_format_idc) {
        CHECK(av2h_output_layout_calculate(
                  16384U, 16384U, chroma_format_idc, 2U,
                  planes, &output_size, 0) == AV2H_OK);
        CHECK(planes[0].row_bytes == 32768U);
        CHECK(output_size == expected_sizes[chroma_format_idc]);
    }
}

static void test_invalid_inputs(void) {
    Av2hPlaneLayout planes[3];
    Av2hError error;
    size_t output_size;
    CHECK(av2h_output_layout_calculate(
              0U, 1U, 0U, 1U, planes, &output_size, &error) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(error.status == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_QUERY);
    CHECK(av2h_output_layout_calculate(
              1U, 0U, 0U, 1U, planes, &output_size, 0) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_output_layout_calculate(
              1U, 1U, 3U, 1U, planes, &output_size, 0) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_output_layout_calculate(
              1U, 1U, 0U, 0U, planes, &output_size, 0) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_output_layout_calculate(
              1U, 1U, 0U, 1U, 0, &output_size, 0) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_output_layout_calculate(
              1U, 1U, 0U, 1U, planes, 0, 0) ==
          AV2H_INVALID_ARGUMENT);
}

static void expect_overflow(uint32_t width,
                            uint32_t height,
                            size_t bytes_per_sample) {
    Av2hPlaneLayout planes[3];
    Av2hPlaneLayout before[3];
    Av2hError error;
    size_t output_size = 123U;
    av2h_memory_fill(planes, 0xa5U, sizeof(planes));
    av2h_memory_copy(before, planes, sizeof(before));
    CHECK(av2h_output_layout_calculate(
              width, height, 2U, bytes_per_sample,
              planes, &output_size, &error) == AV2H_OVERFLOW);
    CHECK(error.status == AV2H_OVERFLOW);
    CHECK(error.context == AV2H_CONTEXT_QUERY);
    CHECK(av2h_memory_compare(planes, before, sizeof(planes)) == 0);
    CHECK(output_size == 123U);
}

static void test_overflow_paths(void) {
    expect_overflow(2U, 1U, SIZE_MAX);
    expect_overflow(1U, 2U, SIZE_MAX / 2U + 1U);
    expect_overflow(1U, 1U, SIZE_MAX / 2U + 1U);
    expect_overflow(1U, 1U, SIZE_MAX / 3U + 1U);
}

int main(void) {
    test_one_and_two_byte_layouts();
    test_minimum_dimensions();
    test_wp0_10bit_420_layout();
    test_maximum_dimensions();
    test_invalid_inputs();
    test_overflow_paths();
    if (failures != 0) return 1;
    puts("output layout unit: pass");
    return 0;
}