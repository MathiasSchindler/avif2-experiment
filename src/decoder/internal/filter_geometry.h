#ifndef AV2H_DECODER_INTERNAL_FILTER_GEOMETRY_H
#define AV2H_DECODER_INTERNAL_FILTER_GEOMETRY_H

#include "decoder/frame.h"

#define AV2H_CCSO_LEGACY_UNIT_SIZE 64U

static inline size_t av2h_filter_coded_dimension(uint16_t value) {
    return ((size_t)value + 7U) & ~(size_t)7U;
}

static inline uint16_t av2h_filter_ccso_unit_size(const Av2hFrameState *frame) {
    return frame->ccso_unit_size != 0U ? frame->ccso_unit_size : AV2H_CCSO_LEGACY_UNIT_SIZE;
}

#endif