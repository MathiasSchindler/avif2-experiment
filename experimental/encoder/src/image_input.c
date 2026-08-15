#include "encoder/image_input.h"

#define ENC_PNG_CHUNK_IHDR 0x49484452U
#define ENC_PNG_CHUNK_PLTE 0x504c5445U
#define ENC_PNG_CHUNK_IDAT 0x49444154U
#define ENC_PNG_CHUNK_IEND 0x49454e44U

#define ENC_DEFLATE_MAX_BITS 15U
#define ENC_DEFLATE_LITERALS 288U
#define ENC_DEFLATE_DISTANCES 32U

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
    uint32_t bits;
    unsigned int count;
} EncBitReader;

typedef struct {
    uint16_t counts[ENC_DEFLATE_MAX_BITS + 1U];
    uint16_t symbols[ENC_DEFLATE_LITERALS];
} EncHuffman;

typedef struct {
    size_t width;
    size_t height;
    size_t row_size;
    size_t compressed_size;
    size_t inflated_size;
    size_t output_size;
    unsigned int channels;
    uint8_t color_type;
} EncPngHeader;

static uint16_t enc_u16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static uint32_t enc_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static uint32_t enc_u32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | (uint32_t)p[3];
}

static uint32_t enc_crc32_update(uint32_t crc, const uint8_t *data,
                                 size_t size) {
    static const uint32_t table[16] = {
        0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
        0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
        0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
        0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU
    };
    size_t i;
    for (i = 0U; i < size; ++i) {
        crc ^= data[i];
        crc = (crc >> 4U) ^ table[crc & 15U];
        crc = (crc >> 4U) ^ table[crc & 15U];
    }
    return crc;
}

static uint32_t enc_adler32(const uint8_t *data, size_t size) {
    uint32_t a = 1U;
    uint32_t b = 0U;
    while (size != 0U) {
        size_t count = size > 5552U ? 5552U : size;
        size_t i;
        for (i = 0U; i < count; ++i) {
            a += data[i];
            b += a;
        }
        a %= 65521U;
        b %= 65521U;
        data += count;
        size -= count;
    }
    return (b << 16U) | a;
}

static uint8_t enc_clip(int value) {
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static void enc_rgb_to_yuv(uint8_t r, uint8_t g, uint8_t b,
                           uint8_t *y, uint8_t *u, uint8_t *v) {
    *y = enc_clip((54 * (int)r + 183 * (int)g + 19 * (int)b + 128) / 256);
    *u = enc_clip(128 + (-29 * (int)r - 99 * (int)g +
                          128 * (int)b) / 256);
    *v = enc_clip(128 + (128 * (int)r - 116 * (int)g -
                          12 * (int)b) / 256);
}

static EncStatus enc_layout(size_t width, size_t height, EncImageType type,
                            size_t scratch, EncImageLayout *layout) {
    size_t pixels;
    size_t output;
    if (width == 0U || height == 0U ||
        width > ENC_IMAGE_MAX_DIMENSION || height > ENC_IMAGE_MAX_DIMENSION) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_multiply(width, height, &pixels) ||
        !enc_size_multiply(pixels, (size_t)type, &output)) {
        return ENC_OVERFLOW;
    }
    if (output > ENC_IMAGE_MAX_BYTES || scratch > ENC_IMAGE_MAX_BYTES) {
        return ENC_UNSUPPORTED;
    }
    layout->type = type;
    layout->width = width;
    layout->height = height;
    layout->output_size = output;
    layout->scratch_size = scratch;
    return ENC_OK;
}

static EncStatus enc_bmp_query(const uint8_t *data, size_t size,
                               EncImageLayout *layout) {
    uint32_t dib;
    uint32_t offset;
    uint32_t width;
    int32_t signed_height;
    uint32_t height;
    uint16_t bits;
    uint32_t compression;
    size_t row;
    size_t bytes;
    if (size < 54U || data[0] != 'B' || data[1] != 'M') {
        return ENC_INVALID_ARGUMENT;
    }
    if (enc_u32le(data + 2U) != size) return ENC_SIZE_MISMATCH;
    offset = enc_u32le(data + 10U);
    dib = enc_u32le(data + 14U);
    if (dib < 40U || (size_t)dib > size - 14U) return ENC_UNSUPPORTED;
    width = enc_u32le(data + 18U);
    signed_height = (int32_t)enc_u32le(data + 22U);
    if (signed_height == 0 || signed_height == INT32_MIN ||
        enc_u16le(data + 26U) != 1U) {
        return ENC_INVALID_ARGUMENT;
    }
    height = signed_height < 0 ? (uint32_t)(-signed_height) :
                                (uint32_t)signed_height;
    bits = enc_u16le(data + 28U);
    compression = enc_u32le(data + 30U);
    if (compression != 0U) return ENC_UNSUPPORTED;
    if (bits != 8U && bits != 24U) return ENC_UNSUPPORTED;
    if (width == 0U || offset < 14U + dib || offset > size) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_multiply((size_t)width, bits == 24U ? 3U : 1U, &row) ||
        !enc_size_add(row, 3U, &row)) {
        return ENC_OVERFLOW;
    }
    row &= ~(size_t)3U;
    if (!enc_size_multiply(row, (size_t)height, &bytes)) return ENC_OVERFLOW;
    if (bytes > size - (size_t)offset) return ENC_SIZE_MISMATCH;
    if (bits == 8U) {
        uint32_t colors = enc_u32le(data + 46U);
        size_t palette_end;
        int grayscale = 1;
        size_t index;
        if (colors == 0U) colors = 256U;
        if (colors > 256U ||
            !enc_size_multiply((size_t)colors, 4U, &palette_end) ||
            !enc_size_add(14U + (size_t)dib, palette_end, &palette_end) ||
            palette_end > offset || palette_end > size) {
            return ENC_INVALID_ARGUMENT;
        }
        for (index = 0U; index < colors; ++index) {
            const uint8_t *entry = data + 14U + dib + index * 4U;
            if (entry[0] != entry[1] || entry[1] != entry[2]) {
                grayscale = 0;
                break;
            }
        }
        return enc_layout(width, height,
                          grayscale ? ENC_IMAGE_I400 : ENC_IMAGE_I444,
                          0U, layout);
    }
    return enc_layout(width, height, ENC_IMAGE_I444, 0U, layout);
}

static EncStatus enc_png_header(const uint8_t *data, size_t size,
                                EncPngHeader *header) {
    static const uint8_t signature[8] =
        {0x89U, 'P', 'N', 'G', '\r', '\n', 0x1aU, '\n'};
    size_t offset = 8U;
    unsigned int seen_ihdr = 0U;
    unsigned int seen_idat = 0U;
    unsigned int seen_iend = 0U;
    unsigned int seen_plte = 0U;
    unsigned int ended_idat = 0U;
    size_t i;
    header->compressed_size = 0U;
    for (i = 0U; i < 8U; ++i) {
        if (size <= i || data[i] != signature[i]) return ENC_INVALID_ARGUMENT;
    }
    while (offset < size) {
        uint32_t length;
        uint32_t type;
        uint32_t crc;
        size_t end;
        if (size - offset < 12U) return ENC_SIZE_MISMATCH;
        length = enc_u32be(data + offset);
        type = enc_u32be(data + offset + 4U);
        if (!enc_size_add(offset, 12U, &end) ||
            !enc_size_add(end, (size_t)length, &end) || end > size) {
            return ENC_SIZE_MISMATCH;
        }
        crc = enc_crc32_update(0xffffffffU, data + offset + 4U,
                               4U + (size_t)length) ^ 0xffffffffU;
        if (crc != enc_u32be(data + offset + 8U + length)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (type == ENC_PNG_CHUNK_IHDR) {
            size_t row;
            if (seen_ihdr || offset != 8U || length != 13U) {
                return ENC_INVALID_ARGUMENT;
            }
            header->width = enc_u32be(data + offset + 8U);
            header->height = enc_u32be(data + offset + 12U);
            if (data[offset + 16U] != 8U) return ENC_UNSUPPORTED;
            header->color_type = data[offset + 17U];
            if (header->color_type == 0U) header->channels = 1U;
            else if (header->color_type == 2U) header->channels = 3U;
            else if (header->color_type == 3U) header->channels = 1U;
            else if (header->color_type == 6U) header->channels = 4U;
            else return ENC_UNSUPPORTED;
            if (data[offset + 18U] != 0U || data[offset + 19U] != 0U) {
                return ENC_UNSUPPORTED;
            }
            if (data[offset + 20U] != 0U) return ENC_UNSUPPORTED;
            if (header->width == 0U || header->height == 0U ||
                header->width > ENC_IMAGE_MAX_DIMENSION ||
                header->height > ENC_IMAGE_MAX_DIMENSION) {
                return ENC_INVALID_ARGUMENT;
            }
            if (!enc_size_multiply(header->width, header->channels, &row) ||
                !enc_size_add(row, 1U, &header->row_size) ||
                !enc_size_multiply(header->row_size, header->height,
                                   &header->inflated_size)) {
                return ENC_OVERFLOW;
            }
            seen_ihdr = 1U;
        } else if (type == ENC_PNG_CHUNK_PLTE) {
            if (!seen_ihdr || seen_plte || seen_idat || length == 0U ||
                length > 768U || length % 3U != 0U) {
                return ENC_INVALID_ARGUMENT;
            }
            seen_plte = 1U;
        } else if (type == ENC_PNG_CHUNK_IDAT) {
            if (!seen_ihdr || seen_iend || ended_idat) {
                return ENC_INVALID_ARGUMENT;
            }
            if (!enc_size_add(header->compressed_size, length,
                              &header->compressed_size)) {
                return ENC_OVERFLOW;
            }
            seen_idat = 1U;
        } else if (type == ENC_PNG_CHUNK_IEND) {
            if (!seen_ihdr || !seen_idat || length != 0U ||
                end != size) return ENC_INVALID_ARGUMENT;
            seen_iend = 1U;
        } else if ((data[offset + 4U] & 0x20U) == 0U) {
            return ENC_UNSUPPORTED;
        }
        if (seen_idat && type != ENC_PNG_CHUNK_IDAT &&
            type != ENC_PNG_CHUNK_IEND) ended_idat = 1U;
        offset = end;
    }
    if (!seen_ihdr || !seen_idat || !seen_iend ||
        (header->color_type == 3U && !seen_plte)) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_add(header->compressed_size, header->inflated_size,
                      &header->output_size)) return ENC_OVERFLOW;
    if (header->output_size > ENC_IMAGE_MAX_BYTES) return ENC_UNSUPPORTED;
    return ENC_OK;
}

static EncStatus enc_png_query(const uint8_t *data, size_t size,
                               EncImageLayout *layout) {
    EncPngHeader header;
    EncStatus status = enc_png_header(data, size, &header);
    if (status != ENC_OK) return status;
    return enc_layout(header.width, header.height,
                      header.color_type == 0U ? ENC_IMAGE_I400 :
                                               ENC_IMAGE_I444,
                      header.output_size, layout);
}

EncStatus enc_image_query(const uint8_t *data, size_t size,
                          EncImageLayout *layout) {
    if (data == 0 || layout == 0 || size < 2U) return ENC_INVALID_ARGUMENT;
    if (data[0] == 'B' && data[1] == 'M') {
        return enc_bmp_query(data, size, layout);
    }
    if (data[0] == 0x89U && data[1] == 'P') {
        return enc_png_query(data, size, layout);
    }
    return ENC_UNSUPPORTED;
}

static int enc_bit_read(EncBitReader *reader, unsigned int count,
                        unsigned int *value) {
    while (reader->count < count) {
        if (reader->offset >= reader->size) return 0;
        reader->bits |= (uint32_t)reader->data[reader->offset++] <<
                        reader->count;
        reader->count += 8U;
    }
    *value = reader->bits & ((1U << count) - 1U);
    reader->bits >>= count;
    reader->count -= count;
    return 1;
}

static int enc_huffman_build(EncHuffman *huffman, const uint8_t *lengths,
                             unsigned int count) {
    unsigned int offsets[ENC_DEFLATE_MAX_BITS + 1U];
    unsigned int left = 1U;
    unsigned int i;
    enc_memory_fill(huffman->counts, 0U, sizeof(huffman->counts));
    enc_memory_fill(offsets, 0U, sizeof(offsets));
    for (i = 0U; i < count; ++i) {
        if (lengths[i] > ENC_DEFLATE_MAX_BITS) return 0;
        ++huffman->counts[lengths[i]];
    }
    if (huffman->counts[0] == count) return 0;
    for (i = 1U; i <= ENC_DEFLATE_MAX_BITS; ++i) {
        left <<= 1U;
        if (huffman->counts[i] > left) return 0;
        left -= huffman->counts[i];
    }
    offsets[1] = 0U;
    for (i = 1U; i < ENC_DEFLATE_MAX_BITS; ++i) {
        offsets[i + 1U] = offsets[i] + huffman->counts[i];
    }
    for (i = 0U; i < count; ++i) {
        if (lengths[i] != 0U) {
            huffman->symbols[offsets[lengths[i]]++] = (uint16_t)i;
        }
    }
    return 1;
}

static int enc_huffman_decode(EncBitReader *reader, const EncHuffman *huffman,
                              unsigned int *symbol) {
    unsigned int code = 0U;
    unsigned int first = 0U;
    unsigned int index = 0U;
    unsigned int length;
    for (length = 1U; length <= ENC_DEFLATE_MAX_BITS; ++length) {
        unsigned int bit;
        if (!enc_bit_read(reader, 1U, &bit)) return 0;
        code |= bit;
        if (code < first + huffman->counts[length]) {
            *symbol = huffman->symbols[index + code - first];
            return 1;
        }
        index += huffman->counts[length];
        first = (first + huffman->counts[length]) << 1U;
        code <<= 1U;
    }
    return 0;
}

static int enc_inflate_codes(EncBitReader *reader, const EncHuffman *literal,
                             const EncHuffman *distance, uint8_t *output,
                             size_t capacity, size_t *position) {
    static const uint16_t length_base[29] = {
        3U,4U,5U,6U,7U,8U,9U,10U,11U,13U,15U,17U,19U,23U,27U,
        31U,35U,43U,51U,59U,67U,83U,99U,115U,131U,163U,195U,227U,258U
    };
    static const uint8_t length_bits[29] =
        {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t distance_base[30] = {
        1U,2U,3U,4U,5U,7U,9U,13U,17U,25U,33U,49U,65U,97U,129U,
        193U,257U,385U,513U,769U,1025U,1537U,2049U,3073U,4097U,
        6145U,8193U,12289U,16385U,24577U
    };
    static const uint8_t distance_bits[30] =
        {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,
         12,12,13,13};
    for (;;) {
        unsigned int symbol;
        if (!enc_huffman_decode(reader, literal, &symbol)) return 0;
        if (symbol < 256U) {
            if (*position >= capacity) return 0;
            output[(*position)++] = (uint8_t)symbol;
        } else if (symbol == 256U) {
            return 1;
        } else if (symbol <= 285U) {
            unsigned int index = symbol - 257U;
            unsigned int length = length_base[index];
            unsigned int distance_symbol;
            unsigned int distance_value;
            unsigned int extra = 0U;
            unsigned int i;
            if (length_bits[index] != 0U &&
                !enc_bit_read(reader, length_bits[index], &extra)) return 0;
            length += extra;
            if (!enc_huffman_decode(reader, distance, &distance_symbol) ||
                distance_symbol >= 30U) return 0;
            distance_value = distance_base[distance_symbol];
            extra = 0U;
            if (distance_bits[distance_symbol] != 0U &&
                !enc_bit_read(reader, distance_bits[distance_symbol],
                              &extra)) return 0;
            distance_value += extra;
            if (distance_value == 0U || distance_value > *position ||
                length > capacity - *position) return 0;
            for (i = 0U; i < length; ++i) {
                output[*position] = output[*position - distance_value];
                ++*position;
            }
        } else return 0;
    }
}

static int enc_inflate_fixed(EncBitReader *reader, uint8_t *output,
                             size_t capacity, size_t *position) {
    uint8_t literals[ENC_DEFLATE_LITERALS];
    uint8_t distances[ENC_DEFLATE_DISTANCES];
    EncHuffman literal;
    EncHuffman distance;
    unsigned int i;
    for (i = 0U; i <= 143U; ++i) literals[i] = 8U;
    for (; i <= 255U; ++i) literals[i] = 9U;
    for (; i <= 279U; ++i) literals[i] = 7U;
    for (; i < ENC_DEFLATE_LITERALS; ++i) literals[i] = 8U;
    for (i = 0U; i < ENC_DEFLATE_DISTANCES; ++i) distances[i] = 5U;
    return enc_huffman_build(&literal, literals, ENC_DEFLATE_LITERALS) &&
           enc_huffman_build(&distance, distances, ENC_DEFLATE_DISTANCES) &&
           enc_inflate_codes(reader, &literal, &distance, output, capacity,
                             position);
}

static int enc_inflate_dynamic(EncBitReader *reader, uint8_t *output,
                               size_t capacity, size_t *position) {
    static const uint8_t order[19] =
        {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    uint8_t code_lengths[19];
    uint8_t combined[ENC_DEFLATE_LITERALS + ENC_DEFLATE_DISTANCES];
    EncHuffman code;
    EncHuffman literal;
    EncHuffman distance;
    unsigned int hlit;
    unsigned int hdist;
    unsigned int hclen;
    unsigned int value;
    unsigned int index;
    enc_memory_fill(code_lengths, 0U, sizeof(code_lengths));
    enc_memory_fill(combined, 0U, sizeof(combined));
    if (!enc_bit_read(reader, 5U, &hlit) ||
        !enc_bit_read(reader, 5U, &hdist) ||
        !enc_bit_read(reader, 4U, &hclen)) return 0;
    hlit += 257U;
    hdist += 1U;
    hclen += 4U;
    if (hlit > 286U || hdist > 32U) return 0;
    for (index = 0U; index < hclen; ++index) {
        if (!enc_bit_read(reader, 3U, &value)) return 0;
        code_lengths[order[index]] = (uint8_t)value;
    }
    if (!enc_huffman_build(&code, code_lengths, 19U)) return 0;
    index = 0U;
    while (index < hlit + hdist) {
        unsigned int symbol;
        unsigned int repeat;
        uint8_t repeated = 0U;
        if (!enc_huffman_decode(reader, &code, &symbol)) return 0;
        if (symbol <= 15U) {
            combined[index++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 16U) {
            if (index == 0U || !enc_bit_read(reader, 2U, &value)) return 0;
            repeat = value + 3U;
            repeated = combined[index - 1U];
        } else if (symbol == 17U) {
            if (!enc_bit_read(reader, 3U, &value)) return 0;
            repeat = value + 3U;
        } else if (symbol == 18U) {
            if (!enc_bit_read(reader, 7U, &value)) return 0;
            repeat = value + 11U;
        } else return 0;
        if (repeat > hlit + hdist - index) return 0;
        while (repeat-- != 0U) combined[index++] = repeated;
    }
    if (!enc_huffman_build(&literal, combined, hlit) ||
        !enc_huffman_build(&distance, combined + hlit, hdist)) return 0;
    return enc_inflate_codes(reader, &literal, &distance, output, capacity,
                             position);
}

static int enc_zlib_inflate(const uint8_t *input, size_t size,
                            uint8_t *output, size_t capacity) {
    EncBitReader reader;
    size_t position = 0U;
    unsigned int final = 0U;
    if (size < 6U || (input[0] & 15U) != 8U || (input[0] >> 4U) > 7U ||
        ((((unsigned int)input[0] << 8U) | input[1]) % 31U) != 0U ||
        (input[1] & 32U) != 0U) return 0;
    reader.data = input + 2U;
    reader.size = size - 6U;
    reader.offset = 0U;
    reader.bits = 0U;
    reader.count = 0U;
    while (!final) {
        unsigned int type;
        if (!enc_bit_read(&reader, 1U, &final) ||
            !enc_bit_read(&reader, 2U, &type)) return 0;
        if (type == 0U) {
            unsigned int length;
            unsigned int inverse;
            reader.bits = 0U;
            reader.count = 0U;
            if (reader.offset + 4U > reader.size) return 0;
            length = enc_u16le(reader.data + reader.offset);
            inverse = enc_u16le(reader.data + reader.offset + 2U);
            reader.offset += 4U;
            if ((length ^ 0xffffU) != inverse ||
                length > reader.size - reader.offset ||
                length > capacity - position) return 0;
            if (enc_memory_copy(output + position, capacity - position,
                                reader.data + reader.offset, length) != ENC_OK) {
                return 0;
            }
            reader.offset += length;
            position += length;
        } else if (type == 1U) {
            if (!enc_inflate_fixed(&reader, output, capacity, &position)) {
                return 0;
            }
        } else if (type == 2U) {
            if (!enc_inflate_dynamic(&reader, output, capacity, &position)) {
                return 0;
            }
        } else return 0;
    }
    return reader.offset == reader.size && position == capacity &&
           enc_adler32(output, capacity) == enc_u32be(input + size - 4U);
}

static EncStatus enc_bmp_decode(const uint8_t *data, size_t size,
                                uint8_t *output, EncImage *image) {
    uint32_t dib = enc_u32le(data + 14U);
    uint32_t pixel_offset = enc_u32le(data + 10U);
    size_t width = enc_u32le(data + 18U);
    int32_t signed_height = (int32_t)enc_u32le(data + 22U);
    size_t height = signed_height < 0 ? (size_t)(-signed_height) :
                                       (size_t)signed_height;
    unsigned int bits = enc_u16le(data + 28U);
    size_t row_size = ((width * (bits == 24U ? 3U : 1U)) + 3U) &
                      ~(size_t)3U;
    size_t pixels = width * height;
    int grayscale = bits == 8U;
    size_t y;
    uint8_t *planes[3] = {output, output + pixels, output + 2U * pixels};
    (void)size;
    if (bits == 8U) {
        uint32_t colors = enc_u32le(data + 46U);
        size_t index;
        if (colors == 0U) colors = 256U;
        for (index = 0U; index < colors; ++index) {
            const uint8_t *entry = data + 14U + dib + index * 4U;
            if (entry[0] != entry[1] || entry[1] != entry[2]) {
                grayscale = 0;
                break;
            }
        }
    }
    for (y = 0U; y < height; ++y) {
        size_t source_y = signed_height < 0 ? y : height - 1U - y;
        const uint8_t *row = data + pixel_offset + source_y * row_size;
        size_t x;
        for (x = 0U; x < width; ++x) {
            size_t p = y * width + x;
            if (bits == 8U) {
                uint32_t colors = enc_u32le(data + 46U);
                const uint8_t *entry;
                uint8_t index = row[x];
                if (colors == 0U) colors = 256U;
                if ((uint32_t)index >= colors) return ENC_INVALID_ARGUMENT;
                entry = data + 14U + dib + (size_t)index * 4U;
                if (grayscale) planes[0][p] = entry[0];
                else {
                    enc_rgb_to_yuv(entry[2], entry[1], entry[0],
                                   &planes[0][p], &planes[1][p],
                                   &planes[2][p]);
                }
            } else {
                enc_rgb_to_yuv(row[x * 3U + 2U], row[x * 3U + 1U],
                               row[x * 3U], &planes[0][p], &planes[1][p],
                               &planes[2][p]);
            }
        }
    }
    image->type = grayscale ? ENC_IMAGE_I400 : ENC_IMAGE_I444;
    image->width = width;
    image->height = height;
    image->plane[0] = planes[0];
    image->plane[1] = grayscale ? 0 : planes[1];
    image->plane[2] = grayscale ? 0 : planes[2];
    image->stride[0] = width;
    image->stride[1] = grayscale ? 0U : width;
    image->stride[2] = grayscale ? 0U : width;
    return ENC_OK;
}

static uint8_t enc_paeth(uint8_t left, uint8_t above, uint8_t corner) {
    int p = (int)left + (int)above - (int)corner;
    int pa = p - (int)left;
    int pb = p - (int)above;
    int pc = p - (int)corner;
    if (pa < 0) pa = -pa;
    if (pb < 0) pb = -pb;
    if (pc < 0) pc = -pc;
    return pa <= pb && pa <= pc ? left : (pb <= pc ? above : corner);
}

static EncStatus enc_png_decode(const uint8_t *data, size_t size,
                                uint8_t *output, uint8_t *scratch,
                                EncImage *image) {
    EncPngHeader header;
    size_t offset = 8U;
    size_t compressed = 0U;
    uint8_t *inflated;
    const uint8_t *palette = 0;
    size_t palette_entries = 0U;
    size_t pixels;
    size_t y;
    EncStatus status = enc_png_header(data, size, &header);
    if (status != ENC_OK) return status;
    inflated = scratch + header.compressed_size;
    while (offset < size) {
        uint32_t length = enc_u32be(data + offset);
        uint32_t type = enc_u32be(data + offset + 4U);
        if (type == ENC_PNG_CHUNK_IDAT) {
            status = enc_memory_copy(scratch + compressed,
                                     header.compressed_size - compressed,
                                     data + offset + 8U, length);
            if (status != ENC_OK) return status;
            compressed += length;
        } else if (type == ENC_PNG_CHUNK_PLTE) {
            palette = data + offset + 8U;
            palette_entries = length / 3U;
        }
        offset += 12U + length;
    }
    if (!enc_zlib_inflate(scratch, header.compressed_size, inflated,
                          header.inflated_size)) return ENC_INVALID_ARGUMENT;
    for (y = 0U; y < header.height; ++y) {
        uint8_t *row = inflated + y * header.row_size + 1U;
        const uint8_t *above = y == 0U ? 0 :
            inflated + (y - 1U) * header.row_size + 1U;
        uint8_t filter = inflated[y * header.row_size];
        size_t x;
        if (filter > 4U) return ENC_UNSUPPORTED;
        for (x = 0U; x < header.row_size - 1U; ++x) {
            uint8_t left = x < header.channels ? 0U : row[x - header.channels];
            uint8_t up = above == 0 ? 0U : above[x];
            uint8_t corner = above == 0 || x < header.channels ?
                             0U : above[x - header.channels];
            if (filter == 1U) row[x] = (uint8_t)(row[x] + left);
            else if (filter == 2U) row[x] = (uint8_t)(row[x] + up);
            else if (filter == 3U) {
                row[x] = (uint8_t)(row[x] +
                                   (((unsigned int)left + up) >> 1U));
            } else if (filter == 4U) {
                row[x] = (uint8_t)(row[x] + enc_paeth(left, up, corner));
            }
        }
    }
    pixels = header.width * header.height;
    for (y = 0U; y < header.height; ++y) {
        const uint8_t *row = inflated + y * header.row_size + 1U;
        size_t x;
        for (x = 0U; x < header.width; ++x) {
            size_t p = y * header.width + x;
            if (header.color_type == 0U) {
                output[p] = row[x];
            } else {
                uint8_t r;
                uint8_t g;
                uint8_t b;
                if (header.color_type == 3U) {
                    size_t index = row[x];
                    if (index >= palette_entries) return ENC_INVALID_ARGUMENT;
                    r = palette[index * 3U];
                    g = palette[index * 3U + 1U];
                    b = palette[index * 3U + 2U];
                } else {
                    size_t source = x * header.channels;
                    r = row[source];
                    g = row[source + 1U];
                    b = row[source + 2U];
                }
                enc_rgb_to_yuv(r, g, b, &output[p], &output[pixels + p],
                               &output[2U * pixels + p]);
            }
        }
    }
    image->type = header.color_type == 0U ? ENC_IMAGE_I400 : ENC_IMAGE_I444;
    image->width = header.width;
    image->height = header.height;
    image->plane[0] = output;
    image->plane[1] = image->type == ENC_IMAGE_I400 ? 0 : output + pixels;
    image->plane[2] = image->type == ENC_IMAGE_I400 ? 0 :
                      output + 2U * pixels;
    image->stride[0] = header.width;
    image->stride[1] = image->type == ENC_IMAGE_I400 ? 0U : header.width;
    image->stride[2] = image->type == ENC_IMAGE_I400 ? 0U : header.width;
    return ENC_OK;
}

EncStatus enc_image_decode(const uint8_t *data, size_t size,
                           void *output, size_t output_size,
                           void *scratch, size_t scratch_size,
                           EncImage *image) {
    EncImageLayout layout;
    EncStatus status;
    if (output == 0 || image == 0) return ENC_INVALID_ARGUMENT;
    status = enc_image_query(data, size, &layout);
    if (status != ENC_OK) return status;
    if (output_size < layout.output_size ||
        scratch_size < layout.scratch_size ||
        (layout.scratch_size != 0U && scratch == 0)) {
        return ENC_OUT_OF_SPACE;
    }
    if (data[0] == 'B') {
        return enc_bmp_decode(data, size, (uint8_t *)output, image);
    }
    return enc_png_decode(data, size, (uint8_t *)output, (uint8_t *)scratch,
                          image);
}
