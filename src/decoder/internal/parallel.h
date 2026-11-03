#ifndef AV2H_DECODER_INTERNAL_PARALLEL_H
#define AV2H_DECODER_INTERNAL_PARALLEL_H

#include "runtime/execution.h"

#include <stddef.h>

#define AV2H_PARALLEL_RANGE_COUNT 32U
#define AV2H_PARALLEL_TILE_WORKERS 16U

typedef struct {
    Av2hParallelBody body;
    void *argument;
    size_t count;
    size_t chunks;
    int results[AV2H_PARALLEL_RANGE_COUNT];
} Av2hParallelRanges;

static inline int av2h_parallel_ranges_body(size_t begin, size_t end, unsigned int worker_index,
                                            void *argument) {
    Av2hParallelRanges *work = argument;
    size_t index;
    for (index = begin; index < end; ++index) {
        size_t first = work->count / work->chunks * index +
                       (work->count % work->chunks * index) / work->chunks;
        size_t last = work->count / work->chunks * (index + 1U) +
                      (work->count % work->chunks * (index + 1U)) / work->chunks;
        work->results[index] = work->body(first, last, worker_index, work->argument);
    }
    return 0;
}

/* Collect failures in range order, not the pool's completion order. */
static inline int av2h_parallel_ranges(const Av2hExecution *execution, size_t count,
                                       Av2hParallelBody body, void *argument) {
    Av2hParallelRanges work;
    size_t index;
    if (execution == NULL || execution->parallel_for == NULL || execution->width <= 1U ||
        count <= 1U) {
        return body(0U, count, 0U, argument);
    }
    work.body = body;
    work.argument = argument;
    work.count = count;
    work.chunks = count < AV2H_PARALLEL_RANGE_COUNT ? count : AV2H_PARALLEL_RANGE_COUNT;
    for (index = 0U; index < work.chunks; ++index)
        work.results[index] = -1;
    if (execution->parallel_for(execution->context, work.chunks, 1U, av2h_parallel_ranges_body,
                                &work) != 0) {
        return -1;
    }
    for (index = 0U; index < work.chunks; ++index) {
        if (work.results[index] != 0)
            return work.results[index];
    }
    return 0;
}

#endif
