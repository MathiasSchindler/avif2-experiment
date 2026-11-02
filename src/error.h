#ifndef AV2H_ERROR_H
#define AV2H_ERROR_H

#include "av2heif.h"

/* Preserve extended diagnostics unless the caller explicitly starts new ones. */
static inline Av2hStatus av2h_error_set_core(
    Av2hError *error, Av2hStatus status, size_t offset, uint32_t context) {
    if (error != 0) {
        error->status = status;
        error->offset = offset;
        error->context = context;
        error->obu_index = SIZE_MAX;
        error->obu_type = 0U;
    }
    return status;
}

static inline void av2h_error_reset_diagnostics(
    Av2hError *error, uint8_t stage, uint8_t reason) {
    if (error == 0) return;
    error->diagnostic_stage = stage;
    error->diagnostic_reason = reason;
    error->syntax_stage = 0U;
    error->plane = 0U;
    error->row = 0U;
    error->column = 0U;
    error->entropy_tell = 0U;
    error->parsed_value = 0U;
}

#endif
