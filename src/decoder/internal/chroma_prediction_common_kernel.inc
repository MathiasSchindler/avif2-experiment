int AV2H_KERNEL_NAME(
    av2h_chroma_plane_region_reconstructed,
    av2h_chroma_plane_region_reconstructed_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *plane, uint16_t x,
                                           uint16_t y, uint16_t width, uint16_t height) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < height; row += 4U) {
        for (column = 0U; column < width; column += 4U) {
            if (!AV2H_KERNEL_NAME(
                    av2h_plane_sample_reconstructed,
                    av2h_chroma_plane_sample_reconstructed_u16)(plane, (size_t)x + column, (size_t)y + row)) {
                return 0;
            }
        }
    }
    return 1;
}
AV2H_KERNEL_TYPE(uint8_t, uint16_t)
AV2H_KERNEL_NAME(
    av2h_chroma_plane_coded_sample,
    av2h_chroma_plane_coded_sample_u16)(const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) *plane, size_t x, size_t y) {
    return AV2H_KERNEL_NAME(
        av2h_reference_load_u8,
        av2h_reference_load_u16)(plane->data, plane->stride_samples, plane->width, plane->height,
                                  x, y);
}