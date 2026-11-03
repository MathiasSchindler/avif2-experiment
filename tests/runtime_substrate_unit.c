#include "runtime/concurrency.h"

#include <stddef.h>
#include <stdio.h>

_Static_assert(sizeof(((Av2hTaskPool *)NULL)->stop) == sizeof(uint8_t),
               "stop flag is not byte-sized");
_Static_assert(offsetof(Av2hTaskPool, generation) ==
                   offsetof(Av2hTaskPool, stop) + sizeof(unsigned int),
               "task pool synchronization-word offset changed");
_Static_assert(sizeof(((Av2hTaskPool *)NULL)->generation) == sizeof(unsigned int),
               "platform wait-word width changed");

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

typedef struct {
    unsigned int *hits;
    unsigned int width;
    volatile unsigned int worker_mask;
    volatile unsigned int calls;
    volatile unsigned int invalid_calls;
    unsigned int wait_for_native;
    size_t fail_index;
} Work;

static int cover_range(size_t begin, size_t end, unsigned int worker_index,
                       void *argument) {
    Work *work = (Work *)argument;
    size_t index;
    if (begin >= end || worker_index >= work->width) {
        (void)__atomic_fetch_add(&work->invalid_calls, 1U, __ATOMIC_RELAXED);
        return -99;
    }
    (void)__atomic_fetch_or(&work->worker_mask, 1U << worker_index,
                            __ATOMIC_RELAXED);
    if (worker_index == 0U && work->wait_for_native != 0U) {
        while ((__atomic_load_n(&work->worker_mask, __ATOMIC_RELAXED) &
                ~1U) == 0U) {
        }
    }
    (void)__atomic_fetch_add(&work->calls, 1U, __ATOMIC_RELAXED);
    for (index = begin; index < end; ++index) {
        (void)__atomic_fetch_add(&work->hits[index], 1U, __ATOMIC_RELAXED);
    }
    return begin <= work->fail_index && work->fail_index < end ? -17 : 0;
}

static void check_coverage(const unsigned int *hits, size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        CHECK(__atomic_load_n(&hits[index], __ATOMIC_RELAXED) == 1U);
    }
}

static void test_serial_fallback(void) {
    unsigned int hits[17] = { 0U };
    Work work = { hits, 1U, 0U, 0U, 0U, 0U, (size_t)-1 };
    Av2hTaskPool pool;
    Av2hExecution execution;
    CHECK(av2h_parallel_for(0, 17U, 4U, cover_range, &work) == 0);
    CHECK(__atomic_load_n(&work.calls, __ATOMIC_RELAXED) == 5U);
    CHECK(__atomic_load_n(&work.worker_mask, __ATOMIC_RELAXED) == 1U);
    CHECK(__atomic_load_n(&work.invalid_calls, __ATOMIC_RELAXED) == 0U);
    check_coverage(hits, 17U);
    CHECK(av2h_task_pool_init(&pool, 1U) == 0);
    CHECK(av2h_task_pool_width(&pool) == 1U);
    av2h_task_pool_execution(&pool, &execution);
    CHECK(execution.context == &pool);
    CHECK(execution.parallel_for != 0);
    CHECK(execution.width == 1U);
    CHECK(av2h_parallel_for(&pool, 0U, 0U, cover_range, &work) == 0);
    CHECK(av2h_parallel_for(&pool, 1U, 1U, 0, &work) == -1);
    av2h_task_pool_destroy(&pool);
}

static void test_native_pool(void) {
    enum { ITEM_COUNT = 4096 };
    unsigned int hits[ITEM_COUNT] = { 0U };
    Av2hTaskPool pool;
    Av2hExecution execution;
    Work work;
    unsigned int generation;
    CHECK(av2h_task_pool_init(&pool, 4U) == 0);
    if (av2h_task_pool_width(&pool) == 1U) {
        av2h_task_pool_destroy(&pool);
        return;
    }
    CHECK(av2h_task_pool_width(&pool) == 4U);
    av2h_task_pool_execution(&pool, &execution);
    CHECK(execution.width == 4U);
    work.hits = hits;
    work.width = av2h_task_pool_width(&pool);
    work.worker_mask = 0U;
    work.calls = 0U;
    work.invalid_calls = 0U;
    work.wait_for_native = 1U;
    work.fail_index = (size_t)-1;
    for (generation = 0U; generation < 8U; ++generation) {
        size_t index;
        for (index = 0U; index < ITEM_COUNT; ++index) hits[index] = 0U;
        __atomic_store_n(&work.worker_mask, 0U, __ATOMIC_RELAXED);
        __atomic_store_n(&work.calls, 0U, __ATOMIC_RELAXED);
        CHECK(execution.parallel_for(
                  execution.context, ITEM_COUNT, 7U,
                  cover_range, &work) == 0);
        check_coverage(hits, ITEM_COUNT);
        CHECK(__atomic_load_n(&work.calls, __ATOMIC_RELAXED) == 32U);
        CHECK((__atomic_load_n(&work.worker_mask, __ATOMIC_RELAXED) &
               ~1U) != 0U);
        CHECK(av2h_task_pool_width(&pool) == 4U);
    }
    for (generation = 0U; generation < ITEM_COUNT; ++generation) {
        hits[generation] = 0U;
    }
    __atomic_store_n(&work.worker_mask, 0U, __ATOMIC_RELAXED);
    __atomic_store_n(&work.calls, 0U, __ATOMIC_RELAXED);
    work.fail_index = 2000U;
    CHECK(av2h_parallel_for(&pool, ITEM_COUNT, 3U,
                            cover_range, &work) == -17);
    check_coverage(hits, ITEM_COUNT);
    CHECK(__atomic_load_n(&work.calls, __ATOMIC_RELAXED) == 32U);
    CHECK((__atomic_load_n(&work.worker_mask, __ATOMIC_RELAXED) &
           ~1U) != 0U);
    CHECK(__atomic_load_n(&work.invalid_calls, __ATOMIC_RELAXED) == 0U);
    av2h_task_pool_destroy(&pool);
}

typedef struct {
    volatile unsigned int calls;
    volatile unsigned int saw_tail;
} BoundaryWork;

static int boundary_range(size_t begin, size_t end, unsigned int worker_index,
                          void *argument) {
    BoundaryWork *work = (BoundaryWork *)argument;
    (void)worker_index;
    if (begin == (size_t)-2 && end == (size_t)-1) {
        __atomic_store_n(&work->saw_tail, 1U, __ATOMIC_RELAXED);
    }
    (void)__atomic_fetch_add(&work->calls, 1U, __ATOMIC_RELAXED);
    return 0;
}

static void test_size_boundary(void) {
    Av2hTaskPool pool;
    BoundaryWork work = { 0U, 0U };
    CHECK(av2h_task_pool_init(&pool, 4U) == 0);
    CHECK(av2h_parallel_for(&pool, (size_t)-1, (size_t)-2,
                            boundary_range, &work) == 0);
    CHECK(__atomic_load_n(&work.calls, __ATOMIC_RELAXED) == 2U);
    CHECK(__atomic_load_n(&work.saw_tail, __ATOMIC_RELAXED) == 1U);
    av2h_task_pool_destroy(&pool);
}

int main(void) {
    test_serial_fallback();
    test_native_pool();
    test_size_boundary();
    if (failures != 0) {
        fprintf(stderr, "%d runtime substrate checks failed\n", failures);
        return 1;
    }
    return 0;
}
