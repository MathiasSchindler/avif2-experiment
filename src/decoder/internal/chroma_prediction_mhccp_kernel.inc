static void AV2H_KERNEL_NAME(mhccp_accumulate,
                             mhccp_accumulate_u16)(AV2H_KERNEL_TYPE(int32_t, int64_t)
                                                       matrix[3U][3U],
                                                   AV2H_KERNEL_TYPE(int32_t, int64_t) target[3U],
                                                   const int32_t vector[3U], int32_t chroma) {
    int first;
    for (first = 0; first < 3; ++first) {
        int second;
        target[first] += (AV2H_KERNEL_TYPE(int32_t, int64_t))vector[first] * chroma;
        for (second = first; second < 3; ++second) {
            matrix[first][second] +=
                (AV2H_KERNEL_TYPE(int32_t, int64_t))vector[first] * vector[second];
        }
    }
}

static uint16_t AV2H_KERNEL_NAME(mhccp_luma_q3, mhccp_luma_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t x0 = (size_t)chroma_x * 2U;
    size_t y0 = (size_t)chroma_y * 2U;
    size_t x1 = x0 + 1U < luma_plane->width ? x0 + 1U : x0;
    size_t y1 = y0 + 1U < luma_plane->height ? y0 + 1U : y0;
    return (uint16_t)((luma_plane->data[y0 * luma_plane->stride_samples + x0] +
                       luma_plane->data[y0 * luma_plane->stride_samples + x1] +
                       luma_plane->data[y1 * luma_plane->stride_samples + x0] +
                       luma_plane->data[y1 * luma_plane->stride_samples + x1])
                      << 1U);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter2_q3, mhccp_luma_filter2_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ? center_y + 1U : center_y;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      4U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
                      luma_plane->data[top_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter2_top_clamped_q3,
                                 mhccp_luma_filter2_top_clamped_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ? center_y + 1U : center_y;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      5U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter1_q3, mhccp_luma_filter1_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ? center_y + 1U : center_y;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      2U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + left_x] +
                      2U * luma_plane->data[bottom_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[bottom_y * luma_plane->stride_samples + right_x]);
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter1_sb_top_q3, mhccp_luma_filter1_sb_top_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    return (uint16_t)(2U *
                      (luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                       2U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                       luma_plane->data[center_y * luma_plane->stride_samples + right_x]));
}
static uint16_t AV2H_KERNEL_NAME(mhccp_luma_filter2_sb_top_q3, mhccp_luma_filter2_sb_top_q3_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * luma_plane,
    uint16_t chroma_x, uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ? center_x + 1U : center_x;
    return (uint16_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                      6U * luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
                      luma_plane->data[center_y * luma_plane->stride_samples + right_x]);
}
