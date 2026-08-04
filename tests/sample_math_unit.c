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
    if (failures != 0) return 1;
    puts("sample math unit: pass");
    return 0;
}