#include "encoder/m1.h"
#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

EncStatus enc_m1_constant_128_8x8(void *data, size_t capacity, size_t *size) {
    uint8_t sequence[9U];
    uint8_t frame_header[4U];
    uint8_t tile[8U];
    uint8_t frame_payload[12U];
    EncByteWriter payload_writer;
    EncByteWriter writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    size_t frame_payload_size;
    EncStatus status;

    if (size == 0 || (data == 0 && capacity != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    status = enc_m1_sequence_header(
        sequence, sizeof(sequence), &sequence_size);
    if (status != ENC_OK) return status;
    status = enc_m1_frame_header(
        frame_header, sizeof(frame_header), &frame_header_size);
    if (status != ENC_OK) return status;
    status = enc_m1_tile(tile, sizeof(tile), &tile_size);
    if (status != ENC_OK) return status;

    enc_byte_writer_init(
        &payload_writer, frame_payload, sizeof(frame_payload));
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    frame_payload_size = payload_writer.position;
    if (sequence_size != sizeof(sequence) ||
        frame_header_size != sizeof(frame_header) || tile_size != 2U ||
        capacity < ENC_M1_STREAM_SIZE) {
        return ENC_OUT_OF_SPACE;
    }
    enc_byte_writer_init(&writer, data, capacity);
    if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &writer, 4U, frame_payload, frame_payload_size) != ENC_OK) {
        return writer.status;
    }
    *size = writer.position;
    return *size == ENC_M1_STREAM_SIZE ? ENC_OK : ENC_OVERFLOW;
}
