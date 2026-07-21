#ifndef AV2H_DECODER_PARTITION_H
#define AV2H_DECODER_PARTITION_H

#include "partition_cdf.h"
#include "range.h"

typedef enum {
    AV2H_PARTITION_HORIZONTAL = 0,
    AV2H_PARTITION_VERTICAL = 1
} Av2hPartitionAxis;

typedef struct {
    Av2hPartitionAxis axis;
    size_t after_split_bits;
    size_t after_axis_bits;
    size_t after_extension_bits;
} Av2hOrdinaryPartitionPrefix;

Av2hStatus av2h_partition_ordinary_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    Av2hOrdinaryPartitionPrefix *prefix);

#endif