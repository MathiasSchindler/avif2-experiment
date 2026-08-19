#include "av2heif.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_INPUT_SIZE (1024U * 1024U)
#define FUZZ_MAX_PIXELS (512U * 512U)
#define FUZZ_MAX_ALLOCATION_SIZE (1024U * 1024U)

static void require_unchanged(const unsigned char *bytes,
                              size_t size,
                              unsigned char expected) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (bytes[index] != expected) abort();
    }
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
    if (status != AV2H_OK) return 0;
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
        status = av2h_decode((Av2hSpan){ data, size }, &info,
                            workspace, info.workspace_size - 1U,
                            output, info.output_size, &error);
        if (status != AV2H_OUT_OF_MEMORY) abort();
        require_unchanged(output, info.output_size, 0xa5U);
    }
    if (info.output_size != 0U) {
        status = av2h_decode((Av2hSpan){ data, size }, &info,
                            workspace, info.workspace_size,
                            output, info.output_size - 1U, &error);
        if (status != AV2H_OUT_OF_MEMORY) abort();
        require_unchanged(output, info.output_size, 0xa5U);
    }
    stale = info;
    ++stale.input_fingerprint;
    status = av2h_decode((Av2hSpan){ data, size }, &stale,
                        workspace, info.workspace_size,
                        output, info.output_size, &error);
    if (status != AV2H_INVALID_DATA) abort();
    require_unchanged(output, info.output_size, 0xa5U);

    (void)av2h_decode((Av2hSpan){ data, size }, &info,
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
    Av2hObu entries[8U];
    Av2hObuLedger ledger = { entries, 8U, 0U };
    Av2hHeifSource source;
    Av2hError error;
    const Av2hObu *sequence = 0;
    const Av2hObu *frame = 0;
    size_t index;
    if (av2h_obu_parse((Av2hSpan){ stream, stream_size }, &ledger,
                       &error) != AV2H_OK) {
        return 0;
    }
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence = &entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            frame = &entries[index];
        }
    }
    if (sequence == 0 || frame == 0) return 0;
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
        if (!load_file(arguments[argument_index], &stream, &stream_size) ||
            !wrap_obu_seed(stream, stream_size, &container, &container_size)) {
            fprintf(stderr, "could not prepare seed: %s\n",
                    arguments[argument_index]);
            free(stream);
            free(container);
            return 1;
        }
        seed_cases = exercise_seed(container, container_size);
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