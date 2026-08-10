.text

.global _start
.type _start,@function
_start:
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    call main
    mov %eax, %edi
    mov $60, %eax
    syscall
.size _start, .-_start

.global av2h_linux_syscall1
.type av2h_linux_syscall1,@function
av2h_linux_syscall1:
    mov %rdi, %rax
    mov %rsi, %rdi
    syscall
    ret
.size av2h_linux_syscall1, .-av2h_linux_syscall1

.global av2h_linux_syscall2
.type av2h_linux_syscall2,@function
av2h_linux_syscall2:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    syscall
    ret
.size av2h_linux_syscall2, .-av2h_linux_syscall2

.global av2h_linux_syscall3
.type av2h_linux_syscall3,@function
av2h_linux_syscall3:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    syscall
    ret
.size av2h_linux_syscall3, .-av2h_linux_syscall3

.global av2h_linux_syscall4
.type av2h_linux_syscall4,@function
av2h_linux_syscall4:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    syscall
    ret
.size av2h_linux_syscall4, .-av2h_linux_syscall4

.global av2h_linux_syscall6
.type av2h_linux_syscall6,@function
av2h_linux_syscall6:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    mov %r9, %r8
    mov 8(%rsp), %r9
    syscall
    ret
.size av2h_linux_syscall6, .-av2h_linux_syscall6

.section .note.GNU-stack,"",@progbits