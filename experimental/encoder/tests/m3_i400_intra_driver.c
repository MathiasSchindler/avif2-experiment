#include "encoder/m3.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    EncM3WorkspaceLayout layout;
    EncM3Result result;
    unsigned int qindex;
    unsigned int search;
    size_t width;
    size_t height;
    size_t input_size;
    uint8_t *input;
    uint8_t *workspace;
    uint8_t *stream;
    uint8_t *reconstruction;
    FILE *file;
    EncStatus status;
    if (argc != 8) return 2;
    width = (size_t)strtoull(argv[2], 0, 10);
    height = (size_t)strtoull(argv[3], 0, 10);
    qindex = (unsigned int)strtoul(argv[4], 0, 10);
    search = (unsigned int)strtoul(argv[5], 0, 10);
    if (width == 0U || height == 0U || width > SIZE_MAX / height) return 2;
    input_size = width * height;
    if (enc_m3_query_workspace(width, height, &layout) != ENC_OK) return 3;
    input = malloc(input_size);
    workspace = malloc(layout.size);
    stream = malloc(layout.stream_capacity);
    reconstruction = malloc(input_size);
    if (input == 0 || workspace == 0 || stream == 0 ||
        reconstruction == 0) {
        return 4;
    }
    file = fopen(argv[1], "rb");
    if (file == 0 || fread(input, 1U, input_size, file) != input_size ||
        fgetc(file) != EOF || fclose(file) != 0) {
        return 5;
    }
    status = enc_m3_intra_i400_q_workspace(
        input, width, height, width, qindex, (EncM3IntraSearch)search,
        workspace, layout.size, stream, layout.stream_capacity,
        reconstruction, input_size, &result);
    if (status != ENC_OK) return 6;
    file = fopen(argv[6], "wb");
    if (file == 0 ||
        fwrite(stream, 1U, result.stream_size, file) != result.stream_size ||
        fclose(file) != 0) {
        return 7;
    }
    file = fopen(argv[7], "wb");
    if (file == 0 ||
        fwrite(reconstruction, 1U, input_size, file) != input_size ||
        fclose(file) != 0) {
        return 8;
    }
    printf("%zu %llu %u\n", result.stream_size,
           (unsigned long long)result.squared_error,
           (unsigned int)result.intra_mode);
    free(reconstruction);
    free(stream);
    free(workspace);
    free(input);
    return 0;
}
