#include "encoder/obu.h"

EncStatus enc_leb128_write(EncByteWriter *writer, size_t value) {
    do {
        uint8_t byte = (uint8_t)(value & 0x7fU);
        value >>= 7U;
        if (value != 0U) {
            byte |= 0x80U;
        }
        if (enc_byte_writer_u8(writer, byte) != ENC_OK) {
            return writer != 0 ? writer->status : ENC_INVALID_ARGUMENT;
        }
    } while (value != 0U);
    return ENC_OK;
}

EncStatus enc_obu_write(EncByteWriter *writer, uint8_t obu_type,
                        const void *payload, size_t payload_size) {
    size_t obu_size;
    if (writer == 0 || obu_type >= 32U ||
        (payload == 0 && payload_size != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_add(payload_size, 1U, &obu_size)) {
        writer->status = ENC_OVERFLOW;
        return writer->status;
    }
    if (enc_leb128_write(writer, obu_size) != ENC_OK ||
        enc_byte_writer_u8(writer, (uint8_t)(obu_type << 2U)) != ENC_OK ||
        enc_byte_writer_write(writer, payload, payload_size) != ENC_OK) {
        return writer->status;
    }
    return ENC_OK;
}
