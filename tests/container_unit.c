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

int main(void) {
    test_golden_and_round_trip();
    test_writer_errors();
    test_avm_payload_round_trip();
    test_top_level_and_item_mutations();
    test_property_mutations();
    test_mdat_before_meta();
    test_forbidden_av2_metadata();
    if (failures != 0) return 1;
    puts("container_unit: ok");
    return 0;
}
