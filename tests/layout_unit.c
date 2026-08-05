#include "decoder/filter.h"
#include "decoder/tile.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void test_64_bit_decoder_layout(void) {
    if (sizeof(void *) != 8U || sizeof(size_t) != 8U) return;

    CHECK(sizeof(Av2hSequenceState) == 66U);
    CHECK(_Alignof(Av2hSequenceState) == 2U);
    CHECK(sizeof(Av2hFrameState) == 3192U);
    CHECK(_Alignof(Av2hFrameState) == 8U);
    CHECK(sizeof(Av2hFirstCoeffLevelCdfs) == 20464U);
    CHECK(_Alignof(Av2hFirstCoeffLevelCdfs) == 2U);
    CHECK(sizeof(Av2hLeafCdfs) == 2498U);
    CHECK(_Alignof(Av2hLeafCdfs) == 2U);
    CHECK(sizeof(Av2hTileDecoder) == 35176U);
    CHECK(_Alignof(Av2hTileDecoder) == 8U);
    CHECK(sizeof(Av2hReconstructionScratch) == 16896U);
    CHECK(_Alignof(Av2hReconstructionScratch) == 4U);
    CHECK(sizeof(Av2hFilterWorkspace) == 368U);
    CHECK(_Alignof(Av2hFilterWorkspace) == 8U);
    CHECK(sizeof(Av2hFilterWorkspaceLayout) == 152U);
    CHECK(_Alignof(Av2hFilterWorkspaceLayout) == 8U);
}

int main(void) {
    test_64_bit_decoder_layout();
    if (failures != 0) return 1;
    puts("layout unit: pass");
    return 0;
}