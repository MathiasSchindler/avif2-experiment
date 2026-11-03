#include "decoder/internal/frame_workspace.h"
#include "decoder/internal/parallel.h"
#include "shared/cli_common.h"
#include "parallel_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int missing_dispatch(void *context, size_t count, size_t min_chunk, Av2hParallelBody body,
                            void *argument);
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static Av2hOwnedMemory read_input(const char *path) {
    Av2hOwnedMemory input = {0};
    FILE *file = fopen(path, "rb");
    long size;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || (size = ftell(file)) <= 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "parallel test: cannot read %s\n", path);
        exit(2);
    }
    input.size = (size_t)size;
    input.memory = malloc(input.size);
    if (input.memory == NULL || fread(input.memory, 1U, input.size, file) != input.size ||
        fclose(file) != 0) {
        fprintf(stderr, "parallel test: failed reading %s\n", path);
        exit(2);
    }
    return input;
}

static int allocate_image(void *context, size_t size, size_t alignment, Av2hOwnedMemory *owned) {
    (void)context;
    if (alignment > _Alignof(max_align_t))
        return 0;
    owned->memory = malloc(size == 0U ? 1U : size);
    owned->size = size;
    return owned->memory != NULL;
}

static void release_image(void *context, Av2hOwnedMemory *owned) {
    (void)context;
    free(owned->memory);
    owned->memory = NULL;
    owned->size = 0U;
}

static int results_equal(Av2hFrameDecodeResult left, Av2hFrameDecodeResult right) {
    if ((left.diagnostics == NULL) != (right.diagnostics == NULL))
        return 0;
    left.diagnostics = NULL;
    right.diagnostics = NULL;
    return memcmp(&left, &right, sizeof(left)) == 0;
}

static void compare_image(const char *path, Av2hTestPool *pool) {
    Av2hOwnedMemory input = read_input(path);
    Av2hCliAllocator allocator = {NULL, allocate_image, release_image};
    Av2hExecution execution = av2h_test_pool_execution(pool);
    Av2hCliDecodeOptions options[2] = {{NULL, 0U, 0U}, {&execution, 0U, 0U}};
    Av2hCliImage images[2];
    Av2hStatus status[2];
    unsigned int copy;
    for (copy = 0U; copy < 2U; ++copy)
        status[copy] = av2h_cli_decode((Av2hSpan){input.memory, input.size}, &allocator,
                                       &options[copy], &images[copy]);
    if (status[0] != status[1] || !results_equal(images[0].result, images[1].result) ||
        memcmp(&images[0].error, &images[1].error, sizeof(images[0].error)) != 0) {
        fprintf(stderr, "parallel result mismatch: %s (%d/%d, stage %u/%u)\n", path, status[0],
                status[1], images[0].result.stage, images[1].result.stage);
        ++failures;
    }
    if (status[0] == AV2H_OK && status[1] == AV2H_OK) {
        CHECK(images[0].output.size == images[1].output.size);
        CHECK(memcmp(images[0].output.memory, images[1].output.memory, images[0].output.size) == 0);
    }
    for (copy = 0U; copy < 2U; ++copy)
        av2h_cli_image_release(&allocator, &images[copy]);
    free(input.memory);
}

static void test_tile_reuse(const char *path, Av2hTestPool *pool) {
    Av2hOwnedMemory input = read_input(path);
    Av2hObu entries[32];
    Av2hObuLedger ledger = {entries, 32U, 0U};
    const Av2hObu *sequence_obu = NULL;
    const Av2hObu *frame_obu = NULL;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hFrameSyntaxStorage syntax = {0};
    Av2hTileGroup original;
    Av2hError error;
    Av2hFrameDecodeWorkspaceLayout layouts[2];
    Av2hFrameDecodeWorkspace workspace[2];
    Av2hFilterWorkspace filter[2];
    Av2hFilterWorkspaceStorage storage[2];
    Av2hExecution execution = av2h_test_pool_execution(pool);
    uint8_t *memory[2];
    uint8_t *output[2];
    uint8_t *corrupt = malloc(input.size);
    size_t output_size;
    size_t bytes;
    size_t index;
    unsigned int copy;
    unsigned int variant;
    CHECK(corrupt != NULL);
    if (corrupt == NULL)
        exit(2);
    CHECK(av2h_obu_parse((Av2hSpan){input.memory, input.size}, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER && sequence_obu == NULL)
            sequence_obu = &entries[index];
        if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY && frame_obu == NULL)
            frame_obu = &entries[index];
    }
    CHECK(sequence_obu != NULL && frame_obu != NULL);
    if (sequence_obu == NULL || frame_obu == NULL)
        exit(2);
    CHECK(av2h_sequence_state_parse((Av2hSpan){input.memory, input.size}, sequence_obu, &sequence,
                                    &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){(uint8_t *)input.memory + frame_obu->payload_offset,
                         frame_obu->payload_size},
              &sequence, &frame, &syntax, NULL, NULL) == AV2H_OK);
    CHECK(av2h_tile_groups_ledger_parse((Av2hSpan){input.memory, input.size}, &ledger, &frame,
                                        &original) == AV2H_OK);
    bytes = sequence.info.bit_depth_idc == 1U ? 1U : 2U;
    CHECK(av2h_planar_output_size(&frame, bytes, &output_size) == AV2H_OK);
    for (copy = 0U; copy < 2U; ++copy) {
        CHECK(av2h_frame_decode_workspace_query(&sequence, &frame, bytes,
                                                copy == 0U ? NULL : &execution,
                                                &layouts[copy]) == AV2H_OK);
        memory[copy] = malloc(layouts[copy].size_bytes + 16U);
        output[copy] = malloc(output_size);
        CHECK(memory[copy] != NULL && output[copy] != NULL);
        if (memory[copy] == NULL || output[copy] == NULL)
            exit(2);
        memset(memory[copy], 0xa5, layouts[copy].size_bytes + 16U);
        CHECK(av2h_frame_decode_workspace_init(
                  &sequence, &frame, bytes, &layouts[copy], memory[copy],
                  layouts[copy].size_bytes - 1U, &workspace[copy], &filter[copy], &storage[copy],
                  copy == 0U ? NULL : &execution) == AV2H_OUT_OF_MEMORY);
        if (copy == 1U)
            CHECK(av2h_frame_decode_workspace_init(&sequence, &frame, bytes, &layouts[0],
                                                   memory[copy], layouts[copy].size_bytes,
                                                   &workspace[copy], &filter[copy], &storage[copy],
                                                   &execution) == AV2H_INVALID_ARGUMENT);
        CHECK(av2h_frame_decode_workspace_init(&sequence, &frame, bytes, &layouts[copy],
                                               memory[copy], layouts[copy].size_bytes,
                                               &workspace[copy], &filter[copy], &storage[copy],
                                               copy == 0U ? NULL : &execution) == AV2H_OK);
    }
    CHECK(original.tile_count > 1U);
    CHECK(layouts[1].size_bytes > layouts[0].size_bytes);
    {
        Av2hFrameDecodeWorkspaceLayout layout;
        Av2hExecution single = execution;
        Av2hFrameDecodeScratch *scratch = workspace[1].tile_workers[1].workspace.decode_scratch;
        unsigned int count = workspace[1].tile_worker_count;
        single.width = 1U;
        CHECK(av2h_frame_decode_workspace_query(&sequence, &frame, bytes, &single, &layout) ==
              AV2H_OK);
        CHECK(layout.size_bytes == layouts[0].size_bytes);
        single.width = execution.width;
        single.parallel_for = NULL;
        CHECK(av2h_frame_decode_workspace_query(&sequence, &frame, bytes, &single, &layout) ==
              AV2H_OK);
        CHECK(layout.size_bytes == layouts[0].size_bytes);
        CHECK(av2h_decode_frame_group_planar_private(&original, &sequence, &frame, &workspace[1],
                                                     (uint8_t *)scratch, output_size, NULL,
                                                     &execution) == AV2H_INVALID_ARGUMENT);
        workspace[1].tile_workers[1].workspace.decode_scratch = NULL;
        CHECK(av2h_decode_frame_group_planar_private(&original, &sequence, &frame, &workspace[1],
                                                     output[1], output_size, NULL,
                                                     &execution) == AV2H_INVALID_ARGUMENT);
        workspace[1].tile_workers[1].workspace.decode_scratch = scratch;
        workspace[1].tile_worker_count = AV2H_PARALLEL_TILE_WORKERS + 1U;
        CHECK(av2h_decode_frame_group_planar_private(&original, &sequence, &frame, &workspace[1],
                                                     output[1], output_size, NULL,
                                                     &execution) == AV2H_INVALID_ARGUMENT);
        workspace[1].tile_worker_count = count;
        {
            int dispatch_status;
            Av2hExecution broken = {&dispatch_status, missing_dispatch, execution.width};
            Av2hFrameDecodeResult rejected;
            for (dispatch_status = 0; dispatch_status <= 1; ++dispatch_status) {
                CHECK(av2h_decode_frame_group_planar_private(
                          &original, &sequence, &frame, &workspace[1], output[1], output_size,
                          &rejected, &broken) == AV2H_INVALID_DATA);
                CHECK(rejected.stage == AV2H_FRAME_DECODE_STAGE_INITIALIZE);
            }
        }
        {
            Av2hFrameState many_tiles = frame;
            Av2hFrameDecodeWorkspaceLayout capped[2];
            many_tiles.tile_column_count = 17U;
            many_tiles.tile_row_count = 1U;
            single = execution;
            single.width = AV2H_PARALLEL_TILE_WORKERS;
            CHECK(av2h_frame_decode_workspace_query(&sequence, &many_tiles, bytes, &single,
                                                    &capped[0]) == AV2H_OK);
            single.width *= 2U;
            CHECK(av2h_frame_decode_workspace_query(&sequence, &many_tiles, bytes, &single,
                                                    &capped[1]) == AV2H_OK);
            CHECK(capped[0].size_bytes == capped[1].size_bytes);
        }
    }
    for (variant = 0U; variant < 25U; ++variant) {
        Av2hTileGroup group = original;
        Av2hFrameDecodeResult results[2];
        Av2hStatus status[2];
        if (variant != 0U && variant % 6U != 0U) {
            size_t tile = (variant - 1U) % group.tile_count;
            size_t kind = (variant - 1U) / group.tile_count;
            if (kind % 4U == 0U)
                group.tiles[tile].size = 0U;
            else if (kind % 4U == 1U)
                group.tiles[tile].size = 1U;
            else if (kind % 4U == 2U)
                group.tiles[tile].size /= 2U;
            else {
                memcpy(corrupt, group.tiles[tile].data, group.tiles[tile].size);
                corrupt[0] ^= 0xffU;
                group.tiles[tile].data = corrupt;
            }
            if (variant > 12U)
                group.tiles[group.tile_count - 1U].size = 1U;
        }
        for (copy = 0U; copy < 2U; ++copy) {
            memset(&results[copy], 0xa5, sizeof(results[copy]));
            memset(output[copy], 0xa5, output_size);
            status[copy] = av2h_decode_frame_group_planar_private(
                &group, &sequence, &frame, &workspace[copy], output[copy], output_size,
                &results[copy], copy == 0U ? NULL : &execution);
            for (index = 0U; index < 16U; ++index)
                CHECK(memory[copy][layouts[copy].size_bytes + index] == 0xa5U);
        }
        if (variant == 0U) {
            if (workspace[1].tile_workers[1].initialized == 0U)
                fprintf(stderr, "tile dispatch missing: %s, gdf=%u restoration=%u/%u/%u\n", path,
                        frame.gdf_mode, frame.restoration_type[0], frame.restoration_type[1],
                        frame.restoration_type[2]);
            CHECK(workspace[1].tile_workers[1].initialized != 0U);
        }
        if (status[0] != status[1] || !results_equal(results[0], results[1])) {
            fprintf(stderr, "tile reuse mismatch: %s variant %u status %d/%d stage %u/%u\n", path,
                    variant, status[0], status[1], results[0].stage, results[1].stage);
            ++failures;
        }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        CHECK(memcmp(workspace[0].diagnostics, workspace[1].diagnostics,
                     sizeof(*workspace[0].diagnostics)) == 0);
#endif
        CHECK(memcmp(output[0], output[1], output_size) == 0);
        for (unsigned int plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            size_t size = workspace[0].coded_data_size_bytes[plane];
            const void *left = bytes == 1U ? (const void *)workspace[0].coded[plane].data
                                           : (const void *)workspace[0].coded_u16[plane].data;
            const void *right = bytes == 1U ? (const void *)workspace[1].coded[plane].data
                                            : (const void *)workspace[1].coded_u16[plane].data;
            const uint8_t *ready_left = bytes == 1U ? workspace[0].coded[plane].reconstructed
                                                    : workspace[0].coded_u16[plane].reconstructed;
            const uint8_t *ready_right = bytes == 1U ? workspace[1].coded[plane].reconstructed
                                                     : workspace[1].coded_u16[plane].reconstructed;
            size_t ready_count = bytes == 1U
                                     ? workspace[0].coded[plane].reconstructed_count_cells
                                     : workspace[0].coded_u16[plane].reconstructed_count_cells;
            size_t map_size = filter[0].deblock_map.stride_cells[plane] *
                              filter[0].deblock_map.rows[plane] * sizeof(Av2hDeblockCell);
            if (size == 0U)
                continue;
            CHECK(memcmp(left, right, size) == 0);
            CHECK(memcmp(ready_left, ready_right, ready_count) == 0);
            CHECK(memcmp(filter[0].deblock_map.plane[plane], filter[1].deblock_map.plane[plane],
                         map_size) == 0);
        }
    }
    CHECK((pool->workers_used & ~1U) != 0U);
    {
        Av2hFrameDecodeResult results[2];
        for (copy = 0U; copy < 2U; ++copy) {
            workspace[copy].diagnostics = NULL;
            filter[copy].diagnostics = NULL;
            CHECK(av2h_decode_frame_group_planar_private(
                      &original, &sequence, &frame, &workspace[copy], output[copy], output_size,
                      &results[copy], copy == 0U ? NULL : &execution) == AV2H_OK);
            CHECK(results[copy].diagnostics == NULL);
        }
        CHECK(results_equal(results[0], results[1]));
        CHECK(memcmp(output[0], output[1], output_size) == 0);
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    {
        Av2hDecodeDiagnostics diagnostics[2] = {0};
        Av2hFrameDecodeResult results[2];
        Av2hPartitionDecision trace[2] = {0};
        unsigned int traced;
        for (traced = 0U; traced < 2U; ++traced) {
            for (copy = 0U; copy < 2U; ++copy) {
                workspace[copy].diagnostics = &diagnostics[copy];
                filter[copy].diagnostics = &diagnostics[copy];
                diagnostics[copy].partition_trace_capacity = 1U;
                diagnostics[copy].partition_trace = traced ? &trace[copy] : NULL;
                for (index = 0U; index < workspace[copy].tile_worker_count; ++index)
                    if (workspace[copy].tile_workers != NULL)
                        workspace[copy].tile_workers[index].initialized = 0U;
                CHECK(av2h_decode_frame_group_planar_private(
                          &original, &sequence, &frame, &workspace[copy], output[copy], output_size,
                          &results[copy], copy == 0U ? NULL : &execution) == AV2H_OK);
                CHECK(results[copy].diagnostics == &diagnostics[copy]);
                CHECK(diagnostics[copy].partition_trace_capacity == 1U);
            }
            CHECK(results_equal(results[0], results[1]));
            CHECK(memcmp(output[0], output[1], output_size) == 0);
            CHECK((workspace[1].tile_workers[1].initialized == 0U) == (traced != 0U));
            CHECK(diagnostics[0].partition_trace_count == diagnostics[1].partition_trace_count);
            CHECK(memcmp(&trace[0], &trace[1], sizeof(trace[0])) == 0);
        }
    }
#endif
    for (copy = 0U; copy < 2U; ++copy) {
        free(memory[copy]);
        free(output[copy]);
    }
    free(corrupt);
    free(input.memory);
}

static int ordered_failure(size_t begin, size_t end, unsigned int worker, void *argument) {
    (void)end;
    (void)worker;
    (void)argument;
    return begin == 0U ? AV2H_TRUNCATED : AV2H_UNSUPPORTED;
}

static int missing_dispatch(void *context, size_t count, size_t min_chunk, Av2hParallelBody body,
                            void *argument) {
    (void)count;
    (void)min_chunk;
    (void)body;
    (void)argument;
    return *(const int *)context;
}

int main(int argc, char **argv) {
    static const char *paths[] = {
        "tests/fixtures/spatial-tile-filtered-disabled-2x2.obu",
        "tests/fixtures/spatial-tile-filtered-nonuniform.obu",
        "tests/fixtures/spatial-tile-uniform-2x1.obu",
        "tests/fixtures/10bit/avm-reference/tiny-precision-16x16-yuv444p10le-qp128.obu"};
    unsigned int width;
    {
        int status = 0;
        Av2hExecution broken = {&status, missing_dispatch, 4U};
        CHECK(av2h_parallel_ranges(&broken, 32U, ordered_failure, NULL) == -1);
        status = 1;
        CHECK(av2h_parallel_ranges(&broken, 32U, ordered_failure, NULL) == -1);
    }
    for (width = 2U; width <= 8U; width *= 2U) {
        Av2hTestPool pool;
        Av2hExecution execution;
        size_t index;
        av2h_test_pool_init(&pool, width);
        execution = av2h_test_pool_execution(&pool);
        CHECK(av2h_parallel_ranges(&execution, 32U, ordered_failure, NULL) == AV2H_TRUNCATED);
        if (argc > 1) {
            int argument;
            for (argument = 1; argument < argc; ++argument)
                compare_image(argv[argument], &pool);
        } else {
            for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index)
                compare_image(paths[index], &pool);
            test_tile_reuse(paths[0], &pool);
            test_tile_reuse(paths[1], &pool);
        }
        printf("parallel width %u: %u dispatches, workers 0x%x, native=%d\n", width, pool.calls,
               pool.workers_used, AV2H_TEST_NATIVE_THREADS);
        av2h_test_pool_destroy(&pool);
    }
    if (failures != 0) {
        fprintf(stderr, "%d parallel checks failed\n", failures);
        return 1;
    }
    puts("decode parallel unit: pass");
    return 0;
}
