#ifndef AV2H_DECODER_PARTITION_CDF_H
#define AV2H_DECODER_PARTITION_CDF_H

#include "cdf.h"

#define AV2H_PARTITION_STRUCTURE_COUNT 2U
#define AV2H_PARTITION_CONTEXT_COUNT 64U
#define AV2H_SQUARE_SPLIT_CONTEXT_COUNT 8U

typedef struct {
    uint16_t do_split[AV2H_PARTITION_STRUCTURE_COUNT]
                     [AV2H_PARTITION_CONTEXT_COUNT][AV2H_BINARY_CDF_SIZE];
    uint16_t do_square_split[AV2H_PARTITION_STRUCTURE_COUNT]
                            [AV2H_SQUARE_SPLIT_CONTEXT_COUNT]
                            [AV2H_BINARY_CDF_SIZE];
    uint16_t rect_type[AV2H_PARTITION_STRUCTURE_COUNT]
                      [AV2H_PARTITION_CONTEXT_COUNT][AV2H_BINARY_CDF_SIZE];
    uint16_t do_ext_partition[AV2H_PARTITION_STRUCTURE_COUNT]
                             [AV2H_PARTITION_CONTEXT_COUNT]
                             [AV2H_BINARY_CDF_SIZE];
    uint16_t do_uneven_4way_partition[AV2H_PARTITION_STRUCTURE_COUNT]
                                     [AV2H_PARTITION_CONTEXT_COUNT]
                                     [AV2H_BINARY_CDF_SIZE];
} Av2hPartitionCdfs;

Av2hStatus av2h_partition_cdfs_init(Av2hPartitionCdfs *context);

#endif