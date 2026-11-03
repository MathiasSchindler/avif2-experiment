#include "internal/leaf_modules.h"

#include "internal/sample_math.h"
#include "shared/bit_math.h"

#include <stddef.h>

int av2h_leaf_block_allows_palette(Av2hBlockSize block_size) {
    size_t width;
    size_t height;
    return av2h_block_dimensions_mi(block_size, &width, &height) == AV2H_OK &&
        block_size >= AV2H_BLOCK_8X8 && width <= 16U && height <= 16U;
}

static uint32_t palette_read_uniform(
    Av2hRangeDecoder *decoder,
    uint32_t symbol_count) {
    unsigned int bits = av2h_ceil_log2_u32(symbol_count);
    uint32_t cutoff;
    uint32_t value;
    if (bits == 0U) return 0U;
    cutoff = (1U << bits) - symbol_count;
    value = av2h_range_read_bypass(decoder, bits - 1U);
    if (value < cutoff) return value;
    return (value << 1U) - cutoff + av2h_range_read_bypass(decoder, 1U);
}

Av2hStatus av2h_leaf_palette_read_colors(
    Av2hRangeDecoder *decoder,
    const Av2hSequenceState *sequence,
    const Av2hLumaModeNeighbors *neighbors,
    Av2hFirstLeafMode *mode) {
    uint16_t cache[2U * AV2H_PALETTE_MAX_SIZE];
    size_t above_index = 0U;
    size_t left_index = 0U;
    size_t cache_count = 0U;
    uint32_t range;
    uint32_t maximum;
    unsigned int bits;
    unsigned int color_bits;
    size_t index = 0U;
    if (decoder == NULL || sequence == NULL || mode == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->info.bit_depth_idc != 0U &&
        sequence->info.bit_depth_idc != 1U) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->palette_size < 2U ||
        mode->palette_size > AV2H_PALETTE_MAX_SIZE) {
        return AV2H_INVALID_DATA;
    }
    color_bits = sequence->info.bit_depth_idc == 0U ? 10U : 8U;
    maximum = (1U << color_bits) - 1U;
    if (neighbors != NULL) {
        while (above_index < neighbors->palette_size[1] &&
               left_index < neighbors->palette_size[0]) {
            cache[cache_count++] =
                neighbors->palette_colors[1][above_index++];
            cache[cache_count++] =
                neighbors->palette_colors[0][left_index++];
        }
        while (above_index < neighbors->palette_size[1]) {
            cache[cache_count++] =
                neighbors->palette_colors[1][above_index++];
        }
        while (left_index < neighbors->palette_size[0]) {
            cache[cache_count++] =
                neighbors->palette_colors[0][left_index++];
        }
    }
    for (above_index = 0U;
         above_index < cache_count && index < mode->palette_size;
         ++above_index) {
        if (av2h_range_read_bypass(decoder, 1U) != 0U) {
            mode->palette_colors[index++] = cache[above_index];
        }
    }
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (index == mode->palette_size) goto sort_colors;
    mode->palette_colors[index++] = (uint16_t)av2h_range_read_bypass(
        decoder, color_bits);
    if (av2h_range_status(decoder) != AV2H_OK) {
        return av2h_range_status(decoder);
    }
    if (index == mode->palette_size) goto sort_colors;
    bits = color_bits - 3U + av2h_range_read_bypass(decoder, 2U);
    range = maximum - mode->palette_colors[index - 1U];
    for (; index < mode->palette_size; ++index) {
        uint32_t previous = mode->palette_colors[index - 1U];
        uint32_t delta = av2h_range_read_bypass(decoder, bits) + 1U;
        uint32_t color = previous + delta;
        unsigned int next_bits;
        if (av2h_range_status(decoder) != AV2H_OK) {
            return av2h_range_status(decoder);
        }
        if (color > maximum) {
            color = maximum;
        }
        mode->palette_colors[index] = (uint16_t)color;
        range -= color - previous;
        next_bits = av2h_ceil_log2_u32(range);
        if (next_bits < bits) bits = next_bits;
    }
sort_colors:
    for (index = 0U; index < mode->palette_size; ++index) {
        size_t next;
        for (next = 1U; next < mode->palette_size - index; ++next) {
            if (mode->palette_colors[next - 1U] >
                mode->palette_colors[next]) {
                uint16_t temporary = mode->palette_colors[next - 1U];
                mode->palette_colors[next - 1U] =
                    mode->palette_colors[next];
                mode->palette_colors[next] = temporary;
            }
        }
    }
    return AV2H_OK;
}

static void palette_promote_color(
    uint8_t color_order[AV2H_PALETTE_MAX_SIZE],
    uint8_t color_seen[AV2H_PALETTE_MAX_SIZE],
    size_t destination,
    uint8_t color,
    size_t *seen_count) {
    if (!color_seen[color]) {
        color_order[destination] = color;
        color_seen[color] = 1U;
        ++*seen_count;
    }
}

static unsigned int palette_map_bit_width(uint8_t palette_size) {
    return palette_size <= 2U ? 1U : palette_size <= 4U ? 2U : 3U;
}

uint8_t av2h_palette_map_get(
    const Av2hFirstLeafMode *mode, size_t index) {
    unsigned int bits;
    size_t bit_offset;
    size_t byte_offset;
    unsigned int shift;
    uint16_t packed;
    if (mode == NULL || mode->palette_size < 2U ||
        mode->palette_size > AV2H_PALETTE_MAX_SIZE) {
        return AV2H_PALETTE_MAX_SIZE;
    }
    bits = palette_map_bit_width(mode->palette_size);
    bit_offset = index * bits;
    if (bit_offset + bits > AV2H_PALETTE_MAP_CAPACITY * 8U) {
        return AV2H_PALETTE_MAX_SIZE;
    }
    byte_offset = bit_offset >> 3U;
    shift = (unsigned int)(bit_offset & 7U);
    packed = mode->palette_map[byte_offset];
    if (shift + bits > 8U) {
        packed = (uint16_t)(packed |
            ((uint16_t)mode->palette_map[byte_offset + 1U] << 8U));
    }
    return (uint8_t)((packed >> shift) & ((1U << bits) - 1U));
}

Av2hStatus av2h_palette_map_set(
    Av2hFirstLeafMode *mode, size_t index, uint8_t color) {
    unsigned int bits;
    size_t bit_offset;
    size_t byte_offset;
    unsigned int shift;
    uint16_t mask;
    uint16_t packed;
    if (mode == NULL || mode->palette_size < 2U ||
        mode->palette_size > AV2H_PALETTE_MAX_SIZE ||
        color >= mode->palette_size) {
        return AV2H_INVALID_ARGUMENT;
    }
    bits = palette_map_bit_width(mode->palette_size);
    bit_offset = index * bits;
    if (bit_offset + bits > AV2H_PALETTE_MAP_CAPACITY * 8U) {
        return AV2H_OUT_OF_MEMORY;
    }
    byte_offset = bit_offset >> 3U;
    shift = (unsigned int)(bit_offset & 7U);
    mask = (uint16_t)(((1U << bits) - 1U) << shift);
    packed = mode->palette_map[byte_offset];
    if (shift + bits > 8U) {
        packed = (uint16_t)(packed |
            ((uint16_t)mode->palette_map[byte_offset + 1U] << 8U));
    }
    packed = (uint16_t)((packed & ~mask) | ((uint16_t)color << shift));
    mode->palette_map[byte_offset] = (uint8_t)packed;
    if (shift + bits > 8U) {
        mode->palette_map[byte_offset + 1U] = (uint8_t)(packed >> 8U);
    }
    return AV2H_OK;
}

static size_t palette_color_context(
    const Av2hFirstLeafMode *mode,
    size_t width,
    size_t row,
    size_t column,
    uint8_t color_order[AV2H_PALETTE_MAX_SIZE]) {
    uint8_t seen[AV2H_PALETTE_MAX_SIZE] = { 0U };
    size_t seen_count = 0U;
    size_t context;
    size_t color;
    for (color = 0U; color < AV2H_PALETTE_MAX_SIZE; ++color) {
        color_order[color] = (uint8_t)color;
    }
    if (row != 0U && column != 0U) {
        uint8_t left = av2h_palette_map_get(
            mode, row * width + column - 1U);
        uint8_t above_left = av2h_palette_map_get(
            mode, (row - 1U) * width + column - 1U);
        uint8_t above = av2h_palette_map_get(
            mode, (row - 1U) * width + column);
        if (left == above_left && left == above) {
            palette_promote_color(color_order, seen, 0U, left, &seen_count);
            context = 4U;
        } else if (left == above) {
            palette_promote_color(color_order, seen, 0U, left, &seen_count);
            palette_promote_color(
                color_order, seen, 1U, above_left, &seen_count);
            context = 3U;
        } else if (left == above_left) {
            palette_promote_color(color_order, seen, 0U, left, &seen_count);
            palette_promote_color(color_order, seen, 1U, above, &seen_count);
            context = 2U;
        } else if (above_left == above) {
            palette_promote_color(color_order, seen, 0U, above, &seen_count);
            palette_promote_color(color_order, seen, 1U, left, &seen_count);
            context = 2U;
        } else {
            palette_promote_color(color_order, seen, 0U, left, &seen_count);
            palette_promote_color(color_order, seen, 1U, above, &seen_count);
            palette_promote_color(
                color_order, seen, 2U, above_left, &seen_count);
            context = 1U;
        }
    } else if (row != 0U) {
        palette_promote_color(
            color_order, seen, 0U,
            av2h_palette_map_get(mode, (row - 1U) * width), &seen_count);
        context = 0U;
    } else {
        palette_promote_color(
            color_order, seen, 0U,
            av2h_palette_map_get(mode, column - 1U), &seen_count);
        context = 0U;
    }
    for (color = 0U; color < AV2H_PALETTE_MAX_SIZE; ++color) {
        if (!seen[color]) color_order[seen_count++] = (uint8_t)color;
    }
    return context;
}

Av2hStatus av2h_luma_palette_map_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hPartitionNode *leaf,
    Av2hFirstLeafMode *mode) {
    uint32_t direction;
    uint32_t previous_identity = 0U;
    size_t width_mi;
    size_t height_mi;
    size_t width;
    size_t height;
    size_t axis2;
    if (decoder == NULL || cdfs == NULL || leaf == NULL || mode == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (mode->palette_size == 0U) return AV2H_OK;
    if (mode->palette_size < 2U ||
        mode->palette_size > AV2H_PALETTE_MAX_SIZE ||
        !av2h_leaf_block_allows_palette(leaf->block_size) ||
        av2h_block_dimensions_mi(
            leaf->block_size, &width_mi, &height_mi) != AV2H_OK) {
        return AV2H_INVALID_DATA;
    }
    width = width_mi * 4U;
    height = height_mi * 4U;
    if (width * height * palette_map_bit_width(mode->palette_size) >
        AV2H_PALETTE_MAP_CAPACITY * 8U) {
        return AV2H_UNSUPPORTED;
    }
    mode->palette_width = (uint8_t)width;
    mode->palette_height = (uint8_t)height;
    mode->palette_origin_row = leaf->row;
    mode->palette_origin_column = leaf->column;
    direction = width < 64U && height < 64U ?
        av2h_range_read_bypass(decoder, 1U) : 0U;
    for (axis2 = 0U; axis2 < (direction ? width : height); ++axis2) {
        size_t identity_context = axis2 == 0U ? 3U : previous_identity;
        uint32_t identity = av2h_range_read_adaptive(
            decoder, cdfs->palette_y_identity[identity_context], 3U);
        size_t axis1;
        if (identity == 2U && axis2 == 0U) return AV2H_INVALID_DATA;
        for (axis1 = 0U; axis1 < (direction ? height : width); ++axis1) {
            size_t row = direction ? axis1 : axis2;
            size_t column = direction ? axis2 : axis1;
            size_t map_index = row * width + column;
            Av2hStatus status;
            if (identity == 2U) {
                status = av2h_palette_map_set(mode, map_index,
                    av2h_palette_map_get(mode, direction ?
                        map_index - 1U : map_index - width));
            } else if (identity == 1U && axis1 != 0U) {
                status = av2h_palette_map_set(mode, map_index,
                    av2h_palette_map_get(mode, direction ?
                        map_index - width : map_index - 1U));
            } else if (axis2 == 0U && axis1 == 0U) {
                status = av2h_palette_map_set(mode, 0U,
                    (uint8_t)palette_read_uniform(
                        decoder, mode->palette_size));
            } else {
                uint8_t color_order[AV2H_PALETTE_MAX_SIZE];
                size_t context = palette_color_context(
                    mode, width, row, column, color_order);
                uint16_t *cdf = cdfs->palette_y_color_index
                    [mode->palette_size - 2U][context];
                uint32_t color_index = av2h_range_read_adaptive(
                    decoder, cdf, mode->palette_size);
                if (color_index >= mode->palette_size) {
                    return AV2H_INVALID_DATA;
                }
                status = av2h_palette_map_set(
                    mode, map_index, color_order[color_index]);
            }
            if (status != AV2H_OK) return status;
            if (av2h_range_status(decoder) != AV2H_OK) {
                return av2h_range_status(decoder);
            }
        }
        previous_identity = identity;
    }
    mode->after_palette_map_bits = av2h_range_tell(decoder);
    return AV2H_OK;
}