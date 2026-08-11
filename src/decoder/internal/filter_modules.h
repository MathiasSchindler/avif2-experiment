#ifndef AV2H_DECODER_INTERNAL_FILTER_MODULES_H
#define AV2H_DECODER_INTERNAL_FILTER_MODULES_H

#include "decoder/filter.h"

int16_t av2h_deblock_side_threshold(size_t index);
Av2hStatus av2h_deblock_quantizer_thresholds(
    const Av2hDeblockCell *cell,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    unsigned int plane,
    unsigned int pass,
    uint16_t *q_threshold,
    uint16_t *side_threshold);
int av2h_deblock_map_valid(
    const Av2hFilterPlanes *planes, const Av2hDeblockMap *map);
int av2h_ccso_parameters_valid(const Av2hFrameState *frame);
int av2h_ccso_controls_valid(
    const Av2hFrameState *frame, const Av2hCcsoControlMap *controls);

#endif