#include "decoder/filter.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/tile.h"
#include "decoder/prediction.h"

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

    CHECK(sizeof(Av2hPixelFormat) == 6U);
    CHECK(_Alignof(Av2hPixelFormat) == 1U);
    CHECK(sizeof(Av2hPlaneLayout) == 32U);
    CHECK(_Alignof(Av2hPlaneLayout) == 8U);
    CHECK(sizeof(Av2hHeifImage) == 80U);
    CHECK(_Alignof(Av2hHeifImage) == 8U);
    CHECK(offsetof(Av2hHeifImage, pixel_format) == 66U);
    CHECK(offsetof(Av2hHeifImage, property_count) == 72U);
    CHECK(sizeof(Av2hInfo) == 248U);
    CHECK(_Alignof(Av2hInfo) == 8U);
    CHECK(sizeof(Av2hError) == 64U);
    CHECK(_Alignof(Av2hError) == 8U);
    CHECK(sizeof(Av2hSequenceState) == 390U);
    CHECK(_Alignof(Av2hSequenceState) == 2U);
    CHECK(sizeof(Av2hFrameState) == 11088U);
    CHECK(_Alignof(Av2hFrameState) == 8U);
    CHECK(sizeof(Av2hQIndex) == 2U);
    CHECK(sizeof(Av2hDequantizedCoefficient) == 4U);
    CHECK(sizeof(Av2hDequantProduct) == 8U);
    CHECK(sizeof(Av2hTransformIntermediate) == 4U);
    CHECK(sizeof(Av2hTransformSum) == 8U);
    CHECK(sizeof(Av2hPredictionSum) == 8U);
    CHECK(sizeof(Av2hReconstructionSum) == 8U);
    CHECK(sizeof(Av2hCoeffPrefix) == 56U);
    CHECK(_Alignof(Av2hCoeffPrefix) == 8U);
    CHECK(sizeof(Av2hCoeffLevelCdfs) == 25480U);
    CHECK(_Alignof(Av2hCoeffLevelCdfs) == 2U);
    CHECK(sizeof(Av2hLeafCdfs) == 2914U);
    CHECK(_Alignof(Av2hLeafCdfs) == 2U);
    CHECK(sizeof(Av2hTileDecoder) == 70088U);
    CHECK(_Alignof(Av2hTileDecoder) == 8U);
    CHECK(sizeof(Av2hReconstructionScratch) == 29188U);
    CHECK(_Alignof(Av2hReconstructionScratch) == 4U);
    CHECK(sizeof(Av2hReconstructionScratchU16) == 46084U);
    CHECK(_Alignof(Av2hReconstructionScratchU16) == 4U);
    CHECK(sizeof(Av2hReconstructionPlane) == 48U);
    CHECK(_Alignof(Av2hReconstructionPlane) == 8U);
    CHECK(sizeof(Av2hReconstructionPlaneU16) == 48U);
    CHECK(_Alignof(Av2hReconstructionPlaneU16) == 8U);
    CHECK(sizeof(Av2hFilterPlane) == 24U);
    CHECK(_Alignof(Av2hFilterPlane) == 8U);
    CHECK(sizeof(Av2hFilterPlaneU16) == 24U);
    CHECK(_Alignof(Av2hFilterPlaneU16) == 8U);
    CHECK(sizeof(Av2hFilterPlanesU16) == 72U);
    CHECK(_Alignof(Av2hFilterPlanesU16) == 8U);
    CHECK(sizeof(Av2hDeblockCell) == 14U);
    CHECK(_Alignof(Av2hDeblockCell) == 2U);
    CHECK(sizeof(Av2hCcsoControlMap) == 64U);
    CHECK(_Alignof(Av2hCcsoControlMap) == 8U);
    CHECK(sizeof(Av2hGdfControlMap) == 40U);
    CHECK(_Alignof(Av2hGdfControlMap) == 8U);
    CHECK(sizeof(Av2hDeblockMap) == 96U);
    CHECK(_Alignof(Av2hDeblockMap) == 8U);
    CHECK(sizeof(Av2hFilterWorkspace) == 672U);
    CHECK(_Alignof(Av2hFilterWorkspace) == 8U);
    CHECK(sizeof(Av2hFilterWorkspaceLayout) == 168U);
    CHECK(_Alignof(Av2hFilterWorkspaceLayout) == 8U);
    CHECK(sizeof(Av2hFrameDecodeWorkspaceLayout) == 16U);
    CHECK(_Alignof(Av2hFrameDecodeWorkspaceLayout) == 8U);
}

int main(void) {
    test_64_bit_decoder_layout();
    if (failures != 0) return 1;
    puts("layout unit: pass");
    return 0;
}