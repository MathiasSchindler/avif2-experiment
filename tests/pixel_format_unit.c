#include "pixel_format.h"

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

static void check_common_8bit(const Av2hPixelFormat *format,
                              uint8_t chroma_format_idc,
                              unsigned int plane_count) {
    CHECK(av2h_pixel_format_is_valid(format));
    CHECK(format->chroma_format_idc == chroma_format_idc);
    CHECK(format->bit_depth == 8U);
    CHECK(format->bytes_per_sample == 1U);
    CHECK(format->sample_storage == (uint8_t)AV2H_SAMPLE_STORAGE_U8);
    CHECK(format->byte_order == (uint8_t)AV2H_BYTE_ORDER_NOT_APPLICABLE);
    CHECK(format->plane_order == (uint8_t)AV2H_PLANE_ORDER_YUV);
    CHECK(av2h_pixel_format_plane_count(format) == plane_count);
}

static void test_supported_formats(void) {
    Av2hPixelFormat format;
    CHECK(av2h_pixel_format_init_8bit(0U, &format));
    check_common_8bit(&format, 0U, 3U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 0U) == 17U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 1U) == 9U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 15U, 2U) == 8U);

    CHECK(av2h_pixel_format_init_8bit(1U, &format));
    check_common_8bit(&format, 1U, 1U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 0U) == 17U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 1U) == 0U);

    CHECK(av2h_pixel_format_init_8bit(2U, &format));
    check_common_8bit(&format, 2U, 3U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 1U) == 17U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 15U, 2U) == 15U);

    CHECK(av2h_pixel_format_init_10bit_420(&format));
    CHECK(av2h_pixel_format_is_valid(&format));
    CHECK(format.chroma_format_idc == 0U);
    CHECK(format.bit_depth == 10U);
    CHECK(format.bytes_per_sample == 2U);
    CHECK(format.sample_storage == (uint8_t)AV2H_SAMPLE_STORAGE_U16);
    CHECK(format.byte_order == (uint8_t)AV2H_BYTE_ORDER_LITTLE_ENDIAN);
    CHECK(format.plane_order == (uint8_t)AV2H_PLANE_ORDER_YUV);
    CHECK(av2h_pixel_format_plane_count(&format) == 3U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 0U) == 17U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 17U, 1U) == 9U);
    CHECK(av2h_pixel_format_plane_dimension(&format, 15U, 2U) == 8U);
}

static void test_invalid_formats(void) {
    Av2hPixelFormat valid;
    Av2hPixelFormat changed;
    CHECK(!av2h_pixel_format_init_8bit(3U, &changed));
    CHECK(!av2h_pixel_format_init_8bit(0U, 0));
    CHECK(!av2h_pixel_format_init_10bit_420(0));
    CHECK(!av2h_pixel_format_is_valid(0));
    CHECK(av2h_pixel_format_plane_count(0) == 0U);
    CHECK(av2h_pixel_format_plane_dimension(0, 17U, 0U) == 0U);
    CHECK(av2h_pixel_format_init_8bit(0U, &valid));

    changed = valid;
    changed.chroma_format_idc = 3U;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.bit_depth = 10U;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.bytes_per_sample = 2U;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.sample_storage = (uint8_t)AV2H_SAMPLE_STORAGE_U16;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.byte_order = (uint8_t)AV2H_BYTE_ORDER_LITTLE_ENDIAN;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.plane_order = (uint8_t)AV2H_PLANE_ORDER_UNKNOWN;
    CHECK(!av2h_pixel_format_is_valid(&changed));

    CHECK(av2h_pixel_format_init_10bit_420(&valid));
    changed = valid;
    changed.chroma_format_idc = 1U;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.bit_depth = 12U;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.bytes_per_sample = 1U;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.sample_storage = (uint8_t)AV2H_SAMPLE_STORAGE_U8;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    changed = valid;
    changed.byte_order = (uint8_t)AV2H_BYTE_ORDER_BIG_ENDIAN;
    CHECK(!av2h_pixel_format_is_valid(&changed));
    CHECK(av2h_pixel_format_equal(&valid, &valid));
    CHECK(!av2h_pixel_format_equal(&valid, &changed));
    CHECK(!av2h_pixel_format_equal(0, &valid));
}

int main(void) {
    test_supported_formats();
    test_invalid_formats();
    if (failures != 0) return 1;
    puts("pixel format unit: pass");
    return 0;
}