#ifndef EXPERIMENTAL_ENCODER_PLATFORM_H
#define EXPERIMENTAL_ENCODER_PLATFORM_H

#include "encoder/base.h"

typedef struct {
    int descriptor;
} EncFile;

typedef struct {
    const uint8_t *data;
    size_t size;
} EncReadMapping;

EncStatus enc_file_open_read(const char *path, EncFile *file);
EncStatus enc_file_read(EncFile *file, void *buffer, size_t capacity,
                        size_t *bytes_read);
EncStatus enc_file_write_all(EncFile *file, const void *buffer, size_t size);
EncStatus enc_file_close(EncFile *file);
EncStatus enc_file_write_atomic(const char *path, const void *buffer,
                                size_t size);
EncStatus enc_file_map_readonly_exact(const char *path, size_t expected_size,
                                      EncReadMapping *mapping);
EncStatus enc_file_unmap(EncReadMapping *mapping);
EncStatus enc_standard_error_write(const char *message, size_t size);
void enc_process_exit(int status);

#endif
