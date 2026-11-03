static inline AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    av2h_sample_load_u8,
    av2h_sample_load_u16)(const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *samples, size_t stride_samples, size_t x,
                                          size_t y) {
    return samples[y * stride_samples + x];
}

static inline AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    av2h_reference_load_u8,
    av2h_reference_load_u16)(const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *samples, size_t stride_samples,
                                             size_t width_samples, size_t height_samples, size_t x,
                                             size_t y) {
    if (x >= width_samples)
        x = width_samples - 1U;
    if (y >= height_samples)
        y = height_samples - 1U;
    return AV2H_KERNEL_NAME(av2h_sample_load_u8,
                            av2h_sample_load_u16)(samples, stride_samples, x, y);
}

static inline AV2H_KERNEL_TYPE(uint8_t, uint16_t) AV2H_KERNEL_NAME(
    av2h_reconstructed_sample_load_u8,
    av2h_reconstructed_sample_load_u16)(const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *samples,
                                                        size_t stride_samples, size_t x, size_t y) {
    return AV2H_KERNEL_NAME(av2h_sample_load_u8,
                            av2h_sample_load_u16)(samples, stride_samples, x, y);
}

static inline void AV2H_KERNEL_NAME(av2h_sample_store_u8, av2h_sample_store_u16)(
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) *samples, size_t stride_samples, size_t x, size_t y,
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) value) {
    samples[y * stride_samples + x] = value;
}

static inline void AV2H_KERNEL_NAME(av2h_sample_clip_store_u8, av2h_sample_clip_store_u16)(
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) *samples, size_t stride_samples, size_t x,
                                             size_t y, int32_t value
#if AV2H_KERNEL_WIDE
                                             ,
                                             const Av2hSampleDomain *domain
#endif
) {
#if !AV2H_KERNEL_WIDE
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    AV2H_KERNEL_NAME(
        av2h_sample_store_u8,
        av2h_sample_store_u16)(samples, stride_samples, x, y,
                         (AV2H_KERNEL_TYPE(
                                   uint8_t, uint16_t))av2h_sample_clip_to_domain(value, domain));
}

static inline void AV2H_KERNEL_NAME(av2h_plane_fill_u8, av2h_plane_fill_u16)(
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) *destination, size_t destination_stride_samples,
                                      size_t width_samples, size_t height_samples, AV2H_KERNEL_TYPE(uint8_t, uint16_t) value) {
    size_t row;
    for (row = 0U; row < height_samples; ++row) {
#if AV2H_KERNEL_WIDE
        size_t column;
        for (column = 0U; column < width_samples; ++column) {
            destination[row * destination_stride_samples + column] = value;
        }
#else
        av2h_memory_fill(destination + row * destination_stride_samples, value, width_samples);
#endif
    }
}

static inline void AV2H_KERNEL_NAME(av2h_plane_copy_u8, av2h_plane_copy_u16)(
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) *destination, size_t destination_stride_samples,
                                      const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *source, size_t source_stride_samples,
                                      size_t width_samples, size_t height_samples) {
    size_t row;
    for (row = 0U; row < height_samples; ++row) {
        av2h_memory_copy(destination + row * destination_stride_samples,
                         source + row * source_stride_samples,
                         width_samples * sizeof(*destination));
    }
}

static inline void AV2H_KERNEL_NAME(av2h_visible_crop_u8, av2h_visible_crop_u16)(
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) *destination, const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *source,
                                        size_t source_stride_samples, size_t width_samples,
                                        size_t height_samples) {
    AV2H_KERNEL_NAME(
        av2h_plane_copy_u8,
        av2h_plane_copy_u16)(destination, width_samples, source, source_stride_samples, width_samples,
                       height_samples);
}

static inline void AV2H_KERNEL_NAME(av2h_predictor_commit_u8, av2h_predictor_commit_u16)(
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) *destination, size_t destination_stride_samples,
                                            const AV2H_KERNEL_TYPE(uint8_t, uint16_t) *predictor,
                                            size_t predictor_stride_samples, size_t width_samples,
                                            size_t height_samples) {
    AV2H_KERNEL_NAME(
        av2h_plane_copy_u8,
        av2h_plane_copy_u16)(destination, destination_stride_samples, predictor, predictor_stride_samples,
                       width_samples, height_samples);
}
