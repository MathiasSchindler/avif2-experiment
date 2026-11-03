static int AV2H_KERNEL_NAME(plane_sample_inside_active_tile, plane_sample_inside_active_tile_u16)(
    const AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * plane, size_t x,
    size_t y) {
    return plane_sample_inside_active_tile_cells(plane->reconstructed,
                                                 plane->reconstructed_stride_cells,
                                                 plane->reconstructed_count_cells, x, y);
}
static void AV2H_KERNEL_NAME(plane_mark_reconstructed, plane_mark_reconstructed_u16)(
    AV2H_KERNEL_TYPE(Av2hReconstructionPlane, Av2hReconstructionPlaneU16) * plane, uint16_t x,
    uint16_t y, uint16_t width, uint16_t height) {
    plane_mark_reconstructed_cells(plane->reconstructed, plane->reconstructed_stride_cells, x, y,
                                   width, height);
}
static Av2hStatus AV2H_KERNEL_NAME(palette_fill_predictor, palette_fill_predictor_u16)(
    const Av2hLeafMode *mode, uint16_t block_x, uint16_t block_y, uint16_t output_width,
    uint16_t output_height, AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor) {
    size_t offset_x;
    size_t offset_y;
    size_t row;
    size_t column;
    Av2hStatus status = palette_map_origin(mode, block_x, block_y, output_width, output_height,
                                           &offset_x, &offset_y);
    if (status != AV2H_OK)
        return status;
    for (row = 0U; row < output_height; ++row) {
        for (column = 0U; column < output_width; ++column) {
            uint8_t color_index = av2h_palette_map_get(
                mode, (offset_y + row) * mode->palette_width + offset_x + column);
            if (color_index >= mode->palette_size)
                return AV2H_INVALID_DATA;
            predictor[row * (size_t)output_width + column] =
                (AV2H_KERNEL_TYPE(uint8_t, uint16_t))mode->palette_colors[color_index];
        }
    }
    return AV2H_OK;
}
