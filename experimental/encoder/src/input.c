#include "encoder/input.h"

static void enc_i400_input_reset(EncI400Input *input) {
    input->plane = 0;
    input->width = 0U;
    input->height = 0U;
    input->stride = 0U;
    input->storage.data = 0;
    input->storage.size = 0U;
}

EncStatus enc_i400_input_open(const char *path, size_t width, size_t height,
                              size_t stride, EncI400Input *input) {
    size_t byte_size;
    EncStatus status;
    if (input == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    enc_i400_input_reset(input);
    if (path == 0 || width == 0U || height == 0U ||
        width > ENC_I400_MAX_DIMENSION || height > ENC_I400_MAX_DIMENSION ||
        stride < width || stride > ENC_I400_MAX_INPUT_BYTES) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_multiply(stride, height, &byte_size)) {
        return ENC_OVERFLOW;
    }
    if (byte_size == 0U || byte_size > ENC_I400_MAX_INPUT_BYTES) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_file_map_readonly_exact(path, byte_size, &input->storage);
    if (status != ENC_OK) {
        return status;
    }
    input->plane = input->storage.data;
    input->width = width;
    input->height = height;
    input->stride = stride;
    return ENC_OK;
}

EncStatus enc_i400_input_close(EncI400Input *input) {
    EncStatus status;
    if (input == 0 || input->storage.data == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_file_unmap(&input->storage);
    enc_i400_input_reset(input);
    return status;
}
