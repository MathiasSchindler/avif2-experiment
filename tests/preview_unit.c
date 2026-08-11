#include "preview.h"

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

static void test_format_names(void) {
    Av2hPixelFormat format;
    CHECK(av2h_raw_output_extension_count() == 4U);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension_at(0U), ".i400", 6U) == 0);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension_at(1U), ".i420", 6U) == 0);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension_at(2U), ".i444", 6U) == 0);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension_at(3U), ".yuv420p10le", 13U) == 0);
    CHECK(av2h_raw_output_extension_at(4U) == 0);
    CHECK(av2h_pixel_format_init_8bit(1U, &format));
    CHECK(av2h_memory_compare(
        av2h_raw_output_format_name(&format), "gray8", 6U) == 0);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension(&format), ".i400", 6U) == 0);
    CHECK(av2h_pixel_format_init_8bit(0U, &format));
    CHECK(av2h_memory_compare(
        av2h_raw_output_format_name(&format), "yuv420p", 8U) == 0);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension(&format), ".i420", 6U) == 0);
    CHECK(av2h_pixel_format_init_8bit(2U, &format));
    CHECK(av2h_memory_compare(
        av2h_raw_output_format_name(&format), "yuv444p", 8U) == 0);
    format.bit_depth = 10U;
    format.bytes_per_sample = 2U;
    format.sample_storage = (uint8_t)AV2H_SAMPLE_STORAGE_U16;
    format.byte_order = (uint8_t)AV2H_BYTE_ORDER_LITTLE_ENDIAN;
    CHECK(av2h_memory_compare(
        av2h_raw_output_format_name(&format), "yuv444p10le", 12U) == 0);
    CHECK(av2h_raw_output_extension(&format) == 0);
    format.chroma_format_idc = 0U;
    CHECK(av2h_memory_compare(
        av2h_raw_output_format_name(&format), "yuv420p10le", 12U) == 0);
    CHECK(av2h_memory_compare(
        av2h_raw_output_extension(&format), ".yuv420p10le", 13U) == 0);
}

static void test_i400_rows(void) {
    static const uint8_t planar[] = { 16U, 81U, 235U, 0U, 128U, 255U };
    static const uint8_t expected[] = {
        0U, 0U, 0U, 76U, 76U, 76U, 255U, 255U, 255U
    };
    Av2hPlanarPreview preview;
    uint8_t row[11U];
    av2h_memory_fill(row, 0xa5U, sizeof(row));
    CHECK(av2h_planar_preview_init_tight_8bit(
        planar, 3U, 2U, 1U, AV2H_COLOR_RANGE_LIMITED, 6U,
        &preview) == AV2H_OK);
    CHECK(av2h_planar_to_bgr8_row(
        &preview, 0U, &row[1U], 9U) == AV2H_OK);
    CHECK(row[0U] == 0xa5U && row[10U] == 0xa5U);
    CHECK(av2h_memory_compare(&row[1U], expected, sizeof(expected)) == 0);
}

static void test_i420_and_i444_rows(void) {
    static const uint8_t i420[] = {
        16U, 81U, 145U, 235U, 100U, 200U,
        90U, 240U, 240U, 110U
    };
    static const uint8_t i420_expected[] = {
        0U, 0U, 179U, 0U, 0U, 255U, 255U, 121U, 121U
    };
    static const uint8_t i420_rgb_expected[] = {
        179U, 0U, 0U, 255U, 0U, 0U, 121U, 121U, 255U
    };
    static const uint8_t i444[] = {
        81U, 145U, 235U, 90U, 128U, 240U, 240U, 128U, 110U
    };
    static const uint8_t i444_expected[] = {
        0U, 0U, 255U, 150U, 150U, 150U, 255U, 226U, 226U
    };
    Av2hPlanarPreview preview;
    uint8_t row[9U];
    CHECK(av2h_planar_preview_init_tight_8bit(
        i420, 3U, 2U, 0U, AV2H_COLOR_RANGE_LIMITED, 6U,
        &preview) == AV2H_OK);
    CHECK(av2h_planar_to_bgr8_row(
        &preview, 0U, row, sizeof(row)) == AV2H_OK);
    CHECK(av2h_memory_compare(row, i420_expected, sizeof(row)) == 0);
    CHECK(av2h_planar_to_rgb8_row(
        &preview, 0U, row, sizeof(row)) == AV2H_OK);
    CHECK(av2h_memory_compare(row, i420_rgb_expected, sizeof(row)) == 0);
    CHECK(av2h_planar_preview_init_tight_8bit(
        i444, 3U, 1U, 2U, AV2H_COLOR_RANGE_LIMITED, 6U,
        &preview) == AV2H_OK);
    CHECK(av2h_planar_to_bgr8_row(
        &preview, 0U, row, sizeof(row)) == AV2H_OK);
    CHECK(av2h_memory_compare(row, i444_expected, sizeof(row)) == 0);
    CHECK(av2h_planar_preview_init_tight_8bit(
        i444, 3U, 1U, 2U, AV2H_COLOR_RANGE_FULL, 6U,
        &preview) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_planar_preview_init_tight_8bit(
        i444, 3U, 1U, 2U, AV2H_COLOR_RANGE_LIMITED, 1U,
        &preview) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_planar_preview_init_tight_8bit(
        i444, 3U, 1U, 2U, AV2H_COLOR_RANGE_LIMITED, 6U,
        &preview) == AV2H_OK);
    preview.color_range = AV2H_COLOR_RANGE_FULL;
    CHECK(av2h_planar_to_bgr8_row(
        &preview, 0U, row, sizeof(row)) == AV2H_INVALID_ARGUMENT);
    preview.color_range = AV2H_COLOR_RANGE_LIMITED;
    preview.format.bit_depth = 10U;
    CHECK(av2h_planar_to_bgr8_row(
        &preview, 0U, row, sizeof(row)) == AV2H_INVALID_ARGUMENT);
}

int main(void) {
    test_format_names();
    test_i400_rows();
    test_i420_and_i444_rows();
    if (failures != 0) return 1;
    puts("preview unit: pass");
    return 0;
}