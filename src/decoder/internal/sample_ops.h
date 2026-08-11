#ifndef AV2H_DECODER_INTERNAL_SAMPLE_OPS_H
#define AV2H_DECODER_INTERNAL_SAMPLE_OPS_H

#include "base.h"
#include "decoder/internal/sample_math.h"

/*
 * Callers validate non-null storage, dimensions, strides, and address
 * arithmetic before entering these typed inner operations. Reference loads
 * additionally require nonzero width and height.
 */
static inline uint8_t av2h_sample_load_u8(
    const uint8_t *samples, size_t stride_samples, size_t x, size_t y) {
    return samples[y * stride_samples + x];
}

static inline uint8_t av2h_reference_load_u8(
    const uint8_t *samples, size_t stride_samples,
    size_t width_samples, size_t height_samples, size_t x, size_t y) {
    if (x >= width_samples) x = width_samples - 1U;
    if (y >= height_samples) y = height_samples - 1U;
    return av2h_sample_load_u8(samples, stride_samples, x, y);
}

static inline uint8_t av2h_reconstructed_sample_load_u8(
    const uint8_t *samples, size_t stride_samples, size_t x, size_t y) {
    return av2h_sample_load_u8(samples, stride_samples, x, y);
}

static inline void av2h_sample_store_u8(
    uint8_t *samples, size_t stride_samples,
    size_t x, size_t y, uint8_t value) {
    samples[y * stride_samples + x] = value;
}

static inline void av2h_sample_clip_store_u8(
    uint8_t *samples, size_t stride_samples,
    size_t x, size_t y, int32_t value) {
    av2h_sample_store_u8(
        samples, stride_samples, x, y, av2h_sample_clip(value));
}

static inline void av2h_plane_fill_u8(
    uint8_t *destination, size_t destination_stride_samples,
    size_t width_samples, size_t height_samples, uint8_t value) {
    size_t row;
    for (row = 0U; row < height_samples; ++row) {
        av2h_memory_fill(
            destination + row * destination_stride_samples,
            value, width_samples);
    }
}

static inline void av2h_plane_copy_u8(
    uint8_t *destination, size_t destination_stride_samples,
    const uint8_t *source, size_t source_stride_samples,
    size_t width_samples, size_t height_samples) {
    size_t row;
    for (row = 0U; row < height_samples; ++row) {
        av2h_memory_copy(
            destination + row * destination_stride_samples,
            source + row * source_stride_samples, width_samples);
    }
}

static inline void av2h_visible_crop_u8(
    uint8_t *destination, const uint8_t *source,
    size_t source_stride_samples,
    size_t width_samples, size_t height_samples) {
    /* Visible planar output is tightly packed. */
    av2h_plane_copy_u8(
        destination, width_samples, source, source_stride_samples,
        width_samples, height_samples);
}

static inline void av2h_predictor_commit_u8(
    uint8_t *destination, size_t destination_stride_samples,
    const uint8_t *predictor, size_t predictor_stride_samples,
    size_t width_samples, size_t height_samples) {
    av2h_plane_copy_u8(
        destination, destination_stride_samples,
        predictor, predictor_stride_samples, width_samples, height_samples);
}

static inline uint16_t av2h_sample_load_u16(
    const uint16_t *samples, size_t stride_samples, size_t x, size_t y) {
    return samples[y * stride_samples + x];
}

static inline uint16_t av2h_reference_load_u16(
    const uint16_t *samples, size_t stride_samples,
    size_t width_samples, size_t height_samples, size_t x, size_t y) {
    if (x >= width_samples) x = width_samples - 1U;
    if (y >= height_samples) y = height_samples - 1U;
    return av2h_sample_load_u16(samples, stride_samples, x, y);
}

static inline uint16_t av2h_reconstructed_sample_load_u16(
    const uint16_t *samples, size_t stride_samples, size_t x, size_t y) {
    return av2h_sample_load_u16(samples, stride_samples, x, y);
}

static inline void av2h_sample_store_u16(
    uint16_t *samples, size_t stride_samples,
    size_t x, size_t y, uint16_t value) {
    samples[y * stride_samples + x] = value;
}

static inline void av2h_sample_clip_store_u16(
    uint16_t *samples, size_t stride_samples,
    size_t x, size_t y, int32_t value, const Av2hSampleDomain *domain) {
    av2h_sample_store_u16(
        samples, stride_samples, x, y,
        av2h_sample_clip_to_domain(value, domain));
}

static inline void av2h_plane_fill_u16(
    uint16_t *destination, size_t destination_stride_samples,
    size_t width_samples, size_t height_samples, uint16_t value) {
    size_t row;
    for (row = 0U; row < height_samples; ++row) {
        size_t column;
        for (column = 0U; column < width_samples; ++column) {
            destination[row * destination_stride_samples + column] = value;
        }
    }
}

static inline void av2h_plane_copy_u16(
    uint16_t *destination, size_t destination_stride_samples,
    const uint16_t *source, size_t source_stride_samples,
    size_t width_samples, size_t height_samples) {
    size_t row;
    for (row = 0U; row < height_samples; ++row) {
        av2h_memory_copy(
            destination + row * destination_stride_samples,
            source + row * source_stride_samples,
            width_samples * sizeof(uint16_t));
    }
}

static inline void av2h_visible_crop_u16(
    uint16_t *destination, const uint16_t *source,
    size_t source_stride_samples,
    size_t width_samples, size_t height_samples) {
    av2h_plane_copy_u16(
        destination, width_samples, source, source_stride_samples,
        width_samples, height_samples);
}

static inline void av2h_predictor_commit_u16(
    uint16_t *destination, size_t destination_stride_samples,
    const uint16_t *predictor, size_t predictor_stride_samples,
    size_t width_samples, size_t height_samples) {
    av2h_plane_copy_u16(
        destination, destination_stride_samples,
        predictor, predictor_stride_samples, width_samples, height_samples);
}

#endif