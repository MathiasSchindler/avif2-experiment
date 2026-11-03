#include "concurrency.h"

#include "concurrency_platform.h"

#include <stddef.h>

#define AV2H_TASK_CHUNKS_PER_WORKER 8U

static void task_pool_clear(Av2hTaskPool *pool) {
    unsigned char *bytes = (unsigned char *)pool;
    size_t index;
    for (index = 0U; index < sizeof(*pool); ++index)
        bytes[index] = 0U;
}

static size_t effective_min_chunk(const Av2hTaskPool *pool, size_t count, size_t requested) {
    size_t target_chunks;
    size_t automatic;
    if (requested == 0U)
        requested = 1U;
    target_chunks = (size_t)pool->native_width * (size_t)AV2H_TASK_CHUNKS_PER_WORKER;
    if (target_chunks == 0U || count <= target_chunks)
        return requested;
    automatic = count / target_chunks;
    if (count % target_chunks != 0U)
        ++automatic;
    return automatic > requested ? automatic : requested;
}

static void record_error(Av2hTaskPool *pool, int error) {
    int expected = 0;
    if (error != 0) {
        (void)__atomic_compare_exchange_n(&pool->first_error, &expected, error, 0, __ATOMIC_RELEASE,
                                          __ATOMIC_RELAXED);
    }
}

static void run_work(Av2hTaskPool *pool, unsigned int worker_index) {
    for (;;) {
        size_t begin;
        size_t end;
        int result;
        for (;;) {
            begin = __atomic_load_n(&pool->next_index, __ATOMIC_ACQUIRE);
            if (begin >= pool->count)
                return;
            end = begin + pool->min_chunk;
            if (end < begin || end > pool->count)
                end = pool->count;
            if (__atomic_compare_exchange_n(&pool->next_index, &begin, end, 0, __ATOMIC_ACQ_REL,
                                            __ATOMIC_RELAXED)) {
                break;
            }
        }
        result = pool->body(begin, end, worker_index, pool->argument);
        record_error(pool, result);
    }
}

static int task_worker_main(void *argument) {
    Av2hTaskWorker *worker = (Av2hTaskWorker *)argument;
    Av2hTaskPool *pool = worker == NULL ? NULL : worker->pool;
    unsigned int observed_generation = 0U;
    if (pool == NULL)
        return -1;
    for (;;) {
        unsigned int generation = __atomic_load_n(&pool->generation, __ATOMIC_ACQUIRE);
        while (generation == observed_generation &&
               __atomic_load_n(&pool->stop, __ATOMIC_ACQUIRE) == 0U) {
            av2h_platform_wait_word(&pool->generation, observed_generation);
            generation = __atomic_load_n(&pool->generation, __ATOMIC_ACQUIRE);
        }
        if (__atomic_load_n(&pool->stop, __ATOMIC_ACQUIRE) != 0U)
            break;
        observed_generation = generation;
        run_work(pool, worker->index);
        if (__atomic_fetch_add(&pool->finished_workers, 1U, __ATOMIC_ACQ_REL) + 1U >=
            pool->native_width) {
            av2h_platform_wake_word_all(&pool->finished_workers);
        }
    }
    return 0;
}

static void stop_workers(Av2hTaskPool *pool) {
    unsigned int index;
    if (pool->native_width <= 1U)
        return;
    __atomic_store_n(&pool->stop, 1U, __ATOMIC_RELEASE);
    (void)__atomic_fetch_add(&pool->generation, 1U, __ATOMIC_ACQ_REL);
    av2h_platform_wake_word_all(&pool->generation);
    for (index = 1U; index < pool->native_width; ++index) {
        (void)av2h_platform_worker_join(&pool->workers[index]);
    }
}

int av2h_task_pool_init(Av2hTaskPool *pool, unsigned int worker_count) {
    unsigned int index;
    if (pool == NULL)
        return -1;
    task_pool_clear(pool);
    if (worker_count == 0U)
        worker_count = av2h_platform_worker_count();
    if (worker_count == 0U)
        worker_count = 1U;
    if (worker_count > AV2H_TASK_POOL_MAX_WORKERS) {
        worker_count = AV2H_TASK_POOL_MAX_WORKERS;
    }
    if (!av2h_platform_workers_supported())
        worker_count = 1U;
    pool->width = worker_count;
    pool->native_width = 1U;
    pool->workers[0].index = 0U;
    pool->workers[0].pool = pool;
    for (index = 1U; index < worker_count; ++index) {
        Av2hTaskWorker *worker = &pool->workers[index];
        worker->index = index;
        worker->pool = pool;
        if (av2h_platform_worker_start(worker, task_worker_main, worker) != 0) {
            stop_workers(pool);
            task_pool_clear(pool);
            pool->width = 1U;
            pool->native_width = 1U;
            pool->workers[0].index = 0U;
            pool->workers[0].pool = pool;
            return 0;
        }
        pool->native_width = index + 1U;
    }
    return 0;
}

void av2h_task_pool_destroy(Av2hTaskPool *pool) {
    if (pool == NULL || pool->width == 0U)
        return;
    stop_workers(pool);
    task_pool_clear(pool);
}

unsigned int av2h_task_pool_width(const Av2hTaskPool *pool) {
    return pool == NULL || pool->width == 0U ? 1U : pool->width;
}

static int task_pool_dispatch(void *context, size_t count, size_t min_chunk, Av2hParallelBody body,
                              void *argument) {
    return av2h_parallel_for((Av2hTaskPool *)context, count, min_chunk, body, argument);
}

void av2h_task_pool_execution(Av2hTaskPool *pool, Av2hExecution *execution) {
    if (execution == NULL)
        return;
    execution->context = pool;
    execution->parallel_for = task_pool_dispatch;
    execution->width = av2h_task_pool_width(pool);
}

static int run_serial(size_t count, size_t min_chunk, Av2hParallelBody body, void *argument) {
    size_t begin = 0U;
    int first_error = 0;
    if (min_chunk == 0U)
        min_chunk = count == 0U ? 1U : count;
    while (begin < count) {
        size_t end = begin + min_chunk;
        int result;
        if (end < begin || end > count)
            end = count;
        result = body(begin, end, 0U, argument);
        if (first_error == 0 && result != 0)
            first_error = result;
        begin = end;
    }
    return first_error;
}

int av2h_parallel_for(Av2hTaskPool *pool, size_t count, size_t min_chunk, Av2hParallelBody body,
                      void *argument) {
    unsigned int finished;
    if (body == NULL)
        return -1;
    if (count == 0U)
        return 0;
    if (pool == NULL || pool->native_width <= 1U) {
        return run_serial(count, min_chunk, body, argument);
    }
    min_chunk = effective_min_chunk(pool, count, min_chunk);
    if (count <= min_chunk)
        return run_serial(count, min_chunk, body, argument);
    pool->count = count;
    pool->min_chunk = min_chunk;
    pool->body = body;
    pool->argument = argument;
    pool->first_error = 0;
    pool->next_index = 0U;
    pool->finished_workers = 0U;
    (void)__atomic_fetch_add(&pool->generation, 1U, __ATOMIC_ACQ_REL);
    av2h_platform_wake_word_count(&pool->generation, pool->native_width - 1U);
    run_work(pool, 0U);
    if (__atomic_fetch_add(&pool->finished_workers, 1U, __ATOMIC_ACQ_REL) + 1U >=
        pool->native_width) {
        av2h_platform_wake_word_all(&pool->finished_workers);
    }
    for (;;) {
        finished = __atomic_load_n(&pool->finished_workers, __ATOMIC_ACQUIRE);
        if (finished >= pool->native_width)
            break;
        av2h_platform_wait_word(&pool->finished_workers, finished);
    }
    return __atomic_load_n(&pool->first_error, __ATOMIC_ACQUIRE);
}
