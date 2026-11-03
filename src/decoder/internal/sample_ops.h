#ifndef AV2H_DECODER_INTERNAL_SAMPLE_OPS_H
#define AV2H_DECODER_INTERNAL_SAMPLE_OPS_H

#include "base.h"
#include "decoder/internal/sample_math.h"

/*
 * Callers validate non-null storage, dimensions, strides, and address
 * arithmetic before entering these typed inner operations. Reference loads
 * additionally require nonzero width and height.
 */
#include "sample_kernel_u8.inc"
#include "sample_ops_kernel.inc"
#include "sample_kernel_end.inc"
#include "sample_kernel_u16.inc"
#include "sample_ops_kernel.inc"
#include "sample_kernel_end.inc"

#endif
