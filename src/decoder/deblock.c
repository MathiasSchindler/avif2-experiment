#include "decoder/filter.h"

#include "decoder/coeff_level.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"

#include <stddef.h>

#define AV2H_DEBLOCK_MAX_SAMPLES 8U

static const uint8_t deblock_weight[AV2H_DEBLOCK_MAX_SAMPLES] = {85U, 51U, 37U, 28U,
                                                                 23U, 20U, 17U, 15U};

static const uint8_t deblock_q_multiplier[AV2H_DEBLOCK_MAX_SAMPLES] = {32U, 25U, 19U, 19U,
                                                                       18U, 18U, 17U, 17U};

static const uint8_t deblock_first_threshold[5] = {45U, 43U, 40U, 35U, 32U};

Av2hStatus av2h_deblock_quantizer_thresholds(const Av2hDeblockCell *cell,
                                             const Av2hSequenceState *sequence,
                                             const Av2hFrameState *frame, unsigned int plane,
                                             unsigned int pass, uint16_t *q_threshold,
                                             uint16_t *side_threshold) {
    int delta = 0;
    int level;
    int q_index;
    int side_index;
    int side_bias;
    unsigned int side_shift;
    uint32_t q_step;
    Av2hQIndex q_step_index;
    unsigned int delta_index = plane == 0U ? pass : plane + 1U;
    if (cell == NULL || sequence == NULL || frame == NULL || q_threshold == NULL ||
        side_threshold == NULL || plane >= AV2H_FRAME_PLANE_COUNT || pass > 1U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (sequence->info.bit_depth_idc != 0U && sequence->info.bit_depth_idc != 1U) {
        return AV2H_UNSUPPORTED;
    }
    if (plane == 1U) {
        delta = frame->u_ac_delta_q + sequence->base_uv_ac_delta_q;
    } else if (plane == 2U) {
        delta = frame->v_ac_delta_q + sequence->base_uv_ac_delta_q;
    }
    if (cell->q_index == 0U && delta <= 0) {
        q_index = 0;
    } else {
        q_index = av2h_sequence_clamp_delta_q_index(sequence, (int)cell->q_index + delta);
    }
    level = q_index + 8 * frame->deblock_delta[delta_index];
    q_step_index =
        level < 1
            ? (Av2hQIndex)(level == 0 ? 0U : 1U)
            : (Av2hQIndex)av2h_sample_clamp(level, 1, (int)av2h_sequence_max_q_index(sequence));
    q_step = sequence->info.bit_depth_idc == 0U ? av2h_coeff_dequant_step_10bit(q_step_index, 0)
                                                : av2h_coeff_dequant_step_8bit(q_step_index, 0);
    *q_threshold = (uint16_t)(((q_step + 4U) >> 3U) >> 6U);
    if (sequence->info.bit_depth_idc == 0U) {
        side_index = av2h_sample_clamp(level - 48, 0, 295);
        side_bias = 4;
        side_shift = 3U;
    } else {
        side_index = av2h_sample_clamp(level, 0, 295);
        side_bias = 16;
        side_shift = 5U;
    }
    *side_threshold =
        (uint16_t)(av2h_sample_clamp(av2h_deblock_side_threshold(side_index) + side_bias, 0,
                                     65535) >>
                   side_shift);
    return AV2H_OK;
}

static int deblock_transform_dimension_valid(uint8_t value) {
    return value == 4U || value == 8U || value == 16U || value == 32U || value == 64U;
}

Av2hStatus av2h_deblock_stage_u16(const Av2hFilterPlanesU16 *reconstructed,
                                  const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                                  Av2hFilterWorkspace *workspace, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    unsigned int plane;
    if (reconstructed == NULL || sequence == NULL || frame == NULL || workspace == NULL ||
        sequence->info.bit_depth_idc != 0U || !filter_dimensions_valid_u16(reconstructed, frame) ||
        !filter_dimensions_valid_u16(&workspace->deblocked_u16, frame) ||
        !av2h_deblock_map_valid_u16(&workspace->deblocked_u16, &workspace->deblock_map)) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (reconstructed->plane[plane].data == workspace->deblocked_u16.plane[plane].data) {
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlaneU16 *source = &reconstructed->plane[plane];
        Av2hFilterPlaneU16 *destination = &workspace->deblocked_u16.plane[plane];
        av2h_plane_copy_u16(destination->data, destination->stride_samples, source->data,
                            source->stride_samples, source->width, source->height);
    }
    return av2h_deblock_filter_frame_u16(&workspace->deblocked_u16, sequence, frame,
                                         &workspace->deblock_map, domain);
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/deblock_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/deblock_kernel.inc"
#include "internal/sample_kernel_end.inc"
