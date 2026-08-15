#ifndef AV2H_RUNTIME_EXECUTION_H
#define AV2H_RUNTIME_EXECUTION_H

#include <stddef.h>

typedef int (*Av2hParallelBody)(size_t begin, size_t end,
                                unsigned int worker_index, void *argument);

typedef int (*Av2hParallelDispatch)(void *context,
                                    size_t count,
                                    size_t min_chunk,
                                    Av2hParallelBody body,
                                    void *argument);

typedef struct {
    void *context;
    Av2hParallelDispatch parallel_for;
    unsigned int width;
} Av2hExecution;

#endif
