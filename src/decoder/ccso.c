#include "decoder/filter.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/sample_math.h"

static const uint16_t ccso_quant_size[4][4] = {
    { 16U, 8U, 32U, 0U },
    { 56U, 40U, 64U, 128U },
    { 48U, 24U, 96U, 192U },
    { 80U, 112U, 160U, 256U }
};

int av2h_ccso_controls_valid(const Av2hFrameState *frame,
                             const Av2hCcsoControlMap *controls) {
    uint16_t unit_size = av2h_filter_ccso_unit_size(frame);
    size_t columns = (av2h_filter_coded_dimension(frame->width) +
                      unit_size - 1U) / unit_size;
    size_t rows = (av2h_filter_coded_dimension(frame->height) +
                   unit_size - 1U) / unit_size;
    unsigned int plane;
    if (controls == 0 || controls->columns != columns ||
        controls->rows != rows ||
        (unit_size != 64U && unit_size != 128U && unit_size != 256U)) {
        return 0;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (frame->ccso[plane].enabled != 0U &&
            (controls->plane[plane] == 0 ||
             controls->stride[plane] < columns)) {
            return 0;
        }
        if (frame->ccso[plane].enabled != 0U) {
            size_t row;
            for (row = 0U; row < rows; ++row) {
                size_t column;
                for (column = 0U; column < columns; ++column) {
                    if (controls->plane[plane][
                            row * controls->stride[plane] + column] > 1U) {
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
        if (parameters->enabled == 0U) continue;
        if (parameters->scale_index >= 4U) return 0;
        if (parameters->band_only != 0U) {
            if (parameters->quant_index != 0U ||
                parameters->extended_filter_support != 0U ||
                parameters->edge_classifier != 0U ||
                parameters->max_band_log2 > 6U) {
                return 0;
            }
            edge_intervals = 1U;
        } else {
            if (parameters->quant_index >= 4U ||
                parameters->extended_filter_support >= 7U ||
                parameters->edge_classifier >= 2U ||
                parameters->max_band_log2 > 3U ||
                (parameters->scale_index == 0U &&
                 parameters->quant_index == 3U &&
                 parameters->edge_classifier != 0U)) {
                return 0;
            }
            edge_intervals =
                parameters->edge_classifier != 0U ? 2U : 3U;
        }
        expected_offsets = (uint16_t)(
            ((uint16_t)1U << parameters->max_band_log2) *
            edge_intervals * edge_intervals);
        if (parameters->offset_count != expected_offsets) return 0;
    }
    return 1;
}

static void ccso_sample_offsets(uint8_t support,
                                int *first_x,
                                int *first_y,
                                int *second_x,
                                int *second_y) {
    static const int8_t offsets[7][4] = {
        { 0, -1, 0, 1 },
        { -1, 0, 1, 0 },
        { -1, -1, 1, 1 },
        { 1, -1, -1, 1 },
        { -2, -1, 2, 1 },
        { -2, 1, 2, -1 },
        { 2, 0, -2, 0 }
    };
    *first_x = offsets[support][0];
    *first_y = offsets[support][1];
    *second_x = offsets[support][2];
    *second_y = offsets[support][3];
}

static uint8_t luma_sample(const uint8_t *luma,
                           size_t stride,
                           uint16_t width,
                           uint16_t height,
                           int x,
                           int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= (int)width) x = (int)width - 1;
    if (y >= (int)height) y = (int)height - 1;
    return luma[(size_t)y * stride + (size_t)x];
}

static unsigned int ccso_class(uint8_t neighbor,
                               uint8_t center,
                               uint16_t threshold,
                               uint8_t edge_classifier) {
    int difference = (int)neighbor - (int)center;
    if (difference < -(int)threshold) return 0U;
    if (edge_classifier != 0U) return 1U;
    return difference > (int)threshold ? 2U : 1U;
}

static void ccso_filter_plane(Av2hFilterPlane *destination,
                              const uint8_t *luma,
                              size_t luma_stride,
                              uint16_t luma_width,
                              uint16_t luma_height,
                              const Av2hCcsoPlane *parameters,
                              const uint8_t *controls,
                              size_t control_stride,
                              uint16_t luma_unit_size,
                              unsigned int plane_index) {
    unsigned int subsampling = plane_index == 0U ? 0U : 1U;
    unsigned int unit_size = luma_unit_size >> subsampling;
    unsigned int shift = 8U - parameters->max_band_log2;
    uint16_t threshold = ccso_quant_size[parameters->scale_index]
                                        [parameters->quant_index];
    int first_x = 0;
    int first_y = 0;
    int second_x = 0;
    int second_y = 0;
    uint16_t y;
    if (parameters->band_only == 0U) {
        ccso_sample_offsets(parameters->extended_filter_support,
                            &first_x, &first_y, &second_x, &second_y);
    }
    for (y = 0U; y < destination->height; ++y) {
        uint16_t x;
        for (x = 0U; x < destination->width; ++x) {
            size_t control_row = y / unit_size;
            size_t control_column = x / unit_size;
            int luma_x;
            int luma_y;
            uint8_t center;
            unsigned int first_class = 0U;
            unsigned int second_class = 0U;
            unsigned int band;
            size_t lut_index;
            size_t destination_index;
            if (controls[control_row * control_stride + control_column] == 0U) {
                continue;
            }
            luma_x = (int)((uint32_t)x << subsampling);
            luma_y = (int)((uint32_t)y << subsampling);
            center = luma_sample(luma, luma_stride, luma_width,
                                 luma_height, luma_x, luma_y);
            if (parameters->band_only == 0U) {
                first_class = ccso_class(
                    luma_sample(luma, luma_stride, luma_width, luma_height,
                                luma_x + first_x, luma_y + first_y),
                    center, threshold, parameters->edge_classifier);
                second_class = ccso_class(
                    luma_sample(luma, luma_stride, luma_width, luma_height,
                                luma_x + second_x, luma_y + second_y),
                    center, threshold, parameters->edge_classifier);
            }
            band = parameters->max_band_log2 == 0U ? 0U : center >> shift;
            lut_index = ((size_t)band << 4U) +
                        ((size_t)first_class << 2U) + second_class;
            destination_index = (size_t)y * destination->stride + x;
            destination->data[destination_index] = av2h_sample_clip(
                (int)destination->data[destination_index] +
                parameters->filter_offset[lut_index]);
        }
    }
}

Av2hStatus av2h_ccso_filter_frame(
    Av2hFilterPlanes *planes,
    const uint8_t *pre_cdef_luma,
    size_t pre_cdef_luma_stride,
    const Av2hFrameState *frame,
    const Av2hCcsoControlMap *controls) {
    unsigned int plane;
    if (planes == 0 || pre_cdef_luma == 0 || frame == 0 ||
        frame->width == 0U || frame->height == 0U ||
        pre_cdef_luma_stride < av2h_filter_coded_dimension(frame->width) ||
        !filter_dimensions_valid(planes, frame) ||
        !av2h_ccso_parameters_valid(frame) ||
        !av2h_ccso_controls_valid(frame, controls)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (frame->ccso[plane].enabled == 0U) continue;
        ccso_filter_plane(&planes->plane[plane], pre_cdef_luma,
                          pre_cdef_luma_stride, planes->plane[0].width,
                          planes->plane[0].height, &frame->ccso[plane],
                          controls->plane[plane], controls->stride[plane],
                          av2h_filter_ccso_unit_size(frame), plane);
    }
    return AV2H_OK;
}

