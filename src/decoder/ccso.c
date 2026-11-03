#include "decoder/filter.h"

#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

#include <stddef.h>

static const uint16_t ccso_quant_size[4][4] = {
    {16U, 8U, 32U, 0U}, {56U, 40U, 64U, 128U}, {48U, 24U, 96U, 192U}, {80U, 112U, 160U, 256U}};

int av2h_ccso_controls_valid(const Av2hFrameState *frame, const Av2hCcsoControlMap *controls) {
    uint16_t unit_size = av2h_filter_ccso_unit_size(frame);
    size_t columns = (av2h_filter_coded_dimension(frame->width) + unit_size - 1U) / unit_size;
    size_t rows = (av2h_filter_coded_dimension(frame->height) + unit_size - 1U) / unit_size;
    unsigned int plane;
    if (controls == NULL || controls->columns != columns || controls->rows != rows ||
        (unit_size != 64U && unit_size != 128U && unit_size != 256U)) {
        return 0;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (frame->ccso[plane].enabled != 0U &&
            (controls->plane[plane] == NULL || controls->stride_units[plane] < columns)) {
            return 0;
        }
        if (frame->ccso[plane].enabled != 0U) {
            size_t row;
            for (row = 0U; row < rows; ++row) {
                size_t column;
                for (column = 0U; column < columns; ++column) {
                    if (controls->plane[plane][row * controls->stride_units[plane] + column] > 1U) {
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

int av2h_ccso_parameters_valid(const Av2hFrameState *frame) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hCcsoPlane *parameters = &frame->ccso[plane];
        unsigned int edge_intervals;
        uint16_t expected_offsets;
        if (parameters->enabled > 1U || parameters->band_only > 1U) {
            return 0;
        }
        if (parameters->enabled == 0U)
            continue;
        if (parameters->scale_index >= 4U)
            return 0;
        if (parameters->band_only != 0U) {
            if (parameters->quant_index != 0U || parameters->extended_filter_support != 0U ||
                parameters->edge_classifier != 0U || parameters->max_band_log2 > 6U) {
                return 0;
            }
            edge_intervals = 1U;
        } else {
            if (parameters->quant_index >= 4U || parameters->extended_filter_support >= 7U ||
                parameters->edge_classifier >= 2U || parameters->max_band_log2 > 3U ||
                (parameters->scale_index == 0U && parameters->quant_index == 3U &&
                 parameters->edge_classifier != 0U)) {
                return 0;
            }
            edge_intervals = parameters->edge_classifier != 0U ? 2U : 3U;
        }
        expected_offsets = (uint16_t)(((uint16_t)1U << parameters->max_band_log2) * edge_intervals *
                                      edge_intervals);
        if (parameters->offset_count != expected_offsets)
            return 0;
    }
    return 1;
}

static void ccso_sample_offsets(uint8_t support, int *first_x, int *first_y, int *second_x,
                                int *second_y) {
    static const int8_t offsets[7][4] = {{0, -1, 0, 1},  {-1, 0, 1, 0},  {-1, -1, 1, 1},
                                         {1, -1, -1, 1}, {-2, -1, 2, 1}, {-2, 1, 2, -1},
                                         {2, 0, -2, 0}};
    *first_x = offsets[support][0];
    *first_y = offsets[support][1];
    *second_x = offsets[support][2];
    *second_y = offsets[support][3];
}

static void ccso_tile_bounds(const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                             int center_x, int center_y, int *left, int *top, int *right,
                             int *bottom) {
    unsigned int column;
    unsigned int row;
    *left = 0;
    *top = 0;
    *right = frame->width;
    *bottom = frame->height;
    if (sequence == NULL || sequence->disable_loopfilters_across_tiles == 0U) {
        return;
    }
    for (column = 0U; column < frame->tile_column_count; ++column) {
        int end = (int)frame->tile_column_start_sb[column + 1U] * sequence->superblock_size;
        if (center_x < end || column + 1U == frame->tile_column_count) {
            *left = (int)frame->tile_column_start_sb[column] * sequence->superblock_size;
            *right = end < frame->width ? end : frame->width;
            break;
        }
    }
    for (row = 0U; row < frame->tile_row_count; ++row) {
        int end = (int)frame->tile_row_start_sb[row + 1U] * sequence->superblock_size;
        if (center_y < end || row + 1U == frame->tile_row_count) {
            *top = (int)frame->tile_row_start_sb[row] * sequence->superblock_size;
            *bottom = end < frame->height ? end : frame->height;
            break;
        }
    }
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/ccso_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/ccso_kernel.inc"
#include "internal/sample_kernel_end.inc"
