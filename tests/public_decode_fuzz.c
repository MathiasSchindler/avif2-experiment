#define _POSIX_C_SOURCE 200112L

#include "av2heif.h"
#include "shared/cli_common.h"
#include "parallel_test.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_INPUT_SIZE (1024U * 1024U)
#define FUZZ_MAX_PIXELS (512U * 512U)
#define FUZZ_MAX_ALLOCATION_SIZE (2U * 1024U * 1024U)

static void require_unchanged(const unsigned char *bytes,
                              size_t size,
                              unsigned char expected) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (bytes[index] != expected) abort();
    }
}

static int fuzz_allocate(void *context, size_t size, size_t alignment,
                         Av2hOwnedMemory *owned) {
    void *memory = NULL;
    size_t actual = size == 0U ? 1U : size;
    (void)context;
    if (size > FUZZ_MAX_ALLOCATION_SIZE) return 0;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    if (posix_memalign(&memory, alignment, actual) != 0) return 0;
    owned->memory = memory;
    owned->size = size;
    return 1;
}

static void fuzz_release(void *context, Av2hOwnedMemory *owned) {
    (void)context;
    free(owned->memory);
    owned->memory = NULL;
    owned->size = 0U;
}

static Av2hTestPool fuzz_pool;
static int fuzz_pool_initialized;

static void fuzz_worker_pool_shutdown(void) {
    if (fuzz_pool_initialized) {
        av2h_test_pool_destroy(&fuzz_pool);
        fuzz_pool_initialized = 0;
    }
}

static Av2hTestPool *fuzz_worker_pool(void) {
    if (!fuzz_pool_initialized) {
        av2h_test_pool_init(&fuzz_pool, 2U);
        fuzz_pool_initialized = 1;
        atexit(fuzz_worker_pool_shutdown);
    }
    return &fuzz_pool;
}

static void fuzz_raw_obu(const uint8_t *data, size_t size,
                         const Av2hLimits *limits) {
    Av2hCliAllocator allocator = { NULL, fuzz_allocate, fuzz_release };
    Av2hCliDecodeOptions serial_options = { NULL, 0U, 0U };
    Av2hCliImage serial_image;
    Av2hStatus serial_status = av2h_cli_decode_with_limits(
        (Av2hSpan){ data, size }, limits, &allocator, &serial_options,
        &serial_image);
    if (serial_status == AV2H_OK) {
        Av2hExecution execution = av2h_test_pool_execution(fuzz_worker_pool());
        Av2hCliDecodeOptions parallel_options = { &execution, 0U, 0U };
        Av2hCliImage parallel_image;
        Av2hStatus parallel_status = av2h_cli_decode_with_limits(
            (Av2hSpan){ data, size }, limits, &allocator, &parallel_options,
            &parallel_image);
        if (parallel_status != AV2H_OK ||
            parallel_image.output.size != serial_image.output.size ||
            memcmp(parallel_image.output.memory, serial_image.output.memory,
                   serial_image.output.size) != 0) {
            abort();
        }
        av2h_cli_image_release(&allocator, &parallel_image);
    }
    av2h_cli_image_release(&allocator, &serial_image);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Av2hLimits limits;
    Av2hInfo info;
    Av2hInfo stale;
    Av2hError error;
    Av2hStatus status;
    unsigned char *allocation;
    unsigned char *workspace;
    unsigned char *output;
    size_t allocation_size;
    size_t output_alignment = _Alignof(uint16_t);

    if (size > FUZZ_MAX_INPUT_SIZE || (data == 0 && size != 0U)) return 0;
    av2h_limits_default(&limits);
    limits.max_input_size = FUZZ_MAX_INPUT_SIZE;
    limits.max_config_obus_size = FUZZ_MAX_INPUT_SIZE;
    limits.max_item_payload_size = FUZZ_MAX_INPUT_SIZE;
    limits.max_pixels = FUZZ_MAX_PIXELS;
    limits.max_output_size = FUZZ_MAX_ALLOCATION_SIZE;
    limits.max_workspace_size = FUZZ_MAX_ALLOCATION_SIZE;
    limits.max_width = 512U;
    limits.max_height = 512U;
    status = av2h_query((Av2hSpan){ data, size }, &limits, &info, &error);
    if (status != AV2H_OK) {
        fuzz_raw_obu(data, size, &limits);
        return 0;
    }
    if (av2h_validate((Av2hSpan){ data, size }, &limits, &error) != AV2H_OK) {
        abort();
    }
    if (info.workspace_alignment == 0U ||
        info.workspace_size > FUZZ_MAX_ALLOCATION_SIZE ||
        info.output_size > FUZZ_MAX_ALLOCATION_SIZE) {
        abort();
    }
    allocation_size = info.workspace_size + info.workspace_alignment - 1U +
                      output_alignment - 1U + info.output_size;
    allocation = (unsigned char *)malloc(allocation_size);
    if (allocation == 0) abort();
    workspace = (unsigned char *)(((uintptr_t)allocation +
        info.workspace_alignment - 1U) & ~(info.workspace_alignment - 1U));
    output = (unsigned char *)(((uintptr_t)(workspace + info.workspace_size) +
        output_alignment - 1U) & ~(output_alignment - 1U));

    memset(output, 0xa5, info.output_size);
    if (info.workspace_size != 0U) {
        status = av2h_decode_with_limits(
            (Av2hSpan){ data, size }, &limits, &info,
            workspace, info.workspace_size - 1U,
            output, info.output_size, &error);
        if (status != AV2H_OUT_OF_MEMORY) abort();
        require_unchanged(output, info.output_size, 0xa5U);
    }
    if (info.output_size != 0U) {
        status = av2h_decode_with_limits(
            (Av2hSpan){ data, size }, &limits, &info,
            workspace, info.workspace_size,
            output, info.output_size - 1U, &error);
        if (status != AV2H_OUT_OF_MEMORY) abort();
        require_unchanged(output, info.output_size, 0xa5U);
    }
    stale = info;
    ++stale.input_fingerprint;
    status = av2h_decode_with_limits(
        (Av2hSpan){ data, size }, &limits, &stale,
        workspace, info.workspace_size,
        output, info.output_size, &error);
    if (status != AV2H_INVALID_DATA) abort();
    require_unchanged(output, info.output_size, 0xa5U);

    (void)av2h_decode_with_limits(
        (Av2hSpan){ data, size }, &limits, &info,
        workspace, info.workspace_size,
        output, info.output_size, &error);
    free(allocation);
    return 0;
}

#if defined(AV2H_FUZZ_STANDALONE)

#include <stdio.h>

#define SEED_MAX_SIZE (4U * 1024U * 1024U)
#define MUTATION_POSITIONS 128U
#define EXHAUSTIVE_TRUNCATION_SIZE 1024U

static int load_file(const char *path, unsigned char **bytes, size_t *size) {
    FILE *file = fopen(path, "rb");
    long length;
    unsigned char *allocation;
    if (file == 0 || fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) <= 0L || (size_t)length > SEED_MAX_SIZE ||
        fseek(file, 0L, SEEK_SET) != 0) {
        if (file != 0) fclose(file);
        return 0;
    }
    allocation = (unsigned char *)malloc((size_t)length);
    if (allocation == 0 ||
        fread(allocation, 1U, (size_t)length, file) != (size_t)length ||
        fclose(file) != 0) {
        free(allocation);
        return 0;
    }
    *bytes = allocation;
    *size = (size_t)length;
    return 1;
}

static int wrap_obu_seed(const unsigned char *stream,
                         size_t stream_size,
                         unsigned char **container,
                         size_t *container_size) {
    Av2hObu entries[16U];
    Av2hObuLedger ledger = { entries, 16U, 0U };
    Av2hHeifSource source;
    Av2hError error;
    const Av2hObu *sequence = 0;
    const Av2hObu *frame = 0;
    size_t frame_count = 0U;
    size_t index;
    if (av2h_obu_parse((Av2hSpan){ stream, stream_size }, &ledger,
                       &error) != AV2H_OK) {
        return 0;
    }
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence = &entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY ||
                   entries[index].type == AV2H_OBU_REGULAR_TILE_GROUP ||
                   entries[index].type == AV2H_OBU_QUANTIZATION_MATRIX) {
            if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY && frame == 0) {
                frame = &entries[index];
            }
            ++frame_count;
        }
    }
    if (sequence == 0 || frame == 0 || frame_count != 1U) return 0;
    source.config_obus = (Av2hSpan){
        stream + sequence->offset,
        sequence->length_field_size + sequence->obu_size
    };
    source.item_payload = (Av2hSpan){
        stream + frame->offset,
        frame->length_field_size + frame->obu_size
    };
    av2h_color_info_default(&source.color);
    if (av2h_heif_write(&source, 0, 0U, container_size, &error) != AV2H_OK ||
        *container_size > FUZZ_MAX_INPUT_SIZE) {
        return 0;
    }
    *container = (unsigned char *)malloc(*container_size);
    if (*container == 0) return 0;
    if (av2h_heif_write(&source, *container, *container_size,
                        container_size, &error) != AV2H_OK) {
        free(*container);
        *container = 0;
        return 0;
    }
    return 1;
}

static size_t exercise_seed(const unsigned char *seed, size_t size) {
    unsigned char *mutated;
    size_t case_count = 0U;
    size_t index;
    size_t position_count;
    LLVMFuzzerTestOneInput(seed, size);
    ++case_count;
    if (size <= EXHAUSTIVE_TRUNCATION_SIZE) {
        for (index = 0U; index <= size; ++index) {
            LLVMFuzzerTestOneInput(seed, index);
            ++case_count;
        }
    } else {
        for (index = 0U; index <= 16U; ++index) {
            LLVMFuzzerTestOneInput(seed, (size * index) / 16U);
            ++case_count;
        }
    }
    if (size == 0U) return case_count;
    mutated = (unsigned char *)malloc(size);
    if (mutated == 0) abort();
    position_count = size < MUTATION_POSITIONS ? size : MUTATION_POSITIONS;
    for (index = 0U; index < position_count; ++index) {
        size_t position = position_count == 1U ? 0U :
            (index * (size - 1U)) / (position_count - 1U);
        memcpy(mutated, seed, size);
        mutated[position] ^= (unsigned char)(1U << (index & 7U));
        LLVMFuzzerTestOneInput(mutated, size);
        memcpy(mutated, seed, size);
        mutated[position] = 0U;
        LLVMFuzzerTestOneInput(mutated, size);
        memcpy(mutated, seed, size);
        mutated[position] = 0xffU;
        LLVMFuzzerTestOneInput(mutated, size);
        case_count += 3U;
    }
    free(mutated);
    return case_count;
}

int main(int argument_count, char **arguments) {
    int argument_index;
    size_t total_cases = 0U;
    if (argument_count < 2) {
        fprintf(stderr, "usage: %s SEED.obu...\n", arguments[0]);
        return 2;
    }
    for (argument_index = 1; argument_index < argument_count;
         ++argument_index) {
        unsigned char *stream = 0;
        unsigned char *container = 0;
        size_t stream_size = 0U;
        size_t container_size = 0U;
        size_t seed_cases;
        if (!load_file(arguments[argument_index], &stream, &stream_size)) {
            fprintf(stderr, "could not prepare seed: %s\n",
                    arguments[argument_index]);
            free(stream);
            return 1;
        }
        seed_cases = exercise_seed(stream, stream_size);
        if (wrap_obu_seed(stream, stream_size, &container, &container_size)) {
            seed_cases += exercise_seed(container, container_size);
        }
        total_cases += seed_cases;
        printf("%s: %zu deterministic cases\n",
               arguments[argument_index], seed_cases);
        free(stream);
        free(container);
    }
    printf("public decode fuzz smoke: %zu cases passed\n", total_cases);
    return 0;
}

#endif