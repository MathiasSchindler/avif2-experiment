#ifndef AV2H_PLATFORM_LINUX_ABI_H
#define AV2H_PLATFORM_LINUX_ABI_H

#include <stddef.h>

#define AV2H_LINUX_AT_FDCWD (-100)

#define AV2H_LINUX_O_RDONLY 0
#define AV2H_LINUX_O_WRONLY 1
#define AV2H_LINUX_O_CREAT 0100
#define AV2H_LINUX_O_TRUNC 01000
#define AV2H_LINUX_O_CLOEXEC 02000000

#define AV2H_LINUX_PROT_READ 1
#define AV2H_LINUX_PROT_WRITE 2
#define AV2H_LINUX_MAP_PRIVATE 2
#define AV2H_LINUX_MAP_ANONYMOUS 0x20

#define AV2H_LINUX_SEEK_SET 0
#define AV2H_LINUX_SEEK_END 2

#if defined(__x86_64__)
#define AV2H_LINUX_SYS_READ 0
#define AV2H_LINUX_SYS_WRITE 1
#define AV2H_LINUX_SYS_CLOSE 3
#define AV2H_LINUX_SYS_LSEEK 8
#define AV2H_LINUX_SYS_MMAP 9
#define AV2H_LINUX_SYS_MUNMAP 11
#define AV2H_LINUX_SYS_OPENAT 257

long av2h_linux_syscall1(long number, long argument0);
long av2h_linux_syscall2(long number, long argument0, long argument1);
long av2h_linux_syscall3(long number, long argument0, long argument1,
                         long argument2);
long av2h_linux_syscall4(long number, long argument0, long argument1,
                         long argument2, long argument3);
long av2h_linux_syscall6(long number, long argument0, long argument1,
                         long argument2, long argument3, long argument4,
                         long argument5);
#elif defined(__aarch64__)
#define AV2H_LINUX_SYS_OPENAT 56
#define AV2H_LINUX_SYS_CLOSE 57
#define AV2H_LINUX_SYS_LSEEK 62
#define AV2H_LINUX_SYS_READ 63
#define AV2H_LINUX_SYS_WRITE 64
#define AV2H_LINUX_SYS_MUNMAP 215
#define AV2H_LINUX_SYS_MMAP 222

static inline long av2h_linux_syscall1(long number, long argument0) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = argument0;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

static inline long av2h_linux_syscall2(long number, long argument0,
                                       long argument1) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return x0;
}

static inline long av2h_linux_syscall3(long number, long argument0,
                                       long argument1, long argument2) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    register long x2 __asm__("x2") = argument2;
    __asm__ volatile("svc #0" : "+r"(x0) :
                     "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}

static inline long av2h_linux_syscall4(long number, long argument0,
                                       long argument1, long argument2,
                                       long argument3) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    register long x2 __asm__("x2") = argument2;
    register long x3 __asm__("x3") = argument3;
    __asm__ volatile("svc #0" : "+r"(x0) :
                     "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory");
    return x0;
}

static inline long av2h_linux_syscall6(long number, long argument0,
                                       long argument1, long argument2,
                                       long argument3, long argument4,
                                       long argument5) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    register long x2 __asm__("x2") = argument2;
    register long x3 __asm__("x3") = argument3;
    register long x4 __asm__("x4") = argument4;
    register long x5 __asm__("x5") = argument5;
    __asm__ volatile("svc #0" : "+r"(x0) :
                     "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5),
                     "r"(x8) : "memory");
    return x0;
}
#else
#error "The Linux no-libc decoder supports only x86-64 and AArch64"
#endif

#endif