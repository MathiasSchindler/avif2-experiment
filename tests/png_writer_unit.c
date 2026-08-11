#include "shared/png_writer.h"

#include <stdio.h>

#define OUTPUT_CAPACITY 70000U
#define ROW_CAPACITY 65538U
#define INFLATED_CAPACITY 65539U

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t size;
} MemorySink;

typedef struct {
    const uint8_t *pixels;
    size_t row_size;
} RowSource;

static int failures;
static uint8_t output_a[OUTPUT_CAPACITY];
static uint8_t output_b[OUTPUT_CAPACITY];
static uint8_t row_buffer[ROW_CAPACITY];
static uint8_t boundary_pixels[ROW_CAPACITY];
static uint8_t inflated[INFLATED_CAPACITY];

#define CHECK(expression)                                      \
    do {                                                       \
        if (!(expression)) {                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n",     \
                    __FILE__, __LINE__, #expression);           \
            ++failures;                                        \
        }                                                      \
    } while (0)

static uint16_t load_u16le(const uint8_t *bytes) {
    return (uint16_t)(bytes[0U] | (uint16_t)bytes[1U] << 8U);
}

static uint32_t load_u32be(const uint8_t *bytes) {
    return (uint32_t)bytes[0U] << 24U |
        (uint32_t)bytes[1U] << 16U |
        (uint32_t)bytes[2U] << 8U |
        bytes[3U];
}

static uint32_t independent_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xffffffffU;
    size_t index;
    for (index = 0U; index < size; ++index) {
        unsigned int bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = crc & 1U ? (crc >> 1U) ^ 0xedb88320U : crc >> 1U;
        }
    }
    return crc ^ 0xffffffffU;
}

static uint32_t independent_adler32(const uint8_t *data, size_t size) {
    uint32_t first = 1U;
    uint32_t second = 0U;
    size_t index;
    for (index = 0U; index < size; ++index) {
        first = (first + data[index]) % 65521U;
        second = (second + first) % 65521U;
    }
    return second << 16U | first;
}

static Av2hStatus memory_write(
    void *context,
    const uint8_t *data,
    size_t size) {
    MemorySink *sink = (MemorySink *)context;
    if (size > sink->capacity - sink->size) return AV2H_LIMIT_EXCEEDED;
    if (size != 0U) {
        av2h_memory_copy(sink->data + sink->size, data, size);
    }
    sink->size += size;
    return AV2H_OK;
}

static Av2hStatus copy_row(
    void *context,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes) {
    const RowSource *source = (const RowSource *)context;
    if (destination_size_bytes != source->row_size) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(
        destination, source->pixels + (size_t)row * source->row_size,
        source->row_size);
    return AV2H_OK;
}

static void validate_zlib(
    const uint8_t *data,
    size_t size,
    const uint8_t *pixels,
    uint32_t height,
    size_t row_size,
    size_t expected_blocks) {
    size_t cursor = 2U;
    size_t inflated_size = 0U;
    size_t block_count = 0U;
    int final_block = 0;
    uint32_t row;
    CHECK(size >= 11U);
    CHECK(data[0U] == 0x78U && data[1U] == 0x01U);
    while (!final_block && cursor + 5U <= size) {
        uint8_t header = data[cursor++];
        uint16_t length = load_u16le(data + cursor);
        uint16_t inverse_length = load_u16le(data + cursor + 2U);
        cursor += 4U;
        CHECK((header & 0xfeU) == 0U);
        CHECK((uint16_t)~length == inverse_length);
        CHECK((size_t)length <= size - cursor);
        CHECK((size_t)length <= sizeof(inflated) - inflated_size);
        if ((size_t)length > size - cursor ||
            (size_t)length > sizeof(inflated) - inflated_size) {
            return;
        }
        av2h_memory_copy(inflated + inflated_size, data + cursor, length);
        inflated_size += length;
        cursor += length;
        final_block = (header & 1U) != 0U;
        ++block_count;
    }
    CHECK(final_block);
    CHECK(block_count == expected_blocks);
    CHECK(cursor + 4U == size);
    if (cursor + 4U != size) return;
    CHECK(load_u32be(data + cursor) ==
        independent_adler32(inflated, inflated_size));
    CHECK(inflated_size == (row_size + 1U) * height);
    if (inflated_size != (row_size + 1U) * height) return;
    for (row = 0U; row < height; ++row) {
        size_t offset = (size_t)row * (row_size + 1U);
        CHECK(inflated[offset] == 0U);
        CHECK(av2h_memory_compare(
            inflated + offset + 1U,
            pixels + (size_t)row * row_size,
            row_size) == 0);
    }
}

static void validate_png(
    const uint8_t *png,
    size_t png_size,
    uint32_t width,
    uint32_t height,
    uint8_t bit_depth,
    const uint8_t *pixels,
    size_t row_size,
    size_t expected_blocks) {
    static const uint8_t signature[8U] = {
        0x89U, 'P', 'N', 'G', 0x0dU, 0x0aU, 0x1aU, 0x0aU
    };
    size_t cursor = 8U;
    unsigned int chunk_index = 0U;
    CHECK(png_size >= sizeof(signature));
    CHECK(av2h_memory_compare(png, signature, sizeof(signature)) == 0);
    while (cursor + 12U <= png_size) {
        uint32_t length = load_u32be(png + cursor);
        const uint8_t *type = png + cursor + 4U;
        const uint8_t *data = png + cursor + 8U;
        uint32_t stored_crc;
        CHECK((size_t)length <= png_size - cursor - 12U);
        if ((size_t)length > png_size - cursor - 12U) return;
        stored_crc = load_u32be(data + length);
        CHECK(stored_crc == independent_crc32(type, (size_t)length + 4U));
        if (chunk_index == 0U) {
            CHECK(length == 13U);
            CHECK(av2h_memory_compare(type, "IHDR", 4U) == 0);
            CHECK(load_u32be(data) == width);
            CHECK(load_u32be(data + 4U) == height);
            CHECK(data[8U] == bit_depth);
            CHECK(data[9U] == 2U);
            CHECK(data[10U] == 0U && data[11U] == 0U && data[12U] == 0U);
        } else if (chunk_index == 1U) {
            CHECK(av2h_memory_compare(type, "IDAT", 4U) == 0);
            validate_zlib(
                data, length, pixels, height, row_size, expected_blocks);
        } else if (chunk_index == 2U) {
            CHECK(length == 0U);
            CHECK(av2h_memory_compare(type, "IEND", 4U) == 0);
        } else {
            CHECK(0);
        }
        cursor += (size_t)length + 12U;
        ++chunk_index;
    }
    CHECK(cursor == png_size);
    CHECK(chunk_index == 3U);
}

static size_t write_image(
    const uint8_t *pixels,
    uint32_t width,
    uint32_t height,
    uint8_t bit_depth,
    uint8_t *output,
    size_t output_capacity) {
    size_t row_size = (size_t)width * 3U * (bit_depth / 8U);
    RowSource source = { pixels, row_size };
    Av2hPngRgbImage image = {
        width, height, bit_depth, copy_row, &source
    };
    MemorySink sink = { output, output_capacity, 0U };
    CHECK(av2h_png_write_rgb(
        &image, row_buffer, sizeof(row_buffer),
        memory_write, &sink) == AV2H_OK);
    return sink.size;
}

static void test_small_images(void) {
    static const uint8_t rgb8[] = {
        0U, 1U, 2U, 10U, 20U, 30U, 250U, 251U, 252U,
        9U, 8U, 7U, 128U, 64U, 32U, 255U, 0U, 127U
    };
    static const uint8_t rgb16be[] = {
        0x00U, 0x01U, 0x12U, 0x34U, 0xabU, 0xcdU,
        0xffU, 0xffU, 0x80U, 0x00U, 0x03U, 0xffU
    };
    size_t first_size = write_image(
        rgb8, 3U, 2U, 8U, output_a, sizeof(output_a));
    size_t second_size = write_image(
        rgb8, 3U, 2U, 8U, output_b, sizeof(output_b));
    CHECK(first_size == second_size);
    CHECK(av2h_memory_compare(output_a, output_b, first_size) == 0);
    validate_png(output_a, first_size, 3U, 2U, 8U, rgb8, 9U, 1U);
    first_size = write_image(
        rgb16be, 2U, 1U, 16U, output_a, sizeof(output_a));
    validate_png(output_a, first_size, 2U, 1U, 16U, rgb16be, 12U, 1U);
}

static void test_stored_block_boundary(void) {
    size_t index;
    size_t output_size;
    for (index = 0U; index < sizeof(boundary_pixels); ++index) {
        boundary_pixels[index] = (uint8_t)(index * 37U + 11U);
    }
    output_size = write_image(
        boundary_pixels, 21846U, 1U, 8U,
        output_a, sizeof(output_a));
    validate_png(
        output_a, output_size, 21846U, 1U, 8U,
        boundary_pixels, sizeof(boundary_pixels), 2U);
    CHECK(load_u16le(output_a + 44U) == 65535U);
}

static void test_rejections(void) {
    static const uint8_t pixel[3U] = { 1U, 2U, 3U };
    RowSource source = { pixel, sizeof(pixel) };
    Av2hPngRgbImage image = { 1U, 1U, 12U, copy_row, &source };
    MemorySink sink = { output_a, sizeof(output_a), 0U };
    CHECK(av2h_png_write_rgb(
        &image, row_buffer, sizeof(row_buffer),
        memory_write, &sink) == AV2H_INVALID_ARGUMENT);
    image.bit_depth = 8U;
    image.width = 0x80000000U;
    CHECK(av2h_png_write_rgb(
        &image, row_buffer, sizeof(row_buffer),
        memory_write, &sink) == AV2H_INVALID_ARGUMENT);
    image.width = 1U;
    CHECK(av2h_png_write_rgb(
        &image, row_buffer, 2U,
        memory_write, &sink) == AV2H_OVERFLOW);
    sink.capacity = 1U;
    CHECK(av2h_png_write_rgb(
        &image, row_buffer, sizeof(row_buffer),
        memory_write, &sink) == AV2H_LIMIT_EXCEEDED);
}

int main(void) {
    test_small_images();
    test_stored_block_boundary();
    test_rejections();
    if (failures != 0) return 1;
    puts("PNG writer unit: pass");
    return 0;
}