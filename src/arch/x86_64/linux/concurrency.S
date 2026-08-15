.text

.global av2h_linux_clone_thread
.type av2h_linux_clone_thread,@function
av2h_linux_clone_thread:
    sub $16, %rsi
    mov %rcx, (%rsi)
    mov %r8, 8(%rsi)
    mov %rdx, %r10
    xor %r8, %r8
    mov $56, %rax
    syscall
    test %rax, %rax
    jnz 1f
    pop %rax
    pop %rdi
    call *%rax
    mov %rax, %rdi
    mov $60, %rax
    syscall
2:
    jmp 2b
1:
    ret
.size av2h_linux_clone_thread, .-av2h_linux_clone_thread

.section .note.GNU-stack,"",@progbits
