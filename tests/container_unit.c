#include "base.h"

#include <stdio.h>

static int failures;

enum {
    OFFSET_FTYP_MAJOR_BRAND = 8,
    OFFSET_FTYP_COMPATIBLE_BRAND = 16,
    OFFSET_META_FIRST_CHILD_SIZE = 32,
    OFFSET_HDLR_TYPE = 36,
    OFFSET_ILOC_BOX_TYPE = 83,
    OFFSET_PITM_ITEM_ID_LOW = 78,
    OFFSET_ILOC_VERSION = 87,
    OFFSET_ILOC_FIELD_SIZES = 96,
    OFFSET_ILOC_EXTENT_OFFSET = 104,
    OFFSET_INFE_VERSION = 131,
    OFFSET_INFE_ITEM_TYPE_LAST = 142,
    OFFSET_AV2C_PROPERTY_TYPE = 167,
    OFFSET_ISPE_PROPERTY_TYPE = 197,
    OFFSET_ISPE_WIDTH_LAST = 208,
    OFFSET_PIXI_PROPERTY_TYPE = 217,
    OFFSET_PIXI_FIRST_DEPTH = 226,
    OFFSET_PIXI_SECOND_DEPTH = 227,
    OFFSET_PIXI_THIRD_DEPTH = 228,
    OFFSET_COLR_PROPERTY_TYPE = 233,
    OFFSET_COLR_RESERVED = 247,
    OFFSET_IPMA_ASSOCIATION_AV2C = 267,
    OFFSET_IPMA_ASSOCIATION_ISPE = 268,
    OFFSET_IPMA_ASSOCIATION_PIXI = 269,
    OFFSET_IPMA_ASSOCIATION_COLR = 270,
    OFFSET_MDAT_TYPE = 275,
    OFFSET_ILOC_REORDERED_EXTENT_OFFSET = 112
};

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static const unsigned char config_obu[] = {
    0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU,
    0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U
};

static const unsigned char item_payload[] = { 0x02U, 0x10U, 0xaaU };

static const unsigned char ten_bit_config_obu[] = {
    0x0cU, 0x04U, 0x82U, 0x0cU, 0xcfU, 0xfcU, 0x70U,
    0xe7U, 0x37U, 0x91U, 0xb8U, 0x08U, 0x80U
};

static const unsigned char ten_bit_item_payload[] = {
    0x3eU, 0x10U, 0xf7U, 0x5aU, 0x03U, 0x40U, 0xd8U, 0x68U,
    0xefU, 0xdeU, 0x32U, 0x70U, 0xa1U, 0xf0U, 0x1eU, 0xcaU,
    0x53U, 0x61U, 0xe9U, 0x05U, 0x83U, 0x14U, 0x54U, 0x8fU,
    0x42U, 0xbeU, 0x68U, 0x8aU, 0x00U, 0xa9U, 0xeeU, 0x65U,
    0xc1U, 0x5bU, 0x53U, 0x5fU, 0x9bU, 0xcaU, 0xa8U, 0xffU,
    0x43U, 0xa2U, 0xabU, 0x95U, 0xe2U, 0x36U, 0xd3U, 0x2cU,
    0x24U, 0xf5U, 0xe7U, 0x19U, 0xb4U, 0xdfU, 0xe5U, 0x2bU,
    0x18U, 0x55U, 0x95U, 0x2cU, 0x06U, 0x6fU, 0x80U
};

static const unsigned char monochrome_config_obu[] = {
    0x0bU, 0x04U, 0x82U, 0x04U, 0x8cU, 0xffU,
    0xc6U, 0x1fU, 0x9bU, 0xc4U, 0x84U, 0x40U
};

static const unsigned char monochrome_item_payload[] = {
    0x39U, 0x10U, 0xe3U, 0x00U, 0x3fU, 0x68U, 0xd7U, 0xc3U,
    0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU, 0x52U, 0xaaU,
    0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U, 0x73U, 0x4cU,
    0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U, 0x35U, 0x9cU, 0x23U,
    0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU, 0x83U, 0x17U, 0x25U,
    0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU, 0xdbU, 0x55U, 0xb9U,
    0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU, 0xf5U, 0x35U, 0x75U,
    0xb2U, 0xd6U
};

static const unsigned char avm_item_payload[] = {
    0x63U, 0x10U, 0xe3U, 0x00U, 0x33U, 0xd0U, 0xb8U, 0x70U,
    0x00U, 0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU,
    0x2aU, 0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU,
    0x36U, 0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U,
    0x35U, 0x9cU, 0x23U, 0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU,
    0x83U, 0x17U, 0x25U, 0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU,
    0xdbU, 0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU,
    0xf5U, 0x35U, 0x75U, 0xb2U, 0xd6U, 0xd4U, 0xc1U, 0x5bU,
    0xe2U, 0xc6U, 0x99U, 0x46U, 0x47U, 0x4eU, 0x59U, 0x14U,
    0xa2U, 0xe0U, 0x35U, 0x23U, 0x50U, 0x71U, 0x5bU, 0xc0U,
    0x5dU, 0x24U, 0x8bU, 0xa5U, 0x4bU, 0xcbU, 0x90U, 0xa9U,
    0x75U, 0xfcU, 0xb0U, 0xbaU, 0x59U, 0xf2U, 0x43U, 0xc5U,
    0x73U, 0x44U, 0xeeU, 0xd8U
};

static const unsigned char canonical[] = {
    0x00U, 0x00U, 0x00U, 0x14U, 0x66U, 0x74U, 0x79U, 0x70U,
    0x6dU, 0x69U, 0x66U, 0x31U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x6dU, 0x69U, 0x66U, 0x31U, 0x00U, 0x00U, 0x00U, 0xfbU,
    0x6dU, 0x65U, 0x74U, 0x61U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x21U, 0x68U, 0x64U, 0x6cU, 0x72U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x70U, 0x69U, 0x63U, 0x74U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x0eU, 0x70U, 0x69U, 0x74U,
    0x6dU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U,
    0x00U, 0x00U, 0x1eU, 0x69U, 0x6cU, 0x6fU, 0x63U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x44U, 0x00U, 0x00U, 0x01U, 0x00U,
    0x01U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0x00U, 0x01U,
    0x17U, 0x00U, 0x00U, 0x00U, 0x03U, 0x00U, 0x00U, 0x00U,
    0x26U, 0x69U, 0x69U, 0x6eU, 0x66U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x01U, 0x00U, 0x00U, 0x00U, 0x18U, 0x69U,
    0x6eU, 0x66U, 0x65U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x01U, 0x00U, 0x00U, 0x61U, 0x76U, 0x30U, 0x32U, 0x41U,
    0x56U, 0x32U, 0x00U, 0x00U, 0x00U, 0x00U, 0x7cU, 0x69U,
    0x70U, 0x72U, 0x70U, 0x00U, 0x00U, 0x00U, 0x5dU, 0x69U,
    0x70U, 0x63U, 0x6fU, 0x00U, 0x00U, 0x00U, 0x1eU, 0x61U,
    0x76U, 0x32U, 0x43U, 0x01U, 0x00U, 0x00U, 0xe0U, 0x00U,
    0x00U, 0x0fU, 0x00U, 0x0fU, 0x0cU, 0x04U, 0x82U, 0x0aU,
    0x33U, 0xffU, 0x1cU, 0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U,
    0x20U, 0x00U, 0x00U, 0x00U, 0x14U, 0x69U, 0x73U, 0x70U,
    0x65U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x10U, 0x00U, 0x00U, 0x00U, 0x10U, 0x00U, 0x00U, 0x00U,
    0x10U, 0x70U, 0x69U, 0x78U, 0x69U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x03U, 0x08U, 0x08U, 0x08U, 0x00U, 0x00U, 0x00U,
    0x13U, 0x63U, 0x6fU, 0x6cU, 0x72U, 0x6eU, 0x63U, 0x6cU,
    0x78U, 0x00U, 0x02U, 0x00U, 0x02U, 0x00U, 0x02U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x17U, 0x69U, 0x70U, 0x6dU, 0x61U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U,
    0x00U, 0x01U, 0x04U, 0x81U, 0x02U, 0x03U, 0x04U, 0x00U,
    0x00U, 0x00U, 0x0bU, 0x6dU, 0x64U, 0x61U, 0x74U, 0x02U,
    0x10U, 0xaaU
};

static Av2hHeifSource make_source(void) {
    Av2hHeifSource source;
    source.config_obus.data = config_obu;
    source.config_obus.size = sizeof(config_obu);
    source.item_payload.data = item_payload;
    source.item_payload.size = sizeof(item_payload);
    source.color.color_description_present = 1U;
    source.color.color_description_idc = 0U;
    source.color.color_primaries = 2U;
    source.color.transfer_characteristics = 2U;
    source.color.matrix_coefficients = 2U;
    source.color.full_range_flag = 0U;
    return source;
}

static void set_u32(unsigned char *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (unsigned char)(value >> 24);
    bytes[offset + 1U] = (unsigned char)(value >> 16);
    bytes[offset + 2U] = (unsigned char)(value >> 8);
    bytes[offset + 3U] = (unsigned char)value;
}

static size_t find_box_type(const unsigned char *bytes,
                            size_t size,
                            uint32_t type) {
    size_t offset;
    for (offset = 4U; offset + 4U <= size; ++offset) {
        if ((((uint32_t)bytes[offset] << 24U) |
             ((uint32_t)bytes[offset + 1U] << 16U) |
             ((uint32_t)bytes[offset + 2U] << 8U) |
             bytes[offset + 3U]) == type) {
            return offset - 4U;
        }
    }
    return SIZE_MAX;
}

static void adjust_box_size(unsigned char *bytes,
                            size_t size,
                            uint32_t type,
                            int delta) {
    size_t offset = find_box_type(bytes, size, type);
    uint32_t box_size;
    CHECK(offset != SIZE_MAX);
    if (offset == SIZE_MAX) return;
    box_size = ((uint32_t)bytes[offset] << 24U) |
               ((uint32_t)bytes[offset + 1U] << 16U) |
               ((uint32_t)bytes[offset + 2U] << 8U) |
               bytes[offset + 3U];
    set_u32(bytes, offset, (uint32_t)((int)box_size + delta));
}

static size_t change_pixi_channels(unsigned char *bytes,
                                   size_t size,
                                   uint8_t channel_count) {
    size_t pixi = find_box_type(bytes, size, 0x70697869U);
    size_t box_size;
    size_t box_end;
    int delta = channel_count == 1U ? -2 : 2;
    CHECK(pixi != SIZE_MAX);
    if (pixi == SIZE_MAX) return size;
    box_size = ((size_t)bytes[pixi] << 24U) |
               ((size_t)bytes[pixi + 1U] << 16U) |
               ((size_t)bytes[pixi + 2U] << 8U) |
               bytes[pixi + 3U];
    box_end = pixi + box_size;
    if (delta > 0) {
        av2h_memory_copy(bytes + box_end + 2U, bytes + box_end,
                         size - box_end);
        bytes[box_end] = 8U;
        bytes[box_end + 1U] = 8U;
    } else {
        av2h_memory_copy(bytes + box_end - 2U, bytes + box_end,
                         size - box_end);
    }
    bytes[pixi + 12U] = channel_count;
    set_u32(bytes, pixi, (uint32_t)((int)box_size + delta));
    adjust_box_size(bytes, size, 0x6d657461U, delta);
    adjust_box_size(bytes, size, 0x69707270U, delta);
    adjust_box_size(bytes, size, 0x6970636fU, delta);
    set_u32(bytes, OFFSET_ILOC_EXTENT_OFFSET,
            (((uint32_t)bytes[OFFSET_ILOC_EXTENT_OFFSET] << 24U) |
             ((uint32_t)bytes[OFFSET_ILOC_EXTENT_OFFSET + 1U] << 16U) |
             ((uint32_t)bytes[OFFSET_ILOC_EXTENT_OFFSET + 2U] << 8U) |
             bytes[OFFSET_ILOC_EXTENT_OFFSET + 3U]) + delta);
    return (size_t)((int)size + delta);
}

static size_t make_libavif_fixture(unsigned char *bytes) {
    av2h_memory_copy(bytes, canonical, 163U);
    set_u32(bytes, 20U, 233U);
    set_u32(bytes, 101U, 261U);
    set_u32(bytes, 105U, 16U);
    set_u32(bytes, 147U, 106U);
    set_u32(bytes, 155U, 75U);
    set_u32(bytes, 163U, 12U);
    set_u32(bytes, 167U, 0x61763243U);
    bytes[171U] = 0x81U;
    bytes[172U] = 0U;
    bytes[173U] = 0U;
    bytes[174U] = 0U;
    av2h_memory_copy(bytes + 175U, canonical + 193U, 78U);
    set_u32(bytes, 253U, 24U);
    set_u32(bytes, 257U, 0x6d646174U);
    av2h_memory_copy(bytes + 261U, config_obu, sizeof(config_obu));
    av2h_memory_copy(bytes + 261U + sizeof(config_obu),
                     item_payload, sizeof(item_payload));
    set_u32(bytes, 8U, 0x61766966U);
    set_u32(bytes, 16U, 0x61766966U);
    return 277U;
}

static Av2hStatus parse_changed(size_t offset,
                                unsigned char value,
                                Av2hError *error) {
    unsigned char bytes[sizeof(canonical)];
    Av2hHeifImage image;
    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    bytes[offset] = value;
    return av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) }, &image, error);
}

static void check_heif_status(Av2hStatus actual,
                              Av2hStatus expected,
                              const Av2hError *error) {
    CHECK(actual == expected);
    CHECK(error->context == AV2H_CONTEXT_HEIF);
}

static void test_golden_and_round_trip(void) {
    Av2hHeifSource source = make_source();
    Av2hHeifImage image;
    Av2hError error;
    unsigned char bytes[sizeof(canonical)];
    unsigned char remuxed[sizeof(canonical)];
    size_t required = 0U;
    size_t remuxed_size = 0U;
    CHECK(av2h_heif_write(&source, 0, 0U, &required, &error) == AV2H_OK);
    CHECK(required == sizeof(canonical));
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &required, &error) == AV2H_OK);
    CHECK(av2h_memory_compare(bytes, canonical, sizeof(canonical)) == 0);
    CHECK(av2h_heif_parse((Av2hSpan){ canonical, sizeof(canonical) },
                           &image, &error) == AV2H_OK);
    CHECK(image.width == 16U && image.height == 16U);
    CHECK(image.channel_count == 3U && image.bits_per_channel == 8U);
    CHECK(image.pixel_format.chroma_format_idc == 0U);
    CHECK(image.pixel_format.bit_depth == 8U);
    CHECK(image.pixel_format.bytes_per_sample == 1U);
    CHECK(image.pixel_format.sample_storage == AV2H_SAMPLE_STORAGE_U8);
    CHECK(image.pixel_format.byte_order == AV2H_BYTE_ORDER_NOT_APPLICABLE);
    CHECK(image.pixel_format.plane_order == AV2H_PLANE_ORDER_YUV);
    CHECK(image.source.config_obus.size == sizeof(config_obu));
    CHECK(image.source.item_payload.size == sizeof(item_payload));
    CHECK(av2h_memory_compare(image.source.config_obus.data, config_obu,
                              sizeof(config_obu)) == 0);
    CHECK(av2h_memory_compare(image.source.item_payload.data, item_payload,
                              sizeof(item_payload)) == 0);
    CHECK(image.source.color.color_primaries == 2U);
    CHECK(av2h_heif_write(&image.source, remuxed, sizeof(remuxed),
                           &remuxed_size, &error) == AV2H_OK);
    CHECK(remuxed_size == sizeof(canonical));
    CHECK(av2h_memory_compare(remuxed, canonical, sizeof(canonical)) == 0);
}

static void test_writer_errors(void) {
    Av2hHeifSource source = make_source();
    Av2hError error;
    unsigned char bytes[sizeof(canonical)];
    size_t written = 0U;
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes) - 1U,
                           &written, &error) == AV2H_OUT_OF_MEMORY);
    CHECK(written == sizeof(canonical));
    source.color.full_range_flag = 2U;
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_INVALID_ARGUMENT);
    source = make_source();
    source.item_payload.size = SIZE_MAX;
    CHECK(av2h_heif_write(&source, 0, 0U,
                           &written, &error) == AV2H_OVERFLOW);
}

static void test_avm_payload_round_trip(void) {
    Av2hHeifSource source = make_source();
    Av2hHeifImage image;
    Av2hError error;
    unsigned char bytes[512];
    size_t written = 0U;
    source.item_payload.data = avm_item_payload;
    source.item_payload.size = sizeof(avm_item_payload);
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_OK);
    CHECK(written == 379U);
    CHECK(av2h_heif_parse((Av2hSpan){ bytes, written },
                           &image, &error) == AV2H_OK);
    CHECK(image.source.item_payload.size == sizeof(avm_item_payload));
    CHECK(av2h_memory_compare(image.source.item_payload.data, avm_item_payload,
                              sizeof(avm_item_payload)) == 0);
}

static void test_monochrome_pixi(void) {
    Av2hHeifSource source = make_source();
    Av2hHeifImage image;
    Av2hError error;
    unsigned char bytes[512];
    size_t written = 0U;
    source.config_obus = (Av2hSpan){ monochrome_config_obu,
                                     sizeof(monochrome_config_obu) };
    source.item_payload = (Av2hSpan){ monochrome_item_payload,
                                     sizeof(monochrome_item_payload) };
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_OK);
    CHECK(av2h_heif_parse((Av2hSpan){ bytes, written },
                           &image, &error) == AV2H_OK);
    CHECK(image.channel_count == 1U && image.bits_per_channel == 8U);
    CHECK(image.pixel_format.chroma_format_idc == 1U);
    CHECK(image.pixel_format.bit_depth == 8U);
    CHECK(image.pixel_format.bytes_per_sample == 1U);
    CHECK(image.pixel_format.sample_storage == AV2H_SAMPLE_STORAGE_U8);
    CHECK(image.pixel_format.byte_order == AV2H_BYTE_ORDER_NOT_APPLICABLE);
    CHECK(image.pixel_format.plane_order == AV2H_PLANE_ORDER_YUV);
    written = change_pixi_channels(bytes, written, 3U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, written },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(canonical));
    written = change_pixi_channels(bytes, sizeof(canonical), 1U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, written },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);
}

static void test_ten_bit_pixi(void) {
    Av2hHeifSource source = make_source();
    Av2hHeifImage image;
    Av2hError error;
    unsigned char bytes[512];
    size_t written = 0U;
    source.config_obus = (Av2hSpan){ ten_bit_config_obu,
                                     sizeof(ten_bit_config_obu) };
    source.item_payload = (Av2hSpan){ ten_bit_item_payload,
                                     sizeof(ten_bit_item_payload) };
    CHECK(av2h_heif_write(&source, 0, 0U, &written, &error) == AV2H_OK);
    CHECK(written == sizeof(canonical) - sizeof(item_payload) +
                         sizeof(ten_bit_item_payload));
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_OK);
    CHECK(bytes[OFFSET_PIXI_FIRST_DEPTH] == 10U);
    CHECK(bytes[OFFSET_PIXI_SECOND_DEPTH] == 10U);
    CHECK(bytes[OFFSET_PIXI_THIRD_DEPTH] == 10U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, written },
                                      &image, &error),
                      AV2H_UNSUPPORTED, &error);
    CHECK(image.sequence.bit_depth_idc == 0U);
    CHECK(image.channel_count == 3U && image.bits_per_channel == 10U);

    bytes[OFFSET_PIXI_FIRST_DEPTH] = 8U;
    bytes[OFFSET_PIXI_SECOND_DEPTH] = 8U;
    bytes[OFFSET_PIXI_THIRD_DEPTH] = 8U;
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, written },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);
}

static void test_top_level_and_item_mutations(void) {
    unsigned char bytes[sizeof(canonical)];
    Av2hHeifImage image;
    Av2hError error;
    check_heif_status(av2h_heif_parse(
                          (Av2hSpan){ canonical, sizeof(canonical) - 1U },
                          &image, &error), AV2H_TRUNCATED, &error);
    check_heif_status(parse_changed(OFFSET_FTYP_MAJOR_BRAND,
                                    (unsigned char)'a', &error),
                      AV2H_UNSUPPORTED, &error);
    check_heif_status(parse_changed(OFFSET_HDLR_TYPE,
                                    (unsigned char)'f', &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_PITM_ITEM_ID_LOW, 0U, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_ILOC_VERSION, 1U, &error),
                      AV2H_UNSUPPORTED, &error);
    check_heif_status(parse_changed(OFFSET_ILOC_FIELD_SIZES, 2U, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_ILOC_EXTENT_OFFSET, 0xffU, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_INFE_VERSION, 1U, &error),
                      AV2H_UNSUPPORTED, &error);
    check_heif_status(parse_changed(OFFSET_INFE_ITEM_TYPE_LAST,
                                    (unsigned char)'1', &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_FTYP_COMPATIBLE_BRAND, 0x61763032U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_UNSUPPORTED, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_META_FIRST_CHILD_SIZE, 20U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_TRUNCATED, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_MDAT_TYPE, 0x6d657461U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_ILOC_BOX_TYPE, 0x7069746dU);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);
}

static void test_property_mutations(void) {
    unsigned char bytes[sizeof(canonical)];
    Av2hHeifImage image;
    Av2hError error;
    check_heif_status(parse_changed(OFFSET_IPMA_ASSOCIATION_AV2C,
                                    0x01U, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_IPMA_ASSOCIATION_AV2C,
                                    0x80U, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_IPMA_ASSOCIATION_AV2C,
                                    0x85U, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_ISPE_WIDTH_LAST, 17U, &error),
                      AV2H_INVALID_DATA, &error);
    check_heif_status(parse_changed(OFFSET_COLR_RESERVED, 1U, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    bytes[OFFSET_IPMA_ASSOCIATION_ISPE] = 0x03U;
    bytes[OFFSET_IPMA_ASSOCIATION_PIXI] = 0x02U;
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_PIXI_PROPERTY_TYPE, 0x61626364U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_COLR_PROPERTY_TYPE, 0x61626364U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_ISPE_PROPERTY_TYPE, 0x61626364U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_AV2C_PROPERTY_TYPE, 0x61626364U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_UNSUPPORTED, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_AV2C_PROPERTY_TYPE, 0x61626364U);
    bytes[OFFSET_IPMA_ASSOCIATION_AV2C] = 0x01U;
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_COLR_PROPERTY_TYPE, 0x69737065U);
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    set_u32(bytes, OFFSET_COLR_PROPERTY_TYPE, 0x61626364U);
    bytes[OFFSET_IPMA_ASSOCIATION_COLR] = 0x84U;
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_UNSUPPORTED, &error);

    check_heif_status(parse_changed(OFFSET_PIXI_FIRST_DEPTH, 10U, &error),
                      AV2H_INVALID_DATA, &error);

    av2h_memory_copy(bytes, canonical, sizeof(bytes));
    bytes[OFFSET_PIXI_FIRST_DEPTH] = 10U;
    bytes[OFFSET_PIXI_SECOND_DEPTH] = 10U;
    bytes[OFFSET_PIXI_THIRD_DEPTH] = 10U;
    check_heif_status(av2h_heif_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                      &image, &error),
                      AV2H_INVALID_DATA, &error);
}

static void test_mdat_before_meta(void) {
    unsigned char reordered[sizeof(canonical)];
    Av2hHeifImage image;
    Av2hError error;
    av2h_memory_copy(reordered, canonical, 20U);
    av2h_memory_copy(reordered + 20U, canonical + 271U, 11U);
    av2h_memory_copy(reordered + 31U, canonical + 20U, 251U);
    set_u32(reordered, OFFSET_ILOC_REORDERED_EXTENT_OFFSET, 28U);
    CHECK(av2h_heif_parse((Av2hSpan){ reordered, sizeof(reordered) },
                           &image, &error) == AV2H_OK);
    CHECK(image.source.item_payload.data == reordered + 28U);
    CHECK(image.source.item_payload.size == sizeof(item_payload));
}

static void test_forbidden_av2_metadata(void) {
    static const unsigned char content_obu[] = {
        0x03U, 0x60U, 0x20U, 0xaaU
    };
    unsigned char config_with_content[sizeof(config_obu) + sizeof(content_obu)];
    unsigned char payload_with_content[
        sizeof(item_payload) + sizeof(content_obu)];
    Av2hHeifSource source = make_source();
    Av2hError error;
    unsigned char bytes[512];
    size_t written = 0U;
    av2h_memory_copy(config_with_content, config_obu, sizeof(config_obu));
    av2h_memory_copy(config_with_content + sizeof(config_obu), content_obu,
                     sizeof(content_obu));
    source.config_obus.data = config_with_content;
    source.config_obus.size = sizeof(config_with_content);
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_INVALID_DATA);

    source = make_source();
    av2h_memory_copy(payload_with_content, item_payload, sizeof(item_payload));
    av2h_memory_copy(payload_with_content + sizeof(item_payload), content_obu,
                     sizeof(content_obu));
    source.item_payload.data = payload_with_content;
    source.item_payload.size = sizeof(payload_with_content);
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_INVALID_DATA);
}

static void test_query_and_limits(void) {
    Av2hInfo info;
    Av2hLimits limits;
    Av2hError error;
    Av2hHeifSource source = make_source();
    unsigned char bytes[512];
    size_t written = 0U;
    source.item_payload.data = avm_item_payload;
    source.item_payload.size = sizeof(avm_item_payload);
    CHECK(av2h_heif_write(&source, bytes, sizeof(bytes),
                           &written, &error) == AV2H_OK);
    CHECK(av2h_query((Av2hSpan){ bytes, written }, 0,
                     &info, &error) == AV2H_OK);
    CHECK(info.dialect == AV2H_DIALECT_CANONICAL_V0);
    CHECK(info.image.width == 16U && info.image.height == 16U);
    CHECK(info.image.property_count == 4U);
    CHECK(info.planes[0].width == 16U && info.planes[0].height == 16U);
    CHECK(info.planes[0].offset == 0U && info.planes[0].size == 256U);
    CHECK(info.planes[1].width == 8U && info.planes[1].height == 8U);
    CHECK(info.planes[1].offset == 256U && info.planes[1].size == 64U);
    CHECK(info.planes[2].offset == 320U && info.planes[2].size == 64U);
    CHECK(info.output_size == 384U);
    CHECK(info.workspace_size != 0U && info.workspace_alignment != 0U);
    CHECK(info.obu_count == 2U && info.property_count == 4U);
    CHECK(info.container_depth == 3U);
    CHECK(info.input_size == written);
    CHECK(info.input_fingerprint != 0U);
    CHECK(av2h_validate((Av2hSpan){ bytes, written }, 0,
                        &error) == AV2H_OK);

    av2h_limits_default(&limits);
    limits.max_input_size = written - 1U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_config_obus_size = sizeof(config_obu) - 1U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_item_payload_size = sizeof(avm_item_payload) - 1U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_width = 15U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_height = 15U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_pixels = 255U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_obu_count = 1U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_recursion_depth = 2U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_output_size = 383U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_workspace_size = 1U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
    av2h_limits_default(&limits);
    limits.max_property_count = 3U;
    CHECK(av2h_query((Av2hSpan){ bytes, written }, &limits,
                     &info, &error) == AV2H_LIMIT_EXCEEDED);
    CHECK(error.context == AV2H_CONTEXT_LIMITS);
}

static void test_dialect_detection(void) {
    unsigned char compatibility[277];
    unsigned char conflicting[sizeof(canonical)];
    unsigned char unknown_version[sizeof(canonical)];
    Av2hDialect dialect;
    Av2hInfo info;
    Av2hError error;
    size_t size = make_libavif_fixture(compatibility);
    CHECK(size == sizeof(compatibility));
    CHECK(av2h_dialect_detect((Av2hSpan){ canonical, sizeof(canonical) },
                              &dialect, &error) == AV2H_OK);
    CHECK(dialect == AV2H_DIALECT_CANONICAL_V0);
    CHECK(av2h_dialect_detect((Av2hSpan){ compatibility, size },
                              &dialect, &error) == AV2H_OK);
    CHECK(dialect == AV2H_DIALECT_LIBAVIF_AVM_2026_07);
    CHECK(av2h_query((Av2hSpan){ compatibility, size }, 0,
                     &info, &error) == AV2H_UNSUPPORTED);
    CHECK(error.context == AV2H_CONTEXT_DIALECT);

    av2h_memory_copy(conflicting, canonical, sizeof(conflicting));
    set_u32(conflicting, 8U, 0x61766966U);
    set_u32(conflicting, 16U, 0x61766966U);
    CHECK(av2h_dialect_detect((Av2hSpan){ conflicting, sizeof(conflicting) },
                              &dialect, &error) == AV2H_INVALID_DATA);
    CHECK(dialect == AV2H_DIALECT_CONFLICTING);
    CHECK(error.context == AV2H_CONTEXT_DIALECT);

    av2h_memory_copy(unknown_version, canonical, sizeof(unknown_version));
    unknown_version[171U] = 2U;
    CHECK(av2h_dialect_detect(
              (Av2hSpan){ unknown_version, sizeof(unknown_version) },
              &dialect, &error) == AV2H_UNSUPPORTED);
    CHECK(dialect == AV2H_DIALECT_UNKNOWN);
    CHECK(error.context == AV2H_CONTEXT_AV2C);
}

int main(void) {
    test_golden_and_round_trip();
    test_writer_errors();
    test_avm_payload_round_trip();
    test_monochrome_pixi();
    test_ten_bit_pixi();
    test_top_level_and_item_mutations();
    test_property_mutations();
    test_mdat_before_meta();
    test_forbidden_av2_metadata();
    test_query_and_limits();
    test_dialect_detection();
    if (failures != 0) return 1;
    puts("container_unit: ok");
    return 0;
}
