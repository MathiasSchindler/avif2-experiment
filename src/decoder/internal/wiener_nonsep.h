#ifndef AV2H_DECODER_INTERNAL_WIENER_NONSEP_H
#define AV2H_DECODER_INTERNAL_WIENER_NONSEP_H

#include "decoder/frame.h"

Av2hStatus av2h_wienerns_luma_frame_predictor(
    uint8_t match_index,
    int16_t taps[AV2H_WIENER_NONSEP_MAX_TAPS]);

#endif