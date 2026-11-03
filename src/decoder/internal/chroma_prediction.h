#ifndef AV2H_DECODER_INTERNAL_CHROMA_PREDICTION_H
#define AV2H_DECODER_INTERNAL_CHROMA_PREDICTION_H

#include "decoder/plane.h"
#include "decoder/prediction.h"

struct Av2hDecodeDiagnostics;

int av2h_chroma_plane_region_reconstructed(
    const Av2hReconstructionPlane *plane,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height);

uint8_t av2h_chroma_plane_coded_sample(
    const Av2hReconstructionPlane *plane,
    size_t x,
    size_t y);

int av2h_chroma_plane_sample_reconstructed_u16(
    const Av2hReconstructionPlaneU16 *plane,
    size_t x,
    size_t y);

int av2h_chroma_plane_region_reconstructed_u16(
    const Av2hReconstructionPlaneU16 *plane,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height);

uint16_t av2h_chroma_plane_coded_sample_u16(
    const Av2hReconstructionPlaneU16 *plane,
    size_t x,
    size_t y);

uint16_t av2h_chroma_luma_sample_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y);

uint16_t av2h_chroma_luma_sb_top_sample_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y);

uint16_t av2h_chroma_luma_left_clamped_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y);

uint16_t av2h_chroma_luma_sb_top_left_clamped_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y);

int av2h_chroma_geometry_supported(uint16_t width, uint16_t height);

uint8_t av2h_chroma_dc_predictor_average(
    unsigned int sum,
    unsigned int count,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left);

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
    struct Av2hDecodeDiagnostics *diagnostics,
    const Av2hChromaLeafMode *mode,
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor,
    size_t predictor_stride_samples);

Av2hStatus av2h_mhccp_predict_observed_u16(
    const Av2hChromaLeafMode *mode,
    const Av2hReconstructionPlaneU16 *luma_plane,
    const Av2hReconstructionPlaneU16 *chroma_plane,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint16_t *predictor,
    size_t predictor_stride_samples);

#endif