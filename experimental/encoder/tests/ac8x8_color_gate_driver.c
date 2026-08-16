#include <stdio.h>
#include <stdlib.h>

#include "encoder/ac8x8.h"

static int write_file(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "wb");
    int ok = file != NULL && fwrite(data, 1U, size, file) == size;
    if (file != NULL && fclose(file) != 0) ok = 0;
    return ok;
}

int main(int argc, char **argv) {
    FILE *file;
    long input_size;
    unsigned long width;
    unsigned long height;
    unsigned long qindex;
    uint8_t *input = NULL;
    uint8_t *workspace = NULL;
    uint8_t *stream = NULL;
    uint8_t *reconstruction = NULL;
    EncAc8x8WorkspaceLayout layout;
    EncAc8x8Result result;
    int status = 1;

    if (argc != 7) return 2;
    width = strtoul(argv[2], NULL, 10);
    height = strtoul(argv[3], NULL, 10);
    qindex = strtoul(argv[4], NULL, 10);
    file = fopen(argv[1], "rb");
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0) goto cleanup;
    input_size = ftell(file);
    if (input_size < 0 || fseek(file, 0L, SEEK_SET) != 0 ||
        (size_t)input_size != (size_t)width * (size_t)height) {
        goto cleanup;
    }
    input = malloc((size_t)input_size);
    if (input == NULL ||
        fread(input, 1U, (size_t)input_size, file) != (size_t)input_size ||
        enc_ac8x8_query_workspace(
            (size_t)width, (size_t)height, &layout) != ENC_OK) {
        goto cleanup;
    }
    workspace = malloc(layout.size);
    stream = malloc(layout.stream_capacity);
    reconstruction = malloc(layout.pixel_count);
    if (workspace == NULL || stream == NULL || reconstruction == NULL ||
        enc_ac8x8_i400_q_workspace(
            input, (size_t)width, (size_t)height, (size_t)width,
            (unsigned int)qindex, ENC_AC8X8_SEARCH_BOUNDED,
            workspace, layout.size, stream, layout.stream_capacity,
            reconstruction, layout.pixel_count, &result) != ENC_OK ||
        !write_file(argv[5], stream, result.stream_size) ||
        !write_file(argv[6], reconstruction, layout.pixel_count)) {
        goto cleanup;
    }
    status = 0;

cleanup:
    if (file != NULL) fclose(file);
    free(reconstruction);
    free(stream);
    free(workspace);
    free(input);
    return status;
}
