#ifndef AV2H_TESTS_PARALLEL_TEST_H
#define AV2H_TESTS_PARALLEL_TEST_H

#include "runtime/execution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#define AV2H_TEST_NATIVE_THREADS 1
#else
#define AV2H_TEST_NATIVE_THREADS 0
#endif

#define AV2H_TEST_MAX_THREADS 16U

typedef struct Av2hTestPool Av2hTestPool;
typedef struct {
    Av2hTestPool *pool;
    unsigned int index;
} Av2hTestWorker;

struct Av2hTestPool {
    unsigned int width;
    unsigned int calls;
    unsigned int workers_used;
    size_t count;
    size_t chunk;
    Av2hParallelBody body;
    void *argument;
    int error;
#if AV2H_TEST_NATIVE_THREADS
    pthread_t threads[AV2H_TEST_MAX_THREADS];
    pthread_mutex_t mutex;
    pthread_cond_t ready;
    pthread_cond_t finished;
    unsigned int generation;
    unsigned int remaining;
    int stop;
    Av2hTestWorker workers[AV2H_TEST_MAX_THREADS];
#endif
};

#if AV2H_TEST_NATIVE_THREADS
static void av2h_test_thread_check(int status) {
    if (status != 0) {
        fprintf(stderr, "parallel test: %s\n", strerror(status));
        exit(2);
    }
}
#endif

static void av2h_test_pool_run(Av2hTestPool *pool, unsigned int index) {
    size_t begin;
    int first_error = 0;
    for (begin = index * pool->chunk; begin < pool->count; begin += pool->width * pool->chunk) {
        size_t end = begin + pool->chunk;
        int error;
        if (end > pool->count)
            end = pool->count;
        error = pool->body(begin, end, index, pool->argument);
        if (first_error == 0)
            first_error = error;
    }
#if AV2H_TEST_NATIVE_THREADS
    av2h_test_thread_check(pthread_mutex_lock(&pool->mutex));
#endif
    if (index * pool->chunk < pool->count)
        pool->workers_used |= 1U << index;
    if (pool->error == 0)
        pool->error = first_error;
#if AV2H_TEST_NATIVE_THREADS
    av2h_test_thread_check(pthread_mutex_unlock(&pool->mutex));
#endif
}

#if AV2H_TEST_NATIVE_THREADS
static void *av2h_test_pool_worker(void *argument) {
    Av2hTestWorker *worker = argument;
    Av2hTestPool *pool = worker->pool;
    unsigned int generation = 0U;
    av2h_test_thread_check(pthread_mutex_lock(&pool->mutex));
    for (;;) {
        while (generation == pool->generation && !pool->stop)
            av2h_test_thread_check(pthread_cond_wait(&pool->ready, &pool->mutex));
        if (pool->stop)
            break;
        generation = pool->generation;
        av2h_test_thread_check(pthread_mutex_unlock(&pool->mutex));
        av2h_test_pool_run(pool, worker->index);
        av2h_test_thread_check(pthread_mutex_lock(&pool->mutex));
        if (--pool->remaining == 0U)
            av2h_test_thread_check(pthread_cond_signal(&pool->finished));
    }
    av2h_test_thread_check(pthread_mutex_unlock(&pool->mutex));
    return NULL;
}
#endif

static int av2h_test_pool_dispatch(void *context, size_t count, size_t min_chunk,
                                   Av2hParallelBody body, void *argument) {
    Av2hTestPool *pool = context;
    ++pool->calls;
#if AV2H_TEST_NATIVE_THREADS
    av2h_test_thread_check(pthread_mutex_lock(&pool->mutex));
#endif
    pool->count = count;
    pool->chunk = min_chunk == 0U ? 1U : min_chunk;
    pool->body = body;
    pool->argument = argument;
    pool->error = 0;
#if AV2H_TEST_NATIVE_THREADS
    pool->remaining = pool->width - 1U;
    ++pool->generation;
    av2h_test_thread_check(pthread_cond_broadcast(&pool->ready));
    av2h_test_thread_check(pthread_mutex_unlock(&pool->mutex));
    av2h_test_pool_run(pool, 0U);
    av2h_test_thread_check(pthread_mutex_lock(&pool->mutex));
    while (pool->remaining != 0U)
        av2h_test_thread_check(pthread_cond_wait(&pool->finished, &pool->mutex));
    av2h_test_thread_check(pthread_mutex_unlock(&pool->mutex));
#else
    {
        unsigned int index = pool->width;
        while (index != 0U)
            av2h_test_pool_run(pool, --index);
    }
#endif
    return pool->error;
}

static void av2h_test_pool_init(Av2hTestPool *pool, unsigned int width) {
    memset(pool, 0, sizeof(*pool));
    if (width == 0U || width > AV2H_TEST_MAX_THREADS) {
        fprintf(stderr, "parallel test: invalid width %u\n", width);
        exit(2);
    }
    pool->width = width;
#if AV2H_TEST_NATIVE_THREADS
    {
        unsigned int index;
        av2h_test_thread_check(pthread_mutex_init(&pool->mutex, NULL));
        av2h_test_thread_check(pthread_cond_init(&pool->ready, NULL));
        av2h_test_thread_check(pthread_cond_init(&pool->finished, NULL));
        for (index = 1U; index < width; ++index) {
            pool->workers[index] = (Av2hTestWorker){pool, index};
            av2h_test_thread_check(pthread_create(&pool->threads[index], NULL,
                                                  av2h_test_pool_worker, &pool->workers[index]));
        }
    }
#endif
}

static void av2h_test_pool_destroy(Av2hTestPool *pool) {
#if AV2H_TEST_NATIVE_THREADS
    unsigned int index;
    av2h_test_thread_check(pthread_mutex_lock(&pool->mutex));
    pool->stop = 1;
    av2h_test_thread_check(pthread_cond_broadcast(&pool->ready));
    av2h_test_thread_check(pthread_mutex_unlock(&pool->mutex));
    for (index = 1U; index < pool->width; ++index)
        av2h_test_thread_check(pthread_join(pool->threads[index], NULL));
    av2h_test_thread_check(pthread_cond_destroy(&pool->ready));
    av2h_test_thread_check(pthread_cond_destroy(&pool->finished));
    av2h_test_thread_check(pthread_mutex_destroy(&pool->mutex));
#else
    (void)pool;
#endif
}

static Av2hExecution av2h_test_pool_execution(Av2hTestPool *pool) {
    Av2hExecution execution = {pool, av2h_test_pool_dispatch, pool->width};
    return execution;
}

#endif
