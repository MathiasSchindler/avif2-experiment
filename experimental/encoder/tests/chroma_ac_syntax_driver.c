#include <stdio.h>
#include <stdlib.h>

#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

int main(int argc, char **argv) {
    int16_t y[64U] = {0};
    int16_t u[256U] = {0};
    int16_t v[256U] = {0};
    uint8_t sequence[16U];
    uint8_t frame[16U];
    uint8_t tile[1024U];
    uint8_t payload[2048U];
    uint8_t stream[4096U];
    EncByteWriter payload_writer;
    EncByteWriter stream_writer;
    size_t sequence_size;
    size_t frame_size;
    size_t tile_size;
    unsigned long position;
    long level;
    unsigned long plane;
    FILE *file;
    if (argc != 5) return 2;
    plane = strtoul(argv[1], 0, 10);
    position = strtoul(argv[2], 0, 10);
    level = strtol(argv[3], 0, 10);
    if (plane > 2U || position >= 64U ||
        level < -32767L || level > 32767L) {
        return 2;
    }
    (plane == 2U ? v : u)[position] = (int16_t)level;
    if (enc_m3_sequence_header_for_format(
            sequence, sizeof(sequence), &sequence_size,
            8U, 8U, 2U) != ENC_OK ||
        enc_m3_frame_header_q_for_size(
            frame, sizeof(frame), &frame_size, 8U, 8U, 100U) != ENC_OK ||
        (plane == 0U
            ? enc_ac8x8_yuv_grid_tile_q(
                  tile, sizeof(tile), &tile_size, 8U, 8U,
                  y, u, v, 1U, 2U, 100U)
            : enc_ac8x8_yuv_chroma_grid_tile_q(
                  tile, sizeof(tile), &tile_size, 8U, 8U,
                  y, u, v, 1U, 64U, 2U, 100U)) != ENC_OK) {
        return 1;
    }
    enc_byte_writer_init(&payload_writer, payload, sizeof(payload));
    if (enc_byte_writer_write(
            &payload_writer, frame, frame_size) != ENC_OK ||
        enc_byte_writer_write(
            &payload_writer, tile, tile_size) != ENC_OK) {
        return 1;
    }
    enc_byte_writer_init(&stream_writer, stream, sizeof(stream));
    if (enc_obu_write(&stream_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 4U, payload,
            payload_writer.position) != ENC_OK) {
        return 1;
    }
    file = fopen(argv[4], "wb");
    if (file == 0 ||
        fwrite(stream, 1U, stream_writer.position, file) !=
            stream_writer.position ||
        fclose(file) != 0) {
        return 1;
    }
    return 0;
}
