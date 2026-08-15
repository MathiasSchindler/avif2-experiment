#ifndef EXPERIMENTAL_ENCODER_RANGE_H
#define EXPERIMENTAL_ENCODER_RANGE_H

#include "encoder/base.h"

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t position;
    size_t symbols_out;
    uint64_t low;
    uint32_t range;
    int count;
    int pending_byte;
    size_t pending_ff;
    EncStatus status;
    int finished;
} EncRangeWriter;

void enc_range_writer_init(EncRangeWriter *writer, void *data, size_t capacity);
EncStatus enc_range_writer_bypass(EncRangeWriter *writer, unsigned int bit);
EncStatus enc_range_writer_literal(EncRangeWriter *writer, uint32_t value,
                                   unsigned int bit_count);
/*
 * The first symbol_count entries are descending inverse-Q15 thresholds ending
 * in zero. Adaptive tables additionally hold the count at [symbol_count] and
 * the three normative rate offsets at the following indices.
 */
EncStatus enc_range_writer_symbol(EncRangeWriter *writer, uint32_t symbol,
                                  uint16_t *inverse_cdf,
                                  size_t symbol_count, int update_cdf);
EncStatus enc_range_cdf_update(uint16_t *inverse_cdf, size_t symbol_count,
                               uint32_t symbol);
size_t enc_range_writer_tell(const EncRangeWriter *writer);
size_t enc_range_writer_bytes_used(const EncRangeWriter *writer);
EncStatus enc_range_writer_finish(EncRangeWriter *writer);
EncStatus enc_range_writer_status(const EncRangeWriter *writer);

#endif
