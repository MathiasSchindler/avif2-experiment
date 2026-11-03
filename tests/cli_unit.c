#define _POSIX_C_SOURCE 200112L

#include "shared/cli_common.h"

#include "pixel_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", \
                    __FILE__, __LINE__, #expression); \
            ++failures; \
        } \
    } while (0)

typedef enum {
    TEST_ALLOCATION_FAILURE = 0,
    TEST_PARTIAL_ALLOCATION_FAILURE,
    TEST_NULL_ALLOCATION_SUCCESS,
    TEST_SHORT_ALLOCATION_SUCCESS
} TestAllocationFailure;

typedef struct {
    size_t allocations;
    size_t fail_allocation;
    size_t live;
    TestAllocationFailure failure_mode;
} TestAllocator;

typedef struct {
    uint8_t bytes[65536U];
    size_t size;
    size_t writes;
    size_t fail_write;
    size_t opens;
    size_t closes;
    Av2hStatus open_status;
    Av2hStatus close_status;
} TestFile;

static int failures;

static int allocate(
    void *context, size_t size, size_t alignment, Av2hOwnedMemory *owned) {
    TestAllocator *state = context;
    void *memory = NULL;
    int fail = ++state->allocations == state->fail_allocation;
    if (fail && state->failure_mode == TEST_ALLOCATION_FAILURE) return 0;
    if (fail && state->failure_mode == TEST_NULL_ALLOCATION_SUCCESS) return 1;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    if (posix_memalign(&memory, alignment, size + 16U) != 0) {
        return 0;
    }
    CHECK((uintptr_t)memory % alignment == 0U);
    owned->memory = memory;
    owned->size = fail && state->failure_mode == TEST_SHORT_ALLOCATION_SUCCESS ?
        size - 1U : size + 16U;
    ++state->live;
    return !fail || state->failure_mode == TEST_SHORT_ALLOCATION_SUCCESS;
}

static void release(void *context, Av2hOwnedMemory *owned) {
    TestAllocator *state = context;
    if (owned->memory != NULL) {
        CHECK(state->live != 0U);
        --state->live;
        free(owned->memory);
    }
    owned->memory = NULL;
    owned->size = 0U;
}

static Av2hStatus open_file(void *context) {
    TestFile *file = context;
    ++file->opens;
    return file->open_status;
}

static Av2hStatus write_bytes(
    void *context, const uint8_t *bytes, size_t size) {
    TestFile *file = context;
    if (++file->writes == file->fail_write) {
        if (size != 0U && file->size < sizeof(file->bytes)) {
            file->bytes[file->size++] = bytes[0U];
        }
        return AV2H_TRUNCATED;
    }
    if (size > sizeof(file->bytes) - file->size) return AV2H_LIMIT_EXCEEDED;
    if (size != 0U) memcpy(file->bytes + file->size, bytes, size);
    file->size += size;
    return AV2H_OK;
}

static Av2hStatus close_file(void *context) {
    TestFile *file = context;
    ++file->closes;
    return file->close_status;
}

static Av2hCliImage test_image(uint8_t *pixels, size_t size) {
    Av2hCliImage image;
    memset(&image, 0, sizeof(image));
    image.width = 3U;
    image.height = 2U;
    image.chroma_format_idc = 1U;
    image.output.memory = pixels;
    image.output.size = size;
    CHECK(av2h_pixel_format_init_8bit(1U, &image.format));
    return image;
}

static void test_suffixes(void) {
    static const uint16_t wide[] = {
        0x03b1U, '.', 'Y', 'U', 'V', '4', '4', '4', 'P', '1', '0', 'L', 'E', 0U
    };
    static const char *const paths[] = {
        "x.I400", "x.i420", "x.I444", "x.yuv420p10le", "x.yuv444p10le",
        "x.BmP", "x.PnG"
    };
    Av2hCliOutput output;
    size_t index;
    for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        CHECK(av2h_cli_output_parse(
            (Av2hCliPath){ paths[index], NULL }, &output) == AV2H_OK);
        CHECK(output.kind == (index < 5U ? AV2H_CLI_OUTPUT_RAW :
            index == 5U ? AV2H_CLI_OUTPUT_BMP : AV2H_CLI_OUTPUT_PNG));
    }
    CHECK(av2h_cli_output_parse(
        (Av2hCliPath){ NULL, wide }, &output) == AV2H_OK);
    CHECK(output.kind == AV2H_CLI_OUTPUT_RAW);
    CHECK(strcmp(output.raw_extension, ".yuv444p10le") == 0);
    CHECK(av2h_cli_output_parse(
        (Av2hCliPath){ NULL, NULL }, &output) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_cli_output_parse(
        (Av2hCliPath){ "x.bmp", wide }, &output) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_cli_output_parse(
        (Av2hCliPath){ "x.bmp.backup", NULL }, &output) == AV2H_UNSUPPORTED);
    CHECK(output.kind == AV2H_CLI_OUTPUT_INVALID);
}

static void test_serialization(void) {
    static const uint8_t bmp_rows[] = {
        0U, 0U, 0U, 130U, 130U, 130U, 255U, 255U, 255U, 0U, 0U, 0U,
        0U, 0U, 0U, 76U, 76U, 76U, 255U, 255U, 255U, 0U, 0U, 0U
    };
    uint8_t pixels[] = { 16U, 81U, 235U, 0U, 128U, 255U, 123U };
    char extension[] = ".i400";
    Av2hCliImage image = test_image(pixels, sizeof(pixels));
    TestAllocator state = { 0U, 0U, 0U, 0 };
    Av2hCliAllocator allocator = { &state, allocate, release };
    TestFile sink = { { 0U }, 0U, 0U, 0U, 0U, 0U, AV2H_OK, AV2H_OK };
    Av2hCliFile file = { &sink, open_file, write_bytes, close_file };
    Av2hCliOutput output = { AV2H_CLI_OUTPUT_RAW, extension };
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_OK);
    CHECK(sink.size == 6U);
    CHECK(memcmp(sink.bytes, pixels, 6U) == 0);
    CHECK(sink.opens == 1U && sink.closes == 1U && state.allocations == 0U);
    memset(&sink, 0, sizeof(sink));
    output.kind = AV2H_CLI_OUTPUT_BMP;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_OK);
    CHECK(sink.size == 78U && sink.opens == 1U && sink.closes == 1U);
    CHECK(sink.bytes[0U] == 'B' && sink.bytes[1U] == 'M');
    CHECK(av2h_load_u32le(sink.bytes + 2U) == 78U);
    CHECK(av2h_load_u32le(sink.bytes + 10U) == 54U);
    CHECK(av2h_load_u32le(sink.bytes + 18U) == 3U);
    CHECK(av2h_load_u32le(sink.bytes + 22U) == 2U);
    CHECK(av2h_load_u32le(sink.bytes + 34U) == 24U);
    CHECK(memcmp(sink.bytes + 54U, bmp_rows, sizeof(bmp_rows)) == 0);
    CHECK(state.live == 0U);
    memset(&sink, 0, sizeof(sink));
    output.kind = AV2H_CLI_OUTPUT_PNG;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_OK);
    CHECK(memcmp(sink.bytes, "\x89PNG\r\n\x1a\n", 8U) == 0);
    CHECK(av2h_load_u32be(sink.bytes + 16U) == 3U);
    CHECK(av2h_load_u32be(sink.bytes + 20U) == 2U);
    CHECK(sink.opens == 1U && sink.closes == 1U && state.live == 0U);
}

static void test_output_failures(void) {
    uint8_t pixels[6U] = { 16U, 81U, 235U, 0U, 128U, 255U };
    Av2hCliImage image = test_image(pixels, sizeof(pixels));
    TestAllocator state = { 0U, 0U, 0U, 0 };
    Av2hCliAllocator allocator = { &state, allocate, release };
    TestFile sink;
    Av2hCliFile file = { &sink, open_file, write_bytes, close_file };
    Av2hCliOutput output = { AV2H_CLI_OUTPUT_RAW, ".i420" };
    size_t index;
    memset(&sink, 0, sizeof(sink));
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_RAW_MISMATCH);
    CHECK(sink.opens == 0U);
    output.kind = AV2H_CLI_OUTPUT_BMP;
    image.format.bytes_per_sample = 2U;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_PREVIEW_DEPTH);
    CHECK(sink.opens == 0U);
    image.format.bytes_per_sample = 1U;
    image.output.size = 5U;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
    CHECK(sink.opens == 0U);
    image.output.size = sizeof(pixels);
    image.output.memory = NULL;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
    CHECK(sink.opens == 0U);
    image.output.memory = pixels;
    output.kind = (Av2hCliOutputKind)99;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
    CHECK(sink.opens == 0U);
    output.kind = AV2H_CLI_OUTPUT_BMP;
    state.fail_allocation = state.allocations + 1U;
    state.failure_mode = TEST_PARTIAL_ALLOCATION_FAILURE;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
    CHECK(sink.opens == 0U && state.live == 0U);
    state.fail_allocation = 0U;
    sink.open_status = AV2H_INVALID_DATA;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
    CHECK(sink.opens == 1U && sink.closes == 0U && state.live == 0U);
    for (index = 1U; index <= 3U; ++index) {
        memset(&sink, 0, sizeof(sink));
        sink.fail_write = index;
        CHECK(av2h_cli_write_output(
            &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
        CHECK(sink.writes == index && sink.closes == 1U && state.live == 0U);
    }
    memset(&sink, 0, sizeof(sink));
    sink.close_status = AV2H_INVALID_DATA;
    CHECK(av2h_cli_write_output(
        &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
    CHECK(sink.closes == 1U && state.live == 0U);
    for (index = AV2H_CLI_OUTPUT_RAW; index <= AV2H_CLI_OUTPUT_PNG; ++index) {
        size_t writes;
        size_t fail;
        output.kind = (Av2hCliOutputKind)index;
        output.raw_extension = ".i400";
        memset(&sink, 0, sizeof(sink));
        CHECK(av2h_cli_write_output(
            &image, &output, &allocator, &file) == AV2H_CLI_WRITE_OK);
        writes = sink.writes;
        for (fail = 1U; fail <= writes; ++fail) {
            memset(&sink, 0, sizeof(sink));
            sink.fail_write = fail;
            CHECK(av2h_cli_write_output(
                &image, &output, &allocator, &file) == AV2H_CLI_WRITE_FAILED);
            CHECK(sink.writes == fail && sink.closes == 1U && state.live == 0U);
        }
    }
    CHECK(av2h_cli_write_exit_code(AV2H_CLI_WRITE_OK) == 0);
    CHECK(av2h_cli_write_exit_code(AV2H_CLI_WRITE_RAW_MISMATCH) == 2);
    CHECK(av2h_cli_write_exit_code(AV2H_CLI_WRITE_PREVIEW_DEPTH) == 1);
}

static void test_diagnostics(void) {
    uint8_t pixels[6U] = { 0U };
    Av2hCliImage image = test_image(pixels, sizeof(pixels));
    TestFile sink;
    char offset[64U];
    size_t writes;
    size_t index;
    memset(&sink, 0, sizeof(sink));
    CHECK(av2h_cli_write_decode_error(
        &image, AV2H_UNSUPPORTED, AV2H_CLI_DIAGNOSTIC_STATUS,
        write_bytes, &sink) == AV2H_OK);
    CHECK(sink.size == sizeof("error: decode failed: unsupported feature\n") - 1U);
    CHECK(memcmp(sink.bytes, "error: decode failed: unsupported feature\n",
                 sizeof("error: decode failed: unsupported feature\n") - 1U) == 0);
    memset(&sink, 0, sizeof(sink));
    CHECK(av2h_cli_write_message(
        AV2H_CLI_MESSAGE_READ_INPUT, AV2H_CLI_DIAGNOSTIC_TRACE,
        write_bytes, &sink) == AV2H_OK);
    CHECK(sink.size == sizeof("error: cannot read input OBU\r\n") - 1U);
    CHECK(memcmp(sink.bytes, "error: cannot read input OBU\r\n",
                 sizeof("error: cannot read input OBU\r\n") - 1U) == 0);
    memset(&sink, 0, sizeof(sink));
    image.error.offset = SIZE_MAX;
    CHECK(av2h_cli_write_json(&image, write_bytes, &sink) == AV2H_OK);
    CHECK(sink.bytes[0U] == '{' && sink.bytes[sink.size - 1U] == '\n');
    sink.bytes[sink.size] = 0U;
    CHECK(snprintf(offset, sizeof(offset), "\"offset\":%zu,", SIZE_MAX) > 0);
    CHECK(strstr((const char *)sink.bytes, offset) != NULL);
    writes = sink.writes;
    for (index = 1U; index <= writes; ++index) {
        memset(&sink, 0, sizeof(sink));
        sink.fail_write = index;
        CHECK(av2h_cli_write_json(&image, write_bytes, &sink) == AV2H_TRUNCATED);
        CHECK(sink.writes == index);
    }
    memset(&sink, 0, sizeof(sink));
    CHECK(av2h_cli_write_trace(&image.result, write_bytes, &sink) == AV2H_OK);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    CHECK(sink.size != 0U);
    CHECK(sink.bytes[sink.size - 2U] == '\r');
    CHECK(sink.bytes[sink.size - 1U] == '\n');
    memset(&sink, 0, sizeof(sink));
    sink.fail_write = 1U;
    CHECK(av2h_cli_write_trace(
        &image.result, write_bytes, &sink) == AV2H_TRUNCATED);
    CHECK(sink.writes == 1U);
#else
    CHECK(sink.size == 0U);
#endif
}

static void test_decode_ownership(void) {
    static const char *const paths[] = {
        "tests/fixtures/i444-tiny.obu",
        "tests/fixtures/10bit/negative/unsupported-10bit-422.obu"
    };
    size_t path_index;
    int early;
    int retain;
    for (path_index = 0U; path_index < sizeof(paths) / sizeof(paths[0]);
         ++path_index) {
        uint8_t input[8192U];
        FILE *file = fopen(paths[path_index], "rb");
        size_t size;
        CHECK(file != NULL);
        if (file == NULL) continue;
        size = fread(input, 1U, sizeof(input), file);
        CHECK(!ferror(file) && feof(file));
        CHECK(fclose(file) == 0);
        for (early = 0; early <= 1; ++early) {
            for (retain = 0; retain <= 1; ++retain) {
                TestAllocator state = { 0U, 0U, 0U, 0 };
                Av2hCliAllocator allocator = { &state, allocate, release };
                Av2hCliDecodeOptions options = { NULL, early, retain };
                Av2hCliImage image;
                size_t count;
                size_t fail;
                int mode;
                Av2hStatus expected = path_index == 0U ?
                    AV2H_OK : AV2H_UNSUPPORTED;
                CHECK(av2h_cli_decode(
                    (Av2hSpan){ input, size }, &allocator, &options,
                    &image) == expected);
                CHECK(image.error.status == expected);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
                int retained = retain && path_index == 0U;
                CHECK((image.diagnostic_memory.memory != NULL) == retained);
                CHECK((image.result.diagnostics != NULL) == retained);
                if (retained) {
                    TestFile sink;
                    memset(&sink, 0, sizeof(sink));
                    CHECK(av2h_cli_write_trace(
                        &image.result, write_bytes, &sink) == AV2H_OK);
                    CHECK(sink.size != 0U);
                }
#else
                CHECK(image.diagnostic_memory.memory == NULL);
                CHECK(image.result.diagnostics == NULL);
#endif
                count = state.allocations;
                av2h_cli_image_release(&allocator, &image);
                CHECK(state.live == 0U && image.result.diagnostics == NULL);
                for (mode = TEST_ALLOCATION_FAILURE;
                     mode <= TEST_SHORT_ALLOCATION_SUCCESS; ++mode) {
                    for (fail = 1U; fail <= count; ++fail) {
                        state.allocations = 0U;
                        state.fail_allocation = fail;
                        state.failure_mode = (TestAllocationFailure)mode;
                        CHECK(av2h_cli_decode(
                            (Av2hSpan){ input, size }, &allocator, &options,
                            &image) == AV2H_OUT_OF_MEMORY);
                        av2h_cli_image_release(&allocator, &image);
                        CHECK(state.live == 0U);
                    }
                }
            }
        }
    }
}

int main(void) {
    test_suffixes();
    test_serialization();
    test_output_failures();
    test_diagnostics();
    test_decode_ownership();
    if (failures != 0) return 1;
    puts("cli-unit: passed");
    return 0;
}
