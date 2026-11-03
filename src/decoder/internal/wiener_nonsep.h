#ifndef AV2H_DECODER_INTERNAL_WIENER_NONSEP_H
#define AV2H_DECODER_INTERNAL_WIENER_NONSEP_H

#include "decoder/frame.h"

#define AV2H_WIENER_NONSEP_LUMA_TAPS 16U
#define AV2H_WIENER_NONSEP_CHROMA_TAPS 18U
#define AV2H_WIENER_NONSEP_MAX_SUBSETS 4U

typedef struct {
    uint8_t coefficient_count;
    uint8_t subset_count;
    uint8_t symmetric_first;
    uint8_t bits[AV2H_WIENER_NONSEP_MAX_TAPS];
    int16_t minimum[AV2H_WIENER_NONSEP_MAX_TAPS];
    uint32_t subset_mask[AV2H_WIENER_NONSEP_MAX_SUBSETS];
} Av2hWienerNonsepParameters;

const Av2hWienerNonsepParameters *av2h_wienerns_parameters(unsigned int plane);

/* Filters that may be referenced by a frame filter match index. Slot zero is
   the all zero filter, slots one to class_count - 1 hold previously decoded
   classes of the same plane and the remaining slots hold translated
   pre-computed Wiener filters (luma only). */
Av2hStatus av2h_wienerns_frame_predictor(
    unsigned int plane, uint32_t match_index, unsigned int class_index, unsigned int class_count,
    const int16_t decoded[AV2H_WIENER_NONSEP_MAX_CLASSES][AV2H_WIENER_NONSEP_MAX_TAPS],
    int16_t taps[AV2H_WIENER_NONSEP_MAX_TAPS]);

/* Maps a pre-computed Wiener classifier index to the signalled class of a
   multi class frame filter. */
const uint8_t *av2h_wienerns_class_map(unsigned int class_count, unsigned int set_index);

#endif
