#include "concurrency_platform.h"

int av2h_platform_workers_supported(void) {
    return 0;
}

unsigned int av2h_platform_worker_count(void) {
    return 1U;
}

int av2h_platform_worker_start(Av2hTaskWorker *worker, int (*entry)(void *), void *argument) {
    (void)worker;
    (void)entry;
    (void)argument;
    return -1;
}

int av2h_platform_worker_join(Av2hTaskWorker *worker) {
    (void)worker;
    return -1;
}

void av2h_platform_wait_word(volatile unsigned int *word, unsigned int expected) {
    (void)word;
    (void)expected;
}

void av2h_platform_wake_word_count(volatile unsigned int *word, unsigned int count) {
    (void)word;
    (void)count;
}

void av2h_platform_wake_word_all(volatile unsigned int *word) {
    (void)word;
}
