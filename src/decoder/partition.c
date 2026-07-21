#include "partition.h"

Av2hStatus av2h_partition_ordinary_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hPartitionCdfs *cdfs,
    size_t split_context,
    size_t rect_type_context,
    size_t extension_context,
    Av2hOrdinaryPartitionPrefix *prefix) {
    uint32_t value;
    if (decoder == 0 || cdfs == 0 || prefix == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (split_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        rect_type_context >= AV2H_PARTITION_CONTEXT_COUNT ||
        extension_context >= AV2H_PARTITION_CONTEXT_COUNT) {
        return AV2H_INVALID_ARGUMENT;
    }
    value = av2h_range_read_adaptive(
        decoder, cdfs->do_split[0][split_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->after_split_bits = av2h_range_tell(decoder);
    if (value == 0U) return AV2H_UNSUPPORTED;

    value = av2h_range_read_adaptive(
        decoder, cdfs->rect_type[0][rect_type_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->axis = value == 0U ? AV2H_PARTITION_HORIZONTAL
                               : AV2H_PARTITION_VERTICAL;
    prefix->after_axis_bits = av2h_range_tell(decoder);

    value = av2h_range_read_adaptive(
        decoder,
        cdfs->do_ext_partition[0][extension_context], 2U);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    prefix->after_extension_bits = av2h_range_tell(decoder);
    return value == 0U ? AV2H_OK : AV2H_UNSUPPORTED;
}