#include "encoder/syntax.h"

static int bytes_equal(
    const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

int main(void) {
    static const uint8_t shared_sequence[] = {
        0x92U, 0x06U, 0x88U, 0xbfU, 0x1cU, 0x39U,
        0x4dU, 0xf0U, 0x6fU, 0x02U, 0x20U
    };
    static const uint8_t shared_frame[] = {
        0xe3U, 0x1bU, 0xf0U, 0x08U
    };
    static const uint8_t independent_sequence[] = {
        0x92U, 0x06U, 0x88U, 0xbfU, 0x1cU, 0x39U,
        0x4dU, 0xf0U, 0xaeU, 0xafU, 0x50U, 0x22U
    };
    static const uint8_t independent_frame[] = {
        0xe3U, 0x1bU, 0xf8U, 0x88U, 0x90U, 0xf0U, 0x04U
    };
    EncQuantizerDeltaConfig config;
    EncFrameQuantizerDeltas deltas;
    uint8_t output[32U];
    size_t size;

    config = (EncQuantizerDeltaConfig){0, 1, 0, 0, 0, 0, 0, 1};
    deltas = (EncFrameQuantizerDeltas){0, -8, -8, -8, -8};
    if (enc_m3_sequence_header_for_qdelta(
            output, sizeof(output), &size, 8U, 8U, 2U, &config) != ENC_OK ||
        size != sizeof(shared_sequence) ||
        !bytes_equal(output, shared_sequence, size)) return 1;
    if (enc_m3_frame_header_qdelta_for_size(
            output, sizeof(output), &size, 8U, 8U, 141U,
            &config, &deltas) != ENC_OK ||
        size != sizeof(shared_frame) ||
        !bytes_equal(output, shared_frame, size)) return 2;

    config = (EncQuantizerDeltaConfig){1, 0, 0, 0, -2, 1, 3, 1};
    deltas = (EncFrameQuantizerDeltas){0, -8, 8, 16, -16};
    if (enc_m3_sequence_header_for_qdelta(
            output, sizeof(output), &size, 8U, 8U, 2U, &config) != ENC_OK ||
        size != sizeof(independent_sequence) ||
        !bytes_equal(output, independent_sequence, size)) return 3;
    if (enc_m3_frame_header_qdelta_for_size(
            output, sizeof(output), &size, 8U, 8U, 141U,
            &config, &deltas) != ENC_OK ||
        size != sizeof(independent_frame) ||
        !bytes_equal(output, independent_frame, size)) return 4;

    deltas.u_ac = -65;
    return enc_m3_frame_header_qdelta_for_size(
               output, sizeof(output), &size, 8U, 8U, 141U,
               &config, &deltas) == ENC_INVALID_ARGUMENT ? 0 : 5;
}
