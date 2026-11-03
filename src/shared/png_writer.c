#include "png_writer.h"

#include <stddef.h>

#define AV2H_PNG_STORED_BLOCK_MAX 65535U
#define AV2H_PNG_MAX_CHUNK_SIZE 0x7fffffffU

typedef struct {
    Av2hPngWriteCallback callback;
    void *context;
    uint32_t crc;
    Av2hStatus status;
} Av2hPngOutput;

typedef struct {
    uint32_t first;
    uint32_t second;
} Av2hAdler32;

static uint32_t crc32_update(
    uint32_t crc,
    const uint8_t *data,
    size_t size) {
    static const uint32_t table[16U] = {
        0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
        0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
        0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
        0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU
    };
    size_t index;
    for (index = 0U; index < size; ++index) {
        crc ^= data[index];
        crc = (crc >> 4U) ^ table[crc & 0x0fU];
        crc = (crc >> 4U) ^ table[crc & 0x0fU];
    }
    return crc;
}

static void adler32_update(
    Av2hAdler32 *adler,
    const uint8_t *data,
    size_t size) {
    while (size != 0U) {
        size_t chunk = size > 5552U ? 5552U : size;
        size_t index;
        for (index = 0U; index < chunk; ++index) {
            adler->first += data[index];
            adler->second += adler->first;
        }
        adler->first %= 65521U;
        adler->second %= 65521U;
        data += chunk;
        size -= chunk;
    }
}

static void output_raw(
    Av2hPngOutput *output,
    const uint8_t *data,
    size_t size) {
    if (output->status == AV2H_OK && size != 0U) {
        output->status = output->callback(output->context, data, size);
    }
}

static void chunk_data(
    Av2hPngOutput *output,
    const uint8_t *data,
    size_t size) {
    if (output->status == AV2H_OK && size != 0U) {
        output->status = output->callback(output->context, data, size);
        if (output->status == AV2H_OK) {
            output->crc = crc32_update(output->crc, data, size);
        }
    }
}

static void chunk_begin(
    Av2hPngOutput *output,
    uint32_t size,
    const uint8_t type[4U]) {
    uint8_t encoded_size[4U];
    av2h_store_u32be(encoded_size, size);
    output_raw(output, encoded_size, sizeof(encoded_size));
    output->crc = 0xffffffffU;
    chunk_data(output, type, 4U);
}

static void chunk_end(Av2hPngOutput *output) {
    uint8_t encoded_crc[4U];
    av2h_store_u32be(encoded_crc, output->crc ^ 0xffffffffU);
    output_raw(output, encoded_crc, sizeof(encoded_crc));
}

static void write_chunk(
    Av2hPngOutput *output,
    const uint8_t type[4U],
    const uint8_t *data,
    uint32_t size) {
    chunk_begin(output, size, type);
    chunk_data(output, data, size);
    chunk_end(output);
}

static void write_stored_data(
    Av2hPngOutput *output,
    Av2hAdler32 *adler,
    const uint8_t *data,
    size_t size,
    size_t *stream_remaining,
    size_t *block_remaining) {
    while (size != 0U && output->status == AV2H_OK) {
        size_t part;
        if (*block_remaining == 0U) {
            uint16_t block_size = (uint16_t)(
                *stream_remaining > AV2H_PNG_STORED_BLOCK_MAX ?
                    AV2H_PNG_STORED_BLOCK_MAX : *stream_remaining);
            uint8_t header[5U];
            header[0U] = *stream_remaining == block_size ? 1U : 0U;
            av2h_store_u16le(header + 1U, block_size);
            av2h_store_u16le(header + 3U, (uint16_t)~block_size);
            chunk_data(output, header, sizeof(header));
            *block_remaining = block_size;
        }
        part = size < *block_remaining ? size : *block_remaining;
        chunk_data(output, data, part);
        adler32_update(adler, data, part);
        data += part;
        size -= part;
        *stream_remaining -= part;
        *block_remaining -= part;
    }
}

Av2hStatus av2h_png_write_rgb(
    const Av2hPngRgbImage *image,
    uint8_t *row_buffer,
    size_t row_buffer_size,
    Av2hPngWriteCallback write_callback,
    void *write_context) {
    static const uint8_t signature[8U] = {
        0x89U, 'P', 'N', 'G', 0x0dU, 0x0aU, 0x1aU, 0x0aU
    };
    static const uint8_t ihdr_type[4U] = { 'I', 'H', 'D', 'R' };
    static const uint8_t idat_type[4U] = { 'I', 'D', 'A', 'T' };
    static const uint8_t iend_type[4U] = { 'I', 'E', 'N', 'D' };
    static const uint8_t zlib_header[2U] = { 0x78U, 0x01U };
    static const uint8_t filter_none = 0U;
    uint8_t ihdr[13U];
    uint8_t encoded_adler[4U];
    Av2hPngOutput output;
    Av2hAdler32 adler = { 1U, 0U };
    size_t bytes_per_channel;
    size_t row_size;
    size_t scanline_size;
    size_t stream_size;
    size_t block_count;
    size_t block_overhead;
    size_t idat_size;
    size_t stream_remaining;
    size_t block_remaining = 0U;
    uint32_t row;
    if (image == NULL || row_buffer == NULL || write_callback == NULL ||
        image->row_callback == NULL || image->width == 0U ||
        image->height == 0U || image->width > AV2H_PNG_MAX_CHUNK_SIZE ||
        image->height > AV2H_PNG_MAX_CHUNK_SIZE ||
        (image->bit_depth != 8U && image->bit_depth != 16U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    bytes_per_channel = image->bit_depth / 8U;
    if (!av2h_size_multiply(image->width, 3U, &row_size) ||
        !av2h_size_multiply(row_size, bytes_per_channel, &row_size) ||
        !av2h_size_add(row_size, 1U, &scanline_size) ||
        !av2h_size_multiply(scanline_size, image->height, &stream_size) ||
        row_buffer_size < row_size) {
        return AV2H_OVERFLOW;
    }
    block_count = stream_size / AV2H_PNG_STORED_BLOCK_MAX;
    if (stream_size % AV2H_PNG_STORED_BLOCK_MAX != 0U) ++block_count;
    if (!av2h_size_multiply(block_count, 5U, &block_overhead) ||
        !av2h_size_add(stream_size, block_overhead, &idat_size) ||
        !av2h_size_add(idat_size, 6U, &idat_size) ||
        idat_size > AV2H_PNG_MAX_CHUNK_SIZE) {
        return AV2H_LIMIT_EXCEEDED;
    }
    output.callback = write_callback;
    output.context = write_context;
    output.crc = 0U;
    output.status = AV2H_OK;
    output_raw(&output, signature, sizeof(signature));
    av2h_store_u32be(ihdr + 0U, image->width);
    av2h_store_u32be(ihdr + 4U, image->height);
    ihdr[8U] = image->bit_depth;
    ihdr[9U] = 2U;
    ihdr[10U] = 0U;
    ihdr[11U] = 0U;
    ihdr[12U] = 0U;
    write_chunk(&output, ihdr_type, ihdr, sizeof(ihdr));
    chunk_begin(&output, (uint32_t)idat_size, idat_type);
    chunk_data(&output, zlib_header, sizeof(zlib_header));
    stream_remaining = stream_size;
    for (row = 0U; row < image->height && output.status == AV2H_OK; ++row) {
        Av2hStatus row_status = image->row_callback(
            image->row_context, row, row_buffer, row_size);
        if (row_status != AV2H_OK) {
            output.status = row_status;
            break;
        }
        write_stored_data(
            &output, &adler, &filter_none, 1U,
            &stream_remaining, &block_remaining);
        write_stored_data(
            &output, &adler, row_buffer, row_size,
            &stream_remaining, &block_remaining);
    }
    av2h_store_u32be(encoded_adler, (adler.second << 16U) | adler.first);
    chunk_data(&output, encoded_adler, sizeof(encoded_adler));
    chunk_end(&output);
    write_chunk(&output, iend_type, NULL, 0U);
    return output.status;
}