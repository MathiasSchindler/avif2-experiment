#include <stdio.h>
#include <stdlib.h>

#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

int main(int argc, char **argv) {
    int16_t y[16U] = {0};
    int16_t u[16U] = {0};
    int16_t v[16U] = {0};
    uint8_t sequence[16U];
    uint8_t frame[16U];
    uint8_t tile[2048U];
    uint8_t payload[4096U];
    uint8_t stream[8192U];
    EncByteWriter payload_writer;
    EncByteWriter stream_writer;
    size_t sequence_size;
    size_t frame_size;
    size_t tile_size;
    unsigned long format;
    unsigned long luma_mode;
    unsigned long chroma_mode;
    uint8_t luma_token;
    uint8_t chroma_token;
    FILE *file;
    if (argc != 5) return 2;
    format = strtoul(argv[1], 0, 10);
    luma_mode = strtoul(argv[2], 0, 10);
    chroma_mode = strtoul(argv[3], 0, 10);
    if (format > 2U || luma_mode > 64U || chroma_mode > 64U) return 2;
    luma_token = (uint8_t)luma_mode;
    chroma_token = (uint8_t)chroma_mode;
    if ((format == 1U
            ? enc_m3_sequence_header(
                  sequence, sizeof(sequence), &sequence_size, 32U, 32U)
            : enc_m3_sequence_header_for_format(
                  sequence, sizeof(sequence), &sequence_size,
                  32U, 32U, (unsigned int)format)) != ENC_OK) {
        return 3;
    }
    if (enc_m3_frame_header_q_for_size(
            frame, sizeof(frame), &frame_size, 32U, 32U, 100U) != ENC_OK) {
        return 4;
    }
    if ((format == 1U
            ? enc_m3_intra_grid_tile_q(
                  tile, sizeof(tile), &tile_size, 32U, 32U,
                  y, 16U, &luma_token, 1U, 100U)
            : enc_m3_intra_yuv_grid_tile_q(
                  tile, sizeof(tile), &tile_size, 32U, 32U,
                  y, u, v, 16U,
                  &luma_token, 1U, &chroma_token, 1U,
                  (unsigned int)format, 100U)) != ENC_OK) {
        return 5;
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
