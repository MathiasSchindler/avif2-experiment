#include "encoder/image_input.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *data;
    if (file == 0 || fseek(file, 0, SEEK_END) != 0) return 0;
    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) return 0;
    data = (uint8_t *)malloc((size_t)length);
    if (data == 0 || fread(data, 1U, (size_t)length, file) != (size_t)length) {
        free(data);
        return 0;
    }
    fclose(file);
    *size = (size_t)length;
    return data;
}

static int decode(const char *name, EncImageType type, size_t width,
                  size_t height, const uint8_t *expected_y,
                  size_t expected_count) {
    char path[256];
    uint8_t *data;
    uint8_t *output;
    uint8_t *scratch;
    size_t size;
    size_t i;
    EncImageLayout layout;
    EncImage image;
    (void)snprintf(path, sizeof(path), "fixtures/image-input/%s", name);
    data = read_file(path, &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_OK) return 1;
    if (layout.type != type || layout.width != width || layout.height != height)
        return 2;
    output = (uint8_t *)malloc(layout.output_size);
    scratch = layout.scratch_size == 0U ? 0 :
              (uint8_t *)malloc(layout.scratch_size);
    if (output == 0 || (layout.scratch_size != 0U && scratch == 0)) return 3;
    if (enc_image_decode(data, size, output, layout.output_size, scratch,
                         layout.scratch_size, &image) != ENC_OK) return 4;
    if (image.type != type || image.width != width || image.height != height ||
        image.stride[0] != width) return 5;
    for (i = 0U; i < expected_count; ++i) {
        if (image.plane[0][i] != expected_y[i]) return 6;
    }
    if (type == ENC_IMAGE_I444 &&
        (image.plane[1] == 0 || image.plane[2] == 0 ||
         image.plane[1][0] != 100U || image.plane[2][0] != 255U)) return 7;
    free(scratch);
    free(output);
    free(data);
    return 0;
}

static int malformed_tests(void) {
    uint8_t *data;
    uint8_t *copy;
    uint8_t output[45];
    uint8_t scratch[512];
    EncImage image;
    EncImageLayout layout;
    size_t size;
    data = read_file("fixtures/image-input/rgb-filters-dynamic.png", &size);
    if (data == 0) return 1;
    copy = (uint8_t *)malloc(size);
    if (copy == 0) return 2;
    if (enc_image_query(data, size - 1U, &layout) != ENC_SIZE_MISMATCH) return 3;
    for (size_t i = 0U; i < size; ++i) copy[i] = data[i];
    copy[29] ^= 1U;
    if (enc_image_query(copy, size, &layout) != ENC_INVALID_ARGUMENT) return 4;
    if (enc_image_query(data, size, &layout) != ENC_OK) return 5;
    if (enc_image_decode(data, size, output, layout.output_size - 1U, scratch,
                         sizeof(scratch), &image) != ENC_OUT_OF_SPACE) return 6;
    for (size_t i = 0U; i < size; ++i) copy[i] = data[i];
    copy[24] = 16U;
    if (enc_image_query(copy, size, &layout) != ENC_INVALID_ARGUMENT) return 7;
    free(copy);
    free(data);
    data = read_file("fixtures/image-input/invalid-filter.png", &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_OK) return 8;
    if (enc_image_decode(data, size, output, sizeof(output), scratch,
                         sizeof(scratch), &image) != ENC_UNSUPPORTED) return 9;
    free(data);
    data = read_file("fixtures/image-input/invalid-deflate.png", &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_OK) return 10;
    if (enc_image_decode(data, size, output, sizeof(output), scratch,
                         sizeof(scratch), &image) != ENC_INVALID_ARGUMENT)
        return 11;
    free(data);
    data = read_file("fixtures/image-input/unsupported-depth.png", &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_UNSUPPORTED)
        return 12;
    free(data);
    data = read_file("fixtures/image-input/unsupported-interlace.png", &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_UNSUPPORTED)
        return 13;
    free(data);
    data = read_file("fixtures/image-input/overflow-dimensions.png", &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_INVALID_ARGUMENT)
        return 14;
    free(data);
    data = read_file("fixtures/image-input/rgb-bottom-up.bmp", &size);
    if (data == 0) return 15;
    data[30] = 1U;
    if (enc_image_query(data, size, &layout) != ENC_UNSUPPORTED) return 16;
    free(data);
    data = read_file("fixtures/image-input/gray.bmp", &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_OK) return 17;
    data[54U + 10U * 4U] ^= 1U;
    if (enc_image_decode(data, size, output, sizeof(output), scratch,
                         sizeof(scratch), &image) != ENC_UNSUPPORTED) return 18;
    free(data);
    return 0;
}

static int optional_prepared_test(void) {
    const char *path = "../../images/prepared/le896/pitti-ceiling/master.png";
    uint8_t *data;
    uint8_t *output;
    uint8_t *scratch;
    size_t size;
    EncImageLayout layout;
    EncImage image;
    FILE *probe = fopen(path, "rb");
    if (probe == 0) return 0;
    fclose(probe);
    data = read_file(path, &size);
    if (data == 0 || enc_image_query(data, size, &layout) != ENC_OK) return 1;
    if (layout.type != ENC_IMAGE_I444 || layout.width != 896U ||
        layout.height != 896U) return 2;
    output = (uint8_t *)malloc(layout.output_size);
    scratch = (uint8_t *)malloc(layout.scratch_size);
    if (output == 0 || scratch == 0) return 3;
    if (enc_image_decode(data, size, output, layout.output_size, scratch,
                         layout.scratch_size, &image) != ENC_OK) return 4;
    free(scratch);
    free(output);
    free(data);
    return 0;
}

int main(void) {
    static const uint8_t rgb_y[15] =
        {54,182,19,255,0,119,2,5,8,61,181,71,20,53,86};
    static const uint8_t gray_y[6] = {10,20,30,40,50,60};
    static const uint8_t bmp_y[6] = {54,182,19,255,0,119};
    static const uint8_t rgba_y[2] = {54,182};
    static const char *rgb_pngs[] =
        {"rgb-filters-dynamic.png", "rgb-fixed.png", "rgb-stored.png"};
    size_t i;
    int result;
    for (i = 0U; i < sizeof(rgb_pngs) / sizeof(rgb_pngs[0]); ++i) {
        result = decode(rgb_pngs[i], ENC_IMAGE_I444, 3U, 5U, rgb_y, 15U);
        if (result != 0) return 10 + result + (int)i * 10;
    }
    result = decode("gray.png", ENC_IMAGE_I400, 3U, 2U, gray_y, 6U);
    if (result != 0) return 50 + result;
    result = decode("rgba.png", ENC_IMAGE_I444, 2U, 1U, rgba_y, 2U);
    if (result != 0) return 60 + result;
    result = decode("palette.png", ENC_IMAGE_I444, 3U, 1U, bmp_y, 3U);
    if (result != 0) return 70 + result;
    result = decode("rgb-bottom-up.bmp", ENC_IMAGE_I444, 3U, 2U, bmp_y, 6U);
    if (result != 0) return 80 + result;
    result = decode("rgb-top-down.bmp", ENC_IMAGE_I444, 3U, 2U, bmp_y, 6U);
    if (result != 0) return 90 + result;
    result = decode("gray.bmp", ENC_IMAGE_I400, 3U, 2U, gray_y, 6U);
    if (result != 0) return 100 + result;
    result = malformed_tests();
    if (result != 0) return 120 + result;
    result = optional_prepared_test();
    if (result != 0) return 140 + result;
    return 0;
}
