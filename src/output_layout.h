#ifndef AV2H_OUTPUT_LAYOUT_H
#define AV2H_OUTPUT_LAYOUT_H

#include "base.h"

Av2hStatus av2h_output_layout_calculate(uint32_t width, uint32_t height, uint8_t chroma_format_idc,
                                        size_t bytes_per_sample, Av2hPlaneLayout planes[3],
                                        size_t *output_size, Av2hError *error);

#endif