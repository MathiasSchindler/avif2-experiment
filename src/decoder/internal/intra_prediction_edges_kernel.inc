void AV2H_KERNEL_NAME(av2h_directional_filter_edge,
                      av2h_directional_filter_edge_u16)(AV2H_KERNEL_TYPE(uint8_t, uint16_t) * edge,
                                                        size_t count, int strength) {
    static const uint8_t kernels[3U][5U] = {
        {0U, 4U, 8U, 4U, 0U}, {0U, 5U, 6U, 5U, 0U}, {2U, 4U, 4U, 4U, 2U}};
    AV2H_KERNEL_TYPE(uint8_t, uint16_t) source[132U];
    size_t index;
    if (strength == 0 || count > sizeof(source) / sizeof(source[0]))
        return;
    for (index = 0U; index < count; ++index)
        source[index] = edge[index];
    for (index = 1U; index < count; ++index) {
#if AV2H_KERNEL_WIDE
        uint32_t sum = 0U;
#else
        unsigned int sum = 0U;
#endif
        size_t tap;
        for (tap = 0U; tap < 5U; ++tap) {
            int source_index = (int)index - 2 + (int)tap;
            if (source_index < 0)
                source_index = 0;
            if ((size_t)source_index >= count) {
                source_index = (int)count - 1;
            }
            sum += source[source_index] * kernels[strength - 1][tap];
        }
        edge[index] = (AV2H_KERNEL_TYPE(uint8_t, uint16_t))((sum + 8U) >> 4U);
    }
}