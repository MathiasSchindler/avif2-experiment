#include "base.h"

#include <stdio.h>
#include <stdlib.h>

#define STREAM_CAPACITY 65536U
#define CONTAINER_CAPACITY 65536U

static int failures;

static const unsigned char tiny_obu[] = {
        0x01U, 0x08U, 0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU, 0x39U,
        0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U, 0x63U, 0x10U, 0xe3U, 0x00U, 0x33U,
        0xd0U, 0xb8U, 0x70U, 0x00U, 0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U,
        0xfcU, 0x2aU, 0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U,
        0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U, 0x35U, 0x9cU, 0x23U,
        0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU, 0x83U, 0x17U, 0x25U, 0x0fU, 0xfdU,
        0xb5U, 0x2aU, 0x0fU, 0xdbU, 0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU,
        0xebU, 0xf5U, 0x35U, 0x75U, 0xb2U, 0xd6U, 0xd4U, 0xc1U, 0x5bU, 0xe2U,
        0xc6U, 0x99U, 0x46U, 0x47U, 0x4eU, 0x59U, 0x14U, 0xa2U, 0xe0U, 0x35U,
        0x23U, 0x50U, 0x71U, 0x5bU, 0xc0U, 0x5dU, 0x24U, 0x8bU, 0xa5U, 0x4bU,
        0xcbU, 0x90U, 0xa9U, 0x75U, 0xfcU, 0xb0U, 0xbaU, 0x59U, 0xf2U, 0x43U,
        0xc5U, 0x73U, 0x44U, 0xeeU, 0xd8U
};

static const unsigned char odd_obu[] = {
        0x01U, 0x08U, 0x0cU, 0x04U, 0x82U, 0x0aU, 0x43U, 0x87U, 0x0eU, 0x1cU,
        0xe6U, 0xf2U, 0x37U, 0x32U, 0x44U, 0x6bU, 0x10U, 0xeeU, 0xc0U, 0x0cU,
        0xf7U, 0xc2U, 0x89U, 0xc2U, 0x93U, 0xcfU, 0xffU, 0xc1U, 0x00U, 0x00U,
        0xffU, 0x2aU, 0x52U, 0x67U, 0xd6U, 0x80U, 0xf3U, 0x99U, 0x9cU, 0x59U,
        0x42U, 0xfbU, 0xe6U, 0x50U, 0x15U, 0x7dU, 0x6eU, 0x92U, 0x8aU, 0x57U,
        0x4aU, 0x49U, 0xd0U, 0xb7U, 0x83U, 0xaeU, 0x2fU, 0x1dU, 0x7fU, 0xcaU,
        0x2dU, 0xd3U, 0x46U, 0xf6U, 0x12U, 0x54U, 0xa5U, 0x0aU, 0x1fU, 0xb1U,
        0x8eU, 0xe1U, 0x60U, 0x41U, 0xa3U, 0xf1U, 0x29U, 0xb4U, 0x5bU, 0xf3U,
        0xa4U, 0x7cU, 0x8eU, 0x9aU, 0x2eU, 0x0aU, 0x44U, 0x0fU, 0x56U, 0x21U,
        0xbaU, 0x51U, 0xfdU, 0xd5U, 0x16U, 0x78U, 0x3cU, 0xecU, 0x4bU, 0xeaU,
        0x0bU, 0xcfU, 0xe1U, 0xfcU, 0xeeU, 0xcfU, 0x27U, 0x92U, 0x47U, 0x60U,
        0xa1U, 0xd5U, 0x70U, 0x6bU, 0xddU, 0xc5U, 0x0bU, 0x25U, 0x6cU, 0x72U,
        0x4dU, 0xfbU, 0xc9U
};

typedef struct {
    const char *name;
        const char *path;
        const unsigned char *bytes;
        size_t size;
        uint32_t width;
        uint32_t height;
        uint8_t channel_count;
        uint8_t chroma_format_idc;
        uint64_t output_hash;
        size_t workspace_size;
        int check_boundaries;
} PublicFixture;

static const PublicFixture fixtures[] = {
        { "i420-tiny", 0, tiny_obu, sizeof(tiny_obu), 16U, 16U, 3U, 0U,
            UINT64_C(0xb37800c03656c815), 21023U, 0 },
        { "i420-odd", 0, odd_obu, sizeof(odd_obu), 17U, 15U, 3U, 0U,
            UINT64_C(0xcf92b88ddff4be85), 22328U, 0 },
        { "i400-tiny", "tests/fixtures/monochrome-tiny.obu", 0, 0U,
            16U, 16U, 1U, 1U, UINT64_C(0xe5e202f90921b37a), 20092U, 0 },
        { "i400-odd", "tests/fixtures/monochrome-odd.obu", 0, 0U,
            17U, 15U, 1U, 1U, UINT64_C(0x74b332bde19bea7d), 20964U, 0 },
        { "i400-ordinary", "tests/fixtures/monochrome-ordinary.obu", 0, 0U,
            160U, 120U, 1U, 1U, UINT64_C(0xa6f20fd0e7434fce), 151312U, 1 },
        { "i400-high-detail", "tests/fixtures/monochrome-high-detail.obu",
            0, 0U, 96U, 80U, 1U, 1U, UINT64_C(0x99ddbd2918f02c3f),
            72110U, 0 },
        { "i420-ordinary", "tests/fixtures/ordinary-160x120.obu", 0, 0U,
            160U, 120U, 3U, 0U, UINT64_C(0xfd5a024e7532149d), 216276U, 1 },
        { "i420-high-detail", "tests/fixtures/high-detail-96x80.obu", 0, 0U,
            96U, 80U, 3U, 0U, UINT64_C(0xdb8779d2f2c1afc6), 98162U, 0 },
        { "i444-tiny", "tests/fixtures/i444-tiny.obu", 0, 0U,
            16U, 16U, 3U, 2U, UINT64_C(0xad5202f55d8da388), 22175U, 0 },
        { "i444-parity-hidden", "tests/fixtures/parity-hidden-i444.obu",
            0, 0U, 16U, 16U, 3U, 2U, UINT64_C(0x4bf999d643c76605),
            22175U, 1 },
        { "i444-odd", "tests/fixtures/i444-odd.obu", 0, 0U,
            17U, 15U, 3U, 2U, UINT64_C(0xd4c6d7722282b1ad), 24056U, 0 },
        { "i444-ordinary", "tests/fixtures/i444-ordinary.obu", 0, 0U,
            160U, 120U, 3U, 2U, UINT64_C(0x0501cc0d7e7a50f6), 302678U, 1 },
        { "i444-high-detail", "tests/fixtures/i444-high-detail.obu", 0, 0U,
            96U, 80U, 3U, 2U, UINT64_C(0x2fe36a3632f85bb1), 132722U, 0 }
};

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                   \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
            return;                                                            \
        }                                                                      \
    } while (0)

static size_t load_fixture(const PublicFixture *fixture,
                           unsigned char *bytes,
                           size_t capacity) {
    FILE *file;
    long length;
    size_t size;
    if (fixture->bytes != 0) {
        if (fixture->size > capacity) return 0U;
        av2h_memory_copy(bytes, fixture->bytes, fixture->size);
        return fixture->size;
    }
    file = fopen(fixture->path, "rb");
    if (file == 0 || fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) <= 0L || (size_t)length > capacity ||
        fseek(file, 0L, SEEK_SET) != 0) {
        if (file != 0) fclose(file);
        return 0U;
    }
    size = fread(bytes, 1U, (size_t)length, file);
    if (fclose(file) != 0 || size != (size_t)length) return 0U;
    return size;
}

static uint64_t fnv1a64(const unsigned char *bytes, size_t size) {
    uint64_t value = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < size; ++index) {
        value ^= bytes[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static size_t find_fourcc(const unsigned char *bytes,
                          size_t size,
                          const char fourcc[4U]) {
    size_t index;
    for (index = 0U; index + 4U <= size; ++index) {
        if (bytes[index] == (unsigned char)fourcc[0] &&
            bytes[index + 1U] == (unsigned char)fourcc[1] &&
            bytes[index + 2U] == (unsigned char)fourcc[2] &&
            bytes[index + 3U] == (unsigned char)fourcc[3]) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void test_public_decode(const PublicFixture *fixture) {
    unsigned char stream[STREAM_CAPACITY];
    unsigned char container[CONTAINER_CAPACITY];
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    Av2hHeifSource source;
    Av2hInfo info;
    Av2hInfo stale;
    Av2hError error;
    unsigned char *allocation;
    unsigned char *workspace;
    unsigned char *output;
    Av2hStatus decode_status;
    size_t stream_size = load_fixture(fixture, stream, sizeof(stream));
    size_t container_size = 0U;
    size_t sequence_size;
    size_t frame_size;
    size_t allocation_size;
    size_t expected_output;
    size_t index;
    size_t prefix;
    CHECK(stream_size != 0U);
    CHECK(av2h_obu_parse((Av2hSpan){ stream, stream_size }, &ledger,
                          &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            CHECK(sequence_obu == 0);
            sequence_obu = &entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            CHECK(frame_obu == 0);
            frame_obu = &entries[index];
        }
    }
    CHECK(sequence_obu != 0 && frame_obu != 0);
    sequence_size = sequence_obu->length_field_size + sequence_obu->obu_size;
    frame_size = frame_obu->length_field_size + frame_obu->obu_size;
    source.config_obus = (Av2hSpan){ stream + sequence_obu->offset,
                                     sequence_size };
    source.item_payload = (Av2hSpan){ stream + frame_obu->offset, frame_size };
    av2h_color_info_default(&source.color);
    CHECK(av2h_heif_write(&source, container, sizeof(container),
                           &container_size, &error) == AV2H_OK);
    CHECK(av2h_query((Av2hSpan){ container, container_size }, 0,
                     &info, &error) == AV2H_OK);
    CHECK(info.image.width == fixture->width &&
          info.image.height == fixture->height);
    CHECK(info.image.channel_count == fixture->channel_count);
    CHECK(info.image.sequence.chroma_format_idc == fixture->chroma_format_idc);
        CHECK(info.image.pixel_format.chroma_format_idc ==
            fixture->chroma_format_idc);
        CHECK(info.image.pixel_format.bit_depth == 8U);
        CHECK(info.image.pixel_format.bytes_per_sample == 1U);
        CHECK(info.image.pixel_format.sample_storage == AV2H_SAMPLE_STORAGE_U8);
        CHECK(info.image.pixel_format.byte_order ==
            AV2H_BYTE_ORDER_NOT_APPLICABLE);
        CHECK(info.image.pixel_format.plane_order == AV2H_PLANE_ORDER_YUV);
    CHECK(info.image.sequence.seq_profile_idc ==
          (fixture->chroma_format_idc == 2U ? 4U : 0U));
    expected_output = (size_t)fixture->width * fixture->height;
    if (fixture->channel_count != 1U) {
        expected_output += fixture->chroma_format_idc == 2U ?
            2U * expected_output :
            2U * (((size_t)fixture->width + 1U) / 2U) *
                (((size_t)fixture->height + 1U) / 2U);
    }
    CHECK(info.output_size == expected_output);
    if (fixture->channel_count == 1U) {
        CHECK(info.planes[0].width == fixture->width &&
              info.planes[0].height == fixture->height &&
              info.planes[0].row_bytes == fixture->width &&
              info.planes[0].offset == 0U &&
              info.planes[0].size == info.output_size);
        CHECK(info.planes[1].width == 0U && info.planes[1].height == 0U &&
              info.planes[1].row_bytes == 0U &&
              info.planes[1].offset == 0U && info.planes[1].size == 0U);
        CHECK(info.planes[2].width == 0U && info.planes[2].height == 0U &&
              info.planes[2].row_bytes == 0U &&
              info.planes[2].offset == 0U && info.planes[2].size == 0U);
        } else if (fixture->chroma_format_idc == 2U) {
          size_t plane_size = (size_t)fixture->width * fixture->height;
          CHECK(info.planes[0].width == fixture->width &&
              info.planes[0].height == fixture->height &&
              info.planes[0].row_bytes == fixture->width &&
              info.planes[0].offset == 0U &&
              info.planes[0].size == plane_size);
          CHECK(info.planes[1].width == fixture->width &&
              info.planes[1].height == fixture->height &&
              info.planes[1].row_bytes == fixture->width &&
              info.planes[1].offset == plane_size &&
              info.planes[1].size == plane_size);
          CHECK(info.planes[2].width == fixture->width &&
              info.planes[2].height == fixture->height &&
              info.planes[2].row_bytes == fixture->width &&
              info.planes[2].offset == plane_size * 2U &&
              info.planes[2].size == plane_size);
    }
    CHECK(info.workspace_size == fixture->workspace_size);
    CHECK(info.workspace_alignment == 4U);
    allocation_size = info.workspace_size + info.workspace_alignment - 1U +
                      info.output_size;
    allocation = (unsigned char *)malloc(allocation_size);
    CHECK(allocation != 0);
    workspace = (unsigned char *)(((uintptr_t)allocation +
        info.workspace_alignment - 1U) & ~(info.workspace_alignment - 1U));
    output = workspace + info.workspace_size;
    av2h_memory_fill(output, 0xa5U, info.output_size);
    decode_status = av2h_decode(
        (Av2hSpan){ container, container_size }, &info,
        workspace, info.workspace_size, output, info.output_size, &error);
    if (decode_status != AV2H_OK) {
        fprintf(stderr, "%s: decode status=%d context=%u offset=%zu\n",
                fixture->path != 0 ? fixture->path : "embedded",
                (int)decode_status, error.context, error.offset);
    }
    CHECK(decode_status == AV2H_OK);
    CHECK(error.status == AV2H_OK);
    CHECK(error.diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_COMPLETE);
    CHECK(error.diagnostic_reason == AV2H_DIAGNOSTIC_REASON_NONE);
    CHECK(error.syntax_stage == AV2H_DIAGNOSTIC_SYNTAX_NONE);
    CHECK(av2h_diagnostic_stage_string(error.diagnostic_stage)[0] == 'c');
    if (fnv1a64(output, info.output_size) != fixture->output_hash) {
        fprintf(stderr, "%s: output hash=0x%016llx expected=0x%016llx\n",
                fixture->path != 0 ? fixture->path : "embedded",
                (unsigned long long)fnv1a64(output, info.output_size),
                (unsigned long long)fixture->output_hash);
    }
    CHECK(fnv1a64(output, info.output_size) == fixture->output_hash);

    if (!fixture->check_boundaries) {
        free(allocation);
        return;
    }
    av2h_memory_fill(output, 0xa5U, info.output_size);
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &info,
                       workspace, info.workspace_size - 1U, output,
                       info.output_size, &error) == AV2H_OUT_OF_MEMORY);
    CHECK(error.context == AV2H_CONTEXT_WORKSPACE && output[0] == 0xa5U);
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &info,
                       workspace + 1U, info.workspace_size, output,
                       info.output_size, &error) == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_WORKSPACE && output[0] == 0xa5U);
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &info,
                       workspace, info.workspace_size, output,
                       info.output_size - 1U, &error) == AV2H_OUT_OF_MEMORY);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT && output[0] == 0xa5U);
    stale = info;
    ++stale.input_fingerprint;
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &stale,
                       workspace, info.workspace_size, output,
                       info.output_size, &error) == AV2H_INVALID_DATA);
    CHECK(error.context == AV2H_CONTEXT_BINDING && output[0] == 0xa5U);
    stale = info;
    ++stale.planes[1].offset;
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &stale,
                       workspace, info.workspace_size, output,
                       info.output_size, &error) == AV2H_INVALID_DATA);
    CHECK(error.context == AV2H_CONTEXT_BINDING && output[0] == 0xa5U);
    stale = info;
    ++stale.image.pixel_format.bytes_per_sample;
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &stale,
                       workspace, info.workspace_size, output,
                       info.output_size, &error) == AV2H_INVALID_DATA);
    CHECK(error.context == AV2H_CONTEXT_BINDING && output[0] == 0xa5U);
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &info,
                       workspace, info.workspace_size, workspace,
                       info.output_size, &error) == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT && output[0] == 0xa5U);
    av2h_memory_copy(workspace, &info, sizeof(info));
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size },
              (const Av2hInfo *)workspace, workspace, info.workspace_size,
              output, info.output_size, &error) == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT && output[0] == 0xa5U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace, info.workspace_size, output, info.output_size,
              (Av2hError *)output) == AV2H_INVALID_ARGUMENT);
    CHECK(output[0] == 0xa5U);
    for (prefix = 0U; prefix < container_size; ++prefix) {
        output[0] = 0xa5U;
        error.diagnostic_stage = 255U;
        error.parsed_value = UINT32_MAX;
        CHECK(av2h_decode((Av2hSpan){ container, prefix }, &info,
                           workspace, info.workspace_size, output,
                           info.output_size, &error) != AV2H_OK);
        CHECK(output[0] == 0xa5U);
        CHECK(error.diagnostic_stage != 255U);
        CHECK(error.parsed_value == 0U);
    }
    free(allocation);
}

typedef struct {
    const char *name;
    const char *obu_path;
    const char *reference_path;
    size_t stream_size;
    uint32_t width;
    uint32_t height;
    uint8_t chroma_format_idc;
    size_t output_size;
    size_t workspace_size;
    uint64_t output_hash;
} TenBitPublicFixture;

static void test_10bit_public_decode(const TenBitPublicFixture *fixture) {
    PublicFixture loader = {
        fixture->name, fixture->obu_path, 0, 0U,
        fixture->width, fixture->height, 3U, fixture->chroma_format_idc,
        0U, 0U, 0
    };
    unsigned char stream[STREAM_CAPACITY];
    unsigned char container[CONTAINER_CAPACITY];
    unsigned char reference[1536U];
    Av2hObu entries[3U];
    Av2hObuLedger ledger = { entries, 3U, 0U };
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    Av2hHeifSource source;
    Av2hInfo info;
    Av2hInfo stale;
    Av2hError error;
    Av2hStatus decode_status;
    unsigned char *allocation;
    unsigned char *workspace;
    unsigned char *output;
    FILE *reference_file;
    size_t stream_size = load_fixture(&loader, stream, sizeof(stream));
    size_t container_size = 0U;
    size_t allocation_size;
    size_t chroma_height;
    size_t chroma_width;
    size_t chroma_plane_size;
    size_t luma_plane_size;
    size_t index;
    size_t prefix;
    CHECK(stream_size == fixture->stream_size);
    CHECK(av2h_obu_parse(
              (Av2hSpan){ stream, stream_size }, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence_obu = &entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            frame_obu = &entries[index];
        }
    }
    CHECK(sequence_obu != 0 && frame_obu != 0);
    source.config_obus = (Av2hSpan){
        stream + sequence_obu->offset,
        sequence_obu->length_field_size + sequence_obu->obu_size
    };
    source.item_payload = (Av2hSpan){
        stream + frame_obu->offset,
        frame_obu->length_field_size + frame_obu->obu_size
    };
    av2h_color_info_default(&source.color);
    CHECK(av2h_heif_write(&source, container, sizeof(container),
                           &container_size, &error) == AV2H_OK);
    {
        size_t pixi_type = find_fourcc(
            container, container_size, "pixi");
        unsigned char depth;
        CHECK(pixi_type != SIZE_MAX && pixi_type + 9U < container_size);
        depth = container[pixi_type + 9U];
        container[pixi_type + 9U] = 8U;
        CHECK(av2h_query(
                  (Av2hSpan){ container, container_size }, 0,
                  &info, &error) == AV2H_INVALID_DATA);
        CHECK(error.context == AV2H_CONTEXT_HEIF);
        container[pixi_type + 9U] = depth;
    }
    CHECK(av2h_query((Av2hSpan){ container, container_size }, 0,
                     &info, &error) == AV2H_OK);
        CHECK(info.image.width == fixture->width &&
            info.image.height == fixture->height);
    CHECK(info.image.channel_count == 3U);
    CHECK(info.image.bits_per_channel == 10U);
        CHECK(info.image.sequence.seq_profile_idc ==
            (fixture->chroma_format_idc == 2U ? 4U : 0U));
        CHECK(info.image.sequence.chroma_format_idc ==
            fixture->chroma_format_idc);
        CHECK(info.image.pixel_format.chroma_format_idc ==
            fixture->chroma_format_idc);
    CHECK(info.image.pixel_format.bit_depth == 10U);
    CHECK(info.image.pixel_format.bytes_per_sample == 2U);
    CHECK(info.image.pixel_format.sample_storage == AV2H_SAMPLE_STORAGE_U16);
    CHECK(info.image.pixel_format.byte_order == AV2H_BYTE_ORDER_LITTLE_ENDIAN);
    CHECK(info.image.pixel_format.plane_order == AV2H_PLANE_ORDER_YUV);
        luma_plane_size = (size_t)fixture->width * fixture->height * 2U;
        chroma_width = fixture->chroma_format_idc == 2U ?
          fixture->width : (fixture->width + 1U) / 2U;
        chroma_height = fixture->chroma_format_idc == 2U ?
          fixture->height : (fixture->height + 1U) / 2U;
        chroma_plane_size = chroma_width * chroma_height * 2U;
        CHECK(info.planes[0].width == fixture->width &&
            info.planes[0].height == fixture->height);
        CHECK(info.planes[0].row_bytes == fixture->width * 2U &&
            info.planes[0].offset == 0U);
        CHECK(info.planes[0].size == luma_plane_size);
        CHECK(info.planes[1].width == chroma_width &&
            info.planes[1].height == chroma_height);
        CHECK(info.planes[1].row_bytes == chroma_width * 2U &&
            info.planes[1].offset == luma_plane_size);
        CHECK(info.planes[1].size == chroma_plane_size);
        CHECK(info.planes[2].width == chroma_width &&
            info.planes[2].height == chroma_height);
        CHECK(info.planes[2].row_bytes == chroma_width * 2U &&
            info.planes[2].offset == luma_plane_size + chroma_plane_size);
        CHECK(info.planes[2].size == chroma_plane_size);
        CHECK(info.output_size == fixture->output_size);
        CHECK(info.workspace_size == fixture->workspace_size &&
            info.workspace_alignment == 4U);

        reference_file = fopen(fixture->reference_path, "rb");
    CHECK(reference_file != 0);
        CHECK(fread(reference, 1U, fixture->output_size, reference_file) ==
            fixture->output_size);
    CHECK(fclose(reference_file) == 0);

    allocation_size = info.workspace_size + info.workspace_alignment - 1U +
                      _Alignof(uint16_t) + info.output_size;
    allocation = (unsigned char *)malloc(allocation_size);
    CHECK(allocation != 0);
    workspace = (unsigned char *)(((uintptr_t)allocation +
        info.workspace_alignment - 1U) & ~(info.workspace_alignment - 1U));
    output = (unsigned char *)(((uintptr_t)(workspace + info.workspace_size) +
        _Alignof(uint16_t) - 1U) & ~(_Alignof(uint16_t) - 1U));
    av2h_memory_fill(output, 0xa5U, info.output_size);
    decode_status = av2h_decode(
        (Av2hSpan){ container, container_size }, &info,
        workspace, info.workspace_size, output, info.output_size, &error);
    if (decode_status != AV2H_OK) {
        fprintf(stderr, "%s: decode status=%d context=%u offset=%zu\n",
                fixture->name, (int)decode_status, error.context, error.offset);
    }
    CHECK(decode_status == AV2H_OK);
    CHECK(fnv1a64(output, info.output_size) == fixture->output_hash);
    for (index = 0U; index < info.output_size; ++index) {
        CHECK(output[index] == reference[index]);
    }

    av2h_memory_fill(output, 0xa5U, info.output_size + 1U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace, info.workspace_size - 1U,
              output, info.output_size, &error) == AV2H_OUT_OF_MEMORY);
    CHECK(error.context == AV2H_CONTEXT_WORKSPACE && output[0] == 0xa5U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace + 1U, info.workspace_size,
              output, info.output_size, &error) == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_WORKSPACE && output[0] == 0xa5U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace, info.workspace_size,
              output, info.output_size - 1U, &error) == AV2H_OUT_OF_MEMORY);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT && output[0] == 0xa5U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace, info.workspace_size,
              output + 1U, info.output_size, &error) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT && output[1] == 0xa5U);

    stale = info;
    ++stale.input_fingerprint;
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &stale,
              workspace, info.workspace_size,
              output, info.output_size, &error) == AV2H_INVALID_DATA);
    CHECK(error.context == AV2H_CONTEXT_BINDING && output[0] == 0xa5U);
    stale = info;
    ++stale.planes[1].offset;
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &stale,
              workspace, info.workspace_size,
              output, info.output_size, &error) == AV2H_INVALID_DATA);
    CHECK(error.context == AV2H_CONTEXT_BINDING && output[0] == 0xa5U);
    stale = info;
    stale.image.pixel_format.byte_order = AV2H_BYTE_ORDER_NOT_APPLICABLE;
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &stale,
              workspace, info.workspace_size,
              output, info.output_size, &error) == AV2H_INVALID_DATA);
    CHECK(error.context == AV2H_CONTEXT_BINDING && output[0] == 0xa5U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace, info.workspace_size, workspace, info.output_size,
              &error) == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT);
    av2h_memory_copy(workspace, &info, sizeof(info));
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size },
              (const Av2hInfo *)workspace,
              workspace, info.workspace_size,
              output, info.output_size, &error) == AV2H_INVALID_ARGUMENT);
    CHECK(error.context == AV2H_CONTEXT_OUTPUT && output[0] == 0xa5U);
    CHECK(av2h_decode(
              (Av2hSpan){ container, container_size }, &info,
              workspace, info.workspace_size,
              output, info.output_size,
              (Av2hError *)output) == AV2H_INVALID_ARGUMENT);
    CHECK(output[0] == 0xa5U);
    for (prefix = 0U; prefix < container_size; ++prefix) {
        output[0] = 0xa5U;
        CHECK(av2h_decode(
                  (Av2hSpan){ container, prefix }, &info,
                  workspace, info.workspace_size,
                  output, info.output_size, &error) != AV2H_OK);
        CHECK(output[0] == 0xa5U);
    }
    free(allocation);
}

int main(void) {
    static const TenBitPublicFixture ten_bit_fixtures[] = {
        {
            "10bit-420",
            "tests/fixtures/10bit/avm-reference/"
            "tiny-constrained-16x16-yuv420p10le-qp208.obu",
            "tests/fixtures/10bit/avm-reference/"
            "tiny-constrained-16x16-yuv420p10le-qp208-decoded.yuv",
            45U, 16U, 16U, 0U, 768U, 26786U,
            UINT64_C(0x61dd9ffdb54a4439)
        },
        {
            "10bit-444",
            "tests/fixtures/10bit/avm-reference/"
            "tiny-precision-16x16-yuv444p10le-qp128.obu",
            "tests/fixtures/10bit/avm-reference/"
            "tiny-precision-16x16-yuv444p10le-qp128-decoded.yuv",
            218U, 16U, 16U, 2U, 1536U, 29090U,
            UINT64_C(0x1ed1553728d318a0)
        }
    };
    size_t index;
    for (index = 0U; index < sizeof(fixtures) / sizeof(fixtures[0]); ++index) {
        test_public_decode(&fixtures[index]);
    }
    for (index = 0U;
         index < sizeof(ten_bit_fixtures) / sizeof(ten_bit_fixtures[0]);
         ++index) {
        test_10bit_public_decode(&ten_bit_fixtures[index]);
    }
    if (failures != 0) return 1;
    printf("public decode unit: pass\n");
    return 0;
}