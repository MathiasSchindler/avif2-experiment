#ifndef AV2H_DECODER_INTERNAL_FILTER_STAGE_H
#define AV2H_DECODER_INTERNAL_FILTER_STAGE_H

#include "decoder/internal/filter_modules.h"
#include "decoder/internal/parallel.h"

#include <stddef.h>

typedef enum {
    AV2H_FILTER_STAGE_CDEF,
    AV2H_FILTER_STAGE_CCSO,
    AV2H_FILTER_STAGE_PC_WIENER,
    AV2H_FILTER_STAGE_WIENER_NONSEP,
    AV2H_FILTER_STAGE_GDF
} Av2hFilterStage;

/* Instantiate typed kernels after the shared helpers. */
#include "sample_kernel_u8.inc"
#include "filter_stage_kernel.inc"
#include "sample_kernel_end.inc"
#include "sample_kernel_u16.inc"
#include "filter_stage_kernel.inc"
#include "sample_kernel_end.inc"

#endif
