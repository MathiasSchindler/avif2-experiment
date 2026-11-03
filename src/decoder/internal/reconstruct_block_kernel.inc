static void AV2H_KERNEL_NAME(reconstruct_dpcm_samples, reconstruct_dpcm_samples_u16)(
    const Av2hCoeffBlock *coefficients, const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor,
    size_t predictor_stride_samples, uint16_t width, uint16_t height, uint8_t horizontal,
    unsigned int scale_bits, uint8_t truncate_toward_zero,
    AV2H_KERNEL_TYPE(Av2hBlockReconstruction, Av2hBlockReconstructionU16) * reconstruction,
    const Av2hSampleDomain *domain) {
    uint16_t lane;
    uint16_t lane_count = horizontal ? height : width;
    uint16_t lane_length = horizontal ? width : height;
    for (lane = 0U; lane < lane_count; ++lane) {
        uint16_t position;
        int32_t delay = 0;
        for (position = 0U; position < lane_length; ++position) {
            uint16_t row = horizontal ? lane : position;
            uint16_t column = horizontal ? position : lane;
            size_t sample = (size_t)row * width + column;
            int32_t current = truncate_toward_zero
                                  ? coefficients->dequantized[sample] / (int32_t)(1U << scale_bits)
                                  : coefficients->dequantized[sample] >> scale_bits;
            int32_t residual = current + delay;
            reconstruction->residual[sample] = (AV2H_KERNEL_TYPE(int16_t, int32_t))residual;
            reconstruction->pixels[sample] =
                (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                    (int32_t)predictor[(size_t)row * predictor_stride_samples + column] + residual,
                    domain);
            delay += current;
        }
    }
}