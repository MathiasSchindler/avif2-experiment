#ifndef AV2H_DECODER_INTERNAL_OBU_H
#define AV2H_DECODER_INTERNAL_OBU_H

#include "base.h"
#include "error.h"

static inline uint8_t av2h_obu_diagnostic_reason(Av2hStatus status) {
    if (status == AV2H_TRUNCATED) return AV2H_DIAGNOSTIC_REASON_TRUNCATED;
    if (status == AV2H_INVALID_DATA) return AV2H_DIAGNOSTIC_REASON_MALFORMED;
    if (status == AV2H_UNSUPPORTED) {
        return AV2H_DIAGNOSTIC_REASON_UNSUPPORTED_TUPLE;
    }
    if (status == AV2H_INVALID_ARGUMENT) {
        return AV2H_DIAGNOSTIC_REASON_ARGUMENT;
    }
    return AV2H_DIAGNOSTIC_REASON_NONE;
}

static inline Av2hStatus av2h_obu_fail(Av2hError *error,
                       Av2hStatus status,
                       size_t offset,
                       uint32_t context) {
    if (error != 0) {
        (void)av2h_error_set_core(error, status, offset, context);
        av2h_error_reset_diagnostics(
            error, context == AV2H_CONTEXT_SEQUENCE_HEADER ?
            AV2H_DIAGNOSTIC_STAGE_SEQUENCE_HEADER :
            AV2H_DIAGNOSTIC_STAGE_NONE, av2h_obu_diagnostic_reason(status));
    }
    return status;
}

static inline Av2hStatus av2h_obu_fail_at(Av2hError *error,
                           Av2hStatus status,
                           const Av2hObu *obu,
                           size_t offset,
                           uint32_t context) {
    (void)av2h_obu_fail(error, status, offset, context);
    if (error != 0 && obu != 0) {
        error->obu_index = obu->index;
        error->obu_type = obu->type;
    }
    return status;
}

static inline int av2h_obu_check_trailing_bits(Av2hBitReader *reader) {
    size_t remaining = av2h_bit_reader_remaining(reader);
    size_t index;
    if (remaining == 0U || remaining > 8U) return 0;
    if (av2h_bit_reader_read(reader, 1U) != 1U) return 0;
    for (index = 1U; index < remaining; ++index) {
        if (av2h_bit_reader_read(reader, 1U) != 0U) return 0;
    }
    return reader->status == AV2H_OK;
}

#endif
