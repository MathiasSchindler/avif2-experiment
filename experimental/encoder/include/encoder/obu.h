#ifndef EXPERIMENTAL_ENCODER_OBU_H
#define EXPERIMENTAL_ENCODER_OBU_H

#include "encoder/writer.h"

EncStatus enc_leb128_write(EncByteWriter *writer, size_t value);
EncStatus enc_obu_write(EncByteWriter *writer, uint8_t obu_type,
                        const void *payload, size_t payload_size);

#endif
