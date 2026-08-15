#include "concurrency_platform.h"

#define AV2H_LINUX_SYS_MMAP 9
#define AV2H_LINUX_SYS_MUNMAP 11
#define AV2H_LINUX_SYS_SCHED_GETAFFINITY 204
#define AV2H_LINUX_SYS_FUTEX 202

#define AV2H_LINUX_PROT_READ 1
#define AV2H_LINUX_PROT_WRITE 2
#define AV2H_LINUX_MAP_PRIVATE 2
#define AV2H_LINUX_MAP_ANONYMOUS 0x20

#define AV2H_LINUX_FUTEX_WAIT 0
#define AV2H_LINUX_FUTEX_WAKE 1
#define AV2H_LINUX_FUTEX_PRIVATE 128
#define AV2H_LINUX_EINTR 4
#define AV2H_LINUX_EAGAIN 11

#define AV2H_LINUX_CLONE_VM             0x00000100UL
#define AV2H_LINUX_CLONE_FS             0x00000200UL
#define AV2H_LINUX_CLONE_FILES          0x00000400UL
#define AV2H_LINUX_CLONE_SIGHAND        0x00000800UL
#define AV2H_LINUX_CLONE_THREAD         0x00010000UL
#define AV2H_LINUX_CLONE_SYSVSEM        0x00040000UL
#define AV2H_LINUX_CLONE_PARENT_SETTID  0x00100000UL
#define AV2H_LINUX_CLONE_CHILD_CLEARTID 0x00200000UL

long av2h_linux_clone_thread(long flags, void *stack, int *clear_tid,
                             int (*entry)(void *), void *argument);

static long syscall2(long number, long argument0, long argument1) {
    register long result __asm__("rax") = number;
    register long a0 __asm__("rdi") = argument0;
    register long a1 __asm__("rsi") = argument1;
    __asm__ volatile("syscall" : "+a"(result) : "D"(a0), "S"(a1) :
                     "rcx", "r11", "memory");
    return result;
}

static long syscall3(long number, long argument0, long argument1,
                     long argument2) {
    register long result __asm__("rax") = number;
    register long a0 __asm__("rdi") = argument0;
    register long a1 __asm__("rsi") = argument1;
    register long a2 __asm__("rdx") = argument2;
    __asm__ volatile("syscall" : "+a"(result) :
                     "D"(a0), "S"(a1), "d"(a2) :
                     "rcx", "r11", "memory");
    return result;
}

static long syscall6(long number, long argument0, long argument1,
                     long argument2, long argument3, long argument4,
                     long argument5) {
    register long result __asm__("rax") = number;
    register long a0 __asm__("rdi") = argument0;
    register long a1 __asm__("rsi") = argument1;
    register long a2 __asm__("rdx") = argument2;
    register long a3 __asm__("r10") = argument3;
    register long a4 __asm__("r8") = argument4;
    register long a5 __asm__("r9") = argument5;
    __asm__ volatile("syscall" : "+a"(result) :
                     "D"(a0), "S"(a1), "d"(a2), "r"(a3), "r"(a4), "r"(a5) :
                     "rcx", "r11", "memory");
    return result;
}

static void futex_wait(volatile unsigned int *word, unsigned int expected,
                       int private_flag) {
    long result = syscall6(AV2H_LINUX_SYS_FUTEX, (long)word,
                           AV2H_LINUX_FUTEX_WAIT | private_flag,
                           (long)expected, 0, 0, 0);
    (void)result;
}

static void futex_wake(volatile unsigned int *word, unsigned int count) {
    (void)syscall6(AV2H_LINUX_SYS_FUTEX, (long)word,
                   AV2H_LINUX_FUTEX_WAKE | AV2H_LINUX_FUTEX_PRIVATE,
                   (long)count, 0, 0, 0);
}

int av2h_platform_workers_supported(void) {
    return 1;
}

unsigned int av2h_platform_worker_count(void) {
    unsigned long masks[16];
    unsigned int count = 0U;
    size_t index;
    for (index = 0U; index < sizeof(masks) / sizeof(masks[0]); ++index) {
        masks[index] = 0UL;
    }
    if (syscall3(AV2H_LINUX_SYS_SCHED_GETAFFINITY, 0, (long)sizeof(masks),
                 (long)masks) < 0) {
        return 2U;
    }
    for (index = 0U; index < sizeof(masks) / sizeof(masks[0]); ++index) {
        unsigned long value = masks[index];
        while (value != 0UL) {
            count += (unsigned int)(value & 1UL);
            value >>= 1U;
        }
    }
    return count == 0U ? 2U : count;
}

int av2h_platform_worker_start(Av2hTaskWorker *worker,
                               int (*entry)(void *), void *argument) {
    unsigned long flags;
    long mapping;
    long tid;
    if (worker == 0 || entry == 0) return -1;
    mapping = syscall6(AV2H_LINUX_SYS_MMAP, 0,
                       (long)AV2H_TASK_POOL_STACK_SIZE,
                       AV2H_LINUX_PROT_READ | AV2H_LINUX_PROT_WRITE,
                       AV2H_LINUX_MAP_PRIVATE | AV2H_LINUX_MAP_ANONYMOUS,
                       -1, 0);
    if (mapping < 0) return -1;
    worker->stack = (void *)mapping;
    worker->stack_size = AV2H_TASK_POOL_STACK_SIZE;
    worker->clear_tid = 0;
    flags = AV2H_LINUX_CLONE_VM | AV2H_LINUX_CLONE_FS |
            AV2H_LINUX_CLONE_FILES | AV2H_LINUX_CLONE_SIGHAND |
            AV2H_LINUX_CLONE_THREAD | AV2H_LINUX_CLONE_SYSVSEM |
            AV2H_LINUX_CLONE_PARENT_SETTID |
            AV2H_LINUX_CLONE_CHILD_CLEARTID;
    tid = av2h_linux_clone_thread(
        (long)flags, (unsigned char *)worker->stack + worker->stack_size,
        (int *)&worker->clear_tid, entry, argument);
    if (tid < 0) {
        (void)syscall2(AV2H_LINUX_SYS_MUNMAP, mapping,
                       (long)worker->stack_size);
        worker->stack = 0;
        worker->stack_size = 0U;
        return -1;
    }
    worker->tid = (int)tid;
    return 0;
}

int av2h_platform_worker_join(Av2hTaskWorker *worker) {
    if (worker == 0) return -1;
    for (;;) {
        int value = __atomic_load_n(&worker->clear_tid, __ATOMIC_ACQUIRE);
        if (value == 0) break;
        if (value > 0) {
            long result = syscall6(
                AV2H_LINUX_SYS_FUTEX, (long)&worker->clear_tid,
                AV2H_LINUX_FUTEX_WAIT, value, 0, 0, 0);
            if (result < 0 && result != -AV2H_LINUX_EAGAIN &&
                result != -AV2H_LINUX_EINTR) {
                return -1;
            }
        }
    }
    if (worker->stack != 0) {
        (void)syscall2(AV2H_LINUX_SYS_MUNMAP, (long)worker->stack,
                       (long)worker->stack_size);
    }
    worker->stack = 0;
    worker->stack_size = 0U;
    worker->tid = 0;
    return 0;
}

void av2h_platform_wait_word(volatile unsigned int *word,
                             unsigned int expected) {
    futex_wait(word, expected, AV2H_LINUX_FUTEX_PRIVATE);
}

void av2h_platform_wake_word_count(volatile unsigned int *word,
                                   unsigned int count) {
    if (count != 0U) futex_wake(word, count);
}

void av2h_platform_wake_word_all(volatile unsigned int *word) {
    futex_wake(word, 2147483647U);
}
