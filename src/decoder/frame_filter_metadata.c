#include "decoder/internal/frame_filter_metadata.h"
#include "decoder/tx_geometry.h"

Av2hStatus av2h_deblock_map_commit_transform(
    Av2hDeblockMap *map,
    uint8_t chroma_format_idc,
    unsigned int plane,
    const Av2hPartitionNode *leaf,
    const Av2hFirstCoeffPrefix *prefix,
    uint8_t skip,
    uint8_t all_zero,
    uint8_t lossless) {
    const Av2hTxDescriptor *descriptor;
    size_t grid_size;
    size_t block_column;
    size_t block_row;
    size_t block_columns;
    size_t block_rows;
    size_t transform_columns;
    size_t transform_rows;
    size_t row;
    Av2hStatus status;
    if (map == 0 || leaf == 0 || prefix == 0 ||
        plane >= AV2H_FRAME_PLANE_COUNT || skip > 1U ||
        all_zero > 1U || lossless > 1U || map->plane[plane] == 0 ||
        map->columns[plane] == 0U || map->rows[plane] == 0U ||
        map->stride_cells[plane] < map->columns[plane]) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    status = av2h_block_dimensions_mi(
        leaf->block_size, &block_columns, &block_rows);
    if (status != AV2H_OK) return status;
    if ((descriptor->output_width & 3U) != 0U ||
        (descriptor->output_height & 3U) != 0U) {
        return AV2H_UNSUPPORTED;
    }
    grid_size = 4U >> av2h_chroma_shift(chroma_format_idc, plane);
    block_column = leaf->column;
    block_row = leaf->row;
    transform_columns = descriptor->output_width / grid_size;
    transform_rows = descriptor->output_height / grid_size;
    if (transform_columns > block_columns) transform_columns = block_columns;
    if (transform_rows > block_rows) transform_rows = block_rows;
    if (block_column >= map->columns[plane] ||
        block_row >= map->rows[plane]) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (transform_columns > map->columns[plane] - block_column) {
        transform_columns = map->columns[plane] - block_column;
    }
    if (transform_rows > map->rows[plane] - block_row) {
        transform_rows = map->rows[plane] - block_row;
    }
    for (row = 0U; row < transform_rows; ++row) {
        size_t column;
        for (column = 0U; column < transform_columns; ++column) {
            Av2hDeblockCell *cell = (Av2hDeblockCell *)&map->plane[plane][
                (block_row + row) * map->stride_cells[plane] +
                block_column + column];
            if (cell->transform_width != 0U ||
                cell->transform_height != 0U) {
                return AV2H_INVALID_DATA;
            }
            *cell = (Av2hDeblockCell){
                (uint16_t)block_column,
                (uint16_t)block_row,
                (uint16_t)block_column,
                (uint16_t)block_row,
                descriptor->output_width,
                descriptor->output_height,
                prefix->q_index,
                (uint8_t)(skip != 0U || all_zero != 0U),
                lossless
            };
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_restoration_tx_skip_commit(
    Av2hFilterWorkspace *workspace,
    const Av2hPartitionNode *leaf,
    const Av2hFirstCoeffPrefix *prefix) {
    const Av2hTxDescriptor *descriptor;
    size_t columns;
    size_t rows;
    size_t row;
    Av2hStatus status;
    if (workspace == 0 || leaf == 0 || prefix == 0 ||
        workspace->restoration_tx_skip == 0 ||
        workspace->restoration_controls.tx_skip_stride == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    columns = descriptor->output_width / 4U;
    rows = descriptor->output_height / 4U;
    if (leaf->column >= workspace->deblock_map.columns[0] ||
        leaf->row >= workspace->deblock_map.rows[0]) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (columns > workspace->deblock_map.columns[0] - leaf->column) {
        columns = workspace->deblock_map.columns[0] - leaf->column;
    }
    if (rows > workspace->deblock_map.rows[0] - leaf->row) {
        rows = workspace->deblock_map.rows[0] - leaf->row;
    }
    for (row = 0U; row < rows; ++row) {
        size_t column;
        uint8_t *destination = workspace->restoration_tx_skip +
            (leaf->row + row) *
                workspace->restoration_controls.tx_skip_stride +
            leaf->column;
        for (column = 0U; column < columns; ++column) {
            destination[column] = prefix->all_zero;
        }
    }
    return AV2H_OK;
}
