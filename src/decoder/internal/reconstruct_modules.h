#ifndef AV2H_DECODER_INTERNAL_RECONSTRUCT_MODULES_H
#define AV2H_DECODER_INTERNAL_RECONSTRUCT_MODULES_H

#include "decoder/reconstruct.h"

Av2hStatus av2h_inverse_secondary_transform(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY]);

Av2hStatus av2h_inverse_rows(
    const int32_t *input,
    int32_t *transposed,
    uint16_t width,
    uint16_t height,
    uint8_t shift,
    uint8_t identity,
    uint8_t adst,
    uint8_t flip_adst);

Av2hStatus av2h_inverse_columns(
    const int32_t *transposed,
    int32_t *output_block,
    uint16_t width,
    uint16_t height,
    uint8_t shift,
    uint8_t identity,
    uint8_t adst,
    uint8_t flip_adst);

Av2hStatus av2h_reconstruct_transform_generic(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction);

Av2hStatus av2h_reconstruct_lossless_4x4(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride_samples,
    Av2hFirstReconstruction *reconstruction);

Av2hStatus av2h_reconstruct_transform_generic_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);

Av2hStatus av2h_reconstruct_transform_8x16_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);

Av2hStatus av2h_reconstruct_transform_8x4_u16(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint16_t *predictor,
    size_t predictor_stride_samples,
    Av2hReconstructionScratchU16 *scratch,
    Av2hFirstReconstructionU16 *reconstruction);

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

#define AV2H_SUBPEL_BITS 4U
#define AV2H_INTRABC_MAX_BLOCK 128U

int32_t av2h_intrabc_plane_position(
    uint16_t block_position, int32_t displacement, uint8_t subsampling);

int32_t av2h_intrabc_integer_position(int32_t position);

Av2hStatus av2h_intrabc_convolve(
    const uint8_t *source8,
    const uint16_t *source16,
    size_t source_stride,
    int32_t source_x,
    int32_t source_y,
    uint32_t subpel_x,
    uint32_t subpel_y,
    uint16_t width,
    uint16_t height,
    uint8_t bit_depth,
    uint8_t *destination8,
    uint16_t *destination16);

int32_t av2h_intrabc_full_pel(int16_t displacement);

Av2hStatus av2h_intrabc_morph_u16(
    const uint16_t *plane_data,
    size_t stride,
    uint32_t plane_width,
    uint32_t plane_height,
    uint16_t coding_x,
    uint16_t coding_y,
    uint16_t coding_width,
    uint16_t coding_height,
    int32_t reference_x,
    int32_t reference_y,
    uint16_t width,
    uint16_t height,
    uint8_t bit_depth,
    uint16_t *predictor);

Av2hStatus av2h_intrabc_predict(
    const Av2hFirstLeafMode *mode,
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor);

int av2h_directional_base_angle(Av2hFirstLeafIntraMode mode);

int av2h_luma_wide_angle(int angle, uint16_t width, uint16_t height);

int av2h_luma_prediction_dimension_supported(uint16_t dimension);

int av2h_chroma_geometry_supported(uint16_t width, uint16_t height);

Av2hStatus av2h_predict_directional(
    const Av2hFirstLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride_samples,
    uint8_t use_idif,
    uint8_t filter_edges);

uint16_t av2h_cfl_resolve_divisor_32(uint32_t divisor, int *shift);

uint8_t av2h_chroma_dc_predictor_average(
    unsigned int sum,
    unsigned int count,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left);

uint8_t av2h_plane_sample_reconstructed(
    const Av2hReconstructionPlane *plane,
    size_t x,
    size_t y);

int32_t av2h_cfl_derive_alpha(
    int32_t sum_x,
    int32_t sum_y,
    int32_t sum_xx,
    int32_t sum_xy,
    int count,
    int shift);

Av2hStatus av2h_cfl_derived_predict(
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t filter_index,
    uint8_t derive_alpha,
    int32_t supplied_alpha_q3,
    uint16_t superblock_size,
    uint8_t chroma_format_idc,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_cfl_explicit_predict_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    int32_t alpha_q3,
    uint16_t superblock_size,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint16_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_cfl_derived_predict_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    const Av2hReconstructionPlaneU16 *chroma_plane,
    uint8_t filter_index,
    uint16_t superblock_size,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint16_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_cfl_derived_predict_u16_444(
    const Av2hReconstructionPlaneU16 *luma_plane,
    const Av2hReconstructionPlaneU16 *chroma_plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint16_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_cfl_explicit_predict_u16_444(
    const Av2hReconstructionPlaneU16 *luma_plane,
    int32_t alpha_q3,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint16_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_mhccp_predict_observed(
    const Av2hFirstChromaMode *mode,
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_mhccp_predict_observed_u16(
    const Av2hFirstChromaMode *mode,
    const Av2hReconstructionPlaneU16 *luma_plane,
    const Av2hReconstructionPlaneU16 *chroma_plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint16_t *predictor,
    size_t predictor_stride_samples);

int av2h_reconstruct_generic_selected(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    uint8_t tx_size);

#endif