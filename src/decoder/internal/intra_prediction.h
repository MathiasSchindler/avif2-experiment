#ifndef AV2H_DECODER_INTERNAL_INTRA_PREDICTION_H
#define AV2H_DECODER_INTERNAL_INTRA_PREDICTION_H

#include "decoder/internal/sample_math.h"
#include "decoder/prediction.h"

int av2h_luma_prediction_dimension_supported(uint16_t dimension);

static inline int av2h_intra_mrl_angle_delta(uint8_t mrl_index) {
    return mrl_index == 1U ? 1 : mrl_index == 2U ? -1 : 0;
}

Av2hStatus av2h_predict_intra_dip(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

int av2h_directional_filter_strength(int size0, int size1, int delta,
                                       uint8_t smooth);

void av2h_directional_filter_edge(uint8_t *edge, size_t count,
                                    int strength);

void av2h_directional_filter_edge_u16(uint16_t *edge, size_t count,
                                        int strength);

int av2h_directional_base_angle(Av2hIntraMode mode);

int av2h_luma_wide_angle(int angle, uint16_t width, uint16_t height);

Av2hStatus av2h_predict_directional(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples,
    uint8_t use_idif,
    uint8_t filter_edges);

Av2hStatus av2h_predict_directional_idif(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_directional_idif_unfiltered(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_8x4_ibp(
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_luma_directional_feature(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d203_fallback(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_vertical_multiline(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_horizontal_mrl(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_horizontal_multiline(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_directional_multiline_ibp(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_horizontal_multiline_angle(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_vertical_reference(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_horizontal_multiline_tall(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_vertical_multiline_tall(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d203_multiline(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_dc(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_vertical_small(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_smooth_vertical(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_smooth_horizontal(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_smooth(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_smooth_16x8(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_smooth_8x32(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_paeth(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_horizontal(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_missing_edges_ibp(
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_mrl1(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_zone2_mrl2(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_diagonal(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d67_mrl1_minus_one(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d67_4x4_mrl1_minus_two(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d67_8x8_mrl1_minus_two(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d67_4x4_mrl3_minus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_minus_one(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_plus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d45_mrl1_plus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d203_mrl1_plus_three(
    const Av2hLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_d203_plus_three(
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_predict_multiline_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                      const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                      size_t predictor_stride_samples, uint8_t use_idif,
                                      const Av2hSampleDomain *domain);

Av2hStatus av2h_predict_zone2_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, uint8_t disable_edge_filter,
                                  uint8_t use_idif, int angle, const Av2hSampleDomain *domain);

Av2hStatus av2h_predict_zone1_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, uint8_t disable_edge_filter,
                                  uint8_t use_idif, int angle, const Av2hSampleDomain *domain);

Av2hStatus av2h_predict_zone3_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, uint8_t disable_edge_filter,
                                  uint8_t use_idif, int angle, const Av2hSampleDomain *domain);

Av2hStatus av2h_predict_cardinal_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                     const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                     size_t predictor_stride_samples,
                                     const Av2hSampleDomain *domain);

Av2hStatus av2h_predict_smooth_u16(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                   const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                   size_t predictor_stride_samples, const Av2hSampleDomain *domain);

Av2hStatus av2h_predict_paeth_u16(uint16_t width, uint16_t height,
                                  const Av2hLumaReferencesU16 *references, uint16_t *predictor,
                                  size_t predictor_stride_samples, const Av2hSampleDomain *domain);

Av2hStatus av2h_luma_predict_u16_adjusted(const Av2hLeafMode *mode, uint16_t width, uint16_t height,
                                          const Av2hLumaReferencesU16 *references,
                                          uint16_t *predictor, size_t predictor_stride_samples,
                                          int angle_adjust, uint8_t disable_edge_filter,
                                          uint8_t use_idif, const Av2hSampleDomain *domain);

/* Edge pointers have five negative and 136 nonnegative sample slots. */
void av2h_prepare_directional_edges(
    const Av2hLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t filter_edges,
    uint8_t *above,
    uint8_t *left,
    size_t edge_count,
    int missing_left_ibp,
    uint8_t edge_ibp);

Av2hStatus av2h_prepare_directional_zone1_u16(const Av2hLeafMode *mode, uint16_t width,
                                              uint16_t height,
                                              const Av2hLumaReferencesU16 *references,
                                              uint8_t disable_edge_filter, int angle,
                                              uint16_t *above_edge, uint16_t *left_edge,
                                              int max_base, size_t edge_count,
                                              const Av2hSampleDomain *domain);

Av2hStatus av2h_prepare_directional_zone2_u16(const Av2hLeafMode *mode, uint16_t width,
                                              uint16_t height,
                                              const Av2hLumaReferencesU16 *references,
                                              uint8_t disable_edge_filter, int angle,
                                              uint16_t *above_edge, uint16_t *left_edge,
                                              size_t edge_count, const Av2hSampleDomain *domain);

Av2hStatus av2h_prepare_directional_zone3_u16(const Av2hLeafMode *mode, uint16_t width,
                                              uint16_t height,
                                              const Av2hLumaReferencesU16 *references,
                                              uint8_t disable_edge_filter, int angle,
                                              uint16_t *above_edge, uint16_t *left_edge,
                                              int max_base, size_t edge_count,
                                              const Av2hSampleDomain *domain);

#endif
