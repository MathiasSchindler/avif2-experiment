#include "internal/coeff_modules.h"

#include "internal/coeff_common.h"

static unsigned int level_at(const Av2hCoeffBlock *block,
                             int row, int column) {
    if (row < 0 || column < 0 || row >= (int)block->height ||
        column >= (int)block->width) {
        return 0U;
    }
    return block->base_levels[(size_t)row * block->width + (size_t)column];
}

uint16_t av2h_coeff_scan_position(
    uint16_t scan_index, uint16_t width, uint16_t height,
    uint8_t vertical, uint8_t column_scan) {
    unsigned int diagonal;
    unsigned int remaining = scan_index;
    if (column_scan) {
        return (uint16_t)((scan_index % height) * width + scan_index / height);
    }
    if (vertical) return scan_index;
    for (diagonal = 0U; diagonal < (unsigned int)width + height - 1U;
         ++diagonal) {
        int row = (int)av2h_coeff_minimum(diagonal, height - 1U);
        int last_row = diagonal >= width ? (int)(diagonal - width + 1U) : 0;
        unsigned int diagonal_count = (unsigned int)(row - last_row + 1);
        if (remaining < diagonal_count) {
            row -= (int)remaining;
            return (uint16_t)((unsigned int)row * width + diagonal -
                              (unsigned int)row);
        }
        remaining -= diagonal_count;
    }
    return AV2H_COEFF_CAPACITY;
}

uint8_t av2h_coeff_lf_context(
    const Av2hCoeffBlock *block, uint16_t position,
    uint8_t vertical, uint8_t horizontal) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude =
        av2h_coeff_minimum(level_at(block, row, column + 1), 5U) +
        av2h_coeff_minimum(level_at(block, row + 1, column), 5U);
    unsigned int context;
    if (vertical) {
        magnitude += av2h_coeff_minimum(level_at(block, row + 2, column), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row + 3, column), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row + 4, column), 3U);
        context = (magnitude + 1U) >> 1U;
        if (row == 0) {
            return (uint8_t)(21U + av2h_coeff_minimum(context, 6U));
        }
        return (uint8_t)(28U + av2h_coeff_minimum(context, 4U));
    }
    if (horizontal) {
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 2), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 3), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 4), 3U);
        context = (magnitude + 1U) >> 1U;
        if (column == 0) {
            return (uint8_t)(21U + av2h_coeff_minimum(context, 6U));
        }
        return (uint8_t)(28U + av2h_coeff_minimum(context, 4U));
    }
    magnitude += av2h_coeff_minimum(
        level_at(block, row + 1, column + 1), 5U);
    magnitude += av2h_coeff_minimum(level_at(block, row, column + 2), 5U);
    magnitude += av2h_coeff_minimum(level_at(block, row + 2, column), 5U);
    context = (magnitude + 1U) >> 1U;
    if (position == 0U) return (uint8_t)av2h_coeff_minimum(context, 8U);
    if (row + column < 2) {
        return (uint8_t)(9U + av2h_coeff_minimum(context, 6U));
    }
    return (uint8_t)(16U + av2h_coeff_minimum(context, 4U));
}

uint8_t av2h_coeff_base_context(
    const Av2hCoeffBlock *block, uint16_t position,
    uint8_t vertical, uint8_t horizontal) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude =
        av2h_coeff_minimum(level_at(block, row, column + 1), 3U) +
        av2h_coeff_minimum(level_at(block, row + 1, column), 3U);
    unsigned int context;
    if (vertical) {
        magnitude += av2h_coeff_minimum(level_at(block, row + 2, column), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row + 3, column), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row + 4, column), 3U);
        return (uint8_t)(15U +
            av2h_coeff_minimum((magnitude + 1U) >> 1U, 4U));
    }
    if (horizontal) {
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 2), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 3), 3U);
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 4), 3U);
        return (uint8_t)(15U +
            av2h_coeff_minimum((magnitude + 1U) >> 1U, 4U));
    }
    magnitude += av2h_coeff_minimum(
        level_at(block, row + 1, column + 1), 3U);
    magnitude += av2h_coeff_minimum(level_at(block, row, column + 2), 3U);
    magnitude += av2h_coeff_minimum(level_at(block, row + 2, column), 3U);
    context = av2h_coeff_minimum((magnitude + 1U) >> 1U, 4U);
    if (row + column < 6) return (uint8_t)context;
    if (row + column < 8) return (uint8_t)(5U + context);
    return (uint8_t)(10U + context);
}

uint8_t av2h_coeff_br_context(
    const Av2hCoeffBlock *block, uint16_t position,
    uint8_t vertical, uint8_t low, uint8_t horizontal) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude =
        av2h_coeff_minimum(level_at(block, row, column + 1), 5U) +
        av2h_coeff_minimum(level_at(block, row + 1, column), 5U);
    if (vertical) {
        magnitude += av2h_coeff_minimum(level_at(block, row + 2, column), 5U);
    } else if (horizontal) {
        magnitude += av2h_coeff_minimum(level_at(block, row, column + 2), 5U);
    } else {
        magnitude += av2h_coeff_minimum(
            level_at(block, row + 1, column + 1), 5U);
    }
    magnitude = av2h_coeff_minimum((magnitude + 1U) >> 1U, 6U);
    if (!low) return (uint8_t)magnitude;
    if (!vertical && !horizontal && position == 0U) return (uint8_t)magnitude;
    return (uint8_t)(magnitude + 7U);
}

int av2h_coeff_is_low_frequency(
    uint16_t position, uint16_t width, uint8_t vertical,
    uint8_t horizontal) {
    unsigned int row = position / width;
    unsigned int column = position % width;
    return (vertical ? row < 2U :
                     horizontal ? column < 2U : row + column < 4U);
}

uint8_t av2h_coeff_eob_context(uint16_t scan_index, uint16_t capacity) {
    if (scan_index == 0U) return 0U;
    if (scan_index <= capacity / 8U) return 1U;
    if (scan_index <= capacity / 4U) return 2U;
    return 3U;
}

uint8_t av2h_coeff_uv_level_context(
    const Av2hCoeffBlock *block, uint16_t position,
    uint8_t plane, uint8_t low) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int limit = low ? 5U : 3U;
    unsigned int magnitude = av2h_coeff_minimum(
        level_at(block, row, column + 1), limit) + av2h_coeff_minimum(
        level_at(block, row + 1, column), limit) + av2h_coeff_minimum(
        level_at(block, row + 1, column + 1), limit);
    uint8_t context = (uint8_t)av2h_coeff_minimum(
        (magnitude + 1U) >> 1U, 3U);
    return plane == 2U ? (uint8_t)(context + 4U) : context;
}

uint8_t av2h_coeff_fsc_base_context(
    const Av2hCoeffBlock *block, uint16_t position) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude =
        av2h_coeff_minimum(level_at(block, row, column - 1), 3U) +
        av2h_coeff_minimum(level_at(block, row - 1, column), 3U);
    return (uint8_t)av2h_coeff_minimum(magnitude, 6U);
}

uint8_t av2h_coeff_fsc_br_context(
    const Av2hCoeffBlock *block, uint16_t position) {
    int row = position / block->width;
    int column = position % block->width;
    unsigned int magnitude =
        av2h_coeff_minimum(level_at(block, row, column - 1), 5U) +
        av2h_coeff_minimum(level_at(block, row - 1, column), 5U);
    return (uint8_t)av2h_coeff_minimum(magnitude, 6U);
}

static int fsc_sign_at(
    const Av2hCoeffBlock *block, int row, int column) {
    int32_t value;
    if (row < 0 || column < 0 || row >= (int)block->height ||
        column >= (int)block->width) {
        return 0;
    }
    value = block->quantized[(size_t)row * block->width + (size_t)column];
    return value < 0 ? -1 : value > 0 ? 1 : 0;
}

uint8_t av2h_coeff_fsc_sign_context(
    const Av2hCoeffBlock *block, uint16_t position) {
    int row = position / block->width;
    int column = position % block->width;
    int sum = fsc_sign_at(block, row, column - 1) +
        fsc_sign_at(block, row - 1, column) +
        fsc_sign_at(block, row - 1, column - 1);
    uint8_t context = sum > 2 ? 5U : sum < -2 ? 6U :
        sum > 0 ? 1U : sum < 0 ? 2U : 0U;
    if (block->base_levels[position] > 3U && context != 0U) context += 2U;
    return context;
}