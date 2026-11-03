static void AV2H_KERNEL_NAME(cfl_predict_samples, cfl_predict_samples_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t luma_x, uint16_t luma_y, uint16_t width, uint16_t height, uint16_t visible_width,
    uint16_t visible_height, uint8_t full_resolution, uint8_t filter_index, int32_t alpha,
    uint8_t alpha_extra_bits, int32_t average_q3, AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor,
    size_t predictor_stride_samples, const Av2hSampleDomain *domain) {
    uint16_t row;
    for (row = 0U; row < height; ++row) {
        uint16_t column;
        for (column = 0U; column < width; ++column) {
            size_t sample_x = (size_t)luma_x + (size_t)column * 2U;
            size_t sample_y = (size_t)luma_y + (size_t)row * 2U;
            size_t left_x = (column & 31U) == 0U ? sample_x : sample_x - 1U;
            int32_t recon_q3;
            if (full_resolution) {
                sample_x = (size_t)luma_x + (column < visible_width ? column : visible_width - 1U);
                sample_y = (size_t)luma_y + (row < visible_height ? row : visible_height - 1U);
                recon_q3 = (int32_t)AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                     av2h_chroma_plane_coded_sample_u16)(
                               luma_plane, sample_x, sample_y)
                           << 3U;
            } else if (filter_index == 1U) {
                recon_q3 = (int32_t)(AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, left_x, sample_y) +
                                     2U * AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                           av2h_chroma_plane_coded_sample_u16)(
                                              luma_plane, sample_x, sample_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x + 1U, sample_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, left_x, sample_y + 1U) +
                                     2U * AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                           av2h_chroma_plane_coded_sample_u16)(
                                              luma_plane, sample_x, sample_y + 1U) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x + 1U, sample_y + 1U));
            } else if (filter_index == 2U) {
                size_t top_y = (row & 31U) == 0U ? sample_y : sample_y - 1U;
                recon_q3 = (int32_t)(AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, left_x, sample_y) +
                                     4U * AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                           av2h_chroma_plane_coded_sample_u16)(
                                              luma_plane, sample_x, sample_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x + 1U, sample_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x, top_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x, sample_y + 1U));
            } else {
                recon_q3 = (int32_t)(AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x, sample_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x + 1U, sample_y) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x, sample_y + 1U) +
                                     AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                                      av2h_chroma_plane_coded_sample_u16)(
                                         luma_plane, sample_x + 1U, sample_y + 1U))
                           << 1U;
            }
            {
                size_t output = (size_t)row * predictor_stride_samples + column;
                int32_t scaled = av2h_sample_round_pow2_signed(
                    (int64_t)alpha * (recon_q3 - average_q3), 6U + alpha_extra_bits);
                predictor[output] = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(
                    (int32_t)predictor[output] + scaled, domain);
            }
        }
    }
}