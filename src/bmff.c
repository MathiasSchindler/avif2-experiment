#include "base.h"

#include "error.h"

#include <stddef.h>

static Av2hStatus box_fail(Av2hError *error,
                           Av2hStatus status,
                           size_t offset) {
    return av2h_error_set_core(error, status, offset, AV2H_CONTEXT_BOX);
}

Av2hStatus av2h_box_header_parse(Av2hSpan input,
                                 size_t offset,
                                 Av2hBoxHeader *box,
                                 Av2hError *error) {
    uint32_t size32;
    uint64_t box_size;
    size_t header_size = 8U;
    if (box == NULL || (input.data == NULL && input.size != 0U) || offset > input.size) {
        return box_fail(error, AV2H_INVALID_ARGUMENT, offset);
    }
    if (input.size - offset < 8U) return box_fail(error, AV2H_TRUNCATED, offset);
    size32 = av2h_load_u32be(input.data + offset);
    box->type = av2h_load_u32be(input.data + offset + 4U);
    if (size32 == 1U) {
        if (input.size - offset < 16U) return box_fail(error, AV2H_TRUNCATED, offset);
        box_size = av2h_load_u64be(input.data + offset + 8U);
        header_size = 16U;
    } else if (size32 == 0U) {
        box_size = (uint64_t)(input.size - offset);
    } else {
        box_size = size32;
    }
    if (box_size < header_size) {
        return box_fail(error, AV2H_INVALID_DATA, offset);
    }
    if (box_size > (uint64_t)(input.size - offset)) {
        return box_fail(error, AV2H_TRUNCATED, input.size);
    }
    box->offset = offset;
    box->header_size = header_size;
    box->size = box_size;
    box->payload_size = box_size - header_size;
    return AV2H_OK;
}

Av2hStatus av2h_box_header_write(uint32_t type,
                                 uint64_t payload_size,
                                 void *output,
                                 size_t output_size,
                                 size_t *written) {
    Av2hByteWriter writer;
    uint64_t total = payload_size + 8U;
    int large = total > UINT32_MAX || total < payload_size;
    size_t header_size = large ? 16U : 8U;
    if (written == NULL || (output == NULL && output_size != 0U)) return AV2H_INVALID_ARGUMENT;
    if (large) {
        total = payload_size + 16U;
        if (total < payload_size) return AV2H_OVERFLOW;
    }
    *written = header_size;
    if (output == NULL && output_size == 0U) return AV2H_OK;
    av2h_byte_writer_init(&writer, output, output_size);
    if (header_size > output_size) return AV2H_OUT_OF_MEMORY;
    (void)av2h_byte_writer_u32be(&writer, large ? 1U : (uint32_t)total);
    (void)av2h_byte_writer_u32be(&writer, type);
    if (large) {
        (void)av2h_byte_writer_u32be(&writer, (uint32_t)(total >> 32));
        (void)av2h_byte_writer_u32be(&writer, (uint32_t)total);
    }
    return writer.status;
}