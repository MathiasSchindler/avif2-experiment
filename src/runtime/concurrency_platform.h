#ifndef AV2H_RUNTIME_CONCURRENCY_PLATFORM_H
#define AV2H_RUNTIME_CONCURRENCY_PLATFORM_H

#include "concurrency.h"

int av2h_platform_workers_supported(void);
unsigned int av2h_platform_worker_count(void);
int av2h_platform_worker_start(Av2hTaskWorker *worker,
                               int (*entry)(void *), void *argument);
int av2h_platform_worker_join(Av2hTaskWorker *worker);
void av2h_platform_wait_word(volatile unsigned int *word,
                             unsigned int expected);
void av2h_platform_wake_word_count(volatile unsigned int *word,
                                   unsigned int count);
void av2h_platform_wake_word_all(volatile unsigned int *word);

#endif
