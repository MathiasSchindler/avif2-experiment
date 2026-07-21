#include "tile.h"

Av2hStatus av2h_tile_group_single_parse(Av2hSpan payload,
                                         size_t uncompressed_header_bits,
                                         Av2hTileGroup *group) {
    size_t header_bits;
    size_t rounded_bits;
    size_t header_size;
    if (group == 0 || (payload.data == 0 && payload.size != 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    group->header_bits = 0U;
    group->header_size = 0U;
    group->entropy.data = 0;
    group->entropy.size = 0U;
    if (payload.size == 0U) return AV2H_TRUNCATED;
    if ((payload.data[0] & 0x80U) == 0U) return AV2H_UNSUPPORTED;
    if (!av2h_size_add(uncompressed_header_bits, 1U, &header_bits) ||
        !av2h_size_add(header_bits, 7U, &rounded_bits)) {
        return AV2H_OVERFLOW;
    }
    header_size = rounded_bits >> 3U;
    if (header_size >= payload.size) return AV2H_TRUNCATED;
    group->header_bits = header_bits;
    group->header_size = header_size;
    group->entropy.data = payload.data + header_size;
    group->entropy.size = payload.size - header_size;
    return AV2H_OK;
}