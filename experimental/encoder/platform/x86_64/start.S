/*
 * Original stack-entry shim for this experiment. The reviewed CC0 newos
 * runtime concepts and exact revision are documented in ../README.md.
 */
.text
.global _start
.type _start,@function
_start:
    xor %rbp, %rbp
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    andq $-16, %rsp
    call encoder_main
    mov %eax, %edi
    call enc_process_exit
    ud2
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
