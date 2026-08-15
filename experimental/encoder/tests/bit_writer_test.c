#include "encoder/writer.h"

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0; index < size; ++index) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    uint8_t storage[4];
    const uint8_t expected[] = {0xb3, 0xab, 0xc0};
    EncBitWriter writer;
    EncBitWriter short_writer;

    enc_bit_writer_init(&writer, storage, sizeof(storage));
    if (enc_bit_writer_write(&writer, 5, 3) != ENC_OK ||
        enc_bit_writer_write(&writer, 19, 5) != ENC_OK ||
        enc_bit_writer_write(&writer, 0xabc, 12) != ENC_OK ||
        enc_bit_writer_align_zero(&writer) != ENC_OK ||
        enc_bit_writer_bytes_used(&writer) != sizeof(expected) ||
        !bytes_equal(storage, expected, sizeof(expected))) {
        return 1;
    }
    if (enc_bit_writer_write(&writer, 0, 33) != ENC_INVALID_ARGUMENT) {
        return 2;
    }
    enc_bit_writer_init(&short_writer, storage, 1);
    if (enc_bit_writer_write(&short_writer, 0x1ff, 9) != ENC_OUT_OF_SPACE ||
        short_writer.bit_position != 0) {
        return 3;
    }
    return 0;
}
