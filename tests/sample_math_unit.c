#include "decoder/internal/sample_math.h"

#include <stdio.h>

static int failures;

#define CHECK(expression)                                      \
    do {                                                       \
        if (!(expression)) {                                   \
            fprintf(stderr, "%s:%d: check failed: %s\n",     \
                    __FILE__, __LINE__, #expression);           \
            ++failures;                                        \
        }                                                      \
    } while (0)

int main(void) {
    const Av2hSampleDomain domain_10bit = { 10U, 2U, 512U, 1023U };
    CHECK(av2h_sample_domain_8bit.bit_depth == 8U);
    CHECK(av2h_sample_domain_8bit.bytes_per_sample == 1U);
    CHECK(av2h_sample_midpoint_8bit() == 128U);
    CHECK(av2h_sample_below_midpoint_8bit() == 127U);
    CHECK(av2h_sample_above_midpoint_8bit() == 129U);
    CHECK(av2h_sample_maximum_8bit() == 255U);
    CHECK(av2h_sample_clamp(-12, -8, 7) == -8);
    CHECK(av2h_sample_clamp(-8, -8, 7) == -8);
    CHECK(av2h_sample_clamp(3, -8, 7) == 3);
    CHECK(av2h_sample_clamp(7, -8, 7) == 7);
    CHECK(av2h_sample_clamp(12, -8, 7) == 7);
    CHECK(av2h_sample_clip(-1) == 0U);
    CHECK(av2h_sample_clip(0) == 0U);
    CHECK(av2h_sample_clip(128) == 128U);
    CHECK(av2h_sample_clip(255) == 255U);
    CHECK(av2h_sample_clip(256) == 255U);
    CHECK(av2h_sample_clip(INT32_MIN) == 0U);
    CHECK(av2h_sample_clip(INT32_MAX) == 255U);
    CHECK(av2h_sample_clip_to_domain(-1, &domain_10bit) == 0U);
    CHECK(av2h_sample_clip_to_domain(512, &domain_10bit) == 512U);
    CHECK(av2h_sample_clip_to_domain(1023, &domain_10bit) == 1023U);
    CHECK(av2h_sample_clip_to_domain(1024, &domain_10bit) == 1023U);
    CHECK(av2h_sample_clip_to_domain(INT32_MIN, &domain_10bit) == 0U);
    CHECK(av2h_sample_clip_to_domain(INT32_MAX, &domain_10bit) == 1023U);
    if (failures != 0) return 1;
    puts("sample math unit: pass");
    return 0;
}