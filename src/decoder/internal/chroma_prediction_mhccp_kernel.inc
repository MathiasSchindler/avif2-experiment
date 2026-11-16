static void AV2H_KERNEL_NAME(mhccp_accumulate, mhccp_accumulate_u16)(
    AV2H_KERNEL_TYPE(int32_t, int64_t) matrix[3U][3U],
    AV2H_KERNEL_TYPE(int32_t, int64_t) target[3U], const int32_t vector[3U],
                             int32_t chroma) {
    int first;
    for (first = 0; first < 3; ++first) {
        int second;
        target[first] += (AV2H_KERNEL_TYPE(int32_t, int64_t))vector[first] * chroma;
        for (second = first; second < 3; ++second) {
            matrix[first][second] += (AV2H_KERNEL_TYPE(int32_t, int64_t))vector[first] * vector[second];
        }
    }
}

static uint16_t AV2H_KERNEL_NAME(
    mhccp_luma_q3,
    mhccp_luma_q3_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *luma_plane, uint16_t chroma_x,
                              uint16_t chroma_y) {
    size_t x0 = (size_t)chroma_x * 2U;
    size_t y0 = (size_t)chroma_y * 2U;
    size_t x1 = x0 + 1U;
    size_t y1 = y0 + 1U;
    return (uint16_t)((AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                        av2h_chroma_plane_coded_sample_u16)(luma_plane, x0, y0) +
                       AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                        av2h_chroma_plane_coded_sample_u16)(luma_plane, x1, y0) +
                       AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                        av2h_chroma_plane_coded_sample_u16)(luma_plane, x0, y1) +
                       AV2H_KERNEL_NAME(av2h_chroma_plane_coded_sample,
                                        av2h_chroma_plane_coded_sample_u16)(luma_plane, x1, y1))
                      << 1U);
}
static uint16_t AV2H_KERNEL_NAME(
    mhccp_luma_filter2_q3,
    mhccp_luma_filter2_q3_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *luma_plane, uint16_t chroma_x,
                                      uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U;
    return (uint16_t)(AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, left_x, center_y) +
                      4U * AV2H_KERNEL_NAME(
                               av2h_chroma_plane_coded_sample,
                               av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, right_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, top_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, bottom_y));
}
static uint16_t AV2H_KERNEL_NAME(
    mhccp_luma_filter2_top_clamped_q3,
    mhccp_luma_filter2_top_clamped_q3_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *luma_plane,
                                                  uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U;
    size_t bottom_y = center_y + 1U;
    return (uint16_t)(AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, left_x, center_y) +
                      5U * AV2H_KERNEL_NAME(
                               av2h_chroma_plane_coded_sample,
                               av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, right_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, bottom_y));
}
static uint16_t AV2H_KERNEL_NAME(
    mhccp_luma_filter1_q3,
    mhccp_luma_filter1_q3_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *luma_plane, uint16_t chroma_x,
                                      uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U;
    size_t bottom_y = center_y + 1U;
    return (uint16_t)(AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, left_x, center_y) +
                      2U * AV2H_KERNEL_NAME(
                               av2h_chroma_plane_coded_sample,
                               av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, right_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, left_x, bottom_y) +
                      2U * AV2H_KERNEL_NAME(
                               av2h_chroma_plane_coded_sample,
                               av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, bottom_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, right_x, bottom_y));
}
static uint16_t AV2H_KERNEL_NAME(
    mhccp_luma_filter1_sb_top_q3,
    mhccp_luma_filter1_sb_top_q3_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *luma_plane,
                                             uint16_t chroma_x, uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = block_y == 0U ? 0U : (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U;
    return (uint16_t)(2U * (AV2H_KERNEL_NAME(
                                av2h_chroma_plane_coded_sample,
                                av2h_chroma_plane_coded_sample_u16)(luma_plane, left_x, center_y) +
                            2U *
                                AV2H_KERNEL_NAME(
                                    av2h_chroma_plane_coded_sample,
                                    av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, center_y) +
                            AV2H_KERNEL_NAME(
                                av2h_chroma_plane_coded_sample,
                                av2h_chroma_plane_coded_sample_u16)(luma_plane, right_x, center_y)));
}
static uint16_t AV2H_KERNEL_NAME(
    mhccp_luma_filter2_sb_top_q3,
    mhccp_luma_filter2_sb_top_q3_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *luma_plane,
                                             uint16_t chroma_x, uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = block_y == 0U ? 0U : (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U;
    return (uint16_t)(AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, left_x, center_y) +
                      6U * AV2H_KERNEL_NAME(
                               av2h_chroma_plane_coded_sample,
                               av2h_chroma_plane_coded_sample_u16)(luma_plane, center_x, center_y) +
                      AV2H_KERNEL_NAME(
                          av2h_chroma_plane_coded_sample,
                          av2h_chroma_plane_coded_sample_u16)(luma_plane, right_x, center_y));
}
