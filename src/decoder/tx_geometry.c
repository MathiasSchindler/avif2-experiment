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
        case AV2H_BLOCK_64X128: tx_size = AV2H_TX_64X64; break;
        case AV2H_BLOCK_128X64: tx_size = AV2H_TX_64X64; break;
        case AV2H_BLOCK_128X128: tx_size = AV2H_TX_64X64; break;
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

Av2hStatus av2h_luma_tx_node_count(
        Av2hBlockSize block_size,
        uint8_t *count) {
        const Av2hTxDescriptor *descriptor;
        Av2hStatus status;
        if (count == 0) return AV2H_INVALID_ARGUMENT;
        status = av2h_luma_tx_descriptor_get(block_size, &descriptor);
        if (status != AV2H_OK) return status;
        (void)descriptor;
        *count = block_size == AV2H_BLOCK_128X128 ? 4U :
                block_size == AV2H_BLOCK_64X128 ||
                block_size == AV2H_BLOCK_128X64 ? 2U : 1U;
        return AV2H_OK;
}

Av2hStatus av2h_luma_tx_node_get(
        const Av2hPartitionNode *coding_node,
        uint8_t index,
        Av2hPartitionNode *tx_node) {
        uint8_t count;
        Av2hStatus status;
        if (coding_node == 0 || tx_node == 0) return AV2H_INVALID_ARGUMENT;
        status = av2h_luma_tx_node_count(coding_node->block_size, &count);
        if (status != AV2H_OK) return status;
        if (index >= count) return AV2H_INVALID_ARGUMENT;
        *tx_node = *coding_node;
        if (count == 1U) return AV2H_OK;
        tx_node->block_size = AV2H_BLOCK_64X64;
        if (coding_node->block_size == AV2H_BLOCK_64X128) {
                tx_node->row = (uint16_t)(tx_node->row + index * 16U);
        } else if (coding_node->block_size == AV2H_BLOCK_128X64) {
                tx_node->column = (uint16_t)(tx_node->column + index * 16U);
        } else {
                tx_node->row = (uint16_t)(tx_node->row + (index / 2U) * 16U);
                tx_node->column = (uint16_t)(
                        tx_node->column + (index % 2U) * 16U);
        }
        return AV2H_OK;
}

Av2hStatus av2h_chroma_tx_descriptor_get(
    Av2hBlockSize block_size,
        uint8_t chroma_format_idc,
    const Av2hTxDescriptor **descriptor) {
    size_t width_mi;
    size_t height_mi;
    uint8_t width;
    uint8_t height;
    size_t index;
    Av2hStatus status;
    if (descriptor == 0) return AV2H_INVALID_ARGUMENT;
    *descriptor = 0;
    status = av2h_block_dimensions_mi(block_size, &width_mi, &height_mi);
    if (status != AV2H_OK) return status;
    if (width_mi > 32U || height_mi > 32U) return AV2H_UNSUPPORTED;
        if (chroma_format_idc != 0U && chroma_format_idc != 2U) {
                return AV2H_UNSUPPORTED;
        }
        width = (uint8_t)(width_mi * (chroma_format_idc == 2U ? 4U : 2U));
        height = (uint8_t)(height_mi * (chroma_format_idc == 2U ? 4U : 2U));
        if (width < 4U) width = 4U;
        if (height < 4U) height = 4U;
    for (index = 0U; index < AV2H_TX_SIZE_COUNT; ++index) {
        const Av2hTxDescriptor *candidate = &tx_descriptors[index];
        if (candidate->output_width == width &&
            candidate->output_height == height &&
            tx_descriptor_valid(candidate, candidate->tx_size)) {
            *descriptor = candidate;
            return AV2H_OK;
        }
    }
    return AV2H_UNSUPPORTED;
}

Av2hStatus av2h_chroma_tx_node_count(
        Av2hBlockSize block_size,
        uint8_t chroma_format_idc,
        uint8_t *count) {
        size_t width_mi;
        size_t height_mi;
        size_t width_pixels;
        size_t height_pixels;
        size_t columns;
        size_t rows;
        Av2hStatus status;
        if (count == 0) return AV2H_INVALID_ARGUMENT;
        if (chroma_format_idc != 0U && chroma_format_idc != 2U) {
                return AV2H_UNSUPPORTED;
        }
        status = av2h_block_dimensions_mi(block_size, &width_mi, &height_mi);
        if (status != AV2H_OK) return status;
        width_pixels = width_mi * (chroma_format_idc == 2U ? 4U : 2U);
        height_pixels = height_mi * (chroma_format_idc == 2U ? 4U : 2U);
        if (width_pixels < 4U) width_pixels = 4U;
        if (height_pixels < 4U) height_pixels = 4U;
        columns = (width_pixels + 63U) / 64U;
        rows = (height_pixels + 63U) / 64U;
        if (columns == 0U || rows == 0U || columns * rows > 4U) {
                return AV2H_UNSUPPORTED;
        }
        *count = (uint8_t)(columns * rows);
        return AV2H_OK;
}

Av2hStatus av2h_chroma_tx_node_get(
        const Av2hPartitionNode *coding_node,
        uint8_t chroma_format_idc,
        uint8_t index,
        Av2hPartitionNode *tx_node) {
        size_t width_mi;
        size_t height_mi;
        size_t width_pixels;
        size_t columns;
        uint8_t count;
        Av2hStatus status;
        if (coding_node == 0 || tx_node == 0) return AV2H_INVALID_ARGUMENT;
        status = av2h_chroma_tx_node_count(
                coding_node->block_size, chroma_format_idc, &count);
        if (status != AV2H_OK) return status;
        if (index >= count) return AV2H_INVALID_ARGUMENT;
        status = av2h_block_dimensions_mi(
                coding_node->block_size, &width_mi, &height_mi);
        if (status != AV2H_OK) return status;
        width_pixels = width_mi * (chroma_format_idc == 2U ? 4U : 2U);
        columns = (width_pixels + 63U) / 64U;
        *tx_node = *coding_node;
        if (count == 1U) return AV2H_OK;
        tx_node->block_size = AV2H_BLOCK_64X64;
        tx_node->row = (uint16_t)(
                tx_node->row + (index / columns) * 16U);
        tx_node->column = (uint16_t)(
                tx_node->column + (index % columns) * 16U);
        return AV2H_OK;
}
