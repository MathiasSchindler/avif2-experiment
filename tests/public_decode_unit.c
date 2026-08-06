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
        const char *path;
        const unsigned char *bytes;
        size_t size;
        uint32_t width;
        uint32_t height;
        uint64_t output_hash;
        int check_boundaries;
} PublicFixture;

static const PublicFixture fixtures[] = {
        { 0, tiny_obu, sizeof(tiny_obu), 16U, 16U,
            UINT64_C(0xb37800c03656c815), 0 },
        { 0, odd_obu, sizeof(odd_obu), 17U, 15U,
            UINT64_C(0xcf92b88ddff4be85), 0 },
        { "tests/fixtures/ordinary-160x120.obu", 0, 0U, 160U, 120U,
            UINT64_C(0xfd5a024e7532149d), 1 },
        { "tests/fixtures/high-detail-96x80.obu", 0, 0U, 96U, 80U,
            UINT64_C(0xdb8779d2f2c1afc6), 0 }
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
    size_t stream_size = load_fixture(fixture, stream, sizeof(stream));
    size_t container_size = 0U;
    size_t sequence_size;
    size_t frame_size;
    size_t allocation_size;
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
    CHECK(info.workspace_size != 0U);
    CHECK(info.workspace_alignment != 0U &&
          (info.workspace_alignment & (info.workspace_alignment - 1U)) == 0U);
    allocation_size = info.workspace_size + info.workspace_alignment - 1U +
                      info.output_size;
    allocation = (unsigned char *)malloc(allocation_size);
    CHECK(allocation != 0);
    workspace = (unsigned char *)(((uintptr_t)allocation +
        info.workspace_alignment - 1U) & ~(info.workspace_alignment - 1U));
    output = workspace + info.workspace_size;
    av2h_memory_fill(output, 0xa5U, info.output_size);
    CHECK(av2h_decode((Av2hSpan){ container, container_size }, &info,
                       workspace, info.workspace_size, output,
                       info.output_size, &error) == AV2H_OK);
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
        CHECK(av2h_decode((Av2hSpan){ container, prefix }, &info,
                           workspace, info.workspace_size, output,
                           info.output_size, &error) != AV2H_OK);
        CHECK(output[0] == 0xa5U);
    }
    free(allocation);
}

int main(void) {
    size_t index;
    for (index = 0U; index < sizeof(fixtures) / sizeof(fixtures[0]); ++index) {
        test_public_decode(&fixtures[index]);
    }
    if (failures != 0) return 1;
    printf("public decode unit: pass\n");
    return 0;
}