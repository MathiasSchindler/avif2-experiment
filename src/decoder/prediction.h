#ifndef AV2H_DECODER_PREDICTION_H
#define AV2H_DECODER_PREDICTION_H

#include "base.h"
#include "block_geometry.h"
#include "leaf_mode.h"

typedef int64_t Av2hPredictionSum;

typedef struct {
    const uint8_t *left;
    size_t left_count;
    const uint8_t *above;
    size_t above_count;
    const uint8_t *secondary_left;
    size_t secondary_left_count;
    const uint8_t *secondary_above;
    size_t secondary_above_count;
    uint8_t secondary_above_left;
    uint8_t has_secondary_above_left;
    uint8_t above_left;
    uint8_t has_above_left;
    uint8_t above_negative[5U];
    uint8_t left_negative[5U];
    uint8_t negative_count;
} Av2hLumaReferences;

typedef struct {
    const uint16_t *left;
    size_t left_count;
    const uint16_t *above;
    size_t above_count;
    const uint16_t *secondary_left;
    size_t secondary_left_count;
    const uint16_t *secondary_above;
    size_t secondary_above_count;
    uint16_t secondary_above_left;
    uint16_t above_left;
    uint16_t above_negative[5U];
    uint16_t left_negative[5U];
    uint8_t negative_count;
    uint8_t has_secondary_above_left;
    uint8_t has_above_left;
} Av2hLumaReferencesU16;

Av2hStatus av2h_luma_predict(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_chroma_predict(
    const Av2hFirstChromaMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_luma_predict_u16(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferencesU16 *references,
    uint16_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_chroma_predict_u16(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferencesU16 *references,
    uint16_t *predictor,
    size_t predictor_stride_samples);

#endif
