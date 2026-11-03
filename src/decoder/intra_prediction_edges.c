#include "internal/intra_prediction.h"

#include "internal/sample_math.h"

#include <stddef.h>

void av2h_directional_filter_edge(uint8_t *edge, size_t count, int strength);
void av2h_directional_filter_edge_u16(uint16_t *edge, size_t count, int strength);

int av2h_directional_filter_strength(int size0,
                                     int size1,
                                     int delta,
                                     uint8_t smooth) {
    int distance = delta < 0 ? -delta : delta;
    int sum = size0 + size1;
    if (smooth) {
        if (sum <= 8)
            return distance >= 64 ? 2 : distance >= 40 ? 1 : 0;
        if (sum <= 16)
            return distance >= 48 ? 2 : distance >= 20 ? 1 : 0;
        if (sum <= 24)
            return distance >= 4 ? 3 : 0;
        return distance >= 1 ? 3 : 0;
    }
    if (sum <= 8)
        return distance >= 56 ? 1 : 0;
    if (sum <= 12)
        return distance >= 40 ? 1 : 0;
    if (sum <= 16)
        return distance >= 40 ? 1 : 0;
    if (sum <= 24) {
        return distance >= 32 ? 3 : distance >= 16 ? 2 : distance >= 8 ? 1 : 0;
    }
    if (sum <= 32) {
        return distance >= 32 ? 3 : distance >= 4 ? 2 : distance >= 1 ? 1 : 0;
    }
    return distance >= 1 ? 3 : 0;
}

void av2h_prepare_directional_edges(const Av2hLeafMode *mode,
                                    int angle,
                                    uint16_t width,
                                    uint16_t height,
                                    const Av2hLumaReferences *references,
                                    uint8_t filter_edges,
                                    uint8_t *above,
                                    uint8_t *left,
                                    size_t edge_count,
                                    int missing_left_ibp,
                                    uint8_t edge_ibp) {
    size_t index;
    uint8_t no_references =
        (references->above == NULL || references->above_count == 0U) &&
        (references->left == NULL || references->left_count == 0U);
    uint8_t fallback = references->above != NULL && references->above_count != 0U
                           ? references->above[0]
                       : references->left != NULL && references->left_count != 0U
                           ? references->left[0]
                           : av2h_sample_midpoint_8bit();
    uint8_t above_fallback =
        no_references ? av2h_sample_below_midpoint_8bit() : fallback;
    uint8_t left_fallback =
        no_references ? av2h_sample_above_midpoint_8bit() : fallback;
    uint8_t corner =
        references->has_above_left ? references->above_left : fallback;
    above[-4] = above[-3] = above[-2] = above[-1] = corner;
    left[-4] = left[-3] = left[-2] = left[-1] = corner;
    if (mode->mrl_index != 0U) {
        size_t negative_count = (size_t)mode->mrl_index + 1U;
        for (index = 1U; index <= negative_count; ++index) {
            above[-(int)index] = index <= references->negative_count
                                     ? references->above_negative[index]
                                     : corner;
            left[-(int)index] = index <= references->negative_count
                                    ? references->left_negative[index]
                                    : corner;
        }
        above[-(int)negative_count - 1] = above[-(int)negative_count];
        left[-(int)negative_count - 1] = left[-(int)negative_count];
    }
    for (index = 0U; index < edge_count; ++index) {
        size_t directional_extent = (size_t)width + height;
        size_t above_available = references->above_count;
        size_t left_available = references->left_count;
        if ((angle < 90 || (angle > 180 && edge_ibp)) &&
            above_available > 2U * width) {
            above_available = 2U * width;
        }
        if ((angle > 180 || (angle < 90 && edge_ibp)) &&
            left_available > 2U * height) {
            left_available = 2U * height;
        }
        size_t above_source =
            angle < 90
                ? (index < above_available ? index : above_available - 1U)
            : angle > 180 && edge_ibp
                ? (index < above_available ? index : above_available - 1U)
            : index < width ? index
                            : width - 1U;
        size_t left_source =
            angle < 90 && edge_ibp
                ? (index < directional_extent ? index : directional_extent - 1U)
            : angle > 180 ? index
            : mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 1 &&
                    width == 16U && height == 16U
                ? index
            : index < height ? index
                             : height - 1U;
        above[index] =
            references->above != NULL && references->above_count != 0U
                ? references->above[above_source < references->above_count
                                        ? above_source
                                        : references->above_count - 1U]
                : above_fallback;
        left[index] =
            references->left != NULL && references->left_count != 0U
                ? references
                      ->left[left_source < left_available ? left_source
                                                          : left_available - 1U]
                : left_fallback;
    }
    if (filter_edges != 0U && mode->mrl_index == 0U && references->above != NULL &&
        references->above_count != 0U && references->left != NULL &&
        references->left_count != 0U && width + height >= 24U) {
        corner =
            (uint8_t)((left[0] * 5U + above[-1] * 6U + above[0] * 5U + 8U) >>
                      4U);
        above[-1] = left[-1] = corner;
    }
    if (filter_edges != 0U && mode->mrl_index == 0U && references->above != NULL &&
        references->above_count != 0U) {
        size_t above_filter_count =
            angle < 90                ? (size_t)width + height + 1U
            : angle > 180 && edge_ibp ? (size_t)width + height + 1U
            : missing_left_ibp        ? (size_t)width + height + 1U
                                      : (size_t)width + 1U;
        av2h_directional_filter_edge(
            above - 1, above_filter_count,
            av2h_directional_filter_strength(
                width, height,
                edge_ibp && angle > 180 ? angle - 270 : angle - 90,
                edge_ibp ? mode->above_smooth
                         : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    if (filter_edges != 0U && mode->mrl_index == 0U && references->left != NULL &&
        references->left_count != 0U) {
        size_t left_filter_count = (size_t)height + 1U;
        if (angle > 180 || (angle < 90 && edge_ibp)) {
            left_filter_count += width;
        } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
                   mode->angle_delta == 1 && width == 16U && height == 16U) {
            left_filter_count += width;
        }
        av2h_directional_filter_edge(
            left - 1, left_filter_count,
            av2h_directional_filter_strength(
                height, width, edge_ibp && angle < 90 ? angle : angle - 180,
                edge_ibp ? mode->left_smooth
                         : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    if (angle > 90 && angle < 180 && mode->mrl_index == 0U) {
        above[width] = above[width - 1U];
        left[height] = left[height - 1U];
    } else if (angle < 90) {
        size_t max_base = (size_t)width + height - 1U + 2U * mode->mrl_index;
        above[max_base + 1U] = above[max_base];
        above[max_base + 2U] = above[max_base];
        if (mode->mrl_index == 0U && mode->enable_ibp) {
            left[max_base + 1U] = left[max_base];
            left[max_base + 2U] = left[max_base];
        }
    } else if (angle > 180) {
        size_t max_base = (size_t)width + height - 1U + 2U * mode->mrl_index;
        left[max_base + 1U] = left[max_base];
        left[max_base + 2U] = left[max_base];
        if (mode->mrl_index == 0U && mode->enable_ibp) {
            above[max_base + 1U] = above[max_base];
            above[max_base + 2U] = above[max_base];
        }
    }
}

static void prepare_negative_u16(const Av2hLeafMode *mode,
                                 const Av2hLumaReferencesU16 *references,
                                 uint16_t *above_edge,
                                 uint16_t *left_edge,
                                 uint16_t corner) {
    size_t index;
    above_edge[-4] = above_edge[-3] = above_edge[-2] = above_edge[-1] = corner;
    left_edge[-4] = left_edge[-3] = left_edge[-2] = left_edge[-1] = corner;
    if (mode->mrl_index != 0U) {
        size_t negative_count = (size_t)mode->mrl_index + 1U;
        for (index = 1U; index <= negative_count; ++index) {
            above_edge[-(int)index] = index <= references->negative_count
                                          ? references->above_negative[index]
                                          : corner;
            left_edge[-(int)index] = index <= references->negative_count
                                         ? references->left_negative[index]
                                         : corner;
        }
        above_edge[-(int)negative_count - 1] = above_edge[-(int)negative_count];
        left_edge[-(int)negative_count - 1] = left_edge[-(int)negative_count];
    }
}

Av2hStatus av2h_prepare_directional_zone1_u16(const Av2hLeafMode *mode, uint16_t width,
                                              uint16_t height,
                                              const Av2hLumaReferencesU16 *references,
                                              uint8_t disable_edge_filter, int angle,
                                              uint16_t *above_edge, uint16_t *left_edge,
                                              int max_base, size_t edge_count,
                                              const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t above_left;
    uint8_t has_above_left;
    int have_above;
    int have_left;
    size_t index;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    above_left = references->above_left;
    has_above_left = references->has_above_left;
    have_above = above != NULL && above_count != 0U;
    have_left = left != NULL && left_count != 0U;
    uint16_t fallback = have_above ? above[0] : have_left ? left[0] : domain->midpoint;
    uint16_t above_fallback = !have_above && !have_left ? (domain->midpoint - 1U) : fallback;
    uint16_t left_fallback = !have_above && !have_left ? (domain->midpoint + 1U) : fallback;
    uint16_t corner = has_above_left ? above_left : fallback;
    if (edge_count > 136U)
        return AV2H_UNSUPPORTED;
    prepare_negative_u16(mode, references, above_edge, left_edge, corner);
    for (index = 0U; index < edge_count; ++index) {
        size_t above_available =
            above_count > 2U * width ? 2U * width : above_count;
        size_t left_available =
            left_count > 2U * height ? 2U * height : left_count;
        above_edge[index] =
            have_above
                ? above[index < above_available ? index : above_available - 1U]
                : above_fallback;
        left_edge[index] =
            have_left
                ? left[index < left_available ? index : left_available - 1U]
                : left_fallback;
    }
    if (disable_edge_filter == 0U && mode->mrl_index == 0U &&
        (mode->enable_ibp != 0U || mode->edge_ibp != 0U) && have_above &&
        have_left && width + height >= 24U) {
        corner = (uint16_t)((left_edge[0] * 5U + above_edge[-1] * 6U +
                             above_edge[0] * 5U + 8U) >>
                            4U);
        above_edge[-1] = left_edge[-1] = corner;
    }
    if (disable_edge_filter == 0U && have_above && mode->mrl_index == 0U) {
        av2h_directional_filter_edge_u16(
            above_edge - 1, (size_t)width + height + 1U,
            av2h_directional_filter_strength(
                width, height, angle - 90,
                (mode->enable_ibp ||
                 (mode->edge_ibp && (width != 4U || height != 4U)))
                    ? mode->above_smooth
                    : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    if (disable_edge_filter == 0U && have_left && mode->mrl_index == 0U) {
        av2h_directional_filter_edge_u16(
            left_edge - 1,
            (size_t)height + 1U + (mode->enable_ibp ? width : 0U),
            av2h_directional_filter_strength(
                height, width, angle,
                (mode->enable_ibp ||
                 (mode->edge_ibp && (width != 4U || height != 4U)))
                    ? mode->left_smooth
                    : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    above_edge[max_base + 1] = above_edge[max_base];
    above_edge[max_base + 2] = above_edge[max_base];
    if (mode->enable_ibp) {
        left_edge[max_base + 1] = left_edge[max_base];
        left_edge[max_base + 2] = left_edge[max_base];
    }
    return AV2H_OK;
}

Av2hStatus av2h_prepare_directional_zone2_u16(const Av2hLeafMode *mode, uint16_t width,
                                              uint16_t height,
                                              const Av2hLumaReferencesU16 *references,
                                              uint8_t disable_edge_filter, int angle,
                                              uint16_t *above_edge, uint16_t *left_edge,
                                              size_t edge_count, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t above_left;
    uint8_t has_above_left;
    int have_above;
    int have_left;
    size_t index;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    above_left = references->above_left;
    has_above_left = references->has_above_left;
    have_above = above != NULL && above_count != 0U;
    have_left = left != NULL && left_count != 0U;
    uint16_t fallback = have_above ? above[0] : have_left ? left[0] : domain->midpoint;
    uint16_t corner = has_above_left ? above_left : fallback;
    if (edge_count > 136U)
        return AV2H_UNSUPPORTED;
    prepare_negative_u16(mode, references, above_edge, left_edge, corner);
    for (index = 0U; index < edge_count; ++index) {
        above_edge[index] = have_above
                                ? above[index < above_count && index < width
                                            ? index
                                            : (above_count < width ? above_count : width) - 1U]
                            : have_left ? left[0]
                                        : (domain->midpoint - 1U);
        left_edge[index] = have_left ? left[index < left_count && index < height
                                                ? index
                                                : (left_count < height ? left_count : height) - 1U]
                           : have_above ? above[0]
                                        : (domain->midpoint + 1U);
    }
    if (disable_edge_filter == 0U && mode->mrl_index == 0U && have_above &&
        have_left && width + height >= 24U) {
        corner = (uint16_t)((left_edge[0] * 5U + above_edge[-1] * 6U +
                             above_edge[0] * 5U + 8U) >>
                            4U);
        above_edge[-1] = left_edge[-1] = corner;
    }
    if (disable_edge_filter == 0U && have_above && mode->mrl_index == 0U) {
        size_t filter_count =
            above_count < width ? above_count + 1U : (size_t)width + 1U;
        av2h_directional_filter_edge_u16(
            above_edge - 1, filter_count,
            av2h_directional_filter_strength(
                width, height, angle - 90,
                (mode->enable_ibp ||
                 (mode->edge_ibp && (width != 4U || height != 4U)))
                    ? mode->above_smooth
                    : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    if (disable_edge_filter == 0U && have_left && mode->mrl_index == 0U) {
        size_t filter_count =
            left_count < height ? left_count + 1U : (size_t)height + 1U;
        av2h_directional_filter_edge_u16(
            left_edge - 1, filter_count,
            av2h_directional_filter_strength(
                height, width, angle - 180,
                (mode->enable_ibp ||
                 (mode->edge_ibp && (width != 4U || height != 4U)))
                    ? mode->left_smooth
                    : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    above_edge[width] = above_edge[width - 1U];
    left_edge[height] = left_edge[height - 1U];
    return AV2H_OK;
}

Av2hStatus av2h_prepare_directional_zone3_u16(const Av2hLeafMode *mode, uint16_t width,
                                              uint16_t height,
                                              const Av2hLumaReferencesU16 *references,
                                              uint8_t disable_edge_filter, int angle,
                                              uint16_t *above_edge, uint16_t *left_edge,
                                              int max_base, size_t edge_count,
                                              const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    const uint16_t *above;
    const uint16_t *left;
    size_t above_count;
    size_t left_count;
    uint16_t above_left;
    uint8_t has_above_left;
    int have_above;
    int have_left;
    size_t index;
    above = references->above;
    above_count = references->above_count;
    left = references->left;
    left_count = references->left_count;
    above_left = references->above_left;
    has_above_left = references->has_above_left;
    have_above = above != NULL && above_count != 0U;
    have_left = left != NULL && left_count != 0U;
    uint16_t fallback = have_above ? above[0] : have_left ? left[0] : domain->midpoint;
    uint16_t above_fallback = !have_above && !have_left ? (domain->midpoint - 1U) : fallback;
    uint16_t left_fallback = !have_above && !have_left ? (domain->midpoint + 1U) : fallback;
    uint16_t corner = has_above_left ? above_left : fallback;
    if (edge_count > 136U)
        return AV2H_UNSUPPORTED;
    prepare_negative_u16(mode, references, above_edge, left_edge, corner);
    for (index = 0U; index < edge_count; ++index) {
        size_t above_available =
            above_count > 2U * width ? 2U * width : above_count;
        size_t left_available =
            left_count > 2U * height ? 2U * height : left_count;
        above_edge[index] =
            have_above
                ? above[index < above_available ? index : above_available - 1U]
                : above_fallback;
        left_edge[index] =
            have_left
                ? left[index < left_available ? index : left_available - 1U]
                : left_fallback;
    }
    if (disable_edge_filter == 0U && mode->mrl_index == 0U &&
        (mode->enable_ibp != 0U || mode->edge_ibp != 0U) && have_above &&
        have_left && width + height >= 24U) {
        corner = (uint16_t)((left_edge[0] * 5U + above_edge[-1] * 6U +
                             above_edge[0] * 5U + 8U) >>
                            4U);
        above_edge[-1] = left_edge[-1] = corner;
    }
    if (disable_edge_filter == 0U && mode->mrl_index == 0U && have_above) {
        av2h_directional_filter_edge_u16(
            above_edge - 1,
            (size_t)width + (mode->enable_ibp ? height : 0U) + 1U,
            av2h_directional_filter_strength(
                width, height, mode->enable_ibp ? angle - 270 : angle - 90,
                (mode->enable_ibp ||
                 (mode->edge_ibp && (width != 4U || height != 4U)))
                    ? mode->above_smooth
                    : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    if (disable_edge_filter == 0U && mode->mrl_index == 0U && have_left) {
        av2h_directional_filter_edge_u16(
            left_edge - 1, (size_t)height + width + 1U,
            av2h_directional_filter_strength(
                height, width, angle - 180,
                (mode->enable_ibp ||
                 (mode->edge_ibp && (width != 4U || height != 4U)))
                    ? mode->left_smooth
                    : (uint8_t)(mode->above_smooth || mode->left_smooth)));
    }
    left_edge[max_base + 1] = left_edge[max_base];
    left_edge[max_base + 2] = left_edge[max_base];
    if (mode->mrl_index == 0U && mode->enable_ibp != 0U) {
        above_edge[max_base + 1] = above_edge[max_base];
        above_edge[max_base + 2] = above_edge[max_base];
    }
    return AV2H_OK;
}

#include "internal/sample_kernel_u8.inc"
#include "internal/intra_prediction_edges_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/intra_prediction_edges_kernel.inc"
#include "internal/sample_kernel_end.inc"
