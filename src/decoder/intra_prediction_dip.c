#include "internal/intra_prediction.h"

#include "internal/sample_math.h"

#include <stddef.h>

#include "intra_dip_data.inc"

static void intra_dip_matrix_predict(const uint16_t input[11U], unsigned int mode,
                                     uint16_t output[64U], const Av2hSampleDomain *domain) {
    unsigned int position;
    for (position = 0U; position < 64U; ++position) {
        int64_t sum = 0;
        unsigned int feature;
        size_t base = ((size_t)mode * 64U + position) * 11U;
        for (feature = 0U; feature < 11U; ++feature) {
            sum += (int64_t)av2h_intra_dip_weights[base + feature] * input[feature];
        }
        output[position] = av2h_sample_clip_to_domain(av2h_sample_round_pow2(sum, 10U), domain);
    }
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/intra_prediction_dip_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/intra_prediction_dip_kernel.inc"
#include "internal/sample_kernel_end.inc"
