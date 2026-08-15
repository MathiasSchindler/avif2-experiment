#include "base.h"
#include "decoder/frame_output.h"
#include "decoder/internal/frame_workspace.h"

#include <stdio.h>

#define TEST_WIDTH 512U
#define TEST_HEIGHT 256U
#define TEST_SAMPLE_COUNT (TEST_WIDTH * TEST_HEIGHT)
#define TEST_OUTPUT_BYTES (TEST_SAMPLE_COUNT * 3U)

static int failures;
static uint8_t source_u8[3U][TEST_SAMPLE_COUNT];
static uint16_t source_u16[3U][TEST_SAMPLE_COUNT];
static uint8_t serial_output[TEST_OUTPUT_BYTES * 2U];
static uint8_t parallel_output[TEST_OUTPUT_BYTES * 2U];

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

typedef struct {
    unsigned int width;
    unsigned int calls;
} TestDispatch;

static int dispatch_ranges(void *context, size_t count, size_t min_chunk,
                           Av2hParallelBody body, void *argument) {
    TestDispatch *dispatch = (TestDispatch *)context;
    unsigned int worker;
    size_t begin = 0U;
    (void)min_chunk;
    ++dispatch->calls;
    for (worker = 0U; worker < dispatch->width && begin < count; ++worker) {
        size_t remaining = count - begin;
        size_t workers_left = dispatch->width - worker;
        size_t end = begin + (remaining + workers_left - 1U) / workers_left;
        int result = body(begin, end, worker, argument);
        if (result != 0) return result;
        begin = end;
    }
    return 0;
}

static void initialize_frame(Av2hFrameState *frame,
                             uint16_t width,
                             uint16_t height) {
    av2h_memory_fill(frame, 0U, sizeof(*frame));
    frame->width = width;
    frame->height = height;
    frame->chroma_format_idc = 2U;
}

static void fill_sources(void) {
    unsigned int plane;
    for (plane = 0U; plane < 3U; ++plane) {
        size_t index;
        for (index = 0U; index < TEST_SAMPLE_COUNT; ++index) {
            source_u8[plane][index] =
                (uint8_t)(index * 29U + plane * 53U);
            source_u16[plane][index] =
                (uint16_t)((index * 131U + plane * 997U) & 1023U);
        }
    }
}

static void test_u8_width_parity(void) {
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout layout;
    Av2hFilterPlanes coded;
    size_t capacities[3U];
    TestDispatch serial_dispatch = { 1U, 0U };
    TestDispatch parallel_dispatch = { 4U, 0U };
    Av2hExecution serial = {
        &serial_dispatch, dispatch_ranges, serial_dispatch.width
    };
    Av2hExecution parallel = {
        &parallel_dispatch, dispatch_ranges, parallel_dispatch.width
    };
    size_t output_size;
    unsigned int plane;
    initialize_frame(&frame, TEST_WIDTH, TEST_HEIGHT);
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    for (plane = 0U; plane < 3U; ++plane) {
        CHECK(layout.plane_width[plane] == TEST_WIDTH);
        CHECK(layout.plane_height[plane] == TEST_HEIGHT);
        coded.plane[plane] = (Av2hFilterPlane){
            source_u8[plane], TEST_WIDTH, TEST_WIDTH, TEST_HEIGHT
        };
        capacities[plane] = sizeof(source_u8[plane]);
    }
    CHECK(av2h_planar_output_size(&frame, &output_size) == AV2H_OK);
    CHECK(av2h_copy_visible_planar_with_execution_private(
              &frame, &coded, capacities, serial_output,
              sizeof(serial_output), &serial) == AV2H_OK);
    CHECK(serial_dispatch.calls == 0U);
    CHECK(av2h_copy_visible_planar_with_execution_private(
              &frame, &coded, capacities, parallel_output,
              sizeof(parallel_output), &parallel) == AV2H_OK);
    CHECK(parallel_dispatch.calls == 3U);
    CHECK(av2h_memory_compare(
              serial_output, parallel_output, output_size) == 0);
}

static void test_u16_width_parity(void) {
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout layout;
    Av2hReconstructionPlaneU16 coded[3U];
    size_t capacities[3U];
    TestDispatch serial_dispatch = { 1U, 0U };
    TestDispatch parallel_dispatch = { 4U, 0U };
    Av2hExecution serial = {
        &serial_dispatch, dispatch_ranges, serial_dispatch.width
    };
    Av2hExecution parallel = {
        &parallel_dispatch, dispatch_ranges, parallel_dispatch.width
    };
    size_t output_size;
    unsigned int plane;
    initialize_frame(&frame, TEST_WIDTH, TEST_HEIGHT);
    CHECK(av2h_filter_workspace_query_bytes(
              &frame, sizeof(uint16_t), &layout) == AV2H_OK);
    for (plane = 0U; plane < 3U; ++plane) {
        coded[plane] = (Av2hReconstructionPlaneU16){
            source_u16[plane], TEST_WIDTH, TEST_WIDTH, TEST_HEIGHT,
            TEST_WIDTH, TEST_HEIGHT, 0, 0U, 0U
        };
        capacities[plane] = sizeof(source_u16[plane]);
    }
    CHECK(av2h_planar_output_size_bytes(
              &frame, sizeof(uint16_t), &output_size) == AV2H_OK);
    CHECK(av2h_copy_visible_planar_u16_with_execution_private(
              &frame, coded, capacities, serial_output,
              sizeof(serial_output), &serial) == AV2H_OK);
    CHECK(serial_dispatch.calls == 0U);
    CHECK(av2h_copy_visible_planar_u16_with_execution_private(
              &frame, coded, capacities, parallel_output,
              sizeof(parallel_output), &parallel) == AV2H_OK);
    CHECK(parallel_dispatch.calls == 3U);
    CHECK(av2h_memory_compare(
              serial_output, parallel_output, output_size) == 0);
}

static void test_small_copy_stays_serial(void) {
    Av2hFrameState frame;
    Av2hFilterWorkspaceLayout layout;
    Av2hFilterPlanes coded;
    size_t capacities[3U];
    TestDispatch dispatch = { 4U, 0U };
    Av2hExecution execution = {
        &dispatch, dispatch_ranges, dispatch.width
    };
    unsigned int plane;
    initialize_frame(&frame, 64U, 64U);
    CHECK(av2h_filter_workspace_query(&frame, &layout) == AV2H_OK);
    for (plane = 0U; plane < 3U; ++plane) {
        coded.plane[plane] = (Av2hFilterPlane){
            source_u8[plane], layout.plane_width[plane],
            layout.plane_width[plane], layout.plane_height[plane]
        };
        capacities[plane] = sizeof(source_u8[plane]);
    }
    CHECK(av2h_copy_visible_planar_with_execution_private(
              &frame, &coded, capacities, parallel_output,
              sizeof(parallel_output), &execution) == AV2H_OK);
    CHECK(dispatch.calls == 0U);
}

int main(void) {
    fill_sources();
    test_u8_width_parity();
    test_u16_width_parity();
    test_small_copy_stays_serial();
    if (failures != 0) {
        fprintf(stderr, "%d parallel output checks failed\n", failures);
        return 1;
    }
    return 0;
}
