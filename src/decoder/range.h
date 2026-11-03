#ifndef AV2H_DECODER_RANGE_H
#define AV2H_DECODER_RANGE_H

#include "base.h"

struct Av2hDecodeDiagnostics;

typedef struct {
    const Av2hSpan *spans;
    size_t span_count;
    size_t size;
    size_t bit_position;
    uint32_t range;
    uint32_t value;
    int64_t available_bits;
    size_t tell;
    Av2hStatus status;
    struct Av2hDecodeDiagnostics *diagnostics;
} Av2hRangeDecoder;

Av2hStatus av2h_range_init(Av2hRangeDecoder *decoder, const Av2hSpan *spans, size_t span_count);
uint32_t av2h_range_read_symbol(Av2hRangeDecoder *decoder, const uint16_t *inverse_cdf,
                                size_t symbol_count);
uint32_t av2h_range_read_adaptive(Av2hRangeDecoder *decoder, uint16_t *inverse_cdf,
                                  size_t symbol_count);
uint32_t av2h_range_read_bypass(Av2hRangeDecoder *decoder, unsigned int bit_count);
size_t av2h_range_tell(const Av2hRangeDecoder *decoder);
Av2hStatus av2h_range_status(const Av2hRangeDecoder *decoder);

#endif