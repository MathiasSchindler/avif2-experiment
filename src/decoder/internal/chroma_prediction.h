#ifndef AV2H_DECODER_INTERNAL_CHROMA_PREDICTION_H
#define AV2H_DECODER_INTERNAL_CHROMA_PREDICTION_H

#include "decoder/reconstruct.h"

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

#endif