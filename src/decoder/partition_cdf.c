#include "partition_cdf.h"

#include <stddef.h>

#include "partition_cdf_data.inc"

static Av2hStatus initialize_table(uint16_t (*destination)[AV2H_BINARY_CDF_SIZE],
                                   const Av2hBinaryCdfSpec *source, size_t row_count) {
    size_t index;
    for (index = 0U; index < row_count; ++index) {
        Av2hStatus status = av2h_binary_cdf_init(destination[index], source[index]);
        if (status != AV2H_OK)
            return status;
    }
    return AV2H_OK;
}

Av2hStatus av2h_partition_cdfs_init(Av2hPartitionCdfs *context) {
    Av2hStatus status;
    if (context == NULL)
        return AV2H_INVALID_ARGUMENT;
    status = initialize_table(&context->do_split[0][0], do_split_defaults,
                              sizeof(do_split_defaults) / sizeof(do_split_defaults[0]));
    if (status != AV2H_OK)
        return status;
    status =
        initialize_table(&context->do_square_split[0][0], do_square_split_defaults,
                         sizeof(do_square_split_defaults) / sizeof(do_square_split_defaults[0]));
    if (status != AV2H_OK)
        return status;
    status = initialize_table(&context->rect_type[0][0], rect_type_defaults,
                              sizeof(rect_type_defaults) / sizeof(rect_type_defaults[0]));
    if (status != AV2H_OK)
        return status;
    status =
        initialize_table(&context->do_ext_partition[0][0], do_ext_partition_defaults,
                         sizeof(do_ext_partition_defaults) / sizeof(do_ext_partition_defaults[0]));
    if (status != AV2H_OK)
        return status;
    return initialize_table(
        &context->do_uneven_4way_partition[0][0], do_uneven_4way_partition_defaults,
        sizeof(do_uneven_4way_partition_defaults) / sizeof(do_uneven_4way_partition_defaults[0]));
}