#ifndef AV2H_DECODER_LEAF_H
#define AV2H_DECODER_LEAF_H

#include "frame.h"
#include "partition.h"

#define AV2H_INTRABC_CONTEXT_COUNT 3U
#define AV2H_CCSO_CONTEXT_COUNT 4U
#define AV2H_Y_MODE_CONTEXT_COUNT 3U
#define AV2H_Y_MODE_SET_SYMBOLS 4U
#define AV2H_Y_MODE_INDEX_SYMBOLS 8U
#define AV2H_Y_MODE_OFFSET_SYMBOLS 6U
#define AV2H_FSC_MODE_CONTEXT_COUNT 4U
#define AV2H_FSC_BLOCK_CONTEXT_COUNT 6U

typedef struct {
    uint16_t intrabc[AV2H_INTRABC_CONTEXT_COUNT][AV2H_BINARY_CDF_SIZE];
    uint16_t ccso[AV2H_FRAME_PLANE_COUNT][AV2H_CCSO_CONTEXT_COUNT]
                 [AV2H_BINARY_CDF_SIZE];
    uint16_t y_mode_set[AV2H_Y_MODE_SET_SYMBOLS + 4U];
    uint16_t y_mode_index[AV2H_Y_MODE_CONTEXT_COUNT]
                         [AV2H_Y_MODE_INDEX_SYMBOLS + 4U];
    uint16_t y_mode_offset[AV2H_Y_MODE_CONTEXT_COUNT]
                          [AV2H_Y_MODE_OFFSET_SYMBOLS + 4U];
    uint16_t fsc_mode[AV2H_FSC_MODE_CONTEXT_COUNT]
                     [AV2H_FSC_BLOCK_CONTEXT_COUNT]
                     [AV2H_BINARY_CDF_SIZE];
} Av2hLeafCdfs;

typedef enum {
    AV2H_INTRA_DC = 0,
    AV2H_INTRA_SMOOTH = 9,
    AV2H_INTRA_SMOOTH_VERTICAL = 10,
    AV2H_INTRA_SMOOTH_HORIZONTAL = 11,
    AV2H_INTRA_PAETH = 12
} Av2hFirstLeafIntraMode;

typedef struct {
    uint8_t use_intrabc;
    uint8_t ccso_present_mask;
    uint8_t ccso_block_control[AV2H_FRAME_PLANE_COUNT];
    uint8_t mode_set;
    uint8_t mode_index;
    uint8_t fsc_mode;
    uint8_t fsc_block_context;
    Av2hFirstLeafIntraMode mode;
    size_t before_mode_bits;
    size_t after_mode_set_bits;
    size_t after_mode_index_bits;
    size_t after_fsc_bits;
} Av2hFirstLeafMode;

Av2hStatus av2h_leaf_cdfs_init(Av2hLeafCdfs *cdfs);
Av2hStatus av2h_first_leaf_mode_read(Av2hRangeDecoder *decoder,
                                     Av2hLeafCdfs *cdfs,
                                     const Av2hSequenceState *sequence,
                                     const Av2hFrameState *frame,
                                     const Av2hPartitionNode *leaf,
                                     Av2hFirstLeafMode *mode);

#endif