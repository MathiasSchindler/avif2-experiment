#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoder/obu.h"
#include "encoder/syntax.h"

static int load_levels(const char *path, int16_t *levels, size_t count) {
    size_t index;
    FILE *input;
    if (strcmp(path, "flat") == 0) {
        for (index = 0U; index < count; ++index) levels[index] = 0;
        return 1;
    }
    input = fopen(path, "rb");
    if (input == 0) return 0;
    for (index = 0U; index < count; ++index) {
        uint8_t bytes[2U];
        if (fread(bytes, 1U, sizeof(bytes), input) != sizeof(bytes)) {
            fclose(input);
            return 0;
        }
        levels[index] = (int16_t)(
            (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
    }
    if (fgetc(input) != EOF) {
        fclose(input);
        return 0;
    }
    return fclose(input) == 0;
}

int main(int argc, char **argv) {
    uint8_t sequence[32U];
    uint8_t frame_header[16U];
    uint8_t tile[16384U];
    uint8_t payload[16400U];
    uint8_t stream[16500U];
    int16_t levels[64U];
    EncByteWriter payload_writer;
    EncByteWriter stream_writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    size_t width;
    size_t height;
    size_t count;
    unsigned int qindex = 100U;
    FILE *output;
    if (argc != 6 && argc != 7) return 2;
    width = (size_t)strtoul(argv[1], 0, 10);
    height = (size_t)strtoul(argv[2], 0, 10);
    if (argc == 7) qindex = (unsigned int)strtoul(argv[6], 0, 10);
    count = ((width + 7U) / 8U) * ((height + 7U) / 8U);
    if (width < 17U || width > 64U || height < 17U || height > 64U ||
        count > 64U || qindex == 0U || qindex > 255U ||
        !load_levels(argv[3], levels, count)) {
        return 2;
    }
    if (enc_m3_dc_grid_tile_q(
            tile, sizeof(tile), &tile_size, width, height, levels, count,
            qindex) != ENC_OK) {
        return 3;
    }
    output = fopen(argv[5], "wb");
    if (output == 0) return 4;
    if (strcmp(argv[4], "tile") == 0) {
        if (fwrite(tile, 1U, tile_size, output) != tile_size) {
            fclose(output);
            return 4;
        }
        return fclose(output) == 0 ? 0 : 4;
    }
    if (strcmp(argv[4], "stream") != 0 ||
        enc_m3_sequence_header(
            sequence, sizeof(sequence), &sequence_size, width, height) !=
            ENC_OK ||
        enc_m3_frame_header_q(
            frame_header, sizeof(frame_header), &frame_header_size, qindex) !=
            ENC_OK) {
        fclose(output);
        return 3;
    }
    enc_byte_writer_init(&payload_writer, payload, sizeof(payload));
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
        fclose(output);
        return 3;
    }
    enc_byte_writer_init(&stream_writer, stream, sizeof(stream));
    if (enc_obu_write(&stream_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 4U, payload, payload_writer.position) != ENC_OK) {
        fclose(output);
        return 3;
    }
    if (fwrite(stream, 1U, stream_writer.position, output) !=
        stream_writer.position) {
        fclose(output);
        return 4;
    }
    return fclose(output) == 0 ? 0 : 4;
}
