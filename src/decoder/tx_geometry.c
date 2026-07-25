#include "tx_geometry.h"

#define TX_DESC(id, ow, oh, cw, ch, ctx, sq, up, eob, capacity, signal)         \
    { id, ow, oh, cw, ch, ctx, sq, up, eob, capacity, signal }

static const Av2hTxDescriptor tx_descriptors[AV2H_TX_SIZE_COUNT] = {
    TX_DESC(AV2H_TX_4X4, 4U, 4U, 4U, 4U, 0U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_4,
            AV2H_TX_EOB_16, 16U, AV2H_TX_SIGNAL_INTRA_4),
    TX_DESC(AV2H_TX_8X8, 8U, 8U, 8U, 8U, 1U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_8,
            AV2H_TX_EOB_64, 64U, AV2H_TX_SIGNAL_INTRA_8),
    TX_DESC(AV2H_TX_16X16, 16U, 16U, 16U, 16U, 2U,
            AV2H_TX_SQUARE_16, AV2H_TX_SQUARE_16,
            AV2H_TX_EOB_256, 256U, AV2H_TX_SIGNAL_INTRA_16),
    TX_DESC(AV2H_TX_32X32, 32U, 32U, 32U, 32U, 3U,
            AV2H_TX_SQUARE_32, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_1024, 1024U, AV2H_TX_SIGNAL_DCT_ONLY),
    TX_DESC(AV2H_TX_64X64, 64U, 64U, 32U, 32U, 4U,
            AV2H_TX_SQUARE_64, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_1024, 1024U, AV2H_TX_SIGNAL_DCT_ONLY),
    TX_DESC(AV2H_TX_4X8, 4U, 8U, 4U, 8U, 1U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_8,
            AV2H_TX_EOB_32, 32U, AV2H_TX_SIGNAL_INTRA_4),
    TX_DESC(AV2H_TX_8X4, 8U, 4U, 8U, 4U, 1U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_8,
            AV2H_TX_EOB_32, 32U, AV2H_TX_SIGNAL_INTRA_4),
    TX_DESC(AV2H_TX_8X16, 8U, 16U, 8U, 16U, 2U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_16,
            AV2H_TX_EOB_128, 128U, AV2H_TX_SIGNAL_INTRA_8),
    TX_DESC(AV2H_TX_16X8, 16U, 8U, 16U, 8U, 2U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_16,
            AV2H_TX_EOB_128, 128U, AV2H_TX_SIGNAL_INTRA_8),
    TX_DESC(AV2H_TX_16X32, 16U, 32U, 16U, 32U, 3U,
            AV2H_TX_SQUARE_16, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_512, 512U, AV2H_TX_SIGNAL_HIGH_32),
    TX_DESC(AV2H_TX_32X16, 32U, 16U, 32U, 16U, 3U,
            AV2H_TX_SQUARE_16, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_512, 512U, AV2H_TX_SIGNAL_WIDE_32),
    TX_DESC(AV2H_TX_32X64, 32U, 64U, 32U, 32U, 4U,
            AV2H_TX_SQUARE_32, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_1024, 1024U, AV2H_TX_SIGNAL_DCT_ONLY),
    TX_DESC(AV2H_TX_64X32, 64U, 32U, 32U, 32U, 4U,
            AV2H_TX_SQUARE_32, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_1024, 1024U, AV2H_TX_SIGNAL_DCT_ONLY),
    TX_DESC(AV2H_TX_4X16, 4U, 16U, 4U, 16U, 1U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_16,
            AV2H_TX_EOB_64, 64U, AV2H_TX_SIGNAL_INTRA_4),
    TX_DESC(AV2H_TX_16X4, 16U, 4U, 16U, 4U, 1U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_16,
            AV2H_TX_EOB_64, 64U, AV2H_TX_SIGNAL_INTRA_4),
    TX_DESC(AV2H_TX_8X32, 8U, 32U, 8U, 32U, 2U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_256, 256U, AV2H_TX_SIGNAL_HIGH_32),
    TX_DESC(AV2H_TX_32X8, 32U, 8U, 32U, 8U, 2U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_256, 256U, AV2H_TX_SIGNAL_WIDE_32),
    TX_DESC(AV2H_TX_16X64, 16U, 64U, 16U, 32U, 3U,
            AV2H_TX_SQUARE_16, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_512, 512U, AV2H_TX_SIGNAL_HIGH_64),
    TX_DESC(AV2H_TX_64X16, 64U, 16U, 32U, 16U, 3U,
            AV2H_TX_SQUARE_16, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_512, 512U, AV2H_TX_SIGNAL_WIDE_64),
    TX_DESC(AV2H_TX_4X32, 4U, 32U, 4U, 32U, 2U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_128, 128U, AV2H_TX_SIGNAL_HIGH_32),
    TX_DESC(AV2H_TX_32X4, 32U, 4U, 32U, 4U, 2U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_32,
            AV2H_TX_EOB_128, 128U, AV2H_TX_SIGNAL_WIDE_32),
    TX_DESC(AV2H_TX_8X64, 8U, 64U, 8U, 32U, 3U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_256, 256U, AV2H_TX_SIGNAL_HIGH_64),
    TX_DESC(AV2H_TX_64X8, 64U, 8U, 32U, 8U, 3U,
            AV2H_TX_SQUARE_8, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_256, 256U, AV2H_TX_SIGNAL_WIDE_64),
    TX_DESC(AV2H_TX_4X64, 4U, 64U, 4U, 32U, 2U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_128, 128U, AV2H_TX_SIGNAL_HIGH_64),
    TX_DESC(AV2H_TX_64X4, 64U, 4U, 32U, 4U, 2U,
            AV2H_TX_SQUARE_4, AV2H_TX_SQUARE_64,
            AV2H_TX_EOB_128, 128U, AV2H_TX_SIGNAL_WIDE_64)
};

static int tx_dimension_class(uint8_t dimension) {
    return dimension == 4U ? AV2H_TX_SQUARE_4 :
           dimension == 8U ? AV2H_TX_SQUARE_8 :
           dimension == 16U ? AV2H_TX_SQUARE_16 :
           dimension == 32U ? AV2H_TX_SQUARE_32 :
           dimension == 64U ? AV2H_TX_SQUARE_64 : -1;
}

static int tx_descriptor_valid(const Av2hTxDescriptor *descriptor,
                               Av2hFirstTxSize tx_size) {
    Av2hTxSignalingClass signaling_class;
    uint8_t adjusted_width;
    uint8_t adjusted_height;
    uint16_t capacity;
    uint16_t class_capacity;
    int width_class;
    int height_class;
    int square_class;
    int square_up_class;
    if (descriptor == 0 || descriptor->tx_size != tx_size ||
        descriptor->output_width == 0U || descriptor->output_height == 0U) {
        return 0;
    }
    width_class = tx_dimension_class(descriptor->output_width);
    height_class = tx_dimension_class(descriptor->output_height);
    if (width_class < 0 || height_class < 0) return 0;
    square_class = width_class < height_class ? width_class : height_class;
    square_up_class = width_class > height_class ? width_class : height_class;
    if (square_up_class > AV2H_TX_SQUARE_32) {
        signaling_class = square_class >= AV2H_TX_SQUARE_32 ?
            AV2H_TX_SIGNAL_DCT_ONLY :
            width_class > height_class ? AV2H_TX_SIGNAL_WIDE_64 :
                                         AV2H_TX_SIGNAL_HIGH_64;
    } else if (square_up_class == AV2H_TX_SQUARE_32) {
        signaling_class = square_class == AV2H_TX_SQUARE_32 ?
            AV2H_TX_SIGNAL_DCT_ONLY :
            width_class > height_class ? AV2H_TX_SIGNAL_WIDE_32 :
                                         AV2H_TX_SIGNAL_HIGH_32;
    } else {
        signaling_class = square_class == AV2H_TX_SQUARE_4 ?
            AV2H_TX_SIGNAL_INTRA_4 :
            square_class == AV2H_TX_SQUARE_8 ? AV2H_TX_SIGNAL_INTRA_8 :
                                               AV2H_TX_SIGNAL_INTRA_16;
    }
    adjusted_width =
        descriptor->output_width > 32U ? 32U : descriptor->output_width;
    adjusted_height =
        descriptor->output_height > 32U ? 32U : descriptor->output_height;
    capacity = (uint16_t)adjusted_width * adjusted_height;
    class_capacity = (uint16_t)(16U << descriptor->eob_class);
    return descriptor->coefficient_width == adjusted_width &&
           descriptor->coefficient_height == adjusted_height &&
           descriptor->eob_capacity == capacity &&
           descriptor->eob_capacity == class_capacity &&
           descriptor->square_class == (Av2hTxSquareClass)square_class &&
           descriptor->square_up_class ==
               (Av2hTxSquareClass)square_up_class &&
           descriptor->tx_size_context ==
               (uint8_t)((descriptor->square_class +
                          descriptor->square_up_class + 1U) >> 1U) &&
           descriptor->signaling_class == signaling_class;
}

Av2hStatus av2h_tx_descriptor_get(
    Av2hFirstTxSize tx_size,
    const Av2hTxDescriptor **descriptor) {
    const Av2hTxDescriptor *candidate;
    if (descriptor == 0) return AV2H_INVALID_ARGUMENT;
    *descriptor = 0;
    if ((unsigned int)tx_size >= AV2H_TX_SIZE_COUNT) {
        return AV2H_UNSUPPORTED;
    }
    candidate = &tx_descriptors[(unsigned int)tx_size];
    if (!tx_descriptor_valid(candidate, tx_size)) return AV2H_UNSUPPORTED;
    *descriptor = candidate;
    return AV2H_OK;
}

Av2hStatus av2h_luma_tx_descriptor_get(
    Av2hBlockSize block_size,
    const Av2hTxDescriptor **descriptor) {
    Av2hFirstTxSize tx_size;
    switch (block_size) {
        case AV2H_BLOCK_4X4: tx_size = AV2H_TX_4X4; break;
        case AV2H_BLOCK_4X8: tx_size = AV2H_TX_4X8; break;
        case AV2H_BLOCK_8X4: tx_size = AV2H_TX_8X4; break;
        case AV2H_BLOCK_8X8: tx_size = AV2H_TX_8X8; break;
        case AV2H_BLOCK_8X16: tx_size = AV2H_TX_8X16; break;
        case AV2H_BLOCK_16X8: tx_size = AV2H_TX_16X8; break;
        case AV2H_BLOCK_16X16: tx_size = AV2H_TX_16X16; break;
        case AV2H_BLOCK_16X32: tx_size = AV2H_TX_16X32; break;
        case AV2H_BLOCK_32X16: tx_size = AV2H_TX_32X16; break;
        case AV2H_BLOCK_32X32: tx_size = AV2H_TX_32X32; break;
        case AV2H_BLOCK_32X64: tx_size = AV2H_TX_32X64; break;
        case AV2H_BLOCK_64X32: tx_size = AV2H_TX_64X32; break;
        case AV2H_BLOCK_64X64: tx_size = AV2H_TX_64X64; break;
        case AV2H_BLOCK_4X16: tx_size = AV2H_TX_4X16; break;
        case AV2H_BLOCK_16X4: tx_size = AV2H_TX_16X4; break;
        case AV2H_BLOCK_8X32: tx_size = AV2H_TX_8X32; break;
        case AV2H_BLOCK_32X8: tx_size = AV2H_TX_32X8; break;
        case AV2H_BLOCK_16X64: tx_size = AV2H_TX_16X64; break;
        case AV2H_BLOCK_64X16: tx_size = AV2H_TX_64X16; break;
        case AV2H_BLOCK_4X32: tx_size = AV2H_TX_4X32; break;
        case AV2H_BLOCK_32X4: tx_size = AV2H_TX_32X4; break;
        case AV2H_BLOCK_8X64: tx_size = AV2H_TX_8X64; break;
        case AV2H_BLOCK_64X8: tx_size = AV2H_TX_64X8; break;
        default:
            if (descriptor != 0) *descriptor = 0;
            return descriptor == 0 ? AV2H_INVALID_ARGUMENT :
                                     AV2H_UNSUPPORTED;
    }
    return av2h_tx_descriptor_get(tx_size, descriptor);
}
