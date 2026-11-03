#ifndef AV2H_PLATFORM_MACOS_ABI_H
#define AV2H_PLATFORM_MACOS_ABI_H

#define AV2H_MACOS_EINTR 4
#define AV2H_MACOS_PAGE_SIZE 16384U

#define AV2H_MACOS_O_RDONLY 0
#define AV2H_MACOS_O_WRONLY 1
#define AV2H_MACOS_O_CREAT 0x0200
#define AV2H_MACOS_O_TRUNC 0x0400
#define AV2H_MACOS_O_CLOEXEC 0x01000000

#define AV2H_MACOS_PROT_READ 1
#define AV2H_MACOS_PROT_WRITE 2
#define AV2H_MACOS_MAP_PRIVATE 2
#define AV2H_MACOS_MAP_ANONYMOUS 0x1000

#define AV2H_MACOS_SEEK_SET 0
#define AV2H_MACOS_SEEK_END 2

#define AV2H_MACOS_SYS_READ 3
#define AV2H_MACOS_SYS_WRITE 4
#define AV2H_MACOS_SYS_OPEN 5
#define AV2H_MACOS_SYS_CLOSE 6
#define AV2H_MACOS_SYS_MUNMAP 73
#define AV2H_MACOS_SYS_MMAP 197
#define AV2H_MACOS_SYS_LSEEK 199

static inline long av2h_macos_syscall1(long number, long argument0) {
    register long x16 __asm__("x16") = number;
    register long x0 __asm__("x0") = argument0;
    __asm__ volatile("svc #0x80\n\tcneg %0, %0, cs" : "+r"(x0), "+r"(x16) : : "memory", "cc");
    return x0;
}

static inline long av2h_macos_syscall2(long number, long argument0, long argument1) {
    register long x16 __asm__("x16") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    __asm__ volatile("svc #0x80\n\tcneg %0, %0, cs"
                     : "+r"(x0), "+r"(x1), "+r"(x16)
                     :
                     : "memory", "cc");
    return x0;
}

static inline long av2h_macos_syscall3(long number, long argument0, long argument1,
                                       long argument2) {
    register long x16 __asm__("x16") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    register long x2 __asm__("x2") = argument2;
    __asm__ volatile("svc #0x80\n\tcneg %0, %0, cs"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x16)
                     :
                     : "memory", "cc");
    return x0;
}

static inline long av2h_macos_syscall6(long number, long argument0, long argument1, long argument2,
                                       long argument3, long argument4, long argument5) {
    register long x16 __asm__("x16") = number;
    register long x0 __asm__("x0") = argument0;
    register long x1 __asm__("x1") = argument1;
    register long x2 __asm__("x2") = argument2;
    register long x3 __asm__("x3") = argument3;
    register long x4 __asm__("x4") = argument4;
    register long x5 __asm__("x5") = argument5;
    __asm__ volatile("svc #0x80\n\tcneg %0, %0, cs"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5), "+r"(x16)
                     :
                     : "memory", "cc");
    return x0;
}

#endif
