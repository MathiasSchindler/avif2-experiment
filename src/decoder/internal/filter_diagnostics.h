#ifndef AV2H_DECODER_INTERNAL_FILTER_DIAGNOSTICS_H
#define AV2H_DECODER_INTERNAL_FILTER_DIAGNOSTICS_H

#include "decoder/filter.h"

typedef struct {
    uint8_t reason;
} Av2hFilterDiagnostics;

const Av2hFilterDiagnostics *av2h_filter_diagnostics_get(void);

#endif