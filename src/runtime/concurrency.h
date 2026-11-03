#ifndef AV2H_RUNTIME_CONCURRENCY_H
#define AV2H_RUNTIME_CONCURRENCY_H

#include "execution.h"

#include <stdint.h>

#define AV2H_TASK_POOL_MAX_WORKERS 16U
#define AV2H_TASK_POOL_STACK_SIZE (256U * 1024U)

typedef struct Av2hTaskPool Av2hTaskPool;

typedef struct {
    unsigned int index;
    Av2hTaskPool *pool;
    int tid;
    volatile int clear_tid;
    void *stack;
    size_t stack_size;
} Av2hTaskWorker;

struct Av2hTaskPool {
    unsigned int width;
    unsigned int native_width;
    volatile uint8_t stop;
    volatile unsigned int generation;
    volatile unsigned int finished_workers;
    size_t next_index;
    size_t count;
    size_t min_chunk;
    Av2hParallelBody body;
    void *argument;
    int first_error;
    Av2hTaskWorker workers[AV2H_TASK_POOL_MAX_WORKERS];
};

/*
 * A zero worker_count selects the available CPU count. Initialization falls
 * back to a width-one serial pool if native workers cannot be created.
 * Pools are reusable, but a pool may have only one dispatch in flight.
 */
int av2h_task_pool_init(Av2hTaskPool *pool, unsigned int worker_count);
void av2h_task_pool_destroy(Av2hTaskPool *pool);
unsigned int av2h_task_pool_width(const Av2hTaskPool *pool);
void av2h_task_pool_execution(Av2hTaskPool *pool, Av2hExecution *execution);

/*
 * The callback receives non-overlapping [begin, end) ranges. All ranges finish
 * before return, and the first observed nonzero callback result is returned.
 * A null pool is an explicit serial fallback with worker_index zero.
 */
int av2h_parallel_for(Av2hTaskPool *pool, size_t count, size_t min_chunk,
                      Av2hParallelBody body, void *argument);

#endif
