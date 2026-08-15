#include "encoder/input.h"

#include <stdio.h>
#include <stdint.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "input planar: failed line %d: %s\n", __LINE__,   \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void write_bytes(const char *path, size_t size) {
    FILE *file = fopen(path, "wb");
    size_t index;
    CHECK(file != NULL);
    if (file == NULL) return;
    for (index = 0U; index < size; ++index) {
        CHECK(fputc((int)(index & 255U), file) != EOF);
    }
    CHECK(fclose(file) == 0);
}

static void test_geometry(void) {
    uint8_t bytes[64] = {0};
    size_t strides[3] = {3U, 2U, 2U};
    EncPlanarImage image;
    CHECK(enc_planar_image_layout(ENC_PIXEL_FORMAT_I420_8, 3U, 3U, strides,
                                  17U, bytes, &image) == ENC_OK);
    CHECK(image.plane_count == 3U);
    CHECK(image.planes[0].width == 3U && image.planes[0].height == 3U);
    CHECK(image.planes[1].width == 2U && image.planes[1].height == 2U);
    CHECK(image.planes[2].width == 2U && image.planes[2].height == 2U);
    CHECK(image.planes[0].bytes.size == 9U);
    CHECK(image.planes[1].bytes.data == bytes + 9U);
    CHECK(image.planes[2].bytes.data == bytes + 13U);
    CHECK(enc_planar_image_validate(&image) == ENC_OK);

    strides[0] = 4U;
    strides[1] = 2U;
    strides[2] = 2U;
    CHECK(enc_planar_image_layout(ENC_PIXEL_FORMAT_I420_8, 4U, 2U, strides,
                                  12U, bytes, &image) == ENC_OK);
    CHECK(image.planes[1].width == 2U && image.planes[1].height == 1U);

    strides[0] = 5U;
    strides[1] = 6U;
    strides[2] = 7U;
    CHECK(enc_planar_image_layout(ENC_PIXEL_FORMAT_I444_8, 3U, 2U, strides,
                                  36U, bytes, &image) == ENC_OK);
    CHECK(image.planes[0].bytes.size == 10U);
    CHECK(image.planes[1].bytes.size == 12U);
    CHECK(image.planes[2].bytes.size == 14U);
}

static void test_bounds_overflow_alias(void) {
    uint8_t bytes[32] = {0};
    size_t strides[3] = {ENC_INPUT_MAX_DIMENSION, 0U, 0U};
    EncPlanarImage image;
    CHECK(enc_planar_image_layout(
              ENC_PIXEL_FORMAT_I400_8, ENC_INPUT_MAX_DIMENSION,
              ENC_INPUT_MAX_DIMENSION, strides, 0U, NULL, &image) ==
          ENC_SIZE_MISMATCH);
    CHECK(enc_planar_image_layout(
              ENC_PIXEL_FORMAT_I400_8, ENC_INPUT_MAX_DIMENSION + 1U, 1U,
              strides, 0U, NULL, &image) == ENC_INVALID_ARGUMENT);
    strides[0] = SIZE_MAX;
    CHECK(enc_planar_image_layout(ENC_PIXEL_FORMAT_I400_8, 1U, 2U, strides,
                                  0U, NULL, &image) == ENC_OVERFLOW);

    strides[0] = 2U;
    strides[1] = 1U;
    strides[2] = 1U;
    CHECK(enc_planar_image_layout(ENC_PIXEL_FORMAT_I420_8, 2U, 2U, strides,
                                  6U, bytes, &image) == ENC_OK);
    image.planes[1].bytes.data = image.planes[0].bytes.data;
    CHECK(enc_planar_image_validate(&image) == ENC_INVALID_ARGUMENT);
    image.planes[1].bytes.data =
        (const uint8_t *)(uintptr_t)UINTPTR_MAX;
    image.planes[1].bytes.size = 1U;
    CHECK(enc_planar_image_validate(&image) == ENC_OVERFLOW);
}

static void test_names(void) {
    EncPixelFormat format;
    size_t width;
    size_t height;
    CHECK(enc_planar_dimensions_from_prepared_name(
              "images/prepared/le896/id/source-897x599-yuv420p8.i420",
              &format, &width, &height) == ENC_OK);
    CHECK(format == ENC_PIXEL_FORMAT_I420_8 && width == 897U &&
          height == 599U);
    CHECK(enc_planar_dimensions_from_prepared_name(
              "source-3x5-gray8.i400", &format, &width, &height) == ENC_OK);
    CHECK(format == ENC_PIXEL_FORMAT_I400_8);
    CHECK(enc_planar_dimensions_from_prepared_name(
              "640x480.i420", &format, &width, &height) ==
          ENC_INVALID_ARGUMENT);
    CHECK(enc_planar_dimensions_from_prepared_name(
              "source-0640x480-yuv420p8.i420", &format, &width, &height) ==
          ENC_INVALID_ARGUMENT);
    CHECK(enc_planar_dimensions_from_prepared_name(
              "source-640x480-gray8.i420", &format, &width, &height) ==
          ENC_INVALID_ARGUMENT);
}

static void test_loading(void) {
    static const char path[] = "build/input-planar-test.i420";
    size_t strides[3] = {3U, 2U, 2U};
    EncPlanarInput input;
    write_bytes(path, 17U);
    CHECK(enc_planar_input_open(path, ENC_PIXEL_FORMAT_I420_8, 3U, 3U,
                                strides, &input) == ENC_OK);
    CHECK(input.storage.size == 17U);
    CHECK(input.image.planes[2].bytes.data == input.storage.data + 13U);
    CHECK(enc_planar_input_close(&input) == ENC_OK);
    CHECK(input.storage.data == NULL && input.image.plane_count == 0U);
    CHECK(enc_planar_input_close(&input) == ENC_INVALID_ARGUMENT);

    write_bytes(path, 16U);
    CHECK(enc_planar_input_open(path, ENC_PIXEL_FORMAT_I420_8, 3U, 3U,
                                strides, &input) == ENC_SIZE_MISMATCH);
    CHECK(input.storage.data == NULL);
    write_bytes(path, 18U);
    CHECK(enc_planar_input_open(path, ENC_PIXEL_FORMAT_I420_8, 3U, 3U,
                                strides, &input) == ENC_SIZE_MISMATCH);
    CHECK(input.storage.data == NULL);
    CHECK(remove(path) == 0);
}

int main(void) {
    test_geometry();
    test_bounds_overflow_alias();
    test_names();
    test_loading();
    if (failures != 0) return 1;
    puts("input planar: PASS");
    return 0;
}
