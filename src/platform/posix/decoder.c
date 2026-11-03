#define _POSIX_C_SOURCE 200112L

#include "shared/cli_common.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *path;
    FILE *file;
} Av2hPosixFile;

static void release_memory(void *context, Av2hOwnedMemory *owned) {
    (void)context;
    free(owned->memory);
    owned->memory = NULL;
    owned->size = 0U;
}

static int allocate_memory(void *context, size_t size, size_t alignment, Av2hOwnedMemory *owned) {
    void *memory = NULL;
    size_t allocation_size = size == 0U ? 1U : size;
    (void)context;
    if (alignment < sizeof(void *))
        alignment = sizeof(void *);
    if (posix_memalign(&memory, alignment, allocation_size) != 0)
        return 0;
    owned->memory = memory;
    owned->size = size;
    return 1;
}

static int read_entire_file(const char *path, Av2hOwnedMemory *owned) {
    FILE *file = fopen(path, "rb");
    long length;
    size_t read_size;
    if (file == NULL)
        return 0;
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0L ||
        (unsigned long)length > AV2H_CLI_MAX_INPUT || fseek(file, 0L, SEEK_SET) != 0 ||
        !allocate_memory(NULL, (size_t)length, sizeof(void *), owned)) {
        fclose(file);
        return 0;
    }
    read_size = fread(owned->memory, 1U, owned->size, file);
    if (fclose(file) != 0 || read_size != owned->size) {
        release_memory(NULL, owned);
        return 0;
    }
    return 1;
}

static Av2hStatus write_console(void *context, const uint8_t *data, size_t size) {
    return fwrite(data, 1U, size, (FILE *)context) == size ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus open_output(void *context) {
    Av2hPosixFile *output = context;
    output->file = fopen(output->path, "wb");
    return output->file != NULL ? AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus write_output(void *context, const uint8_t *data, size_t size) {
    Av2hPosixFile *output = context;
    return write_console(output->file, data, size);
}

static Av2hStatus close_output(void *context) {
    Av2hPosixFile *output = context;
    int result = fclose(output->file);
    output->file = NULL;
    return result == 0 ? AV2H_OK : AV2H_INVALID_DATA;
}

static int write_json(const Av2hCliImage *image) {
    if (av2h_cli_write_json(image, write_console, stdout) != AV2H_OK || fflush(stdout) != 0) {
        (void)av2h_cli_write_message(AV2H_CLI_MESSAGE_DIAGNOSTICS, AV2H_CLI_DIAGNOSTIC_COMPACT,
                                     write_console, stderr);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    Av2hOwnedMemory input = {NULL, 0U};
    Av2hCliAllocator allocator = {NULL, allocate_memory, release_memory};
    Av2hCliDecodeOptions options = {NULL, 0, 0};
    Av2hCliImage image;
    Av2hCliOutput output;
    Av2hPosixFile output_file;
    Av2hCliFile file = {&output_file, open_output, write_output, close_output};
    Av2hStatus status;
    Av2hCliWriteResult result;
    int exit_code;
    int diagnostics_enabled = argc == 5 && strcmp(argv[4], "--diagnostics") == 0;
    if ((argc != 4 && !diagnostics_enabled) || strcmp(argv[2], "-o") != 0 ||
        av2h_cli_output_parse((Av2hCliPath){argv[3], NULL}, &output) != AV2H_OK) {
        return av2h_cli_write_usage(argv[0], 1, AV2H_CLI_DIAGNOSTIC_COMPACT, write_console,
                                    stderr) == AV2H_OK
                   ? 2
                   : 1;
    }
    if (!read_entire_file(argv[1], &input)) {
        (void)av2h_cli_write_message(AV2H_CLI_MESSAGE_READ_INPUT, AV2H_CLI_DIAGNOSTIC_COMPACT,
                                     write_console, stderr);
        return 1;
    }
    status = av2h_cli_decode((Av2hSpan){input.memory, input.size}, &allocator, &options, &image);
    if (status != AV2H_OK) {
        int reported = !diagnostics_enabled || write_json(&image);
        if (av2h_cli_write_decode_error(&image, status, AV2H_CLI_DIAGNOSTIC_COMPACT, write_console,
                                        stderr) != AV2H_OK) {
            reported = 0;
        }
        av2h_cli_image_release(&allocator, &image);
        release_memory(NULL, &input);
        return reported && status == AV2H_UNSUPPORTED ? 3 : 1;
    }
    output_file.path = argv[3];
    output_file.file = NULL;
    result = av2h_cli_write_output(&image, &output, &allocator, &file);
    exit_code = av2h_cli_write_exit_code(result);
    if (result != AV2H_CLI_WRITE_OK) {
        if (av2h_cli_write_output_error(result, AV2H_CLI_DIAGNOSTIC_COMPACT, write_console,
                                        stderr) != AV2H_OK)
            exit_code = 1;
    } else if (diagnostics_enabled && !write_json(&image)) {
        exit_code = 1;
    }
    av2h_cli_image_release(&allocator, &image);
    release_memory(NULL, &input);
    return exit_code;
}
