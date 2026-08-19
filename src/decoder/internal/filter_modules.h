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
int av2h_deblock_map_valid_u16(
    const Av2hFilterPlanesU16 *planes, const Av2hDeblockMap *map);
int av2h_ccso_parameters_valid(const Av2hFrameState *frame);
int av2h_ccso_controls_valid(
    const Av2hFrameState *frame, const Av2hCcsoControlMap *controls);
Av2hStatus av2h_cdef_filter_frame_with_sequence(
    Av2hFilterPlanes *destination,
    const Av2hFilterPlanes *deblocked,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hCdefControlMap *controls);
Av2hStatus av2h_cdef_filter_frame_u16_with_sequence(
    Av2hFilterPlanesU16 *destination,
    const Av2hFilterPlanesU16 *deblocked,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hCdefControlMap *controls);
Av2hStatus av2h_ccso_filter_frame_with_sequence(
    Av2hFilterPlanes *planes,
    const uint8_t *pre_cdef_luma,
    size_t pre_cdef_luma_stride_samples,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hCcsoControlMap *controls);
Av2hStatus av2h_ccso_filter_frame_u16_with_sequence(
    Av2hFilterPlanesU16 *planes,
    const uint16_t *pre_cdef_luma,
    size_t pre_cdef_luma_stride_samples,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hCcsoControlMap *controls);
Av2hStatus av2h_gdf_filter_frame_with_sequence(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *baseline_luma,
    const Av2hFilterPlane *cdef_luma,
    const Av2hFilterPlane *deblocked_luma,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hGdfControlMap *controls);
Av2hStatus av2h_gdf_filter_frame_u16_with_sequence(
    Av2hFilterPlaneU16 *destination,
    const Av2hFilterPlaneU16 *cdef_luma,
    const Av2hFilterPlaneU16 *deblocked_luma,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hGdfControlMap *controls);

#endif