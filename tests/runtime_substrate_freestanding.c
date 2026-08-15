#include "runtime/concurrency.h"

typedef struct {
    volatile unsigned int total;
    volatile unsigned int worker_mask;
    unsigned int width;
} FreestandingWork;

static int add_range(size_t begin, size_t end, unsigned int worker_index,
                     void *argument) {
    FreestandingWork *work = (FreestandingWork *)argument;
    size_t index;
    if (begin >= end || worker_index >= work->width) return 1;
    (void)__atomic_fetch_or(&work->worker_mask, 1U << worker_index,
                            __ATOMIC_RELAXED);
    if (worker_index == 0U) {
        while ((__atomic_load_n(&work->worker_mask, __ATOMIC_RELAXED) &
                ~1U) == 0U) {
        }
    }
    for (index = begin; index < end; ++index) {
        (void)__atomic_fetch_add(&work->total, (unsigned int)(index + 1U),
                                 __ATOMIC_RELAXED);
    }
    return 0;
}

int main(void) {
    Av2hTaskPool pool;
    FreestandingWork work;
    unsigned int pass;
    if (av2h_task_pool_init(&pool, 4U) != 0) return 1;
    if (av2h_task_pool_width(&pool) != 4U) return 2;
    work.width = av2h_task_pool_width(&pool);
    for (pass = 0U; pass < 8U; ++pass) {
        __atomic_store_n(&work.total, 0U, __ATOMIC_RELAXED);
        __atomic_store_n(&work.worker_mask, 0U, __ATOMIC_RELAXED);
        if (av2h_parallel_for(&pool, 4096U, 5U, add_range, &work) != 0) {
            return 3;
        }
        if (__atomic_load_n(&work.total, __ATOMIC_RELAXED) !=
            (4096U * 4097U) / 2U) {
            return 4;
        }
        if ((__atomic_load_n(&work.worker_mask, __ATOMIC_RELAXED) &
             ~1U) == 0U) {
            return 5;
        }
    }
    av2h_task_pool_destroy(&pool);
    return 0;
}
